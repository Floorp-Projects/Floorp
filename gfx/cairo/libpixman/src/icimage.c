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

#include "pixmanint.h"

#include "pixman-xserver-compat.h"

pixman_image_t *
pixman_image_create (pixman_format_t	*format,
	       int	width,
	       int	height)
{
    pixman_image_t	*image;
    FbPixels	*pixels;

    pixels = FbPixelsCreate (width, height, format->depth);
    if (pixels == NULL)
	return NULL;

    image = pixman_image_createForPixels (pixels, format);
    if (image == NULL) {
	FbPixelsDestroy (pixels);
	return NULL;
    }

    image->owns_pixels = 1;

    return image;
}

pixman_image_t *
pixman_image_create_for_data (FbBits *data, pixman_format_t *format, int width, int height, int bpp, int stride)
{
    pixman_image_t	*image;
    FbPixels	*pixels;

    pixels = FbPixelsCreateForData (data, width, height, format->depth, bpp, stride);
    if (pixels == NULL)
	return NULL;

    image = pixman_image_createForPixels (pixels, format);
    if (image == NULL) {
	FbPixelsDestroy (pixels);
	return NULL;
    }

    image->owns_pixels = 1;

    return image;
}

pixman_image_t *
pixman_image_createForPixels (FbPixels	*pixels,
			pixman_format_t	*format)
{
    pixman_image_t		*image;

    image = malloc (sizeof (pixman_image_t));
    if (!image)
    {
	return NULL;
    }

    image->pixels = pixels;
    image->image_format = *format;
    image->format_code = format->format_code;
/* XXX: What's all this about?
    if (pDrawable->type == DRAWABLE_PIXMAP)
    {
	++((PixmapPtr)pDrawable)->refcnt;
	image->pNext = 0;
    }
    else
    {
	image->pNext = GetPictureWindow(((WindowPtr) pDrawable));
	SetPictureWindow(((WindowPtr) pDrawable), image);
    }
*/

    pixman_image_init (image);

    return image;
}

static CARD32 xRenderColorToCard32(pixman_color_t c)
{
    return
        (c.alpha >> 8 << 24) |
        (c.red >> 8 << 16) |
        (c.green & 0xff00) |
        (c.blue >> 8);
}

static uint32_t premultiply(uint32_t x)
{
    uint32_t a = x >> 24;
    uint32_t t = (x & 0xff00ff) * a + 0x800080;
    t = (t + ((t >> 8) & 0xff00ff)) >> 8;
    t &= 0xff00ff;

    x = ((x >> 8) & 0xff) * a + 0x80;
    x = (x + ((x >> 8) & 0xff));
    x &= 0xff00;
    x |= t | (a << 24);
    return x;
}

static uint32_t INTERPOLATE_PIXEL_256(uint32_t x, uint32_t a, uint32_t y, uint32_t b)
{
    CARD32 t = (x & 0xff00ff) * a + (y & 0xff00ff) * b;
    t >>= 8;
    t &= 0xff00ff;

    x = ((x >> 8) & 0xff00ff) * a + ((y >> 8) & 0xff00ff) * b;
    x &= 0xff00ff00;
    x |= t;
    return x;
}

uint32_t
pixman_gradient_color (pixman_gradient_stop_t *stop1,
		       pixman_gradient_stop_t *stop2,
		       uint32_t		      x)
{
    uint32_t current_color, next_color;
    int	     dist, idist;

    current_color = xRenderColorToCard32 (stop1->color);
    next_color    = xRenderColorToCard32 (stop2->color);

    dist  = (int) (256 * (x - stop1->x) / (stop2->x - stop1->x));
    idist = 256 - dist;

    return premultiply (INTERPOLATE_PIXEL_256 (current_color, idist,
					       next_color, dist));
}

