/*
 * Copyright © 2003 Carl Worth
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Carl Worth not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Carl Worth makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * CARL WORTH DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL CARL WORTH BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _ICINT_H_
#define _ICINT_H_

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include "pixman.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "slim_internal.h"

#ifndef __GNUC__
#define __inline
#endif

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

/* C89 has implementation-defined behavior for % with negative operands.
   C99 has well-defined behavior which is that / with integers rounds toward zero
       and a%b is defined so that (a/b)*b + a%b == a.

   The C99 version gives negative remainders rather than the modulus
   in [0 .. b-1] that we want. This macro avoids using % with negative
   operands to avoid both problems.

   a and b are integers. b > 0.
*/
#define MOD(a, b) ((b) == 1 ? 0 : (a) >= 0 ? (a) % (b) : (b) - (-(a) - 1) % (b) - 1)

typedef struct _IcPoint {
	int16_t    x,y ;
} IcPoint;

typedef unsigned int	Mask;


#define GXor		0x7
#define ClipByChildren  0
#define PolyEdgeSharp   0
#define PolyModePrecise 0
#define CPClipMask      (1 << 6)
#define CPLastBit       11




/* These few definitions avoid me needing to include servermd.h and misc.h from Xserver/include */
#ifndef BITMAP_SCANLINE_PAD
#define BITMAP_SCANLINE_PAD  32
#define LOG2_BITMAP_PAD		5
#define LOG2_BYTES_PER_SCANLINE_PAD	2
#endif

#define LSBFirst 0
#define MSBFirst 1

#ifdef WORDS_BIGENDIAN
#  define IMAGE_BYTE_ORDER MSBFirst
#  define BITMAP_BIT_ORDER MSBFirst
#else
#  define IMAGE_BYTE_ORDER LSBFirst
#  define BITMAP_BIT_ORDER LSBFirst
#endif


#define MAXSHORT SHRT_MAX
#define MINSHORT SHRT_MIN

/* XXX: What do we need from here?
#include "picture.h"
*/



#include "pixman.h"

/* XXX: Most of this file is straight from fb.h and I imagine we can
   drop quite a bit of it. Once the real ic code starts to come
   together I can probably figure out what is not needed here. */

#define IC_UNIT	    (1 << IC_SHIFT)
#define IC_HALFUNIT (1 << (IC_SHIFT-1))
#define IC_MASK	    (IC_UNIT - 1)
#define IC_ALLONES  ((pixman_bits_t) -1)
    
/* whether to bother to include 24bpp support */
#ifndef ICNO24BIT
#define IC_24BIT
#endif

/*
 * Unless otherwise instructed, ic includes code to advertise 24bpp
 * windows with 32bpp image format for application compatibility
 */

#ifdef IC_24BIT
#ifndef ICNO24_32
#define IC_24_32BIT
#endif
#endif

#define IC_STIP_SHIFT	LOG2_BITMAP_PAD
#define IC_STIP_UNIT	(1 << IC_STIP_SHIFT)
#define IC_STIP_MASK	(IC_STIP_UNIT - 1)
#define IC_STIP_ALLONES	((IcStip) -1)
    
#define IC_STIP_ODDSTRIDE(s)	(((s) & (IC_MASK >> IC_STIP_SHIFT)) != 0)
#define IC_STIP_ODDPTR(p)	((((long) (p)) & (IC_MASK >> 3)) != 0)
    
#define IcStipStrideToBitsStride(s) (((s) >> (IC_SHIFT - IC_STIP_SHIFT)))
#define IcBitsStrideToStipStride(s) (((s) << (IC_SHIFT - IC_STIP_SHIFT)))
    
#define IcFullMask(n)   ((n) == IC_UNIT ? IC_ALLONES : ((((pixman_bits_t) 1) << n) - 1))


typedef uint32_t	    IcStip;
typedef int		    IcStride;


#ifdef IC_DEBUG
extern void IcValidateDrawable(DrawablePtr d);
extern void IcInitializeDrawable(DrawablePtr d);
extern void IcSetBits (IcStip *bits, int stride, IcStip data);
#define IC_HEAD_BITS   (IcStip) (0xbaadf00d)
#define IC_TAIL_BITS   (IcStip) (0xbaddf0ad)
#else
#define IcValidateDrawable(d)
#define fdInitializeDrawable(d)
#endif

#if BITMAP_BIT_ORDER == LSBFirst
#define IcScrLeft(x,n)	((x) >> (n))
#define IcScrRight(x,n)	((x) << (n))
/* #define IcLeftBits(x,n)	((x) & ((((pixman_bits_t) 1) << (n)) - 1)) */
#define IcLeftStipBits(x,n) ((x) & ((((IcStip) 1) << (n)) - 1))
#define IcStipMoveLsb(x,s,n)	(IcStipRight (x,(s)-(n)))
#define IcPatternOffsetBits	0
#else
#define IcScrLeft(x,n)	((x) << (n))
#define IcScrRight(x,n)	((x) >> (n))
/* #define IcLeftBits(x,n)	((x) >> (IC_UNIT - (n))) */
#define IcLeftStipBits(x,n) ((x) >> (IC_STIP_UNIT - (n)))
#define IcStipMoveLsb(x,s,n)	(x)
#define IcPatternOffsetBits	(sizeof (pixman_bits_t) - 1)
#endif

