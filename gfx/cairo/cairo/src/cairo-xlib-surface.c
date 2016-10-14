/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
 * Copyright © 2005 Red Hat, Inc.
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
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 *	Behdad Esfahbod <behdad@behdad.org>
 *	Chris Wilson <chris@chris-wilson.co.uk>
 *	Karl Tomlinson <karlt+@karlt.net>, Mozilla Corporation
 */

/* Heed well the words of Owen Taylor:
 * "Any patch that works around a render bug, or claims to, without a
 * specific reference to the bug filed in bugzilla.freedesktop.org will
 * never pass approval."
 */

#include "cairoint.h"

#include "cairo-xlib-private.h"
#include "cairo-xlib-surface-private.h"
#include "cairo-clip-private.h"
#include "cairo-error-private.h"
#include "cairo-scaled-font-private.h"
#include "cairo-surface-snapshot-private.h"
#include "cairo-surface-subsurface-private.h"
#include "cairo-region-private.h"
#include "cairo-xlib-xrender-private.h"

#include <X11/Xutil.h> /* for XDestroyImage */
#include <X11/Xlibint.h> /* for access to XDisplay's innards */

#define XLIB_COORD_MAX 32767

#define DEBUG 0

#if DEBUG
#define UNSUPPORTED(reason) \
    fprintf (stderr, \
	     "cairo-xlib: hit unsupported operation %s(), line %d: %s\n", \
	     __FUNCTION__, __LINE__, reason), \
    CAIRO_INT_STATUS_UNSUPPORTED
#else
#define UNSUPPORTED(reason) CAIRO_INT_STATUS_UNSUPPORTED
#endif

#if DEBUG
static void CAIRO_PRINTF_FORMAT (2, 3)
_x_bread_crumb (Display *dpy,
		const char *fmt,
		...)
{
    xReq *req;
    char buf[2048];
    unsigned int len, len_dwords;
    va_list ap;

    va_start (ap, fmt);
    len = vsnprintf (buf, sizeof (buf), fmt, ap);
    va_end (ap);

    buf[len++] = '\0';
    while (len & 3)
	buf[len++] = '\0';

    LockDisplay (dpy);
    GetEmptyReq (NoOperation, req);

    len_dwords = len >> 2;
    SetReqLen (req, len_dwords, len_dwords);
    Data (dpy, buf, len);

    UnlockDisplay (dpy);
    SyncHandle ();
}
#define X_DEBUG(x) _x_bread_crumb x
#else
#define X_DEBUG(x)
#endif

/**
 * SECTION:cairo-xlib
 * @Title: XLib Surfaces
 * @Short_Description: X Window System rendering using XLib
 * @See_Also: #cairo_surface_t
 *
 * The XLib surface is used to render cairo graphics to X Window System
 * windows and pixmaps using the XLib library.
 *
 * Note that the XLib surface automatically takes advantage of X render extension
 * if it is available.
 */

/**
 * CAIRO_HAS_XLIB_SURFACE:
 *
 * Defined if the Xlib surface backend is available.
 * This macro can be used to conditionally compile backend-specific code.
 */

/**
 * SECTION:cairo-xlib-xrender
 * @Title: XLib/XRender Backend
 * @Short_Description: X Window System rendering using XLib and the X Render extension
 * @See_Also: #cairo_surface_t
 *
 * The XLib surface is used to render cairo graphics to X Window System
 * windows and pixmaps using the XLib and Xrender libraries.
 *
 * Note that the XLib surface automatically takes advantage of X Render extension
 * if it is available.
 */

/**
 * CAIRO_HAS_XLIB_XRENDER_SURFACE:
 *
 * Defined if the XLib/XRender surface functions are available.
 * This macro can be used to conditionally compile backend-specific code.
 */

/* Xlib doesn't define a typedef, so define one ourselves */
typedef int (*cairo_xlib_error_func_t) (Display     *display,
					XErrorEvent *event);

static cairo_surface_t *
_cairo_xlib_surface_create_internal (cairo_xlib_screen_t	*screen,
				     Drawable		        drawable,
				     Visual		       *visual,
				     XRenderPictFormat	       *xrender_format,
				     int			width,
				     int			height,
				     int			depth);

static cairo_bool_t
_cairo_surface_is_xlib (cairo_surface_t *surface);

static cairo_bool_t
_native_byte_order_lsb (void);

static cairo_int_status_t
_cairo_xlib_surface_show_glyphs (void                *abstract_dst,
				 cairo_operator_t     op,
				 const cairo_pattern_t  *src_pattern,
				 cairo_glyph_t       *glyphs,
				 int		      num_glyphs,
				 cairo_scaled_font_t *scaled_font,
				 cairo_clip_t	     *clip,
				 int		     *remaining_glyphs);

/*
 * Instead of taking two round trips for each blending request,
 * assume that if a particular drawable fails GetImage that it will
 * fail for a "while"; use temporary pixmaps to avoid the errors
 */

#define CAIRO_ASSUME_PIXMAP	20

static const XTransform identity = { {
    { 1 << 16, 0x00000, 0x00000 },
    { 0x00000, 1 << 16, 0x00000 },
    { 0x00000, 0x00000, 1 << 16 },
} };

#define CAIRO_SURFACE_RENDER_AT_LEAST(surface, major, minor)	\
	(((surface)->render_major > major) ||			\
	 (((surface)->render_major == major) && ((surface)->render_minor >= minor)))

#define CAIRO_SURFACE_RENDER_HAS_CREATE_PICTURE(surface)		CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 0)
#define CAIRO_SURFACE_RENDER_HAS_COMPOSITE(surface)		CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 0)
#define CAIRO_SURFACE_RENDER_HAS_COMPOSITE_TEXT(surface)	CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 0)

#define CAIRO_SURFACE_RENDER_HAS_FILL_RECTANGLES(surface)		CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 1)

#define CAIRO_SURFACE_RENDER_HAS_DISJOINT(surface)			CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 2)
#define CAIRO_SURFACE_RENDER_HAS_CONJOINT(surface)			CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 2)

#define CAIRO_SURFACE_RENDER_HAS_TRAPEZOIDS(surface)		CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 4)
#define CAIRO_SURFACE_RENDER_HAS_TRIANGLES(surface)		CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 4)
#define CAIRO_SURFACE_RENDER_HAS_TRISTRIP(surface)			CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 4)
#define CAIRO_SURFACE_RENDER_HAS_TRIFAN(surface)			CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 4)

#define CAIRO_SURFACE_RENDER_HAS_PICTURE_TRANSFORM(surface)	CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 6)
#define CAIRO_SURFACE_RENDER_HAS_FILTERS(surface)	CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 6)

#define CAIRO_SURFACE_RENDER_HAS_EXTENDED_REPEAT(surface)	CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 10)
#define CAIRO_SURFACE_RENDER_HAS_GRADIENTS(surface)	CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 10)

#define CAIRO_SURFACE_RENDER_HAS_PDF_OPERATORS(surface)	CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 11)

#define CAIRO_SURFACE_RENDER_SUPPORTS_OPERATOR(surface, op)	\
     ((op) <= CAIRO_OPERATOR_SATURATE ||			\
      (CAIRO_SURFACE_RENDER_HAS_PDF_OPERATORS(surface) &&	\
       (op) <= CAIRO_OPERATOR_HSL_LUMINOSITY))

static Visual *
_visual_for_xrender_format(Screen *screen,
			   XRenderPictFormat *xrender_format)
{
    int d, v;

    /* XXX Consider searching through the list of known cairo_visual_t for
     * the reverse mapping.
     */

    for (d = 0; d < screen->ndepths; d++) {
	Depth *d_info = &screen->depths[d];

	if (d_info->depth != xrender_format->depth)
	    continue;

	for (v = 0; v < d_info->nvisuals; v++) {
	    Visual *visual = &d_info->visuals[v];

	    switch (visual->class) {
	    case TrueColor:
		if (xrender_format->type != PictTypeDirect)
		    continue;
		break;

	    case DirectColor:
		/* Prefer TrueColor to DirectColor.
		 * (XRenderFindVisualFormat considers both TrueColor and DirectColor
		 * Visuals to match the same PictFormat.)
		 */
		continue;

	    case StaticGray:
	    case GrayScale:
	    case StaticColor:
	    case PseudoColor:
		if (xrender_format->type != PictTypeIndexed)
		    continue;
		break;
	    }

	    if (xrender_format ==
		XRenderFindVisualFormat (DisplayOfScreen(screen), visual))
		return visual;
	}
    }

    return NULL;
}

static cairo_status_t
_cairo_xlib_surface_set_clip_region (cairo_xlib_surface_t *surface,
				     cairo_region_t *region)
{
    cairo_bool_t had_clip_rects = surface->clip_region != NULL;

    if (had_clip_rects == FALSE && region == NULL)
	return CAIRO_STATUS_SUCCESS;

    if (surface->clip_region == region)
	return CAIRO_STATUS_SUCCESS;

    if (cairo_region_equal (surface->clip_region, region))
	return CAIRO_STATUS_SUCCESS;

    cairo_region_destroy (surface->clip_region);
    surface->clip_region = cairo_region_reference (region);

    if (surface->clip_rects != surface->embedded_clip_rects) {
	free (surface->clip_rects);
	surface->clip_rects = surface->embedded_clip_rects;
    }
    surface->num_clip_rects = 0;

    if (region != NULL) {
	XRectangle *rects = NULL;
	int n_rects, i;

	n_rects = cairo_region_num_rectangles (region);
	if (n_rects > ARRAY_LENGTH (surface->embedded_clip_rects)) {
	    rects = _cairo_malloc_ab (n_rects, sizeof (XRectangle));
	    if (unlikely (rects == NULL))
		return _cairo_error (CAIRO_STATUS_NO_MEMORY);
	} else {
	    rects = surface->embedded_clip_rects;
	}

	for (i = 0; i < n_rects; i++) {
	    cairo_rectangle_int_t rect;

	    cairo_region_get_rectangle (region, i, &rect);

	    rects[i].x = rect.x;
	    rects[i].y = rect.y;
	    rects[i].width = rect.width;
	    rects[i].height = rect.height;
	}

	surface->clip_rects = rects;
	surface->num_clip_rects = n_rects;
    }

    surface->clip_dirty = CAIRO_XLIB_SURFACE_CLIP_DIRTY_ALL;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_content_t
_xrender_format_to_content (XRenderPictFormat *xrender_format)
{
    cairo_bool_t xrender_format_has_alpha;
    cairo_bool_t xrender_format_has_color;

    /* This only happens when using a non-Render server. Let's punt
     * and say there's no alpha here. */
    if (xrender_format == NULL)
	return CAIRO_CONTENT_COLOR;

    xrender_format_has_alpha = (xrender_format->direct.alphaMask != 0);
    xrender_format_has_color = (xrender_format->direct.redMask   != 0 ||
				xrender_format->direct.greenMask != 0 ||
				xrender_format->direct.blueMask  != 0);

    if (xrender_format_has_alpha)
	if (xrender_format_has_color)
	    return CAIRO_CONTENT_COLOR_ALPHA;
	else
	    return CAIRO_CONTENT_ALPHA;
    else
	return CAIRO_CONTENT_COLOR;
}

static cairo_surface_t *
_cairo_xlib_surface_create_similar (void	       *abstract_src,
				    cairo_content_t	content,
				    int			width,
				    int			height)
{
    cairo_xlib_surface_t *src = abstract_src;
    XRenderPictFormat *xrender_format;
    cairo_xlib_surface_t *surface;
    cairo_xlib_display_t *display;
    Pixmap pix;

    if (width > XLIB_COORD_MAX || height > XLIB_COORD_MAX)
	return NULL;

    if (! CAIRO_SURFACE_RENDER_HAS_CREATE_PICTURE (src))
	return NULL;

    if (_cairo_xlib_display_acquire (src->base.device, &display))
        return NULL;

    /* If we never found an XRenderFormat or if it isn't compatible
     * with the content being requested, then we fallback to just
     * constructing a cairo_format_t instead, (which will fairly
     * arbitrarily pick a visual/depth for the similar surface.
     */
    xrender_format = src->xrender_format;
    if ((xrender_format != NULL &&
	_xrender_format_to_content (xrender_format) == content) ||
	(xrender_format =
	 _cairo_xlib_display_get_xrender_format (display,
						 _cairo_format_from_content (content))))
    {
	Visual *visual;

	/* We've got a compatible XRenderFormat now, which means the
	 * similar surface will match the existing surface as closely in
	 * visual/depth etc. as possible. */
	pix = XCreatePixmap (display->display, src->drawable,
			     width <= 0 ? 1 : width, height <= 0 ? 1 : height,
			     xrender_format->depth);

	if (xrender_format == src->xrender_format)
	    visual = src->visual;
	else
	    visual = _visual_for_xrender_format(src->screen->screen,
					        xrender_format);

	surface = (cairo_xlib_surface_t *)
		  _cairo_xlib_surface_create_internal (src->screen, pix,
						       visual,
						       xrender_format,
						       width, height,
						       xrender_format->depth);
    }
    else
    {
#ifdef DEBUG_FORCE_FALLBACKS
	Screen *screen = src->screen->screen;
	int depth;

	/* No compatabile XRenderFormat, see if we can make an ordinary pixmap,
	 * so that we can still accelerate blits with XCopyArea(). */
	if (content != CAIRO_CONTENT_COLOR) {
            cairo_device_release (&display->base);
	    return NULL;
        }

	depth = DefaultDepthOfScreen (screen);

	pix = XCreatePixmap (display->display, RootWindowOfScreen (screen),
			     width <= 0 ? 1 : width, height <= 0 ? 1 : height,
			     depth);

	surface = (cairo_xlib_surface_t *)
		  _cairo_xlib_surface_create_internal (src->screen, pix,
						       DefaultVisualOfScreen (screen),
						       NULL,
						       width, height, depth);
#else
	/* No compatabile XRenderFormat, just say no. */
        cairo_device_release (&display->base);
	return NULL;
#endif
    }

    if (unlikely (surface->base.status)) {
	XFreePixmap (display->display, pix);
        cairo_device_release (&display->base);
	return &surface->base;
    }

    surface->owns_pixmap = TRUE;

    cairo_device_release (&display->base);

    return &surface->base;
}

static cairo_status_t
_cairo_xlib_surface_finish (void *abstract_surface)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    cairo_status_t        status;
    cairo_xlib_display_t *display;

    X_DEBUG ((display->display, "finish (drawable=%x)", (unsigned int) surface->drawable));

    status = _cairo_xlib_display_acquire (surface->base.device, &display);
    if (unlikely (status))
        return status;

    if (surface->owns_pixmap) {
	cairo_status_t status2;

	if (surface->dst_picture != None) {
	    status2 = _cairo_xlib_display_queue_resource (display,
							  XRenderFreePicture,
							  surface->dst_picture);
	    if (status == CAIRO_STATUS_SUCCESS)
		status = status2;
	}

	if (surface->src_picture != None) {
	    status2 = _cairo_xlib_display_queue_resource (display,
							  XRenderFreePicture,
							  surface->src_picture);
	    if (status == CAIRO_STATUS_SUCCESS)
		status = status2;
	}

	status2 = _cairo_xlib_display_queue_resource (display,
		                           (cairo_xlib_notify_resource_func) XFreePixmap,
					   surface->drawable);
	if (status == CAIRO_STATUS_SUCCESS)
	    status = status2;
    } else {
	if (surface->dst_picture != None)
	    XRenderFreePicture (display->display, surface->dst_picture);

	if (surface->src_picture != None)
	    XRenderFreePicture (display->display, surface->src_picture);
    }

    if (surface->clip_rects != surface->embedded_clip_rects)
	free (surface->clip_rects);

    if (display->display != NULL)
	_cairo_xlib_remove_close_display_hook (display,
					       &surface->close_display_hook);

    cairo_device_release (&display->base);

    cairo_region_destroy (surface->clip_region);

    return status;
}

