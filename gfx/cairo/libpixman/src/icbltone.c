/*
 * Id: $
 *
 * Copyright © 1998 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "icint.h"

/*
 *  Example: srcX = 13 dstX = 8	(IC unit 32 dstBpp 8)
 *
 *	**** **** **** **** **** **** **** ****
 *			^
 *	********  ********  ********  ********
 *		  ^
 *  leftShift = 12
 *  rightShift = 20
 *
 *  Example: srcX = 0 dstX = 8 (IC unit 32 dstBpp 8)
 *
 *	**** **** **** **** **** **** **** ****
 *	^		
 *	********  ********  ********  ********
 *		  ^
 *
 *  leftShift = 24
 *  rightShift = 8
 */

#define LoadBits {\
    if (leftShift) { \
	bitsRight = *src++; \
	bits = (IcStipLeft (bitsLeft, leftShift) | \
		IcStipRight(bitsRight, rightShift)); \
	bitsLeft = bitsRight; \
    } else \
	bits = *src++; \
}
    
#ifndef ICNOPIXADDR
    
#define LaneCases1(n,a)	    case n: (void)IcLaneCase(n,a); break
#define LaneCases2(n,a)	    LaneCases1(n,a); LaneCases1(n+1,a)
#define LaneCases4(n,a)	    LaneCases2(n,a); LaneCases2(n+2,a)
#define LaneCases8(n,a)	    LaneCases4(n,a); LaneCases4(n+4,a)
#define LaneCases16(n,a)    LaneCases8(n,a); LaneCases8(n+8,a)
#define LaneCases32(n,a)    LaneCases16(n,a); LaneCases16(n+16,a)
#define LaneCases64(n,a)    LaneCases32(n,a); LaneCases32(n+32,a)
#define LaneCases128(n,a)   LaneCases64(n,a); LaneCases64(n+64,a)
#define LaneCases256(n,a)   LaneCases128(n,a); LaneCases128(n+128,a)
    
#if IC_SHIFT == 6
#define LaneCases(a)	    LaneCases256(0,a)
#endif
    
#if IC_SHIFT == 5
#define LaneCases(a)	    LaneCases16(0,a)
#endif
							   
#if IC_SHIFT == 6
static uint8_t const Ic8Lane[256] = {
0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78,
79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97,
98, 99, 100, 101, 102,103,104,105,106,107,108,109,110,111,112,113,114,115,
116, 117, 118,119,120,121,122,123,124,125,126,127,128,129,130,131,132,133,
134, 135, 136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,
152, 153, 154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,
170, 171, 172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,
188, 189, 190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,
206, 207, 208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
224, 225, 226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,
242, 243, 244,245,246,247,248,249,250,251,252,253,254,255,
};

static uint8_t const Ic16Lane[256] = {
    0x00, 0x03, 0x0c, 0x0f,
    0x30, 0x33, 0x3c, 0x3f,
    0xc0, 0xc3, 0xcc, 0xcf,
    0xf0, 0xf3, 0xfc, 0xff,
};

static uint8_t const Ic32Lane[16] = {
    0x00, 0x0f, 0xf0, 0xff,
};
#endif

#if IC_SHIFT == 5
static uint8_t const Ic8Lane[16] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};

static uint8_t const Ic16Lane[16] = {
    0, 3, 12, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
};

