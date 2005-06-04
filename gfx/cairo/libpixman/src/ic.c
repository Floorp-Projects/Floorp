/*
 * Copyright © 2000 SuSE, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of SuSE not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  SuSE makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * SuSE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL SuSE
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Keith Packard, SuSE, Inc.
 */

#include "icint.h"

#define cvt8888to0565(s)    ((((s) >> 3) & 0x001f) | \
			     (((s) >> 5) & 0x07e0) | \
			     (((s) >> 8) & 0xf800))
#define cvt0565to8888(s)    (((((s) << 3) & 0xf8) | (((s) >> 2) & 0x7)) | \
			     ((((s) << 5) & 0xfc00) | (((s) >> 1) & 0x300)) | \
			     ((((s) << 8) & 0xf80000) | (((s) << 3) & 0x70000)))

#if IMAGE_BYTE_ORDER == MSBFirst
#define Fetch24(a)  ((unsigned long) (a) & 1 ? \
		     ((*(a) << 16) | *((uint16_t *) ((a)+1))) : \
		     ((*((uint16_t *) (a)) << 8) | *((a)+2)))
#define Store24(a,v) ((unsigned long) (a) & 1 ? \
		      ((*(a) = (uint8_t) ((v) >> 16)), \
		       (*((uint16_t *) ((a)+1)) = (uint16_t) (v))) : \
		      ((*((uint16_t *) (a)) = (uint16_t) ((v) >> 8)), \
		       (*((a)+2) = (uint8_t) (v))))
#else
#define Fetch24(a)  ((unsigned long) (a) & 1 ? \
		     ((*(a)) | (*((uint16_t *) ((a)+1)) << 8)) : \
		     ((*((uint16_t *) (a))) | (*((a)+2) << 16)))
#define Store24(a,v) ((unsigned long) (a) & 1 ? \
		      ((*(a) = (uint8_t) (v)), \
		       (*((uint16_t *) ((a)+1)) = (uint16_t) ((v) >> 8))) : \
		      ((*((uint16_t *) (a)) = (uint16_t) (v)),\
		       (*((a)+2) = (uint8_t) ((v) >> 16))))
#endif
		      
static uint32_t
IcOver (uint32_t x, uint32_t y)
{
    uint16_t  a = ~x >> 24;
    uint16_t  t;
    uint32_t  m,n,o,p;

    m = IcOverU(x,y,0,a,t);
    n = IcOverU(x,y,8,a,t);
    o = IcOverU(x,y,16,a,t);
    p = IcOverU(x,y,24,a,t);
    return m|n|o|p;
}

static uint32_t
IcOver24 (uint32_t x, uint32_t y)
{
    uint16_t  a = ~x >> 24;
    uint16_t  t;
    uint32_t  m,n,o;

    m = IcOverU(x,y,0,a,t);
    n = IcOverU(x,y,8,a,t);
    o = IcOverU(x,y,16,a,t);
    return m|n|o;
}

static uint32_t
IcIn (uint32_t x, uint8_t y)
{
    uint16_t  a = y;
    uint16_t  t;
    uint32_t  m,n,o,p;

    m = IcInU(x,0,a,t);
    n = IcInU(x,8,a,t);
    o = IcInU(x,16,a,t);
    p = IcInU(x,24,a,t);
    return m|n|o|p;
}

#define IcComposeGetSolid(image, bits) { \
    pixman_bits_t	*__bits__; \
    IcStride	__stride__; \
    int		__bpp__; \
    int		__xoff__,__yoff__; \
\
    IcGetPixels((image)->pixels,__bits__,__stride__,__bpp__,__xoff__,__yoff__); \
    switch (__bpp__) { \
    case 32: \
	(bits) = *(uint32_t *) __bits__; \
	break; \
    case 24: \
	(bits) = Fetch24 ((uint8_t *) __bits__); \
	break; \
    case 16: \
	(bits) = *(uint16_t *) __bits__; \
	(bits) = cvt0565to8888(bits); \
	break; \
    default: \
	return; \
    } \
    /* manage missing src alpha */ \
    if ((image)->image_format.alphaMask == 0) \
	(bits) |= 0xff000000; \
}

