/**
*   Copyright(C) 2011-2015 Intel Corporation All Rights Reserved.
*
*   The source code, information  and  material ("Material") contained herein is
*   owned  by Intel Corporation or its suppliers or licensors, and title to such
*   Material remains  with Intel Corporation  or its suppliers or licensors. The
*   Material  contains proprietary information  of  Intel or  its  suppliers and
*   licensors. The  Material is protected by worldwide copyright laws and treaty
*   provisions. No  part  of  the  Material  may  be  used,  copied, reproduced,
*   modified, published, uploaded, posted, transmitted, distributed or disclosed
*   in any way  without Intel's  prior  express written  permission. No  license
*   under  any patent, copyright  or  other intellectual property rights  in the
*   Material  is  granted  to  or  conferred  upon  you,  either  expressly,  by
*   implication, inducement,  estoppel or  otherwise.  Any  license  under  such
*   intellectual  property  rights must  be express  and  approved  by  Intel in
*   writing.
*
*   *Third Party trademarks are the property of their respective owners.
*
*   Unless otherwise  agreed  by Intel  in writing, you may not remove  or alter
*   this  notice or  any other notice embedded  in Materials by Intel or Intel's
*   suppliers or licensors in any way.
*/

#include <stdarg.h>
#include <stdio.h>      /* vsnprintf */
#include <string.h>
#include <math.h>
#include "Enclave.h"
#include "Enclave_t.h"  /* print_string */

#include "nmglobal.h"
#include "emfloat.h"
#include "pointer.h"
#include "wordcat.h"

/*
** TYPEDEFS
*/
#define TRUE    1
#define FALSE   0

#ifdef LONG64
#define MAXPOSLONG 0x7FFFFFFFFFFFFFFFL
#else
#define MAXPOSLONG 0x7FFFFFFFL
#endif
#define STRINGARRAYSIZE 8111L
/************************
** HUFFMAN COMPRESSION **
************************/
#define MAXHUFFLOOPS 500000L
#define HUFFARRAYSIZE 5000L
#define EXCLUDED 32000L          /* Big positive value */
#define WORDCATSIZE 50


/*
** TYPEDEFS
*/


typedef struct {
	uchar c;                /* Byte value */
	float freq;             /* Frequency */
	int parent;             /* Parent node */
	int left;               /* Left pointer = 0 */
	int right;              /* Right pointer = 1 */
} huff_node;

/*
** GLOBALS
*/
static huff_node *hufftree;             /* The huffman tree */
static long plaintextlen;               /* Length of plaintext */

/********************************
 * ** BACK PROPAGATION NEURAL NET **
 * ********************************/

/*
 * ** DEFINES
 * */
#define T 1                     /* TRUE */
#define F 0                     /* FALSE */
#define ERR -1
#define MAXPATS 10              /* max number of patterns in data file */
#define IN_X_SIZE 5             /* number of neurodes/row of input layer */
#define IN_Y_SIZE 7             /* number of neurodes/col of input layer */
#define IN_SIZE 35              /* equals IN_X_SIZE*IN_Y_SIZE */
#define MID_SIZE 8              /* number of neurodes in middle layer */
#define OUT_SIZE 8              /* number of neurodes in output layer */
#define MARGIN 0.1              /* how near to 1,0 do we have to come to stop? */
#define BETA 0.09               /* beta learning constant */
#define ALPHA 0.09              /* momentum term constant */
#define STOP 0.1                /* when worst_error less than STOP, training is done */

/*
 * ** GLOBALS
 * */
double  mid_wts[MID_SIZE][IN_SIZE];     /* middle layer weights */
double  out_wts[OUT_SIZE][MID_SIZE];    /* output layer weights */
double  mid_out[MID_SIZE];              /* middle layer output */
double  out_out[OUT_SIZE];              /* output layer output */
double  mid_error[MID_SIZE];            /* middle layer errors */
double  out_error[OUT_SIZE];            /* output layer errors */
double  mid_wt_change[MID_SIZE][IN_SIZE]; /* storage for last wt change */
double  out_wt_change[OUT_SIZE][MID_SIZE]; /* storage for last wt change */
double  in_pats[MAXPATS][IN_SIZE];      /* input patterns */
double  out_pats[MAXPATS][OUT_SIZE];    /* desired output patterns */
double  tot_out_error[MAXPATS];         /* measure of whether net is done */
double  out_wt_cum_change[OUT_SIZE][MID_SIZE]; /* accumulated wt changes */
double  mid_wt_cum_change[MID_SIZE][IN_SIZE];  /* accumulated wt changes */

double  worst_error; /* worst error each pass through the data */
double  average_error; /* average error each pass through the data */
double  avg_out_error[MAXPATS]; /* average error each pattern */

int iteration_count;    /* number of passes thru network so far */
int numpats;            /* number of patterns in data file */
int numpasses;          /* number of training passes through data file */
int learned;            /* flag--if TRUE, network has learned all patterns */

/*
 * ** PROTOTYPES
 * */
void DoNNET(void);
static ulong DoNNetIteration(ulong nloops);
static void do_mid_forward(int patt);
static void do_out_forward();
void display_output(int patt);
static void do_forward_pass(int patt);
static void do_out_error(int patt);
static void worst_pass_error();
static void do_mid_error();
static void adjust_out_wts();
static void adjust_mid_wts();
static void do_back_pass(int patt);
static void move_wt_changes();
static int check_out_error();
static void zero_changes();
static void randomize_wts();
static int read_data_file();

/***********************
**  LU DECOMPOSITION  **
** (Linear Equations) **
***********************/

/*
** DEFINES
*/

#define LUARRAYROWS 101L
#define LUARRAYCOLS 101L

/*
** TYPEDEFS
*/
typedef struct
{       union
	{       fardouble *p;
		fardouble (*ap)[][LUARRAYCOLS];
	} ptrs;
} LUdblptr;

/*
** GLOBALS
*/
fardouble *LUtempvv;

/*************************
** ASSIGNMENT ALGORITHM **
*************************/
#define ASSIGNROWS 101L
#define ASSIGNCOLS 101L
/*
** TYPEDEFS
*/
typedef struct {
	union {
		long *p;
		long (*ap)[ASSIGNROWS][ASSIGNCOLS];
	} ptrs;
} longptr;

/********************
** IDEA ENCRYPTION **
********************/
/*
** DEFINES
*/
#define IDEAKEYSIZE 16
#define IDEABLOCKSIZE 8
#define ROUNDS 8
#define KEYLEN (6*ROUNDS+4)

/*
** MACROS
*/
#define low16(x) ((x) & 0x0FFFF)
#define MUL(x,y) (x=mul(low16(x),y))
typedef u16 IDEAkey[KEYLEN];
/****************************
** MoveMemory
** Moves n bytes from a to b.  Handles overlap.
** In most cases, this is just a memmove operation.
** But, not in DOS....noooo....
*/
void MoveMemory( farvoid *destination,  /* Destination address */
		farvoid *source,        /* Source address */
		unsigned long nbytes)
{
memmove(destination, source, nbytes);
}


/**********************************************************
 * Enclave globals					  *
 **********************************************************
 * These variables are only available in the enclave      *
 *********************************************************/

void* enclave_buffer;
void* enclave_buffer2;
void* enclave_buffer3;
void* enclave_buffer4;
void* enclave_buffer5;

/**********************************************************
 * Public functions					  *
 **********************************************************
 * The following functions must be placed in enclave.edl  *
 *********************************************************/

void encl_AllocateMemory(size_t size){
    enclave_buffer = malloc(size);
    if( !enclave_buffer ){
        printf("FATAL: Enclave malloc returned NULL pointer for buffer 1\n");
	encl_FreeMemory();
	}
}

void encl_AllocateMemory2(size_t size){
   enclave_buffer2 = malloc(size);
    if( !enclave_buffer2 ){
        printf("FATAL: Enclave malloc returned NULL pointer for buffer 2\n");
	encl_FreeMemory2();
	}
}

void encl_AllocateMemory3(size_t size){
    enclave_buffer3 = malloc(size);
    if( !enclave_buffer3 ){
        printf("FATAL: Enclave malloc returned NULL pointer for buffer 3\n");
	encl_FreeMemory3();
	}
}

void encl_AllocateMemory4(size_t size){
    enclave_buffer4 = malloc(size);
    if( !enclave_buffer4 ){
        printf("FATAL: Enclave malloc returned NULL pointer for buffer 4\n");
	encl_FreeMemory4();	
	}
}

void encl_AllocateMemory5(size_t size){
    enclave_buffer5 = malloc(size);
    if( !enclave_buffer5 ){
        printf("FATAL: Enclave malloc returned NULL pointer for buffer 5\n");
	encl_FreeMemory5();
	}
}

void encl_FreeMemory(){
    free(enclave_buffer);
}

void encl_FreeMemory2(){
    free(enclave_buffer2);
}

void encl_FreeMemory3(){
    free(enclave_buffer3);
}

void encl_FreeMemory4(){
    free(enclave_buffer4);
}

void encl_FreeMemory5(){
    free(enclave_buffer5);
}

























/**********************************************************
 * Private functions					  *
 **********************************************************
 * Do not place the following functions in enclave.edl    *
 *********************************************************/