static uint8_t const Ic32Lane[16] = {
    0, 15,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
#endif

static const uint8_t *
IcLaneTable(int bpp)
{
    switch (bpp) {
    case 8:
	return Ic8Lane;
    case 16:
	return Ic16Lane;
    case 32:
	return Ic32Lane;
    }
    return 0;
}
#endif

void
IcBltOne (IcStip    *src,
	  IcStride  srcStride,	    /* IcStip units per scanline */
	  int	    srcX,	    /* bit position of source */
	  pixman_bits_t    *dst,
	  IcStride  dstStride,	    /* pixman_bits_t units per scanline */
	  int	    dstX,	    /* bit position of dest */
	  int	    dstBpp,	    /* bits per destination unit */

	  int	    width,	    /* width in bits of destination */
	  int	    height,	    /* height in scanlines */

	  pixman_bits_t    fgand,	    /* rrop values */
	  pixman_bits_t    fgxor,
	  pixman_bits_t    bgand,
	  pixman_bits_t    bgxor)
{
    const pixman_bits_t    *icbits;
    int		    pixelsPerDst;		/* dst pixels per pixman_bits_t */
    int		    unitsPerSrc;		/* src patterns per IcStip */
    int		    leftShift, rightShift;	/* align source with dest */
    pixman_bits_t	    startmask, endmask;		/* dest scanline masks */
    IcStip	    bits=0, bitsLeft, bitsRight;/* source bits */
    IcStip	    left;
    pixman_bits_t	    mask;
    int		    nDst;			/* dest longwords (w.o. end) */
    int		    w;
    int		    n, nmiddle;
    int		    dstS;			/* stipple-relative dst X coordinate */
    int	    copy;			/* accelerate dest-invariant */
    int	    transparent;		/* accelerate 0 nop */
    int		    srcinc;			/* source units consumed */
    int	    endNeedsLoad = 0;	/* need load for endmask */
#ifndef ICNOPIXADDR
    const uint8_t	    *IcLane;
#endif
    int		    startbyte, endbyte;

#ifdef IC_24BIT
    if (dstBpp == 24)
    {
	IcBltOne24 (src, srcStride, srcX,
		    dst, dstStride, dstX, dstBpp,
		    width, height,
		    fgand, fgxor, bgand, bgxor);
	return;
    }
#endif
    
    /*
     * Number of destination units in pixman_bits_t == number of stipple pixels
     * used each time
     */
    pixelsPerDst = IC_UNIT / dstBpp;

    /*
     * Number of source stipple patterns in IcStip 
     */
    unitsPerSrc = IC_STIP_UNIT / pixelsPerDst;
    
    copy = 0;
    transparent = 0;
    if (bgand == 0 && fgand == 0)
	copy = 1;
    else if (bgand == IC_ALLONES && bgxor == 0)
	transparent = 1;

    /*
     * Adjust source and dest to nearest pixman_bits_t boundary
     */
    src += srcX >> IC_STIP_SHIFT;
    dst += dstX >> IC_SHIFT;
    srcX &= IC_STIP_MASK;
    dstX &= IC_MASK;

    IcMaskBitsBytes(dstX, width, copy, 
		    startmask, startbyte, nmiddle, endmask, endbyte);

    /*
     * Compute effective dest alignment requirement for
     * source -- must align source to dest unit boundary
     */
    dstS = dstX / dstBpp;
    /*
     * Compute shift constants for effective alignement
     */
    if (srcX >= dstS)
    {
	leftShift = srcX - dstS;
	rightShift = IC_STIP_UNIT - leftShift;
    }
    else
    {
	rightShift = dstS - srcX;
	leftShift = IC_STIP_UNIT - rightShift;
    }
    /*
     * Get pointer to stipple mask array for this depth
     */
    icbits = 0;	/* unused */
    if (pixelsPerDst <= 8)
	icbits = IcStippleTable(pixelsPerDst);
#ifndef ICNOPIXADDR
    IcLane = 0;
    if (transparent && fgand == 0 && dstBpp >= 8)
	IcLane = IcLaneTable(dstBpp);
#endif
    
    /*
     * Compute total number of destination words written, but 
     * don't count endmask 
     */
    nDst = nmiddle;
    if (startmask)
	nDst++;
    
    dstStride -= nDst;

    /*
     * Compute total number of source words consumed
     */
    
    srcinc = (nDst + unitsPerSrc - 1) / unitsPerSrc;
    
    if (srcX > dstS)
	srcinc++;
    if (endmask)
    {
	endNeedsLoad = nDst % unitsPerSrc == 0;
	if (endNeedsLoad)
	    srcinc++;
    }

    srcStride -= srcinc;
    
    /*
     * Copy rectangle
     */
    while (height--)
    {
	w = nDst;	    /* total units across scanline */
	n = unitsPerSrc;    /* units avail in single stipple */
	if (n > w)
	    n = w;
	
	bitsLeft = 0;
	if (srcX > dstS)
	    bitsLeft = *src++;
	if (n)
	{
	    /*
	     * Load first set of stipple bits
	     */
	    LoadBits;

	    /*
	     * Consume stipple bits for startmask
	     */
	    if (startmask)
	    {
#if IC_UNIT > 32
		if (pixelsPerDst == 16)
		    mask = IcStipple16Bits(IcLeftStipBits(bits,16));
		else
#endif
		    mask = icbits[IcLeftStipBits(bits,pixelsPerDst)];
#ifndef ICNOPIXADDR		
		if (IcLane)
		{
		    IcTransparentSpan (dst, mask & startmask, fgxor, 1);
		}
		else
#endif
		{
		    if (mask || !transparent)
			IcDoLeftMaskByteStippleRRop (dst, mask,
						     fgand, fgxor, bgand, bgxor,
						     startbyte, startmask);
		}
		bits = IcStipLeft (bits, pixelsPerDst);
		dst++;
		n--;
		w--;
	    }
	    /*
	     * Consume stipple bits across scanline
	     */
	    for (;;)
	    {
		w -= n;
		if (copy)
		{
		    while (n--)
		    {
#if IC_UNIT > 32
			if (pixelsPerDst == 16)
			    mask = IcStipple16Bits(IcLeftStipBits(bits,16));
			else
#endif
			    mask = icbits[IcLeftStipBits(bits,pixelsPerDst)];
			*dst = IcOpaqueStipple (mask, fgxor, bgxor);
			dst++;
			bits = IcStipLeft(bits, pixelsPerDst);
		    }
		}
		else
		{
#ifndef ICNOPIXADDR
		    if (IcLane)
		    {
			while (bits && n)
			{
			    switch (IcLane[IcLeftStipBits(bits,pixelsPerDst)]) {
				LaneCases((uint8_t *) dst);
			    }
			    bits = IcStipLeft(bits,pixelsPerDst);
			    dst++;
			    n--;
			}
			dst += n;
		    }
		    else
#endif
		    {
			while (n--)
			{
			    left = IcLeftStipBits(bits,pixelsPerDst);
			    if (left || !transparent)
			    {
				mask = icbits[left];
				*dst = IcStippleRRop (*dst, mask,
						      fgand, fgxor, bgand, bgxor);
			    }
			    dst++;
			    bits = IcStipLeft(bits, pixelsPerDst);
			}
		    }
		}
		if (!w)
		    break;
		/*
		 * Load another set and reset number of available units
		 */
		LoadBits;
		n = unitsPerSrc;
		if (n > w)
		    n = w;
	    }
	}
	/*
	 * Consume stipple bits for endmask
	 */
	if (endmask)
	{
	    if (endNeedsLoad)
	    {
		LoadBits;
	    }
#if IC_UNIT > 32
	    if (pixelsPerDst == 16)
		mask = IcStipple16Bits(IcLeftStipBits(bits,16));
	    else
#endif
		mask = icbits[IcLeftStipBits(bits,pixelsPerDst)];
#ifndef ICNOPIXADDR
	    if (IcLane)
	    {
		IcTransparentSpan (dst, mask & endmask, fgxor, 1);
	    }
	    else
#endif
	    {
		if (mask || !transparent)
		    IcDoRightMaskByteStippleRRop (dst, mask, 
						  fgand, fgxor, bgand, bgxor,
						  endbyte, endmask);
	    }
	}
	dst += dstStride;
	src += srcStride;
    }
}

#ifdef IC_24BIT

/*
 * Crufty macros to initialize the mask array, most of this
 * is to avoid compile-time warnings about shift overflow
 */

#if BITMAP_BIT_ORDER == MSBFirst
#define Mask24Pos(x,r) ((x)*24-(r))
#else
#define Mask24Pos(x,r) ((x)*24-((r) ? 24 - (r) : 0))
#endif

#define Mask24Neg(x,r)	(Mask24Pos(x,r) < 0 ? -Mask24Pos(x,r) : 0)
#define Mask24Check(x,r)    (Mask24Pos(x,r) < 0 ? 0 : \
			     Mask24Pos(x,r) >= IC_UNIT ? 0 : Mask24Pos(x,r))