#define IcComposeGetStart(image,x,y,type,stride,line,mul) {\
    pixman_bits_t	*__bits__; \
    IcStride	__stride__; \
    int		__bpp__; \
    int		__xoff__,__yoff__; \
\
    IcGetPixels((image)->pixels,__bits__,__stride__,__bpp__,__xoff__,__yoff__); \
    (stride) = __stride__ * sizeof (pixman_bits_t) / sizeof (type); \
    (line) = ((type *) __bits__) + (stride) * ((y) - __yoff__) + (mul) * ((x) - __xoff__); \
}

/*
 * Naming convention:
 *
 *  opSRCxMASKxDST
 */

static void
pixman_compositeSolidMask_nx8x8888 (pixman_operator_t   op,
			       pixman_image_t    *iSrc,
			       pixman_image_t    *iMask,
			       pixman_image_t    *iDst,
			       int16_t      xSrc,
			       int16_t      ySrc,
			       int16_t      xMask,
			       int16_t      yMask,
			       int16_t      xDst,
			       int16_t      yDst,
			       uint16_t     width,
			       uint16_t     height)
{
    uint32_t	src, srca;
    uint32_t	*dstLine, *dst, d, dstMask;
    uint8_t	*maskLine, *mask, m;
    IcStride	dstStride, maskStride;
    uint16_t	w;

    IcComposeGetSolid(iSrc, src);
    
    dstMask = IcFullMask (iDst->pixels->depth);
    srca = src >> 24;
    if (src == 0)
	return;
    
    IcComposeGetStart (iDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);
    IcComposeGetStart (iMask, xMask, yMask, uint8_t, maskStride, maskLine, 1);
    
    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	while (w--)
	{
	    m = *mask++;
	    if (m == 0xff)
	    {
		if (srca == 0xff)
		    *dst = src & dstMask;
		else
		    *dst = IcOver (src, *dst) & dstMask;
	    }
	    else if (m)
	    {
		d = IcIn (src, m);
		*dst = IcOver (d, *dst) & dstMask;
	    }
	    dst++;
	}
    }
}

static void
pixman_compositeSolidMask_nx8888x8888C (pixman_operator_t   op,
				   pixman_image_t    *iSrc,
				   pixman_image_t    *iMask,
				   pixman_image_t    *iDst,
				   int16_t      xSrc,
				   int16_t      ySrc,
				   int16_t      xMask,
				   int16_t      yMask,
				   int16_t      xDst,
				   int16_t      yDst,
				   uint16_t     width,
				   uint16_t     height)
{
    uint32_t	src, srca;
    uint32_t	*dstLine, *dst, d, dstMask;
    uint32_t	*maskLine, *mask, ma;
    IcStride	dstStride, maskStride;
    uint16_t	w;
    uint32_t	m, n, o, p;

    IcComposeGetSolid(iSrc, src);
    
    dstMask = IcFullMask (iDst->pixels->depth);
    srca = src >> 24;
    if (src == 0)
	return;
    
    IcComposeGetStart (iDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);
    IcComposeGetStart (iMask, xMask, yMask, uint32_t, maskStride, maskLine, 1);
    
    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	while (w--)
	{
	    ma = *mask++;
	    if (ma == 0xffffffff)
	    {
		if (srca == 0xff)
		    *dst = src & dstMask;
		else
		    *dst = IcOver (src, *dst) & dstMask;
	    }
	    else if (ma)
	    {
		d = *dst;
#define IcInOverC(src,srca,msk,dst,i,result) { \
    uint16_t  __a = IcGet8(msk,i); \
    uint32_t  __t, __ta; \
    uint32_t  __i; \
    __t = IcIntMult (IcGet8(src,i), __a,__i); \
    __ta = (uint8_t) ~IcIntMult (srca, __a,__i); \
    __t = __t + IcIntMult(IcGet8(dst,i),__ta,__i); \
    __t = (uint32_t) (uint8_t) (__t | (-(__t >> 8))); \
    result = __t << (i); \
}
		IcInOverC (src, srca, ma, d, 0, m);
		IcInOverC (src, srca, ma, d, 8, n);
		IcInOverC (src, srca, ma, d, 16, o);
		IcInOverC (src, srca, ma, d, 24, p);
		*dst = m|n|o|p;
	    }
	    dst++;
	}
    }
}