/**************
** stradjust **
***************
** Used by the string heap sort.  Call this routine to adjust the
** string at offset i to length l.  The members of the string array
** are moved accordingly and the length of the string at offset i
** is set to l.
*/
static void stradjust(farulong *optrarray,      /* Offset pointer array */
	faruchar *strarray,                     /* String array */
	ulong nstrings,                         /* # of strings */
	ulong i,                                /* Offset to adjust */
	uchar l)                                /* New length */
{
unsigned long nbytes;           /* # of bytes to move */
unsigned long j;                /* Index */
int direction;                  /* Direction indicator */
unsigned char adjamount;        /* Adjustment amount */

/*
** If new length is less than old length, the direction is
** down.  If new length is greater than old length, the
** direction is up.
*/
direction=(int)l - (int)*(strarray+*(optrarray+i));
adjamount=(unsigned char)abs(direction);

/*
** See if the adjustment is being made to the last
** string in the string array.  If so, we don't have to
** do anything more than adjust the length field.
*/
if(i==(nstrings-1L))
{       *(strarray+*(optrarray+i))=l;
	return;
}

/*
** Calculate the total # of bytes in string array from
** location i+1 to end of array.  Whether we're moving "up" or
** down, this is how many bytes we'll have to move.
*/
nbytes=*(optrarray+nstrings-1L) +
	(unsigned long)*(strarray+*(optrarray+nstrings-1L)) + 1L -
	*(optrarray+i+1L);

/*
** Calculate the source and the destination.  Source is
** string position i+1.  Destination is string position i+l
** (i+"ell"...don't confuse 1 and l).
** Hand this straight to memmove and let it handle the
** "overlap" problem.
*/
MoveMemory((farvoid *)(strarray+*(optrarray+i)+l+1),
	(farvoid *)(strarray+*(optrarray+i+1)),
	(unsigned long)nbytes);

/*
** We have to adjust the offset pointer array.
** This covers string i+1 to numstrings-1.
*/
for(j=i+1;j<nstrings;j++)
	if(direction<0)
		*(optrarray+j)=*(optrarray+j)-adjamount;
	else
		*(optrarray+j)=*(optrarray+j)+adjamount;

/*
** Store the new length and go home.
*/
*(strarray+*(optrarray+i))=l;
return;
}
/****************
** str_is_less **
*****************
** Pass this function:
**      1) A pointer to an array of offset pointers
**      2) A pointer to a string array
**      3) The number of elements in the string array
**      4) Offsets to two strings (a & b)
** This function returns TRUE if string a is < string b.
*/
static int str_is_less(farulong *optrarray, /* Offset pointers */
	faruchar *strarray,                     /* String array */
	ulong numstrings,                       /* # of strings */
	ulong a, ulong b)                       /* Offsets */
{
int slen;               /* String length */

/*
** Determine which string has the minimum length.  Use that
** to call strncmp().  If they match up to that point, the
** string with the longer length wins.
*/
slen=(int)*(strarray+*(optrarray+a));
if(slen > (int)*(strarray+*(optrarray+b)))
	slen=(int)*(strarray+*(optrarray+b));

slen=strncmp((char *)(strarray+*(optrarray+a)),
		(char *)(strarray+*(optrarray+b)),slen);

if(slen==0)
{
	/*
	** They match.  Return true if the length of a
	** is greater than the length of b.
	*/
	if(*(strarray+*(optrarray+a)) >
		*(strarray+*(optrarray+b)))
		return(TRUE);
	return(FALSE);
}

if(slen<0) return(TRUE);        /* a is strictly less than b */

return(FALSE);                  /* Only other possibility */
}

/************
** strsift **
*************
** Pass this function:
**      1) A pointer to an array of offset pointers
**      2) A pointer to a string array
**      3) The number of elements in the string array
**      4) Offset within which to sort.
** Sift the array within the bounds of those offsets (thus
** building a heap).
*/
static void strsift(farulong *optrarray,        /* Offset pointers */
	faruchar *strarray,                     /* String array */
	ulong numstrings,                       /* # of strings */
	ulong i, ulong j)                       /* Offsets */
{
unsigned long k;                /* Temporaries */
unsigned char temp[80];
unsigned char tlen;             /* For string lengths */


while((i+i)<=j)
{
	k=i+i;
	if(k<j)
		if(str_is_less(optrarray,strarray,numstrings,k,k+1L))
			++k;
	if(str_is_less(optrarray,strarray,numstrings,i,k))
	{
		/* temp=string[k] */
		tlen=*(strarray+*(optrarray+k));
		MoveMemory((farvoid *)&temp[0],
			(farvoid *)(strarray+*(optrarray+k)),
			(unsigned long)(tlen+1));

		/* string[k]=string[i] */
		tlen=*(strarray+*(optrarray+i));
		stradjust(optrarray,strarray,numstrings,k,tlen);
		MoveMemory((farvoid *)(strarray+*(optrarray+k)),
			(farvoid *)(strarray+*(optrarray+i)),
			(unsigned long)(tlen+1));

		/* string[i]=temp */
		tlen=temp[0];
		stradjust(optrarray,strarray,numstrings,i,tlen);
		MoveMemory((farvoid *)(strarray+*(optrarray+i)),
			(farvoid *)&temp[0],
			(unsigned long)(tlen+1));
		i=k;
	}
	else
		i=j+1;
}
return;
}
extern "C" {
/****************************
*        randnum()          *
*****************************
** Second order linear congruential generator.
** Constants suggested by J. G. Skellam.
** If val==0, returns next member of sequence.
**    val!=0, restart generator.
*/
int32 randnum(int32 lngval)
{
	register int32 interm;
	static int32 randw[2] = { (int32)13 , (int32)117 };

	if (lngval!=(int32)0)
	{	randw[0]=(int32)13; randw[1]=(int32)117; }

	interm=(randw[0]*(int32)254754+randw[1]*(int32)529562)%(int32)999563;
	randw[1]=randw[0];
	randw[0]=interm;
	return(interm);
}
/****************************
*         randwc()          *
*****************************
** Returns signed long random modulo num.
*/
/*
long randwc(long num)
{
	return(randnum(0L)%num);
}
*/
/*
** Returns signed 32-bit random modulo num.
*/
int32 randwc(int32 num)
{
	return(randnum((int32)0)%num);
}

/***************************
**      abs_randwc()      **
****************************
** Same as randwc(), only this routine returns only
** positive numbers.
*/

u32 abs_randwc(u32 num)
{
int32 temp;

temp=randwc(num);
if(temp<0) temp=(int32)0-temp;

return((u32)temp);
}

}
/************
** NumSift **
*************
** Peforms the sift operation on a numeric array,
** constructing a heap in the array.
*/
void NumSift(long *array, /* Offset into enclave_buffer */
	unsigned long i,                /* Minimum of array */
	unsigned long j)                /* Maximum of array */
{
unsigned long k;
long temp;                              /* Used for exchange */

while((i+i)<=j)
{
	k=i+i;
	if(k<j)
		if(array[k]<array[k+1L])
			++k;
	if(array[i]<array[k])
	{
		temp=array[k];
		array[k]=array[i];
		array[i]=temp;
		i=k;
	}
	else
		i=j+1;
}
return;
}
/*****************************
**     ToggleBitRun          *
******************************
** Set or clear a run of nbits starting at
** bit_addr in bitmap.
*/
void encl_ToggleBitRun(unsigned long bit_addr, unsigned long nbits, unsigned int val)       
{
unsigned long bindex;   /* Index into array */
unsigned long bitnumb;  /* Bit number */
 //printf("Calculating...Offset1: %ul Offset2: %ul\n",bit_addr,nbits);
bit_addr = *(((unsigned long *)enclave_buffer2)+bit_addr);
nbits = *(((unsigned long *)enclave_buffer2)+nbits);
//printf("After...bit_addr: %ul nbits: %ul\n",bit_addr,nbits);

while(nbits--)
{

	bindex=bit_addr>>6;     /* Index is number /64 */
	bitnumb=bit_addr % 64;   /* Bit number in word */
//printf("Iterating...%u %ul\n",val,bindex);
	if(val)
		((unsigned long *)enclave_buffer)[bindex]|=(1L<<bitnumb);
	else
		((unsigned long *)enclave_buffer)[bindex]&=~(1L<<bitnumb);
	bit_addr++;
}//printf("Escaped.\n");
return;
}




/***************
** FlipBitRun **
****************
** Complements a run of bits.
*/
void encl_FlipBitRun(long bit_addr,long nbits)   
{
unsigned long bindex;   /* Index into array */
unsigned long bitnumb;  /* Bit number */

bit_addr = *(((unsigned long *)enclave_buffer2)+bit_addr);
nbits = *(((unsigned long *)enclave_buffer2)+nbits);

while(nbits--)
{

	bindex=bit_addr>>6;     /* Index is number /64 */
	bitnumb=bit_addr % 64;  /* Bit number in longword */

	((unsigned long *)enclave_buffer)[bindex]^=(1L<<bitnumb);
	bit_addr++;
}

return;
}

unsigned long encl_bitSetup(long bitfieldarraysize, long bitoparraysize)
{
	long i;                         /* Index */
	unsigned long bitoffset;                /* Offset into bitmap */
	unsigned long nbitops=0L;

	randnum((int)13);
	//printf("Before for\n");
	for (i=0;i<bitfieldarraysize;i++)
	{
		*((unsigned long *)enclave_buffer+i)=(unsigned long)0x5555555555555555;
	}
	//printf("After for\n");
	randnum((int)13);
	/* end of addition of code */

	for (i=0;i<bitoparraysize;i++)
	{
	/* First item is offset */
        /* *(bitoparraybase+i+i)=bitoffset=abs_randwc(262140L); */
	*((unsigned long *)enclave_buffer2+i+i)=bitoffset=abs_randwc((int)262140);

	/* Next item is run length */
	/* *nbitops+=*(bitoparraybase+i+i+1L)=abs_randwc(262140L-bitoffset);*/
	nbitops+=*((unsigned long *)enclave_buffer2+i+i+1L)=abs_randwc((int)262140-bitoffset);
	}


	return nbitops;


}

/* Fourier Private Functions */
/****************
 * ** thefunction **
 * *****************
 * ** This routine selects the function to be used
 * ** in the Trapezoid integration.
 * ** x is the independent variable
 * ** omegan is omega * n
 * ** select chooses which of the sine/cosine functions
 * **  are used.  note the special case for select=0.
 * */
