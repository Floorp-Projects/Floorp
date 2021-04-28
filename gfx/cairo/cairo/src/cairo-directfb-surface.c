/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2012 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is Chris Wilson
 *
 * Contributor(s):
 *    Chris Wilson <chris@chris-wilson.co.uk>
 */

#include "cairoint.h"
#include "cairo-directfb.h"

#include "cairo-clip-private.h"
#include "cairo-compositor-private.h"
#include "cairo-default-context-private.h"
#include "cairo-error-private.h"
#include "cairo-image-surface-inline.h"
#include "cairo-pattern-private.h"
#include "cairo-surface-backend-private.h"
#include "cairo-surface-fallback-private.h"

#include <pixman.h>

#include <directfb.h>
#include <direct/types.h>
#include <direct/debug.h>
#include <direct/memcpy.h>
#include <direct/util.h>

slim_hidden_proto(cairo_directfb_surface_create);

typedef struct _cairo_dfb_surface {
    cairo_image_surface_t image;

    IDirectFB			*dfb;
    IDirectFBSurface		*dfb_surface;

    unsigned             blit_premultiplied : 1;
} cairo_dfb_surface_t;

static cairo_content_t
_directfb_format_to_content (DFBSurfacePixelFormat format)
{
    cairo_content_t content = 0;

    if (DFB_PIXELFORMAT_HAS_ALPHA (format))
	content |= CAIRO_CONTENT_ALPHA;
    if (DFB_COLOR_BITS_PER_PIXEL (format))
	content |= CAIRO_CONTENT_COLOR_ALPHA;

    assert(content);
    return content;
}

static inline pixman_format_code_t
_directfb_to_pixman_format (DFBSurfacePixelFormat format)
{
    switch (format) {
    case DSPF_UNKNOWN: return 0;
    case DSPF_ARGB1555: return PIXMAN_a1r5g5b5;
    case DSPF_RGB16: return PIXMAN_r5g6b5;
    case DSPF_RGB24: return PIXMAN_r8g8b8;
    case DSPF_RGB32: return PIXMAN_x8r8g8b8;
    case DSPF_ARGB: return PIXMAN_a8r8g8b8;
    case DSPF_A8: return PIXMAN_a8;
    case DSPF_YUY2: return PIXMAN_yuy2;
    case DSPF_RGB332: return PIXMAN_r3g3b2;
    case DSPF_UYVY: return 0;
    case DSPF_I420: return 0;
    case DSPF_YV12: return PIXMAN_yv12;
    case DSPF_LUT8: return 0;
    case DSPF_ALUT44: return 0;
    case DSPF_AiRGB: return 0;
    case DSPF_A1: return 0; /* bit reversed, oops */
    case DSPF_NV12: return 0;
    case DSPF_NV16: return 0;
    case DSPF_ARGB2554: return 0;
    case DSPF_ARGB4444: return PIXMAN_a4r4g4b4;
    case DSPF_NV21: return 0;
    case DSPF_AYUV: return 0;
    case DSPF_A4: return PIXMAN_a4;
    case DSPF_ARGB1666: return 0;
    case DSPF_ARGB6666: return 0;
    case DSPF_RGB18: return 0;
    case DSPF_LUT2: return 0;
    case DSPF_RGB444: return PIXMAN_x4r4g4b4;
    case DSPF_RGB555: return PIXMAN_x1r5g5b5;
#if DFB_NUM_PIXELFORMATS >= 29
    case DSPF_BGR555: return PIXMAN_x1b5g5r5;
#endif
    }
    return 0;
}