static void
pixman_compositeSolidMask_nx8x0888 (pixman_operator_t   op,
			       pixman_image_t    *iSrc,
			       pixman_image_t    *iMask,
			       pixman_image_t    *iDst,
			       int16_t      xSrc,
			       int16_t      ySrc,
			       int16_t      xMask,
			       int16_t      yMask,
			       int16_t      xDst,
			       int16_t      yDst,
			       uint16_t     width,
			       uint16_t     height)
{
    uint32_t	src, srca;
    uint8_t	*dstLine, *dst;
    uint32_t	d;
    uint8_t	*maskLine, *mask, m;
    IcStride	dstStride, maskStride;
    uint16_t	w;

    IcComposeGetSolid(iSrc, src);
    
    srca = src >> 24;
    if (src == 0)
	return;
    
    IcComposeGetStart (iDst, xDst, yDst, uint8_t, dstStride, dstLine, 3);
    IcComposeGetStart (iMask, xMask, yMask, uint8_t, maskStride, maskLine, 1);
    
    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	while (w--)
	{
	    m = *mask++;
	    if (m == 0xff)
	    {
		if (srca == 0xff)
		    d = src;
		else
		{
		    d = Fetch24(dst);
		    d = IcOver24 (src, d);
		}
		Store24(dst,d);
	    }
	    else if (m)
	    {
		d = IcOver24 (IcIn(src,m), Fetch24(dst));
		Store24(dst,d);
	    }
	    dst += 3;
	}
    }
}

static void
pixman_compositeSolidMask_nx8x0565 (pixman_operator_t      op,
				  pixman_image_t    *iSrc,
				  pixman_image_t    *iMask,
				  pixman_image_t    *iDst,
				  int16_t      xSrc,
				  int16_t      ySrc,
				  int16_t      xMask,
				  int16_t      yMask,
				  int16_t      xDst,
				  int16_t      yDst,
				  uint16_t     width,
				  uint16_t     height)
{
    uint32_t	src, srca;
    uint16_t	*dstLine, *dst;
    uint32_t	d;
    uint8_t	*maskLine, *mask, m;
    IcStride	dstStride, maskStride;
    uint16_t	w;

    IcComposeGetSolid(iSrc, src);
    
    srca = src >> 24;
    if (src == 0)
	return;
    
    IcComposeGetStart (iDst, xDst, yDst, uint16_t, dstStride, dstLine, 1);
    IcComposeGetStart (iMask, xMask, yMask, uint8_t, maskStride, maskLine, 1);
    
    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	while (w--)
	{
	    m = *mask++;
	    if (m == 0xff)
	    {
		if (srca == 0xff)
		    d = src;
		else
		{
		    d = *dst;
		    d = IcOver24 (src, cvt0565to8888(d));
		}
		*dst = cvt8888to0565(d);
	    }
	    else if (m)
	    {
		d = *dst;
		d = IcOver24 (IcIn(src,m), cvt0565to8888(d));
		*dst = cvt8888to0565(d);
	    }
	    dst++;
	}
    }
}

