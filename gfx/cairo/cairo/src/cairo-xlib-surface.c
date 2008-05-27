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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
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
 */

#include "cairoint.h"

#include "cairo-xlib-private.h"
#include "cairo-xlib-surface-private.h"
#include "cairo-clip-private.h"

#include <X11/Xutil.h> /* for XDestroyImage */

/* Xlib doesn't define a typedef, so define one ourselves */
typedef int (*cairo_xlib_error_func_t) (Display     *display,
					XErrorEvent *event);

static cairo_surface_t *
_cairo_xlib_surface_create_internal (Display		       *dpy,
				     Drawable		        drawable,
				     Screen		       *screen,
				     Visual		       *visual,
				     XRenderPictFormat	       *xrender_format,
				     int			width,
				     int			height,
				     int			depth);

static cairo_status_t
_cairo_xlib_surface_ensure_gc (cairo_xlib_surface_t *surface);

static void
_cairo_xlib_surface_ensure_src_picture (cairo_xlib_surface_t *surface);

static void
_cairo_xlib_surface_ensure_dst_picture (cairo_xlib_surface_t *surface);

static cairo_bool_t
_cairo_surface_is_xlib (cairo_surface_t *surface);

static cairo_bool_t
_native_byte_order_lsb (void);

static cairo_int_status_t
_cairo_xlib_surface_show_glyphs (void                *abstract_dst,
				 cairo_operator_t     op,
				 cairo_pattern_t     *src_pattern,
				 cairo_glyph_t       *glyphs,
				 int		      num_glyphs,
				 cairo_scaled_font_t *scaled_font);

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

#define CAIRO_SURFACE_RENDER_HAS_FILL_RECTANGLE(surface)		CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 1)
#define CAIRO_SURFACE_RENDER_HAS_FILL_RECTANGLES(surface)		CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 1)

#define CAIRO_SURFACE_RENDER_HAS_DISJOINT(surface)			CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 2)
#define CAIRO_SURFACE_RENDER_HAS_CONJOINT(surface)			CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 2)

#define CAIRO_SURFACE_RENDER_HAS_TRAPEZOIDS(surface)		CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 4)
#define CAIRO_SURFACE_RENDER_HAS_TRIANGLES(surface)		CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 4)
#define CAIRO_SURFACE_RENDER_HAS_TRISTRIP(surface)			CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 4)
#define CAIRO_SURFACE_RENDER_HAS_TRIFAN(surface)			CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 4)

#define CAIRO_SURFACE_RENDER_HAS_PICTURE_TRANSFORM(surface)	CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 6)
#define CAIRO_SURFACE_RENDER_HAS_FILTERS(surface)	CAIRO_SURFACE_RENDER_AT_LEAST((surface), 0, 6)

static XRenderPictFormat *
_CAIRO_FORMAT_TO_XRENDER_FORMAT(Display *dpy, cairo_format_t format)
{
    int	pict_format;
    switch (format) {
    case CAIRO_FORMAT_A1:
	pict_format = PictStandardA1; break;
    case CAIRO_FORMAT_A8:
	pict_format = PictStandardA8; break;
    case CAIRO_FORMAT_RGB24:
	pict_format = PictStandardRGB24; break;
    case CAIRO_FORMAT_ARGB32:
    default:
	pict_format = PictStandardARGB32; break;
    }
    return XRenderFindStandardFormat (dpy, pict_format);
}

static cairo_surface_t *
_cairo_xlib_surface_create_similar_with_format (void	       *abstract_src,
						cairo_format_t	format,
						int		width,
						int		height)
{
    cairo_xlib_surface_t *src = abstract_src;
    Display *dpy = src->dpy;
    Pixmap pix;
    cairo_xlib_surface_t *surface;
    XRenderPictFormat *xrender_format = _CAIRO_FORMAT_TO_XRENDER_FORMAT (dpy,
									 format);

    /* As a good first approximation, if the display doesn't have even
     * the most elementary RENDER operation, then we're better off
     * using image surfaces for all temporary operations, so return NULL
     * and let the fallback code happen.
     */
    if (xrender_format == NULL || ! CAIRO_SURFACE_RENDER_HAS_COMPOSITE (src))
	return NULL;

    pix = XCreatePixmap (dpy, src->drawable,
			 width <= 0 ? 1 : width, height <= 0 ? 1 : height,
			 xrender_format->depth);

    surface = (cairo_xlib_surface_t *)
	      _cairo_xlib_surface_create_internal (dpy, pix,
		                                   src->screen, NULL,
						   xrender_format,
						   width, height,
						   xrender_format->depth);
    if (surface->base.status) {
	XFreePixmap (dpy, pix);
	return &surface->base;
    }

    surface->owns_pixmap = TRUE;

    return &surface->base;
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
    XRenderPictFormat *xrender_format = src->xrender_format;
    cairo_xlib_surface_t *surface;
    Pixmap pix;

    _cairo_xlib_display_notify (src->screen_info->display);

    /* Start by examining the surface's XRenderFormat, or if it
     * doesn't have one, then look one up through its visual (in the
     * case of a bitmap, it won't even have that). */
    if (xrender_format == NULL && src->visual != NULL)
        xrender_format = XRenderFindVisualFormat (src->dpy, src->visual);

    /* If we never found an XRenderFormat or if it isn't compatible
     * with the content being requested, then we fallback to just
     * constructing a cairo_format_t instead, (which will fairly
     * arbitrarily pick a visual/depth for the similar surface.
     */
    if (xrender_format == NULL ||
	_xrender_format_to_content (xrender_format) != content)
    {
	return _cairo_xlib_surface_create_similar_with_format (abstract_src,
							       _cairo_format_from_content (content),
							       width, height);
    }

    /* We've got a compatible XRenderFormat now, which means the
     * similar surface will match the existing surface as closely in
     * visual/depth etc. as possible. */
    pix = XCreatePixmap (src->dpy, src->drawable,
			 width <= 0 ? 1 : width, height <= 0 ? 1 : height,
			 xrender_format->depth);

    surface = (cairo_xlib_surface_t *)
	      _cairo_xlib_surface_create_internal (src->dpy, pix,
		                                   src->screen, src->visual,
						   xrender_format,
						   width, height,
						   xrender_format->depth);
    if (surface->base.status != CAIRO_STATUS_SUCCESS) {
	XFreePixmap (src->dpy, pix);
	return &surface->base;
    }

    surface->owns_pixmap = TRUE;

    return &surface->base;
}