#define IcStipLeft(x,n)	IcScrLeft(x,n)
#define IcStipRight(x,n) IcScrRight(x,n)

#define IcRotLeft(x,n)	IcScrLeft(x,n) | (n ? IcScrRight(x,IC_UNIT-n) : 0)
#define IcRotRight(x,n)	IcScrRight(x,n) | (n ? IcScrLeft(x,IC_UNIT-n) : 0)

#define IcRotStipLeft(x,n)  IcStipLeft(x,n) | (n ? IcStipRight(x,IC_STIP_UNIT-n) : 0)
#define IcRotStipRight(x,n)  IcStipRight(x,n) | (n ? IcStipLeft(x,IC_STIP_UNIT-n) : 0)

#define IcLeftMask(x)	    ( ((x) & IC_MASK) ? \
			     IcScrRight(IC_ALLONES,(x) & IC_MASK) : 0)
#define IcRightMask(x)	    ( ((IC_UNIT - (x)) & IC_MASK) ? \
			     IcScrLeft(IC_ALLONES,(IC_UNIT - (x)) & IC_MASK) : 0)

#define IcLeftStipMask(x)   ( ((x) & IC_STIP_MASK) ? \
			     IcStipRight(IC_STIP_ALLONES,(x) & IC_STIP_MASK) : 0)
#define IcRightStipMask(x)  ( ((IC_STIP_UNIT - (x)) & IC_STIP_MASK) ? \
			     IcScrLeft(IC_STIP_ALLONES,(IC_STIP_UNIT - (x)) & IC_STIP_MASK) : 0)

#define IcBitsMask(x,w)	(IcScrRight(IC_ALLONES,(x) & IC_MASK) & \
			 IcScrLeft(IC_ALLONES,(IC_UNIT - ((x) + (w))) & IC_MASK))

#define IcStipMask(x,w)	(IcStipRight(IC_STIP_ALLONES,(x) & IC_STIP_MASK) & \
			 IcStipLeft(IC_STIP_ALLONES,(IC_STIP_UNIT - ((x)+(w))) & IC_STIP_MASK))


#define IcMaskBits(x,w,l,n,r) { \
    n = (w); \
    r = IcRightMask((x)+n); \
    l = IcLeftMask(x); \
    if (l) { \
	n -= IC_UNIT - ((x) & IC_MASK); \
	if (n < 0) { \
	    n = 0; \
	    l &= r; \
	    r = 0; \
	} \
    } \
    n >>= IC_SHIFT; \
}

#ifdef ICNOPIXADDR
#define IcMaskBitsBytes(x,w,copy,l,lb,n,r,rb) IcMaskBits(x,w,l,n,r)
#define IcDoLeftMaskByteRRop(dst,lb,l,and,xor) { \
    *dst = IcDoMaskRRop(*dst,and,xor,l); \
}
#define IcDoRightMaskByteRRop(dst,rb,r,and,xor) { \
    *dst = IcDoMaskRRop(*dst,and,xor,r); \
}
#else

#define IcByteMaskInvalid   0x10

#define IcPatternOffset(o,t)  ((o) ^ (IcPatternOffsetBits & ~(sizeof (t) - 1)))

#define IcPtrOffset(p,o,t)		((t *) ((uint8_t *) (p) + (o)))
#define IcSelectPatternPart(xor,o,t)	((xor) >> (IcPatternOffset (o,t) << 3))
#define IcStorePart(dst,off,t,xor)	(*IcPtrOffset(dst,off,t) = \
					 IcSelectPart(xor,off,t))
#ifndef IcSelectPart
#define IcSelectPart(x,o,t) IcSelectPatternPart(x,o,t)
#endif

#define IcMaskBitsBytes(x,w,copy,l,lb,n,r,rb) { \
    n = (w); \
    lb = 0; \
    rb = 0; \
    r = IcRightMask((x)+n); \
    if (r) { \
	/* compute right byte length */ \
	if ((copy) && (((x) + n) & 7) == 0) { \
	    rb = (((x) + n) & IC_MASK) >> 3; \
	} else { \
	    rb = IcByteMaskInvalid; \
	} \
    } \
    l = IcLeftMask(x); \
    if (l) { \
	/* compute left byte length */ \
	if ((copy) && ((x) & 7) == 0) { \
	    lb = ((x) & IC_MASK) >> 3; \
	} else { \
	    lb = IcByteMaskInvalid; \
	} \
	/* subtract out the portion painted by leftMask */ \
	n -= IC_UNIT - ((x) & IC_MASK); \
	if (n < 0) { \
	    if (lb != IcByteMaskInvalid) { \
		if (rb == IcByteMaskInvalid) { \
		    lb = IcByteMaskInvalid; \
		} else if (rb) { \
		    lb |= (rb - lb) << (IC_SHIFT - 3); \
		    rb = 0; \
		} \
	    } \
	    n = 0; \
	    l &= r; \
	    r = 0; \
	}\
    } \
    n >>= IC_SHIFT; \
}

