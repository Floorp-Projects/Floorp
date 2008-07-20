/* -*- Mode: c; c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t; -*- */
/*
 * Copyright © 2000 SuSE, Inc.
 * Copyright © 2007 Red Hat, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pixman-private.h"
#include "pixman-mmx.h"
#include "pixman-vmx.h"
#include "pixman-sse.h"
#include "pixman-combine32.h"

#ifdef __GNUC__
#   define inline __inline__ __attribute__ ((__always_inline__))
#endif

#define FbFullMask(n)   ((n) == 32 ? (uint32_t)-1 : ((((uint32_t) 1) << n) - 1))

#undef READ
#undef WRITE
#define READ(img,x) (*(x))
#define WRITE(img,ptr,v) ((*(ptr)) = (v))

typedef void (* CompositeFunc) (pixman_op_t,
				pixman_image_t *, pixman_image_t *, pixman_image_t *,
				int16_t, int16_t, int16_t, int16_t, int16_t, int16_t,
				uint16_t, uint16_t);

static inline uint32_t
fbOver (uint32_t src, uint32_t dest)
{
    // dest = (dest * (255 - alpha)) / 255 + src
    uint32_t a = ~src >> 24; // 255 - alpha == 255 + (~alpha + 1) == ~alpha
    FbByteMulAdd(dest, a, src);

    return dest;
}

static uint32_t
fbOver24 (uint32_t x, uint32_t y)
{
    uint16_t  a = ~x >> 24;
    uint16_t  t;
    uint32_t  m,n,o;

    m = FbOverU(x,y,0,a,t);
    n = FbOverU(x,y,8,a,t);
    o = FbOverU(x,y,16,a,t);
    return m|n|o;
}

static uint32_t
fbIn (uint32_t x, uint8_t y)
{
    uint16_t  a = y;
    uint16_t  t;
    uint32_t  m,n,o,p;

    m = FbInU(x,0,a,t);
    n = FbInU(x,8,a,t);
    o = FbInU(x,16,a,t);
    p = FbInU(x,24,a,t);
    return m|n|o|p;
}

/*
 * Naming convention:
 *
 *  opSRCxMASKxDST
 */

static void
fbCompositeOver_x888x8x8888 (pixman_op_t      op,
			     pixman_image_t * pSrc,
			     pixman_image_t * pMask,
			     pixman_image_t * pDst,
			     int16_t      xSrc,
			     int16_t      ySrc,
			     int16_t      xMask,
			     int16_t      yMask,
			     int16_t      xDst,
			     int16_t      yDst,
			     uint16_t     width,
			     uint16_t     height)
{
    uint32_t	*src, *srcLine;
    uint32_t    *dst, *dstLine;
    uint8_t	*mask, *maskLine;
    int		 srcStride, maskStride, dstStride;
    uint8_t m;
    uint32_t s, d;
    uint16_t w;

    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, uint8_t, maskStride, maskLine, 1);
    fbComposeGetStart (pSrc, xSrc, ySrc, uint32_t, srcStride, srcLine, 1);

    while (height--)
    {
	src = srcLine;
	srcLine += srcStride;
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;

	w = width;
	while (w--)
	{
	    m = READ(pMask, mask++);
	    if (m)
	    {
		s = READ(pSrc, src) | 0xff000000;

		if (m == 0xff)
		    WRITE(pDst, dst, s);
		else
		{
		    d = fbIn (s, m);
		    WRITE(pDst, dst, fbOver (d, READ(pDst, dst)));
		}
	    }
	    src++;
	    dst++;
	}
    }
}

static void
fbCompositeSolidMaskIn_nx8x8 (pixman_op_t      op,
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
    uint8_t	*dstLine, *dst, dstMask;
    uint8_t	*maskLine, *mask, m;
    int	dstStride, maskStride;
    uint16_t	w;
    uint16_t    t;

    fbComposeGetSolid(iSrc, src, iDst->bits.format);

    dstMask = FbFullMask (PIXMAN_FORMAT_DEPTH (iDst->bits.format));
    srca = src >> 24;

    fbComposeGetStart (iDst, xDst, yDst, uint8_t, dstStride, dstLine, 1);
    fbComposeGetStart (iMask, xMask, yMask, uint8_t, maskStride, maskLine, 1);

    if (srca == 0xff) {
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
		if (m == 0)
		{
		    *dst = 0;
		}
		else if (m != 0xff)
		{
		    *dst = FbIntMult(m, *dst, t);
		}
		dst++;
	    }
	}
    }
    else
    {
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
		m = FbIntMult(m, srca, t);
		if (m == 0)
		{
		    *dst = 0;
		}
		else if (m != 0xff)
		{
		    *dst = FbIntMult(m, *dst, t);
		}
		dst++;
	    }
	}
    }
}


static void
fbCompositeSrcIn_8x8 (pixman_op_t      op,
		      pixman_image_t  *iSrc,
		      pixman_image_t  *iMask,
		      pixman_image_t  *iDst,
		      int16_t          xSrc,
		      int16_t          ySrc,
		      int16_t          xMask,
		      int16_t          yMask,
		      int16_t          xDst,
		      int16_t          yDst,
		      uint16_t         width,
		      uint16_t         height)
{
    uint8_t	*dstLine, *dst;
    uint8_t	*srcLine, *src;
    int	dstStride, srcStride;
    uint16_t	w;
    uint8_t	s;
    uint16_t	t;

    fbComposeGetStart (iSrc, xSrc, ySrc, uint8_t, srcStride, srcLine, 1);
    fbComposeGetStart (iDst, xDst, yDst, uint8_t, dstStride, dstLine, 1);

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
	    if (s == 0)
	    {
		*dst = 0;
	    }
	    else if (s != 0xff)
	    {
		*dst = FbIntMult(s, *dst, t);
	    }
	    dst++;
	}
    }
}

void
fbCompositeSolidMask_nx8x8888 (pixman_op_t      op,
			       pixman_image_t * pSrc,
			       pixman_image_t * pMask,
			       pixman_image_t * pDst,
			       int16_t      xSrc,
			       int16_t      ySrc,
			       int16_t      xMask,
			       int16_t      yMask,
			       int16_t      xDst,
			       int16_t      yDst,
			       uint16_t     width,
			       uint16_t     height)
{
    uint32_t	 src, srca;
    uint32_t	*dstLine, *dst, d, dstMask;
    uint8_t	*maskLine, *mask, m;
    int		 dstStride, maskStride;
    uint16_t	 w;

    fbComposeGetSolid(pSrc, src, pDst->bits.format);

    dstMask = FbFullMask (PIXMAN_FORMAT_DEPTH (pDst->bits.format));
    srca = src >> 24;
    if (src == 0)
	return;

    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, uint8_t, maskStride, maskLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	while (w--)
	{
	    m = READ(pMask, mask++);
	    if (m == 0xff)
	    {
		if (srca == 0xff)
		    WRITE(pDst, dst, src & dstMask);
		else
		    WRITE(pDst, dst, fbOver (src, READ(pDst, dst)) & dstMask);
	    }
	    else if (m)
	    {
		d = fbIn (src, m);
		WRITE(pDst, dst, fbOver (d, READ(pDst, dst)) & dstMask);
	    }
	    dst++;
	}
    }
}

void
fbCompositeSolidMask_nx8888x8888C (pixman_op_t op,
				   pixman_image_t * pSrc,
				   pixman_image_t * pMask,
				   pixman_image_t * pDst,
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
    int	dstStride, maskStride;
    uint16_t	w;
    uint32_t	m, n, o, p;

    fbComposeGetSolid(pSrc, src, pDst->bits.format);

    dstMask = FbFullMask (PIXMAN_FORMAT_DEPTH (pDst->bits.format));
    srca = src >> 24;
    if (src == 0)
	return;

    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, uint32_t, maskStride, maskLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	while (w--)
	{
	    ma = READ(pMask, mask++);
	    if (ma == 0xffffffff)
	    {
		if (srca == 0xff)
		    WRITE(pDst, dst, src & dstMask);
		else
		    WRITE(pDst, dst, fbOver (src, READ(pDst, dst)) & dstMask);
	    }
	    else if (ma)
	    {
		d = READ(pDst, dst);
#define FbInOverC(src,srca,msk,dst,i,result) { \
    uint16_t  __a = FbGet8(msk,i); \
    uint32_t  __t, __ta; \
    uint32_t  __i; \
    __t = FbIntMult (FbGet8(src,i), __a,__i); \
    __ta = (uint8_t) ~FbIntMult (srca, __a,__i); \
    __t = __t + FbIntMult(FbGet8(dst,i),__ta,__i); \
    __t = (uint32_t) (uint8_t) (__t | (-(__t >> 8))); \
    result = __t << (i); \
}
		FbInOverC (src, srca, ma, d, 0, m);
		FbInOverC (src, srca, ma, d, 8, n);
		FbInOverC (src, srca, ma, d, 16, o);
		FbInOverC (src, srca, ma, d, 24, p);
		WRITE(pDst, dst, m|n|o|p);
	    }
	    dst++;
	}
    }
}

void
fbCompositeSolidMask_nx8x0888 (pixman_op_t op,
			       pixman_image_t * pSrc,
			       pixman_image_t * pMask,
			       pixman_image_t * pDst,
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
    int	dstStride, maskStride;
    uint16_t	w;

    fbComposeGetSolid(pSrc, src, pDst->bits.format);

    srca = src >> 24;
    if (src == 0)
	return;

    fbComposeGetStart (pDst, xDst, yDst, uint8_t, dstStride, dstLine, 3);
    fbComposeGetStart (pMask, xMask, yMask, uint8_t, maskStride, maskLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	while (w--)
	{
	    m = READ(pMask, mask++);
	    if (m == 0xff)
	    {
		if (srca == 0xff)
		    d = src;
		else
		{
		    d = Fetch24(pDst, dst);
		    d = fbOver24 (src, d);
		}
		Store24(pDst, dst,d);
	    }
	    else if (m)
	    {
		d = fbOver24 (fbIn(src,m), Fetch24(pDst, dst));
		Store24(pDst, dst, d);
	    }
	    dst += 3;
	}
    }
}