static double thefunction(double x,             /* Independent variable */
                double omegan,          /* Omega * term */
                        int select)             /* Choose term */
{

    /*
     * ** Use select to pick which function we call.
     * */
    switch(select)
    {
            case 0: return(pow(x+(double)1.0,x));

            case 1: return(pow(x+(double)1.0,x) * cos(omegan * x));

            case 2: return(pow(x+(double)1.0,x) * sin(omegan * x));
    }

    /*
     * ** We should never reach this point, but the following
     * ** keeps compilers from issuing a warning message.
     * */
    return(0.0);
}
/***********************
 * ** TrapezoidIntegrate **
 * ************************
 * ** Perform a simple trapezoid integration on the
 * ** function (x+1)**x.
 * ** x0,x1 set the lower and upper bounds of the
 * ** integration.
 * ** nsteps indicates # of trapezoidal sections
 * ** omegan is the fundamental frequency times
 * **  the series member #
 * ** select = 0 for the A[0] term, 1 for cosine terms, and
 * **   2 for sine terms.
 * ** Returns the value.
 * */
static double TrapezoidIntegrate( double x0,            /* Lower bound */
                    double x1,              /* Upper bound */
                    int nsteps,             /* # of steps */
                    double omegan,          /* omega * n */
                    int select)
{
    double x;               /* Independent variable */
    double dx;              /* Stepsize */
    double rvalue;          /* Return value */


    /*
     * ** Initialize independent variable
     * */
    x=x0;

    /*
     * ** Calculate stepsize
     * */
    dx=(x1 - x0) / (double)nsteps;

    /*
     * ** Initialize the return value.
     * */
    rvalue=thefunction(x0,omegan,select)/(double)2.0;

    /*
     * ** Compute the other terms of the integral.
     * */
    if(nsteps!=1)
    {       --nsteps;               /* Already done 1 step */
            while(--nsteps )
            {
                x+=dx;
                rvalue+=thefunction(x,omegan,select);
            }
    }
    /*
     * ** Finish computation
     * */
    rvalue=(rvalue+thefunction(x1,omegan,select)/(double)2.0)*dx;

    return(rvalue);
}

/* Neural Net Private Functions */

/*************************
** do_mid_forward(patt) **
**************************
** Process the middle layer's forward pass
** The activation of middle layer's neurode is the weighted
** sum of the inputs from the input pattern, with sigmoid
** function applied to the inputs.
**/
static void  do_mid_forward(int patt)
{
double  sum;
int     neurode, i;

for (neurode=0;neurode<MID_SIZE; neurode++)
{
	sum = 0.0;
	for (i=0; i<IN_SIZE; i++)
	{       /* compute weighted sum of input signals */
		sum += mid_wts[neurode][i]*in_pats[patt][i];
	}
	/*
	** apply sigmoid function f(x) = 1/(1+exp(-x)) to weighted sum
	*/
	sum = 1.0/(1.0+exp(-sum));
	mid_out[neurode] = sum;
}
return;
}

/*********************
** do_out_forward() **
**********************
** process the forward pass through the output layer
** The activation of the output layer is the weighted sum of
** the inputs (outputs from middle layer), modified by the
** sigmoid function.
**/
static void  do_out_forward()
{
double sum;
int neurode, i;

for (neurode=0; neurode<OUT_SIZE; neurode++)
{
	sum = 0.0;
	for (i=0; i<MID_SIZE; i++)
	{       /*
		** compute weighted sum of input signals
		** from middle layer
		*/
		sum += out_wts[neurode][i]*mid_out[i];
	}
	/*
	** Apply f(x) = 1/(1+exp(-x)) to weighted input
	*/
	sum = 1.0/(1.0+exp(-sum));
	out_out[neurode] = sum;
}
return;
}

/*************************
** display_output(patt) **
**************************
** Display the actual output vs. the desired output of the
** network.
** Once the training is complete, and the "learned" flag set
** to TRUE, then display_output sends its output to both
** the screen and to a text output file.
**
** NOTE: This routine has been disabled in the benchmark
** version. -- RG
**/
/*
void  display_output(int patt)
{
int             i;

	fprintf(outfile,"\n Iteration # %d",iteration_count);
	fprintf(outfile,"\n Desired Output:  ");

	for (i=0; i<OUT_SIZE; i++)
	{
		fprintf(outfile,"%6.3f  ",out_pats[patt][i]);
	}
	fprintf(outfile,"\n Actual Output:   ");

	for (i=0; i<OUT_SIZE; i++)
	{
		fprintf(outfile,"%6.3f  ",out_out[i]);
	}
	fprintf(outfile,"\n");
	return;
}
*/

/**********************
** do_forward_pass() **
***********************
** control function for the forward pass through the network
** NOTE: I have disabled the call to display_output() in
**  the benchmark version -- RG.
**/
static void  do_forward_pass(int patt)
{
do_mid_forward(patt);   /* process forward pass, middle layer */
do_out_forward();       /* process forward pass, output layer */
/* display_output(patt);        ** display results of forward pass */
return;
}

/***********************
** do_out_error(patt) **
************************
** Compute the error for the output layer neurodes.
** This is simply Desired - Actual.
**/
static void do_out_error(int patt)
{
int neurode;
double error,tot_error, sum;

tot_error = 0.0;
sum = 0.0;
for (neurode=0; neurode<OUT_SIZE; neurode++)
{
	out_error[neurode] = out_pats[patt][neurode] - out_out[neurode];
	/*
	** while we're here, also compute magnitude
	** of total error and worst error in this pass.
	** We use these to decide if we are done yet.
	*/
	error = out_error[neurode];
	if (error <0.0)
	{
		sum += -error;
		if (-error > tot_error)
			tot_error = -error; /* worst error this pattern */
	}
	else
	{
		sum += error;
		if (error > tot_error)
			tot_error = error; /* worst error this pattern */
	}
}
avg_out_error[patt] = sum/OUT_SIZE;
tot_out_error[patt] = tot_error;
return;
}

/***********************
** worst_pass_error() **
************************
** Find the worst and average error in the pass and save it
**/
static void  worst_pass_error()
{
double error,sum;

int i;

error = 0.0;
sum = 0.0;
for (i=0; i<numpats; i++)
{
	if (tot_out_error[i] > error) error = tot_out_error[i];
	sum += avg_out_error[i];
}
worst_error = error;
average_error = sum/numpats;
return;
}

/*******************
** do_mid_error() **
********************
** Compute the error for the middle layer neurodes
** This is based on the output errors computed above.
** Note that the derivative of the sigmoid f(x) is
**        f'(x) = f(x)(1 - f(x))
** Recall that f(x) is merely the output of the middle
** layer neurode on the forward pass.
**/
static void do_mid_error()
{
double sum;
int neurode, i;

for (neurode=0; neurode<MID_SIZE; neurode++)
{
	sum = 0.0;
	for (i=0; i<OUT_SIZE; i++)
		sum += out_wts[i][neurode]*out_error[i];

	/*
	** apply the derivative of the sigmoid here
	** Because of the choice of sigmoid f(I), the derivative
	** of the sigmoid is f'(I) = f(I)(1 - f(I))
	*/
	mid_error[neurode] = mid_out[neurode]*(1-mid_out[neurode])*sum;
}
return;
}

/*********************
** adjust_out_wts() **
**********************
** Adjust the weights of the output layer.  The error for
** the output layer has been previously propagated back to
** the middle layer.
** Use the Delta Rule with momentum term to adjust the weights.
**/
static void adjust_out_wts()
{
int weight, neurode;
double learn,delta,alph;

learn = BETA;
alph  = ALPHA;
for (neurode=0; neurode<OUT_SIZE; neurode++)
{
	for (weight=0; weight<MID_SIZE; weight++)
	{
		/* standard delta rule */
		delta = learn * out_error[neurode] * mid_out[weight];

		/* now the momentum term */
		delta += alph * out_wt_change[neurode][weight];
		out_wts[neurode][weight] += delta;

		/* keep track of this pass's cum wt changes for next pass's momentum */
		out_wt_cum_change[neurode][weight] += delta;
	}
}
return;
}

/*************************
** adjust_mid_wts(patt) **
**************************
** Adjust the middle layer weights using the previously computed
** errors.
** We use the Generalized Delta Rule with momentum term
**/
static void adjust_mid_wts(int patt)
{
int weight, neurode;
double learn,alph,delta;

learn = BETA;
alph  = ALPHA;
for (neurode=0; neurode<MID_SIZE; neurode++)
{
	for (weight=0; weight<IN_SIZE; weight++)
	{
		/* first the basic delta rule */
		delta = learn * mid_error[neurode] * in_pats[patt][weight];

		/* with the momentum term */
		delta += alph * mid_wt_change[neurode][weight];
		mid_wts[neurode][weight] += delta;

		/* keep track of this pass's cum wt changes for next pass's momentum */
		mid_wt_cum_change[neurode][weight] += delta;
	}
}
return;
}

/*******************
** do_back_pass() **
********************
** Process the backward propagation of error through network.
**/
void  do_back_pass(int patt)
{

do_out_error(patt);
do_mid_error();
adjust_out_wts();
adjust_mid_wts(patt);

return;
}


/**********************
** move_wt_changes() **
***********************
** Move the weight changes accumulated last pass into the wt-change
** array for use by the momentum term in this pass. Also zero out
** the accumulating arrays after the move.
**/
static void move_wt_changes()
{
int i,j;

for (i = 0; i<MID_SIZE; i++)
	for (j = 0; j<IN_SIZE; j++)
	{
		mid_wt_change[i][j] = mid_wt_cum_change[i][j];
		/*
		** Zero it out for next pass accumulation.
		*/
		mid_wt_cum_change[i][j] = 0.0;
	}

for (i = 0; i<OUT_SIZE; i++)
	for (j=0; j<MID_SIZE; j++)
	{
		out_wt_change[i][j] = out_wt_cum_change[i][j];
		out_wt_cum_change[i][j] = 0.0;
	}

return;
}

