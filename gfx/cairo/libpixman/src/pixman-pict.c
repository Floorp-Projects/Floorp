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
#include "pixman.h"
#include "pixman-private.h"
#include "pixman-mmx.h"

#define FbFullMask(n)   ((n) == 32 ? (uint32_t)-1 : ((((uint32_t) 1) << n) - 1))

#undef READ
#undef WRITE
#define READ(img,x) (*(x))
#define WRITE(img,ptr,v) ((*(ptr)) = (v))

typedef void (* CompositeFunc) (pixman_op_t,
				pixman_image_t *, pixman_image_t *, pixman_image_t *,
				int16_t, int16_t, int16_t, int16_t, int16_t, int16_t,
				uint16_t, uint16_t);

uint32_t
fbOver (uint32_t x, uint32_t y)
{
    uint16_t  a = ~x >> 24;
    uint16_t  t;
    uint32_t  m,n,o,p;

    m = FbOverU(x,y,0,a,t);
    n = FbOverU(x,y,8,a,t);
    o = FbOverU(x,y,16,a,t);
    p = FbOverU(x,y,24,a,t);
    return m|n|o|p;
}

uint32_t
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

uint32_t
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

    fbFinishAccess (pMask->pDrawable);
    fbFinishAccess (pDst->pDrawable);
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

    fbFinishAccess (pMask->pDrawable);
    fbFinishAccess (pDst->pDrawable);
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

    fbFinishAccess (pMask->pDrawable);
    fbFinishAccess (pDst->pDrawable);
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

    fbFinishAccess (pMask->pDrawable);
    fbFinishAccess (pDst->pDrawable);
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

    fbFinishAccess (pMask->pDrawable);
    fbFinishAccess (pDst->pDrawable);
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

    fbFinishAccess (pMask->pDrawable);
    fbFinishAccess (pDst->pDrawable);
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

    fbFinishAccess (pSrc->pDrawable);
    fbFinishAccess (pDst->pDrawable);
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

    fbFinishAccess (pSrc->pDrawable);
    fbFinishAccess (pDst->pDrawable);
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

    fbFinishAccess (pDst->pDrawable);
    fbFinishAccess (pSrc->pDrawable);
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

    fbFinishAccess (pDst->pDrawable);
    fbFinishAccess (pSrc->pDrawable);
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

    fbFinishAccess (pDst->pDrawable);
    fbFinishAccess (pSrc->pDrawable);
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

    fbFinishAccess(pDst->pDrawable);
    fbFinishAccess(pMask->pDrawable);
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

    fbFinishAccess(pDst->pDrawable);
    fbFinishAccess(pSrc->pDrawable);
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

    fbFinishAccess (pDst->pDrawable);
    fbFinishAccess (pMask->pDrawable);
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

    fbFinishAccess(pSrc->pDrawable);
    fbFinishAccess(pDst->pDrawable);
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
void
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

    fbFinishAccess(pSrc->pDrawable);
    fbFinishAccess(pDst->pDrawable);
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
    const pixman_box16_t *pbox;
    int		    w, h, w_this, h_this;
    int		    x_msk, y_msk, x_src, y_src, x_dst, y_dst;
    pixman_region16_t reg;
    pixman_region16_t *region;

    pixman_region_init (&reg);
    if (!pixman_compute_composite_region (&reg, pSrc, pMask, pDst,
					  xSrc, ySrc, xMask, yMask, xDst, yDst, width, height))
    {
	return;
    }

    region = &reg;

    pbox = pixman_region_rectangles (region, &n);
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
    pixman_region_fini (&reg);
}

static pixman_bool_t
can_get_solid (pixman_image_t *image)
{
    if (image->type == SOLID)
	return TRUE;

    if (image->type != BITS	||
	image->bits.width != 1	||
	image->bits.height != 1)
    {
	return FALSE;
    }

    if (image->common.repeat != PIXMAN_REPEAT_NORMAL)
	return FALSE;

    switch (image->bits.format)
    {
    case PIXMAN_a8r8g8b8:
    case PIXMAN_x8r8g8b8:
    case PIXMAN_a8b8g8r8:
    case PIXMAN_x8b8g8r8:
    case PIXMAN_r8g8b8:
    case PIXMAN_b8g8r8:
    case PIXMAN_r5g6b5:
    case PIXMAN_b5g6r5:
	return TRUE;
    default:
	return FALSE;
    }
}