void
fbCompositeSolidMask_nx8x0565 (pixman_op_t op,
				  pixman_image_t * pSrc,
				  pixman_image_t * pMask,
				  pixman_image_t * pDst,
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
    int	dstStride, maskStride;
    uint16_t	w;

    fbComposeGetSolid(pSrc, src, pDst->bits.format);

    srca = src >> 24;
    if (src == 0)
	return;

    fbComposeGetStart (pDst, xDst, yDst, uint16_t, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, uint8_t, maskStride, maskLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	while (w--)
	{
	    m = READ(pMask, mask++);
	    if (m == 0xff)
	    {
		if (srca == 0xff)
		    d = src;
		else
		{
		    d = READ(pDst, dst);
		    d = fbOver24 (src, cvt0565to0888(d));
		}
		WRITE(pDst, dst, cvt8888to0565(d));
	    }
	    else if (m)
	    {
		d = READ(pDst, dst);
		d = fbOver24 (fbIn(src,m), cvt0565to0888(d));
		WRITE(pDst, dst, cvt8888to0565(d));
	    }
	    dst++;
	}
    }
}

void
fbCompositeSolidMask_nx8888x0565C (pixman_op_t op,
				   pixman_image_t * pSrc,
				   pixman_image_t * pMask,
				   pixman_image_t * pDst,
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
    int	dstStride, maskStride;
    uint16_t	w;
    uint32_t	m, n, o;

    fbComposeGetSolid(pSrc, src, pDst->bits.format);

    srca = src >> 24;
    if (src == 0)
	return;

    src16 = cvt8888to0565(src);

    fbComposeGetStart (pDst, xDst, yDst, uint16_t, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, uint32_t, maskStride, maskLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	while (w--)
	{
	    ma = READ(pMask, mask++);
	    if (ma == 0xffffffff)
	    {
		if (srca == 0xff)
		{
		    WRITE(pDst, dst, src16);
		}
		else
		{
		    d = READ(pDst, dst);
		    d = fbOver24 (src, cvt0565to0888(d));
		    WRITE(pDst, dst, cvt8888to0565(d));
		}
	    }
	    else if (ma)
	    {
		d = READ(pDst, dst);
		d = cvt0565to0888(d);
		FbInOverC (src, srca, ma, d, 0, m);
		FbInOverC (src, srca, ma, d, 8, n);
		FbInOverC (src, srca, ma, d, 16, o);
		d = m|n|o;
		WRITE(pDst, dst, cvt8888to0565(d));
	    }
	    dst++;
	}
    }
}

void
fbCompositeSrc_8888x8888 (pixman_op_t op,
			 pixman_image_t * pSrc,
			 pixman_image_t * pMask,
			 pixman_image_t * pDst,
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
    int	dstStride, srcStride;
    uint8_t	a;
    uint16_t	w;

    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);
    fbComposeGetStart (pSrc, xSrc, ySrc, uint32_t, srcStride, srcLine, 1);

    dstMask = FbFullMask (PIXMAN_FORMAT_DEPTH (pDst->bits.format));

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w--)
	{
	    s = READ(pSrc, src++);
	    a = s >> 24;
	    if (a == 0xff)
		WRITE(pDst, dst, s & dstMask);
	    else if (a)
		WRITE(pDst, dst, fbOver (s, READ(pDst, dst)) & dstMask);
	    dst++;
	}
    }
}

void
fbCompositeSrc_8888x0888 (pixman_op_t op,
			 pixman_image_t * pSrc,
			 pixman_image_t * pMask,
			 pixman_image_t * pDst,
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
    int	dstStride, srcStride;
    uint16_t	w;

    fbComposeGetStart (pDst, xDst, yDst, uint8_t, dstStride, dstLine, 3);
    fbComposeGetStart (pSrc, xSrc, ySrc, uint32_t, srcStride, srcLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w--)
	{
	    s = READ(pSrc, src++);
	    a = s >> 24;
	    if (a)
	    {
		if (a == 0xff)
		    d = s;
		else
		    d = fbOver24 (s, Fetch24(pDst, dst));
		Store24(pDst, dst, d);
	    }
	    dst += 3;
	}
    }
}

void
fbCompositeSrc_8888x0565 (pixman_op_t op,
			 pixman_image_t * pSrc,
			 pixman_image_t * pMask,
			 pixman_image_t * pDst,
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
    int	dstStride, srcStride;
    uint16_t	w;

    fbComposeGetStart (pSrc, xSrc, ySrc, uint32_t, srcStride, srcLine, 1);
    fbComposeGetStart (pDst, xDst, yDst, uint16_t, dstStride, dstLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w--)
	{
	    s = READ(pSrc, src++);
	    a = s >> 24;
	    if (a)
	    {
		if (a == 0xff)
		    d = s;
		else
		{
		    d = READ(pDst, dst);
		    d = fbOver24 (s, cvt0565to0888(d));
		}
		WRITE(pDst, dst, cvt8888to0565(d));
	    }
	    dst++;
	}
    }
}

void
fbCompositeSrcAdd_8000x8000 (pixman_op_t	op,
			     pixman_image_t * pSrc,
			     pixman_image_t * pMask,
			     pixman_image_t * pDst,
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
    int	dstStride, srcStride;
    uint16_t	w;
    uint8_t	s, d;
    uint16_t	t;

    fbComposeGetStart (pSrc, xSrc, ySrc, uint8_t, srcStride, srcLine, 1);
    fbComposeGetStart (pDst, xDst, yDst, uint8_t, dstStride, dstLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w--)
	{
	    s = READ(pSrc, src++);
	    if (s)
	    {
		if (s != 0xff)
		{
		    d = READ(pDst, dst);
		    t = d + s;
		    s = t | (0 - (t >> 8));
		}
		WRITE(pDst, dst, s);
	    }
	    dst++;
	}
    }
}

void
fbCompositeSrcAdd_8888x8888 (pixman_op_t	op,
			     pixman_image_t * pSrc,
			     pixman_image_t * pMask,
			     pixman_image_t * pDst,
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
    int	dstStride, srcStride;
    uint16_t	w;
    uint32_t	s, d;
    uint16_t	t;
    uint32_t	m,n,o,p;

    fbComposeGetStart (pSrc, xSrc, ySrc, uint32_t, srcStride, srcLine, 1);
    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dstLine, 1);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	src = srcLine;
	srcLine += srcStride;
	w = width;

	while (w--)
	{
	    s = READ(pSrc, src++);
	    if (s)
	    {
		if (s != 0xffffffff)
		{
		    d = READ(pDst, dst);
		    if (d)
		    {
			m = FbAdd(s,d,0,t);
			n = FbAdd(s,d,8,t);
			o = FbAdd(s,d,16,t);
			p = FbAdd(s,d,24,t);
			s = m|n|o|p;
		    }
		}
		WRITE(pDst, dst, s);
	    }
	    dst++;
	}
    }
}

static void
fbCompositeSrcAdd_8888x8x8 (pixman_op_t op,
			    pixman_image_t * pSrc,
			    pixman_image_t * pMask,
			    pixman_image_t * pDst,
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
    uint8_t	*maskLine, *mask;
    int	dstStride, maskStride;
    uint16_t	w;
    uint32_t	src;
    uint8_t	sa;

    fbComposeGetStart (pDst, xDst, yDst, uint8_t, dstStride, dstLine, 1);
    fbComposeGetStart (pMask, xMask, yMask, uint8_t, maskStride, maskLine, 1);
    fbComposeGetSolid (pSrc, src, pDst->bits.format);
    sa = (src >> 24);

    while (height--)
    {
	dst = dstLine;
	dstLine += dstStride;
	mask = maskLine;
	maskLine += maskStride;
	w = width;

	while (w--)
	{
	    uint16_t	tmp;
	    uint16_t	a;
	    uint32_t	m, d;
	    uint32_t	r;

	    a = READ(pMask, mask++);
	    d = READ(pDst, dst);

	    m = FbInU (sa, 0, a, tmp);
	    r = FbAdd (m, d, 0, tmp);

	    WRITE(pDst, dst++, r);
	}
    }
}

void
fbCompositeSrcAdd_1000x1000 (pixman_op_t	op,
			     pixman_image_t * pSrc,
			     pixman_image_t * pMask,
			     pixman_image_t * pDst,
			     int16_t      xSrc,
			     int16_t      ySrc,
			     int16_t      xMask,
			     int16_t      yMask,
			     int16_t      xDst,
			     int16_t      yDst,
			     uint16_t     width,
			     uint16_t     height)
{
    /* FIXME */
#if 0

    uint32_t	*dstBits, *srcBits;
    int	dstStride, srcStride;
    int		dstBpp, srcBpp;
    int		dstXoff, dstYoff;
    int		srcXoff, srcYoff;

    fbGetDrawable(pSrc->pDrawable, srcBits, srcStride, srcBpp, srcXoff, srcYoff);

    fbGetDrawable(pDst->pDrawable, dstBits, dstStride, dstBpp, dstXoff, dstYoff);

    fbBlt (srcBits + srcStride * (ySrc + srcYoff),
	   srcStride,
	   xSrc + srcXoff,

	   dstBits + dstStride * (yDst + dstYoff),
	   dstStride,
	   xDst + dstXoff,

	   width,
	   height,

	   GXor,
	   FB_ALLONES,
	   srcBpp,

	   FALSE,
	   FALSE);

#endif
}

void
fbCompositeSolidMask_nx1xn (pixman_op_t op,
			    pixman_image_t * pSrc,
			    pixman_image_t * pMask,
			    pixman_image_t * pDst,
			    int16_t      xSrc,
			    int16_t      ySrc,
			    int16_t      xMask,
			    int16_t      yMask,
			    int16_t      xDst,
			    int16_t      yDst,
			    uint16_t     width,
			    uint16_t     height)
{
    /* FIXME */
#if 0
    uint32_t	*dstBits;
    uint32_t	*maskBits;
    int	dstStride, maskStride;
    int		dstBpp, maskBpp;
    int		dstXoff, dstYoff;
    int		maskXoff, maskYoff;
    uint32_t	src;

    fbComposeGetSolid(pSrc, src, pDst->bits.format);
    fbGetStipDrawable (pMask->pDrawable, maskBits, maskStride, maskBpp, maskXoff, maskYoff);
    fbGetDrawable (pDst->pDrawable, dstBits, dstStride, dstBpp, dstXoff, dstYoff);

    switch (dstBpp) {
    case 32:
	break;
    case 24:
	break;
    case 16:
	src = cvt8888to0565(src);
	break;
    }

    src = fbReplicatePixel (src, dstBpp);

    fbBltOne (maskBits + maskStride * (yMask + maskYoff),
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
	      FB_ALLONES,
	      0x0);

#endif
}