static cairo_surface_t *
_cairo_dfb_surface_create_similar (void            *abstract_src,
				   cairo_content_t  content,
				   int              width,
				   int              height)
{
    cairo_dfb_surface_t *other  = abstract_src;
    DFBSurfacePixelFormat     format;
    IDirectFBSurface      *buffer;
    DFBSurfaceDescription  dsc;
    cairo_surface_t *surface;

    if (width <= 0 || height <= 0)
	return _cairo_image_surface_create_with_content (content, width, height);

    switch (content) {
    default:
	ASSERT_NOT_REACHED;
    case CAIRO_CONTENT_COLOR_ALPHA:
	format = DSPF_ARGB;
	break;
    case CAIRO_CONTENT_COLOR:
	format = DSPF_RGB32;
	break;
    case CAIRO_CONTENT_ALPHA:
	format = DSPF_A8;
	break;
    }

    dsc.flags       = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
    dsc.caps        = DSCAPS_PREMULTIPLIED;
    dsc.width       = width;
    dsc.height      = height;
    dsc.pixelformat = format;

    if (other->dfb->CreateSurface (other->dfb, &dsc, &buffer))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_DEVICE_ERROR));

    surface = cairo_directfb_surface_create (other->dfb, buffer);
    buffer->Release (buffer);

    return surface;
}

static cairo_status_t
_cairo_dfb_surface_finish (void *abstract_surface)
{
    cairo_dfb_surface_t *surface = abstract_surface;

    surface->dfb_surface->Release (surface->dfb_surface);
    return _cairo_image_surface_finish (abstract_surface);
}

static cairo_image_surface_t *
_cairo_dfb_surface_map_to_image (void *abstract_surface,
				 const cairo_rectangle_int_t *extents)
{
    cairo_dfb_surface_t *surface = abstract_surface;

    if (surface->image.pixman_image == NULL) {
	IDirectFBSurface *buffer = surface->dfb_surface;
	pixman_image_t *image;
	void *data;
	int pitch;

	if (buffer->Lock (buffer, DSLF_READ | DSLF_WRITE, &data, &pitch))
	    return _cairo_image_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

	image = pixman_image_create_bits (surface->image.pixman_format,
					  surface->image.width,
					  surface->image.height,
					  data, pitch);
	if (image == NULL) {
	    buffer->Unlock (buffer);
	    return _cairo_image_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
	}
	_cairo_image_surface_init (&surface->image, image, surface->image.pixman_format);
    }

    return _cairo_image_surface_map_to_image (&surface->image.base, extents);
}

static cairo_int_status_t
_cairo_dfb_surface_unmap_image (void *abstract_surface,
				cairo_image_surface_t *image)
{
    cairo_dfb_surface_t *surface = abstract_surface;
    return _cairo_image_surface_unmap_image (&surface->image.base, image);
}

static cairo_status_t
_cairo_dfb_surface_flush (void *abstract_surface,
			  unsigned flags)
{
    cairo_dfb_surface_t *surface = abstract_surface;

    if (flags)
	return CAIRO_STATUS_SUCCESS;

    if (surface->image.pixman_image) {
	surface->dfb_surface->Unlock (surface->dfb_surface);

	pixman_image_unref (surface->image.pixman_image);
	surface->image.pixman_image = NULL;
	surface->image.data = NULL;
    }

    return CAIRO_STATUS_SUCCESS;
}

#if 0
static inline DFBSurfacePixelFormat
_directfb_from_pixman_format (pixman_format_code_t format)
{
    switch ((int) format) {
    case PIXMAN_a1r5g5b5: return DSPF_ARGB1555;
    case PIXMAN_r5g6b5: return DSPF_RGB16;
    case PIXMAN_r8g8b8: return DSPF_RGB24;
    case PIXMAN_x8r8g8b8: return DSPF_RGB32;
    case PIXMAN_a8r8g8b8: return DSPF_ARGB;
    case PIXMAN_a8: return DSPF_A8;
    case PIXMAN_yuy2: return DSPF_YUY2;
    case PIXMAN_r3g3b2: return DSPF_RGB332;
    case PIXMAN_yv12: return DSPF_YV12;
    case PIXMAN_a1: return DSPF_A1; /* bit reversed, oops */
    case PIXMAN_a4r4g4b4: return DSPF_ARGB4444;
    case PIXMAN_a4: return DSPF_A4;
    case PIXMAN_x4r4g4b4: return DSPF_RGB444;
    case PIXMAN_x1r5g5b5: return DSPF_RGB555;
#if DFB_NUM_PIXELFORMATS >= 29
    case PIXMAN_x1b5g5r5: return DSPF_BGR555;
#endif
    default: return 0;
    }
}