static int
_pixman_init_gradient (pixman_gradient_image_t	    *gradient,
		       const pixman_gradient_stop_t *stops,
		       int			    n_stops)
{
    pixman_fixed16_16_t dpos;
    int			i;

    if (n_stops <= 0)
	return 1;

    dpos = -1;
    for (i = 0; i < n_stops; i++)
    {
	if (stops[i].x < dpos || stops[i].x > (1 << 16))
	    return 1;

	dpos = stops[i].x;
    }

    gradient->class	     = SourcePictClassUnknown;
    gradient->stopRange	     = 0xffff;
    gradient->colorTable     = NULL;
    gradient->colorTableSize = 0;

    return 0;
}

static pixman_image_t *
_pixman_create_source_image (void)
{
    pixman_image_t *image;

    image = (pixman_image_t *) malloc (sizeof (pixman_image_t));
    if (image == NULL)
	return NULL;
    image->pDrawable   = NULL;
    image->pixels      = NULL;
    image->format_code = PICT_a8r8g8b8;

    pixman_image_init (image);

    return image;
}

pixman_image_t *
pixman_image_create_linear_gradient (const pixman_linear_gradient_t *gradient,
				     const pixman_gradient_stop_t   *stops,
				     int			    n_stops)
{
    pixman_linear_gradient_image_t *linear;
    pixman_image_t		   *image;

    if (n_stops < 2)
	return NULL;

    image = _pixman_create_source_image ();
    if (!image)
	return NULL;

    linear = malloc (sizeof (pixman_linear_gradient_image_t) +
		     sizeof (pixman_gradient_stop_t) * n_stops);
    if (!linear)
    {
	free (image);
	return NULL;
    }

    linear->stops  = (pixman_gradient_stop_t *) (linear + 1);
    linear->nstops = n_stops;

    memcpy (linear->stops, stops, sizeof (pixman_gradient_stop_t) * n_stops);

    linear->type = SourcePictTypeLinear;
    linear->p1   = gradient->p1;
    linear->p2   = gradient->p2;

    image->pSourcePict = (pixman_source_image_t *) linear;

    if (_pixman_init_gradient (&image->pSourcePict->gradient, stops, n_stops))
    {
	free (linear);
	free (image);
	return NULL;
    }

    return image;
}

pixman_image_t *
pixman_image_create_radial_gradient (const pixman_radial_gradient_t *gradient,
				     const pixman_gradient_stop_t   *stops,
				     int			    n_stops)
{
    pixman_radial_gradient_image_t *radial;
    pixman_image_t		   *image;

    if (n_stops < 2)
	return NULL;

    image = _pixman_create_source_image ();
    if (!image)
	return NULL;

    radial = malloc (sizeof (pixman_radial_gradient_image_t) +
		     sizeof (pixman_gradient_stop_t) * n_stops);
    if (!radial)
    {
	free (image);
	return NULL;
    }

    radial->stops  = (pixman_gradient_stop_t *) (radial + 1);
    radial->nstops = n_stops;

    memcpy (radial->stops, stops, sizeof (pixman_gradient_stop_t) * n_stops);

    radial->type = SourcePictTypeRadial;
    radial->c1 = gradient->c1;
    radial->c2 = gradient->c2;
    radial->cdx = xFixedToDouble (gradient->c2.x - gradient->c1.x);
    radial->cdy = xFixedToDouble (gradient->c2.y - gradient->c1.y);
    radial->dr = xFixedToDouble (gradient->c2.radius - gradient->c1.radius);
    radial->A = (  radial->cdx * radial->cdx
		 + radial->cdy * radial->cdy
		 - radial->dr  * radial->dr);

    image->pSourcePict = (pixman_source_image_t *) radial;

    if (_pixman_init_gradient (&image->pSourcePict->gradient, stops, n_stops))
    {
	free (radial);
	free (image);
	return NULL;
    }

    return image;
}