static void
pixman_compositeSolidMask_nx8888x0565C (pixman_operator_t   op,
				   pixman_image_t    *iSrc,
				   pixman_image_t    *iMask,
				   pixman_image_t    *iDst,
				   int16_t      xSrc,
				   int16_t      ySrc,
				   int16_t      xMask,
				   int16_t      yMask,
				   int16_t      xDst,
				   int16_t      yDst,
				   uint16_t     width,
				   uint16_t     height)
{
    uint32_t	src, srca;
    uint16_t	src16;
    uint16_t	*dstLine, *dst;
    uint32_t	d;
    uint32_t	*maskLine, *mask, ma;
    IcStride	dstStride, maskStride;
    uint16_t	w;
    uint32_t	m, n, o;

    IcComposeGetSolid(iSrc, src);
    
    srca = src >> 24;
    if (src == 0)
	return;
    
    src16 = cvt8888to0565(src);
    
    IcComposeGetStart (iDst, xDst, yDst, uint16_t, dstStride, dstLine, 1);
    IcComposeGetStart (iMask, xMask, yMask, uint32_t, maskStride, maskLine, 1);
    
    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	while (w--)
	{
	    ma = *mask++;
	    if (ma == 0xffffffff)
	    {
		if (srca == 0xff)
		{
		    *dst = src16;
		}
		else
		{
		    d = *dst;
		    d = IcOver24 (src, cvt0565to8888(d));
		    *dst = cvt8888to0565(d);
		}
	    }
	    else if (ma)
	    {
		d = *dst;
		d = cvt0565to8888(d);
		IcInOverC (src, srca, ma, d, 0, m);
		IcInOverC (src, srca, ma, d, 8, n);
		IcInOverC (src, srca, ma, d, 16, o);
		d = m|n|o;
		*dst = cvt8888to0565(d);
	    }
	    dst++;
	}
    }
}

static void
pixman_compositeSrc_8888x8888 (pixman_operator_t  op,
			 pixman_image_t    *iSrc,
			 pixman_image_t    *iMask,
			 pixman_image_t    *iDst,
			 int16_t      xSrc,
			 int16_t      ySrc,
			 int16_t      xMask,
			 int16_t      yMask,
			 int16_t      xDst,
			 int16_t      yDst,
			 uint16_t     width,
			 uint16_t     height)
{
    uint32_t	*dstLine, *dst, dstMask;
    uint32_t	*srcLine, *src, s;
    IcStride	dstStride, srcStride;
    uint8_t	a;
    uint16_t	w;
    
    IcComposeGetStart (iDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);
    IcComposeGetStart (iSrc, xSrc, ySrc, uint32_t, srcStride, srcLine, 1);
    
    dstMask = IcFullMask (iDst->pixels->depth);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w--)
	{
	    s = *src++;
	    a = s >> 24;
	    if (a == 0xff)
		*dst = s & dstMask;
	    else if (a)
		*dst = IcOver (s, *dst) & dstMask;
	    dst++;
	}
    }
}

static void
pixman_compositeSrc_8888x0888 (pixman_operator_t  op,
			 pixman_image_t    *iSrc,
			 pixman_image_t    *iMask,
			 pixman_image_t    *iDst,
			 int16_t      xSrc,
			 int16_t      ySrc,
			 int16_t      xMask,
			 int16_t      yMask,
			 int16_t      xDst,
			 int16_t      yDst,
			 uint16_t     width,
			 uint16_t     height)
{
    uint8_t	*dstLine, *dst;
    uint32_t	d;
    uint32_t	*srcLine, *src, s;
    uint8_t	a;
    IcStride	dstStride, srcStride;
    uint16_t	w;
    
    IcComposeGetStart (iDst, xDst, yDst, uint8_t, dstStride, dstLine, 3);
    IcComposeGetStart (iSrc, xSrc, ySrc, uint32_t, srcStride, srcLine, 1);
    
    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w--)
	{
	    s = *src++;
	    a = s >> 24;
	    if (a)
	    {
		if (a == 0xff)
		    d = s;
		else
		    d = IcOver24 (s, Fetch24(dst));
		Store24(dst,d);
	    }
	    dst += 3;
	}
    }
}

