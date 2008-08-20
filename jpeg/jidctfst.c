/*
 * jidctfst.c
 *
 * Copyright (C) 1994-1998, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains a fast, not so accurate integer implementation of the
 * inverse DCT (Discrete Cosine Transform).  In the IJG code, this routine
 * must also perform dequantization of the input coefficients.
 *
 * A 2-D IDCT can be done by 1-D IDCT on each column followed by 1-D IDCT
 * on each row (or vice versa, but it's more convenient to emit a row at
 * a time).  Direct algorithms are also available, but they are much more
 * complex and seem not to be any faster when reduced to code.
 *
 * This implementation is based on Arai, Agui, and Nakajima's algorithm for
 * scaled DCT.  Their original paper (Trans. IEICE E-71(11):1095) is in
 * Japanese, but the algorithm is described in the Pennebaker & Mitchell
 * JPEG textbook (see REFERENCES section in file README).  The following code
 * is based directly on figure 4-8 in P&M.
 * While an 8-point DCT cannot be done in less than 11 multiplies, it is
 * possible to arrange the computation so that many of the multiplies are
 * simple scalings of the final outputs.  These multiplies can then be
 * folded into the multiplications or divisions by the JPEG quantization
 * table entries.  The AA&N method leaves only 5 multiplies and 29 adds
 * to be done in the DCT itself.
 * The primary disadvantage of this method is that with fixed-point math,
 * accuracy is lost due to imprecise representation of the scaled
 * quantization values.  The smaller the quantization table entry, the less
 * precise the scaled value, so this implementation does worse with high-
 * quality-setting files than with low-quality ones.
 */

#define JPEG_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"
#include "jdct.h"		/* Private declarations for DCT subsystem */


#ifdef DCT_IFAST_SUPPORTED


/*
 * This module is specialized to the case DCTSIZE = 8.
 */

#if DCTSIZE != 8
  Sorry, this code only copes with 8x8 DCTs. /* deliberate syntax err */
#endif


/* Scaling decisions are generally the same as in the LL&M algorithm;
 * see jidctint.c for more details.  However, we choose to descale
 * (right shift) multiplication products as soon as they are formed,
 * rather than carrying additional fractional bits into subsequent additions.
 * This compromises accuracy slightly, but it lets us save a few shifts.
 * More importantly, 16-bit arithmetic is then adequate (for 8-bit samples)
 * everywhere except in the multiplications proper; this saves a good deal
 * of work on 16-bit-int machines.
 *
 * The dequantized coefficients are not integers because the AA&N scaling
 * factors have been incorporated.  We represent them scaled up by PASS1_BITS,
 * so that the first and second IDCT rounds have the same input scaling.
 * For 8-bit JSAMPLEs, we choose IFAST_SCALE_BITS = PASS1_BITS so as to
 * avoid a descaling shift; this compromises accuracy rather drastically
 * for small quantization table entries, but it saves a lot of shifts.
 * For 12-bit JSAMPLEs, there's no hope of using 16x16 multiplies anyway,
 * so we use a much larger scaling factor to preserve accuracy.
 *
 * A final compromise is to represent the multiplicative constants to only
 * 8 fractional bits, rather than 13.  This saves some shifting work on some
 * machines, and may also reduce the cost of multiplication (since there
 * are fewer one-bits in the constants).
 */

#if BITS_IN_JSAMPLE == 8
#define CONST_BITS  8
#define PASS1_BITS  2
#else
#define CONST_BITS  8
#define PASS1_BITS  1		/* lose a little precision to avoid overflow */
#endif

/* Some C compilers fail to reduce "FIX(constant)" at compile time, thus
 * causing a lot of useless floating-point operations at run time.
 * To get around this we use the following pre-calculated constants.
 * If you change CONST_BITS you may want to add appropriate values.
 * (With a reasonable C compiler, you can just rely on the FIX() macro...)
 */

#if CONST_BITS == 8
#define FIX_1_082392200  ((INT32)  277)		/* FIX(1.082392200) */
#define FIX_1_414213562  ((INT32)  362)		/* FIX(1.414213562) */
#define FIX_1_847759065  ((INT32)  473)		/* FIX(1.847759065) */
#define FIX_2_613125930  ((INT32)  669)		/* FIX(2.613125930) */
#else
#define FIX_1_082392200  FIX(1.082392200)
#define FIX_1_414213562  FIX(1.414213562)
#define FIX_1_847759065  FIX(1.847759065)
#define FIX_2_613125930  FIX(2.613125930)
#endif


/* We can gain a little more speed, with a further compromise in accuracy,
 * by omitting the addition in a descaling shift.  This yields an incorrectly
 * rounded result half the time...
 */

#ifndef USE_ACCURATE_ROUNDING
#undef DESCALE
#define DESCALE(x,n)  RIGHT_SHIFT(x, n)
#endif


/* Multiply a DCTELEM variable by an INT32 constant, and immediately
 * descale to yield a DCTELEM result.
 */

#define MULTIPLY(var,const)  ((DCTELEM) DESCALE((var) * (const), CONST_BITS))


/* Dequantize a coefficient by multiplying it by the multiplier-table
 * entry; produce a DCTELEM result.  For 8-bit data a 16x16->16
 * multiplication will do.  For 12-bit data, the multiplier table is
 * declared INT32, so a 32-bit multiply will be used.
 */

#if BITS_IN_JSAMPLE == 8
#define DEQUANTIZE(coef,quantval)  (((IFAST_MULT_TYPE) (coef)) * (quantval))
#else
#define DEQUANTIZE(coef,quantval)  \
	DESCALE((coef)*(quantval), IFAST_SCALE_BITS-PASS1_BITS)
#endif


/* Like DESCALE, but applies to a DCTELEM and produces an int.
 * We assume that int right shift is unsigned if INT32 right shift is.
 */

#ifdef RIGHT_SHIFT_IS_UNSIGNED
#define ISHIFT_TEMPS	DCTELEM ishift_temp;
#if BITS_IN_JSAMPLE == 8
#define DCTELEMBITS  16		/* DCTELEM may be 16 or 32 bits */
#else
#define DCTELEMBITS  32		/* DCTELEM must be 32 bits */
#endif
#define IRIGHT_SHIFT(x,shft)  \
    ((ishift_temp = (x)) < 0 ? \
     (ishift_temp >> (shft)) | ((~((DCTELEM) 0)) << (DCTELEMBITS-(shft))) : \
     (ishift_temp >> (shft)))
#else
#define ISHIFT_TEMPS
#define IRIGHT_SHIFT(x,shft)	((x) >> (shft))
#endif

#ifdef USE_ACCURATE_ROUNDING
#define IDESCALE(x,n)  ((int) IRIGHT_SHIFT((x) + (1 << ((n)-1)), n))
#else
#define IDESCALE(x,n)  ((int) IRIGHT_SHIFT(x, n))
#endif

#ifdef HAVE_MMX_INTEL_MNEMONICS
__inline GLOBAL(void)
jpeg_idct_ifast_mmx (j_decompress_ptr cinfo, jpeg_component_info * compptr,
		 JCOEFPTR coef_block,
		 JSAMPARRAY output_buf, JDIMENSION output_col);
__inline GLOBAL(void)
jpeg_idct_ifast_orig (j_decompress_ptr cinfo, jpeg_component_info * compptr,
		 JCOEFPTR coef_block,
		 JSAMPARRAY output_buf, JDIMENSION output_col);
#endif

#ifdef HAVE_SSE2_INTRINSICS
GLOBAL(void)
jpeg_idct_ifast_sse2 (j_decompress_ptr cinfo, jpeg_component_info * compptr,
                 JCOEFPTR coef_block,
                 JSAMPARRAY output_buf, JDIMENSION output_col);
#endif /* HAVE_SSE2_INTRINSICS */

GLOBAL(void)
jpeg_idct_ifast(j_decompress_ptr cinfo, jpeg_component_info * compptr,
		 JCOEFPTR coef_block,
		 JSAMPARRAY output_buf, JDIMENSION output_col);


#ifdef HAVE_MMX_INTEL_MNEMONICS
GLOBAL(void)
jpeg_idct_ifast (j_decompress_ptr cinfo, jpeg_component_info * compptr,
		 JCOEFPTR coef_block,
		 JSAMPARRAY output_buf, JDIMENSION output_col)
{
if (MMXAvailable)
	jpeg_idct_ifast_mmx(cinfo, compptr, coef_block, output_buf, output_col);
else
	jpeg_idct_ifast_orig(cinfo, compptr, coef_block, output_buf, output_col);
}
#else

/*
 * Perform dequantization and inverse DCT on one block of coefficients.
 */