/*
 * Apply a constant alpha value in an over computation
 */
static void
fbCompositeSrcSrc_nxn  (pixman_op_t	   op,
			pixman_image_t * pSrc,
			pixman_image_t * pMask,
			pixman_image_t * pDst,
			int16_t      xSrc,
			int16_t      ySrc,
			int16_t      xMask,
			int16_t      yMask,
			int16_t      xDst,
			int16_t      yDst,
			uint16_t     width,
			uint16_t     height);

/*
 * Simple bitblt
 */

static void
fbCompositeSrcSrc_nxn  (pixman_op_t	   op,
			pixman_image_t * pSrc,
			pixman_image_t * pMask,
			pixman_image_t * pDst,
			int16_t      xSrc,
			int16_t      ySrc,
			int16_t      xMask,
			int16_t      yMask,
			int16_t      xDst,
			int16_t      yDst,
			uint16_t     width,
			uint16_t     height)
{
    /* FIXME */
#if 0
    uint32_t	*dst;
    uint32_t	*src;
    int	dstStride, srcStride;
    int		srcXoff, srcYoff;
    int		dstXoff, dstYoff;
    int		srcBpp;
    int		dstBpp;
    pixman_bool_t	reverse = FALSE;
    pixman_bool_t	upsidedown = FALSE;

    fbGetDrawable(pSrc->pDrawable,src,srcStride,srcBpp,srcXoff,srcYoff);
    fbGetDrawable(pDst->pDrawable,dst,dstStride,dstBpp,dstXoff,dstYoff);

    fbBlt (src + (ySrc + srcYoff) * srcStride,
	   srcStride,
	   (xSrc + srcXoff) * srcBpp,

	   dst + (yDst + dstYoff) * dstStride,
	   dstStride,
	   (xDst + dstXoff) * dstBpp,

	   (width) * dstBpp,
	   (height),

	   GXcopy,
	   FB_ALLONES,
	   dstBpp,

	   reverse,
	   upsidedown);
#endif
}

static void
pixman_image_composite_rect  (pixman_op_t                   op,
			      pixman_image_t               *src,
			      pixman_image_t               *mask,
			      pixman_image_t               *dest,
			      int16_t                       src_x,
			      int16_t                       src_y,
			      int16_t                       mask_x,
			      int16_t                       mask_y,
			      int16_t                       dest_x,
			      int16_t                       dest_y,
			      uint16_t                      width,
			      uint16_t                      height);
static void
fbCompositeSolidFill (pixman_op_t op,
		      pixman_image_t * pSrc,
		      pixman_image_t * pMask,
		      pixman_image_t * pDst,
		      int16_t      xSrc,
		      int16_t      ySrc,
		      int16_t      xMask,
		      int16_t      yMask,
		      int16_t      xDst,
		      int16_t      yDst,
		      uint16_t     width,
		      uint16_t     height)
{
    uint32_t	src;

    fbComposeGetSolid(pSrc, src, pDst->bits.format);

    if (pDst->bits.format == PIXMAN_a8)
	src = src >> 24;
    else if (pDst->bits.format == PIXMAN_r5g6b5 ||
	     pDst->bits.format == PIXMAN_b5g6r5)
	src = cvt8888to0565 (src);

    pixman_fill (pDst->bits.bits, pDst->bits.rowstride,
		 PIXMAN_FORMAT_BPP (pDst->bits.format),
		 xDst, yDst,
		 width, height,
		 src);
}

static void
fbCompositeSrc_8888xx888 (pixman_op_t op,
			  pixman_image_t * pSrc,
			  pixman_image_t * pMask,
			  pixman_image_t * pDst,
			  int16_t      xSrc,
			  int16_t      ySrc,
			  int16_t      xMask,
			  int16_t      yMask,
			  int16_t      xDst,
			  int16_t      yDst,
			  uint16_t     width,
			  uint16_t     height)
{
    uint32_t	*dst;
    uint32_t    *src;
    int		 dstStride, srcStride;
    uint32_t	 n_bytes = width * sizeof (uint32_t);

    fbComposeGetStart (pSrc, xSrc, ySrc, uint32_t, srcStride, src, 1);
    fbComposeGetStart (pDst, xDst, yDst, uint32_t, dstStride, dst, 1);

    while (height--)
    {
	memcpy (dst, src, n_bytes);

	dst += dstStride;
	src += srcStride;
    }
}

static void
pixman_walk_composite_region (pixman_op_t op,
			      pixman_image_t * pSrc,
			      pixman_image_t * pMask,
			      pixman_image_t * pDst,
			      int16_t xSrc,
			      int16_t ySrc,
			      int16_t xMask,
			      int16_t yMask,
			      int16_t xDst,
			      int16_t yDst,
			      uint16_t width,
			      uint16_t height,
			      pixman_bool_t srcRepeat,
			      pixman_bool_t maskRepeat,
			      CompositeFunc compositeRect)
{
    int		    n;
    const pixman_box32_t *pbox;
    int		    w, h, w_this, h_this;
    int		    x_msk, y_msk, x_src, y_src, x_dst, y_dst;
    pixman_region32_t reg;
    pixman_region32_t *region;

    pixman_region32_init (&reg);
    if (!pixman_compute_composite_region32 (&reg, pSrc, pMask, pDst,
					    xSrc, ySrc, xMask, yMask, xDst, yDst, width, height))
    {
	return;
    }

    region = &reg;

    pbox = pixman_region32_rectangles (region, &n);
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
		y_msk = MOD (y_msk, pMask->bits.height);
		if (h_this > pMask->bits.height - y_msk)
		    h_this = pMask->bits.height - y_msk;
	    }
	    if (srcRepeat)
	    {
		y_src = MOD (y_src, pSrc->bits.height);
		if (h_this > pSrc->bits.height - y_src)
		    h_this = pSrc->bits.height - y_src;
	    }
	    while (w)
	    {
		w_this = w;
		if (maskRepeat)
		{
		    x_msk = MOD (x_msk, pMask->bits.width);
		    if (w_this > pMask->bits.width - x_msk)
			w_this = pMask->bits.width - x_msk;
		}
		if (srcRepeat)
		{
		    x_src = MOD (x_src, pSrc->bits.width);
		    if (w_this > pSrc->bits.width - x_src)
			w_this = pSrc->bits.width - x_src;
		}
		(*compositeRect) (op, pSrc, pMask, pDst,
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
    pixman_region32_fini (&reg);
}

static void
pixman_image_composite_rect  (pixman_op_t                   op,
			      pixman_image_t               *src,
			      pixman_image_t               *mask,
			      pixman_image_t               *dest,
			      int16_t                       src_x,
			      int16_t                       src_y,
			      int16_t                       mask_x,
			      int16_t                       mask_y,
			      int16_t                       dest_x,
			      int16_t                       dest_y,
			      uint16_t                      width,
			      uint16_t                      height)
{
    FbComposeData compose_data;

    return_if_fail (src != NULL);
    return_if_fail (dest != NULL);

    compose_data.op = op;
    compose_data.src = src;
    compose_data.mask = mask;
    compose_data.dest = dest;
    compose_data.xSrc = src_x;
    compose_data.ySrc = src_y;
    compose_data.xMask = mask_x;
    compose_data.yMask = mask_y;
    compose_data.xDest = dest_x;
    compose_data.yDest = dest_y;
    compose_data.width = width;
    compose_data.height = height;

    pixman_composite_rect_general (&compose_data);
}

/* These "formats" both have depth 0, so they
 * will never clash with any real ones
 */
#define PIXMAN_null		PIXMAN_FORMAT(0,0,0,0,0,0)
#define PIXMAN_solid		PIXMAN_FORMAT(0,1,0,0,0,0)

#define NEED_COMPONENT_ALPHA		(1 << 0)
#define NEED_PIXBUF			(1 << 1)
#define NEED_SOLID_MASK		        (1 << 2)

typedef struct
{
    pixman_op_t			op;
    pixman_format_code_t	src_format;
    pixman_format_code_t	mask_format;
    pixman_format_code_t	dest_format;
    CompositeFunc		func;
    uint32_t			flags;
} FastPathInfo;

#ifdef USE_MMX
static const FastPathInfo mmx_fast_paths[] =
{
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_r5g6b5,   fbCompositeSolidMask_nx8x0565mmx,     0 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_b5g6r5,   fbCompositeSolidMask_nx8x0565mmx,     0 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_a8r8g8b8, fbCompositeSolidMask_nx8x8888mmx,     0 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_x8r8g8b8, fbCompositeSolidMask_nx8x8888mmx,     0 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_a8b8g8r8, fbCompositeSolidMask_nx8x8888mmx,     0 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_x8b8g8r8, fbCompositeSolidMask_nx8x8888mmx,     0 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8r8g8b8, PIXMAN_a8r8g8b8, fbCompositeSolidMask_nx8888x8888Cmmx, NEED_COMPONENT_ALPHA },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8r8g8b8, PIXMAN_x8r8g8b8, fbCompositeSolidMask_nx8888x8888Cmmx, NEED_COMPONENT_ALPHA },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8r8g8b8, PIXMAN_r5g6b5,   fbCompositeSolidMask_nx8888x0565Cmmx, NEED_COMPONENT_ALPHA },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8b8g8r8, PIXMAN_a8b8g8r8, fbCompositeSolidMask_nx8888x8888Cmmx, NEED_COMPONENT_ALPHA },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8b8g8r8, PIXMAN_x8b8g8r8, fbCompositeSolidMask_nx8888x8888Cmmx, NEED_COMPONENT_ALPHA },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8b8g8r8, PIXMAN_b5g6r5,   fbCompositeSolidMask_nx8888x0565Cmmx, NEED_COMPONENT_ALPHA },
    { PIXMAN_OP_OVER, PIXMAN_x8b8g8r8, PIXMAN_a8r8g8b8, PIXMAN_a8r8g8b8, fbCompositeSrc_8888RevNPx8888mmx, NEED_PIXBUF },
    { PIXMAN_OP_OVER, PIXMAN_x8b8g8r8, PIXMAN_a8b8g8r8, PIXMAN_a8r8g8b8, fbCompositeSrc_8888RevNPx8888mmx, NEED_PIXBUF },
    { PIXMAN_OP_OVER, PIXMAN_x8b8g8r8, PIXMAN_a8r8g8b8, PIXMAN_x8r8g8b8, fbCompositeSrc_8888RevNPx8888mmx, NEED_PIXBUF },
    { PIXMAN_OP_OVER, PIXMAN_x8b8g8r8, PIXMAN_a8b8g8r8, PIXMAN_x8r8g8b8, fbCompositeSrc_8888RevNPx8888mmx, NEED_PIXBUF },
    { PIXMAN_OP_OVER, PIXMAN_x8b8g8r8, PIXMAN_a8r8g8b8, PIXMAN_r5g6b5,   fbCompositeSrc_8888RevNPx0565mmx, NEED_PIXBUF },
    { PIXMAN_OP_OVER, PIXMAN_x8b8g8r8, PIXMAN_a8b8g8r8, PIXMAN_r5g6b5,   fbCompositeSrc_8888RevNPx0565mmx, NEED_PIXBUF },
    { PIXMAN_OP_OVER, PIXMAN_x8r8g8b8, PIXMAN_a8r8g8b8, PIXMAN_a8b8g8r8, fbCompositeSrc_8888RevNPx8888mmx, NEED_PIXBUF },
    { PIXMAN_OP_OVER, PIXMAN_x8r8g8b8, PIXMAN_a8b8g8r8, PIXMAN_a8b8g8r8, fbCompositeSrc_8888RevNPx8888mmx, NEED_PIXBUF },
    { PIXMAN_OP_OVER, PIXMAN_x8r8g8b8, PIXMAN_a8r8g8b8, PIXMAN_x8b8g8r8, fbCompositeSrc_8888RevNPx8888mmx, NEED_PIXBUF },
    { PIXMAN_OP_OVER, PIXMAN_x8r8g8b8, PIXMAN_a8b8g8r8, PIXMAN_x8b8g8r8, fbCompositeSrc_8888RevNPx8888mmx, NEED_PIXBUF },
    { PIXMAN_OP_OVER, PIXMAN_x8r8g8b8, PIXMAN_a8r8g8b8, PIXMAN_b5g6r5,   fbCompositeSrc_8888RevNPx0565mmx, NEED_PIXBUF },
    { PIXMAN_OP_OVER, PIXMAN_x8r8g8b8, PIXMAN_a8b8g8r8, PIXMAN_b5g6r5,   fbCompositeSrc_8888RevNPx0565mmx, NEED_PIXBUF },
    { PIXMAN_OP_OVER, PIXMAN_x8r8g8b8, PIXMAN_a8,       PIXMAN_a8r8g8b8, fbCompositeSrc_x888xnx8888mmx,    NEED_SOLID_MASK },
    { PIXMAN_OP_OVER, PIXMAN_x8r8g8b8, PIXMAN_a8,       PIXMAN_x8r8g8b8, fbCompositeSrc_x888xnx8888mmx,	   NEED_SOLID_MASK },
    { PIXMAN_OP_OVER, PIXMAN_x8b8g8r8, PIXMAN_a8,	PIXMAN_a8b8g8r8, fbCompositeSrc_x888xnx8888mmx,	   NEED_SOLID_MASK },
    { PIXMAN_OP_OVER, PIXMAN_x8b8g8r8, PIXMAN_a8,	PIXMAN_x8b8g8r8, fbCompositeSrc_x888xnx8888mmx,	   NEED_SOLID_MASK },
    { PIXMAN_OP_OVER, PIXMAN_a8r8g8b8, PIXMAN_a8,       PIXMAN_a8r8g8b8, fbCompositeSrc_8888x8x8888mmx,    NEED_SOLID_MASK },
    { PIXMAN_OP_OVER, PIXMAN_a8r8g8b8, PIXMAN_a8,       PIXMAN_x8r8g8b8, fbCompositeSrc_8888x8x8888mmx,	   NEED_SOLID_MASK },
    { PIXMAN_OP_OVER, PIXMAN_a8b8g8r8, PIXMAN_a8,	PIXMAN_a8b8g8r8, fbCompositeSrc_8888x8x8888mmx,	   NEED_SOLID_MASK },
    { PIXMAN_OP_OVER, PIXMAN_a8b8g8r8, PIXMAN_a8,	PIXMAN_x8b8g8r8, fbCompositeSrc_8888x8x8888mmx,	   NEED_SOLID_MASK },