static void
pixman_compositeSrc_8888x0565 (pixman_operator_t  op,
			 pixman_image_t    *iSrc,
			 pixman_image_t    *iMask,
			 pixman_image_t    *iDst,
			 int16_t      xSrc,
			 int16_t      ySrc,
			 int16_t      xMask,
			 int16_t      yMask,
			 int16_t      xDst,
			 int16_t      yDst,
			 uint16_t     width,
			 uint16_t     height)
{
    uint16_t	*dstLine, *dst;
    uint32_t	d;
    uint32_t	*srcLine, *src, s;
    uint8_t	a;
    IcStride	dstStride, srcStride;
    uint16_t	w;
    
    IcComposeGetStart (iSrc, xSrc, ySrc, uint32_t, srcStride, srcLine, 1);
    IcComposeGetStart (iDst, xDst, yDst, uint16_t, dstStride, dstLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w--)
	{
	    s = *src++;
	    a = s >> 24;
	    if (a)
	    {
		if (a == 0xff)
		    d = s;
		else
		{
		    d = *dst;
		    d = IcOver24 (s, cvt0565to8888(d));
		}
		*dst = cvt8888to0565(d);
	    }
	    dst++;
	}
    }
}

static void
pixman_compositeSrc_0565x0565 (pixman_operator_t   op,
			  pixman_image_t    *iSrc,
			  pixman_image_t    *iMask,
			  pixman_image_t    *iDst,
			  int16_t      xSrc,
			  int16_t      ySrc,
			  int16_t      xMask,
			  int16_t      yMask,
			  int16_t      xDst,
			  int16_t      yDst,
			  uint16_t     width,
			  uint16_t     height)
{
    uint16_t	*dstLine, *dst;
    uint16_t	*srcLine, *src;
    IcStride	dstStride, srcStride;
    uint16_t	w;
    
    IcComposeGetStart (iSrc, xSrc, ySrc, uint16_t, srcStride, srcLine, 1);

    IcComposeGetStart (iDst, xDst, yDst, uint16_t, dstStride, dstLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w--)
	    *dst++ = *src++;
    }
}

static void
pixman_compositeSrcAdd_8000x8000 (pixman_operator_t	  op,
			     pixman_image_t    *iSrc,
			     pixman_image_t    *iMask,
			     pixman_image_t    *iDst,
			     int16_t      xSrc,
			     int16_t      ySrc,
			     int16_t      xMask,
			     int16_t      yMask,
			     int16_t      xDst,
			     int16_t      yDst,
			     uint16_t     width,
			     uint16_t     height)
{
    uint8_t	*dstLine, *dst;
    uint8_t	*srcLine, *src;
    IcStride	dstStride, srcStride;
    uint8_t	w;
    uint8_t	s, d;
    uint16_t	t;
    
    IcComposeGetStart (iSrc, xSrc, ySrc, uint8_t, srcStride, srcLine, 1);
    IcComposeGetStart (iDst, xDst, yDst, uint8_t, dstStride, dstLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w--)
	{
	    s = *src++;
	    if (s != 0xff)
	    {
		d = *dst;
		t = d + s;
		s = t | (0 - (t >> 8));
	    }
	    *dst++ = s;
	}
    }
}

static void
pixman_compositeSrcAdd_8888x8888 (pixman_operator_t   op,
			     pixman_image_t    *iSrc,
			     pixman_image_t    *iMask,
			     pixman_image_t    *iDst,
			     int16_t      xSrc,
			     int16_t      ySrc,
			     int16_t      xMask,
			     int16_t      yMask,
			     int16_t      xDst,
			     int16_t      yDst,
			     uint16_t     width,
			     uint16_t     height)
{
    uint32_t	*dstLine, *dst;
    uint32_t	*srcLine, *src;
    IcStride	dstStride, srcStride;
    uint16_t	w;
    uint32_t	s, d;
    uint16_t	t;
    uint32_t	m,n,o,p;
    
    IcComposeGetStart (iSrc, xSrc, ySrc, uint32_t, srcStride, srcLine, 1);
    IcComposeGetStart (iDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w--)
	{
	    s = *src++;
	    if (s != 0xffffffff)
	    {
		d = *dst;
		if (d)
		{
		    m = IcAdd(s,d,0,t);
		    n = IcAdd(s,d,8,t);
		    o = IcAdd(s,d,16,t);
		    p = IcAdd(s,d,24,t);
		    s = m|n|o|p;
		}
	    }
	    *dst++ = s;
	}
    }
}