static cairo_status_t
_cairo_xlib_surface_finish (void *abstract_surface)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    cairo_xlib_display_t *display = surface->screen_info ?
	                            surface->screen_info->display :
				    NULL;
    cairo_status_t        status  = CAIRO_STATUS_SUCCESS;

    if (surface->owns_pixmap) {
	cairo_status_t status2;

	if (surface->dst_picture != None) {
	    status2 = _cairo_xlib_display_queue_resource (display,
							  XRenderFreePicture,
							  surface->dst_picture);
	    if (status2 == CAIRO_STATUS_SUCCESS)
		surface->dst_picture = None;
	    else if (status == CAIRO_STATUS_SUCCESS)
		status = status2;
	}

	if (surface->src_picture != None) {
	    status2 = _cairo_xlib_display_queue_resource (display,
							  XRenderFreePicture,
							  surface->src_picture);
	    if (status2 == CAIRO_STATUS_SUCCESS)
		surface->src_picture = None;
	    else if (status == CAIRO_STATUS_SUCCESS)
		status = status2;
	}

	status2 = _cairo_xlib_display_queue_resource (display,
		                           (cairo_xlib_notify_resource_func) XFreePixmap,
					   surface->drawable);
	if (status2 == CAIRO_STATUS_SUCCESS) {
	    surface->owns_pixmap = FALSE;
	    surface->drawable = None;
	} else if (status == CAIRO_STATUS_SUCCESS)
	    status = status2;
    } else {
	if (surface->dst_picture != None)
	    XRenderFreePicture (surface->dpy, surface->dst_picture);

	if (surface->src_picture != None)
	    XRenderFreePicture (surface->dpy, surface->src_picture);
    }

    if (surface->gc != NULL) {
	cairo_status_t status2;
	status2 = _cairo_xlib_screen_put_gc (surface->screen_info,
		                             surface->depth,
				             surface->gc,
				             surface->have_clip_rects);
	surface->gc = NULL;
	if (status == CAIRO_STATUS_SUCCESS)
	    status = status2;
    }

    if (surface->clip_rects != surface->embedded_clip_rects)
	free (surface->clip_rects);

    if (surface->screen_info != NULL)
	_cairo_xlib_screen_info_destroy (surface->screen_info);

    if (surface->dpy != NULL) {
	_cairo_xlib_remove_close_display_hooks (surface->dpy, surface);
	surface->dpy = NULL;
    }

    return status;
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
	uint16_t *p = (uint16_t *)line;
	for (i = ximage->width; i; i--) {
	    *p = (((*p & 0x00ff) << 8) |
		  ((*p)          >> 8));
	    p++;
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
	uint32_t *p = (uint32_t *)line;
	for (i = ximage->width; i; i--) {
	    *p = (((*p & 0x000000ff) << 24) |
		  ((*p & 0x0000ff00) << 8) |
		  ((*p & 0x00ff0000) >> 8) |
		  ((*p)              >> 24));
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
	ximage->bitmap_bit_order != native_byte_order) {
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
    case 8:
    case 16:
    case 32:
	unit_bytes = ximage->bits_per_pixel / 8;
	break;
    default:
        /* This could be hit on some uncommon but possible cases,
	 * such as bpp=4. These are cases that libpixman can't deal
	 * with in any case.
	 */
	ASSERT_NOT_REACHED;
    }

    switch (unit_bytes) {
    case 1:
	return;
    case 2:
	_swap_ximage_2bytes (ximage);
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
static inline void
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

/* Given an 8-bit value, convert it to a field of 'width', shift it up
 *  to 'shift, and return it. */
static inline uint32_t
_field_from_8 (uint32_t field, int width, int shift)
{
    return _resize_field (field, 8, width) << shift;
}

static cairo_status_t
_get_image_surface (cairo_xlib_surface_t    *surface,
		    cairo_rectangle_int_t   *interest_rect,
		    cairo_image_surface_t  **image_out,
		    cairo_rectangle_int_t   *image_rect)
{
    cairo_int_status_t status;
    cairo_image_surface_t *image;
    XImage *ximage;
    unsigned short x1, y1, x2, y2;
    pixman_format_code_t pixman_format;
    cairo_format_masks_t xlib_masks;

    x1 = 0;
    y1 = 0;
    x2 = surface->width;
    y2 = surface->height;

    if (interest_rect) {
	cairo_rectangle_int_t rect;

	rect.x = interest_rect->x;
	rect.y = interest_rect->y;
	rect.width = interest_rect->width;
	rect.height = interest_rect->height;

	if (rect.x > x1)
	    x1 = rect.x;
	if (rect.y > y1)
	    y1 = rect.y;
	if (rect.x + rect.width < x2)
	    x2 = rect.x + rect.width;
	if (rect.y + rect.height < y2)
	    y2 = rect.y + rect.height;

	if (x1 >= x2 || y1 >= y2) {
	    *image_out = NULL;
	    return CAIRO_STATUS_SUCCESS;
	}
    }

    if (image_rect) {
	image_rect->x = x1;
	image_rect->y = y1;
	image_rect->width = x2 - x1;
	image_rect->height = y2 - y1;
    }

    /* XXX: This should try to use the XShm extension if available */

    if (surface->use_pixmap == 0)
    {
	cairo_xlib_error_func_t old_handler;

	old_handler = XSetErrorHandler (_noop_error_handler);

	ximage = XGetImage (surface->dpy,
			    surface->drawable,
			    x1, y1,
			    x2 - x1, y2 - y1,
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

    if (!ximage)
    {

	/* XGetImage from a window is dangerous because it can
	 * produce errors if the window is unmapped or partially
	 * outside the screen. We could check for errors and
	 * retry, but to keep things simple, we just create a
	 * temporary pixmap
	 */
	Pixmap pixmap;
	cairo_status_t status = _cairo_xlib_surface_ensure_gc (surface);
	if (status)
	    return status;

	pixmap = XCreatePixmap (surface->dpy,
				       surface->drawable,
				       x2 - x1, y2 - y1,
				       surface->depth);
	if (pixmap) {
	    XCopyArea (surface->dpy, surface->drawable, pixmap, surface->gc,
		       x1, y1, x2 - x1, y2 - y1, 0, 0);

	    ximage = XGetImage (surface->dpy,
				pixmap,
				0, 0,
				x2 - x1, y2 - y1,
				AllPlanes, ZPixmap);

	    XFreePixmap (surface->dpy, pixmap);
	}
    }
    if (!ximage)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    _swap_ximage_to_native (ximage);

    xlib_masks.bpp = ximage->bits_per_pixel;
    xlib_masks.alpha_mask = surface->a_mask;
    xlib_masks.red_mask = surface->r_mask;
    xlib_masks.green_mask = surface->g_mask;
    xlib_masks.blue_mask = surface->b_mask;

    status = _pixman_format_from_masks (&xlib_masks, &pixman_format);
    if (status == CAIRO_STATUS_SUCCESS) {
	image = (cairo_image_surface_t*)
	    _cairo_image_surface_create_with_pixman_format ((unsigned char *) ximage->data,
							    pixman_format,
							    ximage->width,
							    ximage->height,
							    ximage->bytes_per_line);
	status = image->base.status;
	if (status) {
	    XDestroyImage (ximage);
	    return status;
	}

	/* Let the surface take ownership of the data */
	_cairo_image_surface_assume_ownership_of_data (image);
	ximage->data = NULL;
    } else {
	cairo_format_t format;
	unsigned char *data;
	uint32_t *row;
	uint32_t in_pixel, out_pixel;
	unsigned int rowstride;
	uint32_t a_mask=0, r_mask=0, g_mask=0, b_mask=0;
	int a_width=0, r_width=0, g_width=0, b_width=0;
	int a_shift=0, r_shift=0, g_shift=0, b_shift=0;
	int x, y;
	XColor *colors = NULL;

	/* The visual we are dealing with is not supported by the
	 * standard pixman formats. So we must first convert the data
	 * to a supported format. */
	if (surface->visual->class == TrueColor) {
	    cairo_bool_t has_color;
	    cairo_bool_t has_alpha;

	    has_color = (surface->r_mask ||
			 surface->g_mask ||
			 surface->b_mask);
	    has_alpha = surface->a_mask;

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
	    cairo_xlib_visual_info_t *visual_info;

	    format = CAIRO_FORMAT_RGB24;

	    status = _cairo_xlib_screen_get_visual_info (surface->screen_info,
							 surface->visual,
							 &visual_info);
	    if (status) {
		XDestroyImage (ximage);
		return status;
	    }

	    colors = visual_info->colors;
	}

	image = (cairo_image_surface_t *) cairo_image_surface_create
	    (format, ximage->width, ximage->height);
	status = image->base.status;
	if (status) {
	    XDestroyImage (ximage);
	    return status;
	}

	data = cairo_image_surface_get_data (&image->base);
	rowstride = cairo_image_surface_get_stride (&image->base) >> 2;
	row = (uint32_t *) data;
	for (y = 0; y < ximage->height; y++) {
	    for (x = 0; x < ximage->width; x++) {
		in_pixel = XGetPixel (ximage, x, y);
		if (surface->visual->class == TrueColor) {
		    out_pixel = (
			_field_to_8 (in_pixel & a_mask, a_width, a_shift) << 24 |
			_field_to_8 (in_pixel & r_mask, r_width, r_shift) << 16 |
			_field_to_8 (in_pixel & g_mask, g_width, g_shift) << 8 |
			_field_to_8 (in_pixel & b_mask, b_width, b_shift));
		} else {
		    XColor *color;
		    color = &colors[in_pixel & 0xff];
		    out_pixel = (
			_field_to_8 (color->red,   16, 0) << 16 |
			_field_to_8 (color->green, 16, 0) << 8 |
			_field_to_8 (color->blue,  16, 0));
		}
		row[x] = out_pixel;
	    }
	    row += rowstride;
	}
    }

    XDestroyImage (ximage);

    *image_out = image;
    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_xlib_surface_ensure_src_picture (cairo_xlib_surface_t    *surface)
{
    if (!surface->src_picture)
    {
	XRenderPictureAttributes pa;
	int mask = 0;

	pa.subwindow_mode = IncludeInferiors;
	mask |= CPSubwindowMode;

	surface->src_picture = XRenderCreatePicture (surface->dpy,
						     surface->drawable,
						     surface->xrender_format,
						     mask, &pa);
    }
}

static void
_cairo_xlib_surface_set_picture_clip_rects (cairo_xlib_surface_t *surface)
{
    if (surface->have_clip_rects) {
	XRenderSetPictureClipRectangles (surface->dpy, surface->dst_picture,
					 0, 0,
					 surface->clip_rects,
					 surface->num_clip_rects);
    } else {
	XRenderPictureAttributes pa;
	pa.clip_mask = None;
	XRenderChangePicture (surface->dpy, surface->dst_picture,
			      CPClipMask, &pa);
    }

    surface->clip_dirty &= ~CAIRO_XLIB_SURFACE_CLIP_DIRTY_PICTURE;
}

static void
_cairo_xlib_surface_set_gc_clip_rects (cairo_xlib_surface_t *surface)
{
    if (surface->have_clip_rects) {
	XSetClipRectangles(surface->dpy, surface->gc,
			   0, 0,
			   surface->clip_rects,
			   surface->num_clip_rects, YXSorted);
    } else
	XSetClipMask (surface->dpy, surface->gc, None);

    surface->clip_dirty &= ~CAIRO_XLIB_SURFACE_CLIP_DIRTY_GC;
}

static void
_cairo_xlib_surface_ensure_dst_picture (cairo_xlib_surface_t    *surface)
{
    if (!surface->dst_picture) {
	surface->dst_picture = XRenderCreatePicture (surface->dpy,
						     surface->drawable,
						     surface->xrender_format,
						     0, NULL);
	_cairo_xlib_surface_set_picture_clip_rects (surface);
    } else if (surface->clip_dirty & CAIRO_XLIB_SURFACE_CLIP_DIRTY_PICTURE)
	_cairo_xlib_surface_set_picture_clip_rects (surface);
}

static cairo_status_t
_cairo_xlib_surface_ensure_gc (cairo_xlib_surface_t *surface)
{
    XGCValues gcv;

    if (surface->gc == NULL) {
	surface->gc = _cairo_xlib_screen_get_gc (surface->screen_info,
						 surface->depth);
	if (surface->gc == NULL) {
	    gcv.graphics_exposures = False;
	    surface->gc = XCreateGC (surface->dpy, surface->drawable,
				     GCGraphicsExposures, &gcv);
	    if (!surface->gc)
		return _cairo_error (CAIRO_STATUS_NO_MEMORY);
	}

	_cairo_xlib_surface_set_gc_clip_rects (surface);
    } else if (surface->clip_dirty & CAIRO_XLIB_SURFACE_CLIP_DIRTY_GC)
	_cairo_xlib_surface_set_gc_clip_rects (surface);

    return CAIRO_STATUS_SUCCESS;
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
    XImage ximage;
    cairo_format_masks_t image_masks;
    int native_byte_order = _native_byte_order_lsb () ? LSBFirst : MSBFirst;
    cairo_status_t status;
    cairo_bool_t own_data;
    unsigned long *rgb333_to_pseudocolor = NULL;

    _pixman_format_to_masks (image->pixman_format, &image_masks);

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

    if (image_masks.red_mask   == surface->r_mask &&
	image_masks.green_mask == surface->g_mask &&
	image_masks.blue_mask  == surface->b_mask)
    {
	ximage.bits_per_pixel = image_masks.bpp;
	ximage.bytes_per_line = image->stride;
	ximage.data = (char *)image->data;
	own_data = FALSE;
	XInitImage (&ximage);
    } else {
	unsigned int stride, rowstride;
	int x, y;
	uint32_t in_pixel, out_pixel, *row;
	int a_width=0, r_width=0, g_width=0, b_width=0;
	int a_shift=0, r_shift=0, g_shift=0, b_shift=0;

	if (surface->depth > 16) {
	    ximage.bits_per_pixel = 32;
	} else if (surface->depth > 8) {
	    ximage.bits_per_pixel = 16;
	} else if (surface->depth > 1) {
	    ximage.bits_per_pixel = 8;
	} else {
	    ximage.bits_per_pixel = 1;
	}
	stride = CAIRO_STRIDE_FOR_WIDTH_BPP (ximage.width,
					     ximage.bits_per_pixel);
	ximage.bytes_per_line = stride;
	ximage.data = _cairo_malloc_ab (stride, ximage.height);
	if (ximage.data == NULL)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	own_data = TRUE;

	XInitImage (&ximage);

	if (surface->visual->class == TrueColor) {
	    _characterize_field (surface->a_mask, &a_width, &a_shift);
	    _characterize_field (surface->r_mask, &r_width, &r_shift);
	    _characterize_field (surface->g_mask, &g_width, &g_shift);
	    _characterize_field (surface->b_mask, &b_width, &b_shift);
	} else {
	    cairo_xlib_visual_info_t *visual_info;

	    status = _cairo_xlib_screen_get_visual_info (surface->screen_info,
							 surface->visual,
							 &visual_info);
	    if (status)
		goto BAIL;

	    rgb333_to_pseudocolor = visual_info->rgb333_to_pseudocolor;
	}

	rowstride = cairo_image_surface_get_stride (&image->base) >> 2;
	row = (uint32_t *) cairo_image_surface_get_data (&image->base);
	for (y = 0; y < ximage.height; y++) {
	    for (x = 0; x < ximage.width; x++) {
		int a, r, g, b;
		in_pixel = row[x];
		a = (in_pixel >> 24) & 0xff;
		r = (in_pixel >> 16) & 0xff;
		g = (in_pixel >>  8) & 0xff;
		b = (in_pixel      ) & 0xff;
		if (surface->visual->class == TrueColor)
		    out_pixel = (_field_from_8 (a, a_width, a_shift) |
				 _field_from_8 (r, r_width, r_shift) |
				 _field_from_8 (g, g_width, g_shift) |
				 _field_from_8 (b, b_width, b_shift));
		else
		    out_pixel = rgb333_to_pseudocolor[_field_from_8 (r, 3, 6) |
						      _field_from_8 (g, 3, 3) |
						      _field_from_8 (b, 3, 0)];
		XPutPixel (&ximage, x, y, out_pixel);
	    }
	    row += rowstride;
	}
    }

    status = _cairo_xlib_surface_ensure_gc (surface);
    if (status)
	goto BAIL;

    XPutImage(surface->dpy, surface->drawable, surface->gc,
	      &ximage, src_x, src_y, dst_x, dst_y,
	      width, height);

  BAIL:
    if (own_data)
	free (ximage.data);

    return status;
}

static cairo_status_t
_cairo_xlib_surface_acquire_source_image (void                    *abstract_surface,
					  cairo_image_surface_t  **image_out,
					  void                   **image_extra)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    cairo_image_surface_t *image;
    cairo_status_t status;

    _cairo_xlib_display_notify (surface->screen_info->display);

    status = _get_image_surface (surface, NULL, &image, NULL);
    if (status)
	return status;

    *image_out = image;
    *image_extra = NULL;

    return CAIRO_STATUS_SUCCESS;
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

    _cairo_xlib_display_notify (surface->screen_info->display);

    status = _get_image_surface (surface, interest_rect, &image, image_rect_out);
    if (status)
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
static cairo_bool_t
_cairo_xlib_surface_same_screen (cairo_xlib_surface_t *dst,
				 cairo_xlib_surface_t *src)
{
    return dst->dpy == src->dpy && dst->screen == src->screen;
}

static cairo_status_t
_cairo_xlib_surface_clone_similar (void			*abstract_surface,
				   cairo_surface_t	*src,
				   int                   src_x,
				   int                   src_y,
				   int                   width,
				   int                   height,
				   cairo_surface_t     **clone_out)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    cairo_xlib_surface_t *clone;
    cairo_status_t status;

    _cairo_xlib_display_notify (surface->screen_info->display);

    if (src->backend == surface->base.backend ) {
	cairo_xlib_surface_t *xlib_src = (cairo_xlib_surface_t *)src;

	if (_cairo_xlib_surface_same_screen (surface, xlib_src)) {
	    *clone_out = cairo_surface_reference (src);

	    return CAIRO_STATUS_SUCCESS;
	}
    } else if (_cairo_surface_is_image (src)) {
	cairo_image_surface_t *image_src = (cairo_image_surface_t *)src;

	if (! CAIRO_FORMAT_VALID (image_src->format))
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	clone = (cairo_xlib_surface_t *)
	    _cairo_xlib_surface_create_similar_with_format (surface, image_src->format,
						image_src->width, image_src->height);
	if (clone == NULL)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	if (clone->base.status)
	    return clone->base.status;

	status = _draw_image_surface (clone, image_src, src_x, src_y,
			              width, height, src_x, src_y);
	if (status) {
	    cairo_surface_destroy (&clone->base);
	    return status;
	}

	*clone_out = &clone->base;

	return CAIRO_STATUS_SUCCESS;
    }

    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_status_t
_cairo_xlib_surface_set_matrix (cairo_xlib_surface_t *surface,
				cairo_matrix_t	     *matrix)
{
    XTransform xtransform;

    if (!surface->src_picture)
	return CAIRO_STATUS_SUCCESS;

    xtransform.matrix[0][0] = _cairo_fixed_16_16_from_double (matrix->xx);
    xtransform.matrix[0][1] = _cairo_fixed_16_16_from_double (matrix->xy);
    xtransform.matrix[0][2] = _cairo_fixed_16_16_from_double (matrix->x0);

    xtransform.matrix[1][0] = _cairo_fixed_16_16_from_double (matrix->yx);
    xtransform.matrix[1][1] = _cairo_fixed_16_16_from_double (matrix->yy);
    xtransform.matrix[1][2] = _cairo_fixed_16_16_from_double (matrix->y0);

    xtransform.matrix[2][0] = 0;
    xtransform.matrix[2][1] = 0;
    xtransform.matrix[2][2] = 1 << 16;

    if (memcmp (&xtransform, &surface->xtransform, sizeof (XTransform)) == 0)
	return CAIRO_STATUS_SUCCESS;

    if (!CAIRO_SURFACE_RENDER_HAS_PICTURE_TRANSFORM (surface))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    XRenderSetPictureTransform (surface->dpy, surface->src_picture, &xtransform);
    surface->xtransform = xtransform;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xlib_surface_set_filter (cairo_xlib_surface_t *surface,
				cairo_filter_t	     filter)
{
    const char *render_filter;

    if (!surface->src_picture)
	return CAIRO_STATUS_SUCCESS;

    if (surface->filter == filter)
	return CAIRO_STATUS_SUCCESS;

    if (!CAIRO_SURFACE_RENDER_HAS_FILTERS (surface))
    {
	if (filter == CAIRO_FILTER_FAST || filter == CAIRO_FILTER_NEAREST)
	    return CAIRO_STATUS_SUCCESS;

	return CAIRO_INT_STATUS_UNSUPPORTED;
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

    XRenderSetPictureFilter (surface->dpy, surface->src_picture,
			     (char *) render_filter, NULL, 0);
    surface->filter = filter;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xlib_surface_set_repeat (cairo_xlib_surface_t *surface, int repeat)
{
    XRenderPictureAttributes pa;
    unsigned long	     mask;

    if (!surface->src_picture)
	return CAIRO_STATUS_SUCCESS;

    if (surface->repeat == repeat)
	return CAIRO_STATUS_SUCCESS;

    mask = CPRepeat;
    pa.repeat = repeat;

    XRenderChangePicture (surface->dpy, surface->src_picture, mask, &pa);
    surface->repeat = repeat;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_xlib_surface_set_attributes (cairo_xlib_surface_t	    *surface,
				    cairo_surface_attributes_t	    *attributes)
{
    cairo_int_status_t status;

    _cairo_xlib_surface_ensure_src_picture (surface);

    status = _cairo_xlib_surface_set_matrix (surface, &attributes->matrix);
    if (status)
	return status;

    switch (attributes->extend) {
    case CAIRO_EXTEND_NONE:
	status = _cairo_xlib_surface_set_repeat (surface, 0);
	break;
    case CAIRO_EXTEND_REPEAT:
	status = _cairo_xlib_surface_set_repeat (surface, 1);
	break;
    case CAIRO_EXTEND_REFLECT:
    case CAIRO_EXTEND_PAD:
    default:
	status = CAIRO_INT_STATUS_UNSUPPORTED;
    }
    if (status)
	return status;

    status = _cairo_xlib_surface_set_filter (surface, attributes->filter);
    if (status)
	return status;

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
    if (!_cairo_xlib_surface_same_screen (dst, src))
	return FALSE;

    /* same depth (for core) */
    if (src->depth != dst->depth)
	return FALSE;

    /* if Render is supported, match picture formats */
    if (src->xrender_format != NULL && src->xrender_format == dst->xrender_format)
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

/* Returns true if the given operator and source-alpha combination
 * requires alpha compositing to complete.
 */
static cairo_bool_t
_operator_needs_alpha_composite (cairo_operator_t op,
				 cairo_bool_t     surface_has_alpha)
{
    if (op == CAIRO_OPERATOR_SOURCE ||
	(!surface_has_alpha &&
	 (op == CAIRO_OPERATOR_OVER ||
	  op == CAIRO_OPERATOR_ATOP ||
	  op == CAIRO_OPERATOR_IN)))
	return FALSE;

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
				 cairo_pattern_t      *src_pattern,
				 cairo_bool_t	       have_mask)

{
    if (!dst->buggy_repeat)
	return DO_RENDER;

    if (src_pattern->type == CAIRO_PATTERN_TYPE_SURFACE)
    {
	cairo_surface_pattern_t *surface_pattern = (cairo_surface_pattern_t *)src_pattern;

	if (_cairo_matrix_is_integer_translation (&src_pattern->matrix, NULL, NULL) &&
	    src_pattern->extend == CAIRO_EXTEND_REPEAT)
	{
	    /* This is the case where we have the bug involving
	     * untransformed repeating source patterns with off-screen
	     * video memory; reject some cases where a core protocol
	     * fallback is impossible.
	     */
	    if (have_mask ||
		!(op == CAIRO_OPERATOR_SOURCE || op == CAIRO_OPERATOR_OVER))
		return DO_UNSUPPORTED;

	    if (_cairo_surface_is_xlib (surface_pattern->surface)) {
		cairo_xlib_surface_t *src = (cairo_xlib_surface_t *)surface_pattern->surface;

		if (op == CAIRO_OPERATOR_OVER && _surface_has_alpha (src))
		    return DO_UNSUPPORTED;

		/* If these are on the same screen but otherwise incompatible,
		 * make a copy as core drawing can't cross depths and doesn't
		 * work right across visuals of the same depth
		 */
		if (_cairo_xlib_surface_same_screen (dst, src) &&
		    !_surfaces_compatible (dst, src))
		    return DO_UNSUPPORTED;
	    }
	}

	/* Check for the other bug involving repeat patterns with general
	 * transforms. */
	if (!_cairo_matrix_is_integer_translation (&src_pattern->matrix, NULL, NULL) &&
	    src_pattern->extend == CAIRO_EXTEND_REPEAT)
	    return DO_UNSUPPORTED;
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
    cairo_bool_t is_integer_translation =
	_cairo_matrix_is_integer_translation (&src_attr->matrix, NULL, NULL);
    cairo_bool_t needs_alpha_composite;

    if (!_cairo_surface_is_xlib (&src->base))
	return DO_UNSUPPORTED;

    needs_alpha_composite =
	_operator_needs_alpha_composite (op, _surface_has_alpha (src));

    if (!have_mask &&
	is_integer_translation &&
	src_attr->extend == CAIRO_EXTEND_NONE &&
	!needs_alpha_composite &&
	_surfaces_compatible(src, dst))
    {
	return DO_XCOPYAREA;
    }

    if (dst->buggy_repeat &&
	is_integer_translation &&
	src_attr->extend == CAIRO_EXTEND_REPEAT &&
	(src->width != 1 || src->height != 1))
    {
	if (!have_mask &&
	    !needs_alpha_composite &&
	    _surfaces_compatible (dst, src))
	{
	    return DO_XTILE;
	}

	return DO_UNSUPPORTED;
    }

    if (!CAIRO_SURFACE_RENDER_HAS_COMPOSITE (src))
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
    default:
	return PictOpOver;
    }
}

static cairo_int_status_t
_cairo_xlib_surface_composite (cairo_operator_t		op,
			       cairo_pattern_t		*src_pattern,
			       cairo_pattern_t		*mask_pattern,
			       void			*abstract_dst,
			       int			src_x,
			       int			src_y,
			       int			mask_x,
			       int			mask_y,
			       int			dst_x,
			       int			dst_y,
			       unsigned int		width,
			       unsigned int		height)
{
    cairo_surface_attributes_t	src_attr, mask_attr;
    cairo_xlib_surface_t	*dst = abstract_dst;
    cairo_xlib_surface_t	*src;
    cairo_xlib_surface_t	*mask;
    cairo_int_status_t		status;
    composite_operation_t       operation;
    int				itx, ity;
    cairo_bool_t		is_integer_translation;

    _cairo_xlib_display_notify (dst->screen_info->display);

    if (!CAIRO_SURFACE_RENDER_HAS_COMPOSITE (dst))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    operation = _categorize_composite_operation (dst, op, src_pattern,
						 mask_pattern != NULL);
    if (operation == DO_UNSUPPORTED)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = _cairo_pattern_acquire_surfaces (src_pattern, mask_pattern,
					      &dst->base,
					      src_x, src_y,
					      mask_x, mask_y,
					      width, height,
					      (cairo_surface_t **) &src,
					      (cairo_surface_t **) &mask,
					      &src_attr, &mask_attr);
    if (status)
	return status;

    /* check for fallback surfaces that we cannot handle ... */
    if (!_cairo_surface_is_xlib (&src->base)) {
	status = CAIRO_INT_STATUS_UNSUPPORTED;
	goto BAIL;
    }
    if (mask != NULL && !_cairo_surface_is_xlib (&mask->base)) {
	status = CAIRO_INT_STATUS_UNSUPPORTED;
	goto BAIL;
    }

    operation = _recategorize_composite_operation (dst, op, src, &src_attr,
						   mask_pattern != NULL);
    if (operation == DO_UNSUPPORTED) {
	status = CAIRO_INT_STATUS_UNSUPPORTED;
	goto BAIL;
    }

    switch (operation)
    {
    case DO_RENDER:
	status = _cairo_xlib_surface_set_attributes (src, &src_attr);
	if (status)
	    goto BAIL;

	_cairo_xlib_surface_ensure_dst_picture (dst);
	if (mask) {
	    status = _cairo_xlib_surface_set_attributes (mask, &mask_attr);
	    if (status)
		goto BAIL;

	    XRenderComposite (dst->dpy,
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
	    XRenderComposite (dst->dpy,
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
	status = _cairo_xlib_surface_ensure_gc (dst);
	if (status)
	    goto BAIL;
	XCopyArea (dst->dpy,
		   src->drawable,
		   dst->drawable,
		   dst->gc,
		   src_x + src_attr.x_offset,
		   src_y + src_attr.y_offset,
		   width, height,
		   dst_x, dst_y);
	break;

    case DO_XTILE:
	/* This case is only used for bug fallbacks, though it is theoretically
	 * applicable to the case where we don't have the RENDER extension as
	 * well.
	 *
	 * We've checked that we have a repeating unscaled source in
	 * _recategorize_composite_operation.
	 */

	status = _cairo_xlib_surface_ensure_gc (dst);
	if (status)
	    goto BAIL;
	is_integer_translation = _cairo_matrix_is_integer_translation (&src_attr.matrix,
								       &itx, &ity);
	/* This is a pre-condition for DO_XTILE. */
	assert (is_integer_translation);

	XSetTSOrigin (dst->dpy, dst->gc,
		      - (itx + src_attr.x_offset), - (ity + src_attr.y_offset));
	XSetTile (dst->dpy, dst->gc, src->drawable);
	XSetFillStyle (dst->dpy, dst->gc, FillTiled);

	XFillRectangle (dst->dpy, dst->drawable, dst->gc,
			dst_x, dst_y, width, height);
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
							 dst_x, dst_y, width, height);

 BAIL:
    if (mask)
	_cairo_pattern_release_surface (mask_pattern, &mask->base, &mask_attr);

    _cairo_pattern_release_surface (src_pattern, &src->base, &src_attr);

    return status;
}

static cairo_int_status_t
_cairo_xlib_surface_fill_rectangles (void		     *abstract_surface,
				     cairo_operator_t	      op,
				     const cairo_color_t     *color,
				     cairo_rectangle_int_t   *rects,
				     int			      num_rects)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    XRenderColor render_color;
    XRectangle static_xrects[16];
    XRectangle *xrects = static_xrects;
    int i;

    _cairo_xlib_display_notify (surface->screen_info->display);

    if (!CAIRO_SURFACE_RENDER_HAS_FILL_RECTANGLE (surface))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    render_color.red   = color->red_short;
    render_color.green = color->green_short;
    render_color.blue  = color->blue_short;
    render_color.alpha = color->alpha_short;

    if (num_rects > ARRAY_LENGTH(static_xrects)) {
        xrects = _cairo_malloc_ab (num_rects, sizeof(XRectangle));
	if (xrects == NULL)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    for (i = 0; i < num_rects; i++) {
        xrects[i].x = rects[i].x;
        xrects[i].y = rects[i].y;
        xrects[i].width = rects[i].width;
        xrects[i].height = rects[i].height;
    }

    _cairo_xlib_surface_ensure_dst_picture (surface);
    XRenderFillRectangles (surface->dpy,
			   _render_operator (op),
			   surface->dst_picture,
			   &render_color, xrects, num_rects);

    if (xrects != static_xrects)
        free(xrects);

    return CAIRO_STATUS_SUCCESS;
}

/* Creates an A8 picture of size @width x @height, initialized with @color
 */
static Picture
_create_a8_picture (cairo_xlib_surface_t *surface,
		    XRenderColor         *color,
		    int                   width,
		    int                   height,
		    cairo_bool_t          repeat)
{
    XRenderPictureAttributes pa;
    unsigned long mask = 0;

    Pixmap pixmap = XCreatePixmap (surface->dpy, surface->drawable,
				   width <= 0 ? 1 : width,
				   height <= 0 ? 1 : height,
				   8);
    Picture picture;

    if (repeat) {
	pa.repeat = TRUE;
	mask = CPRepeat;
    }

    picture = XRenderCreatePicture (surface->dpy, pixmap,
				    XRenderFindStandardFormat (surface->dpy, PictStandardA8),
				    mask, &pa);
    XRenderFillRectangle (surface->dpy, PictOpSrc, picture, color,
			  0, 0, width, height);
    XFreePixmap (surface->dpy, pixmap);

    return picture;
}

/* Creates a temporary mask for the trapezoids covering the area
 * [@dst_x, @dst_y, @width, @height] of the destination surface.
 */
static Picture
_create_trapezoid_mask (cairo_xlib_surface_t *dst,
			cairo_trapezoid_t    *traps,
			int                   num_traps,
			int                   dst_x,
			int                   dst_y,
			int                   width,
			int                   height,
			XRenderPictFormat     *pict_format)
{
    XRenderColor transparent = { 0, 0, 0, 0 };
    XRenderColor solid = { 0xffff, 0xffff, 0xffff, 0xffff };
    Picture mask_picture, solid_picture;
    XTrapezoid *offset_traps;
    int i;

    /* This would be considerably simpler using XRenderAddTraps(), but since
     * we are only using this in the unbounded-operator case, we stick with
     * XRenderCompositeTrapezoids, which is available on older versions
     * of RENDER rather than conditionalizing. We should still hit an
     * optimization that avoids creating another intermediate surface on
     * the servers that have XRenderAddTraps().
     */
    mask_picture = _create_a8_picture (dst, &transparent, width, height, FALSE);
    if (num_traps == 0)
	return mask_picture;

    offset_traps = _cairo_malloc_ab (num_traps, sizeof (XTrapezoid));
    if (!offset_traps) {
	XRenderFreePicture (dst->dpy, mask_picture);
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	return None;
    }

    for (i = 0; i < num_traps; i++) {
	offset_traps[i].top = _cairo_fixed_to_16_16(traps[i].top) - 0x10000 * dst_y;
	offset_traps[i].bottom = _cairo_fixed_to_16_16(traps[i].bottom) - 0x10000 * dst_y;
	offset_traps[i].left.p1.x = _cairo_fixed_to_16_16(traps[i].left.p1.x) - 0x10000 * dst_x;
	offset_traps[i].left.p1.y = _cairo_fixed_to_16_16(traps[i].left.p1.y) - 0x10000 * dst_y;
	offset_traps[i].left.p2.x = _cairo_fixed_to_16_16(traps[i].left.p2.x) - 0x10000 * dst_x;
	offset_traps[i].left.p2.y = _cairo_fixed_to_16_16(traps[i].left.p2.y) - 0x10000 * dst_y;
	offset_traps[i].right.p1.x = _cairo_fixed_to_16_16(traps[i].right.p1.x) - 0x10000 * dst_x;
	offset_traps[i].right.p1.y = _cairo_fixed_to_16_16(traps[i].right.p1.y) - 0x10000 * dst_y;
	offset_traps[i].right.p2.x = _cairo_fixed_to_16_16(traps[i].right.p2.x) - 0x10000 * dst_x;
	offset_traps[i].right.p2.y = _cairo_fixed_to_16_16(traps[i].right.p2.y) - 0x10000 * dst_y;
    }

    solid_picture = _create_a8_picture (dst, &solid, width, height, TRUE);

    XRenderCompositeTrapezoids (dst->dpy, PictOpAdd,
				solid_picture, mask_picture,
				pict_format,
				0, 0,
				offset_traps, num_traps);

    XRenderFreePicture (dst->dpy, solid_picture);
    free (offset_traps);

    return mask_picture;
}

static cairo_int_status_t
_cairo_xlib_surface_composite_trapezoids (cairo_operator_t	op,
					  cairo_pattern_t	*pattern,
					  void			*abstract_dst,
					  cairo_antialias_t	antialias,
					  int			src_x,
					  int			src_y,
					  int			dst_x,
					  int			dst_y,
					  unsigned int		width,
					  unsigned int		height,
					  cairo_trapezoid_t	*traps,
					  int			num_traps)
{
    cairo_surface_attributes_t	attributes;
    cairo_xlib_surface_t	*dst = abstract_dst;
    cairo_xlib_surface_t	*src;
    cairo_int_status_t		status;
    composite_operation_t       operation;
    int				render_reference_x, render_reference_y;
    int				render_src_x, render_src_y;
    XRenderPictFormat		*pict_format;

    _cairo_xlib_display_notify (dst->screen_info->display);

    if (!CAIRO_SURFACE_RENDER_HAS_TRAPEZOIDS (dst))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    operation = _categorize_composite_operation (dst, op, pattern, TRUE);
    if (operation == DO_UNSUPPORTED)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = _cairo_pattern_acquire_surface (pattern, &dst->base,
					     src_x, src_y, width, height,
					     (cairo_surface_t **) &src,
					     &attributes);
    if (status)
	return status;

    operation = _recategorize_composite_operation (dst, op, src, &attributes, TRUE);
    if (operation == DO_UNSUPPORTED) {
	status = CAIRO_INT_STATUS_UNSUPPORTED;
	goto BAIL;
    }

    switch (antialias) {
    case CAIRO_ANTIALIAS_NONE:
	pict_format = XRenderFindStandardFormat (dst->dpy, PictStandardA1);
	break;
    case CAIRO_ANTIALIAS_GRAY:
    case CAIRO_ANTIALIAS_SUBPIXEL:
    case CAIRO_ANTIALIAS_DEFAULT:
    default:
	pict_format = XRenderFindStandardFormat (dst->dpy, PictStandardA8);
	break;
    }

    if (traps[0].left.p1.y < traps[0].left.p2.y) {
	render_reference_x = _cairo_fixed_integer_floor (traps[0].left.p1.x);
	render_reference_y = _cairo_fixed_integer_floor (traps[0].left.p1.y);
    } else {
	render_reference_x = _cairo_fixed_integer_floor (traps[0].left.p2.x);
	render_reference_y = _cairo_fixed_integer_floor (traps[0].left.p2.y);
    }

    render_src_x = src_x + render_reference_x - dst_x;
    render_src_y = src_y + render_reference_y - dst_y;

    _cairo_xlib_surface_ensure_dst_picture (dst);
    status = _cairo_xlib_surface_set_attributes (src, &attributes);
    if (status)
	goto BAIL;

    if (!_cairo_operator_bounded_by_mask (op)) {
	/* XRenderCompositeTrapezoids() creates a mask only large enough for the
	 * trapezoids themselves, but if the operator is unbounded, then we need
	 * to actually composite all the way out to the bounds, so we create
	 * the mask and composite ourselves. There actually would
	 * be benefit to doing this in all cases, since RENDER implementations
	 * will frequently create a too temporary big mask, ignoring destination
	 * bounds and clip. (XRenderAddTraps() could be used to make creating
	 * the mask somewhat cheaper.)
	 */
	Picture mask_picture = _create_trapezoid_mask (dst, traps, num_traps,
						       dst_x, dst_y, width, height,
						       pict_format);
	if (!mask_picture) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto BAIL;
	}

	XRenderComposite (dst->dpy,
			  _render_operator (op),
			  src->src_picture,
			  mask_picture,
			  dst->dst_picture,
			  src_x + attributes.x_offset,
			  src_y + attributes.y_offset,
			  0, 0,
			  dst_x, dst_y,
			  width, height);

	XRenderFreePicture (dst->dpy, mask_picture);

	status = _cairo_surface_composite_shape_fixup_unbounded (&dst->base,
								 &attributes, src->width, src->height,
								 width, height,
								 src_x, src_y,
								 0, 0,
								 dst_x, dst_y, width, height);

    } else {
        XTrapezoid xtraps_stack[16];
        XTrapezoid *xtraps = xtraps_stack;
        int i;

        if (num_traps > ARRAY_LENGTH(xtraps_stack)) {
            xtraps = _cairo_malloc_ab (num_traps, sizeof(XTrapezoid));
            if (xtraps == NULL) {
                status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
                goto BAIL;
            }
        }

        for (i = 0; i < num_traps; i++) {
            xtraps[i].top = _cairo_fixed_to_16_16(traps[i].top);
            xtraps[i].bottom = _cairo_fixed_to_16_16(traps[i].bottom);
            xtraps[i].left.p1.x = _cairo_fixed_to_16_16(traps[i].left.p1.x);
            xtraps[i].left.p1.y = _cairo_fixed_to_16_16(traps[i].left.p1.y);
            xtraps[i].left.p2.x = _cairo_fixed_to_16_16(traps[i].left.p2.x);
            xtraps[i].left.p2.y = _cairo_fixed_to_16_16(traps[i].left.p2.y);
            xtraps[i].right.p1.x = _cairo_fixed_to_16_16(traps[i].right.p1.x);
            xtraps[i].right.p1.y = _cairo_fixed_to_16_16(traps[i].right.p1.y);
            xtraps[i].right.p2.x = _cairo_fixed_to_16_16(traps[i].right.p2.x);
            xtraps[i].right.p2.y = _cairo_fixed_to_16_16(traps[i].right.p2.y);
        }

	XRenderCompositeTrapezoids (dst->dpy,
				    _render_operator (op),
				    src->src_picture, dst->dst_picture,
				    pict_format,
				    render_src_x + attributes.x_offset,
				    render_src_y + attributes.y_offset,
				    xtraps, num_traps);

        if (xtraps != xtraps_stack)
            free(xtraps);
    }

 BAIL:
    _cairo_pattern_release_surface (pattern, &src->base, &attributes);

    return status;
}

static cairo_int_status_t
_cairo_xlib_surface_set_clip_region (void           *abstract_surface,
				     cairo_region_t *region)
{
    cairo_xlib_surface_t *surface = abstract_surface;

    if (surface->have_clip_rects == FALSE && region == NULL)
	return CAIRO_STATUS_SUCCESS;

    if (surface->clip_rects != surface->embedded_clip_rects) {
	free (surface->clip_rects);
	surface->clip_rects = surface->embedded_clip_rects;
    }

    surface->have_clip_rects = FALSE;
    surface->num_clip_rects = 0;

    if (region != NULL) {
	cairo_box_int_t *boxes;
	cairo_status_t status;
	XRectangle *rects = NULL;
	int n_boxes, i;

	status = _cairo_region_get_boxes (region, &n_boxes, &boxes);
        if (status)
            return status;

	if (n_boxes > ARRAY_LENGTH (surface->embedded_clip_rects)) {
	    rects = _cairo_malloc_ab (n_boxes, sizeof(XRectangle));
	    if (rects == NULL) {
                _cairo_region_boxes_fini (region, boxes);
		return _cairo_error (CAIRO_STATUS_NO_MEMORY);
            }
	} else {
	    rects = surface->embedded_clip_rects;
	}

	for (i = 0; i < n_boxes; i++) {
	    rects[i].x = boxes[i].p1.x;
	    rects[i].y = boxes[i].p1.y;
	    rects[i].width = boxes[i].p2.x - boxes[i].p1.x;
	    rects[i].height = boxes[i].p2.y - boxes[i].p1.y;
	}

        _cairo_region_boxes_fini (region, boxes);

	surface->have_clip_rects = TRUE;
	surface->clip_rects = rects;
	surface->num_clip_rects = n_boxes;
    }

    surface->clip_dirty = CAIRO_XLIB_SURFACE_CLIP_DIRTY_ALL;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_xlib_surface_get_extents (void		         *abstract_surface,
				 cairo_rectangle_int_t   *rectangle)
{
    cairo_xlib_surface_t *surface = abstract_surface;

    rectangle->x = 0;
    rectangle->y = 0;

    rectangle->width  = surface->width;
    rectangle->height = surface->height;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_xlib_surface_get_font_options (void                  *abstract_surface,
				      cairo_font_options_t  *options)
{
    cairo_xlib_surface_t *surface = abstract_surface;

    *options = surface->screen_info->font_options;
}

static void
_cairo_xlib_surface_scaled_font_fini (cairo_scaled_font_t *scaled_font);

static void
_cairo_xlib_surface_scaled_glyph_fini (cairo_scaled_glyph_t *scaled_glyph,
				       cairo_scaled_font_t  *scaled_font);

static cairo_bool_t
_cairo_xlib_surface_is_similar (void		*surface_a,
	                        void		*surface_b,
				cairo_content_t	 content)
{
    cairo_xlib_surface_t *a = surface_a;
    cairo_xlib_surface_t *b = surface_b;
    XRenderPictFormat *xrender_format = b->xrender_format;

    if (!_cairo_xlib_surface_same_screen (a, b))
	return FALSE;

    /* now inspect the content to check that a is similar to b */
    if (xrender_format == NULL && b->visual != NULL)
        xrender_format = XRenderFindVisualFormat (b->dpy, b->visual);

    if (xrender_format == NULL ||
	_xrender_format_to_content (xrender_format) != content)
    {
	xrender_format = _CAIRO_FORMAT_TO_XRENDER_FORMAT (b->dpy,
		_cairo_format_from_content (content));
    }


    return a->xrender_format == xrender_format;
}

static cairo_status_t
_cairo_xlib_surface_reset (void *abstract_surface)
{
    cairo_xlib_surface_t *surface = abstract_surface;
    cairo_status_t status;

    status = _cairo_xlib_surface_set_clip_region (surface, NULL);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
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
    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_xlib_surface_set_clip_region,
    NULL, /* intersect_clip_path */
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
    NULL,  /* snapshot */
    _cairo_xlib_surface_is_similar,

    _cairo_xlib_surface_reset
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

static void
_cairo_xlib_surface_detach_display (Display *dpy, void *data)
{
    cairo_xlib_surface_t *surface = data;

    surface->dpy = NULL;

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

    if (surface->gc != NULL) {
	XFreeGC (dpy, surface->gc);
	surface->gc = NULL;
    }
}

static cairo_surface_t *
_cairo_xlib_surface_create_internal (Display		       *dpy,
				     Drawable		        drawable,
				     Screen		       *screen,
				     Visual		       *visual,
				     XRenderPictFormat	       *xrender_format,
				     int			width,
				     int			height,
				     int			depth)
{
    cairo_xlib_surface_t *surface;
    cairo_xlib_screen_info_t *screen_info;

    CAIRO_MUTEX_INITIALIZE ();

    screen_info = _cairo_xlib_screen_info_get (dpy, screen);
    if (screen_info == NULL)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    surface = malloc (sizeof (cairo_xlib_surface_t));
    if (surface == NULL) {
	_cairo_xlib_screen_info_destroy (screen_info);
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
    }

    if (! _cairo_xlib_add_close_display_hook (dpy,
	    _cairo_xlib_surface_detach_display, surface, surface)) {
	free (surface);
	_cairo_xlib_screen_info_destroy (screen_info);
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
    }

    if (xrender_format) {
	depth = xrender_format->depth;
    } else if (visual) {
	int j, k;

	/* This is ugly, but we have to walk over all visuals
	 * for the display to find the depth.
	 */
	for (j = 0; j < screen->ndepths; j++) {
	    Depth *d = &screen->depths[j];
	    for (k = 0; k < d->nvisuals; k++) {
		if (&d->visuals[k] == visual) {
		    depth = d->depth;
		    goto found;
		}
	    }
	}
    found:
	;
    }

    if (! XRenderQueryVersion (dpy, &surface->render_major, &surface->render_minor)) {
	surface->render_major = -1;
	surface->render_minor = -1;
    }

    if (CAIRO_SURFACE_RENDER_HAS_CREATE_PICTURE (surface)) {
	if (!xrender_format) {
	    if (visual)
		xrender_format = XRenderFindVisualFormat (dpy, visual);
	    else if (depth == 1)
		xrender_format = XRenderFindStandardFormat (dpy, PictStandardA1);
	}
    } else {
	xrender_format = NULL;
    }

    _cairo_surface_init (&surface->base, &cairo_xlib_surface_backend,
			 _xrender_format_to_content (xrender_format));

    surface->dpy = dpy;
    surface->screen_info = screen_info;

    surface->gc = NULL;
    surface->drawable = drawable;
    surface->screen = screen;
    surface->owns_pixmap = FALSE;
    surface->use_pixmap = 0;
    surface->width = width;
    surface->height = height;

    surface->buggy_repeat = screen_info->display->buggy_repeat;

    surface->dst_picture = None;
    surface->src_picture = None;

    surface->visual = visual;
    surface->xrender_format = xrender_format;
    surface->depth = depth;
    surface->filter = CAIRO_FILTER_NEAREST;
    surface->repeat = FALSE;
    surface->xtransform = identity;

    surface->have_clip_rects = FALSE;
    surface->clip_rects = surface->embedded_clip_rects;
    surface->num_clip_rects = 0;

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

    return (cairo_surface_t *) surface;
}

static Screen *
_cairo_xlib_screen_from_visual (Display *dpy, Visual *visual)
{
    int	    s;
    int	    d;
    int	    v;
    Screen *screen;
    Depth  *depth;

    for (s = 0; s < ScreenCount (dpy); s++) {
	screen = ScreenOfDisplay (dpy, s);
	if (visual == DefaultVisualOfScreen (screen))
	    return screen;
	for (d = 0; d < screen->ndepths; d++) {
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
 * cairo_xlib_surface_set_size must be called whenever the size of the
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
    Screen *screen = _cairo_xlib_screen_from_visual (dpy, visual);

    if (screen == NULL)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_VISUAL));

    return _cairo_xlib_surface_create_internal (dpy, drawable, screen,
						visual, NULL, width, height, 0);
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
				      Screen   *screen,
				      int	width,
				      int	height)
{
    return _cairo_xlib_surface_create_internal (dpy, bitmap, screen,
						NULL, NULL, width, height, 1);
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
 * cairo_xlib_surface_set_size must be called whenever the size of the
 * window changes.
 *
 * Return value: the newly created surface
 **/
cairo_surface_t *
cairo_xlib_surface_create_with_xrender_format (Display		    *dpy,
					       Drawable		    drawable,
					       Screen		    *screen,
					       XRenderPictFormat    *format,
					       int		    width,
					       int		    height)
{
    return _cairo_xlib_surface_create_internal (dpy, drawable, screen,
						NULL, format, width, height, 0);
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

    if (! _cairo_surface_is_xlib (abstract_surface)) {
	status = _cairo_surface_set_error (abstract_surface,
		                           CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
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

    if (! _cairo_surface_is_xlib (abstract_surface)) {
	status = _cairo_surface_set_error (abstract_surface,
		                           CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return;
    }

    /* XXX: and what about this case? */
    if (surface->owns_pixmap)
	return;

    if (surface->drawable != drawable) {
	if (surface->dst_picture != None) {
	    status = _cairo_xlib_display_queue_resource (
		                                  surface->screen_info->display,
						  XRenderFreePicture,
						  surface->dst_picture);
	    if (status) {
		status = _cairo_surface_set_error (&surface->base, status);
		return;
	    }

	    surface->dst_picture = None;
	}

	if (surface->src_picture != None) {
	    status = _cairo_xlib_display_queue_resource (
		                                  surface->screen_info->display,
						  XRenderFreePicture,
						  surface->src_picture);
	    if (status) {
		status = _cairo_surface_set_error (&surface->base, status);
		return;
	    }

	    surface->src_picture = None;
	}

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
    cairo_xlib_surface_t *surface = (cairo_xlib_surface_t *) abstract_surface;

    if (! _cairo_surface_is_xlib (abstract_surface)) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return NULL;
    }

    return surface->dpy;
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

    return surface->screen;
}

/**
 * cairo_xlib_surface_get_visual:
 * @surface: a #cairo_xlib_surface_t
 *
 * Get the X Visual used for underlying X Drawable.
 *
 * Return value: the visual.
 *
 * Since: 1.2
 **/
Visual *
cairo_xlib_surface_get_visual (cairo_surface_t *abstract_surface)
{
    cairo_xlib_surface_t *surface = (cairo_xlib_surface_t *) abstract_surface;

    if (! _cairo_surface_is_xlib (abstract_surface)) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return NULL;
    }

    return surface->visual;
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
	return -1;
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
	return -1;
    }

    return surface->height;
}

enum {
  GLYPHSET_INDEX_ARGB32,
  GLYPHSET_INDEX_A8,
  GLYPHSET_INDEX_A1,
  NUM_GLYPHSETS
};

typedef struct _cairo_xlib_font_glyphset_info {
    GlyphSet		glyphset;
    cairo_format_t	format;
    XRenderPictFormat	*xrender_format;
} cairo_xlib_font_glyphset_info_t;

typedef struct _cairo_xlib_surface_font_private {
    Display		*dpy;
    cairo_xlib_font_glyphset_info_t glyphset_info[NUM_GLYPHSETS];
} cairo_xlib_surface_font_private_t;

static void
_cairo_xlib_surface_remove_scaled_font (Display *dpy,
	                                void    *data)
{
    cairo_scaled_font_t *scaled_font = data;
    cairo_xlib_surface_font_private_t	*font_private;

    CAIRO_MUTEX_LOCK (scaled_font->mutex);
    font_private  = scaled_font->surface_private;
    scaled_font->surface_private = NULL;

    _cairo_scaled_font_reset_cache (scaled_font);
    CAIRO_MUTEX_UNLOCK (scaled_font->mutex);

    if (font_private != NULL) {
	int i;
	for (i = 0; i < NUM_GLYPHSETS; i++) {
	    cairo_xlib_font_glyphset_info_t *glyphset_info = &font_private->glyphset_info[i];
	    if (glyphset_info->glyphset) {
		XRenderFreeGlyphSet (font_private->dpy, glyphset_info->glyphset);
	    }
	}
	free (font_private);
    }
}

static cairo_status_t
_cairo_xlib_surface_font_init (Display		    *dpy,
			       cairo_scaled_font_t  *scaled_font)
{
    cairo_xlib_surface_font_private_t	*font_private;
    int i;

    font_private = malloc (sizeof (cairo_xlib_surface_font_private_t));
    if (!font_private)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    if (!_cairo_xlib_add_close_display_hook (dpy,
		_cairo_xlib_surface_remove_scaled_font,
		scaled_font, scaled_font)) {
	free (font_private);
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }


    font_private->dpy = dpy;
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
    }

    scaled_font->surface_private = font_private;
    scaled_font->surface_backend = &cairo_xlib_surface_backend;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_xlib_surface_scaled_font_fini (cairo_scaled_font_t *scaled_font)
{
    cairo_xlib_surface_font_private_t	*font_private = scaled_font->surface_private;

    if (font_private) {
	cairo_xlib_display_t *display;

	_cairo_xlib_remove_close_display_hooks (font_private->dpy, scaled_font);

	display = _cairo_xlib_display_get (font_private->dpy);
	if (display != NULL) {
	    int i;

	    for (i = 0; i < NUM_GLYPHSETS; i++) {
		cairo_xlib_font_glyphset_info_t *glyphset_info = &font_private->glyphset_info[i];
		if (glyphset_info->glyphset) {
		    cairo_status_t status;
		    status = _cairo_xlib_display_queue_resource (display,
								 XRenderFreeGlyphSet,
								 glyphset_info->glyphset);
		    (void) status; /* XXX cannot propagate failure */
		}
	    }

	    _cairo_xlib_display_destroy (display);
	}

	free (font_private);
    }
}

struct _cairo_xlib_render_free_glyphs {
    GlyphSet glyphset;
    unsigned long glyph_index;
};
static void _cairo_xlib_render_free_glyphs (Display *dpy, struct _cairo_xlib_render_free_glyphs *arg)
{
    XRenderFreeGlyphs (dpy,
	               arg->glyphset,
	               &arg->glyph_index, 1);
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
    cairo_xlib_surface_font_private_t	*font_private = scaled_font->surface_private;

    if (font_private != NULL && scaled_glyph->surface_private != NULL) {
	cairo_xlib_display_t *display = _cairo_xlib_display_get (font_private->dpy);
	if (display != NULL) {
	    struct _cairo_xlib_render_free_glyphs *arg = malloc (sizeof (*arg));
	    if (arg != NULL) {
		cairo_status_t status;
		arg->glyphset = _cairo_xlib_scaled_glyph_get_glyphset_info (scaled_glyph)->glyphset;
		arg->glyph_index = _cairo_scaled_glyph_index (scaled_glyph);
		status = _cairo_xlib_display_queue_work (display,
			(cairo_xlib_notify_func) _cairo_xlib_render_free_glyphs,
			arg,
			free);
		if (status) {
		    /* XXX cannot propagate failure */
		    free (arg);
		}
	    }

	    _cairo_xlib_display_destroy (display);

	}
    }
}

static cairo_bool_t
_native_byte_order_lsb (void)
{
    int	x = 1;

    return *((char *) &x) == 1;
}

static cairo_xlib_font_glyphset_info_t *
_cairo_xlib_scaled_font_get_glyphset_info_for_format (cairo_scaled_font_t *scaled_font,
						      cairo_format_t       format)
{
    cairo_xlib_surface_font_private_t *font_private = scaled_font->surface_private;
    cairo_xlib_font_glyphset_info_t *glyphset_info;
    int glyphset_index;

    switch (format) {
    default:
    case CAIRO_FORMAT_RGB24:
    case CAIRO_FORMAT_ARGB32: glyphset_index = GLYPHSET_INDEX_ARGB32; break;
    case CAIRO_FORMAT_A8:     glyphset_index = GLYPHSET_INDEX_A8;     break;
    case CAIRO_FORMAT_A1:     glyphset_index = GLYPHSET_INDEX_A1;     break;
    }

    glyphset_info = &font_private->glyphset_info[glyphset_index];

    if (glyphset_info->glyphset == None) {
	glyphset_info->xrender_format = _CAIRO_FORMAT_TO_XRENDER_FORMAT(font_private->dpy, glyphset_info->format);
	glyphset_info->glyphset = XRenderCreateGlyphSet (font_private->dpy, glyphset_info->xrender_format);
    }

    return glyphset_info;
}

static cairo_status_t
_cairo_xlib_surface_add_glyph (Display *dpy,
			       cairo_scaled_font_t   *scaled_font,
			       cairo_scaled_glyph_t **pscaled_glyph)
{
    XGlyphInfo glyph_info;
    unsigned long glyph_index;
    unsigned char *data;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_xlib_surface_font_private_t *font_private;
    cairo_scaled_glyph_t *scaled_glyph = *pscaled_glyph;
    cairo_image_surface_t *glyph_surface = scaled_glyph->surface;
    cairo_bool_t already_had_glyph_surface;
    cairo_xlib_font_glyphset_info_t *glyphset_info;

    if (!glyph_surface) {
	status = _cairo_scaled_glyph_lookup (scaled_font,
					     _cairo_scaled_glyph_index (scaled_glyph),
					     CAIRO_SCALED_GLYPH_INFO_METRICS |
					     CAIRO_SCALED_GLYPH_INFO_SURFACE,
					     pscaled_glyph);
	if (status != CAIRO_STATUS_SUCCESS)
	    return status;

	scaled_glyph = *pscaled_glyph;
	glyph_surface = scaled_glyph->surface;
	already_had_glyph_surface = FALSE;
    } else {
	already_had_glyph_surface = TRUE;
    }

    /* XXX XRenderAddGlyph does not handle a glyph surface larger than the
     * maximum XRequest size.
     */
    {
	/* pessimistic length estimation in case we need to change formats */
	int len = 4 * glyph_surface->width * glyph_surface->height;
	int max_request_size = XMaxRequestSize (dpy)  -
	                       sz_xRenderAddGlyphsReq -
	                       sz_xGlyphInfo          -
			       4;
	if (len >= max_request_size)
	    return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    if (scaled_font->surface_private == NULL) {
	status = _cairo_xlib_surface_font_init (dpy, scaled_font);
	if (status)
	    return status;
    }
    font_private = scaled_font->surface_private;

    glyphset_info = _cairo_xlib_scaled_font_get_glyphset_info_for_format (scaled_font,
									  glyph_surface->format);

    /* If the glyph surface has zero height or width, we create
     * a clear 1x1 surface, to avoid various X server bugs.
     */
    if (glyph_surface->width == 0 || glyph_surface->height == 0) {
	cairo_t *cr;
	cairo_surface_t *tmp_surface;

	tmp_surface = cairo_image_surface_create (glyphset_info->format, 1, 1);
	if (tmp_surface->status)
	    goto BAIL;

	cr = cairo_create (tmp_surface);
	cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
	cairo_paint (cr);
	status = cairo_status (cr);
	cairo_destroy (cr);

	tmp_surface->device_transform = glyph_surface->base.device_transform;
	tmp_surface->device_transform_inverse = glyph_surface->base.device_transform_inverse;

	glyph_surface = (cairo_image_surface_t *) tmp_surface;

	if (status)
	    goto BAIL;
    }

    /* If the glyph format does not match the font format, then we
     * create a temporary surface for the glyph image with the font's
     * format.
     */
    if (glyph_surface->format != glyphset_info->format) {
	cairo_t *cr;
	cairo_surface_t *tmp_surface;

	tmp_surface = cairo_image_surface_create (glyphset_info->format,
						  glyph_surface->width,
						  glyph_surface->height);
	if (tmp_surface->status)
	    goto BAIL;

	tmp_surface->device_transform = glyph_surface->base.device_transform;
	tmp_surface->device_transform_inverse = glyph_surface->base.device_transform_inverse;

	cr = cairo_create (tmp_surface);
	cairo_set_source_surface (cr, &glyph_surface->base, 0, 0);
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint (cr);
	status = cairo_status (cr);
	cairo_destroy (cr);

	glyph_surface = (cairo_image_surface_t *) tmp_surface;

	if (status)
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
    switch (scaled_glyph->surface->format) {
    case CAIRO_FORMAT_A1:
	/* local bitmaps are always stored with bit == byte */
	if (_native_byte_order_lsb() != (BitmapBitOrder (dpy) == LSBFirst)) {
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
	    while (c--)
	    {
		char	b = *d++;
		b = ((b << 1) & 0xaa) | ((b >> 1) & 0x55);
		b = ((b << 2) & 0xcc) | ((b >> 2) & 0x33);
		b = ((b << 4) & 0xf0) | ((b >> 4) & 0x0f);
		*n++ = b;
	    }
	    data = new;
	}
	break;
    case CAIRO_FORMAT_A8:
	break;
    case CAIRO_FORMAT_ARGB32:
	if (_native_byte_order_lsb() != (ImageByteOrder (dpy) == LSBFirst)) {
	    unsigned int    c = glyph_surface->stride * glyph_surface->height;
	    unsigned char   *d;
	    unsigned char   *new, *n;

	    new = malloc (c);
	    if (new == NULL) {
		status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
		goto BAIL;
	    }
	    n = new;
	    d = data;
	    while (c >= 4)
	    {
		n[3] = d[0];
		n[2] = d[1];
		n[1] = d[2];
		n[0] = d[3];
		d += 4;
		n += 4;
		c -= 4;
	    }
	    data = new;
	}
	break;
    case CAIRO_FORMAT_RGB24:
    default:
	ASSERT_NOT_REACHED;
	break;
    }
    /* XXX assume X server wants pixman padding. Xft assumes this as well */

    glyph_index = _cairo_scaled_glyph_index (scaled_glyph);

    XRenderAddGlyphs (dpy, glyphset_info->glyphset,
		      &glyph_index, &glyph_info, 1,
		      (char *) data,
		      glyph_surface->stride * glyph_surface->height);

    _cairo_xlib_scaled_glyph_set_glyphset_info (scaled_glyph, glyphset_info);

    if (data != glyph_surface->data)
	free (data);

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
typedef struct {
  unsigned long index;
  union {
    struct {
      double x;
      double y;
    } d;
    struct {
      int x;
      int y;
    } i;
  } p;
} cairo_xlib_glyph_t;

/* compile-time assert that #cairo_xlib_glyph_t is the same size as #cairo_glyph_t */
typedef int cairo_xlib_glyph_t_size_assertion [sizeof (cairo_xlib_glyph_t) == sizeof (cairo_glyph_t) ? 1 : -1];

#define GLYPH_INDEX_SKIP ((unsigned long) -1)

static cairo_status_t
_cairo_xlib_surface_emit_glyphs_chunk (cairo_xlib_surface_t *dst,
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
      if (elts == NULL)
	  return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    /* Fill them in */
    nelt = 0;
    n = 0;
    j = 0;
    for (i = 0; i < num_glyphs; i++) {

      /* Skip glyphs marked so */
      if (glyphs[i].index == GLYPH_INDEX_SKIP)
	continue;

      /* Start a new element for first output glyph, and for glyphs with
       * unexpected position */
      if (!j || glyphs[i].p.i.x || glyphs[i].p.i.y) {
	  if (j) {
	    elts[nelt].nchars = n;
	    nelt++;
	    n = 0;
	  }
	  elts[nelt].chars = char8 + size * j;
	  elts[nelt].glyphset = glyphset_info->glyphset;
	  elts[nelt].xOff = glyphs[i].p.i.x;
	  elts[nelt].yOff = glyphs[i].p.i.y;
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
	n = 0;
    }

    composite_text_func (dst->dpy,
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

static cairo_status_t
_cairo_xlib_surface_emit_glyphs (cairo_xlib_surface_t *dst,
				 cairo_xlib_glyph_t *glyphs,
				 int num_glyphs,
				 cairo_scaled_font_t *scaled_font,
				 cairo_operator_t op,
				 cairo_xlib_surface_t *src,
				 cairo_surface_attributes_t *attributes)
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

    int max_request_size = XMaxRequestSize (dst->dpy)
			 - MAX (sz_xRenderCompositeGlyphs8Req,
				MAX(sz_xRenderCompositeGlyphs16Req,
				    sz_xRenderCompositeGlyphs32Req));
    int request_size = 0;

    _cairo_xlib_surface_ensure_dst_picture (dst);
    _cairo_xlib_display_notify (dst->screen_info->display);

    for (i = 0; i < num_glyphs; i++) {
	int this_x, this_y;
	int old_width;

	status = _cairo_scaled_glyph_lookup (scaled_font,
					     glyphs[i].index,
					     CAIRO_SCALED_GLYPH_INFO_METRICS,
					     &scaled_glyph);
	if (status != CAIRO_STATUS_SUCCESS)
	    return status;

	this_x = _cairo_lround (glyphs[i].p.d.x);
	this_y = _cairo_lround (glyphs[i].p.d.y);

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
	 * Anyway, we will allow positions in the range -1024..15359.  That
	 * will buy us a few more years before this stops working.
	 */
	if (((this_x+1024)|(this_y+1024))&~0x3fffu) {
	    glyphs[i].index = GLYPH_INDEX_SKIP;
	    continue;
	}

	/* Send unsent glyphs to the server */
	if (_cairo_xlib_scaled_glyph_get_glyphset_info (scaled_glyph) == NULL) {
	    status = _cairo_xlib_surface_add_glyph (dst->dpy,
		                                    scaled_font,
						    &scaled_glyph);
	    if (status)
		return status;
	}

	this_glyphset_info = _cairo_xlib_scaled_glyph_get_glyphset_info (scaled_glyph);
	if (!glyphset_info)
	    glyphset_info = this_glyphset_info;

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
	if (request_size + width > max_request_size - sz_xGlyphElt ||
	    (this_glyphset_info != glyphset_info)) {
	    status = _cairo_xlib_surface_emit_glyphs_chunk (dst, glyphs, i,
							    scaled_font, op, src, attributes,
							    num_elts, old_width, glyphset_info);
	    if (status != CAIRO_STATUS_SUCCESS)
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
	glyphs[i].p.i.x = this_x - x;
	glyphs[i].p.i.y = this_y - y;

	/* Start a new element for the first glyph, or for any glyph that
	 * has unexpected position */
	if (!num_out_glyphs || glyphs[i].p.i.x || glyphs[i].p.i.y) {
	    num_elts++;
	    request_size += sz_xGlyphElt;
	}

	/* adjust current-position */
	x = this_x + scaled_glyph->x_advance;
	y = this_y + scaled_glyph->y_advance;

	num_out_glyphs++;
	request_size += width;
    }

    if (num_elts)
	status = _cairo_xlib_surface_emit_glyphs_chunk (dst, glyphs, num_glyphs,
							scaled_font, op, src, attributes,
							num_elts, width, glyphset_info);

    return status;
}

#undef GLYPH_INDEX_SKIP

static cairo_int_status_t
_cairo_xlib_surface_show_glyphs (void                *abstract_dst,
				 cairo_operator_t     op,
				 cairo_pattern_t     *src_pattern,
				 cairo_glyph_t       *glyphs,
				 int		      num_glyphs,
				 cairo_scaled_font_t *scaled_font)
{
    cairo_int_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_xlib_surface_t *dst = (cairo_xlib_surface_t*) abstract_dst;

    composite_operation_t operation;
    cairo_surface_attributes_t attributes;
    cairo_xlib_surface_t *src = NULL;

    cairo_xlib_surface_font_private_t *font_private;

    cairo_pattern_union_t solid_pattern;

    if (!CAIRO_SURFACE_RENDER_HAS_COMPOSITE_TEXT (dst) || !dst->xrender_format)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* Just let unbounded operators go through the fallback code
     * instead of trying to do the fixups here */
    if (!_cairo_operator_bounded_by_mask (op))
        return CAIRO_INT_STATUS_UNSUPPORTED;

    /* Render <= 0.10 seems to have a bug with PictOpSrc and glyphs --
     * the solid source seems to be multiplied by the glyph mask, and
     * then the entire thing is copied to the destination surface,
     * including the fully transparent "background" of the rectangular
     * glyph surface. */
    if (op == CAIRO_OPERATOR_SOURCE &&
        !CAIRO_SURFACE_RENDER_AT_LEAST(dst, 0, 11))
        return CAIRO_INT_STATUS_UNSUPPORTED;

    /* We can only use our code if we either have no clip or
     * have a real native clip region set.  If we're using
     * fallback clip masking, we have to go through the full
     * fallback path.
     */
    if (dst->base.clip &&
        (dst->base.clip->mode != CAIRO_CLIP_MODE_REGION ||
         dst->base.clip->surface != NULL))
        return CAIRO_INT_STATUS_UNSUPPORTED;

    operation = _categorize_composite_operation (dst, op, src_pattern, TRUE);
    if (operation == DO_UNSUPPORTED)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    font_private = scaled_font->surface_private;
    if ((scaled_font->surface_backend != NULL &&
	 scaled_font->surface_backend != &cairo_xlib_surface_backend) ||
	(font_private != NULL && font_private->dpy != dst->dpy))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* After passing all those tests, we're now committed to rendering
     * these glyphs or to fail trying. We first upload any glyphs to
     * the X server that it doesn't have already, then we draw
     * them. We tie into the scaled_font's glyph cache and remove
     * glyphs from the X server when they are ejected from the
     * scaled_font cache. Because of this we first freeze the
     * scaled_font's cache so that we don't cause any of our glyphs to
     * be ejected and removed from the X server before we have a
     * chance to render them. */
    _cairo_scaled_font_freeze_cache (scaled_font);

    /* PictOpClear doesn't seem to work with CompositeText; it seems to ignore
     * the mask (the glyphs).  This code below was executed as a side effect
     * of going through the _clip_and_composite fallback code for old_show_glyphs,
     * so PictOpClear was never used with CompositeText before.
     */
    if (op == CAIRO_OPERATOR_CLEAR) {
	_cairo_pattern_init_solid (&solid_pattern.solid, CAIRO_COLOR_WHITE,
				   CAIRO_CONTENT_COLOR);
	src_pattern = &solid_pattern.base;
	op = CAIRO_OPERATOR_DEST_OUT;
    }

    if (src_pattern->type == CAIRO_PATTERN_TYPE_SOLID) {
        status = _cairo_pattern_acquire_surface (src_pattern, &dst->base,
                                                 0, 0, 1, 1,
                                                 (cairo_surface_t **) &src,
                                                 &attributes);
	if (status)
	    goto BAIL0;
    } else {
        cairo_rectangle_int_t glyph_extents;

        status = _cairo_scaled_font_glyph_device_extents (scaled_font,
                                                          glyphs,
                                                          num_glyphs,
                                                          &glyph_extents);
        if (status)
	    goto BAIL0;

        status = _cairo_pattern_acquire_surface (src_pattern, &dst->base,
                                                 glyph_extents.x, glyph_extents.y,
                                                 glyph_extents.width, glyph_extents.height,
                                                 (cairo_surface_t **) &src,
                                                 &attributes);
        if (status)
	    goto BAIL0;
    }

    operation = _recategorize_composite_operation (dst, op, src, &attributes, TRUE);
    if (operation == DO_UNSUPPORTED) {
	status = CAIRO_INT_STATUS_UNSUPPORTED;
	goto BAIL1;
    }

    status = _cairo_xlib_surface_set_attributes (src, &attributes);
    if (status)
        goto BAIL1;

    status = _cairo_xlib_surface_emit_glyphs (dst,
	                                      (cairo_xlib_glyph_t *) glyphs,
					      num_glyphs,
					      scaled_font,
					      op,
					      src,
					      &attributes);

  BAIL1:
    if (src)
        _cairo_pattern_release_surface (src_pattern, &src->base, &attributes);
    if (src_pattern == &solid_pattern.base)
	_cairo_pattern_fini (&solid_pattern.base);
  BAIL0:
    _cairo_scaled_font_thaw_cache (scaled_font);
    _cairo_xlib_display_notify (dst->screen_info->display);

    return status;
}