#if IC_SHIFT == 6
#define IcDoLeftMaskByteRRop6Cases(dst,xor) \
    case (sizeof (pixman_bits_t) - 7) | (1 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (pixman_bits_t) - 7,uint8_t,xor); \
	break; \
    case (sizeof (pixman_bits_t) - 7) | (2 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (pixman_bits_t) - 7,uint8_t,xor); \
	IcStorePart(dst,sizeof (pixman_bits_t) - 6,uint8_t,xor); \
	break; \
    case (sizeof (pixman_bits_t) - 7) | (3 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (pixman_bits_t) - 7,uint8_t,xor); \
	IcStorePart(dst,sizeof (pixman_bits_t) - 6,uint16_t,xor); \
	break; \
    case (sizeof (pixman_bits_t) - 7) | (4 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (pixman_bits_t) - 7,uint8_t,xor); \
	IcStorePart(dst,sizeof (pixman_bits_t) - 6,uint16_t,xor); \
	IcStorePart(dst,sizeof (pixman_bits_t) - 4,uint8_t,xor); \
	break; \
    case (sizeof (pixman_bits_t) - 7) | (5 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (pixman_bits_t) - 7,uint8_t,xor); \
	IcStorePart(dst,sizeof (pixman_bits_t) - 6,uint16_t,xor); \
	IcStorePart(dst,sizeof (pixman_bits_t) - 4,uint16_t,xor); \
	break; \
    case (sizeof (pixman_bits_t) - 7) | (6 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (pixman_bits_t) - 7,uint8_t,xor); \
	IcStorePart(dst,sizeof (pixman_bits_t) - 6,uint16_t,xor); \
	IcStorePart(dst,sizeof (pixman_bits_t) - 4,uint16_t,xor); \
	IcStorePart(dst,sizeof (pixman_bits_t) - 2,uint8_t,xor); \
	break; \
    case (sizeof (pixman_bits_t) - 7): \
	IcStorePart(dst,sizeof (pixman_bits_t) - 7,uint8_t,xor); \
	IcStorePart(dst,sizeof (pixman_bits_t) - 6,uint16_t,xor); \
	IcStorePart(dst,sizeof (pixman_bits_t) - 4,uint32_t,xor); \
	break; \
    case (sizeof (pixman_bits_t) - 6) | (1 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (pixman_bits_t) - 6,uint8_t,xor); \
	break; \
    case (sizeof (pixman_bits_t) - 6) | (2 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (pixman_bits_t) - 6,uint16_t,xor); \
	break; \
    case (sizeof (pixman_bits_t) - 6) | (3 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (pixman_bits_t) - 6,uint16_t,xor); \
	IcStorePart(dst,sizeof (pixman_bits_t) - 4,uint8_t,xor); \
	break; \
    case (sizeof (pixman_bits_t) - 6) | (4 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (pixman_bits_t) - 6,uint16_t,xor); \
	IcStorePart(dst,sizeof (pixman_bits_t) - 4,uint16_t,xor); \
	break; \
    case (sizeof (pixman_bits_t) - 6) | (5 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (pixman_bits_t) - 6,uint16_t,xor); \
	IcStorePart(dst,sizeof (pixman_bits_t) - 4,uint16_t,xor); \
	IcStorePart(dst,sizeof (pixman_bits_t) - 2,uint8_t,xor); \
	break; \
    case (sizeof (pixman_bits_t) - 6): \
	IcStorePart(dst,sizeof (pixman_bits_t) - 6,uint16_t,xor); \
	IcStorePart(dst,sizeof (pixman_bits_t) - 4,uint32_t,xor); \
	break; \
    case (sizeof (pixman_bits_t) - 5) | (1 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (pixman_bits_t) - 5,uint8_t,xor); \
	break; \
    case (sizeof (pixman_bits_t) - 5) | (2 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (pixman_bits_t) - 5,uint8_t,xor); \
	IcStorePart(dst,sizeof (pixman_bits_t) - 4,uint8_t,xor); \
	break; \
    case (sizeof (pixman_bits_t) - 5) | (3 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (pixman_bits_t) - 5,uint8_t,xor); \
	IcStorePart(dst,sizeof (pixman_bits_t) - 4,uint16_t,xor); \
	break; \
    case (sizeof (pixman_bits_t) - 5) | (4 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (pixman_bits_t) - 5,uint8_t,xor); \
	IcStorePart(dst,sizeof (pixman_bits_t) - 4,uint16_t,xor); \
	IcStorePart(dst,sizeof (pixman_bits_t) - 2,uint8_t,xor); \
	break; \
    case (sizeof (pixman_bits_t) - 5): \
	IcStorePart(dst,sizeof (pixman_bits_t) - 5,uint8_t,xor); \
	IcStorePart(dst,sizeof (pixman_bits_t) - 4,uint32_t,xor); \
	break; \
    case (sizeof (pixman_bits_t) - 4) | (1 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (pixman_bits_t) - 4,uint8_t,xor); \
	break; \
    case (sizeof (pixman_bits_t) - 4) | (2 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (pixman_bits_t) - 4,uint16_t,xor); \
	break; \
    case (sizeof (pixman_bits_t) - 4) | (3 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (pixman_bits_t) - 4,uint16_t,xor); \
	IcStorePart(dst,sizeof (pixman_bits_t) - 2,uint8_t,xor); \
	break; \
    case (sizeof (pixman_bits_t) - 4): \
	IcStorePart(dst,sizeof (pixman_bits_t) - 4,uint32_t,xor); \
	break;