static cairo_status_t
_cairo_xlib_surface_get_gc (cairo_xlib_display_t *display,
                            cairo_xlib_surface_t *surface,
                            GC                   *gc)
{
    *gc = _cairo_xlib_screen_get_gc (display,
                                     surface->screen,
				     surface->depth,
				     surface->drawable);
    if (unlikely (*gc == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_xlib_surface_put_gc (cairo_xlib_display_t *display,
                            cairo_xlib_surface_t *surface,
                            GC                    gc)
{
    _cairo_xlib_screen_put_gc (display,
                               surface->screen,
			       surface->depth,
			       gc);
}

static int
_noop_error_handler (Display     *display,
		     XErrorEvent *event)
{
    return False;		/* return value is ignored */
}

static void
_swap_ximage_2bytes (XImage *ximage)
{
    int i, j;
    char *line = ximage->data;

    for (j = ximage->height; j; j--) {
	uint16_t *p = (uint16_t *) line;
	for (i = ximage->width; i; i--) {
	    *p = bswap_16 (*p);
	    p++;
	}

	line += ximage->bytes_per_line;
    }
}

static void
_swap_ximage_3bytes (XImage *ximage)
{
    int i, j;
    char *line = ximage->data;

    for (j = ximage->height; j; j--) {
	uint8_t *p = (uint8_t *) line;
	for (i = ximage->width; i; i--) {
	    uint8_t tmp;
	    tmp = p[2];
	    p[2] = p[0];
	    p[0] = tmp;
	    p += 3;
	}

	line += ximage->bytes_per_line;
    }
}

static void
_swap_ximage_4bytes (XImage *ximage)
{
    int i, j;
    char *line = ximage->data;

    for (j = ximage->height; j; j--) {
	uint32_t *p = (uint32_t *) line;
	for (i = ximage->width; i; i--) {
	    *p = bswap_32 (*p);
	    p++;
	}

	line += ximage->bytes_per_line;
    }
}

static void
_swap_ximage_nibbles (XImage *ximage)
{
    int i, j;
    char *line = ximage->data;

    for (j = ximage->height; j; j--) {
	uint8_t *p = (uint8_t *) line;
	for (i = (ximage->width + 1) / 2; i; i--) {
	    *p = ((*p >> 4) & 0xf) | ((*p << 4) & ~0xf);
	    p++;
	}

	line += ximage->bytes_per_line;
    }
}

static void
_swap_ximage_bits (XImage *ximage)
{
    int i, j;
    char *line = ximage->data;
    int unit = ximage->bitmap_unit;
    int line_bytes = ((ximage->width + unit - 1) & ~(unit - 1)) / 8;

    for (j = ximage->height; j; j--) {
	char *p = line;

	for (i = line_bytes; i; i--) {
	    char b = *p;
	    b = ((b << 1) & 0xaa) | ((b >> 1) & 0x55);
	    b = ((b << 2) & 0xcc) | ((b >> 2) & 0x33);
	    b = ((b << 4) & 0xf0) | ((b >> 4) & 0x0f);
	    *p = b;

	    p++;
	}

	line += ximage->bytes_per_line;
    }
}

static void
_swap_ximage_to_native (XImage *ximage)
{
    int unit_bytes = 0;
    int native_byte_order = _native_byte_order_lsb () ? LSBFirst : MSBFirst;

    if (ximage->bits_per_pixel == 1 &&
	ximage->bitmap_bit_order != native_byte_order)
    {
	_swap_ximage_bits (ximage);
	if (ximage->bitmap_bit_order == ximage->byte_order)
	    return;
    }

    if (ximage->byte_order == native_byte_order)
	return;

    switch (ximage->bits_per_pixel) {
    case 1:
	unit_bytes = ximage->bitmap_unit / 8;
	break;
    case 4:
	_swap_ximage_nibbles (ximage);
	/* fall-through */
    case 8:
    case 16:
    case 20:
    case 24:
    case 28:
    case 30:
    case 32:
	unit_bytes = (ximage->bits_per_pixel + 7) / 8;
	break;
    default:
        /* This could be hit on some rare but possible cases. */
	ASSERT_NOT_REACHED;
    }

    switch (unit_bytes) {
    case 1:
	break;
    case 2:
	_swap_ximage_2bytes (ximage);
	break;
    case 3:
	_swap_ximage_3bytes (ximage);
	break;
    case 4:
	_swap_ximage_4bytes (ximage);
	break;
    default:
	ASSERT_NOT_REACHED;
    }
}


/* Given a mask, (with a single sequence of contiguous 1 bits), return
 * the number of 1 bits in 'width' and the number of 0 bits to its
 * right in 'shift'. */
static void
_characterize_field (uint32_t mask, int *width, int *shift)
{
    *width = _cairo_popcount (mask);
    /* The final '& 31' is to force a 0 mask to result in 0 shift. */
    *shift = _cairo_popcount ((mask - 1) & ~mask) & 31;
}


/* Convert a field of 'width' bits to 'new_width' bits with correct
 * rounding. */
static inline uint32_t
_resize_field (uint32_t field, int width, int new_width)
{
    if (width == 0)
	return 0;

    if (width >= new_width) {
	return field >> (width - new_width);
    } else {
	uint32_t result = field << (new_width - width);

	while (width < new_width) {
	    result |= result >> width;
	    width <<= 1;
	}
	return result;
    }
}

static inline uint32_t
_adjust_field (uint32_t field, int adjustment)
{
    return MIN (255, MAX(0, (int)field + adjustment));
}

/* Given a shifted field value, (described by 'width' and 'shift),
 * resize it 8-bits and return that value.
 *
 * Note that the original field value must not have any non-field bits
 * set.
 */
static inline uint32_t
_field_to_8 (uint32_t field, int width, int shift)
{
    return _resize_field (field >> shift, width, 8);
}

static inline uint32_t
_field_to_8_undither (uint32_t field, int width, int shift,
		      int dither_adjustment)
{
    return _adjust_field (_field_to_8 (field, width, shift), - dither_adjustment>>width);
}

/* Given an 8-bit value, convert it to a field of 'width', shift it up
 *  to 'shift, and return it. */
static inline uint32_t
_field_from_8 (uint32_t field, int width, int shift)
{
    return _resize_field (field, 8, width) << shift;
}

static inline uint32_t
_field_from_8_dither (uint32_t field, int width, int shift,
		      int8_t dither_adjustment)
{
    return _field_from_8 (_adjust_field (field, dither_adjustment>>width), width, shift);
}

static inline uint32_t
_pseudocolor_from_rgb888_dither (cairo_xlib_visual_info_t *visual_info,
				 uint32_t r, uint32_t g, uint32_t b,
				 int8_t dither_adjustment)
{
    if (r == g && g == b) {
	dither_adjustment /= RAMP_SIZE;
	return visual_info->gray8_to_pseudocolor[_adjust_field (r, dither_adjustment)];
    } else {
	dither_adjustment = visual_info->dither8_to_cube[dither_adjustment+128];
	return visual_info->cube_to_pseudocolor[visual_info->field8_to_cube[_adjust_field (r, dither_adjustment)]]
					       [visual_info->field8_to_cube[_adjust_field (g, dither_adjustment)]]
					       [visual_info->field8_to_cube[_adjust_field (b, dither_adjustment)]];
    }
}

static inline uint32_t
_pseudocolor_to_rgb888 (cairo_xlib_visual_info_t *visual_info,
			uint32_t pixel)
{
    uint32_t r, g, b;
    pixel &= 0xff;
    r = visual_info->colors[pixel].r;
    g = visual_info->colors[pixel].g;
    b = visual_info->colors[pixel].b;
    return (r << 16) |
	   (g <<  8) |
	   (b      );
}


/* should range from -128 to 127 */
#define X 16
static const int8_t dither_pattern[4][4] = {
    {-8*X, +0*X, -6*X, +2*X},
    {+4*X, -4*X, +6*X, -2*X},
    {-5*X, +4*X, -7*X, +1*X},
    {+7*X, -1*X, +5*X, -3*X}
};
#undef X


static cairo_status_t
_get_image_surface (cairo_xlib_surface_t    *surface,
		    cairo_rectangle_int_t   *interest_rect,
		    cairo_image_surface_t  **image_out,
		    cairo_rectangle_int_t   *image_rect)
{
    cairo_int_status_t status;
    cairo_image_surface_t *image = NULL;
    XImage *ximage;
    cairo_rectangle_int_t extents;
    pixman_format_code_t pixman_format;
    cairo_format_masks_t xlib_masks;
    cairo_xlib_display_t *display;

    extents.x = 0;
    extents.y = 0;
    extents.width  = surface->width;
    extents.height = surface->height;

    if (interest_rect) {
	if (! _cairo_rectangle_intersect (&extents, interest_rect)) {
	    *image_out = NULL;
	    return CAIRO_STATUS_SUCCESS;
	}
    }

    status = _cairo_xlib_display_acquire (surface->base.device, &display);
    if (status)
        return status;

    if (image_rect)
	*image_rect = extents;

    /* XXX: This should try to use the XShm extension if available */

    if (surface->use_pixmap == 0)
    {
	cairo_xlib_error_func_t old_handler;

	old_handler = XSetErrorHandler (_noop_error_handler);

	ximage = XGetImage (display->display,
			    surface->drawable,
			    extents.x, extents.y,
			    extents.width, extents.height,
			    AllPlanes, ZPixmap);

	XSetErrorHandler (old_handler);

	/* If we get an error, the surface must have been a window,
	 * so retry with the safe code path.
	 */
	if (!ximage)
	    surface->use_pixmap = CAIRO_ASSUME_PIXMAP;
    }
    else
    {
	surface->use_pixmap--;
	ximage = NULL;
    }

    if (ximage == NULL) {
	/* XGetImage from a window is dangerous because it can
	 * produce errors if the window is unmapped or partially
	 * outside the screen. We could check for errors and
	 * retry, but to keep things simple, we just create a
	 * temporary pixmap
	 */
	Pixmap pixmap;
	GC gc;

	status = _cairo_xlib_surface_get_gc (display, surface, &gc);
	if (unlikely (status))
            goto BAIL;

	pixmap = XCreatePixmap (display->display,
				surface->drawable,
				extents.width <= 0 ? 1 : extents.width,
				extents.height <= 0 ? 1 : extents.height,
				surface->depth);
	if (pixmap) {
	    XCopyArea (display->display, surface->drawable, pixmap, gc,
		       extents.x, extents.y,
		       extents.width, extents.height,
		       0, 0);

	    ximage = XGetImage (display->display,
				pixmap,
				0, 0,
				extents.width <= 0 ? 1 : extents.width,
				extents.height <= 0 ? 1 : extents.height,
				AllPlanes, ZPixmap);

	    XFreePixmap (display->display, pixmap);
	}

	_cairo_xlib_surface_put_gc (display, surface, gc);

	if (ximage == NULL) {
	    status =  _cairo_error (CAIRO_STATUS_NO_MEMORY);
            goto BAIL;
        }
    }

    _swap_ximage_to_native (ximage);

    xlib_masks.bpp = ximage->bits_per_pixel;
    xlib_masks.alpha_mask = surface->a_mask;
    xlib_masks.red_mask = surface->r_mask;
    xlib_masks.green_mask = surface->g_mask;
    xlib_masks.blue_mask = surface->b_mask;

    /* We can't use pixman to simply write to image if:
     *   (a) the pixels are not appropriately aligned,
     *   (b) pixman does not the pixel format, or
     *   (c) if the image is palettized and we need to convert.
     */
    if (ximage->bitmap_unit == 32 && ximage->bitmap_pad == 32 &&
	_pixman_format_from_masks (&xlib_masks, &pixman_format) &&
	(surface->visual == NULL || surface->visual->class == TrueColor))
    {
	image = (cairo_image_surface_t*)
	    _cairo_image_surface_create_with_pixman_format ((unsigned char *) ximage->data,
							    pixman_format,
							    ximage->width,
							    ximage->height,
							    ximage->bytes_per_line);
	status = image->base.status;
	if (unlikely (status))
	    goto BAIL;

	/* Let the surface take ownership of the data */
	_cairo_image_surface_assume_ownership_of_data (image);
	ximage->data = NULL;
    } else {
	/* The visual we are dealing with is not supported by the
	 * standard pixman formats. So we must first convert the data
	 * to a supported format. */

	cairo_format_t format;
	unsigned char *data;
	uint32_t *row;
	uint32_t in_pixel, out_pixel;
	unsigned int rowstride;
	uint32_t a_mask=0, r_mask=0, g_mask=0, b_mask=0;
	int a_width=0, r_width=0, g_width=0, b_width=0;
	int a_shift=0, r_shift=0, g_shift=0, b_shift=0;
	int x, y, x0, y0, x_off, y_off;
	cairo_xlib_visual_info_t *visual_info = NULL;

	if (surface->visual == NULL || surface->visual->class == TrueColor) {
	    cairo_bool_t has_alpha;
	    cairo_bool_t has_color;

	    has_alpha =  surface->a_mask;
	    has_color = (surface->r_mask ||
			 surface->g_mask ||
			 surface->b_mask);

	    if (has_color) {
		if (has_alpha) {
		    format = CAIRO_FORMAT_ARGB32;
		} else {
		    format = CAIRO_FORMAT_RGB24;
		}
	    } else {
		/* XXX: Using CAIRO_FORMAT_A8 here would be more
		 * efficient, but would require slightly different code in
		 * the image conversion to put the alpha channel values
		 * into the right place. */
		format = CAIRO_FORMAT_ARGB32;
	    }

	    a_mask = surface->a_mask;
	    r_mask = surface->r_mask;
	    g_mask = surface->g_mask;
	    b_mask = surface->b_mask;

	    _characterize_field (a_mask, &a_width, &a_shift);
	    _characterize_field (r_mask, &r_width, &r_shift);
	    _characterize_field (g_mask, &g_width, &g_shift);
	    _characterize_field (b_mask, &b_width, &b_shift);

	} else {
	    format = CAIRO_FORMAT_RGB24;

	    status = _cairo_xlib_screen_get_visual_info (display,
                                                         surface->screen,
							 surface->visual,
							 &visual_info);
	    if (unlikely (status))
		goto BAIL;
	}

	image = (cairo_image_surface_t *) cairo_image_surface_create
	    (format, ximage->width, ximage->height);
	status = image->base.status;
	if (unlikely (status))
	    goto BAIL;

	data = cairo_image_surface_get_data (&image->base);
	rowstride = cairo_image_surface_get_stride (&image->base) >> 2;
	row = (uint32_t *) data;
	x0 = extents.x + surface->base.device_transform.x0;
	y0 = extents.y + surface->base.device_transform.y0;
	for (y = 0, y_off = y0 % ARRAY_LENGTH (dither_pattern);
	     y < ximage->height;
	     y++, y_off = (y_off+1) % ARRAY_LENGTH (dither_pattern)) {
	    const int8_t *dither_row = dither_pattern[y_off];
	    for (x = 0, x_off = x0 % ARRAY_LENGTH (dither_pattern[0]);
		 x < ximage->width;
		 x++, x_off = (x_off+1) % ARRAY_LENGTH (dither_pattern[0])) {
		int dither_adjustment = dither_row[x_off];

		in_pixel = XGetPixel (ximage, x, y);
		if (visual_info == NULL) {
		    out_pixel = (
			_field_to_8 (in_pixel & a_mask, a_width, a_shift) << 24 |
			_field_to_8_undither (in_pixel & r_mask, r_width, r_shift, dither_adjustment) << 16 |
			_field_to_8_undither (in_pixel & g_mask, g_width, g_shift, dither_adjustment) << 8 |
			_field_to_8_undither (in_pixel & b_mask, b_width, b_shift, dither_adjustment));
		} else {
		    /* Undithering pseudocolor does not look better */
		    out_pixel = _pseudocolor_to_rgb888 (visual_info, in_pixel);
		}
		row[x] = out_pixel;
	    }
	    row += rowstride;
	}
	cairo_surface_mark_dirty (&image->base);
    }

 BAIL:
    if (ximage)
        XDestroyImage (ximage);

    cairo_device_release (&display->base);

    if (unlikely (status)) {
	if (image) {
	    cairo_surface_destroy (&image->base);
	    image = NULL;
	}
    }
    *image_out = image;
    return status;
}

static void
_cairo_xlib_surface_ensure_src_picture (cairo_xlib_display_t *display,
                                        cairo_xlib_surface_t *surface)
{
    if (!surface->src_picture) {
	XRenderPictureAttributes pa;
	int mask = 0;

	pa.subwindow_mode = IncludeInferiors;
	mask |= CPSubwindowMode;

	surface->src_picture = XRenderCreatePicture (display->display,
						     surface->drawable,
						     surface->xrender_format,
						     mask, &pa);
    }
}

static void
_cairo_xlib_surface_set_picture_clip_rects (cairo_xlib_display_t *display,
                                            cairo_xlib_surface_t *surface)
{
    if (surface->clip_region != NULL) {
	XRenderSetPictureClipRectangles (display->display, surface->dst_picture,
					 0, 0,
					 surface->clip_rects,
					 surface->num_clip_rects);
    } else {
	XRenderPictureAttributes pa;
	pa.clip_mask = None;
	XRenderChangePicture (display->display, surface->dst_picture,
			      CPClipMask, &pa);
    }

    surface->clip_dirty &= ~CAIRO_XLIB_SURFACE_CLIP_DIRTY_PICTURE;
}

static void
_cairo_xlib_surface_set_precision (cairo_xlib_display_t	*display,
				   cairo_xlib_surface_t	*surface,
				   cairo_antialias_t	 antialias)
{
    int precision;

    switch (antialias) {
    case CAIRO_ANTIALIAS_DEFAULT:
    case CAIRO_ANTIALIAS_GRAY:
	precision = PolyModeImprecise;
	break;
    case CAIRO_ANTIALIAS_NONE:
    case CAIRO_ANTIALIAS_SUBPIXEL:
	precision = PolyModePrecise;
	break;
    }

    /* NVidia's driver version 190.42 is much slower when using PolyModeInprecise */
    precision = PolyModePrecise;

    if (surface->precision != precision) {
	XRenderPictureAttributes pa;

	pa.poly_mode = precision;
	XRenderChangePicture (display->display, surface->dst_picture,
			      CPPolyMode, &pa);

	surface->precision = precision;
    }
}

static void
_cairo_xlib_surface_ensure_dst_picture (cairo_xlib_display_t    *display,
                                        cairo_xlib_surface_t    *surface)
{
    if (!surface->dst_picture) {
	surface->dst_picture = XRenderCreatePicture (display->display,
						     surface->drawable,
						     surface->xrender_format,
						     0, NULL);
    }

    if (surface->clip_dirty & CAIRO_XLIB_SURFACE_CLIP_DIRTY_PICTURE)
	_cairo_xlib_surface_set_picture_clip_rects (display, surface);
}

static cairo_status_t
_draw_image_surface (cairo_xlib_surface_t   *surface,
		     cairo_image_surface_t  *image,
		     int                    src_x,
		     int                    src_y,
		     int                    width,
		     int                    height,
		     int                    dst_x,
		     int                    dst_y)
{
    cairo_xlib_display_t *display;
    XImage ximage;
    cairo_format_masks_t image_masks;
    int native_byte_order = _native_byte_order_lsb () ? LSBFirst : MSBFirst;
    pixman_image_t *pixman_image = NULL;
    cairo_status_t status;
    cairo_bool_t own_data;
    GC gc;

    ximage.width = image->width;
    ximage.height = image->height;
    ximage.format = ZPixmap;
    ximage.byte_order = native_byte_order;
    ximage.bitmap_unit = 32;	/* always for libpixman */
    ximage.bitmap_bit_order = native_byte_order;
    ximage.bitmap_pad = 32;	/* always for libpixman */
    ximage.depth = surface->depth;
    ximage.red_mask = surface->r_mask;
    ximage.green_mask = surface->g_mask;
    ximage.blue_mask = surface->b_mask;
    ximage.xoffset = 0;

    status = _cairo_xlib_display_acquire (surface->base.device, &display);
    if (unlikely (status))
        return status;

    if (!_pixman_format_to_masks (image->pixman_format, &image_masks))
    {
        pixman_format_code_t intermediate_format;
        int ret;

        image_masks.alpha_mask = surface->a_mask;
        image_masks.red_mask   = surface->r_mask;
        image_masks.green_mask = surface->g_mask;
        image_masks.blue_mask  = surface->b_mask;
        image_masks.bpp        = surface->depth;
        ret = _pixman_format_from_masks (&image_masks, &intermediate_format);
        assert (ret);

	own_data = FALSE;

        pixman_image = pixman_image_create_bits (intermediate_format,
                                                 image->width,
                                                 image->height,
                                                 NULL,
                                                 0);
        if (pixman_image == NULL) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
            goto BAIL;
        }

        pixman_image_composite32 (PIXMAN_OP_SRC,
                                  image->pixman_image,
                                  NULL,
                                  pixman_image,
                                  0, 0,
                                  0, 0,
                                  0, 0,
                                  image->width, image->height);

	ximage.bits_per_pixel = image_masks.bpp;
	ximage.data = (char *) pixman_image_get_data (pixman_image);
	ximage.bytes_per_line = pixman_image_get_stride (pixman_image);

	ret = XInitImage (&ximage);
	assert (ret != 0);
    }
    else if ((image_masks.alpha_mask == surface->a_mask || surface->a_mask == 0) &&
             (image_masks.red_mask   == surface->r_mask || surface->r_mask == 0) &&
             (image_masks.green_mask == surface->g_mask || surface->g_mask == 0) &&
             (image_masks.blue_mask  == surface->b_mask || surface->b_mask == 0))
    {
	int ret;

	ximage.bits_per_pixel = image_masks.bpp;
	ximage.bytes_per_line = image->stride;
	ximage.data = (char *)image->data;
	own_data = FALSE;

	ret = XInitImage (&ximage);
	assert (ret != 0);
    }
    else
    {
	unsigned int stride, rowstride;
	int x, y, x0, y0, x_off, y_off;
	uint32_t in_pixel, out_pixel, *row;
	int i_a_width=0, i_r_width=0, i_g_width=0, i_b_width=0;
	int i_a_shift=0, i_r_shift=0, i_g_shift=0, i_b_shift=0;
	int o_a_width=0, o_r_width=0, o_g_width=0, o_b_width=0;
	int o_a_shift=0, o_r_shift=0, o_g_shift=0, o_b_shift=0;
	cairo_xlib_visual_info_t *visual_info = NULL;
	cairo_bool_t true_color;
	int ret;

	if (surface->depth > 16)
	    ximage.bits_per_pixel = 32;
	else if (surface->depth > 8)
	    ximage.bits_per_pixel = 16;
	else if (surface->depth > 1)
	    ximage.bits_per_pixel = 8;
	else
	    ximage.bits_per_pixel = 1;
	stride = CAIRO_STRIDE_FOR_WIDTH_BPP (ximage.width,
					     ximage.bits_per_pixel);
	ximage.bytes_per_line = stride;
	ximage.data = _cairo_malloc_ab (stride, ximage.height);
	if (unlikely (ximage.data == NULL)) {
            own_data = FALSE;
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
            goto BAIL;
        }

	own_data = TRUE;

	ret = XInitImage (&ximage);
	assert (ret != 0);

	_characterize_field (image_masks.alpha_mask, &i_a_width, &i_a_shift);
	_characterize_field (image_masks.red_mask  , &i_r_width, &i_r_shift);
	_characterize_field (image_masks.green_mask, &i_g_width, &i_g_shift);
	_characterize_field (image_masks.blue_mask , &i_b_width, &i_b_shift);

	true_color = surface->visual == NULL ||
	             surface->visual->class == TrueColor;
	if (true_color) {
	    _characterize_field (surface->a_mask, &o_a_width, &o_a_shift);
	    _characterize_field (surface->r_mask, &o_r_width, &o_r_shift);
	    _characterize_field (surface->g_mask, &o_g_width, &o_g_shift);
	    _characterize_field (surface->b_mask, &o_b_width, &o_b_shift);
	} else {
	    status = _cairo_xlib_screen_get_visual_info (display,
                                                         surface->screen,
							 surface->visual,
							 &visual_info);
	    if (unlikely (status))
		goto BAIL;
	}

	rowstride = image->stride >> 2;
	row = (uint32_t *) image->data;
	x0 = dst_x + surface->base.device_transform.x0;
	y0 = dst_y + surface->base.device_transform.y0;
	for (y = 0, y_off = y0 % ARRAY_LENGTH (dither_pattern);
	     y < ximage.height;
	     y++, y_off = (y_off+1) % ARRAY_LENGTH (dither_pattern))
	{
	    const int8_t *dither_row = dither_pattern[y_off];

	    for (x = 0, x_off = x0 % ARRAY_LENGTH (dither_pattern[0]);
		 x < ximage.width;
		 x++, x_off = (x_off+1) % ARRAY_LENGTH (dither_pattern[0]))
	    {
		int dither_adjustment = dither_row[x_off];
		int a, r, g, b;

		if (image_masks.bpp == 1)
		    in_pixel = !! (((uint8_t*)row)[x/8] & (1 << (x & 7)));
		else if (image_masks.bpp <= 8)
		    in_pixel = ((uint8_t*)row)[x];
		else if (image_masks.bpp <= 16)
		    in_pixel = ((uint16_t*)row)[x];
		else if (image_masks.bpp <= 24)
#ifdef WORDS_BIGENDIAN
		    in_pixel = ((uint8_t*)row)[3 * x]     << 16 |
			       ((uint8_t*)row)[3 * x + 1] << 8  |
			       ((uint8_t*)row)[3 * x + 2];
#else
		    in_pixel = ((uint8_t*)row)[3 * x]           |
			       ((uint8_t*)row)[3 * x + 1] << 8  |
			       ((uint8_t*)row)[3 * x + 2] << 16;
#endif
		else
		    in_pixel = row[x];

		/* If the incoming image has no alpha channel, then the input
		 * is opaque and the output should have the maximum alpha value.
		 * For all other channels, their absence implies 0.
		 */
		if (image_masks.alpha_mask == 0x0)
		    a = 0xff;
		else
		    a = _field_to_8 (in_pixel & image_masks.alpha_mask, i_a_width, i_a_shift);
		r = _field_to_8 (in_pixel & image_masks.red_mask  , i_r_width, i_r_shift);
		g = _field_to_8 (in_pixel & image_masks.green_mask, i_g_width, i_g_shift);
		b = _field_to_8 (in_pixel & image_masks.blue_mask , i_b_width, i_b_shift);

		if (true_color) {
		    out_pixel = _field_from_8        (a, o_a_width, o_a_shift) |
				_field_from_8_dither (r, o_r_width, o_r_shift, dither_adjustment) |
				_field_from_8_dither (g, o_g_width, o_g_shift, dither_adjustment) |
				_field_from_8_dither (b, o_b_width, o_b_shift, dither_adjustment);
		} else {
		    out_pixel = _pseudocolor_from_rgb888_dither (visual_info, r, g, b, dither_adjustment);
		}

		XPutPixel (&ximage, x, y, out_pixel);
	    }

	    row += rowstride;
	}
    }

    status = _cairo_xlib_surface_get_gc (display, surface, &gc);
    if (unlikely (status))
	goto BAIL;

    XPutImage (display->display, surface->drawable, gc,
	       &ximage, src_x, src_y, dst_x, dst_y,
	       width, height);

    _cairo_xlib_surface_put_gc (display, surface, gc);

  BAIL:

    cairo_device_release (&display->base);

    if (own_data)
	free (ximage.data);
    if (pixman_image)
        pixman_image_unref (pixman_image);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xlib_surface_acquire_source_image (void                    *abstract_surface,
					  cairo_image_surface_t  **image_out,
					  void                   **image_extra)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    cairo_image_surface_t *image;
    cairo_status_t status;

    status = _get_image_surface (surface, NULL, &image, NULL);
    if (unlikely (status))
	return status;

    *image_out = image;
    *image_extra = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_surface_t *
_cairo_xlib_surface_snapshot (void *abstract_surface)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    cairo_image_surface_t *image;
    cairo_status_t status;

    status = _get_image_surface (surface, NULL, &image, NULL);
    if (unlikely (status))
	return _cairo_surface_create_in_error (status);

    return &image->base;
}

static void
_cairo_xlib_surface_release_source_image (void                   *abstract_surface,
					  cairo_image_surface_t  *image,
					  void                   *image_extra)
{
    cairo_surface_destroy (&image->base);
}

static cairo_status_t
_cairo_xlib_surface_acquire_dest_image (void                    *abstract_surface,
					cairo_rectangle_int_t   *interest_rect,
					cairo_image_surface_t  **image_out,
					cairo_rectangle_int_t   *image_rect_out,
					void                   **image_extra)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    cairo_image_surface_t *image;
    cairo_status_t status;

    status = _get_image_surface (surface, interest_rect, &image, image_rect_out);
    if (unlikely (status))
	return status;

    *image_out = image;
    *image_extra = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_xlib_surface_release_dest_image (void                    *abstract_surface,
					cairo_rectangle_int_t   *interest_rect,
					cairo_image_surface_t   *image,
					cairo_rectangle_int_t   *image_rect,
					void                    *image_extra)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    cairo_status_t status;

    status = _draw_image_surface (surface, image,
	                          0, 0, image->width, image->height,
				  image_rect->x, image_rect->y);
    status = _cairo_surface_set_error (&surface->base, status);

    cairo_surface_destroy (&image->base);
}

/*
 * Return whether two xlib surfaces share the same
 * screen.  Both core and Render drawing require this
 * when using multiple drawables in an operation.
 */
static inline cairo_bool_t
_cairo_xlib_surface_same_screen (cairo_xlib_surface_t *dst,
				 cairo_xlib_surface_t *src)
{
    return dst->screen == src->screen;
}

static cairo_status_t
_cairo_xlib_surface_clone_similar (void			*abstract_surface,
				   cairo_surface_t	*src,
				   int                   src_x,
				   int                   src_y,
				   int                   width,
				   int                   height,
				   int                  *clone_offset_x,
				   int                  *clone_offset_y,
				   cairo_surface_t     **clone_out)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    cairo_xlib_surface_t *clone;
    cairo_status_t status;

    if (src->backend == surface->base.backend ) {
	cairo_xlib_surface_t *xlib_src = (cairo_xlib_surface_t *)src;

	if (_cairo_xlib_surface_same_screen (surface, xlib_src)) {
	    *clone_offset_x = 0;
	    *clone_offset_y = 0;
	    *clone_out = cairo_surface_reference (src);

	    return CAIRO_STATUS_SUCCESS;
	}
    } else if (_cairo_surface_is_image (src)) {
	cairo_image_surface_t *image_src = (cairo_image_surface_t *)src;

	if (width > XLIB_COORD_MAX || height > XLIB_COORD_MAX)
	    return UNSUPPORTED ("roi too large for xlib");

	clone = (cairo_xlib_surface_t *)
	    _cairo_xlib_surface_create_similar (surface,
						image_src->base.content,
						width, height);
	if (clone == NULL)
	    return UNSUPPORTED ("unhandled image format, no similar surface");

	if (unlikely (clone->base.status))
	    return clone->base.status;

	status = _draw_image_surface (clone, image_src,
				      src_x, src_y,
			              width, height,
				      0, 0);
	if (unlikely (status)) {
	    cairo_surface_destroy (&clone->base);
	    return status;
	}

	*clone_offset_x = src_x;
	*clone_offset_y = src_y;
	*clone_out = &clone->base;

	return CAIRO_STATUS_SUCCESS;
    }

    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_surface_t *
_cairo_xlib_surface_create_solid_pattern_surface (void                  *abstract_surface,
						  const cairo_solid_pattern_t *solid_pattern)
{
    /* This function's only responsibility is to create a proper surface
     * for when XRender is not available.  The proper surface is a xlib
     * surface (as opposed to image surface which is what create_similar
     * returns in those cases) and the size of the dithering pattern, not
     * 1x1.  This surface can then be used in
     * _cairo_xlib_surface_solid_fill_rectangles() to do dithered "solid"
     * fills using core protocol */

    cairo_xlib_surface_t *other = abstract_surface;
    cairo_image_surface_t *image;
    cairo_xlib_surface_t *surface = NULL;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_xlib_display_t *display;

    int width = ARRAY_LENGTH (dither_pattern[0]);
    int height = ARRAY_LENGTH (dither_pattern);

    Pixmap pixmap = None;

    if (CAIRO_SURFACE_RENDER_HAS_COMPOSITE (other))
	return NULL;

    image = (cairo_image_surface_t *)
	_cairo_image_surface_create_with_content (_cairo_color_get_content (&solid_pattern->color),
						  width, height);
    status = image->base.status;
    if (unlikely (status))
	goto BAIL;

    status = _cairo_xlib_display_acquire (other->base.device, &display);
    if (unlikely (status))
        goto BAIL;

    pixmap = XCreatePixmap (display->display,
			    other->drawable,
			    width, height,
			    other->depth);
    cairo_device_release (&display->base);

    surface = (cairo_xlib_surface_t *)
	      _cairo_xlib_surface_create_internal (other->screen,
						   pixmap,
						   other->visual,
						   other->xrender_format,
						   width, height,
						   other->depth);
    status = surface->base.status;
    if (unlikely (status))
	goto BAIL;

    status = _cairo_surface_paint (&image->base,
				   CAIRO_OPERATOR_SOURCE,
				   &solid_pattern->base,
				   NULL);
    if (unlikely (status))
	goto BAIL;

    status = _draw_image_surface (surface, image,
				  0, 0,
				  width, height,
				  0, 0);
    if (unlikely (status))
	goto BAIL;

  BAIL:
    cairo_surface_destroy (&image->base);

    if (status) {
        if (pixmap != None) {
            if (!_cairo_xlib_display_acquire (other->base.device, &display)) {
              XFreePixmap (display->display, pixmap);
              cairo_device_release (&display->base);
            }
        }
        cairo_surface_destroy (&surface->base);

        return _cairo_surface_create_in_error (status);
    }

    surface->owns_pixmap = TRUE;
    return &surface->base;
}

static cairo_bool_t
_cairo_xlib_surface_can_repaint_solid_pattern_surface (void *abstract_surface,
						       const cairo_solid_pattern_t *solid_pattern)
{
    cairo_xlib_surface_t *other = abstract_surface;
    return CAIRO_SURFACE_RENDER_HAS_COMPOSITE (other);
}

static cairo_status_t
_cairo_xlib_surface_set_matrix (cairo_xlib_display_t *display,
                                cairo_xlib_surface_t *surface,
				const cairo_matrix_t *matrix,
				double                xc,
				double                yc)
{
    XTransform xtransform;

    /* Casting between pixman_transform_t and XTransform is safe because
     * they happen to be the exact same type.
     */
    _cairo_matrix_to_pixman_matrix (matrix,
				    (pixman_transform_t *) &xtransform,
				    xc, yc);

    if (memcmp (&xtransform, &surface->xtransform, sizeof (XTransform)) == 0)
	return CAIRO_STATUS_SUCCESS;

    if (! CAIRO_SURFACE_RENDER_HAS_PICTURE_TRANSFORM (surface))
	return UNSUPPORTED ("XRender does not support picture transforms");

    XRenderSetPictureTransform (display->display, surface->src_picture, &xtransform);
    surface->xtransform = xtransform;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xlib_surface_set_filter (cairo_xlib_display_t *display,
                                cairo_xlib_surface_t *surface,
				cairo_filter_t	     filter)
{
    const char *render_filter;

    if (surface->filter == filter)
	return CAIRO_STATUS_SUCCESS;

    if (!CAIRO_SURFACE_RENDER_HAS_FILTERS (surface)) {
	if (filter == CAIRO_FILTER_FAST || filter == CAIRO_FILTER_NEAREST)
	    return CAIRO_STATUS_SUCCESS;

	return UNSUPPORTED ("XRender does not support filter");
    }

    switch (filter) {
    case CAIRO_FILTER_FAST:
	render_filter = FilterFast;
	break;
    case CAIRO_FILTER_GOOD:
	render_filter = FilterGood;
	break;
    case CAIRO_FILTER_BEST:
	render_filter = FilterBest;
	break;
    case CAIRO_FILTER_NEAREST:
	render_filter = FilterNearest;
	break;
    case CAIRO_FILTER_BILINEAR:
	render_filter = FilterBilinear;
	break;
    case CAIRO_FILTER_GAUSSIAN:
	/* XXX: The GAUSSIAN value has no implementation in cairo
	 * whatsoever, so it was really a mistake to have it in the
	 * API. We could fix this by officially deprecating it, or
	 * else inventing semantics and providing an actual
	 * implementation for it. */
    default:
	render_filter = FilterBest;
	break;
    }

    XRenderSetPictureFilter (display->display, surface->src_picture,
			     (char *) render_filter, NULL, 0);
    surface->filter = filter;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xlib_surface_set_repeat (cairo_xlib_surface_t	*surface,
				cairo_extend_t		 extend,
				unsigned long		*mask,
				XRenderPictureAttributes *pa)
{
    int repeat;

    if (surface->extend == extend)
	return CAIRO_STATUS_SUCCESS;

    switch (extend) {
    case CAIRO_EXTEND_NONE:
	repeat = RepeatNone;
	break;
    case CAIRO_EXTEND_REPEAT:
	repeat = RepeatNormal;
	break;
    case CAIRO_EXTEND_REFLECT:
	if (surface->buggy_pad_reflect)
	    return UNSUPPORTED ("buggy reflect");

	repeat = RepeatReflect;
	break;
    case CAIRO_EXTEND_PAD:
	if (surface->buggy_pad_reflect)
	    return UNSUPPORTED ("buggy pad");

	repeat = RepeatPad;
	break;
    default:
	ASSERT_NOT_REACHED;
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    *mask |= CPRepeat;
    pa->repeat = repeat;

    surface->extend = extend;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xlib_surface_set_component_alpha (cairo_xlib_surface_t *surface,
					 cairo_bool_t		ca,
					 unsigned long		*mask,
					 XRenderPictureAttributes *pa)
{
    if (surface->has_component_alpha == ca)
	return CAIRO_STATUS_SUCCESS;

    *mask |= CPComponentAlpha;
    pa->component_alpha = ca;

    surface->has_component_alpha = ca;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_xlib_surface_set_attributes (cairo_xlib_display_t             *display,
                                    cairo_xlib_surface_t	     *surface,
				    const cairo_surface_attributes_t *attributes,
				    double			      xc,
				    double			      yc)
{
    cairo_int_status_t status;
    XRenderPictureAttributes pa;
    unsigned long mask = 0;

    _cairo_xlib_surface_ensure_src_picture (display, surface);

    status = _cairo_xlib_surface_set_matrix (display, surface,
                                             &attributes->matrix, xc, yc);
    if (unlikely (status))
	return status;

    status = _cairo_xlib_surface_set_repeat (surface, attributes->extend,
					     &mask, &pa);
    if (unlikely (status))
	return status;

    status = _cairo_xlib_surface_set_component_alpha (surface,
						      attributes->has_component_alpha,
						      &mask, &pa);
    if (unlikely (status))
	return status;

    status = _cairo_xlib_surface_set_filter (display, surface, attributes->filter);
    if (unlikely (status))
	return status;

    if (mask)
	XRenderChangePicture (display->display, surface->src_picture, mask, &pa);

    return CAIRO_STATUS_SUCCESS;
}

/* Checks whether we can can directly draw from src to dst with
 * the core protocol: either with CopyArea or using src as a
 * a tile in a GC.
 */
static cairo_bool_t
_surfaces_compatible (cairo_xlib_surface_t *dst,
		      cairo_xlib_surface_t *src)
{
    /* same screen */
    if (! _cairo_xlib_surface_same_screen (dst, src))
	return FALSE;

    /* same depth (for core) */
    if (src->depth != dst->depth)
	return FALSE;

    /* if Render is supported, match picture formats */
    if (src->xrender_format != dst->xrender_format)
	return FALSE;
    else if (src->xrender_format != NULL)
	return TRUE;

    /* Without Render, match visuals instead */
    if (src->visual == dst->visual)
	return TRUE;

    return FALSE;
}

static cairo_bool_t
_surface_has_alpha (cairo_xlib_surface_t *surface)
{
    if (surface->xrender_format) {
	if (surface->xrender_format->type == PictTypeDirect &&
	    surface->xrender_format->direct.alphaMask != 0)
	    return TRUE;
	else
	    return FALSE;
    } else {
	/* In the no-render case, we never have alpha */
	return FALSE;
    }
}

/* Returns true if the given operator and alpha combination requires alpha
 * compositing to complete on source and destination surfaces with the same
 * format.  i.e. if a simple bitwise copy is not appropriate.
 */
static cairo_bool_t
_operator_needs_alpha_composite (cairo_operator_t op,
				 cairo_bool_t     surfaces_have_alpha)
{
    if (op == CAIRO_OPERATOR_SOURCE)
	return FALSE;

    if (op == CAIRO_OPERATOR_OVER ||
	op == CAIRO_OPERATOR_IN ||
	op == CAIRO_OPERATOR_ATOP)
	return surfaces_have_alpha;

    return TRUE;
}

/* There is a bug in most older X servers with compositing using a
 * untransformed repeating source pattern when the source is in off-screen
 * video memory, and another with repeated transformed images using a
 * general transform matrix. When these bugs could be triggered, we need a
 * fallback: in the common case where we have no transformation and the
 * source and destination have the same format/visual, we can do the
 * operation using the core protocol for the first bug, otherwise, we need
 * a software fallback.
 *
 * We can also often optimize a compositing operation by calling XCopyArea
 * for some common cases where there is no alpha compositing to be done.
 * We figure that out here as well.
 */
typedef enum {
    DO_RENDER,		/* use render */
    DO_XCOPYAREA,	/* core protocol XCopyArea optimization/fallback */
    DO_XTILE,		/* core protocol XSetTile optimization/fallback */
    DO_UNSUPPORTED	/* software fallback */
} composite_operation_t;

/* Initial check for the render bugs; we need to recheck for the
 * offscreen-memory bug after we turn patterns into surfaces, since that
 * may introduce a repeating pattern for gradient patterns.  We don't need
 * to check for the repeat+transform bug because gradient surfaces aren't
 * transformed.
 *
 * All we do here is reject cases where we *know* are going to
 * hit the bug and won't be able to use a core protocol fallback.
 */
static composite_operation_t
_categorize_composite_operation (cairo_xlib_surface_t *dst,
				 cairo_operator_t      op,
				 const cairo_pattern_t *src_pattern,
				 cairo_bool_t	       have_mask)

{
    if (!CAIRO_SURFACE_RENDER_SUPPORTS_OPERATOR (dst, op))
	return DO_UNSUPPORTED;

    if (! dst->buggy_repeat)
	return DO_RENDER;

    if (src_pattern->type != CAIRO_PATTERN_TYPE_SOLID &&
	src_pattern->extend == CAIRO_EXTEND_REPEAT)
    {
	/* Check for the bug with repeat patterns nad general transforms. */
	if (! _cairo_matrix_is_integer_translation (&src_pattern->matrix,
						    NULL, NULL))
	{
	    return DO_UNSUPPORTED;
	}

	if (have_mask ||
	    !(op == CAIRO_OPERATOR_SOURCE || op == CAIRO_OPERATOR_OVER))
	{
	    return DO_UNSUPPORTED;
	}

	if (src_pattern->type == CAIRO_PATTERN_TYPE_SURFACE) {
	    cairo_surface_pattern_t *surface_pattern = (cairo_surface_pattern_t *) src_pattern;

	    /* This is the case where we have the bug involving
	     * untransformed repeating source patterns with off-screen
	     * video memory; reject some cases where a core protocol
	     * fallback is impossible.
	     */
	    if (_cairo_surface_is_xlib (surface_pattern->surface)) {
		cairo_xlib_surface_t *src = (cairo_xlib_surface_t *) surface_pattern->surface;

		if (op == CAIRO_OPERATOR_OVER && _surface_has_alpha (src))
		    return DO_UNSUPPORTED;

		/* If these are on the same screen but otherwise incompatible,
		 * make a copy as core drawing can't cross depths and doesn't
		 * work right across visuals of the same depth
		 */
		if (_cairo_xlib_surface_same_screen (dst, src) &&
		    !_surfaces_compatible (dst, src))
		{
		    return DO_UNSUPPORTED;
		}
	    }
	}
    }

    return DO_RENDER;
}

/* Recheck for composite-repeat once we've turned patterns into Xlib surfaces
 * If we end up returning DO_UNSUPPORTED here, we're throwing away work we
 * did to turn gradients into a pattern, but most of the time we can handle
 * that case with core protocol fallback.
 *
 * Also check here if we can just use XCopyArea, instead of going through
 * Render.
 */
static composite_operation_t
_recategorize_composite_operation (cairo_xlib_surface_t	      *dst,
				   cairo_operator_t	       op,
				   cairo_xlib_surface_t	      *src,
				   cairo_surface_attributes_t *src_attr,
				   cairo_bool_t		       have_mask)
{
    /* Can we use the core protocol? */
    if (! have_mask &&
	_surfaces_compatible (src, dst) &&
	_cairo_matrix_is_integer_translation (&src_attr->matrix, NULL, NULL) &&
	! _operator_needs_alpha_composite (op, _surface_has_alpha (dst)))
    {
	if (src_attr->extend == CAIRO_EXTEND_NONE)
	    return DO_XCOPYAREA;

	if (dst->buggy_repeat && src_attr->extend == CAIRO_EXTEND_REPEAT)
	    return DO_XTILE;
    }

    if (dst->buggy_repeat && src_attr->extend == CAIRO_EXTEND_REPEAT &&
	    (src->width != 1 || src->height != 1))
	return DO_UNSUPPORTED;

    if (! CAIRO_SURFACE_RENDER_HAS_COMPOSITE (src))
	return DO_UNSUPPORTED;

    if (! CAIRO_SURFACE_RENDER_HAS_COMPOSITE (dst))
	return DO_UNSUPPORTED;

    return DO_RENDER;
}

static int
_render_operator (cairo_operator_t op)
{
    switch (op) {
    case CAIRO_OPERATOR_CLEAR:
	return PictOpClear;

    case CAIRO_OPERATOR_SOURCE:
	return PictOpSrc;
    case CAIRO_OPERATOR_OVER:
	return PictOpOver;
    case CAIRO_OPERATOR_IN:
	return PictOpIn;
    case CAIRO_OPERATOR_OUT:
	return PictOpOut;
    case CAIRO_OPERATOR_ATOP:
	return PictOpAtop;

    case CAIRO_OPERATOR_DEST:
	return PictOpDst;
    case CAIRO_OPERATOR_DEST_OVER:
	return PictOpOverReverse;
    case CAIRO_OPERATOR_DEST_IN:
	return PictOpInReverse;
    case CAIRO_OPERATOR_DEST_OUT:
	return PictOpOutReverse;
    case CAIRO_OPERATOR_DEST_ATOP:
	return PictOpAtopReverse;

    case CAIRO_OPERATOR_XOR:
	return PictOpXor;
    case CAIRO_OPERATOR_ADD:
	return PictOpAdd;
    case CAIRO_OPERATOR_SATURATE:
	return PictOpSaturate;

    case CAIRO_OPERATOR_MULTIPLY:
	return PictOpMultiply;
    case CAIRO_OPERATOR_SCREEN:
	return PictOpScreen;
    case CAIRO_OPERATOR_OVERLAY:
	return PictOpOverlay;
    case CAIRO_OPERATOR_DARKEN:
	return PictOpDarken;
    case CAIRO_OPERATOR_LIGHTEN:
	return PictOpLighten;
    case CAIRO_OPERATOR_COLOR_DODGE:
	return PictOpColorDodge;
    case CAIRO_OPERATOR_COLOR_BURN:
	return PictOpColorBurn;
    case CAIRO_OPERATOR_HARD_LIGHT:
	return PictOpHardLight;
    case CAIRO_OPERATOR_SOFT_LIGHT:
	return PictOpSoftLight;
    case CAIRO_OPERATOR_DIFFERENCE:
	return PictOpDifference;
    case CAIRO_OPERATOR_EXCLUSION:
	return PictOpExclusion;
    case CAIRO_OPERATOR_HSL_HUE:
	return PictOpHSLHue;
    case CAIRO_OPERATOR_HSL_SATURATION:
	return PictOpHSLSaturation;
    case CAIRO_OPERATOR_HSL_COLOR:
	return PictOpHSLColor;
    case CAIRO_OPERATOR_HSL_LUMINOSITY:
	return PictOpHSLLuminosity;

    default:
	ASSERT_NOT_REACHED;
	return PictOpOver;
    }
}

static cairo_int_status_t
_cairo_xlib_surface_acquire_pattern_surface (cairo_xlib_display_t *display,
                                             cairo_xlib_surface_t *dst,
					     const cairo_pattern_t *pattern,
					     int x, int y,
					     int width, int height,
					     cairo_xlib_surface_t **surface_out,
					     cairo_surface_attributes_t *attributes)
{
    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_LINEAR:
    case CAIRO_PATTERN_TYPE_RADIAL:
	{
	    cairo_gradient_pattern_t *gradient =
		(cairo_gradient_pattern_t *) pattern;
	    cairo_matrix_t matrix = pattern->matrix;
	    cairo_xlib_surface_t *surface;
	    char buf[CAIRO_STACK_BUFFER_SIZE];
	    XFixed *stops;
	    XRenderColor *colors;
	    XRenderPictFormat *format;
	    Picture picture;
	    unsigned int i;

	    if (dst->buggy_gradients)
		break;

	    if (gradient->n_stops < 2) /* becomes a solid */
		break;

	    if (gradient->n_stops < sizeof (buf) / (sizeof (XFixed) + sizeof (XRenderColor)))
	    {
		stops = (XFixed *) buf;
	    }
	    else
	    {
		stops =
		    _cairo_malloc_ab (gradient->n_stops,
				      sizeof (XFixed) + sizeof (XRenderColor));
		if (unlikely (stops == NULL))
		    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    }

	    colors = (XRenderColor *) (stops + gradient->n_stops);
	    for (i = 0; i < gradient->n_stops; i++) {
		stops[i] =
		    _cairo_fixed_16_16_from_double (gradient->stops[i].offset);

		colors[i].red   = gradient->stops[i].color.red_short;
		colors[i].green = gradient->stops[i].color.green_short;
		colors[i].blue  = gradient->stops[i].color.blue_short;
		colors[i].alpha = gradient->stops[i].color.alpha_short;
	    }

#if 0
	    /* For some weird reason the X server is sometimes getting
	     * CreateGradient requests with bad length. So far I've only seen
	     * XRenderCreateLinearGradient request with 4 stops sometime end up
	     * with length field matching 0 stops at the server side. I've
	     * looked at the libXrender code and I can't see anything that
	     * could cause this behavior. However, for some reason having a
	     * XSync call here seems to avoid the issue so I'll keep it here
	     * until it's solved.
	     */
	    XSync (display->display, False);
#endif

	    if (pattern->type == CAIRO_PATTERN_TYPE_LINEAR) {
		cairo_linear_pattern_t *linear = (cairo_linear_pattern_t *) pattern;
		XLinearGradient grad;

		cairo_fixed_t xdim, ydim;

		xdim = linear->p2.x - linear->p1.x;
		ydim = linear->p2.y - linear->p1.y;

		/*
		 * Transform the matrix to avoid overflow when converting between
		 * cairo_fixed_t and pixman_fixed_t (without incurring performance
		 * loss when the transformation is unnecessary).
		 *
		 * XXX: Consider converting out-of-range co-ordinates and transforms.
		 * Having a function to compute the required transformation to
		 * "normalize" a given bounding box would be generally useful -
		 * cf linear patterns, gradient patterns, surface patterns...
		 */
#define PIXMAN_MAX_INT ((pixman_fixed_1 >> 1) - pixman_fixed_e) /* need to ensure deltas also fit */
		if (_cairo_fixed_integer_ceil (xdim) > PIXMAN_MAX_INT ||
		    _cairo_fixed_integer_ceil (ydim) > PIXMAN_MAX_INT)
		{
		    double sf;

		    if (xdim > ydim)
			sf = PIXMAN_MAX_INT / _cairo_fixed_to_double (xdim);
		    else
			sf = PIXMAN_MAX_INT / _cairo_fixed_to_double (ydim);

		    grad.p1.x = _cairo_fixed_16_16_from_double (_cairo_fixed_to_double (linear->p1.x) * sf);
		    grad.p1.y = _cairo_fixed_16_16_from_double (_cairo_fixed_to_double (linear->p1.y) * sf);
		    grad.p2.x = _cairo_fixed_16_16_from_double (_cairo_fixed_to_double (linear->p2.x) * sf);
		    grad.p2.y = _cairo_fixed_16_16_from_double (_cairo_fixed_to_double (linear->p2.y) * sf);

		    cairo_matrix_scale (&matrix, sf, sf);
		}
		else
		{
		    grad.p1.x = _cairo_fixed_to_16_16 (linear->p1.x);
		    grad.p1.y = _cairo_fixed_to_16_16 (linear->p1.y);
		    grad.p2.x = _cairo_fixed_to_16_16 (linear->p2.x);
		    grad.p2.y = _cairo_fixed_to_16_16 (linear->p2.y);
		}

		picture = XRenderCreateLinearGradient (display->display, &grad,
						       stops, colors,
						       gradient->n_stops);
	    } else {
		cairo_radial_pattern_t *radial = (cairo_radial_pattern_t *) pattern;
		XRadialGradient grad;

		grad.inner.x = _cairo_fixed_to_16_16 (radial->c1.x);
		grad.inner.y = _cairo_fixed_to_16_16 (radial->c1.y);
		grad.inner.radius = _cairo_fixed_to_16_16 (radial->r1);

		grad.outer.x = _cairo_fixed_to_16_16 (radial->c2.x);
		grad.outer.y = _cairo_fixed_to_16_16 (radial->c2.y);
		grad.outer.radius = _cairo_fixed_to_16_16 (radial->r2);

		picture = XRenderCreateRadialGradient (display->display, &grad,
						       stops, colors,
						       gradient->n_stops);

	    }

	    if (stops != (XFixed *) buf)
		free (stops);

	    if (unlikely (picture == None))
		return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	    /* Wrap the remote Picture in an xlib surface. */
	    format = _cairo_xlib_display_get_xrender_format (display,
							     CAIRO_FORMAT_ARGB32);

	    surface = (cairo_xlib_surface_t *)
		_cairo_xlib_surface_create_internal (dst->screen, None,
						     NULL, format,
                                                     /* what could possibly go wrong? */
						     XLIB_COORD_MAX, XLIB_COORD_MAX, 32);
	    if (unlikely (surface->base.status)) {
		XRenderFreePicture (display->display, picture);
		return surface->base.status;
	    }

	    surface->src_picture = picture;

	    attributes->matrix   = matrix;
	    attributes->extend   = pattern->extend;
	    attributes->filter   = CAIRO_FILTER_NEAREST;
	    attributes->x_offset = 0;
	    attributes->y_offset = 0;
	    attributes->has_component_alpha = FALSE;

	    *surface_out = surface;
	    return CAIRO_STATUS_SUCCESS;
	}
    default:
	ASSERT_NOT_REACHED;
    case CAIRO_PATTERN_TYPE_SOLID:
    case CAIRO_PATTERN_TYPE_SURFACE:
	break;
    }

    return _cairo_pattern_acquire_surface (pattern, &dst->base,
					   x, y, width, height,
					   dst->buggy_pad_reflect ?
					   CAIRO_PATTERN_ACQUIRE_NO_REFLECT :
					   CAIRO_PATTERN_ACQUIRE_NONE,
					   (cairo_surface_t **) surface_out,
					   attributes);
}

static cairo_int_status_t
_cairo_xlib_surface_acquire_pattern_surfaces (cairo_xlib_display_t       *display,
                                              cairo_xlib_surface_t	 *dst,
					      const cairo_pattern_t	 *src,
					      const cairo_pattern_t	 *mask,
					      int			 src_x,
					      int			 src_y,
					      int			 mask_x,
					      int			 mask_y,
					      unsigned int		 width,
					      unsigned int		 height,
					      cairo_xlib_surface_t	 **src_out,
					      cairo_xlib_surface_t	 **mask_out,
					      cairo_surface_attributes_t *src_attr,
					      cairo_surface_attributes_t *mask_attr)
{
    if (! dst->buggy_gradients &&
	(src->type == CAIRO_PATTERN_TYPE_LINEAR		 ||
	 src->type == CAIRO_PATTERN_TYPE_RADIAL		 ||
	 (mask && (mask->type == CAIRO_PATTERN_TYPE_LINEAR ||
		   mask->type == CAIRO_PATTERN_TYPE_RADIAL))))
    {
	cairo_int_status_t status;

	status = _cairo_xlib_surface_acquire_pattern_surface (display,
                                                              dst, src,
							      src_x, src_y,
							      width, height,
							      src_out,
							      src_attr);
	if (unlikely (status))
	    return status;

	if (mask) {
	    status = _cairo_xlib_surface_acquire_pattern_surface (display,
                                                                  dst, mask,
								  mask_x,
								  mask_y,
								  width,
								  height,
								  mask_out,
								  mask_attr);
	    if (unlikely (status)) {
		_cairo_pattern_release_surface (src, &(*src_out)->base,
						src_attr);
		return status;
	    }
	} else {
	    *mask_out = NULL;
	}

	return CAIRO_STATUS_SUCCESS;
    }

    return _cairo_pattern_acquire_surfaces (src, mask,
					    &dst->base,
					    src_x, src_y,
					    mask_x, mask_y,
					    width, height,
					    dst->buggy_pad_reflect ?
					    CAIRO_PATTERN_ACQUIRE_NO_REFLECT :
					    CAIRO_PATTERN_ACQUIRE_NONE,
					    (cairo_surface_t **) src_out,
					    (cairo_surface_t **) mask_out,
					    src_attr, mask_attr);
}

static cairo_int_status_t
_cairo_xlib_surface_upload(cairo_xlib_surface_t *surface,
			   cairo_operator_t op,
			   const cairo_pattern_t *pattern,
			   int src_x, int src_y,
			   int dst_x, int dst_y,
			   unsigned int width,
			   unsigned int height,
			   cairo_region_t *clip_region)
{
    cairo_image_surface_t *image;
    cairo_rectangle_int_t extents;
    cairo_status_t status;
    int tx, ty;

    if (pattern->type != CAIRO_PATTERN_TYPE_SURFACE)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    image = (cairo_image_surface_t *) ((cairo_surface_pattern_t *) pattern)->surface;
    if (image->base.type != CAIRO_SURFACE_TYPE_IMAGE)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (! (op == CAIRO_OPERATOR_SOURCE ||
	   (op == CAIRO_OPERATOR_OVER &&
	    (image->base.content & CAIRO_CONTENT_ALPHA) == 0)))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (image->base.backend->type != CAIRO_SURFACE_TYPE_IMAGE) {
	if (image->base.backend->type == CAIRO_INTERNAL_SURFACE_TYPE_SNAPSHOT) {
	    image = (cairo_image_surface_t *) ((cairo_surface_snapshot_t *) image)->target;
	    extents.x = extents.y = 0;
	    extents.width = image->width;
	    extents.height = image->height;
	} else if (image->base.backend->type == CAIRO_SURFACE_TYPE_SUBSURFACE) {
	    cairo_surface_subsurface_t *sub = (cairo_surface_subsurface_t *) image;
	    image = (cairo_image_surface_t *) sub->target;
	    src_x += sub->extents.x;
	    src_y += sub->extents.y;
	    extents = sub->extents;
	} else {
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}
    } else {
	extents.x = extents.y = 0;
	extents.width = image->width;
	extents.height = image->height;
    }

    if (image->format == CAIRO_FORMAT_INVALID)
	return CAIRO_INT_STATUS_UNSUPPORTED;
    if (image->depth != surface->depth)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (! _cairo_matrix_is_integer_translation (&pattern->matrix, &tx, &ty))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    src_x += tx;
    src_y += ty;

    /* XXX for EXTEND_NONE perform unbounded fixups? */
    if (src_x < extents.x ||
	src_y < extents.y ||
	src_x + width  > (unsigned) extents.width ||
	src_y + height > (unsigned) extents.height)
    {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    status = cairo_device_acquire (surface->base.device);
    if (unlikely (status))
        return status;

    if (clip_region != NULL) {
	int n, num_rect;

	src_x -= dst_x;
	src_y -= dst_y;

	num_rect = cairo_region_num_rectangles (clip_region);
	for (n = 0; n < num_rect; n++) {
	    cairo_rectangle_int_t rect;

	    cairo_region_get_rectangle (clip_region, n, &rect);
	    status = _draw_image_surface (surface, image,
					  rect.x + src_x, rect.y + src_y,
					  rect.width, rect.height,
					  rect.x, rect.y);
	    if (unlikely (status))
		break;
	}
    } else {
	status = _draw_image_surface (surface, image,
				      src_x, src_y,
				      width, height,
				      dst_x, dst_y);
    }

    cairo_device_release (surface->base.device);

    return status;
}

static cairo_int_status_t
_cairo_xlib_surface_composite (cairo_operator_t		op,
			       const cairo_pattern_t	*src_pattern,
			       const cairo_pattern_t	*mask_pattern,
			       void			*abstract_dst,
			       int			src_x,
			       int			src_y,
			       int			mask_x,
			       int			mask_y,
			       int			dst_x,
			       int			dst_y,
			       unsigned int		width,
			       unsigned int		height,
			       cairo_region_t		*clip_region)
{
    cairo_surface_attributes_t	src_attr, mask_attr;
    cairo_xlib_surface_t	*dst = abstract_dst;
    cairo_xlib_surface_t	*src;
    cairo_xlib_surface_t	*mask;
    cairo_xlib_display_t        *display;
    cairo_int_status_t		status;
    composite_operation_t       operation;
    int				itx, ity;
    cairo_bool_t		is_integer_translation;
    GC				gc;

    if (mask_pattern != NULL && ! CAIRO_SURFACE_RENDER_HAS_COMPOSITE (dst))
	return UNSUPPORTED ("no support for masks");

    operation = _categorize_composite_operation (dst, op, src_pattern,
						 mask_pattern != NULL);
    if (operation == DO_UNSUPPORTED)
	return UNSUPPORTED ("unsupported operation");

    X_DEBUG ((display->display, "composite (dst=%x)", (unsigned int) dst->drawable));

    if (mask_pattern == NULL) {
	/* Can we do a simple upload in-place? */
	status = _cairo_xlib_surface_upload(dst, op, src_pattern,
					    src_x, src_y,
					    dst_x, dst_y,
					    width, height,
					    clip_region);
	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    return status;
    }

    status = _cairo_xlib_display_acquire (dst-> base.device, &display);
    if (unlikely (status))
        return status;

    status =
	_cairo_xlib_surface_acquire_pattern_surfaces (display, dst,
						      src_pattern, mask_pattern,
						      src_x, src_y,
						      mask_x, mask_y,
						      width, height,
						      &src, &mask,
						      &src_attr, &mask_attr);
    if (unlikely (status))
        goto BAIL0;

    /* check for fallback surfaces that we cannot handle ... */
    assert (_cairo_surface_is_xlib (&src->base));
    assert (mask == NULL || _cairo_surface_is_xlib (&mask->base));

    if (mask != NULL && ! CAIRO_SURFACE_RENDER_HAS_COMPOSITE (mask)) {
	status = UNSUPPORTED ("unsupported mask");
	goto BAIL;
    }

    operation = _recategorize_composite_operation (dst, op, src, &src_attr,
						   mask_pattern != NULL);
    if (operation == DO_UNSUPPORTED) {
	status = UNSUPPORTED ("unsupported operation");
	goto BAIL;
    }

    switch (operation)
    {
    case DO_RENDER:
	status = _cairo_xlib_surface_set_attributes (display,
                                                     src, &src_attr,
						     dst_x + width / 2.,
						     dst_y + height / 2.);
	if (unlikely (status))
	    goto BAIL;

	status = _cairo_xlib_surface_set_clip_region (dst, clip_region);
	if (unlikely (status))
	    goto BAIL;

	_cairo_xlib_surface_ensure_dst_picture (display, dst);
	if (mask) {
	    status = _cairo_xlib_surface_set_attributes (display,
                                                         mask, &mask_attr,
							 dst_x + width / 2.,
							 dst_y + height/ 2.);
	    if (unlikely (status))
		goto BAIL;

	    XRenderComposite (display->display,
			      _render_operator (op),
			      src->src_picture,
			      mask->src_picture,
			      dst->dst_picture,
			      src_x + src_attr.x_offset,
			      src_y + src_attr.y_offset,
			      mask_x + mask_attr.x_offset,
			      mask_y + mask_attr.y_offset,
			      dst_x, dst_y,
			      width, height);
	} else {
	    XRenderComposite (display->display,
			      _render_operator (op),
			      src->src_picture,
			      0,
			      dst->dst_picture,
			      src_x + src_attr.x_offset,
			      src_y + src_attr.y_offset,
			      0, 0,
			      dst_x, dst_y,
			      width, height);
	}

	break;

    case DO_XCOPYAREA:
	status = _cairo_xlib_surface_get_gc (display, dst, &gc);
	if (unlikely (status))
	    goto BAIL;

	is_integer_translation =
	    _cairo_matrix_is_integer_translation (&src_attr.matrix, &itx, &ity);
	/* This is a pre-condition for DO_XCOPYAREA. */
	assert (is_integer_translation);

	if (clip_region == NULL) {
	    XCopyArea (display->display, src->drawable, dst->drawable, gc,
		       src_x + src_attr.x_offset + itx,
		       src_y + src_attr.y_offset + ity,
		       width, height,
		       dst_x, dst_y);
	} else {
	    int n, num_rects, x, y;

	    x = src_x + src_attr.x_offset + itx - dst_x;
	    y = src_y + src_attr.y_offset + ity - dst_y;

	    num_rects = cairo_region_num_rectangles (clip_region);
	    for (n = 0; n < num_rects; n++) {
		cairo_rectangle_int_t rect;

		cairo_region_get_rectangle (clip_region, n, &rect);
		XCopyArea (display->display, src->drawable, dst->drawable, gc,
			   rect.x + x, rect.y + y,
			   rect.width, rect.height,
			   rect.x, rect.y);
	    }
	}

	_cairo_xlib_surface_put_gc (display, dst, gc);
	break;

    case DO_XTILE:
	/* This case is only used for bug fallbacks, though we also use it for
	 * the case where we don't have the RENDER extension, by forcing
	 * buggy_repeat to TRUE.
	 *
	 * We've checked that we have a repeating unscaled source in
	 * _recategorize_composite_operation.
	 */

	status = _cairo_xlib_surface_get_gc (display, dst, &gc);
	if (unlikely (status))
	    goto BAIL;

	is_integer_translation =
	    _cairo_matrix_is_integer_translation (&src_attr.matrix, &itx, &ity);
	/* This is a pre-condition for DO_XTILE. */
	assert (is_integer_translation);

	XSetTSOrigin (display->display, gc,
		      - (itx + src_attr.x_offset), - (ity + src_attr.y_offset));
	XSetTile (display->display, gc, src->drawable);

	if (clip_region == NULL) {
	    XFillRectangle (display->display, dst->drawable, gc,
			    dst_x, dst_y, width, height);
	} else {
	    int n, num_rects;

	    num_rects = cairo_region_num_rectangles (clip_region);
	    for (n = 0; n < num_rects; n++) {
		cairo_rectangle_int_t rect;

		cairo_region_get_rectangle (clip_region, n, &rect);
		XFillRectangle (display->display, dst->drawable, gc,
				rect.x, rect.y, rect.width, rect.height);
	    }
	}

	_cairo_xlib_surface_put_gc (display, dst, gc);
	break;

    case DO_UNSUPPORTED:
    default:
	ASSERT_NOT_REACHED;
    }

    if (!_cairo_operator_bounded_by_source (op))
      status = _cairo_surface_composite_fixup_unbounded (&dst->base,
							 &src_attr, src->width, src->height,
							 mask ? &mask_attr : NULL,
							 mask ? mask->width : 0,
							 mask ? mask->height : 0,
							 src_x, src_y,
							 mask_x, mask_y,
							 dst_x, dst_y, width, height,
							 clip_region);

 BAIL:
    if (mask)
	_cairo_pattern_release_surface (mask_pattern, &mask->base, &mask_attr);

    _cairo_pattern_release_surface (src_pattern, &src->base, &src_attr);

 BAIL0:
    cairo_device_release (&display->base);

    return status;
}

/* XXX move this out of core and into acquire_pattern_surface() above. */
static cairo_int_status_t
_cairo_xlib_surface_solid_fill_rectangles (cairo_xlib_surface_t    *surface,
					   const cairo_color_t     *color,
					   cairo_rectangle_int_t   *rects,
					   int			   num_rects)
{
    cairo_status_t status;
    cairo_solid_pattern_t solid;
    cairo_surface_t *solid_surface = NULL;
    cairo_surface_attributes_t attrs;
    cairo_xlib_display_t *display;
    GC gc;
    int i;

    _cairo_pattern_init_solid (&solid, color);

    status = _cairo_xlib_display_acquire (surface->base.device, &display);
    if (unlikely (status))
        return status;

    status = _cairo_xlib_surface_get_gc (display, surface, &gc);
    if (unlikely (status))
        return status;

    X_DEBUG ((display->display, "solid_fill_rectangles (dst=%x)", (unsigned int) surface->drawable));

    status = _cairo_pattern_acquire_surface (&solid.base, &surface->base,
					     0, 0,
					     ARRAY_LENGTH (dither_pattern[0]),
					     ARRAY_LENGTH (dither_pattern),
					     CAIRO_PATTERN_ACQUIRE_NONE,
					     &solid_surface,
					     &attrs);
    if (unlikely (status)) {
	_cairo_xlib_surface_put_gc (display, surface, gc);
        cairo_device_release (&display->base);
	return status;
    }

    assert (_cairo_surface_is_xlib (solid_surface));

    XSetTSOrigin (display->display, gc,
		  - (surface->base.device_transform.x0 + attrs.x_offset),
		  - (surface->base.device_transform.y0 + attrs.y_offset));
    XSetTile (display->display, gc,
	      ((cairo_xlib_surface_t *) solid_surface)->drawable);

    for (i = 0; i < num_rects; i++) {
	XFillRectangle (display->display, surface->drawable, gc,
			rects[i].x, rects[i].y,
			rects[i].width, rects[i].height);
    }

    _cairo_xlib_surface_put_gc (display, surface, gc);

    _cairo_pattern_release_surface (&solid.base, solid_surface, &attrs);
    
    cairo_device_release (&display->base);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_xlib_surface_fill_rectangles (void		     *abstract_surface,
				     cairo_operator_t	      op,
				     const cairo_color_t     *color,
				     cairo_rectangle_int_t   *rects,
				     int		      num_rects)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    cairo_xlib_display_t *display;
    XRenderColor render_color;
    cairo_status_t status;
    int i;

    if (!CAIRO_SURFACE_RENDER_SUPPORTS_OPERATOR (surface, op))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (!CAIRO_SURFACE_RENDER_HAS_FILL_RECTANGLES (surface)) {
	if (op == CAIRO_OPERATOR_CLEAR ||
	    ((op == CAIRO_OPERATOR_SOURCE || op == CAIRO_OPERATOR_OVER) &&
	     CAIRO_COLOR_IS_OPAQUE (color)))
	{
	    return _cairo_xlib_surface_solid_fill_rectangles (surface, color,
							      rects, num_rects);
	}

	return UNSUPPORTED ("no support for FillRectangles with this op");
    }

    status = _cairo_xlib_display_acquire (surface->base.device, &display);
    if (unlikely (status))
        return status;

    X_DEBUG ((display->display, "fill_rectangles (dst=%x)", (unsigned int) surface->drawable));

    render_color.red   = color->red_short;
    render_color.green = color->green_short;
    render_color.blue  = color->blue_short;
    render_color.alpha = color->alpha_short;

    status = _cairo_xlib_surface_set_clip_region (surface, NULL);
    assert (status == CAIRO_STATUS_SUCCESS);

    _cairo_xlib_surface_ensure_dst_picture (display, surface);
    if (num_rects == 1) {
	/* Take advantage of the protocol compaction that libXrender performs
	 * to amalgamate sequences of XRenderFillRectangle().
	 */
	XRenderFillRectangle (display->display,
			      _render_operator (op),
			      surface->dst_picture,
			      &render_color,
			      rects->x,
			      rects->y,
			      rects->width,
			      rects->height);
    } else {
	XRectangle static_xrects[CAIRO_STACK_ARRAY_LENGTH (XRectangle)];
	XRectangle *xrects = static_xrects;

	if (num_rects > ARRAY_LENGTH (static_xrects)) {
	    xrects = _cairo_malloc_ab (num_rects, sizeof (XRectangle));
	    if (unlikely (xrects == NULL)) {
		status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
                goto BAIL;
            }
	}

	for (i = 0; i < num_rects; i++) {
	    xrects[i].x = rects[i].x;
	    xrects[i].y = rects[i].y;
	    xrects[i].width = rects[i].width;
	    xrects[i].height = rects[i].height;
	}

	XRenderFillRectangles (display->display,
			       _render_operator (op),
			       surface->dst_picture,
			       &render_color, xrects, num_rects);

	if (xrects != static_xrects)
	    free (xrects);
    }

BAIL:
    cairo_device_release (&display->base);
    return status;
}

#define CAIRO_FIXED_16_16_MIN -32768
#define CAIRO_FIXED_16_16_MAX 32767

static cairo_bool_t
_line_exceeds_16_16 (const cairo_line_t *line)
{
    return
	line->p1.x < _cairo_fixed_from_int (CAIRO_FIXED_16_16_MIN) ||
	line->p1.x > _cairo_fixed_from_int (CAIRO_FIXED_16_16_MAX) ||
	line->p2.x < _cairo_fixed_from_int (CAIRO_FIXED_16_16_MIN) ||
	line->p2.x > _cairo_fixed_from_int (CAIRO_FIXED_16_16_MAX) ||
	line->p1.y < _cairo_fixed_from_int (CAIRO_FIXED_16_16_MIN) ||
	line->p1.y > _cairo_fixed_from_int (CAIRO_FIXED_16_16_MAX) ||
	line->p2.y < _cairo_fixed_from_int (CAIRO_FIXED_16_16_MIN) ||
	line->p2.y > _cairo_fixed_from_int (CAIRO_FIXED_16_16_MAX);
}

static void
_project_line_x_onto_16_16 (const cairo_line_t *line,
			    cairo_fixed_t top,
			    cairo_fixed_t bottom,
			    XLineFixed *out)
{
    cairo_point_double_t p1, p2;
    double m;

    p1.x = _cairo_fixed_to_double (line->p1.x);
    p1.y = _cairo_fixed_to_double (line->p1.y);

    p2.x = _cairo_fixed_to_double (line->p2.x);
    p2.y = _cairo_fixed_to_double (line->p2.y);

    m = (p2.x - p1.x) / (p2.y - p1.y);
    out->p1.x = _cairo_fixed_16_16_from_double (p1.x + m * _cairo_fixed_to_double (top - line->p1.y));
    out->p2.x = _cairo_fixed_16_16_from_double (p1.x + m * _cairo_fixed_to_double (bottom - line->p1.y));
}

static cairo_int_status_t
_cairo_xlib_surface_composite_trapezoids (cairo_operator_t	op,
					  const cairo_pattern_t	*pattern,
					  void			*abstract_dst,
					  cairo_antialias_t	antialias,
					  int			src_x,
					  int			src_y,
					  int			dst_x,
					  int			dst_y,
					  unsigned int		width,
					  unsigned int		height,
					  cairo_trapezoid_t	*traps,
					  int			num_traps,
					  cairo_region_t	*clip_region)
{
    cairo_surface_attributes_t	attributes;
    cairo_xlib_surface_t	*dst = abstract_dst;
    cairo_xlib_surface_t	*src;
    cairo_xlib_display_t        *display;
    cairo_int_status_t		status;
    composite_operation_t       operation;
    int				render_reference_x, render_reference_y;
    int				render_src_x, render_src_y;
    XRenderPictFormat		*pict_format;
    XTrapezoid xtraps_stack[CAIRO_STACK_ARRAY_LENGTH (XTrapezoid)];
    XTrapezoid *xtraps = xtraps_stack;
    int i;

    if (! CAIRO_SURFACE_RENDER_HAS_TRAPEZOIDS (dst))
	return UNSUPPORTED ("XRender does not support CompositeTrapezoids");

    operation = _categorize_composite_operation (dst, op, pattern, TRUE);
    if (operation == DO_UNSUPPORTED)
	return UNSUPPORTED ("unsupported operation");

    status = _cairo_xlib_display_acquire (dst->base.device, &display);
    if (unlikely (status))
        return status;

    X_DEBUG ((display->display, "composite_trapezoids (dst=%x)", (unsigned int) dst->drawable));

    status = _cairo_xlib_surface_acquire_pattern_surface (display,
                                                          dst,
							  pattern,
							  src_x, src_y,
							  width, height,
							  &src, &attributes);
    if (unlikely (status))
	goto BAIL0;

    operation = _recategorize_composite_operation (dst, op, src,
						   &attributes, TRUE);
    if (operation == DO_UNSUPPORTED) {
	status = UNSUPPORTED ("unsupported operation");
	goto BAIL;
    }

    switch (antialias) {
    case CAIRO_ANTIALIAS_NONE:
	pict_format =
	    _cairo_xlib_display_get_xrender_format (display,
						    CAIRO_FORMAT_A1);
	break;
    case CAIRO_ANTIALIAS_GRAY:
    case CAIRO_ANTIALIAS_SUBPIXEL:
    case CAIRO_ANTIALIAS_DEFAULT:
    default:
	pict_format =
	    _cairo_xlib_display_get_xrender_format (display,
						    CAIRO_FORMAT_A8);
	break;
    }

    status = _cairo_xlib_surface_set_clip_region (dst, clip_region);
    if (unlikely (status))
	goto BAIL;

    _cairo_xlib_surface_ensure_dst_picture (display, dst);
    _cairo_xlib_surface_set_precision (display, dst, antialias);

    status = _cairo_xlib_surface_set_attributes (display,
                                                 src, &attributes,
						 dst_x + width / 2.,
						 dst_y + height / 2.);
    if (unlikely (status))
	goto BAIL;

    if (num_traps > ARRAY_LENGTH (xtraps_stack)) {
	xtraps = _cairo_malloc_ab (num_traps, sizeof (XTrapezoid));
	if (unlikely (xtraps == NULL)) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto BAIL;
	}
    }

    for (i = 0; i < num_traps; i++) {
	/* top/bottom will be clamped to surface bounds */
	xtraps[i].top = _cairo_fixed_to_16_16(traps[i].top);
	xtraps[i].bottom = _cairo_fixed_to_16_16(traps[i].bottom);

	/* However, all the other coordinates will have been left untouched so
	 * as not to introduce numerical error. Recompute them if they
	 * exceed the 16.16 limits.
	 */
	if (unlikely (_line_exceeds_16_16 (&traps[i].left))) {
	    _project_line_x_onto_16_16 (&traps[i].left,
					traps[i].top,
					traps[i].bottom,
					&xtraps[i].left);
	    xtraps[i].left.p1.y = xtraps[i].top;
	    xtraps[i].left.p2.y = xtraps[i].bottom;
	} else {
	    xtraps[i].left.p1.x = _cairo_fixed_to_16_16(traps[i].left.p1.x);
	    xtraps[i].left.p1.y = _cairo_fixed_to_16_16(traps[i].left.p1.y);
	    xtraps[i].left.p2.x = _cairo_fixed_to_16_16(traps[i].left.p2.x);
	    xtraps[i].left.p2.y = _cairo_fixed_to_16_16(traps[i].left.p2.y);
	}

	if (unlikely (_line_exceeds_16_16 (&traps[i].right))) {
	    _project_line_x_onto_16_16 (&traps[i].right,
					traps[i].top,
					traps[i].bottom,
					&xtraps[i].right);
	    xtraps[i].right.p1.y = xtraps[i].top;
	    xtraps[i].right.p2.y = xtraps[i].bottom;
	} else {
	    xtraps[i].right.p1.x = _cairo_fixed_to_16_16(traps[i].right.p1.x);
	    xtraps[i].right.p1.y = _cairo_fixed_to_16_16(traps[i].right.p1.y);
	    xtraps[i].right.p2.x = _cairo_fixed_to_16_16(traps[i].right.p2.x);
	    xtraps[i].right.p2.y = _cairo_fixed_to_16_16(traps[i].right.p2.y);
	}
    }

    if (xtraps[0].left.p1.y < xtraps[0].left.p2.y) {
	render_reference_x = _cairo_fixed_16_16_floor (xtraps[0].left.p1.x);
	render_reference_y = _cairo_fixed_16_16_floor (xtraps[0].left.p1.y);
    } else {
	render_reference_x = _cairo_fixed_16_16_floor (xtraps[0].left.p2.x);
	render_reference_y = _cairo_fixed_16_16_floor (xtraps[0].left.p2.y);
    }

    render_src_x = src_x + render_reference_x - dst_x;
    render_src_y = src_y + render_reference_y - dst_y;

    XRenderCompositeTrapezoids (display->display,
				_render_operator (op),
				src->src_picture, dst->dst_picture,
				pict_format,
				render_src_x + attributes.x_offset,
				render_src_y + attributes.y_offset,
				xtraps, num_traps);

    if (xtraps != xtraps_stack)
	free (xtraps);

    if (! _cairo_operator_bounded_by_mask (op)) {
	cairo_traps_t _traps;
	cairo_box_t box;
	cairo_rectangle_int_t extents;

	/* XRenderCompositeTrapezoids() creates a mask only large enough for the
	 * trapezoids themselves, but if the operator is unbounded, then we need
	 * to actually composite all the way out to the bounds.
	 */
	/* XXX: update the interface to pass composite rects */
	_traps.traps = traps;
	_traps.num_traps = num_traps;
	_cairo_traps_extents (&_traps, &box);
	_cairo_box_round_to_rectangle (&box, &extents);

	status = _cairo_surface_composite_shape_fixup_unbounded (&dst->base,
								 &attributes,
								 src->width, src->height,
								 extents.width, extents.height,
								 src_x, src_y,
								 -extents.x + dst_x, -extents.y + dst_y,
								 dst_x, dst_y,
								 width, height,
								 clip_region);
    }

 BAIL:
    _cairo_pattern_release_surface (pattern, &src->base, &attributes);
 BAIL0:
    cairo_device_release (&display->base);

    return status;
}

static cairo_bool_t
_cairo_xlib_surface_get_extents (void		         *abstract_surface,
				 cairo_rectangle_int_t   *rectangle)
{
    cairo_xlib_surface_t *surface = abstract_surface;

    rectangle->x = 0;
    rectangle->y = 0;

    rectangle->width  = surface->width;
    rectangle->height = surface->height;

    return TRUE;
}

static void
_cairo_xlib_surface_get_font_options (void                  *abstract_surface,
				      cairo_font_options_t  *options)
{
    cairo_xlib_surface_t *surface = abstract_surface;

    *options = *_cairo_xlib_screen_get_font_options (surface->screen);
}

static void
_cairo_xlib_surface_scaled_font_fini (cairo_scaled_font_t *scaled_font);

static void
_cairo_xlib_surface_scaled_glyph_fini (cairo_scaled_glyph_t *scaled_glyph,
				       cairo_scaled_font_t  *scaled_font);

static cairo_bool_t
_cairo_xlib_surface_is_similar (void		*surface_a,
	                        void		*surface_b)
{
    return _cairo_xlib_surface_same_screen (surface_a, surface_b);
}

static const cairo_surface_backend_t cairo_xlib_surface_backend = {
    CAIRO_SURFACE_TYPE_XLIB,
    _cairo_xlib_surface_create_similar,
    _cairo_xlib_surface_finish,
    _cairo_xlib_surface_acquire_source_image,
    _cairo_xlib_surface_release_source_image,
    _cairo_xlib_surface_acquire_dest_image,
    _cairo_xlib_surface_release_dest_image,
    _cairo_xlib_surface_clone_similar,
    _cairo_xlib_surface_composite,
    _cairo_xlib_surface_fill_rectangles,
    _cairo_xlib_surface_composite_trapezoids,
    NULL, /* create_span_renderer */
    NULL, /* check_span_renderer */
    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_xlib_surface_get_extents,
    NULL, /* old_show_glyphs */
    _cairo_xlib_surface_get_font_options,
    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */
    _cairo_xlib_surface_scaled_font_fini,
    _cairo_xlib_surface_scaled_glyph_fini,

    NULL, /* paint */
    NULL, /* mask */
    NULL, /* stroke */
    NULL, /* fill */
    _cairo_xlib_surface_show_glyphs,

    _cairo_xlib_surface_snapshot,
    _cairo_xlib_surface_is_similar,

    NULL, /* fill_stroke */

    _cairo_xlib_surface_create_solid_pattern_surface,
    _cairo_xlib_surface_can_repaint_solid_pattern_surface
};

/**
 * _cairo_surface_is_xlib:
 * @surface: a #cairo_surface_t
 *
 * Checks if a surface is a #cairo_xlib_surface_t
 *
 * Return value: True if the surface is an xlib surface
 **/
static cairo_bool_t
_cairo_surface_is_xlib (cairo_surface_t *surface)
{
    return surface->backend == &cairo_xlib_surface_backend;
}

/* callback from CloseDisplay */
static void
_cairo_xlib_surface_detach_display (cairo_xlib_display_t *display, void *data)
{
    cairo_xlib_surface_t *surface = cairo_container_of (data,
							cairo_xlib_surface_t,
							close_display_hook);
    Display *dpy;

    dpy = display->display;

    X_DEBUG ((dpy, "detach (drawable=%x)", (unsigned int) surface->drawable));

    if (surface->dst_picture != None) {
	XRenderFreePicture (dpy, surface->dst_picture);
	surface->dst_picture = None;
    }

    if (surface->src_picture != None) {
	XRenderFreePicture (dpy, surface->src_picture);
	surface->src_picture = None;
    }

    if (surface->owns_pixmap) {
	XFreePixmap (dpy, surface->drawable);
	surface->drawable = None;
	surface->owns_pixmap = FALSE;
    }
}

static cairo_surface_t *
_cairo_xlib_surface_create_internal (cairo_xlib_screen_t	*screen,
				     Drawable			 drawable,
				     Visual			*visual,
				     XRenderPictFormat		*xrender_format,
				     int			 width,
				     int			 height,
				     int			 depth)
{
    cairo_xlib_surface_t *surface;
    cairo_xlib_display_t *display;
    cairo_status_t status;

    if (depth == 0) {
	if (xrender_format) {
	    depth = xrender_format->depth;

	    /* XXX find matching visual for core/dithering fallbacks? */
	} else if (visual) {
	    Screen *scr = screen->screen;

	    if (visual == DefaultVisualOfScreen (scr)) {
		depth = DefaultDepthOfScreen (scr);
	    } else  {
		int j, k;

		/* This is ugly, but we have to walk over all visuals
		 * for the display to find the correct depth.
		 */
		depth = 0;
		for (j = 0; j < scr->ndepths; j++) {
		    Depth *d = &scr->depths[j];
		    for (k = 0; k < d->nvisuals; k++) {
			if (&d->visuals[k] == visual) {
			    depth = d->depth;
			    goto found;
			}
		    }
		}
	    }
	}

	if (depth == 0)
	    return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_VISUAL));

found:
	;
    }

    surface = malloc (sizeof (cairo_xlib_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    status = _cairo_xlib_display_acquire (screen->device, &display);
    if (unlikely (status)) {
        free (surface);
        return _cairo_surface_create_in_error (_cairo_error (status));
    }

    _cairo_xlib_display_get_xrender_version (display,
					     &surface->render_major,
					     &surface->render_minor);
    if (CAIRO_SURFACE_RENDER_HAS_CREATE_PICTURE (surface)) {
	if (!xrender_format) {
	    if (visual) {
		xrender_format = XRenderFindVisualFormat (display->display, visual);
	    } else if (depth == 1) {
		xrender_format =
		    _cairo_xlib_display_get_xrender_format (display,
							    CAIRO_FORMAT_A1);
	    }
	}
    } else {
	/* we cannot use XRender for this surface, so ensure we don't try */
	surface->render_major = -1;
	surface->render_minor = -1;
    }

    /* initialize and hook into the CloseDisplay callback */
    surface->close_display_hook.func = _cairo_xlib_surface_detach_display;
    _cairo_xlib_add_close_display_hook (display,
					&surface->close_display_hook);

    cairo_device_release (&display->base);

    _cairo_surface_init (&surface->base,
			 &cairo_xlib_surface_backend,
			 screen->device,
			 _xrender_format_to_content (xrender_format));

    surface->screen = screen;

    surface->drawable = drawable;
    surface->owns_pixmap = FALSE;
    surface->use_pixmap = 0;
    surface->width = width;
    surface->height = height;

    surface->buggy_repeat = ! _cairo_xlib_display_has_repeat (screen->device);
    if (! CAIRO_SURFACE_RENDER_HAS_FILL_RECTANGLES (surface)) {
	/* so we can use the XTile fallback */
	surface->buggy_repeat = TRUE;
    }

    surface->buggy_pad_reflect = ! _cairo_xlib_display_has_reflect (screen->device);
    if (! CAIRO_SURFACE_RENDER_HAS_EXTENDED_REPEAT (surface))
	surface->buggy_pad_reflect = TRUE;

    surface->buggy_gradients = ! _cairo_xlib_display_has_gradients (screen->device);
    if (! CAIRO_SURFACE_RENDER_HAS_GRADIENTS (surface))
	surface->buggy_gradients = TRUE;

    surface->dst_picture = None;
    surface->src_picture = None;

    surface->visual = visual;
    surface->xrender_format = xrender_format;
    surface->depth = depth;
    surface->filter = CAIRO_FILTER_NEAREST;
    surface->extend = CAIRO_EXTEND_NONE;
    surface->has_component_alpha = FALSE;
    surface->precision = PolyModePrecise;
    surface->xtransform = identity;

    surface->clip_region = NULL;
    surface->clip_rects = surface->embedded_clip_rects;
    surface->num_clip_rects = 0;
    surface->clip_dirty = 0;

    /*
     * Compute the pixel format masks from either a XrenderFormat or
     * else from a visual; failing that we assume the drawable is an
     * alpha-only pixmap as it could only have been created that way
     * through the cairo_xlib_surface_create_for_bitmap function.
     */
    if (xrender_format) {
	surface->a_mask = (unsigned long)
	    surface->xrender_format->direct.alphaMask
	    << surface->xrender_format->direct.alpha;
	surface->r_mask = (unsigned long)
	    surface->xrender_format->direct.redMask
	    << surface->xrender_format->direct.red;
	surface->g_mask = (unsigned long)
	    surface->xrender_format->direct.greenMask
	    << surface->xrender_format->direct.green;
	surface->b_mask = (unsigned long)
	    surface->xrender_format->direct.blueMask
	    << surface->xrender_format->direct.blue;
    } else if (visual) {
	surface->a_mask = 0;
	surface->r_mask = visual->red_mask;
	surface->g_mask = visual->green_mask;
	surface->b_mask = visual->blue_mask;
    } else {
	if (depth < 32)
	    surface->a_mask = (1 << depth) - 1;
	else
	    surface->a_mask = 0xffffffff;
	surface->r_mask = 0;
	surface->g_mask = 0;
	surface->b_mask = 0;
    }

    return &surface->base;
}

static Screen *
_cairo_xlib_screen_from_visual (Display *dpy, Visual *visual)
{
    int s, d, v;

    for (s = 0; s < ScreenCount (dpy); s++) {
	Screen *screen;

	screen = ScreenOfDisplay (dpy, s);
	if (visual == DefaultVisualOfScreen (screen))
	    return screen;

	for (d = 0; d < screen->ndepths; d++) {
	    Depth  *depth;

	    depth = &screen->depths[d];
	    for (v = 0; v < depth->nvisuals; v++)
		if (visual == &depth->visuals[v])
		    return screen;
	}
    }

    return NULL;
}

/**
 * cairo_xlib_surface_create:
 * @dpy: an X Display
 * @drawable: an X Drawable, (a Pixmap or a Window)
 * @visual: the visual to use for drawing to @drawable. The depth
 *          of the visual must match the depth of the drawable.
 *          Currently, only TrueColor visuals are fully supported.
 * @width: the current width of @drawable.
 * @height: the current height of @drawable.
 *
 * Creates an Xlib surface that draws to the given drawable.
 * The way that colors are represented in the drawable is specified
 * by the provided visual.
 *
 * Note: If @drawable is a Window, then the function
 * cairo_xlib_surface_set_size() must be called whenever the size of the
 * window changes.
 *
 * When @drawable is a Window containing child windows then drawing to
 * the created surface will be clipped by those child windows.  When
 * the created surface is used as a source, the contents of the
 * children will be included.
 *
 * Return value: the newly created surface
 **/
cairo_surface_t *
cairo_xlib_surface_create (Display     *dpy,
			   Drawable	drawable,
			   Visual      *visual,
			   int		width,
			   int		height)
{
    Screen *scr;
    cairo_xlib_screen_t *screen;
    cairo_status_t status;

    if (width > XLIB_COORD_MAX || height > XLIB_COORD_MAX) {
	/* you're lying, and you know it! */
	return _cairo_surface_create_in_error (CAIRO_STATUS_INVALID_SIZE);
    }

    scr = _cairo_xlib_screen_from_visual (dpy, visual);
    if (scr == NULL)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_VISUAL));

    status = _cairo_xlib_screen_get (dpy, scr, &screen);
    if (unlikely (status))
	return _cairo_surface_create_in_error (status);

    X_DEBUG ((dpy, "create (drawable=%x)", (unsigned int) drawable));

    return _cairo_xlib_surface_create_internal (screen, drawable,
                                                visual, NULL,
                                                width, height, 0);
}

/**
 * cairo_xlib_surface_create_for_bitmap:
 * @dpy: an X Display
 * @bitmap: an X Drawable, (a depth-1 Pixmap)
 * @screen: the X Screen associated with @bitmap
 * @width: the current width of @bitmap.
 * @height: the current height of @bitmap.
 *
 * Creates an Xlib surface that draws to the given bitmap.
 * This will be drawn to as a %CAIRO_FORMAT_A1 object.
 *
 * Return value: the newly created surface
 **/
cairo_surface_t *
cairo_xlib_surface_create_for_bitmap (Display  *dpy,
				      Pixmap	bitmap,
				      Screen   *scr,
				      int	width,
				      int	height)
{
    cairo_xlib_screen_t *screen;
    cairo_status_t status;

    if (width > XLIB_COORD_MAX || height > XLIB_COORD_MAX)
	return _cairo_surface_create_in_error (CAIRO_STATUS_INVALID_SIZE);

    status = _cairo_xlib_screen_get (dpy, scr, &screen);
    if (unlikely (status))
	return _cairo_surface_create_in_error (status);

    X_DEBUG ((dpy, "create_for_bitmap (drawable=%x)", (unsigned int) bitmap));

    return _cairo_xlib_surface_create_internal (screen, bitmap,
                                                NULL, NULL,
                                                width, height, 1);
}

#if CAIRO_HAS_XLIB_XRENDER_SURFACE
/**
 * cairo_xlib_surface_create_with_xrender_format:
 * @dpy: an X Display
 * @drawable: an X Drawable, (a Pixmap or a Window)
 * @screen: the X Screen associated with @drawable
 * @format: the picture format to use for drawing to @drawable. The depth
 *          of @format must match the depth of the drawable.
 * @width: the current width of @drawable.
 * @height: the current height of @drawable.
 *
 * Creates an Xlib surface that draws to the given drawable.
 * The way that colors are represented in the drawable is specified
 * by the provided picture format.
 *
 * Note: If @drawable is a Window, then the function
 * cairo_xlib_surface_set_size() must be called whenever the size of the
 * window changes.
 *
 * Return value: the newly created surface
 **/
cairo_surface_t *
cairo_xlib_surface_create_with_xrender_format (Display		    *dpy,
					       Drawable		    drawable,
					       Screen		    *scr,
					       XRenderPictFormat    *format,
					       int		    width,
					       int		    height)
{
    cairo_xlib_screen_t *screen;
    cairo_status_t status;

    if (width > XLIB_COORD_MAX || height > XLIB_COORD_MAX)
	return _cairo_surface_create_in_error (CAIRO_STATUS_INVALID_SIZE);

    status = _cairo_xlib_screen_get (dpy, scr, &screen);
    if (unlikely (status))
	return _cairo_surface_create_in_error (status);

    X_DEBUG ((dpy, "create_with_xrender_format (drawable=%x)", (unsigned int) drawable));

    return _cairo_xlib_surface_create_internal (screen, drawable,
						_visual_for_xrender_format (scr, format),
                                                format, width, height, 0);
}

/**
 * cairo_xlib_surface_get_xrender_format:
 * @surface: an xlib surface
 *
 * Gets the X Render picture format that @surface uses for rendering with the
 * X Render extension. If the surface was created by
 * cairo_xlib_surface_create_with_xrender_format() originally, the return
 * value is the format passed to that constructor.
 *
 * Return value: the XRenderPictFormat* associated with @surface,
 * or %NULL if the surface is not an xlib surface
 * or if the X Render extension is not available.
 *
 * Since: 1.6
 **/
XRenderPictFormat *
cairo_xlib_surface_get_xrender_format (cairo_surface_t *surface)
{
    cairo_xlib_surface_t *xlib_surface = (cairo_xlib_surface_t *) surface;

    /* Throw an error for a non-xlib surface */
    if (! _cairo_surface_is_xlib (surface)) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return NULL;
    }

    return xlib_surface->xrender_format;
}
#endif

/**
 * cairo_xlib_surface_set_size:
 * @surface: a #cairo_surface_t for the XLib backend
 * @width: the new width of the surface
 * @height: the new height of the surface
 *
 * Informs cairo of the new size of the X Drawable underlying the
 * surface. For a surface created for a Window (rather than a Pixmap),
 * this function must be called each time the size of the window
 * changes. (For a subwindow, you are normally resizing the window
 * yourself, but for a toplevel window, it is necessary to listen for
 * ConfigureNotify events.)
 *
 * A Pixmap can never change size, so it is never necessary to call
 * this function on a surface created for a Pixmap.
 **/
void
cairo_xlib_surface_set_size (cairo_surface_t *abstract_surface,
			     int              width,
			     int              height)
{
    cairo_xlib_surface_t *surface = (cairo_xlib_surface_t *) abstract_surface;
    cairo_status_t status;

    if (unlikely (abstract_surface->status))
	return;
    if (unlikely (abstract_surface->finished)) {
	status = _cairo_surface_set_error (abstract_surface,
		                           _cairo_error (CAIRO_STATUS_SURFACE_FINISHED));
	return;
    }

    if (! _cairo_surface_is_xlib (abstract_surface)) {
	status = _cairo_surface_set_error (abstract_surface,
		                           _cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH));
	return;
    }

    if (width > XLIB_COORD_MAX || height > XLIB_COORD_MAX) {
	status = _cairo_surface_set_error (abstract_surface,
		                           _cairo_error (CAIRO_STATUS_INVALID_SIZE));
	return;
    }

    surface->width = width;
    surface->height = height;
}
/**
 * cairo_xlib_surface_set_drawable:
 * @surface: a #cairo_surface_t for the XLib backend
 * @drawable: the new drawable for the surface
 * @width: the width of the new drawable
 * @height: the height of the new drawable
 *
 * Informs cairo of a new X Drawable underlying the
 * surface. The drawable must match the display, screen
 * and format of the existing drawable or the application
 * will get X protocol errors and will probably terminate.
 * No checks are done by this function to ensure this
 * compatibility.
 **/
void
cairo_xlib_surface_set_drawable (cairo_surface_t   *abstract_surface,
				 Drawable	    drawable,
				 int		    width,
				 int		    height)
{
    cairo_xlib_surface_t *surface = (cairo_xlib_surface_t *)abstract_surface;
    cairo_status_t status;

    if (unlikely (abstract_surface->status))
	return;
    if (unlikely (abstract_surface->finished)) {
	status = _cairo_surface_set_error (abstract_surface,
		                           _cairo_error (CAIRO_STATUS_SURFACE_FINISHED));
	return;
    }

    if (! _cairo_surface_is_xlib (abstract_surface)) {
	status = _cairo_surface_set_error (abstract_surface,
		                           _cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH));
	return;
    }

    if (width > XLIB_COORD_MAX || height > XLIB_COORD_MAX) {
	status = _cairo_surface_set_error (abstract_surface,
		                           _cairo_error (CAIRO_STATUS_INVALID_SIZE));
	return;
    }

    /* XXX: and what about this case? */
    if (surface->owns_pixmap)
	return;

    if (surface->drawable != drawable) {
        cairo_xlib_display_t *display;

        status = _cairo_xlib_display_acquire (surface->base.device, &display);
        if (unlikely (status))
            return;

	X_DEBUG ((display->display, "set_drawable (drawable=%x)", (unsigned int) drawable));

	if (surface->dst_picture != None) {
	    status = _cairo_xlib_display_queue_resource (
		                                  display,
						  XRenderFreePicture,
						  surface->dst_picture);
	    if (unlikely (status)) {
		status = _cairo_surface_set_error (&surface->base, status);
		return;
	    }

	    surface->dst_picture = None;
	}

	if (surface->src_picture != None) {
	    status = _cairo_xlib_display_queue_resource (
		                                  display,
						  XRenderFreePicture,
						  surface->src_picture);
	    if (unlikely (status)) {
		status = _cairo_surface_set_error (&surface->base, status);
		return;
	    }

	    surface->src_picture = None;
	}

        cairo_device_release (&display->base);

	surface->drawable = drawable;
    }
    surface->width = width;
    surface->height = height;
}