GLOBAL (void)
jpeg_idct_ifast (j_decompress_ptr cinfo, jpeg_component_info * compptr,
		 JCOEFPTR coef_block,
		 JSAMPARRAY output_buf, JDIMENSION output_col)
{
  DCTELEM tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
  DCTELEM tmp10, tmp11, tmp12, tmp13;
  DCTELEM z5, z10, z11, z12, z13;
  JCOEFPTR inptr;
  IFAST_MULT_TYPE * quantptr;
  int * wsptr;
  JSAMPROW outptr;
  JSAMPLE *range_limit = IDCT_range_limit(cinfo);
  int ctr;
  int workspace[DCTSIZE2];	/* buffers data between passes */
  SHIFT_TEMPS			/* for DESCALE */
  ISHIFT_TEMPS			/* for IDESCALE */

  /* Pass 1: process columns from input, store into work array. */

  inptr = coef_block;
  quantptr = (IFAST_MULT_TYPE *) compptr->dct_table;
  wsptr = workspace;
  for (ctr = DCTSIZE; ctr > 0; ctr--) {
    /* Due to quantization, we will usually find that many of the input
     * coefficients are zero, especially the AC terms.  We can exploit this
     * by short-circuiting the IDCT calculation for any column in which all
     * the AC terms are zero.  In that case each output is equal to the
     * DC coefficient (with scale factor as needed).
     * With typical images and quantization tables, half or more of the
     * column DCT calculations can be simplified this way.
     */
    
    if (inptr[DCTSIZE*1] == 0 && inptr[DCTSIZE*2] == 0 &&
	inptr[DCTSIZE*3] == 0 && inptr[DCTSIZE*4] == 0 &&
	inptr[DCTSIZE*5] == 0 && inptr[DCTSIZE*6] == 0 &&
	inptr[DCTSIZE*7] == 0) {
      /* AC terms all zero */
      int dcval = (int) DEQUANTIZE(inptr[DCTSIZE*0], quantptr[DCTSIZE*0]);

      wsptr[DCTSIZE*0] = dcval;
      wsptr[DCTSIZE*1] = dcval;
      wsptr[DCTSIZE*2] = dcval;
      wsptr[DCTSIZE*3] = dcval;
      wsptr[DCTSIZE*4] = dcval;
      wsptr[DCTSIZE*5] = dcval;
      wsptr[DCTSIZE*6] = dcval;
      wsptr[DCTSIZE*7] = dcval;
      
      inptr++;			/* advance pointers to next column */
      quantptr++;
      wsptr++;
      continue;
    }
    
    /* Even part */

    tmp0 = DEQUANTIZE(inptr[DCTSIZE*0], quantptr[DCTSIZE*0]);
    tmp1 = DEQUANTIZE(inptr[DCTSIZE*2], quantptr[DCTSIZE*2]);
    tmp2 = DEQUANTIZE(inptr[DCTSIZE*4], quantptr[DCTSIZE*4]);
    tmp3 = DEQUANTIZE(inptr[DCTSIZE*6], quantptr[DCTSIZE*6]);

    tmp10 = tmp0 + tmp2;	/* phase 3 */
    tmp11 = tmp0 - tmp2;

    tmp13 = tmp1 + tmp3;	/* phases 5-3 */
    tmp12 = MULTIPLY(tmp1 - tmp3, FIX_1_414213562) - tmp13; /* 2*c4 */

    tmp0 = tmp10 + tmp13;	/* phase 2 */
    tmp3 = tmp10 - tmp13;
    tmp1 = tmp11 + tmp12;
    tmp2 = tmp11 - tmp12;
    
    /* Odd part */

    tmp4 = DEQUANTIZE(inptr[DCTSIZE*1], quantptr[DCTSIZE*1]);
    tmp5 = DEQUANTIZE(inptr[DCTSIZE*3], quantptr[DCTSIZE*3]);
    tmp6 = DEQUANTIZE(inptr[DCTSIZE*5], quantptr[DCTSIZE*5]);
    tmp7 = DEQUANTIZE(inptr[DCTSIZE*7], quantptr[DCTSIZE*7]);

    z13 = tmp6 + tmp5;		/* phase 6 */
    z10 = tmp6 - tmp5;
    z11 = tmp4 + tmp7;
    z12 = tmp4 - tmp7;

    tmp7 = z11 + z13;		/* phase 5 */
    tmp11 = MULTIPLY(z11 - z13, FIX_1_414213562); /* 2*c4 */

    z5 = MULTIPLY(z10 + z12, FIX_1_847759065); /* 2*c2 */
    tmp10 = MULTIPLY(z12, FIX_1_082392200) - z5; /* 2*(c2-c6) */
    tmp12 = MULTIPLY(z10, - FIX_2_613125930) + z5; /* -2*(c2+c6) */

    tmp6 = tmp12 - tmp7;	/* phase 2 */
    tmp5 = tmp11 - tmp6;
    tmp4 = tmp10 + tmp5;

    wsptr[DCTSIZE*0] = (int) (tmp0 + tmp7);
    wsptr[DCTSIZE*7] = (int) (tmp0 - tmp7);
    wsptr[DCTSIZE*1] = (int) (tmp1 + tmp6);
    wsptr[DCTSIZE*6] = (int) (tmp1 - tmp6);
    wsptr[DCTSIZE*2] = (int) (tmp2 + tmp5);
    wsptr[DCTSIZE*5] = (int) (tmp2 - tmp5);
    wsptr[DCTSIZE*4] = (int) (tmp3 + tmp4);
    wsptr[DCTSIZE*3] = (int) (tmp3 - tmp4);

    inptr++;			/* advance pointers to next column */
    quantptr++;
    wsptr++;
  }
  
  /* Pass 2: process rows from work array, store into output array. */
  /* Note that we must descale the results by a factor of 8 == 2**3, */
  /* and also undo the PASS1_BITS scaling. */

  wsptr = workspace;
  for (ctr = 0; ctr < DCTSIZE; ctr++) {
    outptr = output_buf[ctr] + output_col;
    /* Rows of zeroes can be exploited in the same way as we did with columns.
     * However, the column calculation has created many nonzero AC terms, so
     * the simplification applies less often (typically 5% to 10% of the time).
     * On machines with very fast multiplication, it's possible that the
     * test takes more time than it's worth.  In that case this section
     * may be commented out.
     */
    
#ifndef NO_ZERO_ROW_TEST
    if (wsptr[1] == 0 && wsptr[2] == 0 && wsptr[3] == 0 && wsptr[4] == 0 &&
	wsptr[5] == 0 && wsptr[6] == 0 && wsptr[7] == 0) {
      /* AC terms all zero */
      JSAMPLE dcval = range_limit[IDESCALE(wsptr[0], PASS1_BITS+3)
				  & RANGE_MASK];
      
      outptr[0] = dcval;
      outptr[1] = dcval;
      outptr[2] = dcval;
      outptr[3] = dcval;
      outptr[4] = dcval;
      outptr[5] = dcval;
      outptr[6] = dcval;
      outptr[7] = dcval;

      wsptr += DCTSIZE;		/* advance pointer to next row */
      continue;
    }
#endif
    
    /* Even part */

    tmp10 = ((DCTELEM) wsptr[0] + (DCTELEM) wsptr[4]);
    tmp11 = ((DCTELEM) wsptr[0] - (DCTELEM) wsptr[4]);

    tmp13 = ((DCTELEM) wsptr[2] + (DCTELEM) wsptr[6]);
    tmp12 = MULTIPLY((DCTELEM) wsptr[2] - (DCTELEM) wsptr[6], FIX_1_414213562)
	    - tmp13;

    tmp0 = tmp10 + tmp13;
    tmp3 = tmp10 - tmp13;
    tmp1 = tmp11 + tmp12;
    tmp2 = tmp11 - tmp12;

    /* Odd part */

    z13 = (DCTELEM) wsptr[5] + (DCTELEM) wsptr[3];
    z10 = (DCTELEM) wsptr[5] - (DCTELEM) wsptr[3];
    z11 = (DCTELEM) wsptr[1] + (DCTELEM) wsptr[7];
    z12 = (DCTELEM) wsptr[1] - (DCTELEM) wsptr[7];

    tmp7 = z11 + z13;		/* phase 5 */
    tmp11 = MULTIPLY(z11 - z13, FIX_1_414213562); /* 2*c4 */

    z5 = MULTIPLY(z10 + z12, FIX_1_847759065); /* 2*c2 */
    tmp10 = MULTIPLY(z12, FIX_1_082392200) - z5; /* 2*(c2-c6) */
    tmp12 = MULTIPLY(z10, - FIX_2_613125930) + z5; /* -2*(c2+c6) */

    tmp6 = tmp12 - tmp7;	/* phase 2 */
    tmp5 = tmp11 - tmp6;
    tmp4 = tmp10 + tmp5;

    /* Final output stage: scale down by a factor of 8 and range-limit */

    outptr[0] = range_limit[IDESCALE(tmp0 + tmp7, PASS1_BITS+3)
			    & RANGE_MASK];
    outptr[7] = range_limit[IDESCALE(tmp0 - tmp7, PASS1_BITS+3)
			    & RANGE_MASK];
    outptr[1] = range_limit[IDESCALE(tmp1 + tmp6, PASS1_BITS+3)
			    & RANGE_MASK];
    outptr[6] = range_limit[IDESCALE(tmp1 - tmp6, PASS1_BITS+3)
			    & RANGE_MASK];
    outptr[2] = range_limit[IDESCALE(tmp2 + tmp5, PASS1_BITS+3)
			    & RANGE_MASK];
    outptr[5] = range_limit[IDESCALE(tmp2 - tmp5, PASS1_BITS+3)
			    & RANGE_MASK];
    outptr[4] = range_limit[IDESCALE(tmp3 + tmp4, PASS1_BITS+3)
			    & RANGE_MASK];
    outptr[3] = range_limit[IDESCALE(tmp3 - tmp4, PASS1_BITS+3)
			    & RANGE_MASK];

    wsptr += DCTSIZE;		/* advance pointer to next row */
  }
}

#endif

#ifdef HAVE_MMX_INTEL_MNEMONICS


_inline GLOBAL(void)
jpeg_idct_ifast_orig (j_decompress_ptr cinfo, jpeg_component_info * compptr,
		 JCOEFPTR coef_block,
		 JSAMPARRAY output_buf, JDIMENSION output_col)
{
  DCTELEM tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
  DCTELEM tmp10, tmp11, tmp12, tmp13;
  DCTELEM z5, z10, z11, z12, z13;
  JCOEFPTR inptr;
  IFAST_MULT_TYPE * quantptr;
  int * wsptr;
  JSAMPROW outptr;
  JSAMPLE *range_limit = IDCT_range_limit(cinfo);
  int ctr;
  int workspace[DCTSIZE2];	/* buffers data between passes */
  SHIFT_TEMPS			/* for DESCALE */
  ISHIFT_TEMPS			/* for IDESCALE */

  /* Pass 1: process columns from input, store into work array. */

  inptr = coef_block;
  quantptr = (IFAST_MULT_TYPE *) compptr->dct_table;
  wsptr = workspace;
  for (ctr = DCTSIZE; ctr > 0; ctr--) {
    /* Due to quantization, we will usually find that many of the input
     * coefficients are zero, especially the AC terms.  We can exploit this
     * by short-circuiting the IDCT calculation for any column in which all
     * the AC terms are zero.  In that case each output is equal to the
     * DC coefficient (with scale factor as needed).
     * With typical images and quantization tables, half or more of the
     * column DCT calculations can be simplified this way.
     */
    
    if ((inptr[DCTSIZE*1] | inptr[DCTSIZE*2] | inptr[DCTSIZE*3] |
	 inptr[DCTSIZE*4] | inptr[DCTSIZE*5] | inptr[DCTSIZE*6] |
	 inptr[DCTSIZE*7]) == 0) {
      /* AC terms all zero */
      int dcval = (int) DEQUANTIZE(inptr[DCTSIZE*0], quantptr[DCTSIZE*0]);

      wsptr[DCTSIZE*0] = dcval;
      wsptr[DCTSIZE*1] = dcval;
      wsptr[DCTSIZE*2] = dcval;
      wsptr[DCTSIZE*3] = dcval;
      wsptr[DCTSIZE*4] = dcval;
      wsptr[DCTSIZE*5] = dcval;
      wsptr[DCTSIZE*6] = dcval;
      wsptr[DCTSIZE*7] = dcval;
      
      inptr++;			/* advance pointers to next column */
      quantptr++;
      wsptr++;
      continue;
    }
    
    /* Even part */

    tmp0 = DEQUANTIZE(inptr[DCTSIZE*0], quantptr[DCTSIZE*0]);
    tmp1 = DEQUANTIZE(inptr[DCTSIZE*2], quantptr[DCTSIZE*2]);
    tmp2 = DEQUANTIZE(inptr[DCTSIZE*4], quantptr[DCTSIZE*4]);
    tmp3 = DEQUANTIZE(inptr[DCTSIZE*6], quantptr[DCTSIZE*6]);

    tmp10 = tmp0 + tmp2;	/* phase 3 */
    tmp11 = tmp0 - tmp2;

    tmp13 = tmp1 + tmp3;	/* phases 5-3 */
    tmp12 = MULTIPLY(tmp1 - tmp3, FIX_1_414213562) - tmp13; /* 2*c4 */

    tmp0 = tmp10 + tmp13;	/* phase 2 */
    tmp3 = tmp10 - tmp13;
    tmp1 = tmp11 + tmp12;
    tmp2 = tmp11 - tmp12;
    
    /* Odd part */

    tmp4 = DEQUANTIZE(inptr[DCTSIZE*1], quantptr[DCTSIZE*1]);
    tmp5 = DEQUANTIZE(inptr[DCTSIZE*3], quantptr[DCTSIZE*3]);
    tmp6 = DEQUANTIZE(inptr[DCTSIZE*5], quantptr[DCTSIZE*5]);
    tmp7 = DEQUANTIZE(inptr[DCTSIZE*7], quantptr[DCTSIZE*7]);

    z13 = tmp6 + tmp5;		/* phase 6 */
    z10 = tmp6 - tmp5;
    z11 = tmp4 + tmp7;
    z12 = tmp4 - tmp7;

    tmp7 = z11 + z13;		/* phase 5 */
    tmp11 = MULTIPLY(z11 - z13, FIX_1_414213562); /* 2*c4 */

    z5 = MULTIPLY(z10 + z12, FIX_1_847759065); /* 2*c2 */
    tmp10 = MULTIPLY(z12, FIX_1_082392200) - z5; /* 2*(c2-c6) */
    tmp12 = MULTIPLY(z10, - FIX_2_613125930) + z5; /* -2*(c2+c6) */

    tmp6 = tmp12 - tmp7;	/* phase 2 */
    tmp5 = tmp11 - tmp6;
    tmp4 = tmp10 + tmp5;

    wsptr[DCTSIZE*0] = (int) (tmp0 + tmp7);
    wsptr[DCTSIZE*7] = (int) (tmp0 - tmp7);
    wsptr[DCTSIZE*1] = (int) (tmp1 + tmp6);
    wsptr[DCTSIZE*6] = (int) (tmp1 - tmp6);
    wsptr[DCTSIZE*2] = (int) (tmp2 + tmp5);
    wsptr[DCTSIZE*5] = (int) (tmp2 - tmp5);
    wsptr[DCTSIZE*4] = (int) (tmp3 + tmp4);
    wsptr[DCTSIZE*3] = (int) (tmp3 - tmp4);

    inptr++;			/* advance pointers to next column */
    quantptr++;
    wsptr++;
  }
  
  /* Pass 2: process rows from work array, store into output array. */
  /* Note that we must descale the results by a factor of 8 == 2**3, */
  /* and also undo the PASS1_BITS scaling. */

  wsptr = workspace;
  for (ctr = 0; ctr < DCTSIZE; ctr++) {
    outptr = output_buf[ctr] + output_col;
    /* Rows of zeroes can be exploited in the same way as we did with columns.
     * However, the column calculation has created many nonzero AC terms, so
     * the simplification applies less often (typically 5% to 10% of the time).
     * On machines with very fast multiplication, it's possible that the
     * test takes more time than it's worth.  In that case this section
     * may be commented out.
     */
    
#ifndef NO_ZERO_ROW_TEST
    if ((wsptr[1] | wsptr[2] | wsptr[3] | wsptr[4] | wsptr[5] | wsptr[6] |
	 wsptr[7]) == 0) {
      /* AC terms all zero */
      JSAMPLE dcval = range_limit[IDESCALE(wsptr[0], PASS1_BITS+3)
				  & RANGE_MASK];
      
      outptr[0] = dcval;
      outptr[1] = dcval;
      outptr[2] = dcval;
      outptr[3] = dcval;
      outptr[4] = dcval;
      outptr[5] = dcval;
      outptr[6] = dcval;
      outptr[7] = dcval;

      wsptr += DCTSIZE;		/* advance pointer to next row */
      continue;
    }
#endif
    
    /* Even part */

    tmp10 = ((DCTELEM) wsptr[0] + (DCTELEM) wsptr[4]);
    tmp11 = ((DCTELEM) wsptr[0] - (DCTELEM) wsptr[4]);

    tmp13 = ((DCTELEM) wsptr[2] + (DCTELEM) wsptr[6]);
    tmp12 = MULTIPLY((DCTELEM) wsptr[2] - (DCTELEM) wsptr[6], FIX_1_414213562)
	    - tmp13;

    tmp0 = tmp10 + tmp13;
    tmp3 = tmp10 - tmp13;
    tmp1 = tmp11 + tmp12;
    tmp2 = tmp11 - tmp12;

    /* Odd part */

    z13 = (DCTELEM) wsptr[5] + (DCTELEM) wsptr[3];
    z10 = (DCTELEM) wsptr[5] - (DCTELEM) wsptr[3];
    z11 = (DCTELEM) wsptr[1] + (DCTELEM) wsptr[7];
    z12 = (DCTELEM) wsptr[1] - (DCTELEM) wsptr[7];

    tmp7 = z11 + z13;		/* phase 5 */
    tmp11 = MULTIPLY(z11 - z13, FIX_1_414213562); /* 2*c4 */

    z5 = MULTIPLY(z10 + z12, FIX_1_847759065); /* 2*c2 */
    tmp10 = MULTIPLY(z12, FIX_1_082392200) - z5; /* 2*(c2-c6) */
    tmp12 = MULTIPLY(z10, - FIX_2_613125930) + z5; /* -2*(c2+c6) */

    tmp6 = tmp12 - tmp7;	/* phase 2 */
    tmp5 = tmp11 - tmp6;
    tmp4 = tmp10 + tmp5;

    /* Final output stage: scale down by a factor of 8 and range-limit */

    outptr[0] = range_limit[IDESCALE(tmp0 + tmp7, PASS1_BITS+3)
			    & RANGE_MASK];
    outptr[7] = range_limit[IDESCALE(tmp0 - tmp7, PASS1_BITS+3)
			    & RANGE_MASK];
    outptr[1] = range_limit[IDESCALE(tmp1 + tmp6, PASS1_BITS+3)
			    & RANGE_MASK];
    outptr[6] = range_limit[IDESCALE(tmp1 - tmp6, PASS1_BITS+3)
			    & RANGE_MASK];
    outptr[2] = range_limit[IDESCALE(tmp2 + tmp5, PASS1_BITS+3)
			    & RANGE_MASK];
    outptr[5] = range_limit[IDESCALE(tmp2 - tmp5, PASS1_BITS+3)
			    & RANGE_MASK];
    outptr[4] = range_limit[IDESCALE(tmp3 + tmp4, PASS1_BITS+3)
			    & RANGE_MASK];
    outptr[3] = range_limit[IDESCALE(tmp3 - tmp4, PASS1_BITS+3)
			    & RANGE_MASK];

    wsptr += DCTSIZE;		/* advance pointer to next row */
  }
}


	static	  __int64 fix_141		= 0x5a825a825a825a82;
	static	  __int64 fix_184n261	= 0xcf04cf04cf04cf04;
	static	  __int64 fix_184		= 0x7641764176417641;
	static	  __int64 fix_n184		= 0x896f896f896f896f;
	static	  __int64 fix_108n184	= 0xcf04cf04cf04cf04;
	static	  __int64 const_0x0080	= 0x0080008000800080;