#define IcDoRightMaskByteRRop6Cases(dst,xor) \
    case 4: \
	IcStorePart(dst,0,uint32_t,xor); \
	break; \
    case 5: \
	IcStorePart(dst,0,uint32_t,xor); \
	IcStorePart(dst,4,uint8_t,xor); \
	break; \
    case 6: \
	IcStorePart(dst,0,uint32_t,xor); \
	IcStorePart(dst,4,uint16_t,xor); \
	break; \
    case 7: \
	IcStorePart(dst,0,uint32_t,xor); \
	IcStorePart(dst,4,uint16_t,xor); \
	IcStorePart(dst,6,uint8_t,xor); \
	break;
#else
#define IcDoLeftMaskByteRRop6Cases(dst,xor)
#define IcDoRightMaskByteRRop6Cases(dst,xor)
#endif

#define IcDoLeftMaskByteRRop(dst,lb,l,and,xor) { \
    switch (lb) { \
    IcDoLeftMaskByteRRop6Cases(dst,xor) \
    case (sizeof (pixman_bits_t) - 3) | (1 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (pixman_bits_t) - 3,uint8_t,xor); \
	break; \
    case (sizeof (pixman_bits_t) - 3) | (2 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (pixman_bits_t) - 3,uint8_t,xor); \
	IcStorePart(dst,sizeof (pixman_bits_t) - 2,uint8_t,xor); \
	break; \
    case (sizeof (pixman_bits_t) - 2) | (1 << (IC_SHIFT - 3)): \
	IcStorePart(dst,sizeof (pixman_bits_t) - 2,uint8_t,xor); \
	break; \
    case sizeof (pixman_bits_t) - 3: \
	IcStorePart(dst,sizeof (pixman_bits_t) - 3,uint8_t,xor); \
    case sizeof (pixman_bits_t) - 2: \
	IcStorePart(dst,sizeof (pixman_bits_t) - 2,uint16_t,xor); \
	break; \
    case sizeof (pixman_bits_t) - 1: \
	IcStorePart(dst,sizeof (pixman_bits_t) - 1,uint8_t,xor); \
	break; \
    default: \
	*dst = IcDoMaskRRop(*dst, and, xor, l); \
	break; \
    } \
}


#define IcDoRightMaskByteRRop(dst,rb,r,and,xor) { \
    switch (rb) { \
    case 1: \
	IcStorePart(dst,0,uint8_t,xor); \
	break; \
    case 2: \
	IcStorePart(dst,0,uint16_t,xor); \
	break; \
    case 3: \
	IcStorePart(dst,0,uint16_t,xor); \
	IcStorePart(dst,2,uint8_t,xor); \
	break; \
    IcDoRightMaskByteRRop6Cases(dst,xor) \
    default: \
	*dst = IcDoMaskRRop (*dst, and, xor, r); \
    } \
}
#endif

#define IcMaskStip(x,w,l,n,r) { \
    n = (w); \
    r = IcRightStipMask((x)+n); \
    l = IcLeftStipMask(x); \
    if (l) { \
	n -= IC_STIP_UNIT - ((x) & IC_STIP_MASK); \
	if (n < 0) { \
	    n = 0; \
	    l &= r; \
	    r = 0; \
	} \
    } \
    n >>= IC_STIP_SHIFT; \
}

/*
 * These macros are used to transparently stipple
 * in copy mode; the expected usage is with 'n' constant
 * so all of the conditional parts collapse into a minimal
 * sequence of partial word writes
 *
 * 'n' is the bytemask of which bytes to store, 'a' is the address
 * of the pixman_bits_t base unit, 'o' is the offset within that unit
 *
 * The term "lane" comes from the hardware term "byte-lane" which
 */

#define IcLaneCase1(n,a,o)  ((n) == 0x01 ? \
			     (*(uint8_t *) ((a)+IcPatternOffset(o,uint8_t)) = \
			      fgxor) : 0)
#define IcLaneCase2(n,a,o)  ((n) == 0x03 ? \
			     (*(uint16_t *) ((a)+IcPatternOffset(o,uint16_t)) = \
			      fgxor) : \
			     ((void)IcLaneCase1((n)&1,a,o), \
				    IcLaneCase1((n)>>1,a,(o)+1)))
#define IcLaneCase4(n,a,o)  ((n) == 0x0f ? \
			     (*(uint32_t *) ((a)+IcPatternOffset(o,uint32_t)) = \
			      fgxor) : \
			     ((void)IcLaneCase2((n)&3,a,o), \
				    IcLaneCase2((n)>>2,a,(o)+2)))
#define IcLaneCase8(n,a,o)  ((n) == 0x0ff ? (*(pixman_bits_t *) ((a)+(o)) = fgxor) : \
			     ((void)IcLaneCase4((n)&15,a,o), \
				    IcLaneCase4((n)>>4,a,(o)+4)))

#if IC_SHIFT == 6
#define IcLaneCase(n,a)   IcLaneCase8(n,(uint8_t *) (a),0)
#endif

#if IC_SHIFT == 5
#define IcLaneCase(n,a)   IcLaneCase4(n,(uint8_t *) (a),0)
#endif