/**
 * cairo_xlib_surface_get_display:
 * @surface: a #cairo_xlib_surface_t
 *
 * Get the X Display for the underlying X Drawable.
 *
 * Return value: the display.
 *
 * Since: 1.2
 **/
Display *
cairo_xlib_surface_get_display (cairo_surface_t *abstract_surface)
{
    if (! _cairo_surface_is_xlib (abstract_surface)) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return NULL;
    }

    return ((cairo_xlib_display_t *) abstract_surface->device)->display;
}

/**
 * cairo_xlib_surface_get_drawable:
 * @surface: a #cairo_xlib_surface_t
 *
 * Get the underlying X Drawable used for the surface.
 *
 * Return value: the drawable.
 *
 * Since: 1.2
 **/
Drawable
cairo_xlib_surface_get_drawable (cairo_surface_t *abstract_surface)
{
    cairo_xlib_surface_t *surface = (cairo_xlib_surface_t *) abstract_surface;

    if (! _cairo_surface_is_xlib (abstract_surface)) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return 0;
    }

    return surface->drawable;
}

/**
 * cairo_xlib_surface_get_screen:
 * @surface: a #cairo_xlib_surface_t
 *
 * Get the X Screen for the underlying X Drawable.
 *
 * Return value: the screen.
 *
 * Since: 1.2
 **/