static void
pixman_compositeSrcAdd_1000x1000 (pixman_operator_t   op,
			     pixman_image_t    *iSrc,
			     pixman_image_t    *iMask,
			     pixman_image_t    *iDst,
			     int16_t      xSrc,
			     int16_t      ySrc,
			     int16_t      xMask,
			     int16_t      yMask,
			     int16_t      xDst,
			     int16_t      yDst,
			     uint16_t     width,
			     uint16_t     height)
{
    pixman_bits_t	*dstBits, *srcBits;
    IcStride	dstStride, srcStride;
    int		dstBpp, srcBpp;
    int		dstXoff, dstYoff;
    int		srcXoff, srcYoff;
    
    IcGetPixels(iSrc->pixels, srcBits, srcStride, srcBpp, srcXoff, srcYoff);

    IcGetPixels(iDst->pixels, dstBits, dstStride, dstBpp, dstXoff, dstYoff);

    IcBlt (srcBits + srcStride * (ySrc + srcYoff),
	   srcStride,
	   xSrc + srcXoff,

	   dstBits + dstStride * (yDst + dstYoff),
	   dstStride,
	   xDst + dstXoff,

	   width,
	   height,

	   GXor,
	   IC_ALLONES,
	   srcBpp,

	   0,
	   0);
}

static void
pixman_compositeSolidMask_nx1xn (pixman_operator_t   op,
			    pixman_image_t    *iSrc,
			    pixman_image_t    *iMask,
			    pixman_image_t    *iDst,
			    int16_t      xSrc,
			    int16_t      ySrc,
			    int16_t      xMask,
			    int16_t      yMask,
			    int16_t      xDst,
			    int16_t      yDst,
			    uint16_t     width,
			    uint16_t     height)
{
    pixman_bits_t	*dstBits;
    IcStip	*maskBits;
    IcStride	dstStride, maskStride;
    int		dstBpp, maskBpp;
    int		dstXoff, dstYoff;
    int		maskXoff, maskYoff;
    pixman_bits_t	src;
    
    IcComposeGetSolid(iSrc, src);

    if ((src & 0xff000000) != 0xff000000)
    {
	pixman_compositeGeneral  (op, iSrc, iMask, iDst,
			     xSrc, ySrc, xMask, yMask, xDst, yDst, 
			     width, height);
	return;
    }
    IcGetStipPixels (iMask->pixels, maskBits, maskStride, maskBpp, maskXoff, maskYoff);
    IcGetPixels (iDst->pixels, dstBits, dstStride, dstBpp, dstXoff, dstYoff);

    switch (dstBpp) {
    case 32:
	break;
    case 24:
	break;
    case 16:
	src = cvt8888to0565(src);
	break;
    }

    src = IcReplicatePixel (src, dstBpp);

    IcBltOne (maskBits + maskStride * (yMask + maskYoff),
	      maskStride,
	      xMask + maskXoff,

	      dstBits + dstStride * (yDst + dstYoff),
	      dstStride,
	      (xDst + dstXoff) * dstBpp,
	      dstBpp,

	      width * dstBpp,
	      height,

	      0x0,
	      src,
	      IC_ALLONES,
	      0x0);
}