#if 0
    /* FIXME: This code is commented out since it's apparently not actually faster than the generic code. */
    { PIXMAN_OP_OVER, PIXMAN_x8r8g8b8, PIXMAN_a8,	PIXMAN_x8r8g8b8, fbCompositeOver_x888x8x8888mmx,   0 },
    { PIXMAN_OP_OVER, PIXMAN_x8r8g8b8, PIXMAN_a8,	PIXMAN_a8r8g8b8, fbCompositeOver_x888x8x8888mmx,   0 },
    { PIXMAN_OP_OVER, PIXMAN_x8b8r8g8, PIXMAN_a8,	PIXMAN_x8b8g8r8, fbCompositeOver_x888x8x8888mmx,   0 },
    { PIXMAN_OP_OVER, PIXMAN_x8b8r8g8, PIXMAN_a8,	PIXMAN_a8r8g8b8, fbCompositeOver_x888x8x8888mmx,   0 },
#endif
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_null,	PIXMAN_a8r8g8b8, fbCompositeSolid_nx8888mmx,        0 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_null,     PIXMAN_x8r8g8b8, fbCompositeSolid_nx8888mmx,	   0 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_null,     PIXMAN_r5g6b5,   fbCompositeSolid_nx0565mmx,	   0 },
    { PIXMAN_OP_OVER, PIXMAN_x8r8g8b8, PIXMAN_null,     PIXMAN_x8r8g8b8, fbCompositeCopyAreammx,	   0 },
    { PIXMAN_OP_OVER, PIXMAN_x8b8g8r8, PIXMAN_null,     PIXMAN_x8b8g8r8, fbCompositeCopyAreammx,	   0 },
    { PIXMAN_OP_OVER, PIXMAN_a8r8g8b8, PIXMAN_null,     PIXMAN_a8r8g8b8, fbCompositeSrc_8888x8888mmx,	   0 },
    { PIXMAN_OP_OVER, PIXMAN_a8r8g8b8, PIXMAN_null,	PIXMAN_x8r8g8b8, fbCompositeSrc_8888x8888mmx,	   0 },
    { PIXMAN_OP_OVER, PIXMAN_a8r8g8b8, PIXMAN_null,	PIXMAN_r5g6b5,	 fbCompositeSrc_8888x0565mmx,	   0 },
    { PIXMAN_OP_OVER, PIXMAN_a8b8g8r8, PIXMAN_null,	PIXMAN_a8b8g8r8, fbCompositeSrc_8888x8888mmx,	   0 },
    { PIXMAN_OP_OVER, PIXMAN_a8b8g8r8, PIXMAN_null,	PIXMAN_x8b8g8r8, fbCompositeSrc_8888x8888mmx,	   0 },
    { PIXMAN_OP_OVER, PIXMAN_a8b8g8r8, PIXMAN_null,     PIXMAN_b5g6r5,   fbCompositeSrc_8888x0565mmx,	   0 },

    { PIXMAN_OP_ADD, PIXMAN_a8r8g8b8,  PIXMAN_null,	PIXMAN_a8r8g8b8, fbCompositeSrcAdd_8888x8888mmx,   0 },
    { PIXMAN_OP_ADD, PIXMAN_a8b8g8r8,  PIXMAN_null,	PIXMAN_a8b8g8r8, fbCompositeSrcAdd_8888x8888mmx,   0 },
    { PIXMAN_OP_ADD, PIXMAN_a8,        PIXMAN_null,     PIXMAN_a8,       fbCompositeSrcAdd_8000x8000mmx,   0 },
    { PIXMAN_OP_ADD, PIXMAN_solid,     PIXMAN_a8,       PIXMAN_a8,       fbCompositeSrcAdd_8888x8x8mmx,    0 },
    { PIXMAN_OP_SRC, PIXMAN_solid,     PIXMAN_a8,       PIXMAN_a8r8g8b8, fbCompositeSolidMaskSrc_nx8x8888mmx, 0 },
    { PIXMAN_OP_SRC, PIXMAN_solid,     PIXMAN_a8,       PIXMAN_x8r8g8b8, fbCompositeSolidMaskSrc_nx8x8888mmx, 0 },
    { PIXMAN_OP_SRC, PIXMAN_solid,     PIXMAN_a8,       PIXMAN_a8b8g8r8, fbCompositeSolidMaskSrc_nx8x8888mmx, 0 },
    { PIXMAN_OP_SRC, PIXMAN_solid,     PIXMAN_a8,       PIXMAN_x8b8g8r8, fbCompositeSolidMaskSrc_nx8x8888mmx, 0 },

    { PIXMAN_OP_SRC, PIXMAN_a8r8g8b8,  PIXMAN_null,	PIXMAN_a8r8g8b8, fbCompositeCopyAreammx, 0 },
    { PIXMAN_OP_SRC, PIXMAN_a8b8g8r8,  PIXMAN_null,	PIXMAN_a8b8g8r8, fbCompositeCopyAreammx, 0 },
    { PIXMAN_OP_SRC, PIXMAN_a8r8g8b8,  PIXMAN_null,	PIXMAN_x8r8g8b8, fbCompositeCopyAreammx, 0 },
    { PIXMAN_OP_SRC, PIXMAN_a8b8g8r8,  PIXMAN_null,	PIXMAN_x8b8g8r8, fbCompositeCopyAreammx, 0 },
    { PIXMAN_OP_SRC, PIXMAN_x8r8g8b8,  PIXMAN_null,	PIXMAN_x8r8g8b8, fbCompositeCopyAreammx, 0 },
    { PIXMAN_OP_SRC, PIXMAN_x8b8g8r8,  PIXMAN_null,	PIXMAN_x8b8g8r8, fbCompositeCopyAreammx, 0 },
    { PIXMAN_OP_SRC, PIXMAN_r5g6b5,    PIXMAN_null,     PIXMAN_r5g6b5,   fbCompositeCopyAreammx, 0 },
    { PIXMAN_OP_SRC, PIXMAN_b5g6r5,    PIXMAN_null,     PIXMAN_b5g6r5,   fbCompositeCopyAreammx, 0 },
    { PIXMAN_OP_IN,  PIXMAN_a8,        PIXMAN_null,     PIXMAN_a8,       fbCompositeIn_8x8mmx,   0 },
    { PIXMAN_OP_IN,  PIXMAN_solid,     PIXMAN_a8,	PIXMAN_a8,	 fbCompositeIn_nx8x8mmx, 0 },
    { PIXMAN_OP_NONE },
};
#endif