void
pixman_image_init (pixman_image_t *image)
{
    image->refcnt = 1;
    image->repeat = PIXMAN_REPEAT_NONE;
    image->graphicsExposures = 0;
    image->subWindowMode = ClipByChildren;
    image->polyEdge = PolyEdgeSharp;
    image->polyMode = PolyModePrecise;
    /*
     * In the server this was 0 because the composite clip list
     * can be referenced from a window (and often is)
     */
    image->hasCompositeClip = 0;
    image->hasSourceClip = 0;
    image->clientClipType = CT_NONE;
    image->componentAlpha = 0;
    image->compositeClipSource = 0;

    image->alphaMap = NULL;
    image->alphaOrigin.x = 0;
    image->alphaOrigin.y = 0;

    image->clipOrigin.x = 0;
    image->clipOrigin.y = 0;

    image->dither = 0L;

    image->stateChanges = (1 << (CPLastBit+1)) - 1;
/* XXX: What to lodge here?
    image->serialNumber = GC_CHANGE_SERIAL_BIT;
*/

    if (image->pixels) {
	pixman_region_init_rect (&image->compositeClip,
				 0, 0, image->pixels->width,
				 image->pixels->height);
	image->hasCompositeClip = 1;

	pixman_region_init_rect (&image->sourceClip,
				 0, 0, image->pixels->width,
				 image->pixels->height);
	image->hasSourceClip = 1;
    }

    image->transform = NULL;

    image->filter = PIXMAN_FILTER_NEAREST;
    image->filter_params = NULL;
    image->filter_nparams = 0;

    image->owns_pixels = 0;

    image->pSourcePict = NULL;
}

void
pixman_image_set_component_alpha (pixman_image_t	*image,
				  int		component_alpha)
{
    if (image)
	image->componentAlpha = component_alpha;
}

int
pixman_image_set_transform (pixman_image_t		*image,
		     pixman_transform_t	*transform)
{
    static const pixman_transform_t	identity = { {
	{ xFixed1, 0x00000, 0x00000 },
	{ 0x00000, xFixed1, 0x00000 },
	{ 0x00000, 0x00000, xFixed1 },
    } };

    if (transform && memcmp (transform, &identity, sizeof (pixman_transform_t)) == 0)
	transform = NULL;

    if (transform)
    {
	if (!image->transform)
	{
	    image->transform = malloc (sizeof (pixman_transform_t));
	    if (!image->transform)
		return 1;
	}
	*image->transform = *transform;
    }
    else
    {
	if (image->transform)
	{
	    free (image->transform);
	    image->transform = NULL;
	}
    }
    return 0;
}

void
pixman_image_set_repeat (pixman_image_t		*image,
			 pixman_repeat_t	repeat)
{
    if (image)
	image->repeat = repeat;
}

void
pixman_image_set_filter (pixman_image_t	*image,
		  pixman_filter_t	filter)
{
    if (image)
	image->filter = filter;
}

int
pixman_image_get_width (pixman_image_t	*image)
{
    if (image->pixels)
	return image->pixels->width;

    return 0;
}

int
pixman_image_get_height (pixman_image_t	*image)
{
    if (image->pixels)
	return image->pixels->height;

    return 0;
}

int
pixman_image_get_depth (pixman_image_t	*image)
{
    if (image->pixels)
	return image->pixels->depth;

    return 0;
}

int
pixman_image_get_stride (pixman_image_t	*image)
{
    if (image->pixels)
	return image->pixels->stride;

    return 0;
}

pixman_format_t *
pixman_image_get_format (pixman_image_t	*image)
{
    return &image->image_format;
}

FbBits *
pixman_image_get_data (pixman_image_t	*image)
{
    if (image->pixels)
	return image->pixels->data;

    return NULL;
}