#define SCANLINE_BUFFER_LENGTH 2048

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
    uint32_t _scanline_buffer[SCANLINE_BUFFER_LENGTH * 3];
    uint32_t *scanline_buffer = _scanline_buffer;

    return_if_fail (src != NULL);
    return_if_fail (dest != NULL);

    if (width > SCANLINE_BUFFER_LENGTH)
    {
	scanline_buffer = (uint32_t *)pixman_malloc_abc (width, 3, sizeof (uint32_t));

	if (!scanline_buffer)
	    return;
    }

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

    pixman_composite_rect_general (&compose_data, scanline_buffer);

    if (scanline_buffer != _scanline_buffer)
	free (scanline_buffer);
}


void
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
    pixman_bool_t	    srcRepeat = pSrc->type == BITS && pSrc->common.repeat == PIXMAN_REPEAT_NORMAL;
    pixman_bool_t	    maskRepeat = FALSE;
    pixman_bool_t	    srcTransform = pSrc->common.transform != NULL;
    pixman_bool_t	    maskTransform = FALSE;
    pixman_bool_t	    srcAlphaMap = pSrc->common.alpha_map != NULL;
    pixman_bool_t	maskAlphaMap = FALSE;
    pixman_bool_t	dstAlphaMap = pDst->common.alpha_map != NULL;
    CompositeFunc   func = NULL;

#ifdef USE_MMX
    static pixman_bool_t mmx_setup = FALSE;
    if (!mmx_setup)
    {
        fbComposeSetupMMX();
        mmx_setup = TRUE;
    }