#define Mask24(x,r) (Mask24Pos(x,r) < IC_UNIT ? \
		     (Mask24Pos(x,r) < 0 ? \
		      0xffffff >> Mask24Neg (x,r) : \
		      0xffffff << Mask24Check(x,r)) : 0)

#define SelMask24(b,n,r)	((((b) >> n) & 1) * Mask24(n,r))

/*
 * Untested for MSBFirst or IC_UNIT == 32
 */

#if IC_UNIT == 64
#define C4_24(b,r) \
    (SelMask24(b,0,r) | \
     SelMask24(b,1,r) | \
     SelMask24(b,2,r) | \
     SelMask24(b,3,r))

#define IcStip24New(rot)    (2 + (rot != 0))
#define IcStip24Len	    4

static const pixman_bits_t icStipple24Bits[3][1 << IcStip24Len] = {
    /* rotate 0 */
    {
	C4_24( 0, 0), C4_24( 1, 0), C4_24( 2, 0), C4_24( 3, 0),
	C4_24( 4, 0), C4_24( 5, 0), C4_24( 6, 0), C4_24( 7, 0),
	C4_24( 8, 0), C4_24( 9, 0), C4_24(10, 0), C4_24(11, 0),
	C4_24(12, 0), C4_24(13, 0), C4_24(14, 0), C4_24(15, 0),
    },
    /* rotate 8 */
    {
	C4_24( 0, 8), C4_24( 1, 8), C4_24( 2, 8), C4_24( 3, 8),
	C4_24( 4, 8), C4_24( 5, 8), C4_24( 6, 8), C4_24( 7, 8),
	C4_24( 8, 8), C4_24( 9, 8), C4_24(10, 8), C4_24(11, 8),
	C4_24(12, 8), C4_24(13, 8), C4_24(14, 8), C4_24(15, 8),
    },
    /* rotate 16 */
    {
	C4_24( 0,16), C4_24( 1,16), C4_24( 2,16), C4_24( 3,16),
	C4_24( 4,16), C4_24( 5,16), C4_24( 6,16), C4_24( 7,16),
	C4_24( 8,16), C4_24( 9,16), C4_24(10,16), C4_24(11,16),
	C4_24(12,16), C4_24(13,16), C4_24(14,16), C4_24(15,16),
    }
};