void
pixman_composite (pixman_operator_t	op,
	     pixman_image_t	*iSrc,
	     pixman_image_t	*iMask,
	     pixman_image_t	*iDst,
	     int	xSrc,
	     int	ySrc,
	     int	xMask,
	     int	yMask,
	     int	xDst,
	     int	yDst,
	     int	width,
	     int	height)
{
    pixman_region16_t	    *region;
    int		    n;
    pixman_box16_t    *pbox;
    CompositeFunc   func;
    int	    srcRepeat = iSrc->repeat;
    int	    maskRepeat = 0;
    int	    srcAlphaMap = iSrc->alphaMap != 0;
    int	    maskAlphaMap = 0;
    int	    dstAlphaMap = iDst->alphaMap != 0;
    int		    x_msk, y_msk, x_src, y_src, x_dst, y_dst;
    int		    w, h, w_this, h_this;

    if (iSrc->pixels->width == 0 ||
	iSrc->pixels->height == 0)
    {
	return;
    }
    
    xDst += iDst->pixels->x;
    yDst += iDst->pixels->y;
    xSrc += iSrc->pixels->x;
    ySrc += iSrc->pixels->y;
    if (iMask)
    {
	xMask += iMask->pixels->x;
	yMask += iMask->pixels->y;
	maskRepeat = iMask->repeat;
	maskAlphaMap = iMask->alphaMap != 0;
    }

    region = pixman_region_create();
    pixman_region_union_rect (region, region, xDst, yDst, width, height);
    
    if (!IcComputeCompositeRegion (region,
				   iSrc,
				   iMask,
				   iDst,
				   xSrc,
				   ySrc,
				   xMask,
				   yMask,
				   xDst,
				   yDst,
				   width,
				   height))
	return;
				   
    func = pixman_compositeGeneral;
    if (!iSrc->transform && !(iMask && iMask->transform))
    if (!maskAlphaMap && !srcAlphaMap && !dstAlphaMap)
    switch (op) {
    case PIXMAN_OPERATOR_OVER:
	if (iMask)
	{
	    if (srcRepeat && 
		iSrc->pixels->width == 1 &&
		iSrc->pixels->height == 1)
	    {
		srcRepeat = 0;
		if (PICT_FORMAT_COLOR(iSrc->format_code)) {
		    switch (iMask->format_code) {
		    case PICT_a8:
			switch (iDst->format_code) {
			case PICT_r5g6b5:
			case PICT_b5g6r5:
			    func = pixman_compositeSolidMask_nx8x0565;
			    break;
			case PICT_r8g8b8:
			case PICT_b8g8r8:
			    func = pixman_compositeSolidMask_nx8x0888;
			    break;
			case PICT_a8r8g8b8:
			case PICT_x8r8g8b8:
			case PICT_a8b8g8r8:
			case PICT_x8b8g8r8:
			    func = pixman_compositeSolidMask_nx8x8888;
			    break;
			}
			break;
		    case PICT_a8r8g8b8:
			if (iMask->componentAlpha) {
			    switch (iDst->format_code) {
			    case PICT_a8r8g8b8:
			    case PICT_x8r8g8b8:
				func = pixman_compositeSolidMask_nx8888x8888C;
				break;
			    case PICT_r5g6b5:
				func = pixman_compositeSolidMask_nx8888x0565C;
				break;
			    }
			}
			break;
		    case PICT_a8b8g8r8:
			if (iMask->componentAlpha) {
			    switch (iDst->format_code) {
			    case PICT_a8b8g8r8:
			    case PICT_x8b8g8r8:
				func = pixman_compositeSolidMask_nx8888x8888C;
				break;
			    case PICT_b5g6r5:
				func = pixman_compositeSolidMask_nx8888x0565C;
				break;
			    }
			}
			break;
		    case PICT_a1:
			switch (iDst->format_code) {
			case PICT_r5g6b5:
			case PICT_b5g6r5:
			case PICT_r8g8b8:
			case PICT_b8g8r8:
			case PICT_a8r8g8b8:
			case PICT_x8r8g8b8:
			case PICT_a8b8g8r8:
			case PICT_x8b8g8r8:
			    func = pixman_compositeSolidMask_nx1xn;
			    break;
			}
		    }
		}
	    }
	}
	else
	{
	    switch (iSrc->format_code) {
	    case PICT_a8r8g8b8:
		switch (iDst->format_code) {
		case PICT_a8r8g8b8:
		case PICT_x8r8g8b8:
		    func = pixman_compositeSrc_8888x8888;
		    break;
		case PICT_r8g8b8:
		    func = pixman_compositeSrc_8888x0888;
		    break;
		case PICT_r5g6b5:
		    func = pixman_compositeSrc_8888x0565;
		    break;
		}
		break;
	    case PICT_a8b8g8r8:
		switch (iDst->format_code) {
		case PICT_a8b8g8r8:
		case PICT_x8b8g8r8:
		    func = pixman_compositeSrc_8888x8888;
		    break;
		case PICT_b8g8r8:
		    func = pixman_compositeSrc_8888x0888;
		    break;
		case PICT_b5g6r5:
		    func = pixman_compositeSrc_8888x0565;
		    break;
		}
		break;
	    case PICT_r5g6b5:
		switch (iDst->format_code) {
		case PICT_r5g6b5:
		    func = pixman_compositeSrc_0565x0565;
		    break;
		}
		break;
	    case PICT_b5g6r5:
		switch (iDst->format_code) {
		case PICT_b5g6r5:
		    func = pixman_compositeSrc_0565x0565;
		    break;
		}
		break;
	    }
	}
	break;
    case PIXMAN_OPERATOR_ADD:
	if (iMask == 0)
	{
	    switch (iSrc->format_code) {
	    case PICT_a8r8g8b8:
		switch (iDst->format_code) {
		case PICT_a8r8g8b8:
		    func = pixman_compositeSrcAdd_8888x8888;
		    break;
		}
		break;
	    case PICT_a8b8g8r8:
		switch (iDst->format_code) {
		case PICT_a8b8g8r8:
		    func = pixman_compositeSrcAdd_8888x8888;
		    break;
		}
		break;
	    case PICT_a8:
		switch (iDst->format_code) {
		case PICT_a8:
		    func = pixman_compositeSrcAdd_8000x8000;
		    break;
		}
		break;
	    case PICT_a1:
		switch (iDst->format_code) {
		case PICT_a1:
		    func = pixman_compositeSrcAdd_1000x1000;
		    break;
		}
		break;
	    }
	}
	break;
    default:
	func = pixman_compositeGeneral;
	break;
    }
    /* if we are transforming, we handle repeats in
     * IcFetch[a]_transform
     */
    if (iSrc->transform)
      srcRepeat = 0;
    if (iMask && iMask->transform)
      maskRepeat = 0;
    
    n = pixman_region_num_rects (region);
    pbox = pixman_region_rects (region);
    while (n--)
    {
	h = pbox->y2 - pbox->y1;
	y_src = pbox->y1 - yDst + ySrc;
	y_msk = pbox->y1 - yDst + yMask;
	y_dst = pbox->y1;
	while (h)
	{
	    h_this = h;
	    w = pbox->x2 - pbox->x1;
	    x_src = pbox->x1 - xDst + xSrc;
	    x_msk = pbox->x1 - xDst + xMask;
	    x_dst = pbox->x1;
	    if (maskRepeat)
	    {
		y_msk = MOD (y_msk, iMask->pixels->height);
		if (h_this > iMask->pixels->height - y_msk)
		    h_this = iMask->pixels->height - y_msk;
	    }
	    if (srcRepeat)
	    {
		y_src = MOD (y_src, iSrc->pixels->height);
		if (h_this > iSrc->pixels->height - y_src)
		    h_this = iSrc->pixels->height - y_src;
	    }
	    while (w)
	    {
		w_this = w;
		if (maskRepeat)
		{
		    x_msk = MOD (x_msk, iMask->pixels->width);
		    if (w_this > iMask->pixels->width - x_msk)
			w_this = iMask->pixels->width - x_msk;
		}
		if (srcRepeat)
		{
		    x_src = MOD (x_src, iSrc->pixels->width);
		    if (w_this > iSrc->pixels->width - x_src)
			w_this = iSrc->pixels->width - x_src;
		}
		(*func) (op, iSrc, iMask, iDst,
			 x_src, y_src, x_msk, y_msk, x_dst, y_dst, 
			 w_this, h_this);
		w -= w_this;
		x_src += w_this;
		x_msk += w_this;
		x_dst += w_this;
	    }
	    h -= h_this;
	    y_src += h_this;
	    y_msk += h_this;
	    y_dst += h_this;
	}
	pbox++;
    }
    pixman_region_destroy (region);
}
slim_hidden_def(pixman_composite);