/**********************
** check_out_error() **
***********************
** Check to see if the error in the output layer is below
** MARGIN*OUT_SIZE for all output patterns.  If so, then
** assume the network has learned acceptably well.  This
** is simply an arbitrary measure of how well the network
** has learned -- many other standards are possible.
**/
static int check_out_error()
{
int result,i,error;

result  = T;
error   = F;
worst_pass_error();     /* identify the worst error in this pass */

/*
#ifdef DEBUG
printf("\n Iteration # %d",iteration_count);
#endif
*/
for (i=0; i<numpats; i++)
{
/*      printf("\n Error pattern %d:   Worst: %8.3f; Average: %8.3f",
	  i+1,tot_out_error[i], avg_out_error[i]);
	fprintf(outfile,
	 "\n Error pattern %d:   Worst: %8.3f; Average: %8.3f",
	 i+1,tot_out_error[i]);
*/

	if (worst_error >= STOP) result = F;
	if (tot_out_error[i] >= 16.0) error = T;
}

if (error == T) result = ERR;


#ifdef DEBUG
/* printf("\n Error this pass thru data:   Worst: %8.3f; Average: %8.3f",
 worst_error,average_error);
*/
/* fprintf(outfile,
 "\n Error this pass thru data:   Worst: %8.3f; Average: %8.3f",
  worst_error, average_error); */
#endif

return(result);
}


/*******************
** zero_changes() **
********************
** Zero out all the wt change arrays
**/
static void zero_changes()
{
int i,j;

for (i = 0; i<MID_SIZE; i++)
{
	for (j=0; j<IN_SIZE; j++)
	{
		mid_wt_change[i][j] = 0.0;
		mid_wt_cum_change[i][j] = 0.0;
	}
}

for (i = 0; i< OUT_SIZE; i++)
{
	for (j=0; j<MID_SIZE; j++)
	{
		out_wt_change[i][j] = 0.0;
		out_wt_cum_change[i][j] = 0.0;
	}
}
return;
}


/********************
** randomize_wts() **
*********************
** Intialize the weights in the middle and output layers to
** random values between -0.25..+0.25
** Function rand() returns a value between 0 and 32767.
**
** NOTE: Had to make alterations to how the random numbers were
** created.  -- RG.
**/
static void randomize_wts()
{
int neurode,i;
double value;

/*
** Following not used int benchmark version -- RG
**
**        printf("\n Please enter a random number seed (1..32767):  ");
**        scanf("%d", &i);
**        srand(i);
*/

for (neurode = 0; neurode<MID_SIZE; neurode++)
{
	for(i=0; i<IN_SIZE; i++)
	{
	        /* value=(double)abs_randwc(100000L); */
		value=(double)abs_randwc((int32)100000);
		value=value/(double)100000.0 - (double) 0.5;
		mid_wts[neurode][i] = value/2;
	}
}
for (neurode=0; neurode<OUT_SIZE; neurode++)
{
	for(i=0; i<MID_SIZE; i++)
	{
	        /* value=(double)abs_randwc(100000L); */
		value=(double)abs_randwc((int32)100000);
		value=value/(double)10000.0 - (double) 0.5;
		out_wts[neurode][i] = value/2;
	}
}

return;
}


/* 
 * printf: 
 *   Invokes OCALL to display the enclave buffer to the terminal.
 */
void printf(const char *fmt, ...)
{
    char buf[BUFSIZ] = {'\0'};
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    ocall_print_string(buf);
}


/*************************
** LoadNumArrayWithRand **
**************************
** Load up an array with random longs.
*/
void encl_LoadNumArrayWithRand( /* Do not include param for pointer to arrays; we store them internally */
		unsigned long arraysize,
		unsigned int numarrays)         /* # of elements in array */
{
long i;                 /* Used for index */
long *darray;        /* Destination array pointer */
long *array = (long*)enclave_buffer; /* Use the enclave's buffer as the array pointer */

/*
** Initialize the random number generator
*/
/* randnum(13L); */
randnum((int32)13);

/*
** Load up first array with randoms
*/
for(i=0L;i<arraysize;i++)
        /* array[i]=randnum(0L); */
	array[i]=randnum((int32)0);

/*
** Now, if there's more than one array to load, copy the
** first into each of the others.
*/
darray=array;
while(--numarrays)
{       darray+=arraysize;
	for(i=0L;i<arraysize;i++)
		darray[i]=array[i];
}

return;
}

/****************
** NumHeapSort **
*****************
** Pass this routine a pointer to an array of long
** integers.  Also pass in minimum and maximum offsets.
** This routine performs a heap sort on that array.
*/
void encl_NumHeapSort( unsigned long base_offset,/* Again, don't pass the array pointer, but we do need an offset. */
	unsigned long bottom,           /* Lower bound */
	unsigned long top)              /* Upper bound */
{
unsigned long temp;                     /* Used to exchange elements */
unsigned long i;                        /* Loop index */
long *array = (long*)enclave_buffer+base_offset;

/*
** First, build a heap in the array
*/
for(i=(top/2L); i>0; --i)
	NumSift(array,i,top);

/*
** Repeatedly extract maximum from heap and place it at the
** end of the array.  When we get done, we'll have a sorted
** array.
*/
for(i=top; i>0; --i)
{       NumSift(array,bottom,i);
	temp=*array;                    /* Perform exchange */
	*array=*(array+i);
	*(array+i)=temp;
}
return;
}


void test_function(const char *str){
    printf(str);
}

int add(int x, int y){
    return x+y;
}

void nothing(){
    return;
}

unsigned long encl_LoadStringArray(unsigned int numarrays,unsigned long arraysize)          
{
	unsigned long *optrarray;
	unsigned char *strarray;
	unsigned char *tempsbase;            
	unsigned long *tempobase;           
	unsigned long curroffset;       /* Current offset */
	int fullflag;                   /* Indicates full array */
	unsigned char stringlength;     /* Length of string */
	unsigned char i;                /* Index */
	unsigned long j;                /* Another index */
	unsigned int k;                 /* Yet another index */
	unsigned int l;                 /* Ans still one more index 			*/
	int systemerror;                /* For holding error code */
	unsigned long nstrings;         /* # of strings in array */

	strarray= (unsigned char *)enclave_buffer;
/*
** Initialize random number generator.
*/
/* randnum(13L); */
randnum((int32)13);
/*
** Start with no strings.  Initialize our current offset pointer
** to 0.
*/
nstrings=0L;
curroffset=0L;
fullflag=0;

do
{
	/*
	** Allocate a string with a random length no
	** shorter than 4 bytes and no longer than
	** 80 bytes.  Note we have to also make sure
	** there's room in the array.
	*/
        /* stringlength=(unsigned char)((1+abs_randwc(76L)) & 0xFFL);*/
	stringlength=(unsigned char)((1+abs_randwc((int32)76)) & 0xFFL);
	if((unsigned long)stringlength+curroffset+1L>=arraysize)
	{       stringlength=(unsigned char)((arraysize-curroffset-1L) &
				0xFF);
		fullflag=1;     /* Indicates a full */
	}

	/*
	** Store length at curroffset and advance current offset.
	*/
	*(strarray+curroffset)=stringlength;
	curroffset++;

	/*
	** Fill up the rest of the string with random bytes.
	*/
	for(i=0;i<stringlength;i++)
	{       *(strarray+curroffset)=
		        /* (unsigned char)(abs_randwc((long)0xFE)); */
			(unsigned char)(abs_randwc((int32)0xFE));
		//printf("Hello: %c\n", *(strarray+curroffset) );
		curroffset++;
	}

	/*
	** Increment the # of strings counter.
	*/
	nstrings+=1L;

} while(fullflag==0);

/*
** We now have initialized a single full array.  If there
** is more than one array, copy the original into the
** others.
*/
k=1;
tempsbase=strarray;
while(k<numarrays)
{       tempsbase+=arraysize+100;         /* Set base */
	for(l=0;l<arraysize;l++)
		tempsbase[l]=strarray[l];
	k++;
}

/*
** Now the array is full, allocate enough space for an
** offset pointer array.
*/

encl_AllocateMemory2(nstrings * sizeof(unsigned long) * numarrays);

optrarray=(farulong *)enclave_buffer2;

if(!optrarray)
{     
        printf("FATAL: Enclave malloc returned NULL pointer\n");
}

/*
** Go through the newly-built string array, building
** offsets and putting them into the offset pointer
** array.
*/
curroffset=0;
for(j=0;j<nstrings;j++)
{       *(optrarray+j)=curroffset;
	curroffset+=(unsigned long)(*(strarray+curroffset))+1L;
}

/*
** As above, we've made one copy of the offset pointers,
** so duplicate this array in the remaining ones.
*/
k=1;
tempobase=optrarray;
while(k<numarrays)
{       tempobase+=nstrings;
	for(l=0;l<nstrings;l++)
		tempobase[l]=optrarray[l];
	k++;
}

/*
** All done...go home.  Pass local pointer back.
*/
enclave_buffer2=(void *)optrarray;

//printf("%lu\n",nstrings);
return(nstrings);
}