/* Rotate a filled pixel value to the specified alignement */
#define IcRot24(p,b)	    (IcScrRight(p,b) | IcScrLeft(p,24-(b)))
#define IcRot24Stip(p,b)    (IcStipRight(p,b) | IcStipLeft(p,24-(b)))

/* step a filled pixel value to the next/previous IC_UNIT alignment */
#define IcNext24Pix(p)	(IcRot24(p,(24-IC_UNIT%24)))
#define IcPrev24Pix(p)	(IcRot24(p,IC_UNIT%24))
#define IcNext24Stip(p)	(IcRot24(p,(24-IC_STIP_UNIT%24)))
#define IcPrev24Stip(p)	(IcRot24(p,IC_STIP_UNIT%24))

/* step a rotation value to the next/previous rotation value */
#if IC_UNIT == 64
#define IcNext24Rot(r)        ((r) == 16 ? 0 : (r) + 8)
#define IcPrev24Rot(r)        ((r) == 0 ? 16 : (r) - 8)

#if IMAGE_BYTE_ORDER == MSBFirst
#define IcFirst24Rot(x)		(((x) + 8) % 24)
#else
#define IcFirst24Rot(x)		((x) % 24)
#endif

#endif

#if IC_UNIT == 32
#define IcNext24Rot(r)        ((r) == 0 ? 16 : (r) - 8)
#define IcPrev24Rot(r)        ((r) == 16 ? 0 : (r) + 8)

#if IMAGE_BYTE_ORDER == MSBFirst
#define IcFirst24Rot(x)		(((x) + 16) % 24)
#else
#define IcFirst24Rot(x)		((x) % 24)
#endif
#endif

#define IcNext24RotStip(r)        ((r) == 0 ? 16 : (r) - 8)
#define IcPrev24RotStip(r)        ((r) == 16 ? 0 : (r) + 8)

/* Whether 24-bit specific code is needed for this filled pixel value */
#define IcCheck24Pix(p)	((p) == IcNext24Pix(p))

#define IcGetPixels(icpixels, pointer, _stride_, _bpp_, xoff, yoff) { \
    (pointer) = icpixels->data; \
    (_stride_) = icpixels->stride / sizeof(pixman_bits_t); \
    (_bpp_) = icpixels->bpp; \
    (xoff) = icpixels->x; /* XXX: fb.h had this ifdef'd to constant 0. Why? */ \
    (yoff) = icpixels->y; /* XXX: fb.h had this ifdef'd to constant 0. Why? */ \
}

#define IcGetStipPixels(icpixels, pointer, _stride_, _bpp_, xoff, yoff) { \
    (pointer) = (IcStip *) icpixels->data; \
    (_stride_) = icpixels->stride; \
    (_bpp_) = icpixels->bpp; \
    (xoff) = icpixels->x; \
    (yoff) = icpixels->y; \
}

#ifdef IC_OLD_SCREEN
#define BitsPerPixel(d) (\
    ((1 << PixmapWidthPaddingInfo[d].padBytesLog2) * 8 / \
    (PixmapWidthPaddingInfo[d].padRoundUp+1)))
#endif

#define IcPowerOfTwo(w)	    (((w) & ((w) - 1)) == 0)
/*
 * Accelerated tiles are power of 2 width <= IC_UNIT
 */
#define IcEvenTile(w)	    ((w) <= IC_UNIT && IcPowerOfTwo(w))
/*
 * Accelerated stipples are power of 2 width and <= IC_UNIT/dstBpp
 * with dstBpp a power of 2 as well
 */
#define IcEvenStip(w,bpp)   ((w) * (bpp) <= IC_UNIT && IcPowerOfTwo(w) && IcPowerOfTwo(bpp))

/*
 * icblt.c
 */
pixman_private void
IcBlt (pixman_bits_t   *src, 
       IcStride	srcStride,
       int	srcX,
       
       pixman_bits_t   *dst,
       IcStride dstStride,
       int	dstX,
       
       int	width, 
       int	height,
       
       int	alu,
       pixman_bits_t	pm,
       int	bpp,
       
       int	reverse,
       int	upsidedown);

pixman_private void
IcBlt24 (pixman_bits_t	    *srcLine,
	 IcStride   srcStride,
	 int	    srcX,

	 pixman_bits_t	    *dstLine,
	 IcStride   dstStride,
	 int	    dstX,

	 int	    width, 
	 int	    height,

	 int	    alu,
	 pixman_bits_t	    pm,

	 int	    reverse,
	 int	    upsidedown);
    
pixman_private void
IcBltStip (IcStip   *src,
	   IcStride srcStride,	    /* in IcStip units, not pixman_bits_t units */
	   int	    srcX,
	   
	   IcStip   *dst,
	   IcStride dstStride,	    /* in IcStip units, not pixman_bits_t units */
	   int	    dstX,

	   int	    width, 
	   int	    height,

	   int	    alu,
	   pixman_bits_t   pm,
	   int	    bpp);
    
/*
 * icbltone.c
 */
pixman_private void
IcBltOne (IcStip   *src,
	  IcStride srcStride,
	  int	   srcX,
	  pixman_bits_t   *dst,
	  IcStride dstStride,
	  int	   dstX,
	  int	   dstBpp,

	  int	   width,
	  int	   height,

	  pixman_bits_t   fgand,
	  pixman_bits_t   icxor,
	  pixman_bits_t   bgand,
	  pixman_bits_t   bgxor);
 