__inline GLOBAL(void)
jpeg_idct_ifast_mmx (j_decompress_ptr cinfo, jpeg_component_info * compptr,
		 JCOEFPTR inptr,
		 JSAMPARRAY outptr, JDIMENSION output_col)
{

  int16 workspace[DCTSIZE2 + 4];	/* buffers data between passes */
  int16 *wsptr=workspace;
  int16 *quantptr=compptr->dct_table;

  __asm{ 
    
	mov		edi, quantptr
	mov		ebx, inptr
	mov		esi, wsptr
	add		esi, 0x07		;align wsptr to qword
	and		esi, 0xfffffff8	;align wsptr to qword

	mov		eax, esi

    /* Odd part */


	movq		mm1, [ebx + 8*10]		;load inptr[DCTSIZE*5]

	pmullw		mm1, [edi + 8*10]		;tmp6 = DEQUANTIZE(inptr[DCTSIZE*5], quantptr[DCTSIZE*5]);

	movq		mm0, [ebx + 8*6]		;load inptr[DCTSIZE*3]

	pmullw		mm0, [edi + 8*6]		;tmp5 = DEQUANTIZE(inptr[DCTSIZE*3], quantptr[DCTSIZE*3]);

	movq		mm3, [ebx + 8*2]		;load inptr[DCTSIZE*1]
	movq	mm2, mm1					;copy tmp6	/* phase 6 */

	pmullw		mm3, [edi + 8*2]		;tmp4 = DEQUANTIZE(inptr[DCTSIZE*1], quantptr[DCTSIZE*1]);

	movq		mm4, [ebx + 8*14]		;load inptr[DCTSIZE*1]
	paddw	mm1, mm0					;z13 = tmp6 + tmp5;

	pmullw		mm4, [edi + 8*14]	    ;tmp7 = DEQUANTIZE(inptr[DCTSIZE*7], quantptr[DCTSIZE*7]);
	psubw	mm2, mm0					;z10 = tmp6 - tmp5   

	psllw		mm2, 2				;shift z10
	movq		mm0, mm2			;copy z10

	pmulhw		mm2, fix_184n261	;MULTIPLY( z12, FIX_1_847759065); /* 2*c2 */
	movq		mm5, mm3				;copy tmp4

	pmulhw		mm0, fix_n184		;MULTIPLY(z10, -FIX_1_847759065); /* 2*c2 */
	paddw		mm3, mm4				;z11 = tmp4 + tmp7;

	movq		mm6, mm3				;copy z11			/* phase 5 */
	psubw		mm5, mm4				;z12 = tmp4 - tmp7;

	psubw		mm6, mm1				;z11-z13
	psllw		mm5, 2				;shift z12

	movq		mm4, [ebx + 8*12]		;load inptr[DCTSIZE*6], even part
 	movq		mm7, mm5			;copy z12

	pmulhw		mm5, fix_108n184	;MULT(z12, (FIX_1_08-FIX_1_84)) //- z5; /* 2*(c2-c6) */ even part
	paddw		mm3, mm1				;tmp7 = z11 + z13;	


    /* Even part */
	pmulhw		mm7, fix_184		;MULTIPLY(z10,(FIX_1_847759065 - FIX_2_613125930)) //+ z5; /* -2*(c2+c6) */
	psllw		mm6, 2

	movq		mm1, [ebx + 8*4]		;load inptr[DCTSIZE*2]

	pmullw		mm1, [edi + 8*4]		;tmp1 = DEQUANTIZE(inptr[DCTSIZE*2], quantptr[DCTSIZE*2]);
	paddw		mm0, mm5			;tmp10

	pmullw		mm4, [edi + 8*12]		;tmp3 = DEQUANTIZE(inptr[DCTSIZE*6], quantptr[DCTSIZE*6]);
	paddw		mm2, mm7			;tmp12

	pmulhw		mm6, fix_141			;tmp11 = MULTIPLY(z11 - z13, FIX_1_414213562); /* 2*c4 */
	psubw		mm2, mm3		;tmp6 = tmp12 - tmp7

	movq		mm5, mm1				;copy tmp1
	paddw		mm1, mm4				;tmp13= tmp1 + tmp3;	/* phases 5-3 */

	psubw		mm5, mm4				;tmp1-tmp3
	psubw		mm6, mm2		;tmp5 = tmp11 - tmp6;

	movq		[esi+8*0], mm1			;save tmp13 in workspace
	psllw		mm5, 2					;shift tmp1-tmp3
    
	movq		mm7, [ebx + 8*0]		;load inptr[DCTSIZE*0]

	pmulhw		mm5, fix_141			;MULTIPLY(tmp1 - tmp3, FIX_1_414213562)
	paddw		mm0, mm6		;tmp4 = tmp10 + tmp5;

	pmullw		mm7, [edi + 8*0]		;tmp0 = DEQUANTIZE(inptr[DCTSIZE*0], quantptr[DCTSIZE*0]);

	movq		mm4, [ebx + 8*8]		;load inptr[DCTSIZE*4]
	
	pmullw		mm4, [edi + 8*8]		;tmp2 = DEQUANTIZE(inptr[DCTSIZE*4], quantptr[DCTSIZE*4]);
	psubw		mm5, mm1				;tmp12 = MULTIPLY(tmp1 - tmp3, FIX_1_414213562) - tmp13; /* 2*c4 */

	movq		[esi+8*4], mm0		;save tmp4 in workspace
	movq		mm1, mm7			;copy tmp0	/* phase 3 */

	movq		[esi+8*2], mm5		;save tmp12 in workspace
	psubw		mm1, mm4			;tmp11 = tmp0 - tmp2; 

	paddw		mm7, mm4			;tmp10 = tmp0 + tmp2;
    movq		mm5, mm1		;copy tmp11
	
	paddw		mm1, [esi+8*2]	;tmp1 = tmp11 + tmp12;
	movq		mm4, mm7		;copy tmp10		/* phase 2 */

	paddw		mm7, [esi+8*0]	;tmp0 = tmp10 + tmp13;	

	psubw		mm4, [esi+8*0]	;tmp3 = tmp10 - tmp13;
	movq		mm0, mm7		;copy tmp0

	psubw		mm5, [esi+8*2]	;tmp2 = tmp11 - tmp12;
	paddw		mm7, mm3		;wsptr[DCTSIZE*0] = (int) (tmp0 + tmp7);
	
	psubw		mm0, mm3			;wsptr[DCTSIZE*7] = (int) (tmp0 - tmp7);

	movq		[esi + 8*0], mm7	;wsptr[DCTSIZE*0]
	movq		mm3, mm1			;copy tmp1

	movq		[esi + 8*14], mm0	;wsptr[DCTSIZE*7]
	paddw		mm1, mm2			;wsptr[DCTSIZE*1] = (int) (tmp1 + tmp6);

	psubw		mm3, mm2			;wsptr[DCTSIZE*6] = (int) (tmp1 - tmp6);

	movq		[esi + 8*2], mm1	;wsptr[DCTSIZE*1]
	movq		mm1, mm4			;copy tmp3

	movq		[esi + 8*12], mm3	;wsptr[DCTSIZE*6]

	paddw		mm4, [esi+8*4]		;wsptr[DCTSIZE*4] = (int) (tmp3 + tmp4);

	psubw		mm1, [esi+8*4]		;wsptr[DCTSIZE*3] = (int) (tmp3 - tmp4);

	movq		[esi + 8*8], mm4
	movq		mm7, mm5			;copy tmp2

	paddw		mm5, mm6			;wsptr[DCTSIZE*2] = (int) (tmp2 + tmp5)

	movq		[esi+8*6], mm1		;
	psubw		mm7, mm6			;wsptr[DCTSIZE*5] = (int) (tmp2 - tmp5);

	movq		[esi + 8*4], mm5

	movq		[esi + 8*10], mm7



/*****************************************************************/
	add		edi, 8
	add		ebx, 8
	add		esi, 8

/*****************************************************************/




	movq		mm1, [ebx + 8*10]		;load inptr[DCTSIZE*5]

	pmullw		mm1, [edi + 8*10]		;tmp6 = DEQUANTIZE(inptr[DCTSIZE*5], quantptr[DCTSIZE*5]);

	movq		mm0, [ebx + 8*6]		;load inptr[DCTSIZE*3]

	pmullw		mm0, [edi + 8*6]		;tmp5 = DEQUANTIZE(inptr[DCTSIZE*3], quantptr[DCTSIZE*3]);

	movq		mm3, [ebx + 8*2]		;load inptr[DCTSIZE*1]
	movq	mm2, mm1					;copy tmp6	/* phase 6 */

	pmullw		mm3, [edi + 8*2]		;tmp4 = DEQUANTIZE(inptr[DCTSIZE*1], quantptr[DCTSIZE*1]);

	movq		mm4, [ebx + 8*14]		;load inptr[DCTSIZE*1]
	paddw	mm1, mm0					;z13 = tmp6 + tmp5;

	pmullw		mm4, [edi + 8*14]	    ;tmp7 = DEQUANTIZE(inptr[DCTSIZE*7], quantptr[DCTSIZE*7]);
	psubw	mm2, mm0					;z10 = tmp6 - tmp5   

	psllw		mm2, 2				;shift z10
	movq		mm0, mm2			;copy z10

	pmulhw		mm2, fix_184n261	;MULTIPLY( z12, FIX_1_847759065); /* 2*c2 */
	movq		mm5, mm3				;copy tmp4

	pmulhw		mm0, fix_n184		;MULTIPLY(z10, -FIX_1_847759065); /* 2*c2 */
	paddw		mm3, mm4				;z11 = tmp4 + tmp7;

	movq		mm6, mm3				;copy z11			/* phase 5 */
	psubw		mm5, mm4				;z12 = tmp4 - tmp7;

	psubw		mm6, mm1				;z11-z13
	psllw		mm5, 2				;shift z12

	movq		mm4, [ebx + 8*12]		;load inptr[DCTSIZE*6], even part
 	movq		mm7, mm5			;copy z12

	pmulhw		mm5, fix_108n184	;MULT(z12, (FIX_1_08-FIX_1_84)) //- z5; /* 2*(c2-c6) */ even part
	paddw		mm3, mm1				;tmp7 = z11 + z13;	


    /* Even part */
	pmulhw		mm7, fix_184		;MULTIPLY(z10,(FIX_1_847759065 - FIX_2_613125930)) //+ z5; /* -2*(c2+c6) */
	psllw		mm6, 2

	movq		mm1, [ebx + 8*4]		;load inptr[DCTSIZE*2]

	pmullw		mm1, [edi + 8*4]		;tmp1 = DEQUANTIZE(inptr[DCTSIZE*2], quantptr[DCTSIZE*2]);
	paddw		mm0, mm5			;tmp10

	pmullw		mm4, [edi + 8*12]		;tmp3 = DEQUANTIZE(inptr[DCTSIZE*6], quantptr[DCTSIZE*6]);
	paddw		mm2, mm7			;tmp12

	pmulhw		mm6, fix_141			;tmp11 = MULTIPLY(z11 - z13, FIX_1_414213562); /* 2*c4 */
	psubw		mm2, mm3		;tmp6 = tmp12 - tmp7

	movq		mm5, mm1				;copy tmp1
	paddw		mm1, mm4				;tmp13= tmp1 + tmp3;	/* phases 5-3 */

	psubw		mm5, mm4				;tmp1-tmp3
	psubw		mm6, mm2		;tmp5 = tmp11 - tmp6;

	movq		[esi+8*0], mm1			;save tmp13 in workspace
	psllw		mm5, 2					;shift tmp1-tmp3
    
	movq		mm7, [ebx + 8*0]		;load inptr[DCTSIZE*0]
	paddw		mm0, mm6		;tmp4 = tmp10 + tmp5;

	pmulhw		mm5, fix_141			;MULTIPLY(tmp1 - tmp3, FIX_1_414213562)

	pmullw		mm7, [edi + 8*0]		;tmp0 = DEQUANTIZE(inptr[DCTSIZE*0], quantptr[DCTSIZE*0]);

	movq		mm4, [ebx + 8*8]		;load inptr[DCTSIZE*4]
	
	pmullw		mm4, [edi + 8*8]		;tmp2 = DEQUANTIZE(inptr[DCTSIZE*4], quantptr[DCTSIZE*4]);
	psubw		mm5, mm1				;tmp12 = MULTIPLY(tmp1 - tmp3, FIX_1_414213562) - tmp13; /* 2*c4 */

	movq		[esi+8*4], mm0		;save tmp4 in workspace
	movq		mm1, mm7			;copy tmp0	/* phase 3 */

	movq		[esi+8*2], mm5		;save tmp12 in workspace
	psubw		mm1, mm4			;tmp11 = tmp0 - tmp2; 

	paddw		mm7, mm4			;tmp10 = tmp0 + tmp2;
    movq		mm5, mm1		;copy tmp11
	
	paddw		mm1, [esi+8*2]	;tmp1 = tmp11 + tmp12;
	movq		mm4, mm7		;copy tmp10		/* phase 2 */

	paddw		mm7, [esi+8*0]	;tmp0 = tmp10 + tmp13;	

	psubw		mm4, [esi+8*0]	;tmp3 = tmp10 - tmp13;
	movq		mm0, mm7		;copy tmp0

	psubw		mm5, [esi+8*2]	;tmp2 = tmp11 - tmp12;
	paddw		mm7, mm3		;wsptr[DCTSIZE*0] = (int) (tmp0 + tmp7);
	
	psubw		mm0, mm3			;wsptr[DCTSIZE*7] = (int) (tmp0 - tmp7);

	movq		[esi + 8*0], mm7	;wsptr[DCTSIZE*0]
	movq		mm3, mm1			;copy tmp1

	movq		[esi + 8*14], mm0	;wsptr[DCTSIZE*7]
	paddw		mm1, mm2			;wsptr[DCTSIZE*1] = (int) (tmp1 + tmp6);

	psubw		mm3, mm2			;wsptr[DCTSIZE*6] = (int) (tmp1 - tmp6);

	movq		[esi + 8*2], mm1	;wsptr[DCTSIZE*1]
	movq		mm1, mm4			;copy tmp3

	movq		[esi + 8*12], mm3	;wsptr[DCTSIZE*6]

	paddw		mm4, [esi+8*4]		;wsptr[DCTSIZE*4] = (int) (tmp3 + tmp4);

	psubw		mm1, [esi+8*4]		;wsptr[DCTSIZE*3] = (int) (tmp3 - tmp4);

	movq		[esi + 8*8], mm4
	movq		mm7, mm5			;copy tmp2

	paddw		mm5, mm6			;wsptr[DCTSIZE*2] = (int) (tmp2 + tmp5)

	movq		[esi+8*6], mm1		;
	psubw		mm7, mm6			;wsptr[DCTSIZE*5] = (int) (tmp2 - tmp5);

	movq		[esi + 8*4], mm5

	movq		[esi + 8*10], mm7




/*****************************************************************/

  /* Pass 2: process rows from work array, store into output array. */
  /* Note that we must descale the results by a factor of 8 == 2**3, */
  /* and also undo the PASS1_BITS scaling. */

/*****************************************************************/
    /* Even part */

	mov			esi, eax
	mov			eax, outptr

//    tmp10 = ((DCTELEM) wsptr[0] + (DCTELEM) wsptr[4]);
//    tmp13 = ((DCTELEM) wsptr[2] + (DCTELEM) wsptr[6]);
//    tmp11 = ((DCTELEM) wsptr[0] - (DCTELEM) wsptr[4]);
//    tmp14 = ((DCTELEM) wsptr[2] - (DCTELEM) wsptr[6]);
	movq		mm0, [esi+8*0]		;wsptr[0,0],[0,1],[0,2],[0,3]

	movq		mm1, [esi+8*1]		;wsptr[0,4],[0,5],[0,6],[0,7]
	movq		mm2, mm0
	
	movq		mm3, [esi+8*2]		;wsptr[1,0],[1,1],[1,2],[1,3]
	paddw		mm0, mm1			;wsptr[0,tmp10],[xxx],[0,tmp13],[xxx]

	movq		mm4, [esi+8*3]		;wsptr[1,4],[1,5],[1,6],[1,7]
	psubw		mm2, mm1			;wsptr[0,tmp11],[xxx],[0,tmp14],[xxx]

	movq		mm6, mm0
	movq		mm5, mm3
	
	paddw		mm3, mm4			;wsptr[1,tmp10],[xxx],[1,tmp13],[xxx]
	movq		mm1, mm2

	psubw		mm5, mm4			;wsptr[1,tmp11],[xxx],[1,tmp14],[xxx]
	punpcklwd	mm0, mm3			;wsptr[0,tmp10],[1,tmp10],[xxx],[xxx]

	movq		mm7, [esi+8*7]		;wsptr[3,4],[3,5],[3,6],[3,7]
	punpckhwd	mm6, mm3			;wsptr[0,tmp13],[1,tmp13],[xxx],[xxx]

	movq		mm3, [esi+8*4]		;wsptr[2,0],[2,1],[2,2],[2,3]
	punpckldq	mm0, mm6	;wsptr[0,tmp10],[1,tmp10],[0,tmp13],[1,tmp13]

	punpcklwd	mm1, mm5			;wsptr[0,tmp11],[1,tmp11],[xxx],[xxx]
	movq		mm4, mm3

	movq		mm6, [esi+8*6]		;wsptr[3,0],[3,1],[3,2],[3,3]
	punpckhwd	mm2, mm5			;wsptr[0,tmp14],[1,tmp14],[xxx],[xxx]

	movq		mm5, [esi+8*5]		;wsptr[2,4],[2,5],[2,6],[2,7]
	punpckldq	mm1, mm2	;wsptr[0,tmp11],[1,tmp11],[0,tmp14],[1,tmp14]

	
	paddw		mm3, mm5			;wsptr[2,tmp10],[xxx],[2,tmp13],[xxx]
	movq		mm2, mm6

	psubw		mm4, mm5			;wsptr[2,tmp11],[xxx],[2,tmp14],[xxx]
	paddw		mm6, mm7			;wsptr[3,tmp10],[xxx],[3,tmp13],[xxx]

	movq		mm5, mm3
	punpcklwd	mm3, mm6			;wsptr[2,tmp10],[3,tmp10],[xxx],[xxx]
	
	psubw		mm2, mm7			;wsptr[3,tmp11],[xxx],[3,tmp14],[xxx]
	punpckhwd	mm5, mm6			;wsptr[2,tmp13],[3,tmp13],[xxx],[xxx]

	movq		mm7, mm4
	punpckldq	mm3, mm5	;wsptr[2,tmp10],[3,tmp10],[2,tmp13],[3,tmp13]

	punpcklwd	mm4, mm2			;wsptr[2,tmp11],[3,tmp11],[xxx],[xxx]

	punpckhwd	mm7, mm2			;wsptr[2,tmp14],[3,tmp14],[xxx],[xxx]

	punpckldq	mm4, mm7	;wsptr[2,tmp11],[3,tmp11],[2,tmp14],[3,tmp14]
	movq		mm6, mm1

//	mm0 = 	;wsptr[0,tmp10],[1,tmp10],[0,tmp13],[1,tmp13]
//	mm1 =	;wsptr[0,tmp11],[1,tmp11],[0,tmp14],[1,tmp14]


	movq		mm2, mm0
	punpckhdq	mm6, mm4	;wsptr[0,tmp14],[1,tmp14],[2,tmp14],[3,tmp14]

	punpckldq	mm1, mm4	;wsptr[0,tmp11],[1,tmp11],[2,tmp11],[3,tmp11]
	psllw		mm6, 2

	pmulhw		mm6, fix_141
	punpckldq	mm0, mm3	;wsptr[0,tmp10],[1,tmp10],[2,tmp10],[3,tmp10]

	punpckhdq	mm2, mm3	;wsptr[0,tmp13],[1,tmp13],[2,tmp13],[3,tmp13]
	movq		mm7, mm0

//    tmp0 = tmp10 + tmp13;
//    tmp3 = tmp10 - tmp13;
	paddw		mm0, mm2	;[0,tmp0],[1,tmp0],[2,tmp0],[3,tmp0]
	psubw		mm7, mm2	;[0,tmp3],[1,tmp3],[2,tmp3],[3,tmp3]

//    tmp12 = MULTIPLY(tmp14, FIX_1_414213562) - tmp13;
	psubw		mm6, mm2	;wsptr[0,tmp12],[1,tmp12],[2,tmp12],[3,tmp12]
//    tmp1 = tmp11 + tmp12;
//    tmp2 = tmp11 - tmp12;
	movq		mm5, mm1



    /* Odd part */

//    z13 = (DCTELEM) wsptr[5] + (DCTELEM) wsptr[3];
//    z10 = (DCTELEM) wsptr[5] - (DCTELEM) wsptr[3];
//    z11 = (DCTELEM) wsptr[1] + (DCTELEM) wsptr[7];
//    z12 = (DCTELEM) wsptr[1] - (DCTELEM) wsptr[7];
	movq		mm3, [esi+8*0]		;wsptr[0,0],[0,1],[0,2],[0,3]
	paddw		mm1, mm6	;[0,tmp1],[1,tmp1],[2,tmp1],[3,tmp1]

	movq		mm4, [esi+8*1]		;wsptr[0,4],[0,5],[0,6],[0,7]
	psubw		mm5, mm6	;[0,tmp2],[1,tmp2],[2,tmp2],[3,tmp2]

	movq		mm6, mm3
	punpckldq	mm3, mm4			;wsptr[0,0],[0,1],[0,4],[0,5]

	punpckhdq	mm4, mm6			;wsptr[0,6],[0,7],[0,2],[0,3]
	movq		mm2, mm3

//Save tmp0 and tmp1 in wsptr
	movq		[esi+8*0], mm0		;save tmp0
	paddw		mm2, mm4			;wsptr[xxx],[0,z11],[xxx],[0,z13]

	
//Continue with z10 --- z13
	movq		mm6, [esi+8*2]		;wsptr[1,0],[1,1],[1,2],[1,3]
	psubw		mm3, mm4			;wsptr[xxx],[0,z12],[xxx],[0,z10]

	movq		mm0, [esi+8*3]		;wsptr[1,4],[1,5],[1,6],[1,7]
	movq		mm4, mm6

	movq		[esi+8*1], mm1		;save tmp1
	punpckldq	mm6, mm0			;wsptr[1,0],[1,1],[1,4],[1,5]

	punpckhdq	mm0, mm4			;wsptr[1,6],[1,7],[1,2],[1,3]
	movq		mm1, mm6
	
//Save tmp2 and tmp3 in wsptr
	paddw		mm6, mm0		;wsptr[xxx],[1,z11],[xxx],[1,z13]
	movq		mm4, mm2
	
//Continue with z10 --- z13
	movq		[esi+8*2], mm5		;save tmp2
	punpcklwd	mm2, mm6		;wsptr[xxx],[xxx],[0,z11],[1,z11]

	psubw		mm1, mm0		;wsptr[xxx],[1,z12],[xxx],[1,z10]
	punpckhwd	mm4, mm6		;wsptr[xxx],[xxx],[0,z13],[1,z13]

	movq		mm0, mm3
	punpcklwd	mm3, mm1		;wsptr[xxx],[xxx],[0,z12],[1,z12]

	movq		[esi+8*3], mm7		;save tmp3
	punpckhwd	mm0, mm1		;wsptr[xxx],[xxx],[0,z10],[1,z10]

	movq		mm6, [esi+8*4]		;wsptr[2,0],[2,1],[2,2],[2,3]
	punpckhdq	mm0, mm2		;wsptr[0,z10],[1,z10],[0,z11],[1,z11]

	movq		mm7, [esi+8*5]		;wsptr[2,4],[2,5],[2,6],[2,7]
	punpckhdq	mm3, mm4		;wsptr[0,z12],[1,z12],[0,z13],[1,z13]

	movq		mm1, [esi+8*6]		;wsptr[3,0],[3,1],[3,2],[3,3]
	movq		mm4, mm6

	punpckldq	mm6, mm7			;wsptr[2,0],[2,1],[2,4],[2,5]
	movq		mm5, mm1

	punpckhdq	mm7, mm4			;wsptr[2,6],[2,7],[2,2],[2,3]
	movq		mm2, mm6
	
	movq		mm4, [esi+8*7]		;wsptr[3,4],[3,5],[3,6],[3,7]
	paddw		mm6, mm7		;wsptr[xxx],[2,z11],[xxx],[2,z13]

	psubw		mm2, mm7		;wsptr[xxx],[2,z12],[xxx],[2,z10]
	punpckldq	mm1, mm4			;wsptr[3,0],[3,1],[3,4],[3,5]

	punpckhdq	mm4, mm5			;wsptr[3,6],[3,7],[3,2],[3,3]
	movq		mm7, mm1

	paddw		mm1, mm4		;wsptr[xxx],[3,z11],[xxx],[3,z13]
	psubw		mm7, mm4		;wsptr[xxx],[3,z12],[xxx],[3,z10]

	movq		mm5, mm6
	punpcklwd	mm6, mm1		;wsptr[xxx],[xxx],[2,z11],[3,z11]

	punpckhwd	mm5, mm1		;wsptr[xxx],[xxx],[2,z13],[3,z13]
	movq		mm4, mm2

	punpcklwd	mm2, mm7		;wsptr[xxx],[xxx],[2,z12],[3,z12]

	punpckhwd	mm4, mm7		;wsptr[xxx],[xxx],[2,z10],[3,z10]

	punpckhdq	mm4, mm6		;wsptr[2,z10],[3,z10],[2,z11],[3,z11]

	punpckhdq	mm2, mm5		;wsptr[2,z12],[3,z12],[2,z13],[3,z13]
	movq		mm5, mm0

	punpckldq	mm0, mm4		;wsptr[0,z10],[1,z10],[2,z10],[3,z10]

	punpckhdq	mm5, mm4		;wsptr[0,z11],[1,z11],[2,z11],[3,z11]
	movq		mm4, mm3

	punpckhdq	mm4, mm2		;wsptr[0,z13],[1,z13],[2,z13],[3,z13]
	movq		mm1, mm5

	punpckldq	mm3, mm2		;wsptr[0,z12],[1,z12],[2,z12],[3,z12]
//    tmp7 = z11 + z13;		/* phase 5 */
//    tmp8 = z11 - z13;		/* phase 5 */
	psubw		mm1, mm4		;tmp8

	paddw		mm5, mm4		;tmp7
//    tmp21 = MULTIPLY(tmp8, FIX_1_414213562); /* 2*c4 */
	psllw		mm1, 2

	psllw		mm0, 2

	pmulhw		mm1, fix_141	;tmp21
//    tmp20 = MULTIPLY(z12, (FIX_1_082392200- FIX_1_847759065))  /* 2*(c2-c6) */
//			+ MULTIPLY(z10, - FIX_1_847759065); /* 2*c2 */
	psllw		mm3, 2
	movq		mm7, mm0

	pmulhw		mm7, fix_n184
	movq		mm6, mm3

	movq		mm2, [esi+8*0]	;tmp0,final1

	pmulhw		mm6, fix_108n184
//	 tmp22 = MULTIPLY(z10,(FIX_1_847759065 - FIX_2_613125930)) /* -2*(c2+c6) */
//			+ MULTIPLY(z12, FIX_1_847759065); /* 2*c2 */
	movq		mm4, mm2		;final1
  
	pmulhw		mm0, fix_184n261
	paddw		mm2, mm5		;tmp0+tmp7,final1

	pmulhw		mm3, fix_184
	psubw		mm4, mm5		;tmp0-tmp7,final1

//    tmp6 = tmp22 - tmp7;	/* phase 2 */
	psraw		mm2, 5			;outptr[0,0],[1,0],[2,0],[3,0],final1

	paddsw		mm2, const_0x0080	;final1
	paddw		mm7, mm6			;tmp20
	psraw		mm4, 5			;outptr[0,7],[1,7],[2,7],[3,7],final1

	paddsw		mm4, const_0x0080	;final1
	paddw		mm3, mm0			;tmp22

//    tmp5 = tmp21 - tmp6;
	psubw		mm3, mm5		;tmp6

//    tmp4 = tmp20 + tmp5;
	movq		mm0, [esi+8*1]		;tmp1,final2
	psubw		mm1, mm3		;tmp5

	movq		mm6, mm0			;final2
	paddw		mm0, mm3		;tmp1+tmp6,final2

    /* Final output stage: scale down by a factor of 8 and range-limit */


//    outptr[0] = range_limit[IDESCALE(tmp0 + tmp7, PASS1_BITS+3)
//			    & RANGE_MASK];
//    outptr[7] = range_limit[IDESCALE(tmp0 - tmp7, PASS1_BITS+3)
//			    & RANGE_MASK];	final1


//    outptr[1] = range_limit[IDESCALE(tmp1 + tmp6, PASS1_BITS+3)
//			    & RANGE_MASK];
//    outptr[6] = range_limit[IDESCALE(tmp1 - tmp6, PASS1_BITS+3)
//			    & RANGE_MASK];	final2
	psubw		mm6, mm3		;tmp1-tmp6,final2
	psraw		mm0, 5			;outptr[0,1],[1,1],[2,1],[3,1]

	paddsw		mm0, const_0x0080
	psraw		mm6, 5			;outptr[0,6],[1,6],[2,6],[3,6]
	
	paddsw		mm6, const_0x0080		;need to check this value
	packuswb	mm0, mm4	;out[0,1],[1,1],[2,1],[3,1],[0,7],[1,7],[2,7],[3,7]
	
	movq		mm5, [esi+8*2]		;tmp2,final3
	packuswb	mm2, mm6	;out[0,0],[1,0],[2,0],[3,0],[0,6],[1,6],[2,6],[3,6]

//    outptr[2] = range_limit[IDESCALE(tmp2 + tmp5, PASS1_BITS+3)
//			    & RANGE_MASK];
//    outptr[5] = range_limit[IDESCALE(tmp2 - tmp5, PASS1_BITS+3)
//			    & RANGE_MASK];	final3
	paddw		mm7, mm1		;tmp4
	movq		mm3, mm5

	paddw		mm5, mm1		;tmp2+tmp5
	psubw		mm3, mm1		;tmp2-tmp5

	psraw		mm5, 5			;outptr[0,2],[1,2],[2,2],[3,2]

	paddsw		mm5, const_0x0080
	movq		mm4, [esi+8*3]		;tmp3,final4
	psraw		mm3, 5			;outptr[0,5],[1,5],[2,5],[3,5]

	paddsw		mm3, const_0x0080


//    outptr[4] = range_limit[IDESCALE(tmp3 + tmp4, PASS1_BITS+3)
//			    & RANGE_MASK];
//    outptr[3] = range_limit[IDESCALE(tmp3 - tmp4, PASS1_BITS+3)
//			    & RANGE_MASK];	final4
	movq		mm6, mm4
	paddw		mm4, mm7		;tmp3+tmp4

	psubw		mm6, mm7		;tmp3-tmp4
	psraw		mm4, 5			;outptr[0,4],[1,4],[2,4],[3,4]
	mov			ecx, [eax]

	paddsw		mm4, const_0x0080
	psraw		mm6, 5			;outptr[0,3],[1,3],[2,3],[3,3]

	paddsw		mm6, const_0x0080
	packuswb	mm5, mm4	;out[0,2],[1,2],[2,2],[3,2],[0,4],[1,4],[2,4],[3,4]

	packuswb	mm6, mm3	;out[0,3],[1,3],[2,3],[3,3],[0,5],[1,5],[2,5],[3,5]
	movq		mm4, mm2

	movq		mm7, mm5
	punpcklbw	mm2, mm0	;out[0,0],[0,1],[1,0],[1,1],[2,0],[2,1],[3,0],[3,1]

	punpckhbw	mm4, mm0	;out[0,6],[0,7],[1,6],[1,7],[2,6],[2,7],[3,6],[3,7]
	movq		mm1, mm2

	punpcklbw	mm5, mm6	;out[0,2],[0,3],[1,2],[1,3],[2,2],[2,3],[3,2],[3,3]
	add		 	eax, 4

	punpckhbw	mm7, mm6	;out[0,4],[0,5],[1,4],[1,5],[2,4],[2,5],[3,4],[3,5]

	punpcklwd	mm2, mm5	;out[0,0],[0,1],[0,2],[0,3],[1,0],[1,1],[1,2],[1,3]
	add			ecx, output_col

	movq		mm6, mm7
	punpckhwd	mm1, mm5	;out[2,0],[2,1],[2,2],[2,3],[3,0],[3,1],[3,2],[3,3]

	movq		mm0, mm2
	punpcklwd	mm6, mm4	;out[0,4],[0,5],[0,6],[0,7],[1,4],[1,5],[1,6],[1,7]

	mov			ebx, [eax]
	punpckldq	mm2, mm6	;out[0,0],[0,1],[0,2],[0,3],[0,4],[0,5],[0,6],[0,7]

	add		 	eax, 4
	movq		mm3, mm1

	add			ebx, output_col 
	punpckhwd	mm7, mm4	;out[2,4],[2,5],[2,6],[2,7],[3,4],[3,5],[3,6],[3,7]
	
	movq		[ecx], mm2
	punpckhdq	mm0, mm6	;out[1,0],[1,1],[1,2],[1,3],[1,4],[1,5],[1,6],[1,7]

	mov			ecx, [eax]
	add		 	eax, 4
	add			ecx, output_col

	movq		[ebx], mm0
	punpckldq	mm1, mm7	;out[2,0],[2,1],[2,2],[2,3],[2,4],[2,5],[2,6],[2,7]

	mov			ebx, [eax]

	add			ebx, output_col
	punpckhdq	mm3, mm7	;out[3,0],[3,1],[3,2],[3,3],[3,4],[3,5],[3,6],[3,7]
	movq		[ecx], mm1


	movq		[ebx], mm3


		
/*******************************************************************/
	

	add			esi, 64
	add			eax, 4

/*******************************************************************/

//    tmp10 = ((DCTELEM) wsptr[0] + (DCTELEM) wsptr[4]);
//    tmp13 = ((DCTELEM) wsptr[2] + (DCTELEM) wsptr[6]);
//    tmp11 = ((DCTELEM) wsptr[0] - (DCTELEM) wsptr[4]);
//    tmp14 = ((DCTELEM) wsptr[2] - (DCTELEM) wsptr[6]);
	movq		mm0, [esi+8*0]		;wsptr[0,0],[0,1],[0,2],[0,3]

	movq		mm1, [esi+8*1]		;wsptr[0,4],[0,5],[0,6],[0,7]
	movq		mm2, mm0
	
	movq		mm3, [esi+8*2]		;wsptr[1,0],[1,1],[1,2],[1,3]
	paddw		mm0, mm1			;wsptr[0,tmp10],[xxx],[0,tmp13],[xxx]

	movq		mm4, [esi+8*3]		;wsptr[1,4],[1,5],[1,6],[1,7]
	psubw		mm2, mm1			;wsptr[0,tmp11],[xxx],[0,tmp14],[xxx]

	movq		mm6, mm0
	movq		mm5, mm3
	
	paddw		mm3, mm4			;wsptr[1,tmp10],[xxx],[1,tmp13],[xxx]
	movq		mm1, mm2

	psubw		mm5, mm4			;wsptr[1,tmp11],[xxx],[1,tmp14],[xxx]
	punpcklwd	mm0, mm3			;wsptr[0,tmp10],[1,tmp10],[xxx],[xxx]

	movq		mm7, [esi+8*7]		;wsptr[3,4],[3,5],[3,6],[3,7]
	punpckhwd	mm6, mm3			;wsptr[0,tmp13],[1,tmp13],[xxx],[xxx]

	movq		mm3, [esi+8*4]		;wsptr[2,0],[2,1],[2,2],[2,3]
	punpckldq	mm0, mm6	;wsptr[0,tmp10],[1,tmp10],[0,tmp13],[1,tmp13]

	punpcklwd	mm1, mm5			;wsptr[0,tmp11],[1,tmp11],[xxx],[xxx]
	movq		mm4, mm3

	movq		mm6, [esi+8*6]		;wsptr[3,0],[3,1],[3,2],[3,3]
	punpckhwd	mm2, mm5			;wsptr[0,tmp14],[1,tmp14],[xxx],[xxx]

	movq		mm5, [esi+8*5]		;wsptr[2,4],[2,5],[2,6],[2,7]
	punpckldq	mm1, mm2	;wsptr[0,tmp11],[1,tmp11],[0,tmp14],[1,tmp14]

	
	paddw		mm3, mm5			;wsptr[2,tmp10],[xxx],[2,tmp13],[xxx]
	movq		mm2, mm6

	psubw		mm4, mm5			;wsptr[2,tmp11],[xxx],[2,tmp14],[xxx]
	paddw		mm6, mm7			;wsptr[3,tmp10],[xxx],[3,tmp13],[xxx]

	movq		mm5, mm3
	punpcklwd	mm3, mm6			;wsptr[2,tmp10],[3,tmp10],[xxx],[xxx]
	
	psubw		mm2, mm7			;wsptr[3,tmp11],[xxx],[3,tmp14],[xxx]
	punpckhwd	mm5, mm6			;wsptr[2,tmp13],[3,tmp13],[xxx],[xxx]

	movq		mm7, mm4
	punpckldq	mm3, mm5	;wsptr[2,tmp10],[3,tmp10],[2,tmp13],[3,tmp13]

	punpcklwd	mm4, mm2			;wsptr[2,tmp11],[3,tmp11],[xxx],[xxx]

	punpckhwd	mm7, mm2			;wsptr[2,tmp14],[3,tmp14],[xxx],[xxx]

	punpckldq	mm4, mm7	;wsptr[2,tmp11],[3,tmp11],[2,tmp14],[3,tmp14]
	movq		mm6, mm1

//	mm0 = 	;wsptr[0,tmp10],[1,tmp10],[0,tmp13],[1,tmp13]
//	mm1 =	;wsptr[0,tmp11],[1,tmp11],[0,tmp14],[1,tmp14]


	movq		mm2, mm0
	punpckhdq	mm6, mm4	;wsptr[0,tmp14],[1,tmp14],[2,tmp14],[3,tmp14]

	punpckldq	mm1, mm4	;wsptr[0,tmp11],[1,tmp11],[2,tmp11],[3,tmp11]
	psllw		mm6, 2

	pmulhw		mm6, fix_141
	punpckldq	mm0, mm3	;wsptr[0,tmp10],[1,tmp10],[2,tmp10],[3,tmp10]

	punpckhdq	mm2, mm3	;wsptr[0,tmp13],[1,tmp13],[2,tmp13],[3,tmp13]
	movq		mm7, mm0

//    tmp0 = tmp10 + tmp13;
//    tmp3 = tmp10 - tmp13;
	paddw		mm0, mm2	;[0,tmp0],[1,tmp0],[2,tmp0],[3,tmp0]
	psubw		mm7, mm2	;[0,tmp3],[1,tmp3],[2,tmp3],[3,tmp3]

//    tmp12 = MULTIPLY(tmp14, FIX_1_414213562) - tmp13;
	psubw		mm6, mm2	;wsptr[0,tmp12],[1,tmp12],[2,tmp12],[3,tmp12]
//    tmp1 = tmp11 + tmp12;
//    tmp2 = tmp11 - tmp12;
	movq		mm5, mm1



    /* Odd part */

//    z13 = (DCTELEM) wsptr[5] + (DCTELEM) wsptr[3];
//    z10 = (DCTELEM) wsptr[5] - (DCTELEM) wsptr[3];
//    z11 = (DCTELEM) wsptr[1] + (DCTELEM) wsptr[7];
//    z12 = (DCTELEM) wsptr[1] - (DCTELEM) wsptr[7];
	movq		mm3, [esi+8*0]		;wsptr[0,0],[0,1],[0,2],[0,3]
	paddw		mm1, mm6	;[0,tmp1],[1,tmp1],[2,tmp1],[3,tmp1]

	movq		mm4, [esi+8*1]		;wsptr[0,4],[0,5],[0,6],[0,7]
	psubw		mm5, mm6	;[0,tmp2],[1,tmp2],[2,tmp2],[3,tmp2]

	movq		mm6, mm3
	punpckldq	mm3, mm4			;wsptr[0,0],[0,1],[0,4],[0,5]

	punpckhdq	mm4, mm6			;wsptr[0,6],[0,7],[0,2],[0,3]
	movq		mm2, mm3

//Save tmp0 and tmp1 in wsptr
	movq		[esi+8*0], mm0		;save tmp0
	paddw		mm2, mm4			;wsptr[xxx],[0,z11],[xxx],[0,z13]

	
//Continue with z10 --- z13
	movq		mm6, [esi+8*2]		;wsptr[1,0],[1,1],[1,2],[1,3]
	psubw		mm3, mm4			;wsptr[xxx],[0,z12],[xxx],[0,z10]

	movq		mm0, [esi+8*3]		;wsptr[1,4],[1,5],[1,6],[1,7]
	movq		mm4, mm6

	movq		[esi+8*1], mm1		;save tmp1
	punpckldq	mm6, mm0			;wsptr[1,0],[1,1],[1,4],[1,5]

	punpckhdq	mm0, mm4			;wsptr[1,6],[1,7],[1,2],[1,3]
	movq		mm1, mm6
	
//Save tmp2 and tmp3 in wsptr
	paddw		mm6, mm0		;wsptr[xxx],[1,z11],[xxx],[1,z13]
	movq		mm4, mm2
	
//Continue with z10 --- z13
	movq		[esi+8*2], mm5		;save tmp2
	punpcklwd	mm2, mm6		;wsptr[xxx],[xxx],[0,z11],[1,z11]

	psubw		mm1, mm0		;wsptr[xxx],[1,z12],[xxx],[1,z10]
	punpckhwd	mm4, mm6		;wsptr[xxx],[xxx],[0,z13],[1,z13]

	movq		mm0, mm3
	punpcklwd	mm3, mm1		;wsptr[xxx],[xxx],[0,z12],[1,z12]

	movq		[esi+8*3], mm7		;save tmp3
	punpckhwd	mm0, mm1		;wsptr[xxx],[xxx],[0,z10],[1,z10]

	movq		mm6, [esi+8*4]		;wsptr[2,0],[2,1],[2,2],[2,3]
	punpckhdq	mm0, mm2		;wsptr[0,z10],[1,z10],[0,z11],[1,z11]

	movq		mm7, [esi+8*5]		;wsptr[2,4],[2,5],[2,6],[2,7]
	punpckhdq	mm3, mm4		;wsptr[0,z12],[1,z12],[0,z13],[1,z13]

	movq		mm1, [esi+8*6]		;wsptr[3,0],[3,1],[3,2],[3,3]
	movq		mm4, mm6

	punpckldq	mm6, mm7			;wsptr[2,0],[2,1],[2,4],[2,5]
	movq		mm5, mm1

	punpckhdq	mm7, mm4			;wsptr[2,6],[2,7],[2,2],[2,3]
	movq		mm2, mm6
	
	movq		mm4, [esi+8*7]		;wsptr[3,4],[3,5],[3,6],[3,7]
	paddw		mm6, mm7		;wsptr[xxx],[2,z11],[xxx],[2,z13]

	psubw		mm2, mm7		;wsptr[xxx],[2,z12],[xxx],[2,z10]
	punpckldq	mm1, mm4			;wsptr[3,0],[3,1],[3,4],[3,5]

	punpckhdq	mm4, mm5			;wsptr[3,6],[3,7],[3,2],[3,3]
	movq		mm7, mm1

	paddw		mm1, mm4		;wsptr[xxx],[3,z11],[xxx],[3,z13]
	psubw		mm7, mm4		;wsptr[xxx],[3,z12],[xxx],[3,z10]

	movq		mm5, mm6
	punpcklwd	mm6, mm1		;wsptr[xxx],[xxx],[2,z11],[3,z11]

	punpckhwd	mm5, mm1		;wsptr[xxx],[xxx],[2,z13],[3,z13]
	movq		mm4, mm2

	punpcklwd	mm2, mm7		;wsptr[xxx],[xxx],[2,z12],[3,z12]

	punpckhwd	mm4, mm7		;wsptr[xxx],[xxx],[2,z10],[3,z10]

	punpckhdq	mm4, mm6		;wsptr[2,z10],[3,z10],[2,z11],[3,z11]

	punpckhdq	mm2, mm5		;wsptr[2,z12],[3,z12],[2,z13],[3,z13]
	movq		mm5, mm0

	punpckldq	mm0, mm4		;wsptr[0,z10],[1,z10],[2,z10],[3,z10]

	punpckhdq	mm5, mm4		;wsptr[0,z11],[1,z11],[2,z11],[3,z11]
	movq		mm4, mm3

	punpckhdq	mm4, mm2		;wsptr[0,z13],[1,z13],[2,z13],[3,z13]
	movq		mm1, mm5

	punpckldq	mm3, mm2		;wsptr[0,z12],[1,z12],[2,z12],[3,z12]
//    tmp7 = z11 + z13;		/* phase 5 */
//    tmp8 = z11 - z13;		/* phase 5 */
	psubw		mm1, mm4		;tmp8

	paddw		mm5, mm4		;tmp7
//    tmp21 = MULTIPLY(tmp8, FIX_1_414213562); /* 2*c4 */
	psllw		mm1, 2

	psllw		mm0, 2

	pmulhw		mm1, fix_141	;tmp21
//    tmp20 = MULTIPLY(z12, (FIX_1_082392200- FIX_1_847759065))  /* 2*(c2-c6) */
//			+ MULTIPLY(z10, - FIX_1_847759065); /* 2*c2 */
	psllw		mm3, 2
	movq		mm7, mm0

	pmulhw		mm7, fix_n184
	movq		mm6, mm3

	movq		mm2, [esi+8*0]	;tmp0,final1

	pmulhw		mm6, fix_108n184
//	 tmp22 = MULTIPLY(z10,(FIX_1_847759065 - FIX_2_613125930)) /* -2*(c2+c6) */
//			+ MULTIPLY(z12, FIX_1_847759065); /* 2*c2 */
	movq		mm4, mm2		;final1
  
	pmulhw		mm0, fix_184n261
	paddw		mm2, mm5		;tmp0+tmp7,final1

	pmulhw		mm3, fix_184
	psubw		mm4, mm5		;tmp0-tmp7,final1

//    tmp6 = tmp22 - tmp7;	/* phase 2 */
	psraw		mm2, 5			;outptr[0,0],[1,0],[2,0],[3,0],final1

	paddsw		mm2, const_0x0080	;final1
	paddw		mm7, mm6			;tmp20
	psraw		mm4, 5			;outptr[0,7],[1,7],[2,7],[3,7],final1

	paddsw		mm4, const_0x0080	;final1
	paddw		mm3, mm0			;tmp22

//    tmp5 = tmp21 - tmp6;
	psubw		mm3, mm5		;tmp6

//    tmp4 = tmp20 + tmp5;
	movq		mm0, [esi+8*1]		;tmp1,final2
	psubw		mm1, mm3		;tmp5

	movq		mm6, mm0			;final2
	paddw		mm0, mm3		;tmp1+tmp6,final2

    /* Final output stage: scale down by a factor of 8 and range-limit */


//    outptr[0] = range_limit[IDESCALE(tmp0 + tmp7, PASS1_BITS+3)
//			    & RANGE_MASK];
//    outptr[7] = range_limit[IDESCALE(tmp0 - tmp7, PASS1_BITS+3)
//			    & RANGE_MASK];	final1


//    outptr[1] = range_limit[IDESCALE(tmp1 + tmp6, PASS1_BITS+3)
//			    & RANGE_MASK];
//    outptr[6] = range_limit[IDESCALE(tmp1 - tmp6, PASS1_BITS+3)
//			    & RANGE_MASK];	final2
	psubw		mm6, mm3		;tmp1-tmp6,final2
	psraw		mm0, 5			;outptr[0,1],[1,1],[2,1],[3,1]

	paddsw		mm0, const_0x0080
	psraw		mm6, 5			;outptr[0,6],[1,6],[2,6],[3,6]
	
	paddsw		mm6, const_0x0080		;need to check this value
	packuswb	mm0, mm4	;out[0,1],[1,1],[2,1],[3,1],[0,7],[1,7],[2,7],[3,7]
	
	movq		mm5, [esi+8*2]		;tmp2,final3
	packuswb	mm2, mm6	;out[0,0],[1,0],[2,0],[3,0],[0,6],[1,6],[2,6],[3,6]

//    outptr[2] = range_limit[IDESCALE(tmp2 + tmp5, PASS1_BITS+3)
//			    & RANGE_MASK];
//    outptr[5] = range_limit[IDESCALE(tmp2 - tmp5, PASS1_BITS+3)
//			    & RANGE_MASK];	final3
	paddw		mm7, mm1		;tmp4
	movq		mm3, mm5

	paddw		mm5, mm1		;tmp2+tmp5
	psubw		mm3, mm1		;tmp2-tmp5

	psraw		mm5, 5			;outptr[0,2],[1,2],[2,2],[3,2]

	paddsw		mm5, const_0x0080
	movq		mm4, [esi+8*3]		;tmp3,final4
	psraw		mm3, 5			;outptr[0,5],[1,5],[2,5],[3,5]

	paddsw		mm3, const_0x0080


//    outptr[4] = range_limit[IDESCALE(tmp3 + tmp4, PASS1_BITS+3)
//			    & RANGE_MASK];
//    outptr[3] = range_limit[IDESCALE(tmp3 - tmp4, PASS1_BITS+3)
//			    & RANGE_MASK];	final4
	movq		mm6, mm4
	paddw		mm4, mm7		;tmp3+tmp4

	psubw		mm6, mm7		;tmp3-tmp4
	psraw		mm4, 5			;outptr[0,4],[1,4],[2,4],[3,4]
	mov			ecx, [eax]

	paddsw		mm4, const_0x0080
	psraw		mm6, 5			;outptr[0,3],[1,3],[2,3],[3,3]

	paddsw		mm6, const_0x0080
	packuswb	mm5, mm4	;out[0,2],[1,2],[2,2],[3,2],[0,4],[1,4],[2,4],[3,4]

	packuswb	mm6, mm3	;out[0,3],[1,3],[2,3],[3,3],[0,5],[1,5],[2,5],[3,5]
	movq		mm4, mm2

	movq		mm7, mm5
	punpcklbw	mm2, mm0	;out[0,0],[0,1],[1,0],[1,1],[2,0],[2,1],[3,0],[3,1]

	punpckhbw	mm4, mm0	;out[0,6],[0,7],[1,6],[1,7],[2,6],[2,7],[3,6],[3,7]
	movq		mm1, mm2

	punpcklbw	mm5, mm6	;out[0,2],[0,3],[1,2],[1,3],[2,2],[2,3],[3,2],[3,3]
	add		 	eax, 4

	punpckhbw	mm7, mm6	;out[0,4],[0,5],[1,4],[1,5],[2,4],[2,5],[3,4],[3,5]

	punpcklwd	mm2, mm5	;out[0,0],[0,1],[0,2],[0,3],[1,0],[1,1],[1,2],[1,3]
	add			ecx, output_col

	movq		mm6, mm7
	punpckhwd	mm1, mm5	;out[2,0],[2,1],[2,2],[2,3],[3,0],[3,1],[3,2],[3,3]

	movq		mm0, mm2
	punpcklwd	mm6, mm4	;out[0,4],[0,5],[0,6],[0,7],[1,4],[1,5],[1,6],[1,7]

	mov			ebx, [eax]
	punpckldq	mm2, mm6	;out[0,0],[0,1],[0,2],[0,3],[0,4],[0,5],[0,6],[0,7]

	add		 	eax, 4
	movq		mm3, mm1

	add			ebx, output_col 
	punpckhwd	mm7, mm4	;out[2,4],[2,5],[2,6],[2,7],[3,4],[3,5],[3,6],[3,7]
	
	movq		[ecx], mm2
	punpckhdq	mm0, mm6	;out[1,0],[1,1],[1,2],[1,3],[1,4],[1,5],[1,6],[1,7]

	mov			ecx, [eax]
	add		 	eax, 4
	add			ecx, output_col

	movq		[ebx], mm0
	punpckldq	mm1, mm7	;out[2,0],[2,1],[2,2],[2,3],[2,4],[2,5],[2,6],[2,7]

	mov			ebx, [eax]

	add			ebx, output_col
	punpckhdq	mm3, mm7	;out[3,0],[3,1],[3,2],[3,3],[3,4],[3,5],[3,6],[3,7]
	movq		[ecx], mm1

	movq		[ebx], mm3

	emms
	}
}
#endif

