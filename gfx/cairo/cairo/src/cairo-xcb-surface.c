/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
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
 */

#include "cairoint.h"
#include "cairo-xcb.h"
#include "cairo-xcb-xrender.h"
#include "cairo-clip-private.h"
#include <xcb/xcb_renderutil.h>

#define AllPlanes               ((unsigned long)~0L)

slim_hidden_proto (cairo_xcb_surface_create_with_xrender_format);

/*
 * Instead of taking two round trips for each blending request,
 * assume that if a particular drawable fails GetImage that it will
 * fail for a "while"; use temporary pixmaps to avoid the errors
 */

#define CAIRO_ASSUME_PIXMAP	20

typedef struct cairo_xcb_surface {
    cairo_surface_t base;

    xcb_connection_t *dpy;
    xcb_screen_t *screen;

    xcb_gcontext_t gc;
    xcb_drawable_t drawable;
    cairo_bool_t owns_pixmap;
    xcb_visualtype_t *visual;

    int use_pixmap;

    int render_major;
    int render_minor;

    int width;
    int height;
    int depth;

    cairo_bool_t have_clip_rects;
    xcb_rectangle_t *clip_rects;
    int num_clip_rects;

    xcb_render_picture_t src_picture, dst_picture;
    xcb_render_pictforminfo_t xrender_format;
} cairo_xcb_surface_t;

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

static void
_cairo_xcb_surface_ensure_gc (cairo_xcb_surface_t *surface);

static int
_CAIRO_FORMAT_DEPTH (cairo_format_t format)
{
    switch (format) {
    case CAIRO_FORMAT_A1:
	return 1;
    case CAIRO_FORMAT_A8:
	return 8;
    case CAIRO_FORMAT_RGB24:
	return 24;
    case CAIRO_FORMAT_ARGB32:
    default:
	return 32;
    }
}

static xcb_render_pictforminfo_t *
_CAIRO_FORMAT_TO_XRENDER_FORMAT(xcb_connection_t *dpy, cairo_format_t format)
{
    xcb_pict_standard_t	std_format;
    switch (format) {
    case CAIRO_FORMAT_A1:
	std_format = XCB_PICT_STANDARD_A_1; break;
    case CAIRO_FORMAT_A8:
	std_format = XCB_PICT_STANDARD_A_8; break;
    case CAIRO_FORMAT_RGB24:
	std_format = XCB_PICT_STANDARD_RGB_24; break;
    case CAIRO_FORMAT_ARGB32:
    default:
	std_format = XCB_PICT_STANDARD_ARGB_32; break;
    }
    return xcb_render_util_find_standard_format (xcb_render_util_query_formats (dpy), std_format);
}

static cairo_content_t
_xcb_render_format_to_content (xcb_render_pictforminfo_t *xrender_format)
{
    cairo_bool_t xrender_format_has_alpha;
    cairo_bool_t xrender_format_has_color;

    /* This only happens when using a non-Render server. Let's punt
     * and say there's no alpha here. */
    if (xrender_format == NULL)
	return CAIRO_CONTENT_COLOR;

    xrender_format_has_alpha = (xrender_format->direct.alpha_mask != 0);
    xrender_format_has_color = (xrender_format->direct.red_mask   != 0 ||
				xrender_format->direct.green_mask != 0 ||
				xrender_format->direct.blue_mask  != 0);

    if (xrender_format_has_alpha)
	if (xrender_format_has_color)
	    return CAIRO_CONTENT_COLOR_ALPHA;
	else
	    return CAIRO_CONTENT_ALPHA;
    else
	return CAIRO_CONTENT_COLOR;
}

static cairo_surface_t *
_cairo_xcb_surface_create_similar (void		       *abstract_src,
				   cairo_content_t	content,
				   int			width,
				   int			height)
{
    cairo_xcb_surface_t *src = abstract_src;
    xcb_connection_t *dpy = src->dpy;
    xcb_pixmap_t pixmap;
    cairo_xcb_surface_t *surface;
    cairo_format_t format = _cairo_format_from_content (content);
    xcb_render_pictforminfo_t *xrender_format;

    /* As a good first approximation, if the display doesn't have COMPOSITE,
     * we're better off using image surfaces for all temporary operations
     */
    if (!CAIRO_SURFACE_RENDER_HAS_COMPOSITE (src)) {
	return cairo_image_surface_create (format, width, height);
    }

    pixmap = xcb_generate_id (dpy);
    xcb_create_pixmap (dpy, _CAIRO_FORMAT_DEPTH (format),
		     pixmap, src->drawable,
		     width <= 0 ? 1 : width,
		     height <= 0 ? 1 : height);

    xrender_format = _CAIRO_FORMAT_TO_XRENDER_FORMAT (dpy, format);
    /* XXX: what to do if xrender_format is null? */
    surface = (cairo_xcb_surface_t *)
	cairo_xcb_surface_create_with_xrender_format (dpy, pixmap, src->screen,
						      xrender_format,
						      width, height);
    if (surface->base.status)
	return surface;

    surface->owns_pixmap = TRUE;

    return &surface->base;
}