Screen *
cairo_xlib_surface_get_screen (cairo_surface_t *abstract_surface)
{
    cairo_xlib_surface_t *surface = (cairo_xlib_surface_t *) abstract_surface;

    if (! _cairo_surface_is_xlib (abstract_surface)) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return NULL;
    }

    return surface->screen->screen;
}

/**
 * cairo_xlib_surface_get_visual:
 * @surface: a #cairo_xlib_surface_t
 *
 * Gets the X Visual associated with @surface, suitable for use with the
 * underlying X Drawable.  If @surface was created by
 * cairo_xlib_surface_create(), the return value is the Visual passed to that
 * constructor.
 *
 * Return value: the Visual or %NULL if there is no appropriate Visual for
 * @surface.
 *
 * Since: 1.2
 **/
Visual *
cairo_xlib_surface_get_visual (cairo_surface_t *surface)
{
    cairo_xlib_surface_t *xlib_surface = (cairo_xlib_surface_t *) surface;

    if (! _cairo_surface_is_xlib (surface)) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return NULL;
    }

    return xlib_surface->visual;
}

/**
 * cairo_xlib_surface_get_depth:
 * @surface: a #cairo_xlib_surface_t
 *
 * Get the number of bits used to represent each pixel value.
 *
 * Return value: the depth of the surface in bits.
 *
 * Since: 1.2
 **/