#ifdef HAVE_SSE2_INTRINSICS
#include "xmmintrin.h"
#include "emmintrin.h"

/* These values are taken from the decimal shifted left 14. When multiplying */
/* by these constants, we have to shift left the other multiplier so that we */
/* we can use pmulhw which will descale us by the 16 bits.                   */

SSE2_ALIGN unsigned short SSE2_1_414213562[8] =
  {23170, 23170, 23170, 23170, 23170, 23170, 23170, 23170};
SSE2_ALIGN unsigned short SSE2_1_847759065[8] =
  {30274, 30274, 30274, 30274, 30274, 30274, 30274, 30274};
SSE2_ALIGN unsigned short SSE2_1_082392200[8] =
  {17734, 17734, 17734, 17734, 17734, 17734, 17734, 17734};
SSE2_ALIGN unsigned short SSE2_NEG_0_765366865[8] =
  {52996, 52996, 52996, 52996, 52996, 52996, 52996, 52996};

/* This following is to compensate for range_limit and is shifted five to */
/* the left as that's what we're scaled for at the time.                  */

SSE2_ALIGN unsigned short SSE2_ADD_128[8] =
  {4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096};

/*---------------------------------------------------------------------------*/

inline GLOBAL(void)
     jpeg_idct_ifast_sse2 (j_decompress_ptr cinfo, jpeg_component_info * compptr,
                           JCOEFPTR coef_block,
                           JSAMPARRAY output_buf, JDIMENSION output_col)
{
  __m128i row0, row1, row2, row3, row4, row5, row6, row7;
  __m128i tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
  __m128i tmp10, tmp11, tmp12, tmp13, z10, z11, z12, z13;
  __m128i xps0, xps1, xps2, xps3, rowa;
  __m128i * quantptr;
  __m128i * coefptr;
  JSAMPROW outptr0, outptr1, outptr2, outptr3, outptr4, outptr5, outptr6, outptr7;

  /* ====================================================================== */
  /* Load registers from the coefficient block                              */
  /* ====================================================================== */

  quantptr = (__m128i *) compptr->dct_table;
  coefptr  = (__m128i *) coef_block;

  row0 = *(coefptr+0);
  row1 = *(coefptr+1);
  row2 = *(coefptr+2);
  row3 = *(coefptr+3);
  row4 = *(coefptr+4);
  row5 = *(coefptr+5);
  row6 = *(coefptr+6);
  row7 = *(coefptr+7);
  row0 = _mm_mullo_epi16(row0, *(quantptr+0));
  row1 = _mm_mullo_epi16(row1, *(quantptr+1));
  row2 = _mm_mullo_epi16(row2, *(quantptr+2));
  row3 = _mm_mullo_epi16(row3, *(quantptr+3));
  row4 = _mm_mullo_epi16(row4, *(quantptr+4));
  row5 = _mm_mullo_epi16(row5, *(quantptr+5));
  row6 = _mm_mullo_epi16(row6, *(quantptr+6));
  row7 = _mm_mullo_epi16(row7, *(quantptr+7));

  /* ====================================================================== */
  /* Inverse Discrete Cosine Transform Column Processing                    */
  /* ====================================================================== */

  /* Even part */

  /*  tmp10 = ((DCTELEM) wsptr[0] + (DCTELEM) wsptr[4]); */
  /*  tmp11 = ((DCTELEM) wsptr[0] - (DCTELEM) wsptr[4]); */

  tmp10 = _mm_add_epi16(row0, row4);
  tmp11 = _mm_sub_epi16(row0, row4);

  /*  tmp13 = ((DCTELEM) wsptr[2] + (DCTELEM) wsptr[6]); */
  /*  tmp12 = MULTIPLY((DCTELEM) wsptr[2] - (DCTELEM) wsptr[6], FIX_1_414213562) */
  /*        - tmp13; */

  tmp13 = _mm_add_epi16(row2, row6);
  tmp12 = _mm_mulhi_epi16(_mm_slli_epi16(_mm_sub_epi16(row2, row6), 2),
                          *((__m128i *) SSE2_1_414213562));
  tmp12 = _mm_sub_epi16(tmp12, tmp13);

  /*  tmp0 = tmp10 + tmp13; */
  /*  tmp3 = tmp10 - tmp13; */
  /*  tmp1 = tmp11 + tmp12; */
  /*  tmp2 = tmp11 - tmp12; */

  tmp0 = _mm_add_epi16(tmp10, tmp13);
  tmp3 = _mm_sub_epi16(tmp10, tmp13);
  tmp1 = _mm_add_epi16(tmp11, tmp12);
  tmp2 = _mm_sub_epi16(tmp11, tmp12);

  /* Odd part */

  /*  z13 = (DCTELEM) wsptr[5] + (DCTELEM) wsptr[3]; */
  /*  z10 = (DCTELEM) wsptr[5] - (DCTELEM) wsptr[3]; */
  /*  z11 = (DCTELEM) wsptr[1] + (DCTELEM) wsptr[7]; */
  /*  z12 = (DCTELEM) wsptr[1] - (DCTELEM) wsptr[7]; */

  z13 = _mm_add_epi16(row5, row3);
  z10 = _mm_sub_epi16(row5, row3);
  z11 = _mm_add_epi16(row1, row7);
  z12 = _mm_sub_epi16(row1, row7);

  /* phase 5 */

  /*  tmp7 = z11 + z13; */
  /*  tmp11 = MULTIPLY(z11 - z13, FIX_1_414213562); */

  tmp7 = _mm_add_epi16(z11, z13);
  tmp11 = _mm_mulhi_epi16(_mm_slli_epi16(_mm_sub_epi16(z11, z13), 2),
                          *((__m128i *) SSE2_1_414213562));

  /*  z5 = MULTIPLY(z10 + z12, FIX_1_847759065); */
  /*  tmp10 = MULTIPLY(z12, FIX_1_082392200) - z5; */
  /*  tmp12 = MULTIPLY(z10, - FIX_2_613125930) + z5; */

  /* This is really a mess as the negative 2.61 doesn't work well */
  /* as we're working with 16-bit integers. So we're going to apply */
  /* a little algebra to get: */

  /*  tmp10 = z12 * (FIX_1_082392200 - FIX_1_847759065) - z10*FIX_1_847759065 */
  /*  tmp12 = z10 * (FIX_1_847759065 - FIX_2_613125930) + z12*FIX_1_847759065 */

  /* or */

  /*  tmp10 = z12 * FIX_NEG_0_765366865 - z10 * FIX_1_847759065 */
  /*  tmp12 = z10 * FIX_NEG_0_765366865 + z12 * FIX_1_847759065 */

  z10 = _mm_slli_epi16(z10, 2);
  z12 = _mm_slli_epi16(z12, 2);

  tmp10 = _mm_sub_epi16(_mm_mulhi_epi16(z12, *((__m128i *)SSE2_NEG_0_765366865)),
                        _mm_mulhi_epi16(z10, *((__m128i *)SSE2_1_847759065)));
  tmp12 = _mm_add_epi16(_mm_mulhi_epi16(z10, *((__m128i *)SSE2_NEG_0_765366865)),
                        _mm_mulhi_epi16(z12, *((__m128i *)SSE2_1_847759065)));

  /* phase 2 */

  /*  tmp6 = tmp12 - tmp7; */
  /*  tmp5 = tmp11 - tmp6; */
  /*  tmp4 = tmp10 + tmp5; */

  tmp6 = _mm_sub_epi16(tmp12, tmp7);
  tmp5 = _mm_sub_epi16(tmp11, tmp6);
  tmp4 = _mm_add_epi16(tmp10, tmp5);

  /*  outptr[0] = range_limit[IDESCALE(tmp0 + tmp7, PASS1_BITS+3)  */
  /*                        & RANGE_MASK]; */
  /*  outptr[7] = range_limit[IDESCALE(tmp0 - tmp7, PASS1_BITS+3) */
  /*                        & RANGE_MASK]; */

  row0 = _mm_add_epi16(tmp0, tmp7);
  row7 = _mm_sub_epi16(tmp0, tmp7);

  /*  outptr[1] = range_limit[IDESCALE(tmp1 + tmp6, PASS1_BITS+3) */
  /*                        & RANGE_MASK]; */
  /*  outptr[6] = range_limit[IDESCALE(tmp1 - tmp6, PASS1_BITS+3) */
  /*                        & RANGE_MASK]; */

  row1 = _mm_add_epi16(tmp1, tmp6);
  row6 = _mm_sub_epi16(tmp1, tmp6);

  /*  outptr[2] = range_limit[IDESCALE(tmp2 + tmp5, PASS1_BITS+3) */
  /*                        & RANGE_MASK]; */
  /*  outptr[5] = range_limit[IDESCALE(tmp2 - tmp5, PASS1_BITS+3) */
  /*                        & RANGE_MASK]; */

  row2 = _mm_add_epi16(tmp2, tmp5);
  row5 = _mm_sub_epi16(tmp2, tmp5);

  /*  outptr[4] = range_limit[IDESCALE(tmp3 + tmp4, PASS1_BITS+3) */
  /*                        & RANGE_MASK]; */
  /*  outptr[3] = range_limit[IDESCALE(tmp3 - tmp4, PASS1_BITS+3) */
  /*                        & RANGE_MASK]; */

  row4 = _mm_add_epi16(tmp3, tmp4);
  row3 = _mm_sub_epi16(tmp3, tmp4);

  /* ====================================================================== */
  /* Transpose Matrix (Word)                                                */
  /* ====================================================================== */

  /* Save off the low quadwords from the even rows */

  xps0 = _mm_move_epi64(row0);
  xps1 = _mm_move_epi64(row2);
  xps2 = _mm_move_epi64(row4);
  xps3 = _mm_move_epi64(row6);

  /* Interleave the high quadwords */

  row0 = _mm_unpackhi_epi16(row0, row1);    /* 04 14 05 15 06 16 07 17 */
  row2 = _mm_unpackhi_epi16(row2, row3);    /* 24 34 25 35 26 36 27 37 */
  row4 = _mm_unpackhi_epi16(row4, row5);    /* 44 54 45 55 46 56 47 57 */
  row6 = _mm_unpackhi_epi16(row6, row7);    /* 64 74 65 75 66 76 67 77 */ 

  rowa = row0;
  row0 = _mm_unpacklo_epi32(row0, row2);    /* 04 14 24 34 05 15 25 35 */
  rowa = _mm_unpackhi_epi32(rowa, row2);    /* 06 16 26 36 07 17 27 37 */

  row2 = row4;
  row4 = _mm_unpacklo_epi32(row4, row6);    /* 44 54 64 74 45 55 65 75 */
  row2 = _mm_unpackhi_epi32(row2, row6);    /* 46 56 66 76 47 57 67 77 */

  row6 = row0;
  row0 = _mm_unpacklo_epi64(row0, row4);    /* 04 14 24 34 44 54 64 74 */
  row6 = _mm_unpackhi_epi64(row6, row4);    /* 05 15 25 35 45 55 65 75 */

  row4 = rowa;
  rowa = _mm_unpacklo_epi64(rowa, row2);    /* 06 16 26 36 46 56 66 76 */
  row4 = _mm_unpackhi_epi64(row4, row2);    /* 07 17 27 37 47 57 67 77 */

  /* row0, row6, rowa, row4 now contain *4, *5, *6, *7 */

  /* Interleave the low quadwords */

  xps0 = _mm_unpacklo_epi16(xps0, row1);    /* 00 10 01 11 02 12 03 13 */
  xps1 = _mm_unpacklo_epi16(xps1, row3);    /* 20 30 21 31 22 32 23 33 */
  xps2 = _mm_unpacklo_epi16(xps2, row5);    /* 40 50 41 51 42 52 43 43 */
  xps3 = _mm_unpacklo_epi16(xps3, row7);    /* 60 70 61 71 62 72 63 73 */

  row2 = xps0;
  xps0 = _mm_unpacklo_epi32(xps0, xps1);    /* 00 10 20 30 01 11 21 31 */
  row2 = _mm_unpackhi_epi32(row2, xps1);    /* 02 12 22 32 03 13 23 33 */

  xps1 = xps2;
  xps2 = _mm_unpacklo_epi32(xps2, xps3);    /* 40 50 60 70 41 51 61 71 */
  xps1 = _mm_unpackhi_epi32(xps1, xps3);    /* 42 52 62 72 43 53 63 73 */

  xps3 = xps0;
  xps0 = _mm_unpacklo_epi64(xps0, xps2);    /* 00 10 20 30 40 50 60 70 */
  xps3 = _mm_unpackhi_epi64(xps3, xps2);    /* 01 11 21 31 41 51 61 71 */

  xps2 = row2;
  row2 = _mm_unpacklo_epi64(row2, xps1);    /* 02 12 22 32 42 52 62 72 */
  xps2 = _mm_unpackhi_epi64(xps2, xps1);    /* 03 13 23 33 43 53 63 73 */

  /* xps0, xps3, row2, xps2 now contain *0, *1, *2, *3 */

  /* Restructure back to row0 to row7. */

  row7 = row4;
  row5 = row6;
  row6 = rowa;
  row4 = row0;
  row0 = xps0;
  row1 = xps3;
  row3 = xps2;

  /* ====================================================================== */
  /* Inverse Discrete Cosine Transform Row Processing                       */
  /* ====================================================================== */

  /* Even part */

  /*  tmp10 = ((DCTELEM) wsptr[0] + (DCTELEM) wsptr[4]); */
  /*  tmp11 = ((DCTELEM) wsptr[0] - (DCTELEM) wsptr[4]); */

  tmp10 = _mm_add_epi16(row0, row4);
  tmp11 = _mm_sub_epi16(row0, row4);

  /*  tmp13 = ((DCTELEM) wsptr[2] + (DCTELEM) wsptr[6]); */
  /*  tmp12 = MULTIPLY((DCTELEM) wsptr[2] - (DCTELEM) wsptr[6], FIX_1_414213562) */
  /*        - tmp13; */

  tmp13 = _mm_add_epi16(row2, row6);
  tmp12 = _mm_mulhi_epi16(_mm_slli_epi16(_mm_sub_epi16(row2, row6), 2),
                          *((__m128i *) SSE2_1_414213562));
  tmp12 = _mm_sub_epi16(tmp12, tmp13);

  /*  tmp0 = tmp10 + tmp13; */
  /*  tmp3 = tmp10 - tmp13; */
  /*  tmp1 = tmp11 + tmp12; */
  /*  tmp2 = tmp11 - tmp12; */

  tmp0 = _mm_add_epi16(tmp10, tmp13);
  tmp3 = _mm_sub_epi16(tmp10, tmp13);
  tmp1 = _mm_add_epi16(tmp11, tmp12);
  tmp2 = _mm_sub_epi16(tmp11, tmp12);

  /* Odd part */

  /*  z13 = (DCTELEM) wsptr[5] + (DCTELEM) wsptr[3]; */
  /*  z10 = (DCTELEM) wsptr[5] - (DCTELEM) wsptr[3]; */
  /*  z11 = (DCTELEM) wsptr[1] + (DCTELEM) wsptr[7]; */
  /*  z12 = (DCTELEM) wsptr[1] - (DCTELEM) wsptr[7]; */

  z13 = _mm_add_epi16(row5, row3);
  z10 = _mm_sub_epi16(row5, row3);
  z11 = _mm_add_epi16(row1, row7);
  z12 = _mm_sub_epi16(row1, row7);

  /* phase 5 */

  /*  tmp7 = z11 + z13; */
  /*  tmp11 = MULTIPLY(z11 - z13, FIX_1_414213562); */

  tmp7 = _mm_add_epi16(z11, z13);
  tmp11 = _mm_mulhi_epi16(_mm_slli_epi16(_mm_sub_epi16(z11, z13), 2),
                          *((__m128i *) SSE2_1_414213562));

  /*  z5 = MULTIPLY(z10 + z12, FIX_1_847759065); */
  /*  tmp10 = MULTIPLY(z12, FIX_1_082392200) - z5; */
  /*  tmp12 = MULTIPLY(z10, - FIX_2_613125930) + z5; */

  /* This is really a mess as the negative 2.61 doesn't work well */
  /* as we're working with 16-bit integers. So we're going to apply */
  /* a little algebra to get: */

  /*  tmp10 = z12 * (FIX_1_082392200 - FIX_1_847759065) - z10*FIX_1_847759065 */
  /*  tmp12 = z10 * (FIX_1_847759065 - FIX_2_613125930) + z12*FIX_1_847759065 */

  /* or */

  /*  tmp10 = z12 * FIX_NEG_0_765366865 - z10 * FIX_1_847759065 */
  /*  tmp12 = z10 * FIX_NEG_0_765366865 + z12 * FIX_1_847759065 */

  z10 = _mm_slli_epi16(z10, 2);
  z12 = _mm_slli_epi16(z12, 2);

  tmp10 = _mm_sub_epi16(_mm_mulhi_epi16(z12, *((__m128i *)SSE2_NEG_0_765366865)),
                        _mm_mulhi_epi16(z10, *((__m128i *)SSE2_1_847759065)));
  tmp12 = _mm_add_epi16(_mm_mulhi_epi16(z10, *((__m128i *)SSE2_NEG_0_765366865)),
                        _mm_mulhi_epi16(z12, *((__m128i *)SSE2_1_847759065)));

  /* phase 2 */

  /*  tmp6 = tmp12 - tmp7; */
  /*  tmp5 = tmp11 - tmp6; */
  /*  tmp4 = tmp10 + tmp5; */

  tmp6 = _mm_sub_epi16(tmp12, tmp7);
  tmp5 = _mm_sub_epi16(tmp11, tmp6);
  tmp4 = _mm_add_epi16(tmp10, tmp5);

  /*  outptr[0] = range_limit[IDESCALE(tmp0 + tmp7, PASS1_BITS+3)  */
  /*                        & RANGE_MASK]; */
  /*  outptr[7] = range_limit[IDESCALE(tmp0 - tmp7, PASS1_BITS+3) */
  /*                        & RANGE_MASK]; */

  row0 = _mm_add_epi16(tmp0, tmp7);
  row7 = _mm_sub_epi16(tmp0, tmp7);

  /*  outptr[1] = range_limit[IDESCALE(tmp1 + tmp6, PASS1_BITS+3) */
  /*                        & RANGE_MASK]; */
  /*  outptr[6] = range_limit[IDESCALE(tmp1 - tmp6, PASS1_BITS+3) */
  /*                        & RANGE_MASK]; */

  row1 = _mm_add_epi16(tmp1, tmp6);
  row6 = _mm_sub_epi16(tmp1, tmp6);

  /*  outptr[2] = range_limit[IDESCALE(tmp2 + tmp5, PASS1_BITS+3) */
  /*                        & RANGE_MASK]; */
  /*  outptr[5] = range_limit[IDESCALE(tmp2 - tmp5, PASS1_BITS+3) */
  /*                        & RANGE_MASK]; */

  row2 = _mm_add_epi16(tmp2, tmp5);
  row5 = _mm_sub_epi16(tmp2, tmp5);

  /*  outptr[4] = range_limit[IDESCALE(tmp3 + tmp4, PASS1_BITS+3) */
  /*                        & RANGE_MASK]; */
  /*  outptr[3] = range_limit[IDESCALE(tmp3 - tmp4, PASS1_BITS+3) */
  /*                        & RANGE_MASK]; */

  row4 = _mm_add_epi16(tmp3, tmp4);
  row3 = _mm_sub_epi16(tmp3, tmp4);

  /* ====================================================================== */
  /* Transpose Matrix (Byte)                                                */
  /* ====================================================================== */

  /* Shift right after adding half to descale */

  row0 = _mm_add_epi16(row0, *((__m128i *) SSE2_ADD_128));
  row1 = _mm_add_epi16(row1, *((__m128i *) SSE2_ADD_128));
  row2 = _mm_add_epi16(row2, *((__m128i *) SSE2_ADD_128));
  row3 = _mm_add_epi16(row3, *((__m128i *) SSE2_ADD_128));
  row4 = _mm_add_epi16(row4, *((__m128i *) SSE2_ADD_128));
  row5 = _mm_add_epi16(row5, *((__m128i *) SSE2_ADD_128));
  row6 = _mm_add_epi16(row6, *((__m128i *) SSE2_ADD_128));
  row7 = _mm_add_epi16(row7, *((__m128i *) SSE2_ADD_128));

  row0 = _mm_srai_epi16(row0, 5);
  row1 = _mm_srai_epi16(row1, 5);
  row2 = _mm_srai_epi16(row2, 5);
  row3 = _mm_srai_epi16(row3, 5);
  row4 = _mm_srai_epi16(row4, 5);
  row5 = _mm_srai_epi16(row5, 5);
  row6 = _mm_srai_epi16(row6, 5);
  row7 = _mm_srai_epi16(row7, 5);

  /* Range Limit and convert Words to Bytes */

  row0 = _mm_packus_epi16(row0, row4);
  row1 = _mm_packus_epi16(row1, row5);
  row2 = _mm_packus_epi16(row2, row6);
  row3 = _mm_packus_epi16(row3, row7);

  /* Transpose from rows back to columns */

  row4 = _mm_unpackhi_epi8(row0, row1);   /* 40 50 41 51 42 52 43 43 ... */
  row5 = _mm_unpackhi_epi8(row2, row3);   /* 60 70 61 71 62 72 63 73 ... */
  row0 = _mm_unpacklo_epi8(row0, row1);   /* 00 10 01 11 02 12 03 13 ... */
  row2 = _mm_unpacklo_epi8(row2, row3);   /* 20 30 21 31 22 32 23 33 ... */

  row6 = _mm_unpackhi_epi16(row0, row2);  /* 04 14 24 34 05 15 25 35 ... */
  row7 = _mm_unpackhi_epi16(row4, row5);  /* 44 54 64 74 45 55 65 75 ... */
  row0 = _mm_unpacklo_epi16(row0, row2);  /* 00 10 20 30 01 11 21 31 ... */
  row4 = _mm_unpacklo_epi16(row4, row5);  /* 40 50 60 70 41 51 61 71 ... */

  row1 = _mm_unpackhi_epi32(row0, row4);  /* 02 12 22 32 42 52 62 72 ... */
  row2 = _mm_unpackhi_epi32(row6, row7);  /* 06 16 26 36 46 56 66 76 ... */
  row0 = _mm_unpacklo_epi32(row0, row4);  /* 00 10 20 30 40 50 60 70 ... */
  row6 = _mm_unpacklo_epi32(row6, row7);  /* 04 14 24 34 55 54 64 74 ... */

  /* Rearrange for readability */

  row3 = row2;
  row2 = row6;

  /* ====================================================================== */
  /* Final output                                                           */
  /* ====================================================================== */

  outptr0 = output_buf[0] + output_col;
  outptr1 = output_buf[1] + output_col;
  outptr2 = output_buf[2] + output_col;
  outptr3 = output_buf[3] + output_col;
  outptr4 = output_buf[4] + output_col;
  outptr5 = output_buf[5] + output_col;
  outptr6 = output_buf[6] + output_col;
  outptr7 = output_buf[7] + output_col;

  /* We can't use the faster FP instructions to write out the high quad */
  /* so do a rotate right and write them out as the low quad            */

  row4 = _mm_srli_si128(row0, 8);
  row5 = _mm_srli_si128(row1, 8);
  row6 = _mm_srli_si128(row2, 8);
  row7 = _mm_srli_si128(row3, 8);

  _mm_storel_epi64((__m128i *) outptr0, row0);
  _mm_storel_epi64((__m128i *) outptr2, row1);
  _mm_storel_epi64((__m128i *) outptr4, row2);
  _mm_storel_epi64((__m128i *) outptr6, row3);
  _mm_storel_epi64((__m128i *) outptr1, row4);
  _mm_storel_epi64((__m128i *) outptr3, row5);
  _mm_storel_epi64((__m128i *) outptr5, row6);
  _mm_storel_epi64((__m128i *) outptr7, row7);
}
#endif /* HAVE_SSE2_INTRINSICS */

#endif /* DCT_IFAST_SUPPORTED */