/****************
** strheapsort **
*****************
** Pass this routine a pointer to an array of unsigned char.
** The array is presumed to hold strings occupying at most
** 80 bytes (counts a byte count).
** This routine also needs a pointer to an array of offsets
** which represent string locations in the array, and
** an unsigned long indicating the number of strings
** in the array.
*/
void encl_StrHeapSort(farulong *optrarray, faruchar *strarray, unsigned long numstrings, unsigned long bottom, unsigned long top)
{
unsigned char temp[80];                 /* Used to exchange elements */
unsigned char tlen;                     /* Temp to hold length */
unsigned long i;                        /* Loop index */

//unsigned long *optrarray = oparrayOffset+(unsigned long *)enclave_buffer2;
//unsigned char *strarray = strarrayOffset+(unsigned char *)enclave_buffer;



/*
** Build a heap in the array
*/
for(i=(top/2L); i>0; --i)
	strsift(optrarray,strarray,numstrings,i,top);

/*
** Repeatedly extract maximum from heap and place it at the
** end of the array.  When we get done, we'll have a sorted
** array.
*/
for(i=top; i>0; --i)
{
	strsift(optrarray,strarray,numstrings,0,i);

	/* temp = string[0] */
	tlen=*strarray;
	MoveMemory((farvoid *)&temp[0], /* Perform exchange */
		(farvoid *)strarray,
		(unsigned long)(tlen+1));


	/* string[0]=string[i] */
	tlen=*(strarray+*(optrarray+i));
	stradjust(optrarray,strarray,numstrings,0,tlen);
	MoveMemory((farvoid *)strarray,
		(farvoid *)(strarray+*(optrarray+i)),
		(unsigned long)(tlen+1));

	/* string[i]=temp */
	tlen=temp[0];
	stradjust(optrarray,strarray,numstrings,i,tlen);
	MoveMemory((farvoid *)(strarray+*(optrarray+i)),
		(farvoid *)&temp[0],
		(unsigned long)(tlen+1));

}
return;
}


void encl_call_StrHeapSort(unsigned long nstrings, unsigned int numarrays, unsigned long arraysize)
{
	farulong *tempobase;            /* Temporary offset pointer base */
	faruchar *tempsbase;            /* Temporary string base pointer */
	//unsigned long tempobase;            /* Temporary offset pointer base */
	//unsigned long tempsbase;            /* Temporary string base pointer */
	tempobase=(unsigned long *)enclave_buffer2;
	tempsbase=(unsigned char *)enclave_buffer;
//unsigned long *optrarray = oparrayOffset+(unsigned long *)enclave_buffer2;
//unsigned char *strarray = strarrayOffset+(unsigned char *)enclave_buffer;	


//tempobase=optrarray;
//tempsbase=arraybase;
	for(int i=0;i<numarrays;i++)
{	       
//printf("(Before) Tempobase: %lu\n",*tempobase);	
	//printf("(Before) Tempsbase: %d %d %d %d\n",tempsbase[0],tempsbase[1],tempsbase[2],tempsbase[3] );
	encl_StrHeapSort(tempobase,tempsbase,nstrings,0L,nstrings-1);
	//printf("Tempobase: %lu\n",*tempobase);	
	//printf("Tempsbase: %d\n",*tempsbase);	
	tempobase+=nstrings;    /* Advance base pointers */
	tempsbase+=arraysize+100;
}

}

/************************
** DoFPUTransIteration **
*************************
** Perform an iteration of the FPU Transcendental/trigonometric
** benchmark.  Here, an iteration consists of calculating the
** first n fourier coefficients of the function (x+1)^x on
** the interval 0,2.  n is given by arraysize.
** NOTE: The # of integration steps is fixed at
** 200.
*/
void encl_DoFPUTransIteration(ulong arraysize)                /* # of coeffs */
{
    double omega;           /* Fundamental frequency */
    unsigned long i;        /* Index */
    unsigned long elapsed;  /* Elapsed time */

    volatile fardouble *abase = (fardouble *)enclave_buffer;
    volatile fardouble *bbase = (fardouble *)enclave_buffer2;

    /*
    ** Calculate the fourier series.  Begin by
    ** calculating A[0].
    */

    *abase=TrapezoidIntegrate((double)0.0,
                        (double)2.0,
                        200,
                        (double)0.0,    /* No omega * n needed */
                        0 )/(double)2.0;

    /*
    ** Calculate the fundamental frequency.
    ** ( 2 * pi ) / period...and since the period
    ** is 2, omega is simply pi.
    */
    omega=(double)3.1415926535897932;

    for(i=1;i<arraysize;i++)
    {

            /*
             ** Calculate A[i] terms.  Note, once again, that we
             ** can ignore the 2/period term outside the integral
             ** since the period is 2 and the term cancels itself
             ** out.
             */
            *(abase+i)=TrapezoidIntegrate((double)0.0,
                                (double)2.0,
                                200,
                                omega * (double)i,
                                1);

                /*
                 ** Calculate the B[i] terms.
                 */
                *(bbase+i)=TrapezoidIntegrate((double)0.0,
                                    (double)2.0,
                                    200,
                                    omega * (double)i,
                                    2);

    }
#ifdef DEBUG
    {
          int i;
          printf("\nA[i]=\n");
          for (i=0;i<arraysize;i++) printf("%7.3g ",abase[i]);
          printf("\nB[i]=\n(undefined) ");
          for (i=1;i<arraysize;i++) printf("%7.3g ",bbase[i]);
    }
#endif
}

////////////////////////////Assignment Begin////////////////////////////////////////////////

/***************
** LoadAssign **
****************
** The array given by arraybase is loaded with positive random
** numbers.  Elements in the array are capped at 5,000,000.
*/
static void LoadAssign(farlong arraybase[][ASSIGNCOLS])
{
	ushort i,j;

/*
** Reset random number generator so things repeat.
*/
/* randnum(13L); */
randnum((int32)13);

for(i=0;i<ASSIGNROWS;i++)
  for(j=0;j<ASSIGNROWS;j++){
    /* arraybase[i][j]=abs_randwc(5000000L);*/
    arraybase[i][j]=abs_randwc((int32)5000000);
  }

return;
}

/*****************
** CopyToAssign **
******************
** Copy the contents of one array to another.  This is called by
** the routine that builds the initial array, and is used to copy
** the contents of the intial array into all following arrays.
*/
static void CopyToAssign(farlong arrayfrom[ASSIGNROWS][ASSIGNCOLS],
		farlong arrayto[ASSIGNROWS][ASSIGNCOLS])
{
ushort i,j;

for(i=0;i<ASSIGNROWS;i++)
	for(j=0;j<ASSIGNCOLS;j++)
		arrayto[i][j]=arrayfrom[i][j];

return;
}


void encl_LoadAssignArrayWithRand(unsigned long numarrays)
{
	longptr abase,abase1;   /* Local for array pointer */
	ulong i;

	/*
	** Set local array pointer
	*/
	abase.ptrs.p=(long *)enclave_buffer;
	abase1.ptrs.p=(long *)enclave_buffer;

	/*
	** Set up the first array.  Then just copy it into the
	** others.
	*/
	LoadAssign(*(abase.ptrs.ap));
	if(numarrays>1)
	for(i=1;i<numarrays;i++)
	  {     /* abase1.ptrs.p+=i*ASSIGNROWS*ASSIGNCOLS; */
	        /* Fixed  by Eike Dierks */
	        abase1.ptrs.p+=ASSIGNROWS*ASSIGNCOLS;
		CopyToAssign(*(abase.ptrs.ap),*(abase1.ptrs.ap));
	}

return;


}

/***********************
** calc_minimum_costs **
************************
** Revise the tableau by calculating the minimum costs on a
** row and column basis.  These minima are subtracted from
** their rows and columns, creating a new tableau.
*/
static void calc_minimum_costs(long tableau[][ASSIGNCOLS])
{
ushort i,j;              /* Index variables */
long currentmin;        /* Current minimum */
/*
** Determine minimum costs on row basis.  This is done by
** subtracting -- on a row-per-row basis -- the minum value
** for that row.
*/
for(i=0;i<ASSIGNROWS;i++)
{
	currentmin=MAXPOSLONG;  /* Initialize minimum */
	for(j=0;j<ASSIGNCOLS;j++)
		if(tableau[i][j]<currentmin)
			currentmin=tableau[i][j];

	for(j=0;j<ASSIGNCOLS;j++)
		tableau[i][j]-=currentmin;
}

/*
** Determine minimum cost on a column basis.  This works
** just as above, only now we step through the array
** column-wise
*/
for(j=0;j<ASSIGNCOLS;j++)
{
	currentmin=MAXPOSLONG;  /* Initialize minimum */
	for(i=0;i<ASSIGNROWS;i++)
		if(tableau[i][j]<currentmin)
			currentmin=tableau[i][j];

	/*
	** Here, we'll take the trouble to see if the current
	** minimum is zero.  This is likely worth it, since the
	** preceding loop will have created at least one zero in
	** each row.  We can save ourselves a few iterations.
	*/
	if(currentmin!=0)
		for(i=0;i<ASSIGNROWS;i++)
			tableau[i][j]-=currentmin;
}

return;
}