#ifdef IC_24BIT
pixman_private void
IcBltOne24 (IcStip    *src,
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
	  pixman_bits_t    bgxor);
#endif

/*
 * icstipple.c
 */

pixman_private void
IcTransparentSpan (pixman_bits_t   *dst,
		   pixman_bits_t   stip,
		   pixman_bits_t   fgxor,
		   int	    n);

pixman_private void
IcEvenStipple (pixman_bits_t   *dst,
	       IcStride dstStride,
	       int	dstX,
	       int	dstBpp,

	       int	width,
	       int	height,

	       IcStip   *stip,
	       IcStride	stipStride,
	       int	stipHeight,

	       pixman_bits_t   fgand,
	       pixman_bits_t   fgxor,
	       pixman_bits_t   bgand,
	       pixman_bits_t   bgxor,

	       int	xRot,
	       int	yRot);

pixman_private void
IcOddStipple (pixman_bits_t	*dst,
	      IcStride	dstStride,
	      int	dstX,
	      int	dstBpp,

	      int	width,
	      int	height,

	      IcStip	*stip,
	      IcStride	stipStride,
	      int	stipWidth,
	      int	stipHeight,

	      pixman_bits_t	fgand,
	      pixman_bits_t	fgxor,
	      pixman_bits_t	bgand,
	      pixman_bits_t	bgxor,

	      int	xRot,
	      int	yRot);

pixman_private void
IcStipple (pixman_bits_t   *dst,
	   IcStride dstStride,
	   int	    dstX,
	   int	    dstBpp,

	   int	    width,
	   int	    height,

	   IcStip   *stip,
	   IcStride stipStride,
	   int	    stipWidth,
	   int	    stipHeight,
	   int	    even,

	   pixman_bits_t   fgand,
	   pixman_bits_t   fgxor,
	   pixman_bits_t   bgand,
	   pixman_bits_t   bgxor,

	   int	    xRot,
	   int	    yRot);

/* XXX: Is depth redundant here? */
struct pixman_format {
    int		format_code;
    int		depth;
    int		red, redMask;
    int		green, greenMask;
    int		blue, blueMask;
    int		alpha, alphaMask;
};

typedef struct _IcPixels {
    pixman_bits_t		*data;
    unsigned int	width;
    unsigned int	height;
    unsigned int	depth;
    unsigned int	bpp;
    unsigned int	stride;
    int			x;
    int			y;
    unsigned int	refcnt;
} IcPixels;

/* XXX: This is to avoid including colormap.h from the server includes */
typedef uint32_t Pixel;

/* icutil.c */
pixman_private pixman_bits_t
IcReplicatePixel (Pixel p, int bpp);

/* fbtrap.c */

pixman_private void
fbRasterizeTrapezoid (pixman_image_t		*pMask,
		      const pixman_trapezoid_t  *pTrap,
		      int		x_off,
		      int		y_off);

/* XXX: This is to avoid including gc.h from the server includes */
/* clientClipType field in GC */
#define CT_NONE			0
#define CT_PIXMAP		1
#define CT_REGION		2
#define CT_UNSORTED		6
#define CT_YSORTED		10
#define CT_YXSORTED		14
#define CT_YXBANDED		18

#include "icimage.h"

/* iccolor.c */

/* GCC 3.4 supports a "population count" builtin, which on many targets is
   implemented with a single instruction.  There is a fallback definition
   in libgcc in case a target does not have one, which should be just as
   good as the static function below.  */
#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
# if __INT_MIN__ == 0x7fffffff
#  define _IcOnes(mask)		__builtin_popcount(mask)
# else
#  define _IcOnes(mask)		__builtin_popcountl((mask) & 0xffffffff)
# endif
#else
# define ICINT_NEED_IC_ONES
int
_IcOnes(unsigned long mask);
#endif

/* icformat.c */

pixman_private void
pixman_format_init (pixman_format_t *format, int format_code);

/* icimage.c */

pixman_private pixman_image_t *
pixman_image_createForPixels (IcPixels	*pixels,
			pixman_format_t	*format);

/* icpixels.c */

pixman_private IcPixels *
IcPixelsCreate (int width, int height, int depth);

pixman_private IcPixels *
IcPixelsCreateForData (pixman_bits_t *data, int width, int height, int depth, int bpp, int stride);

pixman_private void
IcPixelsDestroy (IcPixels *pixels);

/* ictransform.c */

pixman_private int
pixman_transform_point (pixman_transform_t	*transform,
		  pixman_vector_t	*vector);

/* Avoid unnessecary PLT entries.  */

slim_hidden_proto(pixman_image_create)
slim_hidden_proto(pixman_color_to_pixel)
slim_hidden_proto(pixman_format_init)
slim_hidden_proto(pixman_image_destroy)
slim_hidden_proto(pixman_fill_rectangles)
slim_hidden_proto(pixman_image_set_component_alpha)
slim_hidden_proto(pixman_image_set_repeat)
slim_hidden_proto(pixman_composite)


#include "icrop.h"

/* XXX: For now, I'm just wholesale pasting Xserver/render/picture.h here: */
#ifndef _PICTURE_H_
#define _PICTURE_H_