static cairo_bool_t
_directfb_get_operator (cairo_operator_t         operator,
                        DFBSurfaceBlendFunction *ret_srcblend,
                        DFBSurfaceBlendFunction *ret_dstblend)
{
    DFBSurfaceBlendFunction srcblend = DSBF_ONE;
    DFBSurfaceBlendFunction dstblend = DSBF_ZERO;

    switch (operator) {
    case CAIRO_OPERATOR_CLEAR:
	srcblend = DSBF_ZERO;
	dstblend = DSBF_ZERO;
	break;
    case CAIRO_OPERATOR_SOURCE:
	srcblend = DSBF_ONE;
	dstblend = DSBF_ZERO;
	break;
    case CAIRO_OPERATOR_OVER:
	srcblend = DSBF_ONE;
	dstblend = DSBF_INVSRCALPHA;
	break;
    case CAIRO_OPERATOR_IN:
	srcblend = DSBF_DESTALPHA;
	dstblend = DSBF_ZERO;
	break;
    case CAIRO_OPERATOR_OUT:
	srcblend = DSBF_INVDESTALPHA;
	dstblend = DSBF_ZERO;
	break;
    case CAIRO_OPERATOR_ATOP:
	srcblend = DSBF_DESTALPHA;
	dstblend = DSBF_INVSRCALPHA;
	break;
    case CAIRO_OPERATOR_DEST:
	srcblend = DSBF_ZERO;
	dstblend = DSBF_ONE;
	break;
    case CAIRO_OPERATOR_DEST_OVER:
	srcblend = DSBF_INVDESTALPHA;
	dstblend = DSBF_ONE;
	break;
    case CAIRO_OPERATOR_DEST_IN:
	srcblend = DSBF_ZERO;
	dstblend = DSBF_SRCALPHA;
	break;
    case CAIRO_OPERATOR_DEST_OUT:
	srcblend = DSBF_ZERO;
	dstblend = DSBF_INVSRCALPHA;
	break;
    case CAIRO_OPERATOR_DEST_ATOP:
	srcblend = DSBF_INVDESTALPHA;
	dstblend = DSBF_SRCALPHA;
	break;
    case CAIRO_OPERATOR_XOR:
	srcblend = DSBF_INVDESTALPHA;
	dstblend = DSBF_INVSRCALPHA;
	break;
    case CAIRO_OPERATOR_ADD:
	srcblend = DSBF_ONE;
	dstblend = DSBF_ONE;
	break;
    case CAIRO_OPERATOR_SATURATE:
	/* XXX This does not work. */
#if 0
	srcblend = DSBF_SRCALPHASAT;
	dstblend = DSBF_ONE;
	break;
#endif
    case CAIRO_OPERATOR_MULTIPLY:
    case CAIRO_OPERATOR_SCREEN:
    case CAIRO_OPERATOR_OVERLAY:
    case CAIRO_OPERATOR_DARKEN:
    case CAIRO_OPERATOR_LIGHTEN:
    case CAIRO_OPERATOR_COLOR_DODGE:
    case CAIRO_OPERATOR_COLOR_BURN:
    case CAIRO_OPERATOR_HARD_LIGHT:
    case CAIRO_OPERATOR_SOFT_LIGHT:
    case CAIRO_OPERATOR_DIFFERENCE:
    case CAIRO_OPERATOR_EXCLUSION:
    case CAIRO_OPERATOR_HSL_HUE:
    case CAIRO_OPERATOR_HSL_SATURATION:
    case CAIRO_OPERATOR_HSL_COLOR:
    case CAIRO_OPERATOR_HSL_LUMINOSITY:
    default:
	return FALSE;
    }

    *ret_srcblend = srcblend;
    *ret_dstblend = dstblend;

    return TRUE;
}
#define RUN_CLIPPED(surface, clip_region, clip, func) {\
    if ((clip_region) != NULL) {\
	int n_clips = cairo_region_num_rectangles (clip_region), n; \
        for (n = 0; n < n_clips; n++) {\
            if (clip) {\
                DFBRegion  reg, *cli = (clip); \
		cairo_rectangle_int_t rect; \
		cairo_region_get_rectangle (clip_region, n, &rect); \
		reg.x1 = rect.x; \
		reg.y1 = rect.y; \
		reg.x2 = rect.x + rect.width - 1; \
		reg.y2 = rect.y + rect.height - 1; \
                if (reg.x2 < cli->x1 || reg.y2 < cli->y1 ||\
                    reg.x1 > cli->x2 || reg.y1 > cli->y2)\
                    continue;\
                if (reg.x1 < cli->x1)\
                    reg.x1 = cli->x1;\
                if (reg.y1 < cli->y1)\
                    reg.y1 = cli->y1;\
                if (reg.x2 > cli->x2)\
                    reg.x2 = cli->x2;\
                if (reg.y2 > cli->y2)\
                    reg.y2 = cli->y2;\
                (surface)->dfbsurface->SetClip ((surface)->dfbsurface, &reg);\
            } else {\
		DFBRegion reg; \
		cairo_rectangle_int_t rect; \
		cairo_region_get_rectangle (clip_region, n, &rect); \
		reg.x1 = rect.x; \
		reg.y1 = rect.y; \
		reg.x2 = rect.x + rect.width - 1; \
		reg.y2 = rect.y + rect.height - 1; \
                (surface)->dfbsurface->SetClip ((surface)->dfbsurface, &reg); \
            }\
            func;\
        }\
    } else {\
        (surface)->dfbsurface->SetClip ((surface)->dfbsurface, clip);\
        func;\
    }\
}