#endif

#if IC_UNIT == 32
#define C2_24(b,r)  \
    (SelMask24(b,0,r) | \
     SelMask24(b,1,r))

#define IcStip24Len	    2
#if BITMAP_BIT_ORDER == MSBFirst
#define IcStip24New(rot)    (1 + (rot == 0))
#else
#define IcStip24New(rot)    (1 + (rot == 8))
#endif

static const pixman_bits_t icStipple24Bits[3][1 << IcStip24Len] = {
    /* rotate 0 */
    {
	C2_24( 0, 0), C2_24 ( 1, 0), C2_24 ( 2, 0), C2_24 ( 3, 0),
    },
    /* rotate 8 */
    {
	C2_24( 0, 8), C2_24 ( 1, 8), C2_24 ( 2, 8), C2_24 ( 3, 8),
    },
    /* rotate 16 */
    {
	C2_24( 0,16), C2_24 ( 1,16), C2_24 ( 2,16), C2_24 ( 3,16),
    }
};
#endif

#if BITMAP_BIT_ORDER == LSBFirst

#define IcMergeStip24Bits(left, right, new) \
	(IcStipLeft (left, new) | IcStipRight ((right), (IcStip24Len - (new))))

#define IcMergePartStip24Bits(left, right, llen, rlen) \
	(left | IcStipRight(right, llen))

#else

#define IcMergeStip24Bits(left, right, new) \
	((IcStipLeft (left, new) & ((1 << IcStip24Len) - 1)) | right)

#define IcMergePartStip24Bits(left, right, llen, rlen) \
	(IcStipLeft(left, rlen) | right)

#endif

#define IcFirstStipBits(len,stip) {\
    int	__len = (len); \
    if (len <= remain) { \
	stip = IcLeftStipBits(bits, len); \
    } else { \
	stip = IcLeftStipBits(bits, remain); \
	bits = *src++; \
	__len = (len) - remain; \
	stip = IcMergePartStip24Bits(stip, IcLeftStipBits(bits, __len), \
				     remain, __len); \
	remain = IC_STIP_UNIT; \
    } \
    bits = IcStipLeft (bits, __len); \
    remain -= __len; \
}

#define IcInitStipBits(offset,len,stip) {\
    bits = IcStipLeft (*src++,offset); \
    remain = IC_STIP_UNIT - offset; \
    IcFirstStipBits(len,stip); \
    stip = IcMergeStip24Bits (0, stip, len); \
}
    
#define IcNextStipBits(rot,stip) {\
    int	    __new = IcStip24New(rot); \
    IcStip  __right; \
    IcFirstStipBits(__new, __right); \
    stip = IcMergeStip24Bits (stip, __right, __new); \
    rot = IcNext24Rot (rot); \
}

/*
 * Use deep mask tables that incorporate rotation, pull
 * a variable number of bits out of the stipple and
 * reuse the right bits as needed for the next write
 *
 * Yes, this is probably too much code, but most 24-bpp screens
 * have no acceleration so this code is used for stipples, copyplane
 * and text
 */