#endif

    if (srcRepeat && srcTransform &&
	pSrc->bits.width == 1 &&
	pSrc->bits.height == 1)
	srcTransform = FALSE;

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
	    maskTransform = FALSE;
    }

    if ((pSrc->type == BITS || can_get_solid (pSrc)) && (!pMask || pMask->type == BITS)
        && !srcTransform && !maskTransform
        && !maskAlphaMap && !srcAlphaMap && !dstAlphaMap
        && (pSrc->common.filter != PIXMAN_FILTER_CONVOLUTION)
        && (!pMask || pMask->common.filter != PIXMAN_FILTER_CONVOLUTION)
	&& !pSrc->common.read_func && !pSrc->common.write_func
	&& !(pMask && pMask->common.read_func) && !(pMask && pMask->common.write_func)
	&& !pDst->common.read_func && !pDst->common.write_func)
    switch (op) {
    case PIXMAN_OP_OVER:
	if (pMask)
	{
	    if (can_get_solid(pSrc) &&
		!maskRepeat)
	    {
		if (pSrc->type == SOLID || PIXMAN_FORMAT_COLOR(pSrc->bits.format)) {
		    switch (pMask->bits.format) {
		    case PIXMAN_a8:
			switch (pDst->bits.format) {
			case PIXMAN_r5g6b5:
			case PIXMAN_b5g6r5:
#ifdef USE_MMX
			    if (pixman_have_mmx())
				func = fbCompositeSolidMask_nx8x0565mmx;
			    else
#endif
				func = fbCompositeSolidMask_nx8x0565;
			    break;
			case PIXMAN_r8g8b8:
			case PIXMAN_b8g8r8:
			    func = fbCompositeSolidMask_nx8x0888;
			    break;
			case PIXMAN_a8r8g8b8:
			case PIXMAN_x8r8g8b8:
			case PIXMAN_a8b8g8r8:
			case PIXMAN_x8b8g8r8:
#ifdef USE_MMX
			    if (pixman_have_mmx())
				func = fbCompositeSolidMask_nx8x8888mmx;
			    else
#endif
				func = fbCompositeSolidMask_nx8x8888;
			    break;
			default:
			    break;
			}
			break;
		    case PIXMAN_a8r8g8b8:
			if (pMask->common.component_alpha) {
			    switch (pDst->bits.format) {
			    case PIXMAN_a8r8g8b8:
			    case PIXMAN_x8r8g8b8:
#ifdef USE_MMX
				if (pixman_have_mmx())
				    func = fbCompositeSolidMask_nx8888x8888Cmmx;
				else
#endif
				    func = fbCompositeSolidMask_nx8888x8888C;
				break;
			    case PIXMAN_r5g6b5:
#ifdef USE_MMX
				if (pixman_have_mmx())
				    func = fbCompositeSolidMask_nx8888x0565Cmmx;
				else
#endif
				    func = fbCompositeSolidMask_nx8888x0565C;
				break;
			    default:
				break;
			    }
			}
			break;
		    case PIXMAN_a8b8g8r8:
			if (pMask->common.component_alpha) {
			    switch (pDst->bits.format) {
			    case PIXMAN_a8b8g8r8:
			    case PIXMAN_x8b8g8r8:
#ifdef USE_MMX
				if (pixman_have_mmx())
				    func = fbCompositeSolidMask_nx8888x8888Cmmx;
				else
#endif
				    func = fbCompositeSolidMask_nx8888x8888C;
				break;
			    case PIXMAN_b5g6r5:
#ifdef USE_MMX
				if (pixman_have_mmx())
				    func = fbCompositeSolidMask_nx8888x0565Cmmx;
				else
#endif
				    func = fbCompositeSolidMask_nx8888x0565C;
				break;
			    default:
				break;
			    }
			}
			break;
		    case PIXMAN_a1:
			switch (pDst->bits.format) {
			case PIXMAN_r5g6b5:
			case PIXMAN_b5g6r5:
			case PIXMAN_r8g8b8:
			case PIXMAN_b8g8r8:
			case PIXMAN_a8r8g8b8:
			case PIXMAN_x8r8g8b8:
			case PIXMAN_a8b8g8r8:
			case PIXMAN_x8b8g8r8:
			{
			    uint32_t src;

#if 0
			    /* FIXME */
			    fbComposeGetSolid(pSrc, src, pDst->bits.format);
			    if ((src & 0xff000000) == 0xff000000)
				func = fbCompositeSolidMask_nx1xn;
#endif
			    break;
			}
			default:
			    break;
			}
			break;
		    default:
			break;
		    }
		}
		if (func)
		    srcRepeat = FALSE;
	    }
	    else if (!srcRepeat) /* has mask and non-repeating source */
	    {
		if (pSrc->bits.bits == pMask->bits.bits &&
		    xSrc == xMask &&
		    ySrc == yMask &&
		    !pMask->common.component_alpha && !maskRepeat)
		{
		    /* source == mask: non-premultiplied data */
		    switch (pSrc->bits.format) {
		    case PIXMAN_x8b8g8r8:
			switch (pMask->bits.format) {
			case PIXMAN_a8r8g8b8:
			case PIXMAN_a8b8g8r8:
			    switch (pDst->bits.format) {
			    case PIXMAN_a8r8g8b8:
			    case PIXMAN_x8r8g8b8:
#ifdef USE_MMX
				if (pixman_have_mmx())
				    func = fbCompositeSrc_8888RevNPx8888mmx;
#endif
				break;
			    case PIXMAN_r5g6b5:
#ifdef USE_MMX
				if (pixman_have_mmx())
				    func = fbCompositeSrc_8888RevNPx0565mmx;
#endif
				break;
			    default:
				break;
			    }
			    break;
			default:
			    break;
			}
			break;
		    case PIXMAN_x8r8g8b8:
			switch (pMask->bits.format) {
			case PIXMAN_a8r8g8b8:
			case PIXMAN_a8b8g8r8:
			    switch (pDst->bits.format) {
			    case PIXMAN_a8b8g8r8:
			    case PIXMAN_x8b8g8r8:
#ifdef USE_MMX
				if (pixman_have_mmx())
				    func = fbCompositeSrc_8888RevNPx8888mmx;
#endif
				break;
			    case PIXMAN_r5g6b5:
#ifdef USE_MMX
				if (pixman_have_mmx())
				    func = fbCompositeSrc_8888RevNPx0565mmx;
#endif
				break;
			    default:
				break;
			    }
			    break;
			default:
			    break;
			}
			break;
		    default:
			break;
		    }
		    break;
		}
		else if (maskRepeat &&
			 pMask->bits.width == 1 &&
			 pMask->bits.height == 1)
		{
		    switch (pSrc->bits.format) {
#ifdef USE_MMX
		    case PIXMAN_x8r8g8b8:
			if ((pDst->bits.format == PIXMAN_a8r8g8b8 ||
			     pDst->bits.format == PIXMAN_x8r8g8b8) &&
			    pMask->bits.format == PIXMAN_a8 && pixman_have_mmx())
			    func = fbCompositeSrc_x888xnx8888mmx;
			break;
		    case PIXMAN_x8b8g8r8:
			if ((pDst->bits.format == PIXMAN_a8b8g8r8 ||
			     pDst->bits.format == PIXMAN_x8b8g8r8) &&
			    pMask->bits.format == PIXMAN_a8 && pixman_have_mmx())
			    func = fbCompositeSrc_x888xnx8888mmx;
			break;
		    case PIXMAN_a8r8g8b8:
			if ((pDst->bits.format == PIXMAN_a8r8g8b8 ||
			     pDst->bits.format == PIXMAN_x8r8g8b8) &&
			    pMask->bits.format == PIXMAN_a8 && pixman_have_mmx())
			    func = fbCompositeSrc_8888x8x8888mmx;
			break;
		    case PIXMAN_a8b8g8r8:
			if ((pDst->bits.format == PIXMAN_a8b8g8r8 ||
			     pDst->bits.format == PIXMAN_x8b8g8r8) &&
			    pMask->bits.format == PIXMAN_a8 && pixman_have_mmx())
			    func = fbCompositeSrc_8888x8x8888mmx;
			break;
#endif
		    default:
			break;
		    }

		    if (func)
			maskRepeat = FALSE;
		}
		else
		{
#if 0
		    /* FIXME: This code is commented out since it's apparently not
		     * actually faster than the generic code.
		     */
		    if (pMask->bits.format == PIXMAN_a8)
		    {
			if ((pSrc->bits.format == PIXMAN_x8r8g8b8 &&
			     (pDst->bits.format == PIXMAN_x8r8g8b8 ||
			      pDst->bits.format == PIXMAN_a8r8g8b8))  ||
			    (pSrc->bits.format == PIXMAN_x8b8g8r8 &&
			     (pDst->bits.format == PIXMAN_x8b8g8r8 ||
			      pDst->bits.format == PIXMAN_a8b8g8r8)))
			{
#ifdef USE_MMX
			    if (pixman_have_mmx())
				func = fbCompositeOver_x888x8x8888mmx;
			    else
#endif
				func = fbCompositeOver_x888x8x8888;
			}
		    }
#endif
		}
	    }
	}
	else /* no mask */
	{
	    if (can_get_solid(pSrc))
	    {
		/* no mask and repeating source */
		if (pSrc->type == SOLID || pSrc->bits.format == PIXMAN_a8r8g8b8)
		{
		    switch (pDst->bits.format) {
		    case PIXMAN_a8r8g8b8:
		    case PIXMAN_x8r8g8b8:
#ifdef USE_MMX
			if (pixman_have_mmx())
			{
			    srcRepeat = FALSE;
			    func = fbCompositeSolid_nx8888mmx;
			}
#endif
			break;
		    case PIXMAN_r5g6b5:
#ifdef USE_MMX
			if (pixman_have_mmx())
			{
			    srcRepeat = FALSE;
			    func = fbCompositeSolid_nx0565mmx;
			}
#endif
			break;
		    default:
			break;
		    }
		    break;
		}
	    }
	    else if (! srcRepeat)
	    {
		/*
		 * Formats without alpha bits are just Copy with Over
		 */
		if (pSrc->bits.format == pDst->bits.format && !PIXMAN_FORMAT_A(pSrc->bits.format))
		{
#ifdef USE_MMX
		    if (pixman_have_mmx() &&
			(pSrc->bits.format == PIXMAN_x8r8g8b8 || pSrc->bits.format == PIXMAN_x8b8g8r8))
			func = fbCompositeCopyAreammx;
		    else
#endif
#if 0
		    /* FIXME */
			func = fbCompositeSrcSrc_nxn
#endif
			    ;
		}
		else switch (pSrc->bits.format) {
		case PIXMAN_a8r8g8b8:
		    switch (pDst->bits.format) {
		    case PIXMAN_a8r8g8b8:
		    case PIXMAN_x8r8g8b8:
#ifdef USE_MMX
			if (pixman_have_mmx())
			    func = fbCompositeSrc_8888x8888mmx;
			else
#endif
			    func = fbCompositeSrc_8888x8888;
			break;
		    case PIXMAN_r8g8b8:
			func = fbCompositeSrc_8888x0888;
			break;
		    case PIXMAN_r5g6b5:
#ifdef USE_MMX
			if (pixman_have_mmx())
			    func = fbCompositeSrc_8888x0565mmx;
			else
#endif
			    func = fbCompositeSrc_8888x0565;
			break;
		    default:
			break;
		    }
		    break;
		case PIXMAN_x8r8g8b8:
		    switch (pDst->bits.format) {
		    case PIXMAN_x8r8g8b8:
#ifdef USE_MMX
			if (pixman_have_mmx())
			    func = fbCompositeCopyAreammx;
#endif
			break;
		    default:
			break;
		    }
		case PIXMAN_x8b8g8r8:
		    switch (pDst->bits.format) {
		    case PIXMAN_x8b8g8r8:
#ifdef USE_MMX
			if (pixman_have_mmx())
			    func = fbCompositeCopyAreammx;
#endif
			break;
		    default:
			break;
		    }
		    break;
		case PIXMAN_a8b8g8r8:
		    switch (pDst->bits.format) {
		    case PIXMAN_a8b8g8r8:
		    case PIXMAN_x8b8g8r8:
#ifdef USE_MMX
			if (pixman_have_mmx())
			    func = fbCompositeSrc_8888x8888mmx;
			else
#endif
			    func = fbCompositeSrc_8888x8888;
			break;
		    case PIXMAN_b8g8r8:
			func = fbCompositeSrc_8888x0888;
			break;
		    case PIXMAN_b5g6r5:
#ifdef USE_MMX
			if (pixman_have_mmx())
			    func = fbCompositeSrc_8888x0565mmx;
			else
#endif
			    func = fbCompositeSrc_8888x0565;
			break;
		    default:
			break;
		    }
		    break;
		default:
		    break;
		}
	    }
	}
	break;
    case PIXMAN_OP_ADD:
	if (pMask == 0)
	{
	    switch (pSrc->bits.format) {
	    case PIXMAN_a8r8g8b8:
		switch (pDst->bits.format) {
		case PIXMAN_a8r8g8b8:
#ifdef USE_MMX
		    if (pixman_have_mmx())
			func = fbCompositeSrcAdd_8888x8888mmx;
		    else
#endif
			func = fbCompositeSrcAdd_8888x8888;
		    break;
		default:
		    break;
		}
		break;
	    case PIXMAN_a8b8g8r8:
		switch (pDst->bits.format) {
		case PIXMAN_a8b8g8r8:
#ifdef USE_MMX
		    if (pixman_have_mmx())
			func = fbCompositeSrcAdd_8888x8888mmx;
		    else
#endif
			func = fbCompositeSrcAdd_8888x8888;
		    break;
		default:
		    break;
		}
		break;
	    case PIXMAN_a8:
		switch (pDst->bits.format) {
		case PIXMAN_a8:
#ifdef USE_MMX
		    if (pixman_have_mmx())
			func = fbCompositeSrcAdd_8000x8000mmx;
		    else
#endif
			func = fbCompositeSrcAdd_8000x8000;
		    break;
		default:
		    break;
		}
		break;
	    case PIXMAN_a1:
		switch (pDst->bits.format) {
		case PIXMAN_a1:
#if 0
		    /* FIXME */
		    func = fbCompositeSrcAdd_1000x1000;
#endif
		    break;
		default:
		    break;
		}
		break;
	    default:
		break;
	    }
	}
	else
	{
	    if (can_get_solid (pSrc)		&&
		pMask->bits.format == PIXMAN_a8	&&
		pDst->bits.format == PIXMAN_a8)
	    {
		srcRepeat = FALSE;
#ifdef USE_MMX
		if (pixman_have_mmx())
		    func = fbCompositeSrcAdd_8888x8x8mmx;
		else
#endif
		    func = fbCompositeSrcAdd_8888x8x8;
	    }
	}
	break;
    case PIXMAN_OP_SRC:
	if (pMask)
	{
#ifdef USE_MMX
	    if (can_get_solid (pSrc))
	    {
		if (pMask->bits.format == PIXMAN_a8)
		{
		    switch (pDst->bits.format)
		    {
		    case PIXMAN_a8r8g8b8:
		    case PIXMAN_x8r8g8b8:
		    case PIXMAN_a8b8g8r8:
		    case PIXMAN_x8b8g8r8:
			if (pixman_have_mmx())
			{
			    srcRepeat = FALSE;
			    func = fbCompositeSolidMaskSrc_nx8x8888mmx;
			}
			break;
		    default:
			break;
		    }
		}
	    }
#endif
	}
	else
	{
	    if (can_get_solid (pSrc))
	    {
		switch (pDst->bits.format)
		{
		case PIXMAN_a8r8g8b8:
		case PIXMAN_x8r8g8b8:
		case PIXMAN_a8b8g8r8:
		case PIXMAN_x8b8g8r8:
		case PIXMAN_a8:
		case PIXMAN_r5g6b5:
		    func = fbCompositeSolidFill;
		    srcRepeat = FALSE;
		    break;
		default:
		    break;
		}
	    }
	    else if (pSrc->bits.format == pDst->bits.format)
	    {
#ifdef USE_MMX
		if (pSrc->bits.bits != pDst->bits.bits && pixman_have_mmx() &&
		    (PIXMAN_FORMAT_BPP (pSrc->bits.format) == 16 ||
		     PIXMAN_FORMAT_BPP (pSrc->bits.format) == 32))
		    func = fbCompositeCopyAreammx;
		else
#endif
		    /* FIXME */
#if 0
		    func = fbCompositeSrcSrc_nxn
#endif
			;
	    }
	    else if (((pSrc->bits.format == PIXMAN_a8r8g8b8 ||
		       pSrc->bits.format == PIXMAN_x8r8g8b8) &&
		      pDst->bits.format == PIXMAN_x8r8g8b8)	||
		     ((pSrc->bits.format == PIXMAN_a8b8g8r8 ||
		       pSrc->bits.format == PIXMAN_x8b8g8r8) &&
		      pDst->bits.format == PIXMAN_x8b8g8r8))
	    {
		func = fbCompositeSrc_8888xx888;
	    }
	}
	break;
    case PIXMAN_OP_IN:
	if (pSrc->bits.format == PIXMAN_a8 &&
	    pDst->bits.format == PIXMAN_a8 &&
	    !pMask)
	{
#ifdef USE_MMX
	    if (pixman_have_mmx())
		func = fbCompositeIn_8x8mmx;
	    else
#endif
		func = fbCompositeSrcIn_8x8;
	}
	else if (srcRepeat && pMask && !pMask->common.component_alpha &&
		 (pSrc->bits.format == PIXMAN_a8r8g8b8 ||
		  pSrc->bits.format == PIXMAN_a8b8g8r8)   &&
		 (pMask->bits.format == PIXMAN_a8)        &&
		 pDst->bits.format == PIXMAN_a8)
	{
#ifdef USE_MMX
	    if (pixman_have_mmx())
		func = fbCompositeIn_nx8x8mmx;
	    else
#endif
		func = fbCompositeSolidMaskIn_nx8x8;
	    srcRepeat = FALSE;
	}
       break;
    default:
	break;
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

    if (!func) {
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
	    maskTransform = FALSE;
    }

    pixman_walk_composite_region (op, pSrc, pMask, pDst, xSrc, ySrc,
				  xMask, yMask, xDst, yDst, width, height,
				  srcRepeat, maskRepeat, func);
}


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
#endif /* __amd64__ */
#endif