void
pixman_image_destroy (pixman_image_t *image)
{
    pixman_image_destroyClip (image);

    if (image->hasCompositeClip) {
	pixman_region_fini (&image->compositeClip);
	image->hasCompositeClip = 0;
    }

    if (image->hasSourceClip) {
	pixman_region_fini (&image->sourceClip);
	image->hasSourceClip = 0;
    }

    if (image->owns_pixels) {
	FbPixelsDestroy (image->pixels);
	image->pixels = NULL;
    }

    if (image->transform) {
	free (image->transform);
	image->transform = NULL;
    }

    if (image->pSourcePict) {
	free (image->pSourcePict);
	image->pSourcePict = NULL;
    }

    free (image);
}

void
pixman_image_destroyClip (pixman_image_t *image)
{
    if (CT_NONE != image->clientClipType)
	pixman_region_fini (&image->clientClip);

    image->clientClipType = CT_NONE;
}

int
pixman_image_set_clip_region (pixman_image_t	*image,
			      pixman_region16_t	*region)
{
    pixman_image_destroyClip (image);

    if (region) {
        pixman_region_init (&image->clientClip);
	if (pixman_region_copy (&image->clientClip, region) !=
		PIXMAN_REGION_STATUS_SUCCESS) {
	    pixman_region_fini (&image->clientClip);
	    return 1;
	}
	image->clientClipType = CT_REGION;
    }

    image->stateChanges |= CPClipMask;
    if (image->pSourcePict)
	return 0;

    if (image->hasCompositeClip)
        pixman_region_fini (&image->compositeClip);

    pixman_region_init_rect (&image->compositeClip, 0, 0,
                             image->pixels->width,
                             image->pixels->height);

    image->hasCompositeClip = 1;

    if (region) {
	pixman_region_translate (&image->compositeClip,
				 - image->clipOrigin.x,
				 - image->clipOrigin.y);
	if (pixman_region_intersect (&image->compositeClip,
				 &image->compositeClip,
				 region) != PIXMAN_REGION_STATUS_SUCCESS) {
	    pixman_image_destroyClip (image);
	    pixman_region_fini (&image->compositeClip);
	    image->hasCompositeClip = 0;
	    return 1;
	}
	pixman_region_translate (&image->compositeClip,
				 image->clipOrigin.x,
				 image->clipOrigin.y);
    }

    return 0;
}

#define BOUND(v)	(int16_t) ((v) < MINSHORT ? MINSHORT : (v) > MAXSHORT ? MAXSHORT : (v))

static inline int
FbClipImageReg (pixman_region16_t	*region,
		pixman_region16_t	*clip,
		int		dx,
		int		dy)
{
    int ret = 1;
    if (pixman_region_num_rects (region) == 1 &&
	pixman_region_num_rects (clip) == 1)
    {
	pixman_box16_t *pRbox = pixman_region_rects (region);
	pixman_box16_t *pCbox = pixman_region_rects (clip);
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
	    pixman_region_empty (region);
	}
    }
    else
    {
	pixman_region_status_t status;

	pixman_region_translate (region, dx, dy);
	status = pixman_region_intersect (region, clip, region);
	ret = status == PIXMAN_REGION_STATUS_SUCCESS;
	pixman_region_translate (region, -dx, -dy);
    }
    return ret;
}

static inline int
FbClipImageSrc (pixman_region16_t	*region,
		pixman_image_t		*image,
		int		dx,
		int		dy)
{
    /* XXX what to do with clipping from transformed pictures? */
    if (image->transform)
	return 1;

    /* XXX davidr hates this, wants to never use source-based clipping */
    if (image->repeat != PIXMAN_REPEAT_NONE || image->pSourcePict) {
	int ret = 1;
	/* XXX no source clipping */
	if (image->compositeClipSource &&
	    image->clientClipType != CT_NONE) {
	    pixman_region_status_t status;

	    pixman_region_translate (region,
			   dx - image->clipOrigin.x,
			   dy - image->clipOrigin.y);
	    status = pixman_region_intersect (region,
		                              &image->clientClip,
					      region);
	    ret = status == PIXMAN_REGION_STATUS_SUCCESS;
	    pixman_region_translate (region,
			   - (dx - image->clipOrigin.x),
			   - (dy - image->clipOrigin.y));
	}

	return ret;
    } else {
	pixman_region16_t *clip;

	if (image->compositeClipSource) {
	    clip = (image->hasCompositeClip ? &image->compositeClip : NULL);
	} else {
	    clip = (image->hasSourceClip ? &image->sourceClip : NULL);
        }

	return FbClipImageReg (region, clip, dx, dy);
    }

    return 1;
}