int
cairo_xlib_surface_get_depth (cairo_surface_t *abstract_surface)
{
    cairo_xlib_surface_t *surface = (cairo_xlib_surface_t *) abstract_surface;

    if (! _cairo_surface_is_xlib (abstract_surface)) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return 0;
    }

    return surface->depth;
}

/**
 * cairo_xlib_surface_get_width:
 * @surface: a #cairo_xlib_surface_t
 *
 * Get the width of the X Drawable underlying the surface in pixels.
 *
 * Return value: the width of the surface in pixels.
 *
 * Since: 1.2
 **/
int
cairo_xlib_surface_get_width (cairo_surface_t *abstract_surface)
{
    cairo_xlib_surface_t *surface = (cairo_xlib_surface_t *) abstract_surface;

    if (! _cairo_surface_is_xlib (abstract_surface)) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return 0;
    }

    return surface->width;
}

/**
 * cairo_xlib_surface_get_height:
 * @surface: a #cairo_xlib_surface_t
 *
 * Get the height of the X Drawable underlying the surface in pixels.
 *
 * Return value: the height of the surface in pixels.
 *
 * Since: 1.2
 **/
int
cairo_xlib_surface_get_height (cairo_surface_t *abstract_surface)
{
    cairo_xlib_surface_t *surface = (cairo_xlib_surface_t *) abstract_surface;

    if (! _cairo_surface_is_xlib (abstract_surface)) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return 0;
    }

    return surface->height;
}