/**********************
** first_assignments **
***********************
** Do first assignments.
** The assignedtableau[] array holds a set of values that
** indicate the assignment of a value, or its elimination.
** The values are:
**      0 = Item is neither assigned nor eliminated.
**      1 = Item is assigned
**      2 = Item is eliminated
** Returns the number of selections made.  If this equals
** the number of rows, then an optimum has been determined.
*/
static int first_assignments(long tableau[][ASSIGNCOLS],
		short assignedtableau[][ASSIGNCOLS])
{
ushort i,j,k;                   /* Index variables */
ushort numassigns;              /* # of assignments */
ushort totnumassigns;           /* Total # of assignments */
ushort numzeros;                /* # of zeros in row */
int selected=0;                 /* Flag used to indicate selection */

/*
** Clear the assignedtableau, setting all members to show that
** no one is yet assigned, eliminated, or anything.
*/
for(i=0;i<ASSIGNROWS;i++)
	for(j=0;j<ASSIGNCOLS;j++)
		assignedtableau[i][j]=0;

totnumassigns=0;
do {
	numassigns=0;
	/*
	** Step through rows.  For each one that is not currently
	** assigned, see if the row has only one zero in it.  If so,
	** mark that as an assigned row/col.  Eliminate other zeros
	** in the same column.
	*/
	for(i=0;i<ASSIGNROWS;i++)
	{       numzeros=0;
		for(j=0;j<ASSIGNCOLS;j++)
			if(tableau[i][j]==0L)
				if(assignedtableau[i][j]==0)
				{       numzeros++;
					selected=j;
				}
		if(numzeros==1)
		{       numassigns++;
			totnumassigns++;
			assignedtableau[i][selected]=1;
			for(k=0;k<ASSIGNROWS;k++)
				if((k!=i) &&
				   (tableau[k][selected]==0))
					assignedtableau[k][selected]=2;
		}
	}
	/*
	** Step through columns, doing same as above.  Now, be careful
	** of items in the other rows of a selected column.
	*/
	for(j=0;j<ASSIGNCOLS;j++)
	{       numzeros=0;
		for(i=0;i<ASSIGNROWS;i++)
			if(tableau[i][j]==0L)
				if(assignedtableau[i][j]==0)
				{       numzeros++;
					selected=i;
				}
		if(numzeros==1)
		{       numassigns++;
			totnumassigns++;
			assignedtableau[selected][j]=1;
			for(k=0;k<ASSIGNCOLS;k++)
				if((k!=j) &&
				   (tableau[selected][k]==0))
					assignedtableau[selected][k]=2;
		}
	}
	/*
	** Repeat until no more assignments to be made.
	*/
} while(numassigns!=0);

/*
** See if we can leave at this point.
*/
if(totnumassigns==ASSIGNROWS) return(totnumassigns);

/*
** Now step through the array by row.  If you find any unassigned
** zeros, pick the first in the row.  Eliminate all zeros from
** that same row & column.  This occurs if there are multiple optima...
** possibly.
*/
for(i=0;i<ASSIGNROWS;i++)
{       selected=-1;
	for(j=0;j<ASSIGNCOLS;j++)
		if((tableau[i][j]==0L) &&
		   (assignedtableau[i][j]==0))
		{       selected=j;
			break;
		}
	if(selected!=-1)
	{       assignedtableau[i][selected]=1;
		totnumassigns++;
		for(k=0;k<ASSIGNCOLS;k++)
			if((k!=selected) &&
			   (tableau[i][k]==0L))
				assignedtableau[i][k]=2;
		for(k=0;k<ASSIGNROWS;k++)
			if((k!=i) &&
			   (tableau[k][selected]==0L))
				assignedtableau[k][selected]=2;
	}
}

return(totnumassigns);
}

/***********************
** second_assignments **
************************
** This section of the algorithm creates the revised
** tableau, and is difficult to explain.  I suggest you
** refer to the algorithm's source, mentioned in comments
** toward the beginning of the program.
*/
static void second_assignments(long tableau[][ASSIGNCOLS],
		short assignedtableau[][ASSIGNCOLS])
{
int i,j;                                /* Indexes */
short linesrow[ASSIGNROWS];
short linescol[ASSIGNCOLS];
long smallest;                          /* Holds smallest value */
ushort numassigns;                      /* Number of assignments */
ushort newrows;                         /* New rows to be considered */
/*
** Clear the linesrow and linescol arrays.
*/
for(i=0;i<ASSIGNROWS;i++)
	linesrow[i]=0;
for(i=0;i<ASSIGNCOLS;i++)
	linescol[i]=0;

/*
** Scan rows, flag each row that has no assignment in it.
*/
for(i=0;i<ASSIGNROWS;i++)
{       numassigns=0;
	for(j=0;j<ASSIGNCOLS;j++)
		if(assignedtableau[i][j]==1)
		{       numassigns++;
			break;
		}
	if(numassigns==0) linesrow[i]=1;
}

do {

	newrows=0;
	/*
	** For each row checked above, scan for any zeros.  If found,
	** check the associated column.
	*/
	for(i=0;i<ASSIGNROWS;i++)
	{       if(linesrow[i]==1)
			for(j=0;j<ASSIGNCOLS;j++)
				if(tableau[i][j]==0)
					linescol[j]=1;
	}

	/*
	** Now scan checked columns.  If any contain assigned zeros, check
	** the associated row.
	*/
	for(j=0;j<ASSIGNCOLS;j++)
		if(linescol[j]==1)
			for(i=0;i<ASSIGNROWS;i++)
				if((assignedtableau[i][j]==1) &&
					(linesrow[i]!=1))
				{
					linesrow[i]=1;
					newrows++;
				}
} while(newrows!=0);

/*
** linesrow[n]==0 indicate rows covered by imaginary line
** linescol[n]==1 indicate cols covered by imaginary line
** For all cells not covered by imaginary lines, determine smallest
** value.
*/
smallest=MAXPOSLONG;
for(i=0;i<ASSIGNROWS;i++)
	if(linesrow[i]!=0)
		for(j=0;j<ASSIGNCOLS;j++)
			if(linescol[j]!=1)
				if(tableau[i][j]<smallest)
					smallest=tableau[i][j];

/*
** Subtract smallest from all cells in the above set.
*/
for(i=0;i<ASSIGNROWS;i++)
	if(linesrow[i]!=0)
		for(j=0;j<ASSIGNCOLS;j++)
			if(linescol[j]!=1)
				tableau[i][j]-=smallest;

/*
** Add smallest to all cells covered by two lines.
*/
for(i=0;i<ASSIGNROWS;i++)
	if(linesrow[i]==0)
		for(j=0;j<ASSIGNCOLS;j++)
			if(linescol[j]==1)
				tableau[i][j]+=smallest;

return;
}


/***************
** Assignment **
***************/
static void Assignment(farlong arraybase[][ASSIGNCOLS])
{
short assignedtableau[ASSIGNROWS][ASSIGNCOLS];

/*
** First, calculate minimum costs
*/
calc_minimum_costs(arraybase);

/*
** Repeat following until the number of rows selected
** equals the number of rows in the tableau.
*/
while(first_assignments(arraybase,assignedtableau)!=ASSIGNROWS)
{         second_assignments(arraybase,assignedtableau);
}
return;
}


void encl_call_AssignmentTest(unsigned int numarrays)
{
	longptr abase; 
	abase.ptrs.p= (long *)enclave_buffer;	
	

for(int i=0;i<numarrays;i++)
	{       /* abase.ptrs.p+=i*ASSIGNROWS*ASSIGNCOLS; */
        	/* Fixed  by Eike Dierks */
		Assignment(*abase.ptrs.ap);
		abase.ptrs.p+=ASSIGNROWS*ASSIGNCOLS;
	}


}

void encl_app_loadIDEA(unsigned long arraysize)
{faruchar *plain1;
	
	for(int i=0;i<arraysize;i++)
	((unsigned char *)(enclave_buffer))[i]=(uchar)(abs_randwc(255) & 0xFF);
	
	plain1=(unsigned char *)enclave_buffer;	
	//printf("\nPlain1 Initialized: %d %d %d %d %d \n",plain1[0],plain1[1],plain1[2],plain1[3],plain1[4]);
}

/********
** mul **
*********
** Performs multiplication, modulo (2**16)+1.  This code is structured
** on the assumption that untaken branches are cheaper than taken
** branches, and that the compiler doesn't schedule branches.
*/
static u16 mul(register u16 a, register u16 b)
{
register u32 p;
if(a)
{       if(b)
	{       p=(u32)(a*b);
		b=low16(p);
		a=(u16)(p>>16);
		return(b-a+(b<a));
	}
	else
		return(1-a);
}
else
	return(1-b);
}

/****************
** cipher_idea **
*****************
** IDEA encryption/decryption algorithm.
*/
static void cipher_idea(u16 in[4],
		u16 out[4],
		register IDEAkey Z)
{



register u16 x1, x2, x3, x4, t1, t2;
/* register u16 t16;
register u16 t32; */
int r=ROUNDS;

x1=*in++;
x2=*in++;
x3=*in++;
x4=*in;

do {
	MUL(x1,*Z++);
	x2+=*Z++;
	x3+=*Z++;
	MUL(x4,*Z++);

	t2=x1^x3;
	MUL(t2,*Z++);
	t1=t2+(x2^x4);
	MUL(t1,*Z++);
	t2=t1+t2;

	x1^=t1;
	x4^=t2;

	t2^=x2;
	x2=x3^t1;
	x3=t2;
} while(--r);
MUL(x1,*Z++);
*out++=x1;
*out++=x3+*Z++;
*out++=x2+*Z++;
MUL(x4,*Z);
*out=x4;
return;
}


void encl_callIDEA(unsigned long arraysize, unsigned short * Z, unsigned short * DK, unsigned long nloops)
{
	faruchar *plain1;
	faruchar *crypt1;
	faruchar *plain2;
	
	register ulong i;
	register ulong j;

	plain1=(unsigned char *)enclave_buffer;
	crypt1=(unsigned char *)enclave_buffer2;
	plain2=(unsigned char *)enclave_buffer3;

	for(int i=0;i<nloops;i++)
	{
		for(j=0;j<arraysize;j+=(sizeof(u16)*4))
			cipher_idea(((u16 *)(plain1+j)),((u16 *)(crypt1+j)),Z);       /* Encrypt */

		for(j=0;j<arraysize;j+=(sizeof(u16)*4))
			cipher_idea(((u16 *)(crypt1+j)),((u16 *)(plain2+j)),DK);      /* Decrypt */
	}	
}
/* Neural Net */
void encl_set_numpats(int npats)
{
    numpats = npats;
}

double encl_get_in_pats(int patt, int element)
{
    return in_pats[patt][element];
}

void encl_set_in_pats(int patt, int element, double val)
{
    in_pats[patt][element] = val;
}

void encl_set_out_pats(int patt, int element, double val)
{
    out_pats[patt][element] = val;
}