#ifdef USE_SSE2
static const FastPathInfo sse_fast_paths[] =
{
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_r5g6b5,   fbCompositeSolidMask_nx8x0565sse2,     0 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_b5g6r5,   fbCompositeSolidMask_nx8x0565sse2,     0 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_null,     PIXMAN_a8r8g8b8, fbCompositeSolid_nx8888sse2,           0 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_null,     PIXMAN_x8r8g8b8, fbCompositeSolid_nx8888sse2,           0 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_null,     PIXMAN_r5g6b5,   fbCompositeSolid_nx0565sse2,           0 },
    { PIXMAN_OP_OVER, PIXMAN_a8r8g8b8, PIXMAN_null,     PIXMAN_a8r8g8b8, fbCompositeSrc_8888x8888sse2,          0 },
    { PIXMAN_OP_OVER, PIXMAN_a8r8g8b8, PIXMAN_null,     PIXMAN_x8r8g8b8, fbCompositeSrc_8888x8888sse2,          0 },
    { PIXMAN_OP_OVER, PIXMAN_a8b8g8r8, PIXMAN_null,     PIXMAN_a8b8g8r8, fbCompositeSrc_8888x8888sse2,          0 },
    { PIXMAN_OP_OVER, PIXMAN_a8b8g8r8, PIXMAN_null,     PIXMAN_x8b8g8r8, fbCompositeSrc_8888x8888sse2,          0 },
    { PIXMAN_OP_OVER, PIXMAN_a8r8g8b8, PIXMAN_null,     PIXMAN_r5g6b5,   fbCompositeSrc_8888x0565sse2,          0 },
    { PIXMAN_OP_OVER, PIXMAN_a8b8g8r8, PIXMAN_null,     PIXMAN_b5g6r5,   fbCompositeSrc_8888x0565sse2,          0 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_a8r8g8b8, fbCompositeSolidMask_nx8x8888sse2,     0 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_x8r8g8b8, fbCompositeSolidMask_nx8x8888sse2,     0 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_a8b8g8r8, fbCompositeSolidMask_nx8x8888sse2,     0 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_x8b8g8r8, fbCompositeSolidMask_nx8x8888sse2,     0 },
#if 0
    /* FIXME: This code are buggy in MMX version, now the bug was translated to SSE2 version */
    { PIXMAN_OP_OVER, PIXMAN_x8r8g8b8, PIXMAN_a8,       PIXMAN_x8r8g8b8, fbCompositeOver_x888x8x8888sse2,       0 },
    { PIXMAN_OP_OVER, PIXMAN_x8r8g8b8, PIXMAN_a8,       PIXMAN_a8r8g8b8, fbCompositeOver_x888x8x8888sse2,       0 },
    { PIXMAN_OP_OVER, PIXMAN_x8b8g8r8, PIXMAN_a8,       PIXMAN_x8b8g8r8, fbCompositeOver_x888x8x8888sse2,       0 },
    { PIXMAN_OP_OVER, PIXMAN_x8b8g8r8, PIXMAN_a8,       PIXMAN_a8r8g8b8, fbCompositeOver_x888x8x8888sse2,       0 },
#endif
    { PIXMAN_OP_OVER, PIXMAN_x8r8g8b8, PIXMAN_a8,       PIXMAN_a8r8g8b8, fbCompositeSrc_x888xnx8888sse2,        NEED_SOLID_MASK },
    { PIXMAN_OP_OVER, PIXMAN_x8r8g8b8, PIXMAN_a8,       PIXMAN_x8r8g8b8, fbCompositeSrc_x888xnx8888sse2,        NEED_SOLID_MASK },
    { PIXMAN_OP_OVER, PIXMAN_x8b8g8r8, PIXMAN_a8,       PIXMAN_a8b8g8r8, fbCompositeSrc_x888xnx8888sse2,        NEED_SOLID_MASK },
    { PIXMAN_OP_OVER, PIXMAN_x8b8g8r8, PIXMAN_a8,       PIXMAN_x8b8g8r8, fbCompositeSrc_x888xnx8888sse2,        NEED_SOLID_MASK },
    { PIXMAN_OP_OVER, PIXMAN_a8r8g8b8, PIXMAN_a8,       PIXMAN_a8r8g8b8, fbCompositeSrc_8888x8x8888sse2,        NEED_SOLID_MASK },
    { PIXMAN_OP_OVER, PIXMAN_a8r8g8b8, PIXMAN_a8,       PIXMAN_x8r8g8b8, fbCompositeSrc_8888x8x8888sse2,        NEED_SOLID_MASK },
    { PIXMAN_OP_OVER, PIXMAN_a8b8g8r8, PIXMAN_a8,       PIXMAN_a8b8g8r8, fbCompositeSrc_8888x8x8888sse2,        NEED_SOLID_MASK },
    { PIXMAN_OP_OVER, PIXMAN_a8b8g8r8, PIXMAN_a8,       PIXMAN_x8b8g8r8, fbCompositeSrc_8888x8x8888sse2,        NEED_SOLID_MASK },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8r8g8b8, PIXMAN_a8r8g8b8, fbCompositeSolidMask_nx8888x8888Csse2, NEED_COMPONENT_ALPHA },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8r8g8b8, PIXMAN_x8r8g8b8, fbCompositeSolidMask_nx8888x8888Csse2, NEED_COMPONENT_ALPHA },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8b8g8r8, PIXMAN_a8b8g8r8, fbCompositeSolidMask_nx8888x8888Csse2, NEED_COMPONENT_ALPHA },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8b8g8r8, PIXMAN_x8b8g8r8, fbCompositeSolidMask_nx8888x8888Csse2, NEED_COMPONENT_ALPHA },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8r8g8b8, PIXMAN_r5g6b5,   fbCompositeSolidMask_nx8888x0565Csse2, NEED_COMPONENT_ALPHA },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8b8g8r8, PIXMAN_b5g6r5,   fbCompositeSolidMask_nx8888x0565Csse2, NEED_COMPONENT_ALPHA },
    { PIXMAN_OP_OVER, PIXMAN_x8b8g8r8, PIXMAN_a8r8g8b8, PIXMAN_a8r8g8b8, fbCompositeSrc_8888RevNPx8888sse2,     NEED_PIXBUF },
    { PIXMAN_OP_OVER, PIXMAN_x8b8g8r8, PIXMAN_a8b8g8r8, PIXMAN_a8r8g8b8, fbCompositeSrc_8888RevNPx8888sse2,     NEED_PIXBUF },
    { PIXMAN_OP_OVER, PIXMAN_x8b8g8r8, PIXMAN_a8r8g8b8, PIXMAN_x8r8g8b8, fbCompositeSrc_8888RevNPx8888sse2,     NEED_PIXBUF },
    { PIXMAN_OP_OVER, PIXMAN_x8b8g8r8, PIXMAN_a8b8g8r8, PIXMAN_x8r8g8b8, fbCompositeSrc_8888RevNPx8888sse2,     NEED_PIXBUF },
    { PIXMAN_OP_OVER, PIXMAN_x8r8g8b8, PIXMAN_a8r8g8b8, PIXMAN_a8b8g8r8, fbCompositeSrc_8888RevNPx8888sse2,     NEED_PIXBUF },
    { PIXMAN_OP_OVER, PIXMAN_x8r8g8b8, PIXMAN_a8b8g8r8, PIXMAN_a8b8g8r8, fbCompositeSrc_8888RevNPx8888sse2,     NEED_PIXBUF },
    { PIXMAN_OP_OVER, PIXMAN_x8r8g8b8, PIXMAN_a8r8g8b8, PIXMAN_x8b8g8r8, fbCompositeSrc_8888RevNPx8888sse2,     NEED_PIXBUF },
    { PIXMAN_OP_OVER, PIXMAN_x8r8g8b8, PIXMAN_a8b8g8r8, PIXMAN_x8b8g8r8, fbCompositeSrc_8888RevNPx8888sse2,     NEED_PIXBUF },
    { PIXMAN_OP_OVER, PIXMAN_x8b8g8r8, PIXMAN_a8r8g8b8, PIXMAN_r5g6b5,   fbCompositeSrc_8888RevNPx0565sse2,     NEED_PIXBUF },
    { PIXMAN_OP_OVER, PIXMAN_x8b8g8r8, PIXMAN_a8b8g8r8, PIXMAN_r5g6b5,   fbCompositeSrc_8888RevNPx0565sse2,     NEED_PIXBUF },
    { PIXMAN_OP_OVER, PIXMAN_x8r8g8b8, PIXMAN_a8r8g8b8, PIXMAN_b5g6r5,   fbCompositeSrc_8888RevNPx0565sse2,     NEED_PIXBUF },
    { PIXMAN_OP_OVER, PIXMAN_x8r8g8b8, PIXMAN_a8b8g8r8, PIXMAN_b5g6r5,   fbCompositeSrc_8888RevNPx0565sse2,     NEED_PIXBUF },
#if 0
    /* FIXME: This code is commented out since it's apparently not actually faster than the generic code */
    { PIXMAN_OP_OVER, PIXMAN_x8r8g8b8, PIXMAN_null,     PIXMAN_x8r8g8b8, fbCompositeCopyAreasse2,               0 },
    { PIXMAN_OP_OVER, PIXMAN_x8b8g8r8, PIXMAN_null,     PIXMAN_x8b8g8r8, fbCompositeCopyAreasse2,               0 },
#endif

    { PIXMAN_OP_ADD,  PIXMAN_a8,       PIXMAN_null,     PIXMAN_a8,       fbCompositeSrcAdd_8000x8000sse2,       0 },
    { PIXMAN_OP_ADD,  PIXMAN_a8r8g8b8, PIXMAN_null,     PIXMAN_a8r8g8b8, fbCompositeSrcAdd_8888x8888sse2,       0 },
    { PIXMAN_OP_ADD,  PIXMAN_a8b8g8r8, PIXMAN_null,     PIXMAN_a8b8g8r8, fbCompositeSrcAdd_8888x8888sse2,       0 },
    { PIXMAN_OP_ADD,  PIXMAN_solid,    PIXMAN_a8,       PIXMAN_a8,       fbCompositeSrcAdd_8888x8x8sse2,        0 },

    { PIXMAN_OP_SRC, PIXMAN_solid,     PIXMAN_a8,       PIXMAN_a8r8g8b8, fbCompositeSolidMaskSrc_nx8x8888sse2,  0 },
    { PIXMAN_OP_SRC, PIXMAN_solid,     PIXMAN_a8,       PIXMAN_x8r8g8b8, fbCompositeSolidMaskSrc_nx8x8888sse2,  0 },
    { PIXMAN_OP_SRC, PIXMAN_solid,     PIXMAN_a8,       PIXMAN_a8b8g8r8, fbCompositeSolidMaskSrc_nx8x8888sse2,  0 },
    { PIXMAN_OP_SRC, PIXMAN_solid,     PIXMAN_a8,       PIXMAN_x8b8g8r8, fbCompositeSolidMaskSrc_nx8x8888sse2,  0 },