void
IcBltOne24 (IcStip	*srcLine,
	    IcStride	srcStride,  /* IcStip units per scanline */
	    int		srcX,	    /* bit position of source */
	    pixman_bits_t	*dst,
	    IcStride	dstStride,  /* pixman_bits_t units per scanline */
	    int		dstX,	    /* bit position of dest */
	    int		dstBpp,	    /* bits per destination unit */

	    int		width,	    /* width in bits of destination */
	    int		height,	    /* height in scanlines */

	    pixman_bits_t	fgand,	    /* rrop values */
	    pixman_bits_t	fgxor,
	    pixman_bits_t	bgand,
	    pixman_bits_t	bgxor)
{
    IcStip	*src;
    pixman_bits_t	leftMask, rightMask, mask;
    int		nlMiddle, nl;
    IcStip	stip, bits;
    int		remain;
    int		dstS;
    int		firstlen;
    int		rot0, rot;
    int		nDst;
    
    srcLine += srcX >> IC_STIP_SHIFT;
    dst += dstX >> IC_SHIFT;
    srcX &= IC_STIP_MASK;
    dstX &= IC_MASK;
    rot0 = IcFirst24Rot (dstX);
    
    IcMaskBits (dstX, width, leftMask, nlMiddle, rightMask);
    
    dstS = (dstX + 23) / 24;
    firstlen = IcStip24Len - dstS;
    
    nDst = nlMiddle;
    if (leftMask)
	nDst++;
    dstStride -= nDst;
    
    /* opaque copy */
    if (bgand == 0 && fgand == 0)
    {
	while (height--)
	{
	    rot = rot0;
	    src = srcLine;
	    srcLine += srcStride;
	    IcInitStipBits (srcX,firstlen, stip);
	    if (leftMask)
	    {
		mask = icStipple24Bits[rot >> 3][stip];
		*dst = (*dst & ~leftMask) | (IcOpaqueStipple (mask,
							      IcRot24(fgxor, rot),
							      IcRot24(bgxor, rot))
					     & leftMask);
		dst++;
		IcNextStipBits(rot,stip);
	    }
	    nl = nlMiddle;
	    while (nl--)
	    {
		mask = icStipple24Bits[rot>>3][stip];
		*dst = IcOpaqueStipple (mask, 
					IcRot24(fgxor, rot),
					IcRot24(bgxor, rot));
		dst++;
		IcNextStipBits(rot,stip);
	    }
	    if (rightMask)
	    {
		mask = icStipple24Bits[rot >> 3][stip];
		*dst = (*dst & ~rightMask) | (IcOpaqueStipple (mask,
							       IcRot24(fgxor, rot),
							       IcRot24(bgxor, rot))
					      & rightMask);
	    }
	    dst += dstStride;
	    src += srcStride;
	}
    }
    /* transparent copy */
    else if (bgand == IC_ALLONES && bgxor == 0 && fgand == 0)
    {
	while (height--)
	{
	    rot = rot0;
	    src = srcLine;
	    srcLine += srcStride;
	    IcInitStipBits (srcX, firstlen, stip);
	    if (leftMask)
	    {
		if (stip)
		{
		    mask = icStipple24Bits[rot >> 3][stip] & leftMask;
		    *dst = (*dst & ~mask) | (IcRot24(fgxor, rot) & mask);
		}
		dst++;
		IcNextStipBits (rot, stip);
	    }
	    nl = nlMiddle;
	    while (nl--)
	    {
		if (stip)
		{
		    mask = icStipple24Bits[rot>>3][stip];
		    *dst = (*dst & ~mask) | (IcRot24(fgxor,rot) & mask);
		}
		dst++;
		IcNextStipBits (rot, stip);
	    }
	    if (rightMask)
	    {
		if (stip)
		{
		    mask = icStipple24Bits[rot >> 3][stip] & rightMask;
		    *dst = (*dst & ~mask) | (IcRot24(fgxor, rot) & mask);
		}
	    }
	    dst += dstStride;
	}
    }
    else
    {
	while (height--)
	{
	    rot = rot0;
	    src = srcLine;
	    srcLine += srcStride;
	    IcInitStipBits (srcX, firstlen, stip);
	    if (leftMask)
	    {
		mask = icStipple24Bits[rot >> 3][stip];
		*dst = IcStippleRRopMask (*dst, mask,
					  IcRot24(fgand, rot),
					  IcRot24(fgxor, rot),
					  IcRot24(bgand, rot),
					  IcRot24(bgxor, rot),
					  leftMask);
		dst++;
		IcNextStipBits(rot,stip);
	    }
	    nl = nlMiddle;
	    while (nl--)
	    {
		mask = icStipple24Bits[rot >> 3][stip];
		*dst = IcStippleRRop (*dst, mask,
				      IcRot24(fgand, rot),
				      IcRot24(fgxor, rot),
				      IcRot24(bgand, rot),
				      IcRot24(bgxor, rot));
		dst++;
		IcNextStipBits(rot,stip);
	    }
	    if (rightMask)
	    {
		mask = icStipple24Bits[rot >> 3][stip];
		*dst = IcStippleRRopMask (*dst, mask,
					  IcRot24(fgand, rot),
					  IcRot24(fgxor, rot),
					  IcRot24(bgand, rot),
					  IcRot24(bgxor, rot),
					  rightMask);
	    }
	    dst += dstStride;
	}
    }
}
#endif