int
FbComputeCompositeRegion (pixman_region16_t	*region,
			  pixman_image_t	*iSrc,
			  pixman_image_t	*iMask,
			  pixman_image_t	*iDst,
			  int16_t		xSrc,
			  int16_t		ySrc,
			  int16_t		xMask,
			  int16_t		yMask,
			  int16_t		xDst,
			  int16_t		yDst,
			  uint16_t	width,
			  uint16_t	height)
{
    int		v;
    int x1, y1, x2, y2;

    /* XXX: This code previously directly set the extents of the
       region here. I need to decide whether removing that has broken
       this. Also, it might be necessary to just make the pixman_region16_t
       data structure transparent anyway in which case I can just put
       the code back. */
    x1 = xDst;
    v = xDst + width;
    x2 = BOUND(v);
    y1 = yDst;
    v = yDst + height;
    y2 = BOUND(v);

    /* Check for empty operation */
    if (x1 >= x2 || y1 >= y2) {
	pixman_region_empty (region);
	return 1;
    }

    /* clip against src */
    if (!FbClipImageSrc (region, iSrc, xDst - xSrc, yDst - ySrc))
	return 0;

    if (iSrc->alphaMap &&
	!FbClipImageSrc (region, iSrc->alphaMap,
                         xDst - (xSrc + iSrc->alphaOrigin.x),
                         yDst - (ySrc + iSrc->alphaOrigin.y)))
        return 0;

    /* clip against mask */
    if (iMask) {
	if (!FbClipImageSrc (region, iMask, xDst - xMask, yDst - yMask))
	    return 0;

	if (iMask->alphaMap &&
	    !FbClipImageSrc (region, iMask->alphaMap,
                             xDst - (xMask + iMask->alphaOrigin.x),
                             yDst - (yMask + iMask->alphaOrigin.y)))
            return 0;
    }

    if (!FbClipImageReg (region,
                         iDst->hasCompositeClip ?
                         &iDst->compositeClip : NULL,
                         0, 0))
	return 0;

    if (iDst->alphaMap &&
        !FbClipImageReg (region,
                         iDst->hasCompositeClip ?
                         &iDst->alphaMap->compositeClip : NULL,
                         -iDst->alphaOrigin.x, -iDst->alphaOrigin.y))
        return 0;

    return 1;
}

int
miIsSolidAlpha (pixman_image_t *src)
{
    char	line[1];

    /* Alpha-only */
    if (PICT_FORMAT_TYPE (src->format_code) != PICT_TYPE_A)
	return 0;
    /* repeat */
    if (!src->repeat)
	return 0;
    /* 1x1 */
    if (src->pixels->width != 1 || src->pixels->height != 1)
	return 0;
    line[0] = 1;
    /* XXX: For the next line, fb has:
	(*pScreen->GetImage) (src->pixels, 0, 0, 1, 1, ZPixmap, ~0L, line);
       Is the following simple assignment sufficient?
    */
    line[0] = src->pixels->data[0];
    switch (src->pixels->bpp) {
    case 1:
	return (uint8_t) line[0] == 1 || (uint8_t) line[0] == 0x80;
    case 4:
	return (uint8_t) line[0] == 0xf || (uint8_t) line[0] == 0xf0;
    case 8:
	return (uint8_t) line[0] == 0xff;
    default:
	return 0;
    }
}