#if 0
    /* FIXME: This code is commented out since it's apparently not actually faster than the generic code */
    { PIXMAN_OP_SRC, PIXMAN_a8r8g8b8,  PIXMAN_null,     PIXMAN_a8r8g8b8, fbCompositeCopyAreasse2,               0 },
    { PIXMAN_OP_SRC, PIXMAN_a8b8g8r8,  PIXMAN_null,     PIXMAN_a8b8g8r8, fbCompositeCopyAreasse2,               0 },
    { PIXMAN_OP_SRC, PIXMAN_x8r8g8b8,  PIXMAN_null,     PIXMAN_x8r8g8b8, fbCompositeCopyAreasse2,               0 },
    { PIXMAN_OP_SRC, PIXMAN_x8b8g8r8,  PIXMAN_null,     PIXMAN_x8b8g8r8, fbCompositeCopyAreasse2,               0 },
    { PIXMAN_OP_SRC, PIXMAN_r5g6b5,    PIXMAN_null,     PIXMAN_r5g6b5,   fbCompositeCopyAreasse2,               0 },
    { PIXMAN_OP_SRC, PIXMAN_b5g6r5,    PIXMAN_null,     PIXMAN_b5g6r5,   fbCompositeCopyAreasse2,               0 },
#endif

    { PIXMAN_OP_IN,  PIXMAN_a8,        PIXMAN_null,     PIXMAN_a8,       fbCompositeIn_8x8sse2,                 0 },
    { PIXMAN_OP_IN,  PIXMAN_solid,     PIXMAN_a8,       PIXMAN_a8,       fbCompositeIn_nx8x8sse2,               0 },

    { PIXMAN_OP_NONE },
};
#endif

#ifdef USE_VMX
static const FastPathInfo vmx_fast_paths[] =
{
    { PIXMAN_OP_NONE },
};
#endif


static const FastPathInfo c_fast_paths[] =
{
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_r5g6b5,   fbCompositeSolidMask_nx8x0565, 0 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_b5g6r5,   fbCompositeSolidMask_nx8x0565, 0 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_r8g8b8,   fbCompositeSolidMask_nx8x0888, 0 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_b8g8r8,   fbCompositeSolidMask_nx8x0888, 0 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_a8r8g8b8, fbCompositeSolidMask_nx8x8888, 0 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_x8r8g8b8, fbCompositeSolidMask_nx8x8888, 0 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_a8b8g8r8, fbCompositeSolidMask_nx8x8888, 0 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_x8b8g8r8, fbCompositeSolidMask_nx8x8888, 0 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8r8g8b8, PIXMAN_a8r8g8b8, fbCompositeSolidMask_nx8888x8888C, NEED_COMPONENT_ALPHA },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8r8g8b8, PIXMAN_x8r8g8b8, fbCompositeSolidMask_nx8888x8888C, NEED_COMPONENT_ALPHA },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8r8g8b8, PIXMAN_r5g6b5,   fbCompositeSolidMask_nx8888x0565C, NEED_COMPONENT_ALPHA },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8b8g8r8, PIXMAN_a8b8g8r8, fbCompositeSolidMask_nx8888x8888C, NEED_COMPONENT_ALPHA },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8b8g8r8, PIXMAN_x8b8g8r8, fbCompositeSolidMask_nx8888x8888C, NEED_COMPONENT_ALPHA },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8b8g8r8, PIXMAN_b5g6r5,   fbCompositeSolidMask_nx8888x0565C, NEED_COMPONENT_ALPHA },
#if 0
    /* FIXME: This code is commented out since it's apparently not actually faster than the generic code */
    { PIXMAN_OP_OVER, PIXMAN_x8r8g8b8, PIXMAN_a8,	PIXMAN_x8r8g8b8, fbCompositeOver_x888x8x8888,       0 },
    { PIXMAN_OP_OVER, PIXMAN_x8r8g8b8, PIXMAN_a8,	PIXMAN_a8r8g8b8, fbCompositeOver_x888x8x8888,       0 },
    { PIXMAN_OP_OVER, PIXMAN_x8b8r8g8, PIXMAN_a8,	PIXMAN_x8b8g8r8, fbCompositeOver_x888x8x8888,       0 },
    { PIXMAN_OP_OVER, PIXMAN_x8b8r8g8, PIXMAN_a8,	PIXMAN_a8r8g8b8, fbCompositeOver_x888x8x8888,       0 },
#endif
    { PIXMAN_OP_OVER, PIXMAN_a8r8g8b8, PIXMAN_null,     PIXMAN_a8r8g8b8, fbCompositeSrc_8888x8888,	   0 },
    { PIXMAN_OP_OVER, PIXMAN_a8r8g8b8, PIXMAN_null,	PIXMAN_x8r8g8b8, fbCompositeSrc_8888x8888,	   0 },
    { PIXMAN_OP_OVER, PIXMAN_a8r8g8b8, PIXMAN_null,	PIXMAN_r5g6b5,	 fbCompositeSrc_8888x0565,	   0 },
    { PIXMAN_OP_OVER, PIXMAN_a8b8g8r8, PIXMAN_null,	PIXMAN_a8b8g8r8, fbCompositeSrc_8888x8888,	   0 },
    { PIXMAN_OP_OVER, PIXMAN_a8b8g8r8, PIXMAN_null,	PIXMAN_x8b8g8r8, fbCompositeSrc_8888x8888,	   0 },
    { PIXMAN_OP_OVER, PIXMAN_a8b8g8r8, PIXMAN_null,     PIXMAN_b5g6r5,   fbCompositeSrc_8888x0565,	   0 },
#if 0
    /* FIXME */
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a1,	PIXMAN_r5g6b5,   fbCompositeSolidMask_nx1xn,	   0 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a1,	PIXMAN_b5g6r5,   fbCompositeSolidMask_nx1xn,	   0 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a1,	PIXMAN_r8g8b8,   fbCompositeSolidMask_nx1xn,	   0 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a1,	PIXMAN_b8g8r8,   fbCompositeSolidMask_nx1xn,	   0 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a1,	PIXMAN_a8r8g8b8, fbCompositeSolidMask_nx1xn,	   0 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a1,	PIXMAN_a8b8g8r8, fbCompositeSolidMask_nx1xn,	   0 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a1,	PIXMAN_x8r8g8b8, fbCompositeSolidMask_nx1xn,	   0 },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a1,	PIXMAN_x8b8g8r8, fbCompositeSolidMask_nx1xn,	   0 },
#endif
    { PIXMAN_OP_ADD, PIXMAN_a8r8g8b8,  PIXMAN_null,	PIXMAN_a8r8g8b8, fbCompositeSrcAdd_8888x8888,   0 },
    { PIXMAN_OP_ADD, PIXMAN_a8b8g8r8,  PIXMAN_null,	PIXMAN_a8b8g8r8, fbCompositeSrcAdd_8888x8888,   0 },
    { PIXMAN_OP_ADD, PIXMAN_a8,        PIXMAN_null,     PIXMAN_a8,       fbCompositeSrcAdd_8000x8000,   0 },
#if 0
    /* FIXME */
    { PIXMAN_OP_ADD, PIXMAN_a1,        PIXMAN_null,     PIXMAN_a1,       fbCompositeSrcAdd_1000x1000,   0 },
#endif
    { PIXMAN_OP_ADD, PIXMAN_solid,     PIXMAN_a8,       PIXMAN_a8,       fbCompositeSrcAdd_8888x8x8,    0 },
    { PIXMAN_OP_SRC, PIXMAN_solid,     PIXMAN_null,     PIXMAN_a8r8g8b8, fbCompositeSolidFill, 0 },
    { PIXMAN_OP_SRC, PIXMAN_solid,     PIXMAN_null,     PIXMAN_x8r8g8b8, fbCompositeSolidFill, 0 },
    { PIXMAN_OP_SRC, PIXMAN_solid,     PIXMAN_null,     PIXMAN_a8b8g8r8, fbCompositeSolidFill, 0 },
    { PIXMAN_OP_SRC, PIXMAN_solid,     PIXMAN_null,     PIXMAN_x8b8g8r8, fbCompositeSolidFill, 0 },
    { PIXMAN_OP_SRC, PIXMAN_solid,     PIXMAN_null,     PIXMAN_a8,       fbCompositeSolidFill, 0 },
    { PIXMAN_OP_SRC, PIXMAN_solid,     PIXMAN_null,     PIXMAN_r5g6b5,   fbCompositeSolidFill, 0 },
    { PIXMAN_OP_SRC, PIXMAN_a8r8g8b8,  PIXMAN_null,     PIXMAN_x8r8g8b8, fbCompositeSrc_8888xx888, 0 },
    { PIXMAN_OP_SRC, PIXMAN_x8r8g8b8,  PIXMAN_null,     PIXMAN_x8r8g8b8, fbCompositeSrc_8888xx888, 0 },
    { PIXMAN_OP_SRC, PIXMAN_a8b8g8r8,  PIXMAN_null,     PIXMAN_x8b8g8r8, fbCompositeSrc_8888xx888, 0 },
    { PIXMAN_OP_SRC, PIXMAN_x8b8g8r8,  PIXMAN_null,     PIXMAN_x8b8g8r8, fbCompositeSrc_8888xx888, 0 },