static cairo_int_status_t
_cairo_dfb_surface_fill_rectangles (void                  *abstract_surface,
                                         cairo_operator_t       op,
                                         const cairo_color_t   *color,
                                         cairo_rectangle_int_t *rects,
                                         int                    n_rects)
{
    cairo_dfb_surface_t *dst   = abstract_surface;
    DFBSurfaceDrawingFlags    flags;
    DFBSurfaceBlendFunction   sblend;
    DFBSurfaceBlendFunction   dblend;
    DFBRectangle              r[n_rects];
    int                       i;

    D_DEBUG_AT (CairoDFB_Render,
		"%s( dst=%p, op=%d, color=%p, rects=%p, n_rects=%d ).\n",
		__FUNCTION__, dst, op, color, rects, n_rects);

    if (! dst->supported_destination)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (! _directfb_get_operator (op, &sblend, &dblend))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (CAIRO_COLOR_IS_OPAQUE (color)) {
	if (sblend == DSBF_SRCALPHA)
	    sblend = DSBF_ONE;
	else if (sblend == DSBF_INVSRCALPHA)
	    sblend = DSBF_ZERO;

	if (dblend == DSBF_SRCALPHA)
	    dblend = DSBF_ONE;
	else if (dblend == DSBF_INVSRCALPHA)
	    dblend = DSBF_ZERO;
    }
    if ((dst->base.content & CAIRO_CONTENT_ALPHA) == 0) {
	if (sblend == DSBF_DESTALPHA)
	    sblend = DSBF_ONE;
	else if (sblend == DSBF_INVDESTALPHA)
	    sblend = DSBF_ZERO;

	if (dblend == DSBF_DESTALPHA)
	    dblend = DSBF_ONE;
	else if (dblend == DSBF_INVDESTALPHA)
	    dblend = DSBF_ZERO;
    }

    flags = (sblend == DSBF_ONE && dblend == DSBF_ZERO) ? DSDRAW_NOFX : DSDRAW_BLEND;
    dst->dfbsurface->SetDrawingFlags (dst->dfbsurface, flags);
    if (flags & DSDRAW_BLEND) {
	dst->dfbsurface->SetSrcBlendFunction (dst->dfbsurface, sblend);
	dst->dfbsurface->SetDstBlendFunction (dst->dfbsurface, dblend);
    }

    dst->dfbsurface->SetColor (dst->dfbsurface,
			       color->red_short >> 8,
			       color->green_short >> 8,
			       color->blue_short >> 8,
			       color->alpha_short >> 8);

    for (i = 0; i < n_rects; i++) {
	r[i].x = rects[i].x;
	r[i].y = rects[i].y;
	r[i].w = rects[i].width;
	r[i].h = rects[i].height;
    }

    RUN_CLIPPED (dst, NULL, NULL,
		 dst->dfbsurface->FillRectangles (dst->dfbsurface, r, n_rects));

    return CAIRO_STATUS_SUCCESS;
}
#endif