static cairo_status_t
_cairo_xcb_surface_finish (void *abstract_surface)
{
    cairo_xcb_surface_t *surface = abstract_surface;
    if (surface->dst_picture != XCB_NONE)
	xcb_render_free_picture (surface->dpy, surface->dst_picture);

    if (surface->src_picture != XCB_NONE)
	xcb_render_free_picture (surface->dpy, surface->src_picture);

    if (surface->owns_pixmap)
	xcb_free_pixmap (surface->dpy, surface->drawable);

    if (surface->gc != XCB_NONE)
	xcb_free_gc (surface->dpy, surface->gc);

    free (surface->clip_rects);

    surface->dpy = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static int
_bits_per_pixel(xcb_connection_t *c, int depth)
{
    xcb_format_t *fmt = xcb_setup_pixmap_formats(xcb_get_setup(c));
    xcb_format_t *fmtend = fmt + xcb_setup_pixmap_formats_length(xcb_get_setup(c));

    for(; fmt != fmtend; ++fmt)
	if(fmt->depth == depth)
	    return fmt->bits_per_pixel;

    if(depth <= 4)
	return 4;
    if(depth <= 8)
	return 8;
    if(depth <= 16)
	return 16;
    return 32;
}

static int
_bytes_per_line(xcb_connection_t *c, int width, int bpp)
{
    int bitmap_pad = xcb_get_setup(c)->bitmap_format_scanline_pad;
    return ((bpp * width + bitmap_pad - 1) & -bitmap_pad) >> 3;
}

static cairo_bool_t
_CAIRO_MASK_FORMAT (cairo_format_masks_t *masks, cairo_format_t *format)
{
    switch (masks->bpp) {
    case 32:
	if (masks->alpha_mask == 0xff000000 &&
	    masks->red_mask == 0x00ff0000 &&
	    masks->green_mask == 0x0000ff00 &&
	    masks->blue_mask == 0x000000ff)
	{
	    *format = CAIRO_FORMAT_ARGB32;
	    return TRUE;
	}
	if (masks->alpha_mask == 0x00000000 &&
	    masks->red_mask == 0x00ff0000 &&
	    masks->green_mask == 0x0000ff00 &&
	    masks->blue_mask == 0x000000ff)
	{
	    *format = CAIRO_FORMAT_RGB24;
	    return TRUE;
	}
	break;
    case 8:
	if (masks->alpha_mask == 0xff)
	{
	    *format = CAIRO_FORMAT_A8;
	    return TRUE;
	}
	break;
    case 1:
	if (masks->alpha_mask == 0x1)
	{
	    *format = CAIRO_FORMAT_A1;
	    return TRUE;
	}
	break;
    }
    return FALSE;
}

static cairo_status_t
_get_image_surface (cairo_xcb_surface_t     *surface,
		    cairo_rectangle_int_t   *interest_rect,
		    cairo_image_surface_t  **image_out,
		    cairo_rectangle_int_t   *image_rect)
{
    cairo_image_surface_t *image;
    xcb_get_image_reply_t *imagerep;
    int bpp, bytes_per_line;
    short x1, y1, x2, y2;
    unsigned char *data;
    cairo_format_masks_t masks;
    cairo_format_t format;

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
	xcb_generic_error_t *error;
	imagerep = xcb_get_image_reply(surface->dpy,
				    xcb_get_image(surface->dpy, XCB_IMAGE_FORMAT_Z_PIXMAP,
						surface->drawable,
						x1, y1,
						x2 - x1, y2 - y1,
						AllPlanes), &error);

	/* If we get an error, the surface must have been a window,
	 * so retry with the safe code path.
	 */
	if (error)
	    surface->use_pixmap = CAIRO_ASSUME_PIXMAP;
    }
    else
    {
	surface->use_pixmap--;
	imagerep = NULL;
    }

    if (!imagerep)
    {
	/* xcb_get_image_t from a window is dangerous because it can
	 * produce errors if the window is unmapped or partially
	 * outside the screen. We could check for errors and
	 * retry, but to keep things simple, we just create a
	 * temporary pixmap
	 */
	xcb_pixmap_t pixmap;
	pixmap = xcb_generate_id (surface->dpy);
	xcb_create_pixmap (surface->dpy,
			 surface->depth,
			 pixmap,
			 surface->drawable,
			 x2 - x1, y2 - y1);
	_cairo_xcb_surface_ensure_gc (surface);

	xcb_copy_area (surface->dpy, surface->drawable, pixmap, surface->gc,
		     x1, y1, 0, 0, x2 - x1, y2 - y1);

	imagerep = xcb_get_image_reply(surface->dpy,
				    xcb_get_image(surface->dpy, XCB_IMAGE_FORMAT_Z_PIXMAP,
						pixmap,
						x1, y1,
						x2 - x1, y2 - y1,
						AllPlanes), 0);
	xcb_free_pixmap (surface->dpy, pixmap);

    }
    if (!imagerep)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    bpp = _bits_per_pixel(surface->dpy, imagerep->depth);
    bytes_per_line = _bytes_per_line(surface->dpy, surface->width, bpp);

    data = _cairo_malloc_ab (surface->height, bytes_per_line);
    if (data == NULL) {
	free (imagerep);
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    memcpy (data, xcb_get_image_data (imagerep), bytes_per_line * surface->height);
    free (imagerep);

    /*
     * Compute the pixel format masks from either an xcb_visualtype_t or
     * a xcb_render_pctforminfo_t, failing we assume the drawable is an
     * alpha-only pixmap as it could only have been created that way
     * through the cairo_xlib_surface_create_for_bitmap function.
     */
    if (surface->visual) {
	masks.bpp = bpp;
	masks.alpha_mask = 0;
	masks.red_mask = surface->visual->red_mask;
	masks.green_mask = surface->visual->green_mask;
	masks.blue_mask = surface->visual->blue_mask;
    } else if (surface->xrender_format.id != XCB_NONE) {
	masks.bpp = bpp;
	masks.red_mask = (unsigned long)surface->xrender_format.direct.red_mask << surface->xrender_format.direct.red_shift;
	masks.green_mask = (unsigned long)surface->xrender_format.direct.green_mask << surface->xrender_format.direct.green_shift;
	masks.blue_mask = (unsigned long)surface->xrender_format.direct.blue_mask << surface->xrender_format.direct.blue_shift;
	masks.alpha_mask = (unsigned long)surface->xrender_format.direct.alpha_mask << surface->xrender_format.direct.alpha_shift;
    } else {
	masks.bpp = bpp;
	masks.red_mask = 0;
	masks.green_mask = 0;
	masks.blue_mask = 0;
	if (surface->depth < 32)
	    masks.alpha_mask = (1 << surface->depth) - 1;
	else
	    masks.alpha_mask = 0xffffffff;
    }

    /*
     * Prefer to use a standard pixman format instead of the
     * general masks case.
     */
    if (_CAIRO_MASK_FORMAT (&masks, &format)) {
	image = (cairo_image_surface_t *)
	    cairo_image_surface_create_for_data (data,
						 format,
						 x2 - x1,
						 y2 - y1,
						 bytes_per_line);
	if (image->base.status)
	    goto FAIL;
    } else {
	/*
	 * XXX This can't work.  We must convert the data to one of the
	 * supported pixman formats.  Pixman needs another function
	 * which takes data in an arbitrary format and converts it
	 * to something supported by that library.
	 */
	image = (cairo_image_surface_t *)
	    _cairo_image_surface_create_with_masks (data,
						    &masks,
						    x2 - x1,
						    y2 - y1,
						    bytes_per_line);
	if (image->base.status)
	    goto FAIL;
    }

    /* Let the surface take ownership of the data */
    _cairo_image_surface_assume_ownership_of_data (image);

    *image_out = image;
    return CAIRO_STATUS_SUCCESS;

 FAIL:
    free (data);
    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
}

static void
_cairo_xcb_surface_ensure_src_picture (cairo_xcb_surface_t    *surface)
{
    if (!surface->src_picture) {
	surface->src_picture = xcb_generate_id(surface->dpy);
	xcb_render_create_picture (surface->dpy,
	                           surface->src_picture,
	                           surface->drawable,
	                           surface->xrender_format.id,
				   0, NULL);
    }
}

static void
_cairo_xcb_surface_set_picture_clip_rects (cairo_xcb_surface_t *surface)
{
    if (surface->have_clip_rects)
	xcb_render_set_picture_clip_rectangles (surface->dpy, surface->dst_picture,
					   0, 0,
					   surface->num_clip_rects,
					   surface->clip_rects);
}

static void
_cairo_xcb_surface_set_gc_clip_rects (cairo_xcb_surface_t *surface)
{
    if (surface->have_clip_rects)
	xcb_set_clip_rectangles(surface->dpy, XCB_CLIP_ORDERING_YX_SORTED, surface->gc,
			     0, 0,
			     surface->num_clip_rects,
			     surface->clip_rects );
}

static void
_cairo_xcb_surface_ensure_dst_picture (cairo_xcb_surface_t    *surface)
{
    if (!surface->dst_picture) {
	surface->dst_picture = xcb_generate_id(surface->dpy);
	xcb_render_create_picture (surface->dpy,
	                           surface->dst_picture,
	                           surface->drawable,
	                           surface->xrender_format.id,
				   0, NULL);
	_cairo_xcb_surface_set_picture_clip_rects (surface);
    }

}

static void
_cairo_xcb_surface_ensure_gc (cairo_xcb_surface_t *surface)
{
    if (surface->gc)
	return;

    surface->gc = xcb_generate_id(surface->dpy);
    xcb_create_gc (surface->dpy, surface->gc, surface->drawable, 0, 0);
    _cairo_xcb_surface_set_gc_clip_rects(surface);
}

static cairo_status_t
_draw_image_surface (cairo_xcb_surface_t    *surface,
		     cairo_image_surface_t  *image,
		     int                    src_x,
		     int                    src_y,
		     int                    width,
		     int                    height,
		     int                    dst_x,
		     int                    dst_y)
{
    int bpp, bpl;
    uint32_t data_len;
    uint8_t *data, left_pad=0;

    /* equivalent of XPutImage(..., src_x,src_y, dst_x,dst_y, width,height); */
    /* XXX: assumes image and surface formats and depths are the same */
    /* XXX: assumes depth is a multiple of 8 (not bitmap) */

    /* fit src_{x,y,width,height} within image->{0,0,width,height} */
    if (src_x < 0) {
	width += src_x;
	src_x = 0;
    }
    if (src_y < 0) {
	height += src_y;
	src_y = 0;
    }
    if (width + src_x > image->width)
	width = image->width - src_x;
    if (height + src_y > image->height)
	height = image->height - src_y;
    if (width <= 0 || height <= 0)
	return CAIRO_STATUS_SUCCESS;

    bpp = _bits_per_pixel(surface->dpy, image->depth);
    /* XXX: could use bpl = image->stride? */
    bpl = _bytes_per_line(surface->dpy, image->width, bpp);

    if (src_x == 0 && width == image->width) {
	/* can work in-place */
	data_len = height * bpl;
	data = image->data + src_y * bpl;
    } else {
	/* must copy {src_x,src_y,width,height} into new data */
	int line = 0;
	uint8_t *data_line, *image_line;
	int data_bpl = _bytes_per_line(surface->dpy, width, bpp);
	data_len = height * data_bpl;
	data_line = data = malloc(data_len);
	if (data == NULL)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	image_line = image->data + src_y * bpl + (src_x * bpp / 8);
	while (line++ < height) {
	    memcpy(data_line, image_line, data_bpl);
	    data_line += data_bpl;
	    image_line += bpl;
	}
    }
    _cairo_xcb_surface_ensure_gc (surface);
    xcb_put_image (surface->dpy, XCB_IMAGE_FORMAT_Z_PIXMAP,
	surface->drawable, surface->gc,
	width, height,
	dst_x, dst_y,
	left_pad, image->depth,
	data_len, data);

    if (data < image->data || data >= image->data + image->height * bpl)
	free(data);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xcb_surface_acquire_source_image (void                    *abstract_surface,
					 cairo_image_surface_t  **image_out,
					 void                   **image_extra)
{
    cairo_xcb_surface_t *surface = abstract_surface;
    cairo_image_surface_t *image;
    cairo_status_t status;

    status = _get_image_surface (surface, NULL, &image, NULL);
    if (status)
	return status;

    *image_out = image;
    *image_extra = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_xcb_surface_release_source_image (void                   *abstract_surface,
					 cairo_image_surface_t  *image,
					 void                   *image_extra)
{
    cairo_surface_destroy (&image->base);
}

static cairo_status_t
_cairo_xcb_surface_acquire_dest_image (void                    *abstract_surface,
				       cairo_rectangle_int_t   *interest_rect,
				       cairo_image_surface_t  **image_out,
				       cairo_rectangle_int_t   *image_rect_out,
				       void                   **image_extra)
{
    cairo_xcb_surface_t *surface = abstract_surface;
    cairo_image_surface_t *image;
    cairo_status_t status;

    status = _get_image_surface (surface, interest_rect, &image, image_rect_out);
    if (status)
	return status;

    *image_out = image;
    *image_extra = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_xcb_surface_release_dest_image (void                   *abstract_surface,
				       cairo_rectangle_int_t  *interest_rect,
				       cairo_image_surface_t  *image,
				       cairo_rectangle_int_t  *image_rect,
				       void                   *image_extra)
{
    cairo_xcb_surface_t *surface = abstract_surface;

    /* ignore errors */
    _draw_image_surface (surface, image, 0, 0, image->width, image->height,
			 image_rect->x, image_rect->y);

    cairo_surface_destroy (&image->base);
}

/*
 * Return whether two xcb surfaces share the same
 * screen.  Both core and Render drawing require this
 * when using multiple drawables in an operation.
 */
static cairo_bool_t
_cairo_xcb_surface_same_screen (cairo_xcb_surface_t *dst,
				cairo_xcb_surface_t *src)
{
    return dst->dpy == src->dpy && dst->screen == src->screen;
}

static cairo_status_t
_cairo_xcb_surface_clone_similar (void			*abstract_surface,
				  cairo_surface_t	*src,
				  int                    src_x,
				  int                    src_y,
				  int                    width,
				  int                    height,
				  cairo_surface_t     **clone_out)
{
    cairo_xcb_surface_t *surface = abstract_surface;
    cairo_xcb_surface_t *clone;

    if (src->backend == surface->base.backend ) {
	cairo_xcb_surface_t *xcb_src = (cairo_xcb_surface_t *)src;

	if (_cairo_xcb_surface_same_screen(surface, xcb_src)) {
	    *clone_out = cairo_surface_reference (src);

	    return CAIRO_STATUS_SUCCESS;
	}
    } else if (_cairo_surface_is_image (src)) {
	cairo_image_surface_t *image_src = (cairo_image_surface_t *)src;
	cairo_content_t content = _cairo_content_from_format (image_src->format);

	if (surface->base.status)
	    return surface->base.status;

	clone = (cairo_xcb_surface_t *)
	    _cairo_xcb_surface_create_similar (surface, content,
					       image_src->width, image_src->height);
	if (clone->base.status)
	    return clone->base.status;

	_draw_image_surface (clone, image_src, src_x, src_y,
			     width, height, src_x, src_y);

	*clone_out = &clone->base;

	return CAIRO_STATUS_SUCCESS;
    }

    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_status_t
_cairo_xcb_surface_set_matrix (cairo_xcb_surface_t *surface,
			       cairo_matrix_t	   *matrix)
{
    xcb_render_transform_t xtransform;

    if (!surface->src_picture)
	return CAIRO_STATUS_SUCCESS;

    xtransform.matrix11 = _cairo_fixed_16_16_from_double (matrix->xx);
    xtransform.matrix12 = _cairo_fixed_16_16_from_double (matrix->xy);
    xtransform.matrix13 = _cairo_fixed_16_16_from_double (matrix->x0);

    xtransform.matrix21 = _cairo_fixed_16_16_from_double (matrix->yx);
    xtransform.matrix22 = _cairo_fixed_16_16_from_double (matrix->yy);
    xtransform.matrix23 = _cairo_fixed_16_16_from_double (matrix->y0);

    xtransform.matrix31 = 0;
    xtransform.matrix32 = 0;
    xtransform.matrix33 = 1 << 16;

    if (!CAIRO_SURFACE_RENDER_HAS_PICTURE_TRANSFORM (surface))
    {
	static const xcb_render_transform_t identity = {
	    1 << 16, 0x00000, 0x00000,
	    0x00000, 1 << 16, 0x00000,
	    0x00000, 0x00000, 1 << 16
	};

	if (memcmp (&xtransform, &identity, sizeof (xcb_render_transform_t)) == 0)
	    return CAIRO_STATUS_SUCCESS;

	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    xcb_render_set_picture_transform (surface->dpy, surface->src_picture, xtransform);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xcb_surface_set_filter (cairo_xcb_surface_t *surface,
			       cairo_filter_t	   filter)
{
    const char *render_filter;

    if (!surface->src_picture)
	return CAIRO_STATUS_SUCCESS;

    if (!CAIRO_SURFACE_RENDER_HAS_FILTERS (surface))
    {
	if (filter == CAIRO_FILTER_FAST || filter == CAIRO_FILTER_NEAREST)
	    return CAIRO_STATUS_SUCCESS;

	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    switch (filter) {
    case CAIRO_FILTER_FAST:
	render_filter = "fast";
	break;
    case CAIRO_FILTER_GOOD:
	render_filter = "good";
	break;
    case CAIRO_FILTER_BEST:
	render_filter = "best";
	break;
    case CAIRO_FILTER_NEAREST:
	render_filter = "nearest";
	break;
    case CAIRO_FILTER_BILINEAR:
	render_filter = "bilinear";
	break;
    case CAIRO_FILTER_GAUSSIAN:
    default:
	render_filter = "best";
	break;
    }

    xcb_render_set_picture_filter(surface->dpy, surface->src_picture,
			     strlen(render_filter), render_filter, 0, NULL);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xcb_surface_set_repeat (cairo_xcb_surface_t *surface, int repeat)
{
    uint32_t mask = XCB_RENDER_CP_REPEAT;
    uint32_t pa[] = { repeat };

    if (!surface->src_picture)
	return CAIRO_STATUS_SUCCESS;

    xcb_render_change_picture (surface->dpy, surface->src_picture, mask, pa);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_xcb_surface_set_attributes (cairo_xcb_surface_t	      *surface,
				   cairo_surface_attributes_t *attributes)
{
    cairo_int_status_t status;

    _cairo_xcb_surface_ensure_src_picture (surface);

    status = _cairo_xcb_surface_set_matrix (surface, &attributes->matrix);
    if (status)
	return status;

    switch (attributes->extend) {
    case CAIRO_EXTEND_NONE:
	_cairo_xcb_surface_set_repeat (surface, 0);
	break;
    case CAIRO_EXTEND_REPEAT:
	_cairo_xcb_surface_set_repeat (surface, 1);
	break;
    case CAIRO_EXTEND_REFLECT:
    case CAIRO_EXTEND_PAD:
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    status = _cairo_xcb_surface_set_filter (surface, attributes->filter);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

/* Checks whether we can can directly draw from src to dst with
 * the core protocol: either with CopyArea or using src as a
 * a tile in a GC.
 */
static cairo_bool_t
_surfaces_compatible (cairo_xcb_surface_t *dst,
		      cairo_xcb_surface_t *src)
{
    /* same screen */
    if (!_cairo_xcb_surface_same_screen (dst, src))
	return FALSE;

    /* same depth (for core) */
    if (src->depth != dst->depth)
	return FALSE;

    /* if Render is supported, match picture formats */
    if (src->xrender_format.id != XCB_NONE && src->xrender_format.id == dst->xrender_format.id)
	return TRUE;

    /* Without Render, match visuals instead */
    if (src->visual == dst->visual)
	return TRUE;

    return FALSE;
}

static cairo_bool_t
_surface_has_alpha (cairo_xcb_surface_t *surface)
{
    if (surface->xrender_format.id != XCB_NONE) {
	if (surface->xrender_format.type == XCB_RENDER_PICT_TYPE_DIRECT &&
	    surface->xrender_format.direct.alpha_mask != 0)
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
_categorize_composite_operation (cairo_xcb_surface_t *dst,
				 cairo_operator_t      op,
				 cairo_pattern_t      *src_pattern,
				 cairo_bool_t	       have_mask)

{
#if XXX_BUGGY_REPEAT
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

	    if (_cairo_surface_is_xcb (surface_pattern->surface)) {
		cairo_xcb_surface_t *src = (cairo_xcb_surface_t *)surface_pattern->surface;

		if (op == CAIRO_OPERATOR_OVER && _surface_has_alpha (src))
		    return DO_UNSUPPORTED;

		/* If these are on the same screen but otherwise incompatible,
		 * make a copy as core drawing can't cross depths and doesn't
		 * work rightacross visuals of the same depth
		 */
		if (_cairo_xcb_surface_same_screen (dst, src) &&
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
#endif
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
_recategorize_composite_operation (cairo_xcb_surface_t	      *dst,
				   cairo_operator_t	       op,
				   cairo_xcb_surface_t	      *src,
				   cairo_surface_attributes_t *src_attr,
				   cairo_bool_t		       have_mask)
{
    cairo_bool_t is_integer_translation =
	_cairo_matrix_is_integer_translation (&src_attr->matrix, NULL, NULL);
    cairo_bool_t needs_alpha_composite =
	_operator_needs_alpha_composite (op, _surface_has_alpha (src));

    if (!have_mask &&
	is_integer_translation &&
	src_attr->extend == CAIRO_EXTEND_NONE &&
	!needs_alpha_composite &&
	_surfaces_compatible(src, dst))
    {
	return DO_XCOPYAREA;
    }
#if XXX_BUGGY_REPEAT
    if (!dst->buggy_repeat)
	return DO_RENDER;

    if (is_integer_translation &&
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
#endif
    return DO_RENDER;
}

static int
_render_operator (cairo_operator_t op)
{
    switch (op) {
    case CAIRO_OPERATOR_CLEAR:
	return XCB_RENDER_PICT_OP_CLEAR;
    case CAIRO_OPERATOR_SOURCE:
	return XCB_RENDER_PICT_OP_SRC;
    case CAIRO_OPERATOR_DEST:
	return XCB_RENDER_PICT_OP_DST;
    case CAIRO_OPERATOR_OVER:
	return XCB_RENDER_PICT_OP_OVER;
    case CAIRO_OPERATOR_DEST_OVER:
	return XCB_RENDER_PICT_OP_OVER_REVERSE;
    case CAIRO_OPERATOR_IN:
	return XCB_RENDER_PICT_OP_IN;
    case CAIRO_OPERATOR_DEST_IN:
	return XCB_RENDER_PICT_OP_IN_REVERSE;
    case CAIRO_OPERATOR_OUT:
	return XCB_RENDER_PICT_OP_OUT;
    case CAIRO_OPERATOR_DEST_OUT:
	return XCB_RENDER_PICT_OP_OUT_REVERSE;
    case CAIRO_OPERATOR_ATOP:
	return XCB_RENDER_PICT_OP_ATOP;
    case CAIRO_OPERATOR_DEST_ATOP:
	return XCB_RENDER_PICT_OP_ATOP_REVERSE;
    case CAIRO_OPERATOR_XOR:
	return XCB_RENDER_PICT_OP_XOR;
    case CAIRO_OPERATOR_ADD:
	return XCB_RENDER_PICT_OP_ADD;
    case CAIRO_OPERATOR_SATURATE:
	return XCB_RENDER_PICT_OP_SATURATE;
    default:
	return XCB_RENDER_PICT_OP_OVER;
    }
}

static cairo_int_status_t
_cairo_xcb_surface_composite (cairo_operator_t		op,
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
    cairo_xcb_surface_t		*dst = abstract_dst;
    cairo_xcb_surface_t		*src;
    cairo_xcb_surface_t		*mask;
    cairo_int_status_t		status;
    composite_operation_t       operation;
    int				itx, ity;

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

    operation = _recategorize_composite_operation (dst, op, src, &src_attr,
						   mask_pattern != NULL);
    if (operation == DO_UNSUPPORTED) {
	status = CAIRO_INT_STATUS_UNSUPPORTED;
	goto BAIL;
    }

    status = _cairo_xcb_surface_set_attributes (src, &src_attr);
    if (status)
	goto BAIL;

    switch (operation)
    {
    case DO_RENDER:
	_cairo_xcb_surface_ensure_dst_picture (dst);
	if (mask) {
	    status = _cairo_xcb_surface_set_attributes (mask, &mask_attr);
	    if (status)
		goto BAIL;

	    xcb_render_composite (dst->dpy,
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
	    static xcb_render_picture_t maskpict = { XCB_NONE };

	    xcb_render_composite (dst->dpy,
				_render_operator (op),
				src->src_picture,
				maskpict,
				dst->dst_picture,
				src_x + src_attr.x_offset,
				src_y + src_attr.y_offset,
				0, 0,
				dst_x, dst_y,
				width, height);
	}
	break;

    case DO_XCOPYAREA:
	_cairo_xcb_surface_ensure_gc (dst);
	xcb_copy_area (dst->dpy,
		   src->drawable,
		   dst->drawable,
		   dst->gc,
		   src_x + src_attr.x_offset,
		   src_y + src_attr.y_offset,
		   dst_x, dst_y,
		   width, height);
	break;

    case DO_XTILE:
	/* This case is only used for bug fallbacks, though it is theoretically
	 * applicable to the case where we don't have the RENDER extension as
	 * well.
	 *
	 * We've checked that we have a repeating unscaled source in
	 * _recategorize_composite_operation.
	 */

	_cairo_xcb_surface_ensure_gc (dst);
	_cairo_matrix_is_integer_translation (&src_attr.matrix, &itx, &ity);
	{
	    uint32_t mask = XCB_GC_FILL_STYLE | XCB_GC_TILE
	                  | XCB_GC_TILE_STIPPLE_ORIGIN_X
	                  | XCB_GC_TILE_STIPPLE_ORIGIN_Y;
	    uint32_t values[] = {
		XCB_FILL_STYLE_TILED, src->drawable,
		- (itx + src_attr.x_offset),
		- (ity + src_attr.y_offset)
	    };
	    xcb_rectangle_t rect = { dst_x, dst_y, width, height };

	    xcb_change_gc( dst->dpy, dst->gc, mask, values );
	    xcb_poly_fill_rectangle(dst->dpy, dst->drawable, dst->gc, 1, &rect);
	}
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
_cairo_xcb_surface_fill_rectangles (void			     *abstract_surface,
				     cairo_operator_t	      op,
				     const cairo_color_t	*     color,
				     cairo_rectangle_int_t *rects,
				     int			      num_rects)
{
    cairo_xcb_surface_t *surface = abstract_surface;
    xcb_render_color_t render_color;
    xcb_rectangle_t static_xrects[16];
    xcb_rectangle_t *xrects = static_xrects;
    int i;

    if (!CAIRO_SURFACE_RENDER_HAS_FILL_RECTANGLE (surface))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    render_color.red   = color->red_short;
    render_color.green = color->green_short;
    render_color.blue  = color->blue_short;
    render_color.alpha = color->alpha_short;

    if (num_rects > ARRAY_LENGTH(static_xrects)) {
        xrects = _cairo_malloc_ab (num_rects, sizeof(xcb_rectangle_t));
	if (xrects == NULL)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    for (i = 0; i < num_rects; i++) {
        xrects[i].x = rects[i].x;
        xrects[i].y = rects[i].y;
        xrects[i].width = rects[i].width;
        xrects[i].height = rects[i].height;
    }

    _cairo_xcb_surface_ensure_dst_picture (surface);
    xcb_render_fill_rectangles (surface->dpy,
			   _render_operator (op),
			   surface->dst_picture,
			   render_color, num_rects, xrects);

    if (xrects != static_xrects)
        free(xrects);

    return CAIRO_STATUS_SUCCESS;
}

/* Creates an A8 picture of size @width x @height, initialized with @color
 */
static xcb_render_picture_t
_create_a8_picture (cairo_xcb_surface_t *surface,
		    xcb_render_color_t   *color,
		    int                   width,
		    int                   height,
		    cairo_bool_t          repeat)
{
    uint32_t values[] = { TRUE };
    uint32_t mask = repeat ? XCB_RENDER_CP_REPEAT : 0;

    xcb_pixmap_t pixmap = xcb_generate_id (surface->dpy);
    xcb_render_picture_t picture = xcb_generate_id (surface->dpy);

    xcb_render_pictforminfo_t *format
	= _CAIRO_FORMAT_TO_XRENDER_FORMAT (surface->dpy, CAIRO_FORMAT_A8);
    xcb_rectangle_t rect = { 0, 0, width, height };

    xcb_create_pixmap (surface->dpy, 8, pixmap, surface->drawable,
		       width <= 0 ? 1 : width,
		       height <= 0 ? 1 : height);
    xcb_render_create_picture (surface->dpy, picture, pixmap, format->id, mask, values);
    xcb_render_fill_rectangles (surface->dpy, XCB_RENDER_PICT_OP_SRC, picture, *color, 1, &rect);
    xcb_free_pixmap (surface->dpy, pixmap);

    return picture;
}

/* Creates a temporary mask for the trapezoids covering the area
 * [@dst_x, @dst_y, @width, @height] of the destination surface.
 */
static xcb_render_picture_t
_create_trapezoid_mask (cairo_xcb_surface_t *dst,
			cairo_trapezoid_t    *traps,
			int                   num_traps,
			int                   dst_x,
			int                   dst_y,
			int                   width,
			int                   height,
			xcb_render_pictforminfo_t *pict_format)
{
    xcb_render_color_t transparent = { 0, 0, 0, 0 };
    xcb_render_color_t solid = { 0xffff, 0xffff, 0xffff, 0xffff };
    xcb_render_picture_t mask_picture, solid_picture;
    xcb_render_trapezoid_t *offset_traps;
    int i;

    /* This would be considerably simpler using XRenderAddTraps(), but since
     * we are only using this in the unbounded-operator case, we stick with
     * XRenderCompositeTrapezoids, which is available on older versions
     * of RENDER rather than conditionalizing. We should still hit an
     * optimization that avoids creating another intermediate surface on
     * the servers that have XRenderAddTraps().
     */
    mask_picture = _create_a8_picture (dst, &transparent, width, height, FALSE);
    solid_picture = _create_a8_picture (dst, &solid, width, height, TRUE);

    offset_traps = _cairo_malloc_ab (num_traps, sizeof (xcb_render_trapezoid_t));
    if (!offset_traps) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return XCB_NONE;
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

    xcb_render_trapezoids (dst->dpy, XCB_RENDER_PICT_OP_ADD,
				solid_picture, mask_picture,
				pict_format->id,
				0, 0,
				num_traps, offset_traps);

    xcb_render_free_picture (dst->dpy, solid_picture);
    free (offset_traps);

    return mask_picture;
}

static cairo_int_status_t
_cairo_xcb_surface_composite_trapezoids (cairo_operator_t	op,
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
    cairo_xcb_surface_t		*dst = abstract_dst;
    cairo_xcb_surface_t		*src;
    cairo_int_status_t		status;
    composite_operation_t       operation;
    int				render_reference_x, render_reference_y;
    int				render_src_x, render_src_y;
    int				cairo_format;
    xcb_render_pictforminfo_t	*render_format;

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
	cairo_format = CAIRO_FORMAT_A1;
	break;
    case CAIRO_ANTIALIAS_GRAY:
    case CAIRO_ANTIALIAS_SUBPIXEL:
    case CAIRO_ANTIALIAS_DEFAULT:
    default:
	cairo_format = CAIRO_FORMAT_A8;
	break;
    }
    render_format = _CAIRO_FORMAT_TO_XRENDER_FORMAT (dst->dpy, cairo_format);
    /* XXX: what to do if render_format is null? */

    if (traps[0].left.p1.y < traps[0].left.p2.y) {
	render_reference_x = _cairo_fixed_integer_floor (traps[0].left.p1.x);
	render_reference_y = _cairo_fixed_integer_floor (traps[0].left.p1.y);
    } else {
	render_reference_x = _cairo_fixed_integer_floor (traps[0].left.p2.x);
	render_reference_y = _cairo_fixed_integer_floor (traps[0].left.p2.y);
    }

    render_src_x = src_x + render_reference_x - dst_x;
    render_src_y = src_y + render_reference_y - dst_y;

    _cairo_xcb_surface_ensure_dst_picture (dst);
    status = _cairo_xcb_surface_set_attributes (src, &attributes);
    if (status)
	goto BAIL;

    if (!_cairo_operator_bounded_by_mask (op)) {
	/* xcb_render_composite+trapezoids() creates a mask only large enough for the
	 * trapezoids themselves, but if the operator is unbounded, then we need
	 * to actually composite all the way out to the bounds, so we create
	 * the mask and composite ourselves. There actually would
	 * be benefit to doing this in all cases, since RENDER implementations
	 * will frequently create a too temporary big mask, ignoring destination
	 * bounds and clip. (xcb_render_add_traps() could be used to make creating
	 * the mask somewhat cheaper.)
	 */
	xcb_render_picture_t mask_picture = _create_trapezoid_mask (dst, traps, num_traps,
						       dst_x, dst_y, width, height,
						       render_format);
	if (!mask_picture) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto BAIL;
	}

	xcb_render_composite (dst->dpy,
			  _render_operator (op),
			  src->src_picture,
			  mask_picture,
			  dst->dst_picture,
			  src_x + attributes.x_offset,
			  src_y + attributes.y_offset,
			  0, 0,
			  dst_x, dst_y,
			  width, height);

	xcb_render_free_picture (dst->dpy, mask_picture);

	status = _cairo_surface_composite_shape_fixup_unbounded (&dst->base,
								 &attributes, src->width, src->height,
								 width, height,
								 src_x, src_y,
								 0, 0,
								 dst_x, dst_y, width, height);

    } else {
        xcb_render_trapezoid_t xtraps_stack[16];
        xcb_render_trapezoid_t *xtraps = xtraps_stack;
        int i;

        if (num_traps > ARRAY_LENGTH(xtraps_stack)) {
            xtraps = _cairo_malloc_ab (num_traps, sizeof(xcb_render_trapezoid_t));
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

	xcb_render_trapezoids (dst->dpy,
				    _render_operator (op),
				    src->src_picture, dst->dst_picture,
				    render_format->id,
				    render_src_x + attributes.x_offset,
				    render_src_y + attributes.y_offset,
				    num_traps, xtraps);

        if (xtraps != xtraps_stack)
            free(xtraps);
    }

 BAIL:
    _cairo_pattern_release_surface (pattern, &src->base, &attributes);

    return status;
}

static cairo_int_status_t
_cairo_xcb_surface_set_clip_region (void           *abstract_surface,
				    cairo_region_t *region)
{
    cairo_xcb_surface_t *surface = abstract_surface;

    if (surface->clip_rects) {
	free (surface->clip_rects);
	surface->clip_rects = NULL;
    }

    surface->have_clip_rects = FALSE;
    surface->num_clip_rects = 0;

    if (region == NULL) {
	uint32_t none[] = { XCB_NONE };
	if (surface->gc)
	    xcb_change_gc (surface->dpy, surface->gc, XCB_GC_CLIP_MASK, none);

	if (surface->xrender_format.id != XCB_NONE && surface->dst_picture)
	    xcb_render_change_picture (surface->dpy, surface->dst_picture,
		XCB_RENDER_CP_CLIP_MASK, none);
    } else {
	cairo_box_int_t *boxes;
	cairo_status_t status;
	xcb_rectangle_t *rects = NULL;
	int n_boxes, i;

	status = _cairo_region_get_boxes (region, &n_boxes, &boxes);
        if (status)
            return status;

	if (n_boxes > 0) {
	    rects = _cairo_malloc_ab (n_boxes, sizeof(xcb_rectangle_t));
	    if (rects == NULL) {
                _cairo_region_boxes_fini (region, boxes);
		return _cairo_error (CAIRO_STATUS_NO_MEMORY);
            }
	} else {
	    rects = NULL;
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

	if (surface->gc)
	    _cairo_xcb_surface_set_gc_clip_rects (surface);

	if (surface->dst_picture)
	    _cairo_xcb_surface_set_picture_clip_rects (surface);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_xcb_surface_get_extents (void		        *abstract_surface,
				cairo_rectangle_int_t   *rectangle)
{
    cairo_xcb_surface_t *surface = abstract_surface;

    rectangle->x = 0;
    rectangle->y = 0;

    rectangle->width  = surface->width;
    rectangle->height = surface->height;

    return CAIRO_STATUS_SUCCESS;
}

/* XXX: _cairo_xcb_surface_get_font_options */

static void
_cairo_xcb_surface_scaled_font_fini (cairo_scaled_font_t *scaled_font);

static void
_cairo_xcb_surface_scaled_glyph_fini (cairo_scaled_glyph_t *scaled_glyph,
				       cairo_scaled_font_t  *scaled_font);

static cairo_int_status_t
_cairo_xcb_surface_show_glyphs (void                *abstract_dst,
				 cairo_operator_t     op,
				 cairo_pattern_t     *src_pattern,
				 cairo_glyph_t       *glyphs,
				 int		      num_glyphs,
				 cairo_scaled_font_t *scaled_font);

static cairo_bool_t
_cairo_xcb_surface_is_similar (void *surface_a,
	                       void *surface_b,
			       cairo_content_t content)
{
    cairo_xcb_surface_t *a = surface_a;
    cairo_xcb_surface_t *b = surface_b;
    xcb_render_pictforminfo_t *xrender_format;

    if (! _cairo_xcb_surface_same_screen (a, b))
	return FALSE;

    /* now check that the target is a similar format */
    xrender_format = _CAIRO_FORMAT_TO_XRENDER_FORMAT (b->dpy,
	    _cairo_format_from_content (content));

    return a->xrender_format.id == xrender_format->id;
}

static cairo_status_t
_cairo_xcb_surface_reset (void *abstract_surface)
{
    cairo_xcb_surface_t *surface = abstract_surface;
    cairo_status_t status;

    status = _cairo_xcb_surface_set_clip_region (surface, NULL);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}


/* XXX: move this to the bottom of the file, XCB and Xlib */

static const cairo_surface_backend_t cairo_xcb_surface_backend = {
    CAIRO_SURFACE_TYPE_XCB,
    _cairo_xcb_surface_create_similar,
    _cairo_xcb_surface_finish,
    _cairo_xcb_surface_acquire_source_image,
    _cairo_xcb_surface_release_source_image,
    _cairo_xcb_surface_acquire_dest_image,
    _cairo_xcb_surface_release_dest_image,
    _cairo_xcb_surface_clone_similar,
    _cairo_xcb_surface_composite,
    _cairo_xcb_surface_fill_rectangles,
    _cairo_xcb_surface_composite_trapezoids,
    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_xcb_surface_set_clip_region,
    NULL, /* intersect_clip_path */
    _cairo_xcb_surface_get_extents,
    NULL, /* old_show_glyphs */
    NULL, /* get_font_options */
    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */
    _cairo_xcb_surface_scaled_font_fini,
    _cairo_xcb_surface_scaled_glyph_fini,

    NULL, /* paint */
    NULL, /* mask */
    NULL, /* stroke */
    NULL, /* fill */
    _cairo_xcb_surface_show_glyphs,
    NULL,  /* snapshot */

    _cairo_xcb_surface_is_similar,

    _cairo_xcb_surface_reset
};

/**
 * _cairo_surface_is_xcb:
 * @surface: a #cairo_surface_t
 *
 * Checks if a surface is a #cairo_xcb_surface_t
 *
 * Return value: True if the surface is an xcb surface
 **/
static cairo_bool_t
_cairo_surface_is_xcb (cairo_surface_t *surface)
{
    return surface->backend == &cairo_xcb_surface_backend;
}

static cairo_surface_t *
_cairo_xcb_surface_create_internal (xcb_connection_t	     *dpy,
				    xcb_drawable_t		      drawable,
				    xcb_screen_t		     *screen,
				    xcb_visualtype_t	     *visual,
				    xcb_render_pictforminfo_t    *xrender_format,
				    int			      width,
				    int			      height,
				    int			      depth)
{
    cairo_xcb_surface_t *surface;
    const xcb_render_query_version_reply_t *r;

    surface = malloc (sizeof (cairo_xcb_surface_t));
    if (surface == NULL)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    if (xrender_format) {
	depth = xrender_format->depth;
    } else if (visual) {
	xcb_depth_iterator_t depths;
	xcb_visualtype_iterator_t visuals;

	/* This is ugly, but we have to walk over all visuals
	 * for the screen to find the depth.
	 */
	depths = xcb_screen_allowed_depths_iterator(screen);
	for(; depths.rem; xcb_depth_next(&depths))
	{
	    visuals = xcb_depth_visuals_iterator(depths.data);
	    for(; visuals.rem; xcb_visualtype_next(&visuals))
	    {
		if(visuals.data->visual_id == visual->visual_id)
		{
		    depth = depths.data->depth;
		    goto found;
		}
	    }
	}
    found:
	;
    }

    r = xcb_render_util_query_version(dpy);
    if (r) {
	surface->render_major = r->major_version;
	surface->render_minor = r->minor_version;
    } else {
	surface->render_major = -1;
	surface->render_minor = -1;
    }

    if (CAIRO_SURFACE_RENDER_HAS_CREATE_PICTURE (surface)) {
	if (!xrender_format) {
	    if (visual) {
		const xcb_render_query_pict_formats_reply_t *formats;
		xcb_render_pictvisual_t *pict_visual;
		formats = xcb_render_util_query_formats (dpy);
		pict_visual = xcb_render_util_find_visual_format (formats, visual->visual_id);
		if (pict_visual) {
		    xcb_render_pictforminfo_t template;
		    template.id = pict_visual->format;
		    xrender_format = xcb_render_util_find_format (formats, XCB_PICT_FORMAT_ID, &template, 0);
		}
	    } else if (depth == 1) {
		xrender_format = _CAIRO_FORMAT_TO_XRENDER_FORMAT (dpy, CAIRO_FORMAT_A1);
	    }
	}
    } else {
	xrender_format = NULL;
    }

    _cairo_surface_init (&surface->base, &cairo_xcb_surface_backend,
			 _xcb_render_format_to_content (xrender_format));

    surface->dpy = dpy;

    surface->gc = XCB_NONE;
    surface->drawable = drawable;
    surface->screen = screen;
    surface->owns_pixmap = FALSE;
    surface->use_pixmap = 0;
    surface->width = width;
    surface->height = height;

    /* XXX: set buggy_repeat based on ServerVendor and VendorRelease */

    surface->dst_picture = XCB_NONE;
    surface->src_picture = XCB_NONE;

    surface->visual = visual;
    surface->xrender_format.id = XCB_NONE;
    if (xrender_format) surface->xrender_format = *xrender_format;
    surface->depth = depth;

    surface->have_clip_rects = FALSE;
    surface->clip_rects = NULL;
    surface->num_clip_rects = 0;

    return (cairo_surface_t *) surface;
}

static xcb_screen_t *
_cairo_xcb_screen_from_visual (xcb_connection_t *c, xcb_visualtype_t *visual)
{
    xcb_depth_iterator_t d;
    xcb_screen_iterator_t s = xcb_setup_roots_iterator(xcb_get_setup(c));
    for (; s.rem; xcb_screen_next(&s))
    {
	if (s.data->root_visual == visual->visual_id)
	    return s.data;

	d = xcb_screen_allowed_depths_iterator(s.data);
	for (; d.rem; xcb_depth_next(&d))
	{
	    xcb_visualtype_iterator_t v = xcb_depth_visuals_iterator(d.data);
	    for (; v.rem; xcb_visualtype_next(&v))
	    {
		if (v.data->visual_id == visual->visual_id)
		    return s.data;
	    }
	}
    }
    return NULL;
}

/**
 * cairo_xcb_surface_create:
 * @c: an XCB connection
 * @drawable: an XCB drawable
 * @visual: the visual to use for drawing to @drawable. The depth
 *          of the visual must match the depth of the drawable.
 *          Currently, only TrueColor visuals are fully supported.
 * @width: the current width of @drawable.
 * @height: the current height of @drawable.
 *
 * Creates an XCB surface that draws to the given drawable.
 * The way that colors are represented in the drawable is specified
 * by the provided visual.
 *
 * Note: If @drawable is a window, then the function
 * cairo_xcb_surface_set_size must be called whenever the size of the
 * window changes.
 *
 * Return value: the newly created surface
 **/
cairo_surface_t *
cairo_xcb_surface_create (xcb_connection_t *c,
			  xcb_drawable_t	 drawable,
			  xcb_visualtype_t *visual,
			  int		 width,
			  int		 height)
{
    xcb_screen_t	*screen = _cairo_xcb_screen_from_visual (c, visual);

    if (screen == NULL)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_VISUAL));

    return _cairo_xcb_surface_create_internal (c, drawable, screen,
					       visual, NULL,
					       width, height, 0);
}

/**
 * cairo_xcb_surface_create_for_bitmap:
 * @c: an XCB connection
 * @bitmap: an XCB Pixmap (a depth-1 pixmap)
 * @screen: an XCB Screen
 * @width: the current width of @bitmap
 * @height: the current height of @bitmap
 *
 * Creates an XCB surface that draws to the given bitmap.
 * This will be drawn to as a %CAIRO_FORMAT_A1 object.
 *
 * Return value: the newly created surface
 **/
cairo_surface_t *
cairo_xcb_surface_create_for_bitmap (xcb_connection_t     *c,
				     xcb_pixmap_t		bitmap,
				     xcb_screen_t	       *screen,
				     int		width,
				     int		height)
{
    return _cairo_xcb_surface_create_internal (c, bitmap, screen,
					       NULL, NULL,
					       width, height, 1);
}

/**
 * cairo_xcb_surface_create_with_xrender_format:
 * @c: an XCB connection
 * @drawable: an XCB drawable
 * @screen: the XCB screen associated with @drawable
 * @format: the picture format to use for drawing to @drawable. The
 *          depth of @format mush match the depth of the drawable.
 * @width: the current width of @drawable
 * @height: the current height of @drawable
 *
 * Creates an XCB surface that draws to the given drawable.
 * The way that colors are represented in the drawable is specified
 * by the provided picture format.
 *
 * Note: If @drawable is a Window, then the function
 * cairo_xcb_surface_set_size must be called whenever the size of the
 * window changes.
 *
 * Return value: the newly created surface.
 **/
cairo_surface_t *
cairo_xcb_surface_create_with_xrender_format (xcb_connection_t	    *c,
					      xcb_drawable_t	     drawable,
					      xcb_screen_t		    *screen,
					      xcb_render_pictforminfo_t *format,
					      int		     width,
					      int		     height)
{
    return _cairo_xcb_surface_create_internal (c, drawable, screen,
					       NULL, format,
					       width, height, 0);
}
slim_hidden_def (cairo_xcb_surface_create_with_xrender_format);

/**
 * cairo_xcb_surface_set_size:
 * @surface: a #cairo_surface_t for the XCB backend
 * @width: the new width of the surface
 * @height: the new height of the surface
 *
 * Informs cairo of the new size of the XCB drawable underlying the
 * surface. For a surface created for a window (rather than a pixmap),
 * this function must be called each time the size of the window
 * changes. (For a subwindow, you are normally resizing the window
 * yourself, but for a toplevel window, it is necessary to listen for
 * ConfigureNotify events.)
 *
 * A pixmap can never change size, so it is never necessary to call
 * this function on a surface created for a pixmap.
 **/
void
cairo_xcb_surface_set_size (cairo_surface_t *abstract_surface,
			     int              width,
			     int              height)
{
    cairo_xcb_surface_t *surface = (cairo_xcb_surface_t *) abstract_surface;

    if (! _cairo_surface_is_xcb (abstract_surface)) {
	_cairo_surface_set_error (abstract_surface,
				  CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return;
    }

    surface->width = width;
    surface->height = height;
}

/*
 *  Glyph rendering support
 */

typedef struct _cairo_xcb_surface_font_private {
    xcb_connection_t		*dpy;
    xcb_render_glyphset_t	glyphset;
    cairo_format_t		format;
    xcb_render_pictforminfo_t	*xrender_format;
} cairo_xcb_surface_font_private_t;

static cairo_status_t
_cairo_xcb_surface_font_init (xcb_connection_t		    *dpy,
			       cairo_scaled_font_t  *scaled_font,
			       cairo_format_t	     format)
{
    cairo_xcb_surface_font_private_t	*font_private;

    font_private = malloc (sizeof (cairo_xcb_surface_font_private_t));
    if (!font_private)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    font_private->dpy = dpy;
    font_private->format = format;
    font_private->xrender_format = _CAIRO_FORMAT_TO_XRENDER_FORMAT(dpy, format);
    font_private->glyphset = xcb_generate_id(dpy);
    xcb_render_create_glyph_set (dpy, font_private->glyphset, font_private->xrender_format->id);

    scaled_font->surface_private = font_private;
    scaled_font->surface_backend = &cairo_xcb_surface_backend;
    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_xcb_surface_scaled_font_fini (cairo_scaled_font_t *scaled_font)
{
    cairo_xcb_surface_font_private_t	*font_private = scaled_font->surface_private;

    if (font_private) {
	xcb_render_free_glyph_set (font_private->dpy, font_private->glyphset);
	free (font_private);
    }
}

static void
_cairo_xcb_surface_scaled_glyph_fini (cairo_scaled_glyph_t *scaled_glyph,
				       cairo_scaled_font_t  *scaled_font)
{
    cairo_xcb_surface_font_private_t	*font_private = scaled_font->surface_private;

    if (font_private != NULL && scaled_glyph->surface_private != NULL) {
	xcb_render_glyph_t glyph_index = _cairo_scaled_glyph_index(scaled_glyph);
	xcb_render_free_glyphs (font_private->dpy,
			   font_private->glyphset,
			   1, &glyph_index);
    }
}

static cairo_bool_t
_native_byte_order_lsb (void)
{
    int	x = 1;

    return *((char *) &x) == 1;
}

static cairo_status_t
_cairo_xcb_surface_add_glyph (xcb_connection_t *dpy,
			       cairo_scaled_font_t  *scaled_font,
			       cairo_scaled_glyph_t *scaled_glyph)
{
    xcb_render_glyphinfo_t glyph_info;
    xcb_render_glyph_t glyph_index;
    unsigned char *data;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_xcb_surface_font_private_t *font_private;
    cairo_image_surface_t *glyph_surface = scaled_glyph->surface;

    if (scaled_font->surface_private == NULL) {
	status = _cairo_xcb_surface_font_init (dpy, scaled_font,
						glyph_surface->format);
	if (status)
	    return status;
    }
    font_private = scaled_font->surface_private;

    /* If the glyph format does not match the font format, then we
     * create a temporary surface for the glyph image with the font's
     * format.
     */
    if (glyph_surface->format != font_private->format) {
	cairo_t *cr;
	cairo_surface_t *tmp_surface;
	double x_offset, y_offset;

	tmp_surface = cairo_image_surface_create (font_private->format,
						  glyph_surface->width,
						  glyph_surface->height);
	cr = cairo_create (tmp_surface);
	cairo_surface_get_device_offset (&glyph_surface->base, &x_offset, &y_offset);
	cairo_set_source_surface (cr, &glyph_surface->base, x_offset, y_offset);
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint (cr);

	status = cairo_status (cr);

	cairo_destroy (cr);

	tmp_surface->device_transform = glyph_surface->base.device_transform;
	tmp_surface->device_transform_inverse = glyph_surface->base.device_transform_inverse;

	glyph_surface = (cairo_image_surface_t *) tmp_surface;

	if (status)
	    goto BAIL;
    }

    /* XXX: FRAGILE: We're ignore device_transform scaling here. A bug? */
    glyph_info.x = _cairo_lround (glyph_surface->base.device_transform.x0);
    glyph_info.y = _cairo_lround (glyph_surface->base.device_transform.y0);
    glyph_info.width = glyph_surface->width;
    glyph_info.height = glyph_surface->height;
    glyph_info.x_off = 0;
    glyph_info.y_off = 0;

    data = glyph_surface->data;

    /* flip formats around */
    switch (scaled_glyph->surface->format) {
    case CAIRO_FORMAT_A1:
	/* local bitmaps are always stored with bit == byte */
	if (_native_byte_order_lsb() != (xcb_get_setup(dpy)->bitmap_format_bit_order == XCB_IMAGE_ORDER_LSB_FIRST)) {
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
	if (_native_byte_order_lsb() != (xcb_get_setup(dpy)->image_byte_order == XCB_IMAGE_ORDER_LSB_FIRST)) {
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

    xcb_render_add_glyphs (dpy, font_private->glyphset,
		      1, &glyph_index, &glyph_info,
		      glyph_surface->stride * glyph_surface->height,
		      data);

    if (data != glyph_surface->data)
	free (data);

 BAIL:
    if (glyph_surface != scaled_glyph->surface)
	cairo_surface_destroy (&glyph_surface->base);

    return status;
}

#define N_STACK_BUF 1024

static cairo_status_t
_cairo_xcb_surface_show_glyphs_8  (cairo_xcb_surface_t *dst,
                                   cairo_operator_t op,
                                   cairo_xcb_surface_t *src,
                                   int src_x_offset, int src_y_offset,
                                   const cairo_glyph_t *glyphs,
                                   int num_glyphs,
                                   cairo_scaled_font_t *scaled_font)
{
    cairo_xcb_surface_font_private_t *font_private = scaled_font->surface_private;
    xcb_render_util_composite_text_stream_t *stream;
    int i;
    int thisX, thisY;
    int lastX = 0, lastY = 0;
    uint8_t glyph;

    stream = xcb_render_util_composite_text_stream (font_private->glyphset, num_glyphs, 0);

    for (i = 0; i < num_glyphs; ++i) {
	thisX = _cairo_lround (glyphs[i].x);
	thisY = _cairo_lround (glyphs[i].y);
	glyph = glyphs[i].index;
	xcb_render_util_glyphs_8 (stream, thisX - lastX, thisY - lastY, 1, &glyph);
	lastX = thisX;
	lastY = thisY;
    }

    xcb_render_util_composite_text (dst->dpy,
			    _render_operator (op),
			    src->src_picture,
			    dst->dst_picture,
			    font_private->xrender_format->id,
                            src_x_offset + _cairo_lround (glyphs[0].x),
                            src_y_offset + _cairo_lround (glyphs[0].y),
			    stream);

    xcb_render_util_composite_text_free (stream);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xcb_surface_show_glyphs_16 (cairo_xcb_surface_t *dst,
                                   cairo_operator_t op,
                                   cairo_xcb_surface_t *src,
                                   int src_x_offset, int src_y_offset,
                                   const cairo_glyph_t *glyphs,
                                   int num_glyphs,
                                   cairo_scaled_font_t *scaled_font)
{
    cairo_xcb_surface_font_private_t *font_private = scaled_font->surface_private;
    xcb_render_util_composite_text_stream_t *stream;
    int i;
    int thisX, thisY;
    int lastX = 0, lastY = 0;
    uint16_t glyph;

    stream = xcb_render_util_composite_text_stream (font_private->glyphset, num_glyphs, 0);

    for (i = 0; i < num_glyphs; ++i) {
	thisX = _cairo_lround (glyphs[i].x);
	thisY = _cairo_lround (glyphs[i].y);
	glyph = glyphs[i].index;
	xcb_render_util_glyphs_16 (stream, thisX - lastX, thisY - lastY, 1, &glyph);
	lastX = thisX;
	lastY = thisY;
    }

    xcb_render_util_composite_text (dst->dpy,
			    _render_operator (op),
			    src->src_picture,
			    dst->dst_picture,
			    font_private->xrender_format->id,
                            src_x_offset + _cairo_lround (glyphs[0].x),
                            src_y_offset + _cairo_lround (glyphs[0].y),
			    stream);

    xcb_render_util_composite_text_free (stream);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xcb_surface_show_glyphs_32 (cairo_xcb_surface_t *dst,
                                   cairo_operator_t op,
                                   cairo_xcb_surface_t *src,
                                   int src_x_offset, int src_y_offset,
                                   const cairo_glyph_t *glyphs,
                                   int num_glyphs,
                                   cairo_scaled_font_t *scaled_font)
{
    cairo_xcb_surface_font_private_t *font_private = scaled_font->surface_private;
    xcb_render_util_composite_text_stream_t *stream;
    int i;
    int thisX, thisY;
    int lastX = 0, lastY = 0;
    uint32_t glyph;

    stream = xcb_render_util_composite_text_stream (font_private->glyphset, num_glyphs, 0);

    for (i = 0; i < num_glyphs; ++i) {
	thisX = _cairo_lround (glyphs[i].x);
	thisY = _cairo_lround (glyphs[i].y);
	glyph = glyphs[i].index;
	xcb_render_util_glyphs_32 (stream, thisX - lastX, thisY - lastY, 1, &glyph);
	lastX = thisX;
	lastY = thisY;
    }

    xcb_render_util_composite_text (dst->dpy,
			    _render_operator (op),
			    src->src_picture,
			    dst->dst_picture,
			    font_private->xrender_format->id,
                            src_x_offset + _cairo_lround (glyphs[0].x),
                            src_y_offset + _cairo_lround (glyphs[0].y),
			    stream);

    xcb_render_util_composite_text_free (stream);

    return CAIRO_STATUS_SUCCESS;
}

typedef cairo_status_t (*cairo_xcb_surface_show_glyphs_func_t)
    (cairo_xcb_surface_t *, cairo_operator_t, cairo_xcb_surface_t *, int, int,
     const cairo_glyph_t *, int, cairo_scaled_font_t *);

static cairo_int_status_t
_cairo_xcb_surface_show_glyphs (void                *abstract_dst,
				 cairo_operator_t     op,
				 cairo_pattern_t     *src_pattern,
				 cairo_glyph_t       *glyphs,
				 int		      num_glyphs,
				 cairo_scaled_font_t *scaled_font)
{
    cairo_int_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_xcb_surface_t *dst = abstract_dst;

    composite_operation_t operation;
    cairo_surface_attributes_t attributes;
    cairo_xcb_surface_t *src = NULL;

    cairo_glyph_t *output_glyphs;
    const cairo_glyph_t *glyphs_chunk;
    int glyphs_remaining, chunk_size, max_chunk_size;
    cairo_scaled_glyph_t *scaled_glyph;
    cairo_xcb_surface_font_private_t *font_private;

    int i, o;
    unsigned long max_index = 0;

    cairo_xcb_surface_show_glyphs_func_t show_glyphs_func;

    cairo_pattern_union_t solid_pattern;

    if (!CAIRO_SURFACE_RENDER_HAS_COMPOSITE_TEXT (dst) || dst->xrender_format.id == XCB_NONE)
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
	 scaled_font->surface_backend != &cairo_xcb_surface_backend) ||
	(font_private != NULL && font_private->dpy != dst->dpy))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* We make a copy of the glyphs so that we can elide any size-zero
     * glyphs to workaround an X server bug, (present in at least Xorg
     * 7.1 without EXA). */
    output_glyphs = _cairo_malloc_ab (num_glyphs, sizeof (cairo_glyph_t));
    if (output_glyphs == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

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
    } else {
        cairo_rectangle_int_t glyph_extents;

        status = _cairo_scaled_font_glyph_device_extents (scaled_font,
                                                          glyphs,
                                                          num_glyphs,
                                                          &glyph_extents);
        if (status)
	    goto BAIL;

        status = _cairo_pattern_acquire_surface (src_pattern, &dst->base,
                                                 glyph_extents.x, glyph_extents.y,
                                                 glyph_extents.width, glyph_extents.height,
                                                 (cairo_surface_t **) &src,
                                                 &attributes);
    }

    if (status)
        goto BAIL;

    operation = _recategorize_composite_operation (dst, op, src, &attributes, TRUE);
    if (operation == DO_UNSUPPORTED) {
	status = CAIRO_INT_STATUS_UNSUPPORTED;
	goto BAIL;
    }

    status = _cairo_xcb_surface_set_attributes (src, &attributes);
    if (status)
        goto BAIL;

    /* Send all unsent glyphs to the server, and count the max of the glyph indices */
    for (i = 0, o = 0; i < num_glyphs; i++) {
	if (glyphs[i].index > max_index)
	    max_index = glyphs[i].index;
	status = _cairo_scaled_glyph_lookup (scaled_font,
					     glyphs[i].index,
					     CAIRO_SCALED_GLYPH_INFO_SURFACE,
					     &scaled_glyph);
	if (status != CAIRO_STATUS_SUCCESS)
	    goto BAIL;
	/* Don't put any size-zero glyphs into output_glyphs to avoid
	 * an X server bug which stops rendering glyphs after the
	 * first size-zero glyph. */
	if (scaled_glyph->surface->width && scaled_glyph->surface->height) {
	    output_glyphs[o++] = glyphs[i];
	    if (scaled_glyph->surface_private == NULL) {
		_cairo_xcb_surface_add_glyph (dst->dpy, scaled_font, scaled_glyph);
		scaled_glyph->surface_private = (void *) 1;
	    }
	}
    }
    num_glyphs = o;

    _cairo_xcb_surface_ensure_dst_picture (dst);

    max_chunk_size = xcb_get_maximum_request_length (dst->dpy);
    if (max_index < 256) {
	/* XXX: these are all the same size! (28) */
	max_chunk_size -= sizeof(xcb_render_composite_glyphs_8_request_t);
	show_glyphs_func = _cairo_xcb_surface_show_glyphs_8;
    } else if (max_index < 65536) {
	max_chunk_size -= sizeof(xcb_render_composite_glyphs_16_request_t);
	show_glyphs_func = _cairo_xcb_surface_show_glyphs_16;
    } else {
	max_chunk_size -= sizeof(xcb_render_composite_glyphs_32_request_t);
	show_glyphs_func = _cairo_xcb_surface_show_glyphs_32;
    }
    /* XXX: I think this is wrong; this is only the header size (2 longs) */
    /*      but should also include the glyph (1 long) */
    /* max_chunk_size /= sz_xGlyphElt; */
    max_chunk_size /= 3*sizeof(uint32_t);

    for (glyphs_remaining = num_glyphs, glyphs_chunk = output_glyphs;
	 glyphs_remaining;
	 glyphs_remaining -= chunk_size, glyphs_chunk += chunk_size)
    {
	chunk_size = MIN (glyphs_remaining, max_chunk_size);

	status = show_glyphs_func (dst, op, src,
                                   attributes.x_offset, attributes.y_offset,
                                   glyphs_chunk, chunk_size, scaled_font);
	if (status != CAIRO_STATUS_SUCCESS)
	    break;
    }

  BAIL:
    _cairo_scaled_font_thaw_cache (scaled_font);
    free (output_glyphs);

    if (src)
        _cairo_pattern_release_surface (src_pattern, &src->base, &attributes);
    if (src_pattern == &solid_pattern.base)
	_cairo_pattern_fini (&solid_pattern.base);

    return status;
}