enum {
    GLYPHSET_INDEX_ARGB32,
    GLYPHSET_INDEX_A8,
    GLYPHSET_INDEX_A1,
    NUM_GLYPHSETS
};

typedef struct _cairo_xlib_font_glyphset_free_glyphs {
    GlyphSet		glyphset;
    int			glyph_count;
    unsigned long	glyph_indices[128];
} cairo_xlib_font_glyphset_free_glyphs_t;

typedef struct _cairo_xlib_font_glyphset_info {
    GlyphSet		glyphset;
    cairo_format_t	format;
    XRenderPictFormat	*xrender_format;
    cairo_xlib_font_glyphset_free_glyphs_t *pending_free_glyphs;
} cairo_xlib_font_glyphset_info_t;

typedef struct _cairo_xlib_surface_font_private {
    cairo_scaled_font_t		    *scaled_font;
    cairo_scaled_font_t         *grayscale_font;
    cairo_xlib_hook_t                close_display_hook;
    cairo_device_t                  *device;
    cairo_xlib_font_glyphset_info_t  glyphset_info[NUM_GLYPHSETS];
} cairo_xlib_surface_font_private_t;

/* callback from CloseDisplay */
static void
_cairo_xlib_surface_remove_scaled_font (cairo_xlib_display_t	*display,
	                                void			*data)
{
    cairo_xlib_surface_font_private_t	*font_private;
    cairo_scaled_font_t			*scaled_font;

    font_private = cairo_container_of (data,
				       cairo_xlib_surface_font_private_t,
				       close_display_hook);
    scaled_font = font_private->scaled_font;

    CAIRO_MUTEX_LOCK (scaled_font->mutex);
    font_private = scaled_font->surface_private;
    scaled_font->surface_private = NULL;

    _cairo_scaled_font_reset_cache (scaled_font);
    CAIRO_MUTEX_UNLOCK (scaled_font->mutex);

    if (font_private != NULL) {
	int i;

	if (font_private->grayscale_font) {
	    cairo_scaled_font_destroy (font_private->grayscale_font);
	}

	for (i = 0; i < NUM_GLYPHSETS; i++) {
	    cairo_xlib_font_glyphset_info_t *glyphset_info;

	    glyphset_info = &font_private->glyphset_info[i];
	    if (glyphset_info->glyphset)
		XRenderFreeGlyphSet (display->display, glyphset_info->glyphset);

	    if (glyphset_info->pending_free_glyphs != NULL)
		free (glyphset_info->pending_free_glyphs);
	}

	cairo_device_destroy (font_private->device);
	free (font_private);
    }
}

