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

#if !CAIRO_HAS_XLIB_XCB_FUNCTIONS

#include "cairo-xlib-private.h"
#include "cairo-xlib-surface-private.h"

#include "cairo-compositor-private.h"
#include "cairo-clip-private.h"
#include "cairo-damage-private.h"
#include "cairo-default-context-private.h"
#include "cairo-error-private.h"
#include "cairo-image-surface-private.h"
#include "cairo-list-inline.h"
#include "cairo-pattern-private.h"
#include "cairo-pixman-private.h"
#include "cairo-region-private.h"
#include "cairo-scaled-font-private.h"
#include "cairo-surface-snapshot-private.h"
#include "cairo-surface-subsurface-private.h"

#include <X11/Xutil.h> /* for XDestroyImage */

#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>

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
#include <X11/Xlibint.h>
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
 **/

/**
 * CAIRO_HAS_XLIB_SURFACE:
 *
 * Defined if the Xlib surface backend is available.
 * This macro can be used to conditionally compile backend-specific code.
 *
 * Since: 1.0
 **/

/**
 * SECTION:cairo-xlib-xrender
 * @Title: XLib-XRender Backend
 * @Short_Description: X Window System rendering using XLib and the X Render extension
 * @See_Also: #cairo_surface_t
 *
 * The XLib surface is used to render cairo graphics to X Window System
 * windows and pixmaps using the XLib and Xrender libraries.
 *
 * Note that the XLib surface automatically takes advantage of X Render extension
 * if it is available.
 **/

/**
 * CAIRO_HAS_XLIB_XRENDER_SURFACE:
 *
 * Defined if the XLib/XRender surface functions are available.
 * This macro can be used to conditionally compile backend-specific code.
 *
 * Since: 1.6
 **/

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

/*
 * Instead of taking two round trips for each blending request,
 * assume that if a particular drawable fails GetImage that it will
 * fail for a "while"; use temporary pixmaps to avoid the errors
 */

#define CAIRO_ASSUME_PIXMAP	20

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

static cairo_content_t
_xrender_format_to_content (XRenderPictFormat *xrender_format)
{
    cairo_content_t content;

    /* This only happens when using a non-Render server. Let's punt
     * and say there's no alpha here. */
    if (xrender_format == NULL)
	return CAIRO_CONTENT_COLOR;

    content = 0;
    if (xrender_format->direct.alphaMask)
	    content |= CAIRO_CONTENT_ALPHA;
    if (xrender_format->direct.redMask |
	xrender_format->direct.greenMask |
	xrender_format->direct.blueMask)
	    content |= CAIRO_CONTENT_COLOR;

    return content;
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

    if (width == 0 || height == 0)
	return NULL;

    if (_cairo_xlib_display_acquire (src->base.device, &display))
        return NULL;

    /* If we never found an XRenderFormat or if it isn't compatible
     * with the content being requested, then we fallback to just
     * constructing a cairo_format_t instead, (which will fairly
     * arbitrarily pick a visual/depth for the similar surface.
     */
    xrender_format = NULL;
    if (src->xrender_format &&
	_xrender_format_to_content (src->xrender_format) == content)
    {
	xrender_format = src->xrender_format;
    }
    if (xrender_format == NULL) {
	xrender_format =
	    _cairo_xlib_display_get_xrender_format (display,
						    _cairo_format_from_content (content));
    }
    if (xrender_format) {
	Visual *visual;

	/* We've got a compatible XRenderFormat now, which means the
	 * similar surface will match the existing surface as closely in
	 * visual/depth etc. as possible. */
	pix = XCreatePixmap (display->display, src->drawable,
			     width, height, xrender_format->depth);

	if (xrender_format == src->xrender_format)
	    visual = src->visual;
	else
	    visual = _visual_for_xrender_format(src->screen->screen,
					        xrender_format);

	surface = (cairo_xlib_surface_t *)
		  _cairo_xlib_surface_create_internal (src->screen, pix, visual,
						       xrender_format,
						       width, height,
						       xrender_format->depth);
    }
    else
    {
	Screen *screen = src->screen->screen;
	int depth;

	/* No compatible XRenderFormat, see if we can make an ordinary pixmap,
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
    }

    if (likely (surface->base.status == CAIRO_STATUS_SUCCESS))
	surface->owns_pixmap = TRUE;
    else
	XFreePixmap (display->display, pix);

    cairo_device_release (&display->base);

    return &surface->base;
}

static void
_cairo_xlib_surface_discard_shm (cairo_xlib_surface_t *surface)
{
    if (surface->shm == NULL)
	return;

    /* Force the flush for an external surface */
    if (!surface->owns_pixmap)
	cairo_surface_flush (surface->shm);

    cairo_surface_finish (surface->shm);
    cairo_surface_destroy (surface->shm);
    surface->shm = NULL;

    _cairo_damage_destroy (surface->base.damage);
    surface->base.damage = NULL;

    surface->fallback = 0;
}

static cairo_status_t
_cairo_xlib_surface_finish (void *abstract_surface)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    cairo_status_t        status;
    cairo_xlib_display_t *display;

    cairo_list_del (&surface->link);

    status = _cairo_xlib_display_acquire (surface->base.device, &display);
    if (unlikely (status))
        return status;

    X_DEBUG ((display->display, "finish (drawable=%x)", (unsigned int) surface->drawable));

    if (surface->embedded_source.picture)
	XRenderFreePicture (display->display, surface->embedded_source.picture);
    if (surface->picture)
	XRenderFreePicture (display->display, surface->picture);

    _cairo_xlib_surface_discard_shm (surface);

    if (surface->owns_pixmap)
	XFreePixmap (display->display, surface->drawable);

    cairo_device_release (&display->base);

    return status;
}