typedef struct _DirectFormat	*DirectFormatPtr;
typedef struct _PictFormat	*PictFormatPtr;

/*
 * While the protocol is generous in format support, the
 * sample implementation allows only packed RGB and GBR
 * representations for data to simplify software rendering,
 */
#define PICT_FORMAT(bpp,type,a,r,g,b)	(((bpp) << 24) |  \
					 ((type) << 16) | \
					 ((a) << 12) | \
					 ((r) << 8) | \
					 ((g) << 4) | \
					 ((b)))

/*
 * gray/color formats use a visual index instead of argb
 */
#define PICT_VISFORMAT(bpp,type,vi)	(((bpp) << 24) |  \
					 ((type) << 16) | \
					 ((vi)))

#define PICT_FORMAT_BPP(f)	(((f) >> 24)       )
#define PICT_FORMAT_TYPE(f)	(((f) >> 16) & 0xff)
#define PICT_FORMAT_A(f)	(((f) >> 12) & 0x0f)
#define PICT_FORMAT_R(f)	(((f) >>  8) & 0x0f)
#define PICT_FORMAT_G(f)	(((f) >>  4) & 0x0f)
#define PICT_FORMAT_B(f)	(((f)      ) & 0x0f)
#define PICT_FORMAT_RGB(f)	(((f)      ) & 0xfff)
#define PICT_FORMAT_VIS(f)	(((f)      ) & 0xffff)

#define PICT_TYPE_OTHER	0
#define PICT_TYPE_A	1
#define PICT_TYPE_ARGB	2
#define PICT_TYPE_ABGR	3
#define PICT_TYPE_COLOR	4
#define PICT_TYPE_GRAY	5

#define PICT_FORMAT_COLOR(f)	(PICT_FORMAT_TYPE(f) & 2)

/* 32bpp formats */
#define PICT_a8r8g8b8	PICT_FORMAT(32,PICT_TYPE_ARGB,8,8,8,8)
#define PICT_x8r8g8b8	PICT_FORMAT(32,PICT_TYPE_ARGB,0,8,8,8)
#define PICT_a8b8g8r8	PICT_FORMAT(32,PICT_TYPE_ABGR,8,8,8,8)
#define PICT_x8b8g8r8	PICT_FORMAT(32,PICT_TYPE_ABGR,0,8,8,8)

/* 24bpp formats */
#define PICT_r8g8b8	PICT_FORMAT(24,PICT_TYPE_ARGB,0,8,8,8)
#define PICT_b8g8r8	PICT_FORMAT(24,PICT_TYPE_ABGR,0,8,8,8)

/* 16bpp formats */
#define PICT_r5g6b5	PICT_FORMAT(16,PICT_TYPE_ARGB,0,5,6,5)
#define PICT_b5g6r5	PICT_FORMAT(16,PICT_TYPE_ABGR,0,5,6,5)

#define PICT_a1r5g5b5	PICT_FORMAT(16,PICT_TYPE_ARGB,1,5,5,5)
#define PICT_x1r5g5b5	PICT_FORMAT(16,PICT_TYPE_ARGB,0,5,5,5)
#define PICT_a1b5g5r5	PICT_FORMAT(16,PICT_TYPE_ABGR,1,5,5,5)
#define PICT_x1b5g5r5	PICT_FORMAT(16,PICT_TYPE_ABGR,0,5,5,5)
#define PICT_a4r4g4b4	PICT_FORMAT(16,PICT_TYPE_ARGB,4,4,4,4)
#define PICT_x4r4g4b4	PICT_FORMAT(16,PICT_TYPE_ARGB,4,4,4,4)
#define PICT_a4b4g4r4	PICT_FORMAT(16,PICT_TYPE_ARGB,4,4,4,4)
#define PICT_x4b4g4r4	PICT_FORMAT(16,PICT_TYPE_ARGB,4,4,4,4)

/* 8bpp formats */
#define PICT_a8		PICT_FORMAT(8,PICT_TYPE_A,8,0,0,0)
#define PICT_r3g3b2	PICT_FORMAT(8,PICT_TYPE_ARGB,0,3,3,2)
#define PICT_b2g3r3	PICT_FORMAT(8,PICT_TYPE_ABGR,0,3,3,2)
#define PICT_a2r2g2b2	PICT_FORMAT(8,PICT_TYPE_ARGB,2,2,2,2)
#define PICT_a2b2g2r2	PICT_FORMAT(8,PICT_TYPE_ABGR,2,2,2,2)

#define PICT_c8		PICT_FORMAT(8,PICT_TYPE_COLOR,0,0,0,0)
#define PICT_g8		PICT_FORMAT(8,PICT_TYPE_GRAY,0,0,0,0)

/* 4bpp formats */
#define PICT_a4		PICT_FORMAT(4,PICT_TYPE_A,4,0,0,0)
#define PICT_r1g2b1	PICT_FORMAT(4,PICT_TYPE_ARGB,0,1,2,1)
#define PICT_b1g2r1	PICT_FORMAT(4,PICT_TYPE_ABGR,0,1,2,1)
#define PICT_a1r1g1b1	PICT_FORMAT(4,PICT_TYPE_ARGB,1,1,1,1)
#define PICT_a1b1g1r1	PICT_FORMAT(4,PICT_TYPE_ABGR,1,1,1,1)
				    