void encl_DoNNetIteration(unsigned long nloops)
{
int patt;
randnum((int32)3);    /* Gotta do this for Neural Net */
while(nloops--)
{
	randomize_wts();
	zero_changes();
	iteration_count=1;
	learned = F;
	numpasses = 0;
	while (learned == F)
	{
		for (patt=0; patt<numpats; patt++)
		{
			worst_error = 0.0;      /* reset this every pass through data */
			move_wt_changes();      /* move last pass's wt changes to momentum array */
			do_forward_pass(patt);
			do_back_pass(patt);
			iteration_count++;
		}
		numpasses ++;
		learned = check_out_error();
	}
#ifdef DEBUG
printf("Learned in %d passes\n",numpasses);
#endif
}
}

//////////////////////////////////////////LuDecomposition/////////////////////////////////////////////////////////
/******************
** build_problem **
*******************
** Constructs a solvable set of linear equations.  It does this by
** creating an identity matrix, then loading the solution vector
** with random numbers.  After that, the identity matrix and
** solution vector are randomly "scrambled".  Scrambling is
** done by (a) randomly selecting a row and multiplying that
** row by a random number and (b) adding one randomly-selected
** row to another.
*/
void encl_build_problem()
{

double *b= (double *)enclave_buffer2; 
LUdblptr ptra;
ptra.ptrs.p=(double *)enclave_buffer; 
double (&a)[][LUARRAYCOLS] = *ptra.ptrs.ap;

int n = LUARRAYROWS;

long i,j,k,k1;  /* Indexes */
double rcon;     /* Random constant */

/*
** Reset random number generator
*/
/* randnum(13L); */
randnum((int32)13);

/*
** Build an identity matrix.
** We'll also use this as a chance to load the solution
** vector.
*/
for(i=0;i<n;i++)
{       /* b[i]=(double)(abs_randwc(100L)+1L); */
	b[i]=(double)(abs_randwc((int32)100)+(int32)1);
	for(j=0;j<n;j++)
		if(i==j)
		        /* a[i][j]=(double)(abs_randwc(1000L)+1L); */
			a[i][j]=(double)(abs_randwc((int32)1000)+(int32)1);
		else
			a[i][j]=(double)0.0;
}


/*
** Scramble.  Do this 8n times.  See comment above for
** a description of the scrambling process.
*/


for(i=0;i<8*n;i++)
{
	k=abs_randwc((int32)n);
	k1=abs_randwc((int32)n);
	if(k!=k1)
	{
		if(k<k1) rcon=(double)1.0;
			else rcon=(double)-1.0;
		for(j=0;j<n;j++)
			a[k][j]+=a[k1][j]*rcon;;
		b[k]+=b[k1]*rcon;
	}
}

return;
}

/***********
** ludcmp **
************
** From the procedure of the same name in "Numerical Recipes in Pascal",
** by Press, Flannery, Tukolsky, and Vetterling.
** Given an nxn matrix a[], this routine replaces it by the LU
** decomposition of a rowwise permutation of itself.  a[] and n
** are input.  a[] is output, modified as follows:
**   --                       --
**  |  b(1,1) b(1,2) b(1,3)...  |
**  |  a(2,1) b(2,2) b(2,3)...  |
**  |  a(3,1) a(3,2) b(3,3)...  |
**  |  a(4,1) a(4,2) a(4,3)...  |
**  |  ...                      |
**   --                        --
**
** Where the b(i,j) elements form the upper triangular matrix of the
** LU decomposition, and the a(i,j) elements form the lower triangular
** elements.  The LU decomposition is calculated so that we don't
** need to store the a(i,i) elements (which would have laid along the
** diagonal and would have all been 1).
**
** indx[] is an output vector that records the row permutation
** effected by the partial pivoting; d is output as +/-1 depending
** on whether the number of row interchanges was even or odd,
** respectively.
** Returns 0 if matrix singular, else returns 1.
*/
static int ludcmp(double a[][LUARRAYCOLS],
		int n,
		int indx[],
		int *d)
{

double big;     /* Holds largest element value */
double sum;
double dum;     /* Holds dummy value */
int i,j,k;      /* Indexes */
int imax=0;     /* Holds max index value */
double tiny;    /* A really small number */
fardouble *LUtempvv=(double *)enclave_buffer;
tiny=(double)1.0e-20;

*d=1;           /* No interchanges yet */

for(i=0;i<n;i++)
{       big=(double)0.0;
	for(j=0;j<n;j++)
		if((double)fabs(a[i][j]) > big)
			big=fabs(a[i][j]);
	/* Bail out on singular matrix */
	if(big==(double)0.0) return(0);
	LUtempvv[i]=1.0/big;
}

/*
** Crout's algorithm...loop over columns.
*/
for(j=0;j<n;j++)
{       if(j!=0)
		for(i=0;i<j;i++)
		{       sum=a[i][j];
			if(i!=0)
				for(k=0;k<i;k++)
					sum-=(a[i][k]*a[k][j]);
			a[i][j]=sum;
		}
	big=(double)0.0;
	for(i=j;i<n;i++)
	{       sum=a[i][j];
		if(j!=0)
			for(k=0;k<j;k++)
				sum-=a[i][k]*a[k][j];
		a[i][j]=sum;
		dum=LUtempvv[i]*fabs(sum);
		if(dum>=big)
		{       big=dum;
			imax=i;
		}
	}
	if(j!=imax)             /* Interchange rows if necessary */
	{       for(k=0;k<n;k++)
		{       dum=a[imax][k];
			a[imax][k]=a[j][k];
			a[j][k]=dum;
		}
		*d=-*d;         /* Change parity of d */
		dum=LUtempvv[imax];
		LUtempvv[imax]=LUtempvv[j]; /* Don't forget scale factor */
		LUtempvv[j]=dum;
	}
	indx[j]=imax;
	/*
	** If the pivot element is zero, the matrix is singular
	** (at least as far as the precision of the machine
	** is concerned.)  We'll take the original author's
	** recommendation and replace 0.0 with "tiny".
	*/
	if(a[j][j]==(double)0.0)
		a[j][j]=tiny;

	if(j!=(n-1))
	{       dum=1.0/a[j][j];
		for(i=j+1;i<n;i++)
			a[i][j]=a[i][j]*dum;
	}
}

return(1);
}

/***********
** lubksb **
************
** Also from "Numerical Recipes in Pascal".
** This routine solves the set of n linear equations A X = B.
** Here, a[][] is input, not as the matrix A, but as its
** LU decomposition, created by the routine ludcmp().
** Indx[] is input as the permutation vector returned by ludcmp().
**  b[] is input as the right-hand side an returns the
** solution vector X.
** a[], n, and indx are not modified by this routine and
** can be left in place for different values of b[].
** This routine takes into account the possibility that b will
** begin with many zero elements, so it is efficient for use in
** matrix inversion.
*/
static void lubksb( double a[][LUARRAYCOLS],
		int n,
		int indx[LUARRAYROWS],
		double b[LUARRAYROWS])
{

int i,j;        /* Indexes */
int ip;         /* "pointer" into indx */
int ii;
double sum;

/*
** When ii is set to a positive value, it will become
** the index of the first nonvanishing element of b[].
** We now do the forward substitution. The only wrinkle
** is to unscramble the permutation as we go.
*/
ii=-1;
for(i=0;i<n;i++)
{       ip=indx[i];
	sum=b[ip];
	b[ip]=b[i];
	if(ii!=-1)
		for(j=ii;j<i;j++)
			sum=sum-a[i][j]*b[j];
	else
		/*
		** If a nonzero element is encountered, we have
		** to do the sums in the loop above.
		*/
		if(sum!=(double)0.0)
			ii=i;
	b[i]=sum;
}
/*
** Do backsubstitution
*/
for(i=(n-1);i>=0;i--)
{
	sum=b[i];
	if(i!=(n-1))
		for(j=(i+1);j<n;j++)
			sum=sum-a[i][j]*b[j];
	b[i]=sum/a[i][i];
}
return;
}

/************
** lusolve **
*************
** Solve a linear set of equations: A x = b
** Original matrix A will be destroyed by this operation.
** Returns 0 if matrix is singular, 1 otherwise.
*/
static int lusolve(double a[][LUARRAYCOLS],
		int n,
		double b[LUARRAYROWS])
{
int indx[LUARRAYROWS];
int d;
#ifdef DEBUG
int i,j;
#endif


if(ludcmp(a,n,indx,&d)==0) return(0);

/* Matrix not singular -- proceed */
lubksb(a,n,indx,b);
//printf("hello\n");
#ifdef DEBUG
printf("Solution:\n");
for(i=0;i<n;i++)
{
  for(j=0;j<n;j++){
  /*
    printf("%6.2f ",a[i][j]);
  */
  }
  printf("%6.2f\t",b[i]);
  /*
    printf("\n");
  */
}
printf("\n");
#endif

return(1);
}



void encl_moveSeedArrays(unsigned long numarrays)
{
fardouble *a=(double *)enclave_buffer;
fardouble *b=(double *)enclave_buffer2;
fardouble *abase=(double *)enclave_buffer4;
fardouble *bbase=(double *)enclave_buffer5;
fardouble *locabase;
fardouble *locbbase;
ulong j,i; 

for(j=0;j<numarrays;j++)
	{       locabase=abase+j*LUARRAYROWS*LUARRAYCOLS;
		locbbase=bbase+j*LUARRAYROWS;
		for(i=0;i<LUARRAYROWS*LUARRAYCOLS;i++)
			*(locabase+i)=*(a+i);
		for(i=0;i<LUARRAYROWS;i++)
			*(locbbase+i)=*(b+i);
	}

}


void encl_call_lusolve(unsigned long numarrays)
{
fardouble *a=(double *)enclave_buffer;
fardouble *b=(double *)enclave_buffer2;
fardouble *abase=(double *)enclave_buffer4;
fardouble *bbase=(double *)enclave_buffer5;
fardouble *locabase;
fardouble *locbbase;
ulong i; 
LUdblptr ptra;
//ptra.ptrs.p=(double *)enclave_buffer; 



for(i=0;i<numarrays;i++)
{       locabase=abase+i*LUARRAYROWS*LUARRAYCOLS;
	locbbase=bbase+i*LUARRAYROWS;
	ptra.ptrs.p=locabase;
	lusolve(*ptra.ptrs.ap,LUARRAYROWS,locbbase);
}


}



