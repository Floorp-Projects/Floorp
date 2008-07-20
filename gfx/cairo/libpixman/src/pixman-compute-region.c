/*
 *
 * Copyright © 1999 Keith Packard
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include "pixman-private.h"

#define BOUND(v)	(int16_t) ((v) < INT16_MIN ? INT16_MIN : (v) > INT16_MAX ? INT16_MAX : (v))

static inline pixman_bool_t
miClipPictureReg (pixman_region32_t *	pRegion,
		  pixman_region32_t *	pClip,
		  int		dx,
		  int		dy)
{
    if (pixman_region32_n_rects(pRegion) == 1 &&
	pixman_region32_n_rects(pClip) == 1)
    {
	pixman_box32_t *  pRbox = pixman_region32_rectangles(pRegion, NULL);
	pixman_box32_t *  pCbox = pixman_region32_rectangles(pClip, NULL);
	int	v;
	
	if (pRbox->x1 < (v = pCbox->x1 + dx))
	    pRbox->x1 = BOUND(v);
	if (pRbox->x2 > (v = pCbox->x2 + dx))
	    pRbox->x2 = BOUND(v);
	if (pRbox->y1 < (v = pCbox->y1 + dy))
	    pRbox->y1 = BOUND(v);
	if (pRbox->y2 > (v = pCbox->y2 + dy))
	    pRbox->y2 = BOUND(v);
	if (pRbox->x1 >= pRbox->x2 ||
	    pRbox->y1 >= pRbox->y2)
	{
	    pixman_region32_init (pRegion);
	}
    }
    else if (!pixman_region32_not_empty (pClip))
	return FALSE;
    else
    {
	if (dx || dy)
	    pixman_region32_translate (pRegion, -dx, -dy);
	if (!pixman_region32_intersect (pRegion, pRegion, pClip))
	    return FALSE;
	if (dx || dy)
	    pixman_region32_translate(pRegion, dx, dy);
    }
    return pixman_region32_not_empty(pRegion);
}


static inline pixman_bool_t
miClipPictureSrc (pixman_region32_t *	pRegion,
		  pixman_image_t *	pPicture,
		  int		dx,
		  int		dy)
{
    /* XXX what to do with clipping from transformed pictures? */
    if (pPicture->common.transform || pPicture->type != BITS)
	return TRUE;

    if (pPicture->common.repeat)
    {
	/* If the clip region was set by a client, then it should be intersected
	 * with the composite region since it's interpreted as happening
	 * after the repeat algorithm.
	 *
	 * If the clip region was not set by a client, then it was imposed by
	 * boundaries of the pixmap, or by sibling or child windows, which means
	 * it should in theory be repeated along. FIXME: we ignore that case.
	 * It is only relevant for windows that are (a) clipped by siblings/children
	 * and (b) used as source. However this case is not useful anyway due
	 * to lack of GraphicsExpose events.
	 */
	if (pPicture->common.has_client_clip)
	{
	    pixman_region32_translate ( pRegion, dx, dy);
	    
	    if (!pixman_region32_intersect (pRegion, pRegion, 
					    pPicture->common.src_clip))
		return FALSE;
	    
	    pixman_region32_translate ( pRegion, -dx, -dy);
	}
	    
	return TRUE;
    }
    else
    {
	return miClipPictureReg (pRegion,
				 pPicture->common.src_clip,
				 dx,
				 dy);
    }
}

/*
 * returns FALSE if the final region is empty.  Indistinguishable from
 * an allocation failure, but rendering ignores those anyways.
 */

pixman_bool_t
pixman_compute_composite_region32 (pixman_region32_t *	pRegion,
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
				   uint16_t		height)
{
    int		v;
    
    pRegion->extents.x1 = xDst;
    v = xDst + width;
    pRegion->extents.x2 = BOUND(v);
    pRegion->extents.y1 = yDst;
    v = yDst + height;
    pRegion->extents.y2 = BOUND(v);
    pRegion->data = 0;
    /* Check for empty operation */
    if (pRegion->extents.x1 >= pRegion->extents.x2 ||
	pRegion->extents.y1 >= pRegion->extents.y2)
    {
	pixman_region32_init (pRegion);
	return FALSE;
    }
    /* clip against dst */
    if (!miClipPictureReg (pRegion, &pDst->common.clip_region, 0, 0))
    {
	pixman_region32_fini (pRegion);
	return FALSE;
    }
    if (pDst->common.alpha_map)
    {
	if (!miClipPictureReg (pRegion, &pDst->common.alpha_map->common.clip_region,
			       -pDst->common.alpha_origin.x,
			       -pDst->common.alpha_origin.y))
	{
	    pixman_region32_fini (pRegion);
	    return FALSE;
	}
    }
    /* clip against src */
    if (!miClipPictureSrc (pRegion, pSrc, xDst - xSrc, yDst - ySrc))
    {
	pixman_region32_fini (pRegion);
	return FALSE;
    }
    if (pSrc->common.alpha_map)
    {
	if (!miClipPictureSrc (pRegion, (pixman_image_t *)pSrc->common.alpha_map,
			       xDst - (xSrc + pSrc->common.alpha_origin.x),
			       yDst - (ySrc + pSrc->common.alpha_origin.y)))
	{
	    pixman_region32_fini (pRegion);
	    return FALSE;
	}
    }
    /* clip against mask */
    if (pMask)
    {
	if (!miClipPictureSrc (pRegion, pMask, xDst - xMask, yDst - yMask))
	{
	    pixman_region32_fini (pRegion);
	    return FALSE;
	}	
	if (pMask->common.alpha_map)
	{
	    if (!miClipPictureSrc (pRegion, (pixman_image_t *)pMask->common.alpha_map,
				   xDst - (xMask + pMask->common.alpha_origin.x),
				   yDst - (yMask + pMask->common.alpha_origin.y)))
	    {
		pixman_region32_fini (pRegion);
		return FALSE;
	    }
	}
    }
    
    return TRUE;
}

PIXMAN_EXPORT pixman_bool_t
pixman_compute_composite_region (pixman_region16_t *	pRegion,
				 pixman_image_t *	pSrc,
				 pixman_image_t *	pMask,
				 pixman_image_t *	pDst,
				 int16_t		xSrc,
				 int16_t		ySrc,
				 int16_t		xMask,
				 int16_t		yMask,
				 int16_t		xDst,
				 int16_t		yDst,
				 uint16_t	width,
				 uint16_t	height)
{
    pixman_region32_t r32;
    pixman_bool_t retval;

    pixman_region32_init (&r32);
    
    retval = pixman_compute_composite_region32 (&r32, pSrc, pMask, pDst,
						xSrc, ySrc, xMask, yMask, xDst, yDst,
						width, height);

    if (retval)
    {
	if (!pixman_region16_copy_from_region32 (pRegion, &r32))
	    retval = FALSE;
    }
    
    pixman_region32_fini (&r32);
    return retval;
}