#define PICT_c4		PICT_FORMAT(4,PICT_TYPE_COLOR,0,0,0,0)
#define PICT_g4		PICT_FORMAT(4,PICT_TYPE_GRAY,0,0,0,0)

/* 1bpp formats */
#define PICT_a1		PICT_FORMAT(1,PICT_TYPE_A,1,0,0,0)

#define PICT_g1		PICT_FORMAT(1,PICT_TYPE_GRAY,0,0,0,0)

/*
 * For dynamic indexed visuals (GrayScale and PseudoColor), these control the 
 * selection of colors allocated for drawing to Pictures.  The default
 * policy depends on the size of the colormap:
 *
 * Size		Default Policy
 * ----------------------------
 *  < 64	PolicyMono
 *  < 256	PolicyGray
 *  256		PolicyColor (only on PseudoColor)
 *
 * The actual allocation code lives in miindex.c, and so is
 * austensibly server dependent, but that code does:
 *
 * PolicyMono	    Allocate no additional colors, use black and white
 * PolicyGray	    Allocate 13 gray levels (11 cells used)
 * PolicyColor	    Allocate a 4x4x4 cube and 13 gray levels (71 cells used)
 * PolicyAll	    Allocate as big a cube as possible, fill with gray (all)
 *
 * Here's a picture to help understand how many colors are
 * actually allocated (this is just the gray ramp):
 *
 *                 gray level
 * all   0000 1555 2aaa 4000 5555 6aaa 8000 9555 aaaa bfff d555 eaaa ffff
 * b/w   0000                                                        ffff
 * 4x4x4                     5555                aaaa
 * extra      1555 2aaa 4000      6aaa 8000 9555      bfff d555 eaaa
 *
 * The default colormap supplies two gray levels (black/white), the
 * 4x4x4 cube allocates another two and nine more are allocated to fill
 * in the 13 levels.  When the 4x4x4 cube is not allocated, a total of
 * 11 cells are allocated.
 */   

#define PictureCmapPolicyInvalid    -1
#define PictureCmapPolicyDefault    0
#define PictureCmapPolicyMono	    1
#define PictureCmapPolicyGray	    2
#define PictureCmapPolicyColor	    3
#define PictureCmapPolicyAll	    4

extern int PictureCmapPolicy pixman_private;

int	PictureParseCmapPolicy (const char *name);

/* Fixed point updates from Carl Worth, USC, Information Sciences Institute */

#ifdef WIN32
typedef __int64		xFixed_32_32;
#else
#  if defined(__alpha__) || defined(__alpha) || \
      defined(ia64) || defined(__ia64__) || \
      defined(__sparc64__) || \
      defined(__s390x__) || \
      defined(x86_64) || defined (__x86_64__)
typedef long		xFixed_32_32;
# else
#  if defined(__GNUC__) && \
    ((__GNUC__ > 2) || \
     ((__GNUC__ == 2) && defined(__GNUC_MINOR__) && (__GNUC_MINOR__ > 7)))
__extension__
#  endif
typedef long long int	xFixed_32_32;
# endif
#endif

typedef xFixed_32_32		xFixed_48_16;
typedef uint32_t		xFixed_1_31;
typedef uint32_t		xFixed_1_16;
typedef int32_t		xFixed_16_16;

/*
 * An unadorned "xFixed" is the same as xFixed_16_16, 
 * (since it's quite common in the code) 
 */
typedef	xFixed_16_16	xFixed;
#define XFIXED_BITS	16

#define xFixedToInt(f)	(int) ((f) >> XFIXED_BITS)
#define IntToxFixed(i)	((xFixed) (i) << XFIXED_BITS)
#define xFixedE		((xFixed) 1)
#define xFixed1		(IntToxFixed(1))
#define xFixed1MinusE	(xFixed1 - xFixedE)
#define xFixedFrac(f)	((f) & xFixed1MinusE)
#define xFixedFloor(f)	((f) & ~xFixed1MinusE)
#define xFixedCeil(f)	xFixedFloor((f) + xFixed1MinusE)

#define xFixedFraction(f)	((f) & xFixed1MinusE)
#define xFixedMod2(f)		((f) & (xFixed1 | xFixed1MinusE))

/* whether 't' is a well defined not obviously empty trapezoid */
#define xTrapezoidValid(t)  ((t)->left.p1.y != (t)->left.p2.y && \
			     (t)->right.p1.y != (t)->right.p2.y && \
			     (int) ((t)->bottom - (t)->top) > 0)

/*
 * Standard NTSC luminance conversions:
 *
 *  y = r * 0.299 + g * 0.587 + b * 0.114
 *
 * Approximate this for a bit more speed:
 *
 *  y = (r * 153 + g * 301 + b * 58) / 512
 *
 * This gives 17 bits of luminance; to get 15 bits, lop the low two
 */

#define CvtR8G8B8toY15(s)	(((((s) >> 16) & 0xff) * 153 + \
				  (((s) >>  8) & 0xff) * 301 + \
				  (((s)      ) & 0xff) * 58) >> 2)

#endif /* _PICTURE_H_ */

#endif /* _ICINT_H_ */