/***************
** SetCompBit **
****************
** Set a bit in the compression array.  The value of the
** bit is set according to char bitchar.
*/
static void SetCompBit(u8 *comparray,
		u32 bitoffset,
		char bitchar)
{
u32 byteoffset;
int bitnumb;

/*
** First calculate which element in the comparray to
** alter. and the bitnumber.
*/
byteoffset=bitoffset>>3;
bitnumb=bitoffset % 8;

/*
** Set or clear
*/
if(bitchar=='1')
	comparray[byteoffset]|=(1<<bitnumb);
else
	comparray[byteoffset]&=~(1<<bitnumb);

return;
}
/*********************
** create_text_line **
**********************
** Create a random line of text, stored at *dt.  The line may be
** no more than nchars long.
*/
static void create_text_line(farchar *dt,
			long nchars)
{
long charssofar;        /* # of characters so far */
long tomove;            /* # of characters to move */
char myword[40];        /* Local buffer for words */
farchar *wordptr;       /* Pointer to word from catalog */

charssofar=0;

do {
/*
** Grab a random word from the wordcatalog
*/
/* wordptr=wordcatarray[abs_randwc((long)WORDCATSIZE)];*/
wordptr=wordcatarray[abs_randwc((int32)WORDCATSIZE)];
MoveMemory((farvoid *)myword,
	(farvoid *)wordptr,
	(unsigned long)strlen(wordptr)+1);

/*
** Append a blank.
*/
tomove=strlen(myword)+1;
myword[tomove-1]=' ';

/*
** See how long it is.  If its length+charssofar > nchars, we have
** to trim it.
*/
if((tomove+charssofar)>nchars)
	tomove=nchars-charssofar;
/*
** Attach the word to the current line.  Increment counter.
*/
MoveMemory((farvoid *)dt,(farvoid *)myword,(unsigned long)tomove);
charssofar+=tomove;
dt+=tomove;

/*
** If we're done, bail out.  Otherwise, go get another word.
*/
} while(charssofar<nchars);

return;
}

/**********************
** create_text_block **
***********************
** Build a block of text randomly loaded with words.  The words
** come from the wordcatalog (which must be loaded before you
** call this).
** *tb points to the memory where the text is to be built.
** tblen is the # of bytes to put into the text block
** maxlinlen is the maximum length of any line (line end indicated
**  by a carriage return).
*/
static void create_text_block(farchar *tb,
			ulong tblen,
			ushort maxlinlen)
{
ulong bytessofar;       /* # of bytes so far */
ulong linelen;          /* Line length */

bytessofar=0L;
do {

/*
** Pick a random length for a line and fill the line.
** Make sure the line can fit (haven't exceeded tablen) and also
** make sure you leave room to append a carriage return.
*/
linelen=abs_randwc(maxlinlen-6)+6;
if((linelen+bytessofar)>tblen)
	linelen=tblen-bytessofar;

if(linelen>1)
{
	create_text_line(tb,linelen);
}
tb+=linelen-1;          /* Add the carriage return */
*tb++='\n';

bytessofar+=linelen;

} while(bytessofar<tblen);

}


/***************
** GetCompBit **
****************
** Return the bit value of a bit in the comparession array.
** Returns 0 if the bit is clear, nonzero otherwise.
*/
static int GetCompBit(u8 *comparray,
		u32 bitoffset)
{
u32 byteoffset;
int bitnumb;

/*
** Calculate byte offset and bit number.
*/
byteoffset=bitoffset>>3;
bitnumb=bitoffset % 8;

/*
** Fetch
*/
return((1<<bitnumb) & comparray[byteoffset] );
}



void encl_buildHuffman(unsigned long arraysize)
{

	HuffStruct *lochuffstruct;      /* Loc pointer to global data */
	farchar *plaintext;
	plaintext = (char *)enclave_buffer;	

	//lochuffstruct={0,0,arraysize,0,0};

	randnum((int32)13);

	create_text_block(plaintext,arraysize-1,(ushort)500);
	plaintext[arraysize-1L]='\0';
	plaintextlen=arraysize;

}



void encl_callHuffman(unsigned long nloops, unsigned long arraysize)
{
	farchar *plaintext;
	farchar *comparray;	
	farchar *decomparray;
	huff_node * hufftree;


	plaintext = (char *)enclave_buffer;	
	comparray = (char *)enclave_buffer2;
	decomparray = (char *)enclave_buffer3;
	hufftree = (huff_node *)enclave_buffer4;


	int i;                          /* Index */
	long j;                         /* Bigger index */
	int root;                       /* Pointer to huffman tree root */
	float lowfreq1, lowfreq2;       /* Low frequency counters */
	int lowidx1, lowidx2;           /* Indexes of low freq. elements */
	long bitoffset;                 /* Bit offset into text */
	long textoffset;                /* Char offset into text */
	long maxbitoffset;              /* Holds limit of bit offset */
	long bitstringlen;              /* Length of bitstring */
	int c;                          /* Character from plaintext */
	char bitstring[30];             /* Holds bitstring */
	ulong elapsed; 







while(nloops--)
{


/*
** Calculate the frequency of each byte value. Store the
** results in what will become the "leaves" of the
** Huffman tree.  Interior nodes will be built in those
** nodes greater than node #255.
*/

for(i=0;i<256;i++)
{		
	hufftree[i].freq=(float)0.0;
	hufftree[i].c=(unsigned char)i;

}

for(j=0;j<arraysize;j++)
	hufftree[(int)plaintext[j]].freq+=(float)1.0;

for(i=0;i<256;i++)
	if(hufftree[i].freq != (float)0.0)
		hufftree[i].freq/=(float)arraysize;

/* Reset the second half of the tree. Otherwise the loop below that
** compares the frequencies up to index 512 makes no sense. Some
** systems automatically zero out memory upon allocation, others (like
** for example DEC Unix) do not. Depending on this the loop below gets
** different data and different run times. On our alpha the data that
** was arbitrarily assigned led to an underflow error at runtime. We
** use that zeroed-out bits are in fact 0 as a float.
** Uwe F. Mayer */
bzero((char *)&(hufftree[256]),sizeof(huff_node)*256);
/*
** Build the huffman tree.  First clear all the parent
** pointers and left/right pointers.  Also, discard all
** nodes that have a frequency of true 0.  */
for(i=0;i<512;i++)
{       if(hufftree[i].freq==(float)0.0)
		hufftree[i].parent=EXCLUDED;
	else
		hufftree[i].parent=hufftree[i].left=hufftree[i].right=-1;
}

/*
** Go through the tree. Finding nodes of really low
** frequency.
*/
root=255;                       /* Starting root node-1 */
while(1)
{
	lowfreq1=(float)2.0; lowfreq2=(float)2.0;
	lowidx1=-1; lowidx2=-1;
	/*
	** Find first lowest frequency.
	*/
	for(i=0;i<=root;i++)
		if(hufftree[i].parent<0)
			if(hufftree[i].freq<lowfreq1)
			{       lowfreq1=hufftree[i].freq;
				lowidx1=i;
			}

	/*
	** Did we find a lowest value?  If not, the
	** tree is done.
	*/
	if(lowidx1==-1) break;

	/*
	** Find next lowest frequency
	*/
	for(i=0;i<=root;i++)
		if((hufftree[i].parent<0) && (i!=lowidx1))
			if(hufftree[i].freq<lowfreq2)
			{       lowfreq2=hufftree[i].freq;
				lowidx2=i;
			}

	/*
	** If we could only find one item, then that
	** item is surely the root, and (as above) the
	** tree is done.
	*/
	if(lowidx2==-1) break;

	/*
	** Attach the two new nodes to the current root, and
	** advance the current root.
	*/
	root++;                 /* New root */
	hufftree[lowidx1].parent=root;
	hufftree[lowidx2].parent=root;
	hufftree[root].freq=lowfreq1+lowfreq2;
	hufftree[root].left=lowidx1;
	hufftree[root].right=lowidx2;
	hufftree[root].parent=-2;       /* Show root */
}

/*
** Huffman tree built...compress the plaintext
*/
bitoffset=0L;                           /* Initialize bit offset */
for(i=0;i<arraysize;i++)
{
	c=(int)plaintext[i];                 /* Fetch character */
	/*
	** Build a bit string for byte c
	*/
	bitstringlen=0;
	while(hufftree[c].parent!=-2)
	{       if(hufftree[hufftree[c].parent].left==c)
			bitstring[bitstringlen]='0';
		else
			bitstring[bitstringlen]='1';
		c=hufftree[c].parent;
		bitstringlen++;
	}

	/*
	** Step backwards through the bit string, setting
	** bits in the compressed array as you go.
	*/
	while(bitstringlen--)
	{       SetCompBit((u8 *)comparray,(u32)bitoffset,bitstring[bitstringlen]);
		bitoffset++;
	}
}

/*
** Compression done.  Perform de-compression.
*/
maxbitoffset=bitoffset;
bitoffset=0;
textoffset=0;
do {
	i=root;
	while(hufftree[i].left!=-1)
	{       if(GetCompBit((u8 *)comparray,(u32)bitoffset)==0)
			i=hufftree[i].left;
		else
			i=hufftree[i].right;
		bitoffset++;
	}
	decomparray[textoffset]=hufftree[i].c;

#ifdef DEBUG
	if(hufftree[i].c != plaintext[textoffset])
	{
		/* Show error */
		printf("Error at textoffset %ld\n",textoffset);
		status=1;
	}
#endif
	textoffset++;
} while(bitoffset<maxbitoffset);

}       /* End the big while(nloops--) from above */



}


