#if 0
    /* FIXME */
    { PIXMAN_OP_SRC, PIXMAN_a8r8g8b8,  PIXMAN_null,	PIXMAN_a8r8g8b8, fbCompositeSrcSrc_nxn, 0 },
    { PIXMAN_OP_SRC, PIXMAN_a8b8g8r8,  PIXMAN_null,	PIXMAN_a8b8g8r8, fbCompositeSrcSrc_nxn, 0 },
    { PIXMAN_OP_SRC, PIXMAN_x8r8g8b8,  PIXMAN_null,	PIXMAN_x8r8g8b8, fbCompositeSrcSrc_nxn, 0 },
    { PIXMAN_OP_SRC, PIXMAN_x8b8g8r8,  PIXMAN_null,	PIXMAN_x8b8g8r8, fbCompositeSrcSrc_nxn, 0 },
    { PIXMAN_OP_SRC, PIXMAN_r5g6b5,    PIXMAN_null,     PIXMAN_r5g6b5,   fbCompositeSrcSrc_nxn, 0 },
    { PIXMAN_OP_SRC, PIXMAN_b5g6r5,    PIXMAN_null,     PIXMAN_b5g6r5,   fbCompositeSrcSrc_nxn, 0 },
#endif
    { PIXMAN_OP_IN,  PIXMAN_a8,        PIXMAN_null,     PIXMAN_a8,       fbCompositeSrcIn_8x8,   0 },
    { PIXMAN_OP_IN,  PIXMAN_solid,     PIXMAN_a8,	PIXMAN_a8,	 fbCompositeSolidMaskIn_nx8x8, 0 },
    { PIXMAN_OP_NONE },
};

static pixman_bool_t
mask_is_solid (pixman_image_t *mask)
{
    if (mask->type == SOLID)
	return TRUE;

    if (mask->type == BITS &&
	mask->common.repeat == PIXMAN_REPEAT_NORMAL &&
	mask->bits.width == 1 &&
	mask->bits.height == 1)
    {
	return TRUE;
    }

    return FALSE;
}

static const FastPathInfo *
get_fast_path (const FastPathInfo *fast_paths,
	       pixman_op_t         op,
	       pixman_image_t     *pSrc,
	       pixman_image_t     *pMask,
	       pixman_image_t     *pDst,
	       pixman_bool_t       is_pixbuf)
{
    const FastPathInfo *info;

    for (info = fast_paths; info->op != PIXMAN_OP_NONE; info++)
    {
	pixman_bool_t valid_src		= FALSE;
	pixman_bool_t valid_mask	= FALSE;

	if (info->op != op)
	    continue;

	if ((info->src_format == PIXMAN_solid && pixman_image_can_get_solid (pSrc))		||
	    (pSrc->type == BITS && info->src_format == pSrc->bits.format))
	{
	    valid_src = TRUE;
	}

	if (!valid_src)
	    continue;

	if ((info->mask_format == PIXMAN_null && !pMask)			||
	    (pMask && pMask->type == BITS && info->mask_format == pMask->bits.format))
	{
	    valid_mask = TRUE;

	    if (info->flags & NEED_SOLID_MASK)
	    {
		if (!pMask || !mask_is_solid (pMask))
		    valid_mask = FALSE;
	    }

	    if (info->flags & NEED_COMPONENT_ALPHA)
	    {
		if (!pMask || !pMask->common.component_alpha)
		    valid_mask = FALSE;
	    }
	}

	if (!valid_mask)
	    continue;
	
	if (info->dest_format != pDst->bits.format)
	    continue;

	if ((info->flags & NEED_PIXBUF) && !is_pixbuf)
	    continue;

	return info;
    }

    return NULL;
}

/*
 * Operator optimizations based on source or destination opacity
 */
typedef struct
{
    pixman_op_t			op;
    pixman_op_t			opSrcDstOpaque;
    pixman_op_t			opSrcOpaque;
    pixman_op_t			opDstOpaque;
} OptimizedOperatorInfo;

static const OptimizedOperatorInfo optimized_operators[] =
{
    /* Input Operator           SRC&DST Opaque          SRC Opaque              DST Opaque      */
    { PIXMAN_OP_OVER,           PIXMAN_OP_SRC,          PIXMAN_OP_SRC,          PIXMAN_OP_OVER },
    { PIXMAN_OP_OVER_REVERSE,   PIXMAN_OP_DST,          PIXMAN_OP_OVER_REVERSE, PIXMAN_OP_DST },
    { PIXMAN_OP_IN,             PIXMAN_OP_SRC,          PIXMAN_OP_IN,           PIXMAN_OP_SRC },
    { PIXMAN_OP_IN_REVERSE,     PIXMAN_OP_DST,          PIXMAN_OP_DST,          PIXMAN_OP_IN_REVERSE },
    { PIXMAN_OP_OUT,            PIXMAN_OP_CLEAR,        PIXMAN_OP_OUT,          PIXMAN_OP_CLEAR },
    { PIXMAN_OP_OUT_REVERSE,    PIXMAN_OP_CLEAR,        PIXMAN_OP_CLEAR,        PIXMAN_OP_OUT_REVERSE },
    { PIXMAN_OP_ATOP,           PIXMAN_OP_SRC,          PIXMAN_OP_IN,           PIXMAN_OP_OVER },
    { PIXMAN_OP_ATOP_REVERSE,   PIXMAN_OP_DST,          PIXMAN_OP_OVER_REVERSE, PIXMAN_OP_IN_REVERSE },
    { PIXMAN_OP_XOR,            PIXMAN_OP_CLEAR,        PIXMAN_OP_OUT,          PIXMAN_OP_OUT_REVERSE },
    { PIXMAN_OP_SATURATE,       PIXMAN_OP_DST,          PIXMAN_OP_OVER_REVERSE, PIXMAN_OP_DST },
    { PIXMAN_OP_NONE }
};

/*
 * Check if the current operator could be optimized
 */
static const OptimizedOperatorInfo*
pixman_operator_can_be_optimized(pixman_op_t op)
{
    const OptimizedOperatorInfo *info;

    for (info = optimized_operators; info->op != PIXMAN_OP_NONE; info++)
    {
        if(info->op == op)
            return info;
    }
    return NULL;
}

/*
 * Optimize the current operator based on opacity of source or destination
 * The output operator should be mathematically equivalent to the source.
 */
static pixman_op_t
pixman_optimize_operator(pixman_op_t op, pixman_image_t *pSrc, pixman_image_t *pMask, pixman_image_t *pDst )
{
    pixman_bool_t is_source_opaque;
    pixman_bool_t is_dest_opaque;
    const OptimizedOperatorInfo *info = pixman_operator_can_be_optimized(op);

    if(!info || pMask)
        return op;

    is_source_opaque = pixman_image_is_opaque(pSrc);
    is_dest_opaque = pixman_image_is_opaque(pDst);

    if(is_source_opaque == FALSE && is_dest_opaque == FALSE)
        return op;

    if(is_source_opaque && is_dest_opaque)
        return info->opSrcDstOpaque;
    else if(is_source_opaque)
        return info->opSrcOpaque;
    else if(is_dest_opaque)
        return info->opDstOpaque;

    return op;

}

#if defined(USE_SSE2) && defined (__GNUC__)

/*
 * Work around GCC bug causing crashes in Mozilla with SSE2
 * 
 * When using SSE2 intrinsics, gcc assumes that the stack is 16 byte
 * aligned. Unfortunately some code, such as Mozilla and Mono contain
 * code that aligns the stack to 4 bytes.
 *
 * The __force_align_arg_pointer__ makes gcc generate a prologue that
 * realigns the stack pointer to 16 bytes.
 *
 * See https://bugs.freedesktop.org/show_bug.cgi?id=15693
 */

__attribute__((__force_align_arg_pointer__))
#endif
PIXMAN_EXPORT void
pixman_image_composite (pixman_op_t      op,
			pixman_image_t * pSrc,
			pixman_image_t * pMask,
			pixman_image_t * pDst,
			int16_t      xSrc,
			int16_t      ySrc,
			int16_t      xMask,
			int16_t      yMask,
			int16_t      xDst,
			int16_t      yDst,
			uint16_t     width,
			uint16_t     height)
{
    pixman_bool_t srcRepeat = pSrc->type == BITS && pSrc->common.repeat == PIXMAN_REPEAT_NORMAL;
    pixman_bool_t maskRepeat = FALSE;
    pixman_bool_t srcTransform = pSrc->common.transform != NULL;
    pixman_bool_t maskTransform = FALSE;
    pixman_bool_t srcAlphaMap = pSrc->common.alpha_map != NULL;
    pixman_bool_t maskAlphaMap = FALSE;
    pixman_bool_t dstAlphaMap = pDst->common.alpha_map != NULL;
    CompositeFunc func = NULL;

#ifdef USE_MMX
    fbComposeSetupMMX();
#endif

#ifdef USE_VMX
    fbComposeSetupVMX();
#endif

#ifdef USE_SSE2
    fbComposeSetupSSE();
#endif

    if (srcRepeat && srcTransform &&
	pSrc->bits.width == 1 &&
	pSrc->bits.height == 1)
    {
	srcTransform = FALSE;
    }

    if (pMask && pMask->type == BITS)
    {
	maskRepeat = pMask->common.repeat == PIXMAN_REPEAT_NORMAL;

	maskTransform = pMask->common.transform != 0;
	if (pMask->common.filter == PIXMAN_FILTER_CONVOLUTION)
	    maskTransform = TRUE;

	maskAlphaMap = pMask->common.alpha_map != 0;

	if (maskRepeat && maskTransform &&
	    pMask->bits.width == 1 &&
	    pMask->bits.height == 1)
	{
	    maskTransform = FALSE;
	}
    }

    /*
    * Check if we can replace our operator by a simpler one if the src or dest are opaque
    * The output operator should be mathematically equivalent to the source.
    */
    op = pixman_optimize_operator(op, pSrc, pMask, pDst);
    if(op == PIXMAN_OP_DST)
        return;

    if ((pSrc->type == BITS || pixman_image_can_get_solid (pSrc)) && (!pMask || pMask->type == BITS)
        && !srcTransform && !maskTransform
        && !maskAlphaMap && !srcAlphaMap && !dstAlphaMap
        && (pSrc->common.filter != PIXMAN_FILTER_CONVOLUTION)
        && (pSrc->common.repeat != PIXMAN_REPEAT_PAD)
        && (!pMask || (pMask->common.filter != PIXMAN_FILTER_CONVOLUTION && pMask->common.repeat != PIXMAN_REPEAT_PAD))
	&& !pSrc->common.read_func && !pSrc->common.write_func
	&& !(pMask && pMask->common.read_func) && !(pMask && pMask->common.write_func)
	&& !pDst->common.read_func && !pDst->common.write_func)
    {
	const FastPathInfo *info;
	pixman_bool_t pixbuf;

	pixbuf =
	    pSrc && pSrc->type == BITS		&&
	    pMask && pMask->type == BITS	&&
	    pSrc->bits.bits == pMask->bits.bits &&
	    xSrc == xMask			&&
	    ySrc == yMask			&&
	    !pMask->common.component_alpha	&&
	    !maskRepeat;
	info = NULL;
	
#ifdef USE_SSE2
	if (pixman_have_sse ())
	    info = get_fast_path (sse_fast_paths, op, pSrc, pMask, pDst, pixbuf);
#endif

#ifdef USE_MMX
	if (!info && pixman_have_mmx())
	    info = get_fast_path (mmx_fast_paths, op, pSrc, pMask, pDst, pixbuf);
#endif

#ifdef USE_VMX

	if (!info && pixman_have_vmx())
	    info = get_fast_path (vmx_fast_paths, op, pSrc, pMask, pDst, pixbuf);
#endif
        if (!info)
	    info = get_fast_path (c_fast_paths, op, pSrc, pMask, pDst, pixbuf);

	if (info)
	{
	    func = info->func;

	    if (info->src_format == PIXMAN_solid)
		srcRepeat = FALSE;

	    if (info->mask_format == PIXMAN_solid	||
		info->flags & NEED_SOLID_MASK)
	    {
		maskRepeat = FALSE;
	    }
	}
    }
    
    if ((srcRepeat			&&
	 pSrc->bits.width == 1		&&
	 pSrc->bits.height == 1)	||
	(maskRepeat			&&
	 pMask->bits.width == 1		&&
	 pMask->bits.height == 1))
    {
	/* If src or mask are repeating 1x1 images and srcRepeat or
	 * maskRepeat are still TRUE, it means the fast path we
	 * selected does not actually handle repeating images.
	 *
	 * So rather than call the "fast path" with a zillion
	 * 1x1 requests, we just use the general code (which does
	 * do something sensible with 1x1 repeating images).
	 */
	func = NULL;
    }

    if (!func)
    {
	func = pixman_image_composite_rect;

	/* CompositeGeneral optimizes 1x1 repeating images itself */
	if (pSrc->type == BITS &&
	    pSrc->bits.width == 1 && pSrc->bits.height == 1)
	{
	    srcRepeat = FALSE;
	}

	if (pMask && pMask->type == BITS &&
	    pMask->bits.width == 1 && pMask->bits.height == 1)
	{
	    maskRepeat = FALSE;
	}

	/* if we are transforming, repeats are handled in fbFetchTransformed */
	if (srcTransform)
	    srcRepeat = FALSE;

	if (maskTransform)
	    maskRepeat = FALSE;
    }

    pixman_walk_composite_region (op, pSrc, pMask, pDst, xSrc, ySrc,
				  xMask, yMask, xDst, yDst, width, height,
				  srcRepeat, maskRepeat, func);
}


