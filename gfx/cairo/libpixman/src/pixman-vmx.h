/*
 * Copyright Â© 2007 Luca Barbato
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Luca Barbato not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Luca Barbato makes no representations about the
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
 * Author:  Luca Barbato (lu_zero@gentoo.org)
 *
 * Based on work by Owen Taylor, Søren Sandmann and Lars Knoll
 */

#include "pixman-private.h"

#ifdef USE_VMX

pixman_bool_t pixman_have_vmx(void);

#else
#define pixman_have_vmx() FALSE
#endif

#ifdef USE_VMX

#define AVV(x...) {x}

void fbComposeSetupVMX (void);

#if 0
void fbCompositeIn_nx8x8vmx (pixman_operator_t	op,
			     pixman_image_t * pSrc,
			     pixman_image_t * pMask,
			     pixman_image_t * pDst,
			     INT16      xSrc,
			     INT16      ySrc,
			     INT16      xMask,
			     INT16      yMask,
			     INT16      xDst,
			     INT16      yDst,
			     CARD16     width,
			     CARD16     height);

void fbCompositeSolidMask_nx8888x0565Cvmx (pixman_operator_t      op,
					   pixman_image_t * pSrc,
					   pixman_image_t * pMask,
					   pixman_image_t * pDst,
					   INT16      xSrc,
					   INT16      ySrc,
					   INT16      xMask,
					   INT16      yMask,
					   INT16      xDst,
					   INT16      yDst,
					   CARD16     width,
					   CARD16     height);

void fbCompositeSrcAdd_8888x8888vmx (pixman_operator_t	op,
				     pixman_image_t *	pSrc,
				     pixman_image_t *	pMask,
				     pixman_image_t *	pDst,
				     INT16	xSrc,
				     INT16      ySrc,
				     INT16      xMask,
				     INT16      yMask,
				     INT16      xDst,
				     INT16      yDst,
				     CARD16     width,
				     CARD16     height);

void fbCompositeSolidMask_nx8888x8888Cvmx (pixman_operator_t	op,
					   pixman_image_t *	pSrc,
					   pixman_image_t *	pMask,
					   pixman_image_t *	pDst,
					   INT16	xSrc,
					   INT16	ySrc,
					   INT16	xMask,
					   INT16	yMask,
					   INT16	xDst,
					   INT16	yDst,
					   CARD16	width,
					   CARD16	height);

void fbCompositeSolidMask_nx8x8888vmx (pixman_operator_t      op,
				       pixman_image_t * pSrc,
				       pixman_image_t * pMask,
				       pixman_image_t * pDst,
				       INT16      xSrc,
				       INT16      ySrc,
				       INT16      xMask,
				       INT16      yMask,
				       INT16      xDst,
				       INT16      yDst,
				       CARD16     width,
				       CARD16     height);

void fbCompositeSolidMaskSrc_nx8x8888vmx (pixman_operator_t      op,
					  pixman_image_t * pSrc,
					  pixman_image_t * pMask,
					  pixman_image_t * pDst,
					  INT16      xSrc,
					  INT16      ySrc,
					  INT16      xMask,
					  INT16      yMask,
					  INT16      xDst,
					  INT16      yDst,
					  CARD16     width,
					  CARD16     height);

void fbCompositeSrcAdd_8888x8x8vmx (pixman_operator_t   op,
				    pixman_image_t * pSrc,
				    pixman_image_t * pMask,
				    pixman_image_t * pDst,
				    INT16      xSrc,
				    INT16      ySrc,
				    INT16      xMask,
				    INT16      yMask,
				    INT16      xDst,
				    INT16      yDst,
				    CARD16     width,
				    CARD16     height);

void fbCompositeIn_8x8vmx (pixman_operator_t	op,
			   pixman_image_t * pSrc,
			   pixman_image_t * pMask,
			   pixman_image_t * pDst,
			   INT16      xSrc,
			   INT16      ySrc,
			   INT16      xMask,
			   INT16      yMask,
			   INT16      xDst,
			   INT16      yDst,
			   CARD16     width,
			   CARD16     height);

void fbCompositeSrcAdd_8000x8000vmx (pixman_operator_t	op,
				     pixman_image_t * pSrc,
				     pixman_image_t * pMask,
				     pixman_image_t * pDst,
				     INT16      xSrc,
				     INT16      ySrc,
				     INT16      xMask,
				     INT16      yMask,
				     INT16      xDst,
				     INT16      yDst,
				     CARD16     width,
				     CARD16     height);