cairo_status_t
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
    int native_byte_order = _cairo_is_little_endian () ? LSBFirst : MSBFirst;

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

static int bits_per_pixel(cairo_xlib_surface_t *surface)
{
    if (surface->depth > 16)
	return 32;
    else if (surface->depth > 8)
	return 16;
    else if (surface->depth > 1)
	return 8;
    else
	return 1;
}

pixman_format_code_t
_pixman_format_for_xlib_surface (cairo_xlib_surface_t *surface)
{
    cairo_format_masks_t masks;
    pixman_format_code_t format;

    masks.bpp = bits_per_pixel (surface);
    masks.alpha_mask = surface->a_mask;
    masks.red_mask = surface->r_mask;
    masks.green_mask = surface->g_mask;
    masks.blue_mask = surface->b_mask;
    if (! _pixman_format_from_masks (&masks, &format))
	return 0;

    return format;
}

static cairo_surface_t *
_get_image_surface (cairo_xlib_surface_t    *surface,
		    const cairo_rectangle_int_t *extents,
		    int try_shm)
{
    cairo_int_status_t status;
    cairo_image_surface_t *image = NULL;
    XImage *ximage;
    pixman_format_code_t pixman_format;
    cairo_xlib_display_t *display;

    assert (extents->x >= 0);
    assert (extents->y >= 0);
    assert (extents->x + extents->width <= surface->width);
    assert (extents->y + extents->height <= surface->height);

    if (surface->base.is_clear ||
	(surface->base.serial == 0 && surface->owns_pixmap))
    {
	pixman_format = _pixman_format_for_xlib_surface (surface);
	if (pixman_format)
	{
	    return _cairo_image_surface_create_with_pixman_format (NULL,
								   pixman_format,
								   extents->width,
								   extents->height,
								   0);
	}
    }

    if (surface->shm) {
	cairo_image_surface_t *src = (cairo_image_surface_t *) surface->shm;
	cairo_surface_t *dst;
	cairo_surface_pattern_t pattern;

	dst = cairo_image_surface_create (src->format,
					  extents->width, extents->height);
	if (unlikely (dst->status))
	    return dst;

	_cairo_pattern_init_for_surface (&pattern, &src->base);
	cairo_matrix_init_translate (&pattern.base.matrix,
				     extents->x, extents->y);
	status = _cairo_surface_paint (dst, CAIRO_OPERATOR_SOURCE, &pattern.base, NULL);
	_cairo_pattern_fini (&pattern.base);
	if (unlikely (status)) {
	    cairo_surface_destroy (dst);
	    dst = _cairo_surface_create_in_error (status);
	}

	return dst;
    }

    status = _cairo_xlib_display_acquire (surface->base.device, &display);
    if (status)
        return _cairo_surface_create_in_error (status);

    pixman_format = _pixman_format_for_xlib_surface (surface);
    if (try_shm && pixman_format) {
	image = (cairo_image_surface_t *)
	    _cairo_xlib_surface_create_shm__image (surface, pixman_format,
						   extents->width, extents->height);
	if (image && image->base.status == CAIRO_STATUS_SUCCESS) {
	    cairo_xlib_error_func_t old_handler;
	    XImage shm_image;
	    Bool success;

	    _cairo_xlib_shm_surface_get_ximage (&image->base, &shm_image);

	    XSync (display->display, False);
	    old_handler = XSetErrorHandler (_noop_error_handler);
	    success = XShmGetImage (display->display,
				    surface->drawable,
				    &shm_image,
				    extents->x, extents->y,
				    AllPlanes);
	    XSetErrorHandler (old_handler);

	    if (success) {
		cairo_device_release (&display->base);
		return &image->base;
	    }

	    cairo_surface_destroy (&image->base);
	    image = NULL;
	}
    }

    if (surface->use_pixmap == 0) {
	cairo_xlib_error_func_t old_handler;

	XSync (display->display, False);
	old_handler = XSetErrorHandler (_noop_error_handler);

	ximage = XGetImage (display->display,
			    surface->drawable,
			    extents->x, extents->y,
			    extents->width, extents->height,
			    AllPlanes, ZPixmap);

	XSetErrorHandler (old_handler);

	/* If we get an error, the surface must have been a window,
	 * so retry with the safe code path.
	 */
	if (!ximage)
	    surface->use_pixmap = CAIRO_ASSUME_PIXMAP;
    } else {
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
				extents->width, extents->height,
				surface->depth);
	if (pixmap) {
	    XGCValues gcv;

	    gcv.subwindow_mode = IncludeInferiors;
	    XChangeGC (display->display, gc, GCSubwindowMode, &gcv);

	    XCopyArea (display->display, surface->drawable, pixmap, gc,
		       extents->x, extents->y,
		       extents->width, extents->height,
		       0, 0);

	    gcv.subwindow_mode = ClipByChildren;
	    XChangeGC (display->display, gc, GCSubwindowMode, &gcv);

	    ximage = XGetImage (display->display,
				pixmap,
				0, 0,
				extents->width, extents->height,
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

    /* We can't use pixman to simply write to image if:
     *   (a) the pixels are not appropriately aligned,
     *   (b) pixman does not the pixel format, or
     *   (c) if the image is palettized and we need to convert.
     */
    if (pixman_format &&
	ximage->bitmap_unit == 32 && ximage->bitmap_pad == 32 &&
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
	x0 = extents->x + surface->base.device_transform.x0;
	y0 = extents->y + surface->base.device_transform.y0;
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
			(uint32_t)_field_to_8 (in_pixel & a_mask, a_width, a_shift) << 24 |
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
	if (image)
	    cairo_surface_destroy (&image->base);
	return _cairo_surface_create_in_error (status);
    }

    return &image->base;
}

void
_cairo_xlib_surface_set_precision (cairo_xlib_surface_t	*surface,
				   cairo_antialias_t	 antialias)
{
    cairo_xlib_display_t	*display = surface->display;
    int precision;

    if (display->force_precision != -1)
	    precision = display->force_precision;
    else switch (antialias) {
    default:
    case CAIRO_ANTIALIAS_DEFAULT:
    case CAIRO_ANTIALIAS_GRAY:
    case CAIRO_ANTIALIAS_NONE:
    case CAIRO_ANTIALIAS_FAST:
    case CAIRO_ANTIALIAS_GOOD:
	precision = PolyModeImprecise;
	break;
    case CAIRO_ANTIALIAS_BEST:
    case CAIRO_ANTIALIAS_SUBPIXEL:
	precision = PolyModePrecise;
	break;
    }

    if (surface->precision != precision) {
	XRenderPictureAttributes pa;

	pa.poly_mode = precision;
	XRenderChangePicture (display->display, surface->picture,
			      CPPolyMode, &pa);

	surface->precision = precision;
    }
}

void
_cairo_xlib_surface_ensure_picture (cairo_xlib_surface_t    *surface)
{
    cairo_xlib_display_t *display = surface->display;
    XRenderPictureAttributes pa;
    int mask = 0;

    if (surface->picture)
	return;

    if (display->force_precision != -1)
	pa.poly_mode = display->force_precision;
    else
	pa.poly_mode = PolyModeImprecise;
    if (pa.poly_mode)
	    mask |= CPPolyMode;

    surface->precision = pa.poly_mode;
    surface->picture = XRenderCreatePicture (display->display,
					     surface->drawable,
					     surface->xrender_format,
					     mask, &pa);
}

cairo_status_t
_cairo_xlib_surface_draw_image (cairo_xlib_surface_t   *surface,
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
    int native_byte_order = _cairo_is_little_endian () ? LSBFirst : MSBFirst;
    cairo_surface_t *shm_image = NULL;
    pixman_image_t *pixman_image = NULL;
    cairo_status_t status;
    cairo_bool_t own_data = FALSE;
    cairo_bool_t is_rgb_image;
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
    ximage.obdata = NULL;

    status = _cairo_xlib_display_acquire (surface->base.device, &display);
    if (unlikely (status))
        return status;

    is_rgb_image = _pixman_format_to_masks (image->pixman_format, &image_masks);

    if (is_rgb_image &&
	(image_masks.alpha_mask == surface->a_mask || surface->a_mask == 0) &&
	(image_masks.red_mask   == surface->r_mask || surface->r_mask == 0) &&
	(image_masks.green_mask == surface->g_mask || surface->g_mask == 0) &&
	(image_masks.blue_mask  == surface->b_mask || surface->b_mask == 0))
    {
	int ret;

	ximage.bits_per_pixel = image_masks.bpp;
	ximage.bytes_per_line = image->stride;
	ximage.data = (char *)image->data;
	if (image->base.device != surface->base.device) {
	    /* If PutImage will break the image up into chunks, prefer to
	     * send it all in one pass with ShmPutImage.  For larger images,
	     * it is further advantageous to reduce the number of copies,
	     * albeit at the expense of more SHM bookkeeping.
	     */
	    int max_request_size = XExtendedMaxRequestSize (display->display);
	    if (max_request_size == 0)
		max_request_size = XMaxRequestSize (display->display);
	    if (max_request_size > 8192)
		max_request_size = 8192;
	    if (width * height * 4 > max_request_size) {
		shm_image = _cairo_xlib_surface_create_shm__image (surface,
								   image->pixman_format,
								   width, height);
		if (shm_image && shm_image->status == CAIRO_STATUS_SUCCESS) {
		    cairo_image_surface_t *clone = (cairo_image_surface_t *) shm_image;
		    pixman_image_composite32 (PIXMAN_OP_SRC,
					      image->pixman_image, NULL, clone->pixman_image,
					      src_x, src_y,
					      0, 0,
					      0, 0,
					      width, height);
		    ximage.obdata = _cairo_xlib_shm_surface_get_obdata (shm_image);
		    ximage.data = (char *)clone->data;
		    ximage.bytes_per_line = clone->stride;
		    ximage.width = width;
		    ximage.height = height;
		    src_x = src_y = 0;
		}
	    }
	} else
	    ximage.obdata = _cairo_xlib_shm_surface_get_obdata (&image->base);

	ret = XInitImage (&ximage);
	assert (ret != 0);
    }
    else if (surface->visual == NULL || surface->visual->class == TrueColor)
    {
        pixman_format_code_t intermediate_format;
        int ret;

        image_masks.alpha_mask = surface->a_mask;
        image_masks.red_mask   = surface->r_mask;
        image_masks.green_mask = surface->g_mask;
        image_masks.blue_mask  = surface->b_mask;
        image_masks.bpp        = bits_per_pixel (surface);
        ret = _pixman_format_from_masks (&image_masks, &intermediate_format);
        assert (ret);

	shm_image = _cairo_xlib_surface_create_shm__image (surface,
							   intermediate_format,
							   width, height);
	if (shm_image && shm_image->status == CAIRO_STATUS_SUCCESS) {
	    cairo_image_surface_t *clone = (cairo_image_surface_t *) shm_image;

	    pixman_image_composite32 (PIXMAN_OP_SRC,
				      image->pixman_image,
				      NULL,
				      clone->pixman_image,
				      src_x, src_y,
				      0, 0,
				      0, 0,
				      width, height);

	    ximage.data = (char *) clone->data;
	    ximage.obdata = _cairo_xlib_shm_surface_get_obdata (&clone->base);
	    ximage.bytes_per_line = clone->stride;
	} else {
	    pixman_image = pixman_image_create_bits (intermediate_format,
						     width, height, NULL, 0);
	    if (pixman_image == NULL) {
		status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
		goto BAIL;
	    }

	    pixman_image_composite32 (PIXMAN_OP_SRC,
				      image->pixman_image,
				      NULL,
				      pixman_image,
				      src_x, src_y,
				      0, 0,
				      0, 0,
				      width, height);

	    ximage.data = (char *) pixman_image_get_data (pixman_image);
	    ximage.bytes_per_line = pixman_image_get_stride (pixman_image);
	}

	ximage.width = width;
	ximage.height = height;
	ximage.bits_per_pixel = image_masks.bpp;

	ret = XInitImage (&ximage);
	assert (ret != 0);

	src_x = src_y = 0;
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

	ximage.bits_per_pixel = bits_per_pixel(surface);
	stride = CAIRO_STRIDE_FOR_WIDTH_BPP (ximage.width,
					     ximage.bits_per_pixel);
	ximage.bytes_per_line = stride;
	ximage.data = _cairo_malloc_ab (stride, ximage.height);
	if (unlikely (ximage.data == NULL)) {
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

    if (ximage.obdata)
	XShmPutImage (display->display, surface->drawable, gc, &ximage,
		      src_x, src_y, dst_x, dst_y, width, height, True);
    else
	XPutImage (display->display, surface->drawable, gc, &ximage,
		   src_x, src_y, dst_x, dst_y, width, height);

    _cairo_xlib_surface_put_gc (display, surface, gc);

  BAIL:
    cairo_device_release (&display->base);

    if (own_data)
	free (ximage.data);
    if (shm_image)
	cairo_surface_destroy (shm_image);
    if (pixman_image)
        pixman_image_unref (pixman_image);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_surface_t *
_cairo_xlib_surface_source(void                    *abstract_surface,
			   cairo_rectangle_int_t *extents)
{
    cairo_xlib_surface_t *surface = abstract_surface;

    if (extents) {
	extents->x = extents->y = 0;
	extents->width  = surface->width;
	extents->height = surface->height;
    }

    return &surface->base;
}

static cairo_status_t
_cairo_xlib_surface_acquire_source_image (void                    *abstract_surface,
					  cairo_image_surface_t  **image_out,
					  void                   **image_extra)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    cairo_rectangle_int_t extents;

    *image_extra = NULL;
    *image_out = (cairo_image_surface_t *)
	_cairo_xlib_surface_get_shm (abstract_surface, FALSE);
    if (*image_out) 
	    return (*image_out)->base.status;

    extents.x = extents.y = 0;
    extents.width = surface->width;
    extents.height = surface->height;

    *image_out = (cairo_image_surface_t*)
	_get_image_surface (surface, &extents, TRUE);
    return (*image_out)->base.status;
}

static cairo_surface_t *
_cairo_xlib_surface_snapshot (void *abstract_surface)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    cairo_rectangle_int_t extents;

    extents.x = extents.y = 0;
    extents.width = surface->width;
    extents.height = surface->height;

    return _get_image_surface (surface, &extents, FALSE);
}

static void
_cairo_xlib_surface_release_source_image (void                   *abstract_surface,
					  cairo_image_surface_t  *image,
					  void                   *image_extra)
{
    cairo_xlib_surface_t *surface = abstract_surface;

    if (&image->base == surface->shm)
	return;

    cairo_surface_destroy (&image->base);
}

static cairo_image_surface_t *
_cairo_xlib_surface_map_to_image (void                    *abstract_surface,
				  const cairo_rectangle_int_t   *extents)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    cairo_surface_t *image;

    image = _cairo_xlib_surface_get_shm (abstract_surface, FALSE);
    if (image) {
	assert (surface->base.damage);
	surface->fallback++;
	return _cairo_image_surface_map_to_image (image, extents);
    }

    image = _get_image_surface (abstract_surface, extents, TRUE);
    cairo_surface_set_device_offset (image, -extents->x, -extents->y);

    return (cairo_image_surface_t *) image;
}

static cairo_int_status_t
_cairo_xlib_surface_unmap_image (void *abstract_surface,
				 cairo_image_surface_t *image)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    cairo_int_status_t status;

    if (surface->shm) {
	cairo_rectangle_int_t r;

	assert (surface->fallback);
	assert (surface->base.damage);

	r.x = image->base.device_transform_inverse.x0;
	r.y = image->base.device_transform_inverse.y0;
	r.width  = image->width;
	r.height = image->height;

	TRACE ((stderr, "%s: adding damage (%d,%d)x(%d,%d)\n",
		__FUNCTION__, r.x, r.y, r.width, r.height));
	surface->shm->damage =
	    _cairo_damage_add_rectangle (surface->shm->damage, &r);

	return _cairo_image_surface_unmap_image (surface->shm, image);
    }

    status = _cairo_xlib_surface_draw_image (abstract_surface, image,
					     0, 0,
					     image->width, image->height,
					     image->base.device_transform_inverse.x0,
					     image->base.device_transform_inverse.y0);

    cairo_surface_finish (&image->base);
    cairo_surface_destroy (&image->base);

    return status;
}

static cairo_status_t
_cairo_xlib_surface_flush (void *abstract_surface,
			   unsigned flags)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    cairo_int_status_t status;

    if (flags)
	return CAIRO_STATUS_SUCCESS;

    status = _cairo_xlib_surface_put_shm (surface);
    if (unlikely (status))
	return status;

    surface->fallback >>= 1;
    if (surface->shm && _cairo_xlib_shm_surface_is_idle (surface->shm))
	_cairo_xlib_surface_discard_shm (surface);

    return CAIRO_STATUS_SUCCESS;
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

static inline cairo_int_status_t
get_compositor (cairo_xlib_surface_t **surface,
		const cairo_compositor_t **compositor)
{
    cairo_xlib_surface_t *s = *surface;
    cairo_int_status_t status = CAIRO_INT_STATUS_SUCCESS;;

    if (s->fallback) {
	assert (s->base.damage != NULL);
	assert (s->shm != NULL);
	assert (s->shm->damage != NULL);
	if (! _cairo_xlib_shm_surface_is_active (s->shm)) {
	    *surface = (cairo_xlib_surface_t *) s->shm;
	    *compositor = ((cairo_image_surface_t *) s->shm)->compositor;
	    s->fallback++;
	} else {
	    status = _cairo_xlib_surface_put_shm (s);
	    s->fallback = 0;
	    *compositor = s->compositor;
	}
    } else
	*compositor = s->compositor;

    return status;
}

static cairo_int_status_t
_cairo_xlib_surface_paint (void				*_surface,
			   cairo_operator_t		 op,
			   const cairo_pattern_t	*source,
			   const cairo_clip_t		*clip)
{
    cairo_xlib_surface_t *surface = _surface;
    const cairo_compositor_t *compositor;
    cairo_int_status_t status;

    status = get_compositor (&surface, &compositor);
    if (unlikely (status))
	return status;

    return _cairo_compositor_paint (compositor, &surface->base,
				    op, source,
				    clip);
}

static cairo_int_status_t
_cairo_xlib_surface_mask (void			*_surface,
			  cairo_operator_t	 op,
			  const cairo_pattern_t	*source,
			  const cairo_pattern_t	*mask,
			  const cairo_clip_t	*clip)
{
    cairo_xlib_surface_t *surface = _surface;
    const cairo_compositor_t *compositor;
    cairo_int_status_t status;

    status = get_compositor (&surface, &compositor);
    if (unlikely (status))
	return status;

    return _cairo_compositor_mask (compositor, &surface->base,
				   op, source, mask,
				   clip);
}

static cairo_int_status_t
_cairo_xlib_surface_stroke (void			*_surface,
			    cairo_operator_t		 op,
			    const cairo_pattern_t	*source,
			    const cairo_path_fixed_t	*path,
			    const cairo_stroke_style_t	*style,
			    const cairo_matrix_t	*ctm,
			    const cairo_matrix_t	*ctm_inverse,
			    double			 tolerance,
			    cairo_antialias_t		 antialias,
			    const cairo_clip_t		*clip)
{
    cairo_xlib_surface_t *surface = _surface;
    const cairo_compositor_t *compositor;
    cairo_int_status_t status;

    status = get_compositor (&surface, &compositor);
    if (unlikely (status))
	return status;

    return _cairo_compositor_stroke (compositor, &surface->base,
				     op, source,
				     path, style, ctm, ctm_inverse,
				     tolerance, antialias,
				     clip);
}

static cairo_int_status_t
_cairo_xlib_surface_fill (void				*_surface,
			  cairo_operator_t		 op,
			  const cairo_pattern_t		*source,
			  const cairo_path_fixed_t	*path,
			  cairo_fill_rule_t		 fill_rule,
			  double			 tolerance,
			  cairo_antialias_t		 antialias,
			  const cairo_clip_t		*clip)
{
    cairo_xlib_surface_t *surface = _surface;
    const cairo_compositor_t *compositor;
    cairo_int_status_t status;

    status = get_compositor (&surface, &compositor);
    if (unlikely (status))
	return status;

    return _cairo_compositor_fill (compositor, &surface->base,
				   op, source,
				   path, fill_rule, tolerance, antialias,
				   clip);
}

static cairo_int_status_t
_cairo_xlib_surface_glyphs (void			*_surface,
			    cairo_operator_t		 op,
			    const cairo_pattern_t	*source,
			    cairo_glyph_t		*glyphs,
			    int				 num_glyphs,
			    cairo_scaled_font_t		*scaled_font,
			    const cairo_clip_t		*clip)
{
    cairo_xlib_surface_t *surface = _surface;
    const cairo_compositor_t *compositor;
    cairo_int_status_t status;

    status = get_compositor (&surface, &compositor);
    if (unlikely (status))
	return status;

    return _cairo_compositor_glyphs (compositor, &surface->base,
				     op, source,
				     glyphs, num_glyphs, scaled_font,
				     clip);
}

static const cairo_surface_backend_t cairo_xlib_surface_backend = {
    CAIRO_SURFACE_TYPE_XLIB,
    _cairo_xlib_surface_finish,

    _cairo_default_context_create,

    _cairo_xlib_surface_create_similar,
    _cairo_xlib_surface_create_similar_shm,
    _cairo_xlib_surface_map_to_image,
    _cairo_xlib_surface_unmap_image,

    _cairo_xlib_surface_source,
    _cairo_xlib_surface_acquire_source_image,
    _cairo_xlib_surface_release_source_image,
    _cairo_xlib_surface_snapshot,

    NULL, /* copy_page */
    NULL, /* show_page */

    _cairo_xlib_surface_get_extents,
    _cairo_xlib_surface_get_font_options,

    _cairo_xlib_surface_flush,
    NULL, /* mark_dirty_rectangle */

    _cairo_xlib_surface_paint,
    _cairo_xlib_surface_mask,
    _cairo_xlib_surface_stroke,
    _cairo_xlib_surface_fill,
    NULL, /* fill-stroke */
    _cairo_xlib_surface_glyphs,
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

    surface = _cairo_malloc (sizeof (cairo_xlib_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    status = _cairo_xlib_display_acquire (screen->device, &display);
    if (unlikely (status)) {
        free (surface);
        return _cairo_surface_create_in_error (_cairo_error (status));
    }

    surface->display = display;
    if (CAIRO_RENDER_HAS_CREATE_PICTURE (display)) {
	if (!xrender_format) {
	    if (visual) {
		xrender_format = XRenderFindVisualFormat (display->display, visual);
	    } else if (depth == 1) {
		xrender_format =
		    _cairo_xlib_display_get_xrender_format (display,
							    CAIRO_FORMAT_A1);
	    }
	}
    }

    cairo_device_release (&display->base);

    _cairo_surface_init (&surface->base,
			 &cairo_xlib_surface_backend,
			 screen->device,
			 _xrender_format_to_content (xrender_format),
			 FALSE); /* is_vector */

    surface->screen = screen;
    surface->compositor = display->compositor;
    surface->shm = NULL;
    surface->fallback = 0;

    surface->drawable = drawable;
    surface->owns_pixmap = FALSE;
    surface->use_pixmap = 0;
    surface->width = width;
    surface->height = height;

    surface->picture = None;
    surface->precision = PolyModePrecise;

    surface->embedded_source.picture = None;

    surface->visual = visual;
    surface->xrender_format = xrender_format;
    surface->depth = depth;

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

    cairo_list_add (&surface->link, &screen->surfaces);

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

static cairo_bool_t valid_size (int width, int height)
{
    /* Note: the minimum surface size allowed in the X protocol is 1x1.
     * However, as we historically did not check the minimum size we
     * allowed applications to lie and set the correct size later (one hopes).
     * To preserve compatibility we must allow applications to use
     * 0x0 surfaces.
     */
    return (width  >= 0 && width  <= XLIB_COORD_MAX &&
	    height >= 0 && height <= XLIB_COORD_MAX);
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
 *
 * Since: 1.0
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

    if (! valid_size (width, height)) {
	/* you're lying, and you know it! */
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_SIZE));
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
 *
 * Since: 1.0
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

    if (! valid_size (width, height))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_SIZE));

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
 *
 * Since: 1.0
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

    if (! valid_size (width, height))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_SIZE));

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
 *
 * Since: 1.0
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
	_cairo_surface_set_error (abstract_surface,
				  _cairo_error (CAIRO_STATUS_SURFACE_FINISHED));
	return;
    }

    if (! _cairo_surface_is_xlib (abstract_surface)) {
	_cairo_surface_set_error (abstract_surface,
				  _cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH));
	return;
    }

    if (surface->width == width && surface->height == height)
	return;

    if (! valid_size (width, height)) {
	_cairo_surface_set_error (abstract_surface,
				  _cairo_error (CAIRO_STATUS_INVALID_SIZE));
	return;
    }

    status = _cairo_surface_flush (abstract_surface, 0);
    if (unlikely (status)) {
	_cairo_surface_set_error (abstract_surface, status);
	return;
    }

    _cairo_xlib_surface_discard_shm (surface);

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
 *
 * Since: 1.0
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

    if (! valid_size (width, height)) {
	status = _cairo_surface_set_error (abstract_surface,
		                           _cairo_error (CAIRO_STATUS_INVALID_SIZE));
	return;
    }

    /* XXX: and what about this case? */
    if (surface->owns_pixmap)
	return;

    status = _cairo_surface_flush (abstract_surface, 0);
    if (unlikely (status)) {
	_cairo_surface_set_error (abstract_surface, status);
	return;
    }

    if (surface->drawable != drawable) {
        cairo_xlib_display_t *display;

        status = _cairo_xlib_display_acquire (surface->base.device, &display);
        if (unlikely (status))
            return;

	X_DEBUG ((display->display, "set_drawable (drawable=%x)", (unsigned int) drawable));

	if (surface->picture != None) {
	    XRenderFreePicture (display->display, surface->picture);
	    if (unlikely (status)) {
		status = _cairo_surface_set_error (&surface->base, status);
		return;
	    }

	    surface->picture = None;
	}

        cairo_device_release (&display->base);

	surface->drawable = drawable;
    }

    if (surface->width != width || surface->height != height) {
	_cairo_xlib_surface_discard_shm (surface);

	surface->width = width;
	surface->height = height;
    }
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

#endif /* !CAIRO_HAS_XLIB_XCB_FUNCTIONS */