static cairo_surface_backend_t
_cairo_dfb_surface_backend = {
    CAIRO_SURFACE_TYPE_DIRECTFB, /*type*/
    _cairo_dfb_surface_finish, /*finish*/
    _cairo_default_context_create,

    _cairo_dfb_surface_create_similar,/*create_similar*/
    NULL, /* create similar image */
    _cairo_dfb_surface_map_to_image,
    _cairo_dfb_surface_unmap_image,

    _cairo_surface_default_source,
    _cairo_surface_default_acquire_source_image,
    _cairo_surface_default_release_source_image,
    NULL,

    NULL, /* copy_page */
    NULL, /* show_page */

    _cairo_image_surface_get_extents,
    _cairo_image_surface_get_font_options,

    _cairo_dfb_surface_flush,
    NULL, /* mark_dirty_rectangle */

    _cairo_surface_fallback_paint,
    _cairo_surface_fallback_mask,
    _cairo_surface_fallback_stroke,
    _cairo_surface_fallback_fill,
    NULL, /* fill-stroke */
    _cairo_surface_fallback_glyphs,
};

cairo_surface_t *
cairo_directfb_surface_create (IDirectFB *dfb, IDirectFBSurface *dfbsurface)
{
    cairo_dfb_surface_t *surface;
    DFBSurfacePixelFormat     format;
    DFBSurfaceCapabilities caps;
    pixman_format_code_t pixman_format;
    int width, height;

    D_ASSERT (dfb != NULL);
    D_ASSERT (dfbsurface != NULL);

    dfbsurface->GetPixelFormat (dfbsurface, &format);
    dfbsurface->GetSize (dfbsurface, &width, &height);

    pixman_format = _directfb_to_pixman_format (format);
    if (! pixman_format_supported_destination (pixman_format))
        return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));

    surface = calloc (1, sizeof (cairo_dfb_surface_t));
    if (surface == NULL)
        return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    /* XXX dfb -> device */
    _cairo_surface_init (&surface->image.base,
                         &_cairo_dfb_surface_backend,
			 NULL, /* device */
			 _directfb_format_to_content (format),
			 FALSE); /* is_vector */

    surface->image.pixman_format = pixman_format;
    surface->image.format = _cairo_format_from_pixman_format (pixman_format);

    surface->image.width = width;
    surface->image.height = height;
    surface->image.depth = PIXMAN_FORMAT_DEPTH(pixman_format);

    surface->dfb = dfb;
    surface->dfb_surface = dfbsurface;
    dfbsurface->AddRef (dfbsurface);

    dfbsurface->GetCapabilities (dfbsurface, &caps);
    if (caps & DSCAPS_PREMULTIPLIED)
	surface->blit_premultiplied = TRUE;

    return &surface->image.base;
}
slim_hidden_def(cairo_directfb_surface_create);