void fbCompositeSrc_8888RevNPx8888vmx (pixman_operator_t      op,
				       pixman_image_t * pSrc,
				       pixman_image_t * pMask,
				       pixman_image_t * pDst,
				       INT16      xSrc,
				       INT16      ySrc,
				       INT16      xMask,
				       INT16      yMask,
				       INT16      xDst,
				       INT16      yDst,
				       CARD16     width,
				       CARD16     height);

void fbCompositeSrc_8888x0565vmx (pixman_operator_t      op,
				  pixman_image_t * pSrc,
				  pixman_image_t * pMask,
				  pixman_image_t * pDst,
				  INT16      xSrc,
				  INT16      ySrc,
				  INT16      xMask,
				  INT16      yMask,
				  INT16      xDst,
				  INT16      yDst,
				  CARD16     width,
				  CARD16     height);

void fbCompositeSrc_8888RevNPx0565vmx (pixman_operator_t      op,
				       pixman_image_t * pSrc,
				       pixman_image_t * pMask,
				       pixman_image_t * pDst,
				       INT16      xSrc,
				       INT16      ySrc,
				       INT16      xMask,
				       INT16      yMask,
				       INT16      xDst,
				       INT16      yDst,
				       CARD16     width,
				       CARD16     height);

void fbCompositeSolid_nx8888vmx (pixman_operator_t		op,
				 pixman_image_t *	pSrc,
				 pixman_image_t *	pMask,
				 pixman_image_t *	pDst,
				 INT16		xSrc,
				 INT16		ySrc,
				 INT16		xMask,
				 INT16		yMask,
				 INT16		xDst,
				 INT16		yDst,
				 CARD16		width,
				 CARD16		height);

void fbCompositeSolid_nx0565vmx (pixman_operator_t		op,
				 pixman_image_t *	pSrc,
				 pixman_image_t *	pMask,
				 pixman_image_t *	pDst,
				 INT16		xSrc,
				 INT16		ySrc,
				 INT16		xMask,
				 INT16		yMask,
				 INT16		xDst,
				 INT16		yDst,
				 CARD16		width,
				 CARD16		height);

void fbCompositeSolidMask_nx8x0565vmx (pixman_operator_t      op,
				       pixman_image_t * pSrc,
				       pixman_image_t * pMask,
				       pixman_image_t * pDst,
				       INT16      xSrc,
				       INT16      ySrc,
				       INT16      xMask,
				       INT16      yMask,
				       INT16      xDst,
				       INT16      yDst,
				       CARD16     width,
				       CARD16     height);

void fbCompositeSrc_x888x8x8888vmx (pixman_operator_t	op,
				    pixman_image_t *  pSrc,
				    pixman_image_t *  pMask,
				    pixman_image_t *  pDst,
				    INT16	xSrc,
				    INT16	ySrc,
				    INT16       xMask,
				    INT16       yMask,
				    INT16       xDst,
				    INT16       yDst,
				    CARD16      width,
				    CARD16      height);

void fbCompositeSrc_8888x8x8888vmx (pixman_operator_t	op,
				    pixman_image_t *  pSrc,
				    pixman_image_t *  pMask,
				    pixman_image_t *  pDst,
				    INT16	xSrc,
				    INT16	ySrc,
				    INT16       xMask,
				    INT16       yMask,
				    INT16       xDst,
				    INT16       yDst,
				    CARD16      width,
				    CARD16      height);

void fbCompositeSrc_8888x8888vmx (pixman_operator_t      op,
				  pixman_image_t * pSrc,
				  pixman_image_t * pMask,
				  pixman_image_t * pDst,
				  INT16      xSrc,
				  INT16      ySrc,
				  INT16      xMask,
				  INT16      yMask,
				  INT16      xDst,
				  INT16      yDst,
				  CARD16     width,
				  CARD16     height);

pixman_bool_t fbCopyAreavmx (FbPixels	*pSrc,
		    FbPixels	*pDst,
		    int		src_x,
		    int		src_y,
		    int		dst_x,
		    int		dst_y,
		    int		width,
		    int		height);

void fbCompositeCopyAreavmx (pixman_operator_t	op,
			     pixman_image_t *	pSrc,
			     pixman_image_t *	pMask,
			     pixman_image_t *	pDst,
			     INT16	xSrc,
			     INT16      ySrc,
			     INT16      xMask,
			     INT16      yMask,
			     INT16      xDst,
			     INT16      yDst,
			     CARD16     width,
			     CARD16     height);

pixman_bool_t fbSolidFillvmx (FbPixels	*pDraw,
		     int		x,
		     int		y,
		     int		width,
		     int		height,
		     FbBits		xor);
#endif
#endif /* USE_VMX */