#ifdef USE_VMX
/* The CPU detection code needs to be in a file not compiled with
 * "-maltivec -mabi=altivec", as gcc would try to save vector register
 * across function calls causing SIGILL on cpus without Altivec/vmx.
 */
static pixman_bool_t initialized = FALSE;
static volatile pixman_bool_t have_vmx = TRUE;

#ifdef __APPLE__
#include <sys/sysctl.h>

pixman_bool_t pixman_have_vmx (void) {
    if(!initialized) {
        size_t length = sizeof(have_vmx);
        int error =
            sysctlbyname("hw.optional.altivec", &have_vmx, &length, NULL, 0);
        if(error) have_vmx = FALSE;
        initialized = TRUE;
    }
    return have_vmx;
}

#else
#include <signal.h>
#include <setjmp.h>

static jmp_buf jump_env;

static void vmx_test(int sig, siginfo_t *si, void *unused) {
    longjmp (jump_env, 1);
}

pixman_bool_t pixman_have_vmx (void) {
    struct sigaction sa, osa;
    int jmp_result;
    if (!initialized) {
        sa.sa_flags = SA_SIGINFO;
        sigemptyset(&sa.sa_mask);
        sa.sa_sigaction = vmx_test;
        sigaction(SIGILL, &sa, &osa);
	jmp_result = setjmp (jump_env);
	if (jmp_result == 0) {
	    asm volatile ( "vor 0, 0, 0" );
	}
        sigaction(SIGILL, &osa, NULL);
	have_vmx = (jmp_result == 0);
        initialized = TRUE;
    }
    return have_vmx;
}
#endif /* __APPLE__ */
#endif /* USE_VMX */

#ifdef USE_MMX
/* The CPU detection code needs to be in a file not compiled with
 * "-mmmx -msse", as gcc would generate CMOV instructions otherwise
 * that would lead to SIGILL instructions on old CPUs that don't have
 * it.
 */
#if !defined(__amd64__) && !defined(__x86_64__)

#ifdef HAVE_GETISAX
#include <sys/auxv.h>
#endif

enum CPUFeatures {
    NoFeatures = 0,
    MMX = 0x1,
    MMX_Extensions = 0x2,
    SSE = 0x6,
    SSE2 = 0x8,
    CMOV = 0x10
};

static unsigned int detectCPUFeatures(void) {
    unsigned int features = 0;
    unsigned int result = 0;

#ifdef HAVE_GETISAX
    if (getisax(&result, 1)) {
        if (result & AV_386_CMOV)
            features |= CMOV;
        if (result & AV_386_MMX)
            features |= MMX;
        if (result & AV_386_AMD_MMX)
            features |= MMX_Extensions;
        if (result & AV_386_SSE)
            features |= SSE;
        if (result & AV_386_SSE2)
            features |= SSE2;
    }
#else
    char vendor[13];
#ifdef _MSC_VER
    int vendor0 = 0, vendor1, vendor2;
#endif
    vendor[0] = 0;
    vendor[12] = 0;

#ifdef __GNUC__
    /* see p. 118 of amd64 instruction set manual Vol3 */
    /* We need to be careful about the handling of %ebx and
     * %esp here. We can't declare either one as clobbered
     * since they are special registers (%ebx is the "PIC
     * register" holding an offset to global data, %esp the
     * stack pointer), so we need to make sure they have their
     * original values when we access the output operands.
     */
    __asm__ ("pushf\n"
             "pop %%eax\n"
             "mov %%eax, %%ecx\n"
             "xor $0x00200000, %%eax\n"
             "push %%eax\n"
             "popf\n"
             "pushf\n"
             "pop %%eax\n"
             "mov $0x0, %%edx\n"
             "xor %%ecx, %%eax\n"
             "jz 1f\n"

             "mov $0x00000000, %%eax\n"
	     "push %%ebx\n"
             "cpuid\n"
             "mov %%ebx, %%eax\n"
	     "pop %%ebx\n"
	     "mov %%eax, %1\n"
             "mov %%edx, %2\n"
             "mov %%ecx, %3\n"
             "mov $0x00000001, %%eax\n"
	     "push %%ebx\n"
             "cpuid\n"
	     "pop %%ebx\n"
             "1:\n"
             "mov %%edx, %0\n"
             : "=r" (result),
               "=m" (vendor[0]),
               "=m" (vendor[4]),
               "=m" (vendor[8])
             :
             : "%eax", "%ecx", "%edx"
        );

#elif defined (_MSC_VER)

    _asm {
      pushfd
      pop eax
      mov ecx, eax
      xor eax, 00200000h
      push eax
      popfd
      pushfd
      pop eax
      mov edx, 0
      xor eax, ecx
      jz nocpuid

      mov eax, 0
      push ebx
      cpuid
      mov eax, ebx
      pop ebx
      mov vendor0, eax
      mov vendor1, edx
      mov vendor2, ecx
      mov eax, 1
      push ebx
      cpuid
      pop ebx
    nocpuid:
      mov result, edx
    }
    memmove (vendor+0, &vendor0, 4);
    memmove (vendor+4, &vendor1, 4);
    memmove (vendor+8, &vendor2, 4);

#else
#   error unsupported compiler
#endif

    features = 0;
    if (result) {
        /* result now contains the standard feature bits */
        if (result & (1 << 15))
            features |= CMOV;
        if (result & (1 << 23))
            features |= MMX;
        if (result & (1 << 25))
            features |= SSE;
        if (result & (1 << 26))
            features |= SSE2;
        if ((features & MMX) && !(features & SSE) &&
            (strcmp(vendor, "AuthenticAMD") == 0 ||
             strcmp(vendor, "Geode by NSC") == 0)) {
            /* check for AMD MMX extensions */
#ifdef __GNUC__
            __asm__("push %%ebx\n"
                    "mov $0x80000000, %%eax\n"
                    "cpuid\n"
                    "xor %%edx, %%edx\n"
                    "cmp $0x1, %%eax\n"
                    "jge 2f\n"
                    "mov $0x80000001, %%eax\n"
                    "cpuid\n"
                    "2:\n"
                    "pop %%ebx\n"
                    "mov %%edx, %0\n"
                    : "=r" (result)
                    :
                    : "%eax", "%ecx", "%edx"
                );
#elif defined _MSC_VER
            _asm {
              push ebx
              mov eax, 80000000h
              cpuid
              xor edx, edx
              cmp eax, 1
              jge notamd
              mov eax, 80000001h
              cpuid
            notamd:
              pop ebx
              mov result, edx
            }
#endif
            if (result & (1<<22))
                features |= MMX_Extensions;
        }
    }
#endif /* HAVE_GETISAX */

    return features;
}

pixman_bool_t
pixman_have_mmx (void)
{
    static pixman_bool_t initialized = FALSE;
    static pixman_bool_t mmx_present;

    if (!initialized)
    {
        unsigned int features = detectCPUFeatures();
	mmx_present = (features & (MMX|MMX_Extensions)) == (MMX|MMX_Extensions);
        initialized = TRUE;
    }

    return mmx_present;
}

#ifdef USE_SSE2
pixman_bool_t
pixman_have_sse (void)
{
    static pixman_bool_t initialized = FALSE;
    static pixman_bool_t sse_present;

    if (!initialized)
    {
        unsigned int features = detectCPUFeatures();
        sse_present = (features & (MMX|MMX_Extensions|SSE|SSE2)) == (MMX|MMX_Extensions|SSE|SSE2);
        initialized = TRUE;
    }

    return sse_present;
}
#endif

#endif /* __amd64__ */
#endif
