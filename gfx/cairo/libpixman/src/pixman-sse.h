/*
 * Copyright © 2008 Rodrigo Kumpera
 * Copyright © 2008 André Tupinambá
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Red Hat not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Red Hat makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 *
 * Author:  Rodrigo Kumpera (kumpera@gmail.com)
 *          André Tupinambá (andrelrt@gmail.com)
 * 
 * Based on work by Owen Taylor and Søren Sandmann
 */
#ifndef _PIXMAN_SSE_H_
#define _PIXMAN_SSE_H_

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include "pixman-private.h"

#ifdef USE_SSE2

#if !defined(__amd64__) && !defined(__x86_64__)
pixman_bool_t pixman_have_sse(void);
#else
#define pixman_have_sse() TRUE
#endif

#else
#define pixman_have_sse() FALSE
#endif

#ifdef USE_SSE2

void fbComposeSetupSSE(void);

pixman_bool_t
pixmanFillsse2 (uint32_t *bits,
		 int stride,
		 int bpp,
		 int x,
		 int y,
		 int width,
		 int height,
		 uint32_t data);

pixman_bool_t
pixmanBltsse2 (uint32_t *src_bits,
		uint32_t *dst_bits,
		int src_stride,
		int dst_stride,
		int src_bpp,
		int dst_bpp,
		int src_x, int src_y,
		int dst_x, int dst_y,
		int width, int height);

void
fbCompositeSolid_nx8888sse2 (pixman_op_t op,
			    pixman_image_t * pSrc,
			    pixman_image_t * pMask,
			    pixman_image_t * pDst,
			    int16_t	xSrc,
			    int16_t	ySrc,
			    int16_t	xMask,
			    int16_t	yMask,
			    int16_t	xDst,
			    int16_t	yDst,
			    uint16_t	width,
			    uint16_t	height);

void
fbCompositeSolid_nx0565sse2 (pixman_op_t op,
			    pixman_image_t * pSrc,
			    pixman_image_t * pMask,
			    pixman_image_t * pDst,
			    int16_t	xSrc,
			    int16_t	ySrc,
			    int16_t	xMask,
			    int16_t	yMask,
			    int16_t	xDst,
			    int16_t	yDst,
			    uint16_t	width,
			    uint16_t	height);

void
fbCompositeSolidMask_nx8888x8888Csse2 (pixman_op_t op,
				      pixman_image_t * pSrc,
				      pixman_image_t * pMask,
				      pixman_image_t * pDst,
				      int16_t	xSrc,
				      int16_t	ySrc,
				      int16_t	xMask,
				      int16_t	yMask,
				      int16_t	xDst,
				      int16_t	yDst,
				      uint16_t	width,
				      uint16_t	height);

void
fbCompositeSrc_8888x8x8888sse2 (pixman_op_t op,
			       pixman_image_t * pSrc,
			       pixman_image_t * pMask,
			       pixman_image_t * pDst,
			       int16_t	xSrc,
			       int16_t	ySrc,
			       int16_t      xMask,
			       int16_t      yMask,
			       int16_t      xDst,
			       int16_t      yDst,
			       uint16_t     width,
			       uint16_t     height);

void
fbCompositeSrc_x888xnx8888sse2 (pixman_op_t op,
			       pixman_image_t * pSrc,
			       pixman_image_t * pMask,
			       pixman_image_t * pDst,
			       int16_t	xSrc,
			       int16_t	ySrc,
			       int16_t      xMask,
			       int16_t      yMask,
			       int16_t      xDst,
			       int16_t      yDst,
			       uint16_t     width,
			       uint16_t     height);

void
fbCompositeSrc_8888x8888sse2 (pixman_op_t op,
                 pixman_image_t * pSrc,
                 pixman_image_t * pMask,
                 pixman_image_t * pDst,
                 int16_t    xSrc,
                 int16_t    ySrc,
                 int16_t      xMask,
                 int16_t      yMask,
                 int16_t      xDst,
                 int16_t      yDst,
                 uint16_t     width,
                 uint16_t     height);

void
fbCompositeSrc_8888x0565sse2 (pixman_op_t op,
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

void
fbCompositeSolidMask_nx8x8888sse2 (pixman_op_t op,
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

void
fbCompositeSolidMaskSrc_nx8x8888sse2 (pixman_op_t op,
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

void
fbCompositeSolidMask_nx8x0565sse2 (pixman_op_t op,
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

void
fbCompositeSrc_8888RevNPx0565sse2 (pixman_op_t op,
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

void
fbCompositeSrc_8888RevNPx8888sse2 (pixman_op_t op,
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

void
fbCompositeSolidMask_nx8888x0565Csse2 (pixman_op_t op,
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

void
fbCompositeIn_nx8x8sse2 (pixman_op_t op,
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

void
fbCompositeIn_8x8sse2 (pixman_op_t op,
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

void
fbCompositeSrcAdd_8888x8x8sse2 (pixman_op_t op,
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


void
fbCompositeSrcAdd_8000x8000sse2 (pixman_op_t op,
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

void
fbCompositeSrcAdd_8888x8888sse2 (pixman_op_t    op,
                pixman_image_t *    pSrc,
                pixman_image_t *    pMask,
                pixman_image_t *     pDst,
                int16_t      xSrc,
                int16_t      ySrc,
                int16_t      xMask,
                int16_t      yMask,
                int16_t      xDst,
                int16_t      yDst,
                uint16_t     width,
                uint16_t     height);

void
fbCompositeCopyAreasse2 (pixman_op_t       op,
			pixman_image_t *	pSrc,
			pixman_image_t *	pMask,
			pixman_image_t *	pDst,
			int16_t		xSrc,
			int16_t		ySrc,
			int16_t		xMask,
			int16_t		yMask,
			int16_t		xDst,
			int16_t		yDst,
			uint16_t		width,
			uint16_t		height);

void
fbCompositeOver_x888x8x8888sse2 (pixman_op_t      op,
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

#endif /* USE_SSE2 */

#endif /* _PIXMAN_SSE_H_ */