static cairo_status_t
_cairo_xlib_surface_font_init (cairo_xlib_display_t *display,
			       cairo_scaled_font_t  *scaled_font)
{
    cairo_xlib_surface_font_private_t	*font_private;
    int i;

    font_private = malloc (sizeof (cairo_xlib_surface_font_private_t));
    if (unlikely (font_private == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    font_private->scaled_font = scaled_font;
    font_private->grayscale_font = NULL;
    font_private->device = cairo_device_reference (&display->base);

    /* initialize and hook into the CloseDisplay callback */
    font_private->close_display_hook.func =
	_cairo_xlib_surface_remove_scaled_font;
    _cairo_xlib_add_close_display_hook (display,
					&font_private->close_display_hook);


    for (i = 0; i < NUM_GLYPHSETS; i++) {
	cairo_xlib_font_glyphset_info_t *glyphset_info = &font_private->glyphset_info[i];
	switch (i) {
	case GLYPHSET_INDEX_ARGB32: glyphset_info->format = CAIRO_FORMAT_ARGB32; break;
	case GLYPHSET_INDEX_A8:     glyphset_info->format = CAIRO_FORMAT_A8;     break;
	case GLYPHSET_INDEX_A1:     glyphset_info->format = CAIRO_FORMAT_A1;     break;
	default:                    ASSERT_NOT_REACHED;                          break;
	}
	glyphset_info->xrender_format = NULL;
	glyphset_info->glyphset = None;
	glyphset_info->pending_free_glyphs = NULL;
    }

    scaled_font->surface_private = font_private;
    scaled_font->surface_backend = &cairo_xlib_surface_backend;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_xlib_surface_scaled_font_fini (cairo_scaled_font_t *scaled_font)
{
    cairo_xlib_surface_font_private_t *font_private;
    cairo_status_t status;

    font_private = scaled_font->surface_private;
    if (font_private != NULL) {
	cairo_xlib_display_t *display;
	int i;

	if (font_private->grayscale_font) {
	    cairo_scaled_font_destroy (font_private->grayscale_font);
	}
        status = _cairo_xlib_display_acquire (font_private->device, &display);
        if (status)
            goto BAIL;

	_cairo_xlib_remove_close_display_hook (display,
					       &font_private->close_display_hook);

	for (i = 0; i < NUM_GLYPHSETS; i++) {
	    cairo_xlib_font_glyphset_info_t *glyphset_info;

	    glyphset_info = &font_private->glyphset_info[i];

	    if (glyphset_info->pending_free_glyphs != NULL)
		free (glyphset_info->pending_free_glyphs);

	    if (glyphset_info->glyphset) {
		status = _cairo_xlib_display_queue_resource (display,
							     XRenderFreeGlyphSet,
							     glyphset_info->glyphset);
		(void) status; /* XXX cannot propagate failure */
	    }
	}

	cairo_device_release (&display->base);
BAIL:
	cairo_device_destroy (&display->base);
	free (font_private);
    }
}

static void
_cairo_xlib_render_free_glyphs (Display *dpy,
	                        cairo_xlib_font_glyphset_free_glyphs_t *to_free)
{
    XRenderFreeGlyphs (dpy,
	               to_free->glyphset,
	               to_free->glyph_indices,
		       to_free->glyph_count);
}

static cairo_xlib_font_glyphset_info_t *
_cairo_xlib_scaled_glyph_get_glyphset_info (cairo_scaled_glyph_t            *scaled_glyph)
{
    return scaled_glyph->surface_private;
}

static void
_cairo_xlib_scaled_glyph_set_glyphset_info (cairo_scaled_glyph_t            *scaled_glyph,
					    cairo_xlib_font_glyphset_info_t *glyphset_info)
{
    scaled_glyph->surface_private = glyphset_info;
}

static void
_cairo_xlib_surface_scaled_glyph_fini (cairo_scaled_glyph_t *scaled_glyph,
				       cairo_scaled_font_t  *scaled_font)
{
    cairo_xlib_surface_font_private_t	*font_private;
    cairo_xlib_font_glyphset_info_t *glyphset_info;

    if (scaled_font->finished)
	return;

    font_private = scaled_font->surface_private;
    glyphset_info = _cairo_xlib_scaled_glyph_get_glyphset_info (scaled_glyph);
    if (font_private != NULL && glyphset_info != NULL) {
	cairo_xlib_font_glyphset_free_glyphs_t *to_free;
	cairo_status_t status;

	to_free = glyphset_info->pending_free_glyphs;
	if (to_free != NULL &&
	    to_free->glyph_count == ARRAY_LENGTH (to_free->glyph_indices))
	{
            cairo_xlib_display_t *display;

            status = _cairo_xlib_display_acquire (font_private->device, &display);
            if (status == CAIRO_STATUS_SUCCESS) {
                status = _cairo_xlib_display_queue_work (display,
                        (cairo_xlib_notify_func) _cairo_xlib_render_free_glyphs,
                        to_free,
                        free);
                cairo_device_release (&display->base);
            }
	    /* XXX cannot propagate failure */
	    if (unlikely (status))
		free (to_free);

	    to_free = glyphset_info->pending_free_glyphs = NULL;
	}

	if (to_free == NULL) {
	    to_free = malloc (sizeof (cairo_xlib_font_glyphset_free_glyphs_t));
	    if (unlikely (to_free == NULL)) {
		_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
		return; /* XXX cannot propagate failure */
	    }

	    to_free->glyphset = glyphset_info->glyphset;
	    to_free->glyph_count = 0;
	    glyphset_info->pending_free_glyphs = to_free;
	}

	to_free->glyph_indices[to_free->glyph_count++] =
	    _cairo_scaled_glyph_index (scaled_glyph);
    }
}

static cairo_bool_t
_native_byte_order_lsb (void)
{
    int	x = 1;

    return *((char *) &x) == 1;
}

static int
_cairo_xlib_get_glyphset_index_for_format (cairo_format_t format)
{
    if (format == CAIRO_FORMAT_A8)
        return GLYPHSET_INDEX_A8;
    if (format == CAIRO_FORMAT_A1)
        return GLYPHSET_INDEX_A1;

    assert (format == CAIRO_FORMAT_ARGB32);
    return GLYPHSET_INDEX_ARGB32;
}

static cairo_xlib_font_glyphset_info_t *
_cairo_xlib_scaled_font_get_glyphset_info_for_format (cairo_scaled_font_t *scaled_font,
						      cairo_format_t       format)
{
    cairo_xlib_surface_font_private_t *font_private;
    cairo_xlib_font_glyphset_info_t *glyphset_info;
    int glyphset_index;

    glyphset_index = _cairo_xlib_get_glyphset_index_for_format (format);
    font_private = scaled_font->surface_private;
    glyphset_info = &font_private->glyphset_info[glyphset_index];
    if (glyphset_info->glyphset == None) {
	cairo_xlib_display_t *display;
        
        if (_cairo_xlib_display_acquire (font_private->device, &display))
            return NULL;

	glyphset_info->xrender_format =
	    _cairo_xlib_display_get_xrender_format (display,
		                                    glyphset_info->format);
	glyphset_info->glyphset = XRenderCreateGlyphSet (display->display,
							 glyphset_info->xrender_format);

        cairo_device_release (&display->base);
    }

    return glyphset_info;
}

static cairo_bool_t
_cairo_xlib_glyphset_info_has_pending_free_glyph (
				cairo_xlib_font_glyphset_info_t *glyphset_info,
				unsigned long glyph_index)
{
    if (glyphset_info->pending_free_glyphs != NULL) {
	cairo_xlib_font_glyphset_free_glyphs_t *to_free;
	int i;

	to_free = glyphset_info->pending_free_glyphs;
	for (i = 0; i < to_free->glyph_count; i++) {
	    if (to_free->glyph_indices[i] == glyph_index) {
		to_free->glyph_count--;
		memmove (&to_free->glyph_indices[i],
			 &to_free->glyph_indices[i+1],
			 (to_free->glyph_count - i) * sizeof (to_free->glyph_indices[0]));
		return TRUE;
	    }
	}
    }

    return FALSE;
}

static cairo_xlib_font_glyphset_info_t *
_cairo_xlib_scaled_font_get_glyphset_info_for_pending_free_glyph (
					       cairo_scaled_font_t *scaled_font,
					       unsigned long glyph_index,
					       cairo_image_surface_t *surface)
{
    cairo_xlib_surface_font_private_t *font_private;
    int i;

    font_private = scaled_font->surface_private;
    if (font_private == NULL)
	return NULL;

    if (surface != NULL) {
	i = _cairo_xlib_get_glyphset_index_for_format (surface->format);
	if (_cairo_xlib_glyphset_info_has_pending_free_glyph (
						&font_private->glyphset_info[i],
						glyph_index))
	{
	    return &font_private->glyphset_info[i];
	}
    } else {
	for (i = 0; i < NUM_GLYPHSETS; i++) {
	    if (_cairo_xlib_glyphset_info_has_pending_free_glyph (
						&font_private->glyphset_info[i],
						glyph_index))
	    {
		return &font_private->glyphset_info[i];
	    }
	}
    }

    return NULL;
}

static cairo_status_t
_cairo_xlib_surface_add_glyph (cairo_xlib_display_t *display,
			       cairo_scaled_font_t   *scaled_font,
			       cairo_scaled_glyph_t **pscaled_glyph)
{
    XGlyphInfo glyph_info;
    unsigned long glyph_index;
    unsigned char *data;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_scaled_glyph_t *scaled_glyph = *pscaled_glyph;
    cairo_image_surface_t *glyph_surface = scaled_glyph->surface;
    cairo_bool_t already_had_glyph_surface;
    cairo_xlib_font_glyphset_info_t *glyphset_info;

    glyph_index = _cairo_scaled_glyph_index (scaled_glyph);

    /* check to see if we have a pending XRenderFreeGlyph for this glyph */
    glyphset_info = _cairo_xlib_scaled_font_get_glyphset_info_for_pending_free_glyph (scaled_font, glyph_index, glyph_surface);
    if (glyphset_info != NULL) {
	_cairo_xlib_scaled_glyph_set_glyphset_info (scaled_glyph, glyphset_info);
	return CAIRO_STATUS_SUCCESS;
    }

    if (!glyph_surface) {
	status = _cairo_scaled_glyph_lookup (scaled_font,
					     glyph_index,
					     CAIRO_SCALED_GLYPH_INFO_METRICS |
					     CAIRO_SCALED_GLYPH_INFO_SURFACE,
					     pscaled_glyph);
	if (unlikely (status))
	    return status;

	scaled_glyph = *pscaled_glyph;
	glyph_surface = scaled_glyph->surface;
	already_had_glyph_surface = FALSE;
    } else {
	already_had_glyph_surface = TRUE;
    }

    if (scaled_font->surface_private == NULL) {
	status = _cairo_xlib_surface_font_init (display, scaled_font);
	if (unlikely (status))
	    return status;
    }

    glyphset_info = _cairo_xlib_scaled_font_get_glyphset_info_for_format (scaled_font,
									  glyph_surface->format);

    /* XRenderAddGlyph does not handle a glyph surface larger than the extended maximum XRequest size.  */
    {
	int len = cairo_format_stride_for_width (glyphset_info->format, glyph_surface->width) * glyph_surface->height;
	int max_request_size = (XExtendedMaxRequestSize (display->display) ? XExtendedMaxRequestSize (display->display)
                                                                           : XMaxRequestSize (display->display)) * 4 -
	                       sz_xRenderAddGlyphsReq -
	                       sz_xGlyphInfo          -
			       8;
	if (len >= max_request_size)
	    return UNSUPPORTED ("glyph too large for XRequest");
    }

    /* If the glyph surface has zero height or width, we create
     * a clear 1x1 surface, to avoid various X server bugs.
     */
    if (glyph_surface->width == 0 || glyph_surface->height == 0) {
	cairo_surface_t *tmp_surface;

	tmp_surface = cairo_image_surface_create (glyphset_info->format, 1, 1);
	status = tmp_surface->status;
	if (unlikely (status))
	    goto BAIL;

	tmp_surface->device_transform = glyph_surface->base.device_transform;
	tmp_surface->device_transform_inverse = glyph_surface->base.device_transform_inverse;

	glyph_surface = (cairo_image_surface_t *) tmp_surface;
    }

    /* If the glyph format does not match the font format, then we
     * create a temporary surface for the glyph image with the font's
     * format.
     */
    if (glyph_surface->format != glyphset_info->format) {
	cairo_surface_pattern_t pattern;
	cairo_surface_t *tmp_surface;

	tmp_surface = cairo_image_surface_create (glyphset_info->format,
						  glyph_surface->width,
						  glyph_surface->height);
	status = tmp_surface->status;
	if (unlikely (status))
	    goto BAIL;

	tmp_surface->device_transform = glyph_surface->base.device_transform;
	tmp_surface->device_transform_inverse = glyph_surface->base.device_transform_inverse;

	_cairo_pattern_init_for_surface (&pattern, &glyph_surface->base);
	status = _cairo_surface_paint (tmp_surface,
				       CAIRO_OPERATOR_SOURCE, &pattern.base,
				       NULL);
	_cairo_pattern_fini (&pattern.base);

	glyph_surface = (cairo_image_surface_t *) tmp_surface;

	if (unlikely (status))
	    goto BAIL;
    }

    /* XXX: FRAGILE: We're ignore device_transform scaling here. A bug? */
    glyph_info.x = _cairo_lround (glyph_surface->base.device_transform.x0);
    glyph_info.y = _cairo_lround (glyph_surface->base.device_transform.y0);
    glyph_info.width = glyph_surface->width;
    glyph_info.height = glyph_surface->height;
    glyph_info.xOff = scaled_glyph->x_advance;
    glyph_info.yOff = scaled_glyph->y_advance;

    data = glyph_surface->data;

    /* flip formats around */
    switch (_cairo_xlib_get_glyphset_index_for_format (scaled_glyph->surface->format)) {
    case GLYPHSET_INDEX_A1:
	/* local bitmaps are always stored with bit == byte */
	if (_native_byte_order_lsb() != (BitmapBitOrder (display->display) == LSBFirst)) {
	    int		    c = glyph_surface->stride * glyph_surface->height;
	    unsigned char   *d;
	    unsigned char   *new, *n;

	    new = malloc (c);
	    if (!new) {
		status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
		goto BAIL;
	    }
	    n = new;
	    d = data;
	    do {
		char	b = *d++;
		b = ((b << 1) & 0xaa) | ((b >> 1) & 0x55);
		b = ((b << 2) & 0xcc) | ((b >> 2) & 0x33);
		b = ((b << 4) & 0xf0) | ((b >> 4) & 0x0f);
		*n++ = b;
	    } while (--c);
	    data = new;
	}
	break;
    case GLYPHSET_INDEX_A8:
	break;
    case GLYPHSET_INDEX_ARGB32:
	if (_native_byte_order_lsb() != (ImageByteOrder (display->display) == LSBFirst)) {
	    unsigned int c = glyph_surface->stride * glyph_surface->height / 4;
	    const uint32_t *d;
	    uint32_t *new, *n;

	    new = malloc (4 * c);
	    if (unlikely (new == NULL)) {
		status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
		goto BAIL;
	    }
	    n = new;
	    d = (uint32_t *) data;
	    do {
		*n++ = bswap_32 (*d);
		d++;
	    } while (--c);
	    data = (uint8_t *) new;
	}
	break;
    default:
	ASSERT_NOT_REACHED;
	break;
    }
    /* XXX assume X server wants pixman padding. Xft assumes this as well */

    struct _XDisplay *dpy = (struct _XDisplay *) display->display;
    int req_length = sz_xRenderAddGlyphsReq + 4;
    if (req_length & 3)
	req_length += 4 - (req_length & 3);
    if (dpy->bufptr + req_length > dpy->bufmax)
	XFlush (display->display);

    XRenderAddGlyphs (display->display, glyphset_info->glyphset,
		      &glyph_index, &glyph_info, 1,
		      (char *) data,
		      glyph_surface->stride * glyph_surface->height);

    if (data != glyph_surface->data)
	free (data);

    _cairo_xlib_scaled_glyph_set_glyphset_info (scaled_glyph, glyphset_info);

 BAIL:
    if (glyph_surface != scaled_glyph->surface)
	cairo_surface_destroy (&glyph_surface->base);

    /* if the scaled glyph didn't already have a surface attached
     * to it, release the created surface now that we have it
     * uploaded to the X server.  If the surface has already been
     * there (eg. because image backend requested it), leave it in
     * the cache
     */
    if (!already_had_glyph_surface)
	_cairo_scaled_glyph_set_surface (scaled_glyph, scaled_font, NULL);

    return status;
}

typedef void (*cairo_xrender_composite_text_func_t)
	      (Display                      *dpy,
	       int                          op,
	       Picture                      src,
	       Picture                      dst,
	       _Xconst XRenderPictFormat    *maskFormat,
	       int                          xSrc,
	       int                          ySrc,
	       int                          xDst,
	       int                          yDst,
	       _Xconst XGlyphElt8           *elts,
	       int                          nelt);

/* Build a struct of the same size of #cairo_glyph_t that can be used both as
 * an input glyph with double coordinates, and as "working" glyph with
 * integer from-current-point offsets. */
typedef union {
    cairo_glyph_t d;
    unsigned long index;
    struct {
        unsigned long index;
        int x;
        int y;
    } i;
} cairo_xlib_glyph_t;

/* compile-time assert that #cairo_xlib_glyph_t is the same size as #cairo_glyph_t */
COMPILE_TIME_ASSERT (sizeof (cairo_xlib_glyph_t) == sizeof (cairo_glyph_t));

/* Start a new element for the first glyph,
 * or for any glyph that has unexpected position,
 * or if current element has too many glyphs
 * (Xrender limits each element to 252 glyphs, we limit them to 128)
 *
 * These same conditions need to be mirrored between
 * _cairo_xlib_surface_emit_glyphs and _emit_glyph_chunks
 */
#define _start_new_glyph_elt(count, glyph) \
    (((count) & 127) == 0 || (glyph)->i.x || (glyph)->i.y)

static cairo_status_t
_emit_glyphs_chunk (cairo_xlib_display_t *display,
                    cairo_xlib_surface_t *dst,
		    cairo_xlib_glyph_t *glyphs,
		    int num_glyphs,
		    cairo_scaled_font_t *scaled_font,
		    cairo_operator_t op,
		    cairo_xlib_surface_t *src,
		    cairo_surface_attributes_t *attributes,
		    /* info for this chunk */
		    int num_elts,
		    int width,
		    cairo_xlib_font_glyphset_info_t *glyphset_info)
{
    /* Which XRenderCompositeText function to use */
    cairo_xrender_composite_text_func_t composite_text_func;
    int size;

    /* Element buffer stuff */
    XGlyphElt8 *elts;
    XGlyphElt8 stack_elts[CAIRO_STACK_ARRAY_LENGTH (XGlyphElt8)];

    /* Reuse the input glyph array for output char generation */
    char *char8 = (char *) glyphs;
    unsigned short *char16 = (unsigned short *) glyphs;
    unsigned int *char32 = (unsigned int *) glyphs;

    int i;
    int nelt; /* Element index */
    int n; /* Num output glyphs in current element */
    int j; /* Num output glyphs so far */

    switch (width) {
    case 1:
	/* don't cast the 8-variant, to catch possible mismatches */
	composite_text_func = XRenderCompositeText8;
	size = sizeof (char);
	break;
    case 2:
	composite_text_func = (cairo_xrender_composite_text_func_t) XRenderCompositeText16;
	size = sizeof (unsigned short);
	break;
    default:
    case 4:
	composite_text_func = (cairo_xrender_composite_text_func_t) XRenderCompositeText32;
	size = sizeof (unsigned int);
    }

    /* Allocate element array */
    if (num_elts <= ARRAY_LENGTH (stack_elts)) {
      elts = stack_elts;
    } else {
      elts = _cairo_malloc_ab (num_elts, sizeof (XGlyphElt8));
      if (unlikely (elts == NULL))
	  return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    /* Fill them in */
    nelt = 0;
    n = 0;
    j = 0;
    for (i = 0; i < num_glyphs; i++) {

      /* Start a new element for first output glyph,
       * or for any glyph that has unexpected position,
       * or if current element has too many glyphs.
       *
       * These same conditions are mirrored in _cairo_xlib_surface_emit_glyphs()
       */
      if (_start_new_glyph_elt (j, &glyphs[i])) {
	  if (j) {
	    elts[nelt].nchars = n;
	    nelt++;
	    n = 0;
	  }
	  elts[nelt].chars = char8 + size * j;
	  elts[nelt].glyphset = glyphset_info->glyphset;
	  elts[nelt].xOff = glyphs[i].i.x;
	  elts[nelt].yOff = glyphs[i].i.y;
      }

      switch (width) {
      case 1: char8 [j] = (char)           glyphs[i].index; break;
      case 2: char16[j] = (unsigned short) glyphs[i].index; break;
      default:
      case 4: char32[j] = (unsigned int)   glyphs[i].index; break;
      }

      n++;
      j++;
    }

    if (n) {
	elts[nelt].nchars = n;
	nelt++;
    }

    /* Check that we agree with _cairo_xlib_surface_emit_glyphs() on the
     * expected number of xGlyphElts.  */
    assert (nelt == num_elts);

    composite_text_func (display->display,
			 _render_operator (op),
			 src->src_picture,
			 dst->dst_picture,
			 glyphset_info->xrender_format,
			 attributes->x_offset + elts[0].xOff,
			 attributes->y_offset + elts[0].yOff,
			 elts[0].xOff, elts[0].yOff,
			 (XGlyphElt8 *) elts, nelt);

    if (elts != stack_elts)
      free (elts);

    return CAIRO_STATUS_SUCCESS;
}


/* sz_xGlyphtElt required alignment to a 32-bit boundary, so ensure we have
 * enough room for padding */
#define _cairo_sz_xGlyphElt (sz_xGlyphElt + 4)

static cairo_status_t
_cairo_xlib_surface_emit_glyphs (cairo_xlib_display_t *display,
                                 cairo_xlib_surface_t *dst,
				 cairo_xlib_glyph_t *glyphs,
				 int num_glyphs,
				 cairo_scaled_font_t *scaled_font,
				 cairo_operator_t op,
				 cairo_xlib_surface_t *src,
				 cairo_surface_attributes_t *attributes,
				 int *remaining_glyphs)
{
    int i;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_scaled_glyph_t *scaled_glyph;
    cairo_fixed_t x = 0, y = 0;
    cairo_xlib_font_glyphset_info_t *glyphset_info = NULL, *this_glyphset_info;

    unsigned long max_index = 0;
    int width = 1;
    int num_elts = 0;
    int num_out_glyphs = 0;

    int max_request_size = XMaxRequestSize (display->display) * 4
			 - MAX (sz_xRenderCompositeGlyphs8Req,
				MAX(sz_xRenderCompositeGlyphs16Req,
				    sz_xRenderCompositeGlyphs32Req));
    int request_size = 0;

    _cairo_xlib_surface_ensure_dst_picture (display, dst);

    for (i = 0; i < num_glyphs; i++) {
	int this_x, this_y;
	int old_width;

	status = _cairo_scaled_glyph_lookup (scaled_font,
					     glyphs[i].index,
					     CAIRO_SCALED_GLYPH_INFO_METRICS,
					     &scaled_glyph);
	if (unlikely (status))
	    return status;

	this_x = _cairo_lround (glyphs[i].d.x);
	this_y = _cairo_lround (glyphs[i].d.y);

	/* Glyph skipping:
	 *
	 * We skip any glyphs that have troublesome coordinates.  We want
	 * to make sure that (glyph2.x - (glyph1.x + glyph1.width)) fits in
	 * a signed 16bit integer, otherwise it will overflow in the render
	 * protocol.
	 * To ensure this, we'll make sure that (glyph2.x - glyph1.x) fits in
	 * a signed 15bit integer.  The trivial option would be to allow
	 * coordinates -8192..8192, but that's kinda dull.  It probably will
	 * take a decade or so to get monitors 8192x4096 or something.  A
	 * negative value of -8192 on the other hand, is absolutely useless.
	 * Note that we do want to allow some negative positions.  The glyph
	 * may start off the screen but part of it make it to the screen.
	 * Anyway, we will allow positions in the range -4096..122887.  That
	 * will buy us a few more years before this stops working.
	 *
	 * Update: upon seeing weird glyphs, we just return and let fallback
	 * code do the job.
	 */
	if (((this_x+4096)|(this_y+4096))&~0x3fffu)
	    break;

	/* Send unsent glyphs to the server */
	if (_cairo_xlib_scaled_glyph_get_glyphset_info (scaled_glyph) == NULL) {
	    status = _cairo_xlib_surface_add_glyph (display,
		                                    scaled_font,
						    &scaled_glyph);
	    if (unlikely (status)) {
	        if (status == CAIRO_INT_STATUS_UNSUPPORTED)
		    /* Break so we flush glyphs so far and let fallback code
		     * handle the rest */
		    break;

		return status;
	    }
	}

	this_glyphset_info = _cairo_xlib_scaled_glyph_get_glyphset_info (scaled_glyph);
	if (!glyphset_info)
	    glyphset_info = this_glyphset_info;

	/* The invariant here is that we can always flush the glyphs
	 * accumulated before this one, using old_width, and they
	 * would fit in the request.
	 */
	old_width = width;

	/* Update max glyph index */
	if (glyphs[i].index > max_index) {
	    max_index = glyphs[i].index;
	    if (max_index >= 65536)
	      width = 4;
	    else if (max_index >= 256)
	      width = 2;
	    if (width != old_width)
	      request_size += (width - old_width) * num_out_glyphs;
	}

	/* If we will pass the max request size by adding this glyph,
	 * flush current glyphs.  Note that we account for a
	 * possible element being added below.
	 *
	 * Also flush if changing glyphsets, as Xrender limits one mask
	 * format per request, so we can either break up, or use a
	 * wide-enough mask format.  We do the former.  One reason to
	 * prefer the latter is the fact that Xserver ADDs all glyphs
	 * to the mask first, and then composes that to final surface,
	 * though it's not a big deal.
	 */
	if (request_size + width > max_request_size - _cairo_sz_xGlyphElt ||
	    (this_glyphset_info != glyphset_info)) {
	    status = _emit_glyphs_chunk (display, dst, glyphs, i,
					 scaled_font, op, src, attributes,
					 num_elts, old_width, glyphset_info);
	    if (unlikely (status))
		return status;

	    glyphs += i;
	    num_glyphs -= i;
	    i = 0;
	    max_index = glyphs[i].index;
	    width = max_index < 256 ? 1 : max_index < 65536 ? 2 : 4;
	    request_size = 0;
	    num_elts = 0;
	    num_out_glyphs = 0;
	    x = y = 0;
	    glyphset_info = this_glyphset_info;
	}

	/* Convert absolute glyph position to relative-to-current-point
	 * position */
	glyphs[i].i.x = this_x - x;
	glyphs[i].i.y = this_y - y;

	/* Start a new element for the first glyph,
	 * or for any glyph that has unexpected position,
	 * or if current element has too many glyphs.
	 *
	 * These same conditions are mirrored in _emit_glyphs_chunk().
	 */
      if (_start_new_glyph_elt (num_out_glyphs, &glyphs[i])) {
	    num_elts++;
	    request_size += _cairo_sz_xGlyphElt;
	}

	/* adjust current-position */
	x = this_x + scaled_glyph->x_advance;
	y = this_y + scaled_glyph->y_advance;

	num_out_glyphs++;
	request_size += width;
    }

    if (num_elts) {
	status = _emit_glyphs_chunk (display, dst, glyphs, i,
				     scaled_font, op, src, attributes,
				     num_elts, width, glyphset_info);
    }

    *remaining_glyphs = num_glyphs - i;
    if (*remaining_glyphs != 0 && status == CAIRO_STATUS_SUCCESS)
	status = CAIRO_INT_STATUS_UNSUPPORTED;

    return status;
}

static cairo_bool_t
_cairo_xlib_surface_owns_font (cairo_xlib_surface_t *dst,
			       cairo_scaled_font_t *scaled_font)
{
    cairo_xlib_surface_font_private_t *font_private;

    font_private = scaled_font->surface_private;
    if ((scaled_font->surface_backend != NULL &&
	 scaled_font->surface_backend != &cairo_xlib_surface_backend) ||
	(font_private != NULL && font_private->device != dst->base.device))
    {
	return FALSE;
    }

    return TRUE;
}

/* Gets a grayscale version of scaled_font. The grayscale version is cached
 * in our surface_private data.
 */
static cairo_scaled_font_t *
_cairo_xlib_get_grayscale_font (cairo_xlib_display_t *display,
                                cairo_scaled_font_t *scaled_font)
{
    cairo_xlib_surface_font_private_t *font_private = scaled_font->surface_private;
    cairo_bool_t needs_font;

    if (font_private == NULL) {
        cairo_status_t status = _cairo_xlib_surface_font_init (display, scaled_font);
        if (unlikely (status))
            return _cairo_scaled_font_create_in_error (status);
        font_private = scaled_font->surface_private;
    }

    CAIRO_MUTEX_LOCK (scaled_font->mutex);
    needs_font = !font_private->grayscale_font;
    CAIRO_MUTEX_UNLOCK (scaled_font->mutex);

    if (needs_font) {
        cairo_font_options_t options;
        cairo_scaled_font_t *new_font;

        options = scaled_font->options;
        options.antialias = CAIRO_ANTIALIAS_GRAY;
        new_font = cairo_scaled_font_create (scaled_font->font_face,
                                             &scaled_font->font_matrix,
                                             &scaled_font->ctm, &options);

        CAIRO_MUTEX_LOCK (scaled_font->mutex);
        if (!font_private->grayscale_font) {
            font_private->grayscale_font = new_font;
            new_font = NULL;
        }
        CAIRO_MUTEX_UNLOCK (scaled_font->mutex);

        if (new_font) {
            cairo_scaled_font_destroy (new_font);
        }
    }

    return font_private->grayscale_font;
}

static cairo_int_status_t
_cairo_xlib_surface_show_glyphs (void                *abstract_dst,
				 cairo_operator_t     op,
				 const cairo_pattern_t *src_pattern,
				 cairo_glyph_t       *glyphs,
				 int		      num_glyphs,
				 cairo_scaled_font_t *scaled_font,
				 cairo_clip_t	     *clip,
				 int		     *remaining_glyphs)
{
    cairo_int_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_xlib_surface_t *dst = (cairo_xlib_surface_t*) abstract_dst;
    composite_operation_t operation;
    cairo_surface_attributes_t attributes;
    cairo_xlib_surface_t *src = NULL;
    cairo_region_t *clip_region = NULL;
    cairo_xlib_display_t *display;

    if (! CAIRO_SURFACE_RENDER_HAS_COMPOSITE_TEXT (dst))
	return UNSUPPORTED ("XRender does not support CompositeText");

    /* Just let unbounded operators go through the fallback code
     * instead of trying to do the fixups here */
    if (! _cairo_operator_bounded_by_mask (op))
        return UNSUPPORTED ("unsupported unbounded op");

    /* Render <= 0.10 seems to have a bug with PictOpSrc and glyphs --
     * the solid source seems to be multiplied by the glyph mask, and
     * then the entire thing is copied to the destination surface,
     * including the fully transparent "background" of the rectangular
     * glyph surface. */
    if (op == CAIRO_OPERATOR_SOURCE &&
        ! CAIRO_SURFACE_RENDER_AT_LEAST(dst, 0, 11))
    {
        return UNSUPPORTED ("known bug in Render");
    }

    /* We can only use our code if we either have no clip or
     * have a real native clip region set.  If we're using
     * fallback clip masking, we have to go through the full
     * fallback path.
     */
    if (clip != NULL) {
	status = _cairo_clip_get_region (clip, &clip_region);
	assert (status != CAIRO_INT_STATUS_NOTHING_TO_DO);
	if (status)
	    return status;
    }

    operation = _categorize_composite_operation (dst, op, src_pattern, TRUE);
    if (operation == DO_UNSUPPORTED)
	return UNSUPPORTED ("unsupported op");

    if (! _cairo_xlib_surface_owns_font (dst, scaled_font))
	return UNSUPPORTED ("unowned font");


    status = _cairo_xlib_display_acquire (dst->base.device, &display);
    if (unlikely (status))
        return status;

    if (!dst->base.permit_subpixel_antialiasing &&
        scaled_font->options.antialias == CAIRO_ANTIALIAS_SUBPIXEL) {
        scaled_font = _cairo_xlib_get_grayscale_font (display, scaled_font);
    }

    X_DEBUG ((display->display, "show_glyphs (dst=%x)", (unsigned int) dst->drawable));

    if (clip_region != NULL &&
	cairo_region_num_rectangles (clip_region) == 1)
    {
	cairo_rectangle_int_t glyph_extents;
	cairo_rectangle_int_t clip_extents;

	/* Can we do without the clip?
	 * Around 50% of the time the clip is redundant (firefox).
	 */
	_cairo_scaled_font_glyph_approximate_extents (scaled_font,
						      glyphs, num_glyphs,
						      &glyph_extents);

	cairo_region_get_extents(clip_region, &clip_extents);
	if (clip_extents.x <= glyph_extents.x &&
	    clip_extents.y <= glyph_extents.y &&
	    clip_extents.x + clip_extents.width  >= glyph_extents.x + glyph_extents.width &&
	    clip_extents.y + clip_extents.height >= glyph_extents.y + glyph_extents.height)
	{
	    clip_region = NULL;
	}
    }

    status = _cairo_xlib_surface_set_clip_region (dst, clip_region);
    if (unlikely (status))
        goto BAIL0;

    /* After passing all those tests, we're now committed to rendering
     * these glyphs or to fail trying. We first upload any glyphs to
     * the X server that it doesn't have already, then we draw
     * them.
     */

    /* PictOpClear doesn't seem to work with CompositeText; it seems to ignore
     * the mask (the glyphs).  This code below was executed as a side effect
     * of going through the _clip_and_composite fallback code for old_show_glyphs,
     * so PictOpClear was never used with CompositeText before.
     */
    if (op == CAIRO_OPERATOR_CLEAR) {
	src_pattern = &_cairo_pattern_white.base;
	op = CAIRO_OPERATOR_DEST_OUT;
    }

    if (src_pattern->type == CAIRO_PATTERN_TYPE_SOLID) {
        status = _cairo_pattern_acquire_surface (src_pattern, &dst->base,
                                                 0, 0, 1, 1,
						 CAIRO_PATTERN_ACQUIRE_NONE,
                                                 (cairo_surface_t **) &src,
                                                 &attributes);
	if (unlikely (status))
	    goto BAIL0;
    } else {
        cairo_rectangle_int_t glyph_extents;

        status = _cairo_scaled_font_glyph_device_extents (scaled_font,
                                                          glyphs,
                                                          num_glyphs,
                                                          &glyph_extents,
							  NULL);
        if (unlikely (status))
	    goto BAIL0;

	if (clip != NULL) {
	    if (! _cairo_rectangle_intersect (&glyph_extents,
					      _cairo_clip_get_extents (clip)))
	    {
		goto BAIL0;
	    }
	}

        status = _cairo_xlib_surface_acquire_pattern_surface (display,
                                                              dst, src_pattern,
							      glyph_extents.x,
							      glyph_extents.y,
							      glyph_extents.width,
							      glyph_extents.height,
							      &src, &attributes);
        if (unlikely (status))
	    goto BAIL0;
    }

    operation = _recategorize_composite_operation (dst, op, src,
						   &attributes, TRUE);
    if (operation == DO_UNSUPPORTED) {
	status = UNSUPPORTED ("unsupported op");
	goto BAIL1;
    }

    status = _cairo_xlib_surface_set_attributes (display, src, &attributes, 0, 0);
    if (unlikely (status))
        goto BAIL1;

    _cairo_scaled_font_freeze_cache (scaled_font);
    if (_cairo_xlib_surface_owns_font (dst, scaled_font)) {
	status = _cairo_xlib_surface_emit_glyphs (display,
                                                  dst,
						  (cairo_xlib_glyph_t *) glyphs,
						  num_glyphs,
						  scaled_font,
						  op,
						  src,
						  &attributes,
						  remaining_glyphs);
    } else {
	status = UNSUPPORTED ("unowned font");
    }
    _cairo_scaled_font_thaw_cache (scaled_font);

  BAIL1:
    if (src)
        _cairo_pattern_release_surface (src_pattern, &src->base, &attributes);
  BAIL0:
    cairo_device_release (&display->base);

    return status;
}
