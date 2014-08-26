/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2003 University of Southern California
 * Copyright © 2009,2010 Intel Corporation
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
 *	Chris Wilson <chris@chris-wilson.co.uk>
 */

#include "cairoint.h"

#include "cairo-boxes-private.h"
#include "cairo-clip-private.h"
#include "cairo-composite-rectangles-private.h"
#include "cairo-error-private.h"
#include "cairo-region-private.h"
#include "cairo-scaled-font-private.h"
#include "cairo-surface-snapshot-private.h"
#include "cairo-surface-subsurface-private.h"

/* Limit on the width / height of an image surface in pixels.  This is
 * mainly determined by coordinates of things sent to pixman at the
 * moment being in 16.16 format. */
#define MAX_IMAGE_SIZE 32767
#define PIXMAN_MAX_INT ((pixman_fixed_1 >> 1) - pixman_fixed_e) /* need to ensure deltas also fit */

/**
 * SECTION:cairo-image
 * @Title: Image Surfaces
 * @Short_Description: Rendering to memory buffers
 * @See_Also: #cairo_surface_t
 *
 * Image surfaces provide the ability to render to memory buffers
 * either allocated by cairo or by the calling code.  The supported
 * image formats are those defined in #cairo_format_t.
 */

/**
 * CAIRO_HAS_IMAGE_SURFACE:
 *
 * Defined if the image surface backend is available.
 * The image surface backend is always built in.
 * This macro was added for completeness in cairo 1.8.
 *
 * @Since: 1.8
 */

static cairo_int_status_t
_cairo_image_surface_fill (void *dst,
			   cairo_operator_t		 op,
			   const cairo_pattern_t	*source,
			   cairo_path_fixed_t	*path,
			   cairo_fill_rule_t		 fill_rule,
			   double			 tolerance,
			   cairo_antialias_t		 antialias,
			   cairo_clip_t		*clip);

static pixman_image_t *
_pixman_image_for_solid (const cairo_solid_pattern_t *pattern);

static cairo_bool_t
_cairo_image_surface_is_size_valid (int width, int height)
{
    return 0 <= width  &&  width <= MAX_IMAGE_SIZE &&
	   0 <= height && height <= MAX_IMAGE_SIZE;
}

cairo_format_t
_cairo_format_from_pixman_format (pixman_format_code_t pixman_format)
{
    switch (pixman_format) {
    case PIXMAN_a8r8g8b8:
	return CAIRO_FORMAT_ARGB32;
    case PIXMAN_x8r8g8b8:
	return CAIRO_FORMAT_RGB24;
    case PIXMAN_a8:
	return CAIRO_FORMAT_A8;
    case PIXMAN_a1:
	return CAIRO_FORMAT_A1;
    case PIXMAN_r5g6b5:
	return CAIRO_FORMAT_RGB16_565;
    case PIXMAN_a8b8g8r8: case PIXMAN_x8b8g8r8: case PIXMAN_r8g8b8:
    case PIXMAN_b8g8r8:   case PIXMAN_b5g6r5:
    case PIXMAN_a1r5g5b5: case PIXMAN_x1r5g5b5: case PIXMAN_a1b5g5r5:
    case PIXMAN_x1b5g5r5: case PIXMAN_a4r4g4b4: case PIXMAN_x4r4g4b4:
    case PIXMAN_a4b4g4r4: case PIXMAN_x4b4g4r4: case PIXMAN_r3g3b2:
    case PIXMAN_b2g3r3:   case PIXMAN_a2r2g2b2: case PIXMAN_a2b2g2r2:
    case PIXMAN_c8:       case PIXMAN_g8:       case PIXMAN_x4a4:
    case PIXMAN_a4:       case PIXMAN_r1g2b1:   case PIXMAN_b1g2r1:
    case PIXMAN_a1r1g1b1: case PIXMAN_a1b1g1r1: case PIXMAN_c4:
    case PIXMAN_g4:       case PIXMAN_g1:
    case PIXMAN_yuy2:     case PIXMAN_yv12:
    case PIXMAN_b8g8r8x8:
    case PIXMAN_b8g8r8a8:
    case PIXMAN_x2b10g10r10:
    case PIXMAN_a2b10g10r10:
    case PIXMAN_x2r10g10b10:
    case PIXMAN_a2r10g10b10:
    default:
	return CAIRO_FORMAT_INVALID;
    }

    return CAIRO_FORMAT_INVALID;
}

cairo_content_t
_cairo_content_from_pixman_format (pixman_format_code_t pixman_format)
{
    cairo_content_t content;

    content = 0;
    if (PIXMAN_FORMAT_RGB (pixman_format))
	content |= CAIRO_CONTENT_COLOR;
    if (PIXMAN_FORMAT_A (pixman_format))
	content |= CAIRO_CONTENT_ALPHA;

    return content;
}

cairo_surface_t *
_cairo_image_surface_create_for_pixman_image (pixman_image_t		*pixman_image,
					      pixman_format_code_t	 pixman_format)
{
    cairo_image_surface_t *surface;
    int width = pixman_image_get_width (pixman_image);
    int height = pixman_image_get_height (pixman_image);

    surface = malloc (sizeof (cairo_image_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_surface_init (&surface->base,
			 &_cairo_image_surface_backend,
			 NULL, /* device */
			 _cairo_content_from_pixman_format (pixman_format));

    surface->pixman_image = pixman_image;

    surface->pixman_format = pixman_format;
    surface->format = _cairo_format_from_pixman_format (pixman_format);
    surface->data = (uint8_t *) pixman_image_get_data (pixman_image);
    surface->owns_data = FALSE;
    surface->transparency = CAIRO_IMAGE_UNKNOWN;

    surface->width = width;
    surface->height = height;
    surface->stride = pixman_image_get_stride (pixman_image);
    surface->depth = pixman_image_get_depth (pixman_image);

    return &surface->base;
}

cairo_bool_t
_pixman_format_from_masks (cairo_format_masks_t *masks,
			   pixman_format_code_t *format_ret)
{
    pixman_format_code_t format;
    int format_type;
    int a, r, g, b;
    cairo_format_masks_t format_masks;

    a = _cairo_popcount (masks->alpha_mask);
    r = _cairo_popcount (masks->red_mask);
    g = _cairo_popcount (masks->green_mask);
    b = _cairo_popcount (masks->blue_mask);

    if (masks->red_mask) {
	if (masks->red_mask > masks->blue_mask)
	    format_type = PIXMAN_TYPE_ARGB;
	else
	    format_type = PIXMAN_TYPE_ABGR;
    } else if (masks->alpha_mask) {
	format_type = PIXMAN_TYPE_A;
    } else {
	return FALSE;
    }

    format = PIXMAN_FORMAT (masks->bpp, format_type, a, r, g, b);

    if (! pixman_format_supported_destination (format))
	return FALSE;

    /* Sanity check that we got out of PIXMAN_FORMAT exactly what we
     * expected. This avoid any problems from something bizarre like
     * alpha in the least-significant bits, or insane channel order,
     * or whatever. */
     if (!_pixman_format_to_masks (format, &format_masks) ||
         masks->bpp        != format_masks.bpp            ||
	 masks->red_mask   != format_masks.red_mask       ||
	 masks->green_mask != format_masks.green_mask     ||
	 masks->blue_mask  != format_masks.blue_mask)
     {
	 return FALSE;
     }

    *format_ret = format;
    return TRUE;
}

/* A mask consisting of N bits set to 1. */
#define MASK(N) ((1UL << (N))-1)

cairo_bool_t
_pixman_format_to_masks (pixman_format_code_t	 format,
			 cairo_format_masks_t	*masks)
{
    int a, r, g, b;

    masks->bpp = PIXMAN_FORMAT_BPP (format);

    /* Number of bits in each channel */
    a = PIXMAN_FORMAT_A (format);
    r = PIXMAN_FORMAT_R (format);
    g = PIXMAN_FORMAT_G (format);
    b = PIXMAN_FORMAT_B (format);

    switch (PIXMAN_FORMAT_TYPE (format)) {
    case PIXMAN_TYPE_ARGB:
        masks->alpha_mask = MASK (a) << (r + g + b);
        masks->red_mask   = MASK (r) << (g + b);
        masks->green_mask = MASK (g) << (b);
        masks->blue_mask  = MASK (b);
        return TRUE;
    case PIXMAN_TYPE_ABGR:
        masks->alpha_mask = MASK (a) << (b + g + r);
        masks->blue_mask  = MASK (b) << (g + r);
        masks->green_mask = MASK (g) << (r);
        masks->red_mask   = MASK (r);
        return TRUE;
#ifdef PIXMAN_TYPE_BGRA
    case PIXMAN_TYPE_BGRA:
        masks->blue_mask  = MASK (b) << (masks->bpp - b);
        masks->green_mask = MASK (g) << (masks->bpp - b - g);
        masks->red_mask   = MASK (r) << (masks->bpp - b - g - r);
        masks->alpha_mask = MASK (a);
        return TRUE;
#endif
    case PIXMAN_TYPE_A:
        masks->alpha_mask = MASK (a);
        masks->red_mask   = 0;
        masks->green_mask = 0;
        masks->blue_mask  = 0;
        return TRUE;
    case PIXMAN_TYPE_OTHER:
    case PIXMAN_TYPE_COLOR:
    case PIXMAN_TYPE_GRAY:
    case PIXMAN_TYPE_YUY2:
    case PIXMAN_TYPE_YV12:
    default:
        masks->alpha_mask = 0;
        masks->red_mask   = 0;
        masks->green_mask = 0;
        masks->blue_mask  = 0;
        return FALSE;
    }
}

pixman_format_code_t
_cairo_format_to_pixman_format_code (cairo_format_t format)
{
    pixman_format_code_t ret;
    switch (format) {
    case CAIRO_FORMAT_A1:
	ret = PIXMAN_a1;
	break;
    case CAIRO_FORMAT_A8:
	ret = PIXMAN_a8;
	break;
    case CAIRO_FORMAT_RGB24:
	ret = PIXMAN_x8r8g8b8;
	break;
    case CAIRO_FORMAT_RGB16_565:
	ret = PIXMAN_r5g6b5;
	break;
    case CAIRO_FORMAT_ARGB32:
    case CAIRO_FORMAT_INVALID:
    default:
	ret = PIXMAN_a8r8g8b8;
	break;
    }
    return ret;
}

cairo_surface_t *
_cairo_image_surface_create_with_pixman_format (unsigned char		*data,
						pixman_format_code_t	 pixman_format,
						int			 width,
						int			 height,
						int			 stride)
{
    cairo_surface_t *surface;
    pixman_image_t *pixman_image;

    if (! _cairo_image_surface_is_size_valid (width, height))
    {
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_SIZE));
    }

    pixman_image = pixman_image_create_bits (pixman_format, width, height,
					     (uint32_t *) data, stride ? stride : 4);

    if (unlikely (pixman_image == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    surface = _cairo_image_surface_create_for_pixman_image (pixman_image,
							    pixman_format);
    if (unlikely (surface->status)) {
	pixman_image_unref (pixman_image);
	return surface;
    }

    /* we can not make any assumptions about the initial state of user data */
    surface->is_clear = data == NULL;
    return surface;
}

/**
 * cairo_image_surface_create:
 * @format: format of pixels in the surface to create
 * @width: width of the surface, in pixels
 * @height: height of the surface, in pixels
 *
 * Creates an image surface of the specified format and
 * dimensions. Initially the surface contents are all
 * 0. (Specifically, within each pixel, each color or alpha channel
 * belonging to format will be 0. The contents of bits within a pixel,
 * but not belonging to the given format are undefined).
 *
 * Return value: a pointer to the newly created surface. The caller
 * owns the surface and should call cairo_surface_destroy() when done
 * with it.
 *
 * This function always returns a valid pointer, but it will return a
 * pointer to a "nil" surface if an error such as out of memory
 * occurs. You can use cairo_surface_status() to check for this.
 **/
cairo_surface_t *
cairo_image_surface_create (cairo_format_t	format,
			    int			width,
			    int			height)
{
    pixman_format_code_t pixman_format;

    if (! CAIRO_FORMAT_VALID (format))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));

    pixman_format = _cairo_format_to_pixman_format_code (format);

    return _cairo_image_surface_create_with_pixman_format (NULL, pixman_format,
							   width, height, -1);
}
slim_hidden_def (cairo_image_surface_create);

cairo_surface_t *
_cairo_image_surface_create_with_content (cairo_content_t	content,
					  int			width,
					  int			height)
{
    return cairo_image_surface_create (_cairo_format_from_content (content),
				       width, height);
}

/**
 * cairo_format_stride_for_width:
 * @format: A #cairo_format_t value
 * @width: The desired width of an image surface to be created.
 *
 * This function provides a stride value that will respect all
 * alignment requirements of the accelerated image-rendering code
 * within cairo. Typical usage will be of the form:
 *
 * <informalexample><programlisting>
 * int stride;
 * unsigned char *data;
 * #cairo_surface_t *surface;
 *
 * stride = cairo_format_stride_for_width (format, width);
 * data = malloc (stride * height);
 * surface = cairo_image_surface_create_for_data (data, format,
 *						  width, height,
 *						  stride);
 * </programlisting></informalexample>
 *
 * Return value: the appropriate stride to use given the desired
 * format and width, or -1 if either the format is invalid or the width
 * too large.
 *
 * Since: 1.6
 **/
int
cairo_format_stride_for_width (cairo_format_t	format,
			       int		width)
{
    int bpp;

    if (! CAIRO_FORMAT_VALID (format)) {
	_cairo_error_throw (CAIRO_STATUS_INVALID_FORMAT);
	return -1;
    }

    bpp = _cairo_format_bits_per_pixel (format);
    if ((unsigned) (width) >= (INT32_MAX - 7) / (unsigned) (bpp))
	return -1;

    return CAIRO_STRIDE_FOR_WIDTH_BPP (width, bpp);
}
slim_hidden_def (cairo_format_stride_for_width);

/**
 * cairo_image_surface_create_for_data:
 * @data: a pointer to a buffer supplied by the application in which
 *     to write contents. This pointer must be suitably aligned for any
 *     kind of variable, (for example, a pointer returned by malloc).
 * @format: the format of pixels in the buffer
 * @width: the width of the image to be stored in the buffer
 * @height: the height of the image to be stored in the buffer
 * @stride: the number of bytes between the start of rows in the
 *     buffer as allocated. This value should always be computed by
 *     cairo_format_stride_for_width() before allocating the data
 *     buffer.
 *
 * Creates an image surface for the provided pixel data. The output
 * buffer must be kept around until the #cairo_surface_t is destroyed
 * or cairo_surface_finish() is called on the surface.  The initial
 * contents of @data will be used as the initial image contents; you
 * must explicitly clear the buffer, using, for example,
 * cairo_rectangle() and cairo_fill() if you want it cleared.
 *
 * Note that the stride may be larger than
 * width*bytes_per_pixel to provide proper alignment for each pixel
 * and row. This alignment is required to allow high-performance rendering
 * within cairo. The correct way to obtain a legal stride value is to
 * call cairo_format_stride_for_width() with the desired format and
 * maximum image width value, and then use the resulting stride value
 * to allocate the data and to create the image surface. See
 * cairo_format_stride_for_width() for example code.
 *
 * Return value: a pointer to the newly created surface. The caller
 * owns the surface and should call cairo_surface_destroy() when done
 * with it.
 *
 * This function always returns a valid pointer, but it will return a
 * pointer to a "nil" surface in the case of an error such as out of
 * memory or an invalid stride value. In case of invalid stride value
 * the error status of the returned surface will be
 * %CAIRO_STATUS_INVALID_STRIDE.  You can use
 * cairo_surface_status() to check for this.
 *
 * See cairo_surface_set_user_data() for a means of attaching a
 * destroy-notification fallback to the surface if necessary.
 **/
cairo_surface_t *
cairo_image_surface_create_for_data (unsigned char     *data,
				     cairo_format_t	format,
				     int		width,
				     int		height,
				     int		stride)
{
    pixman_format_code_t pixman_format;
    int minstride;

    if (! CAIRO_FORMAT_VALID (format))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));

    if ((stride & (CAIRO_STRIDE_ALIGNMENT-1)) != 0)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_STRIDE));

    if (! _cairo_image_surface_is_size_valid (width, height))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_SIZE));

    minstride = cairo_format_stride_for_width (format, width);
    if (stride < 0) {
	if (stride > -minstride) {
	    return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_STRIDE));
	}
    } else {
	if (stride < minstride) {
	    return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_STRIDE));
	}
    }

    pixman_format = _cairo_format_to_pixman_format_code (format);
    return _cairo_image_surface_create_with_pixman_format (data,
							   pixman_format,
							   width, height,
							   stride);
}
slim_hidden_def (cairo_image_surface_create_for_data);

/**
 * cairo_image_surface_get_data:
 * @surface: a #cairo_image_surface_t
 *
 * Get a pointer to the data of the image surface, for direct
 * inspection or modification.
 *
 * Return value: a pointer to the image data of this surface or %NULL
 * if @surface is not an image surface, or if cairo_surface_finish()
 * has been called.
 *
 * Since: 1.2
 **/
unsigned char *
cairo_image_surface_get_data (cairo_surface_t *surface)
{
    cairo_image_surface_t *image_surface = (cairo_image_surface_t *) surface;

    if (! _cairo_surface_is_image (surface)) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return NULL;
    }

    return image_surface->data;
}
slim_hidden_def (cairo_image_surface_get_data);

/**
 * cairo_image_surface_get_format:
 * @surface: a #cairo_image_surface_t
 *
 * Get the format of the surface.
 *
 * Return value: the format of the surface
 *
 * Since: 1.2
 **/
cairo_format_t
cairo_image_surface_get_format (cairo_surface_t *surface)
{
    cairo_image_surface_t *image_surface = (cairo_image_surface_t *) surface;

    if (! _cairo_surface_is_image (surface)) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return CAIRO_FORMAT_INVALID;
    }

    return image_surface->format;
}
slim_hidden_def (cairo_image_surface_get_format);

/**
 * cairo_image_surface_get_width:
 * @surface: a #cairo_image_surface_t
 *
 * Get the width of the image surface in pixels.
 *
 * Return value: the width of the surface in pixels.
 **/
int
cairo_image_surface_get_width (cairo_surface_t *surface)
{
    cairo_image_surface_t *image_surface = (cairo_image_surface_t *) surface;

    if (! _cairo_surface_is_image (surface)) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return 0;
    }

    return image_surface->width;
}
slim_hidden_def (cairo_image_surface_get_width);

/**
 * cairo_image_surface_get_height:
 * @surface: a #cairo_image_surface_t
 *
 * Get the height of the image surface in pixels.
 *
 * Return value: the height of the surface in pixels.
 **/
int
cairo_image_surface_get_height (cairo_surface_t *surface)
{
    cairo_image_surface_t *image_surface = (cairo_image_surface_t *) surface;

    if (! _cairo_surface_is_image (surface)) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return 0;
    }

    return image_surface->height;
}
slim_hidden_def (cairo_image_surface_get_height);

/**
 * cairo_image_surface_get_stride:
 * @surface: a #cairo_image_surface_t
 *
 * Get the stride of the image surface in bytes
 *
 * Return value: the stride of the image surface in bytes (or 0 if
 * @surface is not an image surface). The stride is the distance in
 * bytes from the beginning of one row of the image data to the
 * beginning of the next row.
 *
 * Since: 1.2
 **/
int
cairo_image_surface_get_stride (cairo_surface_t *surface)
{

    cairo_image_surface_t *image_surface = (cairo_image_surface_t *) surface;

    if (! _cairo_surface_is_image (surface)) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return 0;
    }

    return image_surface->stride;
}
slim_hidden_def (cairo_image_surface_get_stride);

cairo_format_t
_cairo_format_from_content (cairo_content_t content)
{
    switch (content) {
    case CAIRO_CONTENT_COLOR:
	return CAIRO_FORMAT_RGB24;
    case CAIRO_CONTENT_ALPHA:
	return CAIRO_FORMAT_A8;
    case CAIRO_CONTENT_COLOR_ALPHA:
	return CAIRO_FORMAT_ARGB32;
    }

    ASSERT_NOT_REACHED;
    return CAIRO_FORMAT_INVALID;
}

cairo_content_t
_cairo_content_from_format (cairo_format_t format)
{
    switch (format) {
    case CAIRO_FORMAT_ARGB32:
	return CAIRO_CONTENT_COLOR_ALPHA;
    case CAIRO_FORMAT_RGB24:
	return CAIRO_CONTENT_COLOR;
    case CAIRO_FORMAT_RGB16_565:
	return CAIRO_CONTENT_COLOR;
    case CAIRO_FORMAT_A8:
    case CAIRO_FORMAT_A1:
	return CAIRO_CONTENT_ALPHA;
    case CAIRO_FORMAT_INVALID:
	break;
    }

    ASSERT_NOT_REACHED;
    return CAIRO_CONTENT_COLOR_ALPHA;
}

int
_cairo_format_bits_per_pixel (cairo_format_t format)
{
    switch (format) {
    case CAIRO_FORMAT_ARGB32:
	return 32;
    case CAIRO_FORMAT_RGB24:
	return 32;
    case CAIRO_FORMAT_RGB16_565:
	return 16;
    case CAIRO_FORMAT_A8:
	return 8;
    case CAIRO_FORMAT_A1:
	return 1;
    case CAIRO_FORMAT_INVALID:
    default:
	ASSERT_NOT_REACHED;
	return 0;
    }
}

static cairo_surface_t *
_cairo_image_surface_create_similar (void	       *abstract_other,
				     cairo_content_t	content,
				     int		width,
				     int		height)
{
    cairo_image_surface_t *other = abstract_other;

    if (! _cairo_image_surface_is_size_valid (width, height))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_SIZE));

    if (content == other->base.content) {
	return _cairo_image_surface_create_with_pixman_format (NULL,
							       other->pixman_format,
							       width, height,
							       0);
    }

    return _cairo_image_surface_create_with_content (content,
						     width, height);
}

static cairo_status_t
_cairo_image_surface_finish (void *abstract_surface)
{
    cairo_image_surface_t *surface = abstract_surface;

    if (surface->pixman_image) {
	pixman_image_unref (surface->pixman_image);
	surface->pixman_image = NULL;
    }

    if (surface->owns_data) {
	free (surface->data);
	surface->data = NULL;
    }

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_image_surface_assume_ownership_of_data (cairo_image_surface_t *surface)
{
    surface->owns_data = TRUE;
}

static cairo_status_t
_cairo_image_surface_acquire_source_image (void                    *abstract_surface,
					   cairo_image_surface_t  **image_out,
					   void                   **image_extra)
{
    *image_out = abstract_surface;
    *image_extra = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_image_surface_release_source_image (void                   *abstract_surface,
					   cairo_image_surface_t  *image,
					   void                   *image_extra)
{
}

/* XXX: I think we should fix pixman to match the names/order of the
 * cairo operators, but that will likely be better done at the same
 * time the X server is ported to pixman, (which will change a lot of
 * things in pixman I think).
 */
static pixman_op_t
_pixman_operator (cairo_operator_t op)
{
    switch (op) {
    case CAIRO_OPERATOR_CLEAR:
	return PIXMAN_OP_CLEAR;

    case CAIRO_OPERATOR_SOURCE:
	return PIXMAN_OP_SRC;
    case CAIRO_OPERATOR_OVER:
	return PIXMAN_OP_OVER;
    case CAIRO_OPERATOR_IN:
	return PIXMAN_OP_IN;
    case CAIRO_OPERATOR_OUT:
	return PIXMAN_OP_OUT;
    case CAIRO_OPERATOR_ATOP:
	return PIXMAN_OP_ATOP;

    case CAIRO_OPERATOR_DEST:
	return PIXMAN_OP_DST;
    case CAIRO_OPERATOR_DEST_OVER:
	return PIXMAN_OP_OVER_REVERSE;
    case CAIRO_OPERATOR_DEST_IN:
	return PIXMAN_OP_IN_REVERSE;
    case CAIRO_OPERATOR_DEST_OUT:
	return PIXMAN_OP_OUT_REVERSE;
    case CAIRO_OPERATOR_DEST_ATOP:
	return PIXMAN_OP_ATOP_REVERSE;

    case CAIRO_OPERATOR_XOR:
	return PIXMAN_OP_XOR;
    case CAIRO_OPERATOR_ADD:
	return PIXMAN_OP_ADD;
    case CAIRO_OPERATOR_SATURATE:
	return PIXMAN_OP_SATURATE;

    case CAIRO_OPERATOR_MULTIPLY:
	return PIXMAN_OP_MULTIPLY;
    case CAIRO_OPERATOR_SCREEN:
	return PIXMAN_OP_SCREEN;
    case CAIRO_OPERATOR_OVERLAY:
	return PIXMAN_OP_OVERLAY;
    case CAIRO_OPERATOR_DARKEN:
	return PIXMAN_OP_DARKEN;
    case CAIRO_OPERATOR_LIGHTEN:
	return PIXMAN_OP_LIGHTEN;
    case CAIRO_OPERATOR_COLOR_DODGE:
	return PIXMAN_OP_COLOR_DODGE;
    case CAIRO_OPERATOR_COLOR_BURN:
	return PIXMAN_OP_COLOR_BURN;
    case CAIRO_OPERATOR_HARD_LIGHT:
	return PIXMAN_OP_HARD_LIGHT;
    case CAIRO_OPERATOR_SOFT_LIGHT:
	return PIXMAN_OP_SOFT_LIGHT;
    case CAIRO_OPERATOR_DIFFERENCE:
	return PIXMAN_OP_DIFFERENCE;
    case CAIRO_OPERATOR_EXCLUSION:
	return PIXMAN_OP_EXCLUSION;
    case CAIRO_OPERATOR_HSL_HUE:
	return PIXMAN_OP_HSL_HUE;
    case CAIRO_OPERATOR_HSL_SATURATION:
	return PIXMAN_OP_HSL_SATURATION;
    case CAIRO_OPERATOR_HSL_COLOR:
	return PIXMAN_OP_HSL_COLOR;
    case CAIRO_OPERATOR_HSL_LUMINOSITY:
	return PIXMAN_OP_HSL_LUMINOSITY;

    default:
	ASSERT_NOT_REACHED;
	return PIXMAN_OP_OVER;
    }
}

static cairo_status_t
_cairo_image_surface_set_clip_region (cairo_image_surface_t *surface,
				      cairo_region_t *region)
{
    if (! pixman_image_set_clip_region32 (surface->pixman_image, &region->rgn))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_image_surface_unset_clip_region (cairo_image_surface_t *surface)
{
    pixman_image_set_clip_region32 (surface->pixman_image, NULL);
}

static double
_pixman_nearest_sample (double d)
{
    return ceil (d - .5);
}

static cairo_bool_t
_nearest_sample (cairo_filter_t filter, double *tx, double *ty)
{
    if (filter == CAIRO_FILTER_FAST || filter == CAIRO_FILTER_NEAREST) {
	*tx = _pixman_nearest_sample (*tx);
	*ty = _pixman_nearest_sample (*ty);
    } else {
	if (*tx != floor (*tx) || *ty != floor (*ty))
	    return FALSE;
    }
    return fabs (*tx) < PIXMAN_MAX_INT && fabs (*ty) < PIXMAN_MAX_INT;
}

#if HAS_ATOMIC_OPS
static pixman_image_t *__pixman_transparent_image;
static pixman_image_t *__pixman_black_image;
static pixman_image_t *__pixman_white_image;

static pixman_image_t *
_pixman_transparent_image (void)
{
    pixman_image_t *image;

    image = __pixman_transparent_image;
    if (unlikely (image == NULL)) {
	pixman_color_t color;

	color.red   = 0x00;
	color.green = 0x00;
	color.blue  = 0x00;
	color.alpha = 0x00;

	image = pixman_image_create_solid_fill (&color);
	if (unlikely (image == NULL))
	    return NULL;

	if (_cairo_atomic_ptr_cmpxchg (&__pixman_transparent_image,
				       NULL, image))
	{
	    pixman_image_ref (image);
	}
    } else {
	pixman_image_ref (image);
    }

    return image;
}

static pixman_image_t *
_pixman_black_image (void)
{
    pixman_image_t *image;

    image = __pixman_black_image;
    if (unlikely (image == NULL)) {
	pixman_color_t color;

	color.red   = 0x00;
	color.green = 0x00;
	color.blue  = 0x00;
	color.alpha = 0xffff;

	image = pixman_image_create_solid_fill (&color);
	if (unlikely (image == NULL))
	    return NULL;

	if (_cairo_atomic_ptr_cmpxchg (&__pixman_black_image,
				       NULL, image))
	{
	    pixman_image_ref (image);
	}
    } else {
	pixman_image_ref (image);
    }

    return image;
}

static pixman_image_t *
_pixman_white_image (void)
{
    pixman_image_t *image;

    image = __pixman_white_image;
    if (unlikely (image == NULL)) {
	pixman_color_t color;

	color.red   = 0xffff;
	color.green = 0xffff;
	color.blue  = 0xffff;
	color.alpha = 0xffff;

	image = pixman_image_create_solid_fill (&color);
	if (unlikely (image == NULL))
	    return NULL;

	if (_cairo_atomic_ptr_cmpxchg (&__pixman_white_image,
				       NULL, image))
	{
	    pixman_image_ref (image);
	}
    } else {
	pixman_image_ref (image);
    }

    return image;
}
#else
static pixman_image_t *
_pixman_transparent_image (void)
{
    return _pixman_image_for_solid (&_cairo_pattern_clear);
}
static pixman_image_t *
_pixman_black_image (void)
{
    return _pixman_image_for_solid (&_cairo_pattern_black);
}
static pixman_image_t *
_pixman_white_image (void)
{
    return _pixman_image_for_solid (&_cairo_pattern_white);
}
#endif

static uint32_t
hars_petruska_f54_1_random (void)
{
#define rol(x,k) ((x << k) | (x >> (32-k)))
    static uint32_t x;
    return x = (x ^ rol (x, 5) ^ rol (x, 24)) + 0x37798849;
#undef rol
}

static struct {
    cairo_color_t color;
    pixman_image_t *image;
} cache[16];
static int n_cached;

void
_cairo_image_reset_static_data (void)
{
    while (n_cached)
	pixman_image_unref (cache[--n_cached].image);

#if HAS_ATOMIC_OPS
    if (__pixman_transparent_image) {
	pixman_image_unref (__pixman_transparent_image);
	__pixman_transparent_image = NULL;
    }

    if (__pixman_black_image) {
	pixman_image_unref (__pixman_black_image);
	__pixman_black_image = NULL;
    }

    if (__pixman_white_image) {
	pixman_image_unref (__pixman_white_image);
	__pixman_white_image = NULL;
    }
#endif
}

static pixman_image_t *
_pixman_image_for_solid (const cairo_solid_pattern_t *pattern)
{
    pixman_color_t color;
    pixman_image_t *image;
    int i;

#if HAS_ATOMIC_OPS
    if (pattern->color.alpha_short <= 0x00ff)
	return _pixman_transparent_image ();

    if (pattern->color.alpha_short >= 0xff00) {
	if (pattern->color.red_short <= 0x00ff &&
	    pattern->color.green_short <= 0x00ff &&
	    pattern->color.blue_short <= 0x00ff)
	{
	    return _pixman_black_image ();
	}

	if (pattern->color.red_short >= 0xff00 &&
	    pattern->color.green_short >= 0xff00 &&
	    pattern->color.blue_short >= 0xff00)
	{
	    return _pixman_white_image ();
	}
    }
#endif

    CAIRO_MUTEX_LOCK (_cairo_image_solid_cache_mutex);
    for (i = 0; i < n_cached; i++) {
	if (_cairo_color_equal (&cache[i].color, &pattern->color)) {
	    image = pixman_image_ref (cache[i].image);
	    goto UNLOCK;
	}
    }

    color.red   = pattern->color.red_short;
    color.green = pattern->color.green_short;
    color.blue  = pattern->color.blue_short;
    color.alpha = pattern->color.alpha_short;

    image = pixman_image_create_solid_fill (&color);
    if (image == NULL)
	goto UNLOCK;

    if (n_cached < ARRAY_LENGTH (cache)) {
	i = n_cached++;
    } else {
	i = hars_petruska_f54_1_random () % ARRAY_LENGTH (cache);
	pixman_image_unref (cache[i].image);
    }
    cache[i].image = pixman_image_ref (image);
    cache[i].color = pattern->color;

UNLOCK:
    CAIRO_MUTEX_UNLOCK (_cairo_image_solid_cache_mutex);
    return image;
}

static pixman_image_t *
_pixman_image_for_gradient (const cairo_gradient_pattern_t *pattern,
			    const cairo_rectangle_int_t *extents,
			    int *ix, int *iy)
{
    pixman_image_t	  *pixman_image;
    pixman_gradient_stop_t pixman_stops_static[2];
    pixman_gradient_stop_t *pixman_stops = pixman_stops_static;
    cairo_matrix_t matrix = pattern->base.matrix;
    double tx, ty;
    unsigned int i;

    if (pattern->n_stops > ARRAY_LENGTH(pixman_stops_static)) {
	pixman_stops = _cairo_malloc_ab (pattern->n_stops,
					 sizeof(pixman_gradient_stop_t));
	if (unlikely (pixman_stops == NULL))
	    return NULL;
    }

    for (i = 0; i < pattern->n_stops; i++) {
	pixman_stops[i].x = _cairo_fixed_16_16_from_double (pattern->stops[i].offset);
	pixman_stops[i].color.red   = pattern->stops[i].color.red_short;
	pixman_stops[i].color.green = pattern->stops[i].color.green_short;
	pixman_stops[i].color.blue  = pattern->stops[i].color.blue_short;
	pixman_stops[i].color.alpha = pattern->stops[i].color.alpha_short;
    }

    if (pattern->base.type == CAIRO_PATTERN_TYPE_LINEAR) {
	cairo_linear_pattern_t *linear = (cairo_linear_pattern_t *) pattern;
	pixman_point_fixed_t p1, p2;
	cairo_fixed_t xdim, ydim;

	xdim = fabs (linear->p2.x - linear->p1.x);
	ydim = fabs (linear->p2.y - linear->p1.y);

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
	if (_cairo_fixed_integer_ceil (xdim) > PIXMAN_MAX_INT ||
	    _cairo_fixed_integer_ceil (ydim) > PIXMAN_MAX_INT)
	{
	    double sf;
	    cairo_matrix_t scale;

	    if (xdim > ydim)
		sf = PIXMAN_MAX_INT / _cairo_fixed_to_double (xdim);
	    else
		sf = PIXMAN_MAX_INT / _cairo_fixed_to_double (ydim);

	    p1.x = _cairo_fixed_16_16_from_double (_cairo_fixed_to_double (linear->p1.x) * sf);
	    p1.y = _cairo_fixed_16_16_from_double (_cairo_fixed_to_double (linear->p1.y) * sf);
	    p2.x = _cairo_fixed_16_16_from_double (_cairo_fixed_to_double (linear->p2.x) * sf);
	    p2.y = _cairo_fixed_16_16_from_double (_cairo_fixed_to_double (linear->p2.y) * sf);

	    cairo_matrix_init_scale (&scale, sf, sf);
	    cairo_matrix_multiply (&matrix, &matrix, &scale);
	}
	else
	{
	    p1.x = _cairo_fixed_to_16_16 (linear->p1.x);
	    p1.y = _cairo_fixed_to_16_16 (linear->p1.y);
	    p2.x = _cairo_fixed_to_16_16 (linear->p2.x);
	    p2.y = _cairo_fixed_to_16_16 (linear->p2.y);
	}

	pixman_image = pixman_image_create_linear_gradient (&p1, &p2,
							    pixman_stops,
							    pattern->n_stops);
    } else {
	cairo_radial_pattern_t *radial = (cairo_radial_pattern_t *) pattern;
	pixman_point_fixed_t c1, c2;
	pixman_fixed_t r1, r2;

	c1.x = _cairo_fixed_to_16_16 (radial->c1.x);
	c1.y = _cairo_fixed_to_16_16 (radial->c1.y);
	r1   = _cairo_fixed_to_16_16 (radial->r1);

	c2.x = _cairo_fixed_to_16_16 (radial->c2.x);
	c2.y = _cairo_fixed_to_16_16 (radial->c2.y);
	r2   = _cairo_fixed_to_16_16 (radial->r2);

	pixman_image = pixman_image_create_radial_gradient (&c1, &c2, r1, r2,
							    pixman_stops,
							    pattern->n_stops);
    }

    if (pixman_stops != pixman_stops_static)
	free (pixman_stops);

    if (unlikely (pixman_image == NULL))
	return NULL;

    tx = matrix.x0;
    ty = matrix.y0;
    if (! _cairo_matrix_is_translation (&matrix) ||
	! _nearest_sample (pattern->base.filter, &tx, &ty))
    {
	pixman_transform_t pixman_transform;

	if (tx != 0. || ty != 0.) {
	    cairo_matrix_t m, inv;
	    cairo_status_t status;
	    double x, y, max_x, max_y;

	    /* Pixman also limits the [xy]_offset to 16 bits. We try to evenly
	     * spread the bits between the two, but we need to ensure that
	     * fabs (tx + extents->x + extents->width) < PIXMAN_MAX_INT &&
	     * fabs (ty + extents->y + extents->height) < PIXMAN_MAX_INT,
	     * otherwise the gradient won't render.
	     */
	    inv = matrix;
	    status = cairo_matrix_invert (&inv);
	    assert (status == CAIRO_STATUS_SUCCESS);

	    x = _cairo_lround (inv.x0 / 2);
	    y = _cairo_lround (inv.y0 / 2);

	    max_x = PIXMAN_MAX_INT - 1 - fabs (extents->x + extents->width);
	    x = x > max_x ? max_x : (x < -max_x ? -max_x : x);
	    max_y = PIXMAN_MAX_INT - 1 - fabs (extents->y + extents->height);
	    y = y > max_y ? max_y : (y < -max_y ? -max_y : y);

	    tx = -x;
	    ty = -y;
	    cairo_matrix_init_translate (&inv, x, y);
	    cairo_matrix_multiply (&m, &inv, &matrix);
	    _cairo_matrix_to_pixman_matrix (&m, &pixman_transform,
					    extents->x + extents->width/2.,
					    extents->y + extents->height/2.);
	} else {
	    tx = ty = 0;
	    _cairo_matrix_to_pixman_matrix (&pattern->base.matrix,
					    &pixman_transform,
					    extents->x + extents->width/2.,
					    extents->y + extents->height/2.);
	}

	if (! pixman_image_set_transform (pixman_image, &pixman_transform)) {
	    pixman_image_unref (pixman_image);
	    return NULL;
	}
    }
    *ix = tx;
    *iy = ty;

    {
	pixman_repeat_t pixman_repeat;

	switch (pattern->base.extend) {
	default:
	case CAIRO_EXTEND_NONE:
	    pixman_repeat = PIXMAN_REPEAT_NONE;
	    break;
	case CAIRO_EXTEND_REPEAT:
	    pixman_repeat = PIXMAN_REPEAT_NORMAL;
	    break;
	case CAIRO_EXTEND_REFLECT:
	    pixman_repeat = PIXMAN_REPEAT_REFLECT;
	    break;
	case CAIRO_EXTEND_PAD:
	    pixman_repeat = PIXMAN_REPEAT_PAD;
	    break;
	}

	pixman_image_set_repeat (pixman_image, pixman_repeat);
    }

    return pixman_image;
}

struct acquire_source_cleanup {
    cairo_surface_t *surface;
    cairo_image_surface_t *image;
    void *image_extra;
};

static void
_acquire_source_cleanup (pixman_image_t *pixman_image,
			 void *closure)
{
    struct acquire_source_cleanup *data = closure;

    _cairo_surface_release_source_image (data->surface,
					 data->image,
					 data->image_extra);
    free (data);
}

static cairo_filter_t
sampled_area (const cairo_surface_pattern_t *pattern,
	      const cairo_rectangle_int_t *extents,
	      cairo_rectangle_int_t *sample)
{
    cairo_filter_t filter;
    double x1, x2, y1, y2;
    double pad;

    x1 = extents->x;
    y1 = extents->y;
    x2 = extents->x + (int) extents->width;
    y2 = extents->y + (int) extents->height;

    _cairo_matrix_transform_bounding_box (&pattern->base.matrix,
                                          &x1, &y1, &x2, &y2,
                                          NULL);

    filter = _cairo_pattern_analyze_filter (&pattern->base, &pad);
    sample->x = floor (x1 - pad);
    sample->y = floor (y1 - pad);
    sample->width  = ceil (x2 + pad) - sample->x;
    sample->height = ceil (y2 + pad) - sample->y;

    return filter;
}

static uint16_t
expand_channel (uint16_t v, uint32_t bits)
{
    int offset = 16 - bits;
    while (offset > 0) {
	v |= v >> bits;
	offset -= bits;
	bits += bits;
    }
    return v;
}

static pixman_image_t *
_pixel_to_solid (cairo_image_surface_t *image, int x, int y)
{
    uint32_t pixel;
    pixman_color_t color;

    switch (image->format) {
    default:
    case CAIRO_FORMAT_INVALID:
	ASSERT_NOT_REACHED;
	return NULL;

    case CAIRO_FORMAT_A1:
	pixel = *(uint8_t *) (image->data + y * image->stride + x/8);
	return pixel & (1 << (x&7)) ? _pixman_white_image () : _pixman_transparent_image ();

    case CAIRO_FORMAT_A8:
	color.alpha = *(uint8_t *) (image->data + y * image->stride + x);
	color.alpha |= color.alpha << 8;
	if (color.alpha == 0)
	    return _pixman_transparent_image ();

	color.red = color.green = color.blue = 0;
	return pixman_image_create_solid_fill (&color);

    case CAIRO_FORMAT_RGB16_565:
	pixel = *(uint16_t *) (image->data + y * image->stride + 2 * x);
	if (pixel == 0)
	    return _pixman_black_image ();
	if (pixel == 0xffff)
	    return _pixman_white_image ();

	color.alpha = 0xffff;
	color.red = expand_channel ((pixel >> 11 & 0x1f) << 11, 5);
	color.green = expand_channel ((pixel >> 5 & 0x3f) << 10, 6);
	color.blue = expand_channel ((pixel & 0x1f) << 11, 5);
	return pixman_image_create_solid_fill (&color);

    case CAIRO_FORMAT_ARGB32:
    case CAIRO_FORMAT_RGB24:
	pixel = *(uint32_t *) (image->data + y * image->stride + 4 * x);
	color.alpha = image->format == CAIRO_FORMAT_ARGB32 ? (pixel >> 24) | (pixel >> 16 & 0xff00) : 0xffff;
	if (color.alpha == 0)
	    return _pixman_transparent_image ();
	if (pixel == 0xffffffff)
	    return _pixman_white_image ();
	if (color.alpha == 0xffff && (pixel & 0xffffff) == 0)
	    return _pixman_black_image ();

	color.red = (pixel >> 16 & 0xff) | (pixel >> 8 & 0xff00);
	color.green = (pixel >> 8 & 0xff) | (pixel & 0xff00);
	color.blue = (pixel & 0xff) | (pixel << 8 & 0xff00);
	return pixman_image_create_solid_fill (&color);
    }
}

static pixman_image_t *
_pixman_image_for_surface (const cairo_surface_pattern_t *pattern,
			   cairo_bool_t is_mask,
			   const cairo_rectangle_int_t *extents,
			   int *ix, int *iy)
{
    pixman_image_t *pixman_image;
    cairo_rectangle_int_t sample;
    cairo_extend_t extend;
    cairo_filter_t filter;
    double tx, ty;

    tx = pattern->base.matrix.x0;
    ty = pattern->base.matrix.y0;

    extend = pattern->base.extend;
    filter = sampled_area (pattern, extents, &sample);

    pixman_image = NULL;
    if (pattern->surface->type == CAIRO_SURFACE_TYPE_IMAGE &&
	(! is_mask || ! pattern->base.has_component_alpha ||
	 (pattern->surface->content & CAIRO_CONTENT_COLOR) == 0))
    {
	cairo_image_surface_t *source = (cairo_image_surface_t *) pattern->surface;
	cairo_surface_type_t type;

	if (source->base.backend->type == CAIRO_INTERNAL_SURFACE_TYPE_SNAPSHOT)
	    source = (cairo_image_surface_t *) ((cairo_surface_snapshot_t *) pattern->surface)->target;

	type = source->base.backend->type;
	if (type == CAIRO_SURFACE_TYPE_IMAGE) {
	    if (sample.width == 1 && sample.height == 1) {
		if (sample.x < 0 ||
		    sample.y < 0 ||
		    sample.x >= source->width ||
		    sample.y >= source->height)
		{
		    if (extend == CAIRO_EXTEND_NONE)
			return _pixman_transparent_image ();
		}
		else
		{
		    return _pixel_to_solid (source, sample.x, sample.y);
		}
	    }

	    /* avoid allocating a 'pattern' image if we can reuse the original */
	    if (extend == CAIRO_EXTEND_NONE &&
		_cairo_matrix_is_translation (&pattern->base.matrix) &&
		_nearest_sample (filter, &tx, &ty))
	    {
		*ix = tx;
		*iy = ty;
		return pixman_image_ref (source->pixman_image);
	    }

	    pixman_image = pixman_image_create_bits (source->pixman_format,
						     source->width,
						     source->height,
						     (uint32_t *) source->data,
						     source->stride);
	    if (unlikely (pixman_image == NULL))
		return NULL;
	} else if (type == CAIRO_SURFACE_TYPE_SUBSURFACE) {
	    cairo_surface_subsurface_t *sub;
	    cairo_bool_t is_contained = FALSE;

	    sub = (cairo_surface_subsurface_t *) source;
	    source = (cairo_image_surface_t *) sub->target;

	    if (sample.x >= 0 &&
		sample.y >= 0 &&
		sample.x + sample.width  <= sub->extents.width &&
		sample.y + sample.height <= sub->extents.height)
	    {
		is_contained = TRUE;
	    }

	    if (sample.width == 1 && sample.height == 1) {
		if (is_contained) {
		    return _pixel_to_solid (source,
					    sub->extents.x + sample.x,
					    sub->extents.y + sample.y);
		} else {
		    if (extend == CAIRO_EXTEND_NONE)
			return _pixman_transparent_image ();
		}
	    }

	    if (is_contained &&
		_cairo_matrix_is_translation (&pattern->base.matrix) &&
		_nearest_sample (filter, &tx, &ty))
	    {
		*ix = tx + sub->extents.x;
		*iy = ty + sub->extents.y;
		return pixman_image_ref (source->pixman_image);
	    }

	    /* Avoid sub-byte offsets, force a copy in that case. */
	    if (PIXMAN_FORMAT_BPP (source->pixman_format) >= 8) {
		pixman_image = pixman_image_create_bits (source->pixman_format,
							 sub->extents.width,
							 sub->extents.height,
							 (uint32_t *) (source->data + sub->extents.x * PIXMAN_FORMAT_BPP(source->pixman_format)/8 + sub->extents.y * source->stride),
							 source->stride);
		if (unlikely (pixman_image == NULL))
		    return NULL;
	    }
	}
    }

    if (pixman_image == NULL) {
	struct acquire_source_cleanup *cleanup;
	cairo_image_surface_t *image;
	void *extra;
	cairo_status_t status;

	status = _cairo_surface_acquire_source_image (pattern->surface, &image, &extra);
	if (unlikely (status))
	    return NULL;

	if (sample.width == 1 && sample.height == 1) {
	    if (sample.x < 0 ||
		sample.y < 0 ||
		sample.x >= image->width ||
		sample.y >= image->height)
	    {
		if (extend == CAIRO_EXTEND_NONE) {
		    pixman_image = _pixman_transparent_image ();
		    _cairo_surface_release_source_image (pattern->surface, image, extra);
		    return pixman_image;
		}
	    }
	    else
	    {
		pixman_image = _pixel_to_solid (image, sample.x, sample.y);
		_cairo_surface_release_source_image (pattern->surface, image, extra);
		return pixman_image;
	    }
	}

	pixman_image = pixman_image_create_bits (image->pixman_format,
						 image->width,
						 image->height,
						 (uint32_t *) image->data,
						 image->stride);
	if (unlikely (pixman_image == NULL)) {
	    _cairo_surface_release_source_image (pattern->surface, image, extra);
	    return NULL;
	}

	cleanup = malloc (sizeof (*cleanup));
	if (unlikely (cleanup == NULL)) {
	    _cairo_surface_release_source_image (pattern->surface, image, extra);
	    pixman_image_unref (pixman_image);
	    return NULL;
	}

	cleanup->surface = pattern->surface;
	cleanup->image = image;
	cleanup->image_extra = extra;
	pixman_image_set_destroy_function (pixman_image,
					   _acquire_source_cleanup, cleanup);
    }

    if (! _cairo_matrix_is_translation (&pattern->base.matrix) ||
	! _nearest_sample (filter, &tx, &ty))
    {
	pixman_transform_t pixman_transform;
	cairo_matrix_t m;

	m = pattern->base.matrix;
	if (m.x0 != 0. || m.y0 != 0.) {
	    cairo_matrix_t inv;
	    cairo_status_t status;
	    double x, y;

	    /* pixman also limits the [xy]_offset to 16 bits so evenly
	     * spread the bits between the two.
	     */
	    inv = m;
	    status = cairo_matrix_invert (&inv);
	    assert (status == CAIRO_STATUS_SUCCESS);

	    x = floor (inv.x0 / 2);
	    y = floor (inv.y0 / 2);
	    tx = -x;
	    ty = -y;
	    cairo_matrix_init_translate (&inv, x, y);
	    cairo_matrix_multiply (&m, &inv, &m);
	} else {
	    tx = ty = 0;
	}

	_cairo_matrix_to_pixman_matrix (&m, &pixman_transform,
					extents->x + extents->width/2.,
					extents->y + extents->height/2.);
	if (! pixman_image_set_transform (pixman_image, &pixman_transform)) {
	    pixman_image_unref (pixman_image);
	    return NULL;
	}
    }
    *ix = tx;
    *iy = ty;

    if (_cairo_matrix_has_unity_scale (&pattern->base.matrix) &&
	tx == pattern->base.matrix.x0 &&
	ty == pattern->base.matrix.y0)
    {
	pixman_image_set_filter (pixman_image, PIXMAN_FILTER_NEAREST, NULL, 0);
    }
    else
    {
	pixman_filter_t pixman_filter;

	switch (filter) {
	case CAIRO_FILTER_FAST:
	    pixman_filter = PIXMAN_FILTER_FAST;
	    break;
	case CAIRO_FILTER_GOOD:
	    pixman_filter = PIXMAN_FILTER_GOOD;
	    break;
	case CAIRO_FILTER_BEST:
	    pixman_filter = PIXMAN_FILTER_BEST;
	    break;
	case CAIRO_FILTER_NEAREST:
	    pixman_filter = PIXMAN_FILTER_NEAREST;
	    break;
	case CAIRO_FILTER_BILINEAR:
	    pixman_filter = PIXMAN_FILTER_BILINEAR;
	    break;
	case CAIRO_FILTER_GAUSSIAN:
	    /* XXX: The GAUSSIAN value has no implementation in cairo
	     * whatsoever, so it was really a mistake to have it in the
	     * API. We could fix this by officially deprecating it, or
	     * else inventing semantics and providing an actual
	     * implementation for it. */
	default:
	    pixman_filter = PIXMAN_FILTER_BEST;
	}

	pixman_image_set_filter (pixman_image, pixman_filter, NULL, 0);
    }

    {
	pixman_repeat_t pixman_repeat;

	switch (extend) {
	default:
	case CAIRO_EXTEND_NONE:
	    pixman_repeat = PIXMAN_REPEAT_NONE;
	    break;
	case CAIRO_EXTEND_REPEAT:
	    pixman_repeat = PIXMAN_REPEAT_NORMAL;
	    break;
	case CAIRO_EXTEND_REFLECT:
	    pixman_repeat = PIXMAN_REPEAT_REFLECT;
	    break;
	case CAIRO_EXTEND_PAD:
	    pixman_repeat = PIXMAN_REPEAT_PAD;
	    break;
	}

	pixman_image_set_repeat (pixman_image, pixman_repeat);
    }

    if (pattern->base.has_component_alpha)
	pixman_image_set_component_alpha (pixman_image, TRUE);

    return pixman_image;
}

static pixman_image_t *
_pixman_image_for_pattern (const cairo_pattern_t *pattern,
			   cairo_bool_t is_mask,
			   const cairo_rectangle_int_t *extents,
			   int *tx, int *ty)
{
    *tx = *ty = 0;

    if (pattern == NULL)
	return _pixman_white_image ();

    switch (pattern->type) {
    default:
	ASSERT_NOT_REACHED;
    case CAIRO_PATTERN_TYPE_SOLID:
	return _pixman_image_for_solid ((const cairo_solid_pattern_t *) pattern);

    case CAIRO_PATTERN_TYPE_RADIAL:
    case CAIRO_PATTERN_TYPE_LINEAR:
	return _pixman_image_for_gradient ((const cairo_gradient_pattern_t *) pattern,
					   extents, tx, ty);

    case CAIRO_PATTERN_TYPE_SURFACE:
	return _pixman_image_for_surface ((const cairo_surface_pattern_t *) pattern,
					  is_mask, extents, tx, ty);
    }
}

static cairo_status_t
_cairo_image_surface_fixup_unbounded (cairo_image_surface_t *dst,
				      const cairo_composite_rectangles_t *rects,
				      cairo_clip_t *clip)
{
    pixman_image_t *mask = NULL;
    pixman_box32_t boxes[4];
    int i, mask_x = 0, mask_y = 0, n_boxes = 0;

    if (clip != NULL) {
	cairo_surface_t *clip_surface;
	int clip_x, clip_y;

	clip_surface = _cairo_clip_get_surface (clip, &dst->base, &clip_x, &clip_y);
	if (unlikely (clip_surface->status))
	    return clip_surface->status;

	mask = ((cairo_image_surface_t *) clip_surface)->pixman_image;
	mask_x = -clip_x;
	mask_y = -clip_y;
    } else {
	if (rects->bounded.width  == rects->unbounded.width &&
	    rects->bounded.height == rects->unbounded.height)
	{
	    return CAIRO_STATUS_SUCCESS;
	}
    }

    /* wholly unbounded? */
    if (rects->bounded.width == 0 || rects->bounded.height == 0) {
	int x = rects->unbounded.x;
	int y = rects->unbounded.y;
	int width = rects->unbounded.width;
	int height = rects->unbounded.height;

	if (mask != NULL) {
	    pixman_image_composite32 (PIXMAN_OP_OUT_REVERSE,
                                      mask, NULL, dst->pixman_image,
                                      x + mask_x, y + mask_y,
                                      0, 0,
                                      x, y,
                                      width, height);
	} else {
            pixman_color_t color = { 0, };
            pixman_box32_t box = { x, y, x + width, y + height };

            if (! pixman_image_fill_boxes (PIXMAN_OP_CLEAR,
					   dst->pixman_image,
					   &color,
					   1, &box))
		return _cairo_error (CAIRO_STATUS_NO_MEMORY);
	}

	return CAIRO_STATUS_SUCCESS;
    }

    /* top */
    if (rects->bounded.y != rects->unbounded.y) {
        boxes[n_boxes].x1 = rects->unbounded.x;
        boxes[n_boxes].y1 = rects->unbounded.y;
        boxes[n_boxes].x2 = rects->unbounded.x + rects->unbounded.width;
        boxes[n_boxes].y2 = rects->bounded.y;
        n_boxes++;
    }

    /* left */
    if (rects->bounded.x != rects->unbounded.x) {
        boxes[n_boxes].x1 = rects->unbounded.x;
        boxes[n_boxes].y1 = rects->bounded.y;
        boxes[n_boxes].x2 = rects->bounded.x;
        boxes[n_boxes].y2 = rects->bounded.y + rects->bounded.height;
        n_boxes++;
    }

    /* right */
    if (rects->bounded.x + rects->bounded.width != rects->unbounded.x + rects->unbounded.width) {
        boxes[n_boxes].x1 = rects->bounded.x + rects->bounded.width;
        boxes[n_boxes].y1 = rects->bounded.y;
        boxes[n_boxes].x2 = rects->unbounded.x + rects->unbounded.width;
        boxes[n_boxes].y2 = rects->bounded.y + rects->bounded.height;
        n_boxes++;
    }

    /* bottom */
    if (rects->bounded.y + rects->bounded.height != rects->unbounded.y + rects->unbounded.height) {
        boxes[n_boxes].x1 = rects->unbounded.x;
        boxes[n_boxes].y1 = rects->bounded.y + rects->bounded.height;
        boxes[n_boxes].x2 = rects->unbounded.x + rects->unbounded.width;
        boxes[n_boxes].y2 = rects->unbounded.y + rects->unbounded.height;
        n_boxes++;
    }

    if (mask != NULL) {
        for (i = 0; i < n_boxes; i++) {
            pixman_image_composite32 (PIXMAN_OP_OUT_REVERSE,
                                      mask, NULL, dst->pixman_image,
                                      boxes[i].x1 + mask_x, boxes[i].y1 + mask_y,
                                      0, 0,
                                      boxes[i].x1, boxes[i].y1,
                                      boxes[i].x2 - boxes[i].x1, boxes[i].y2 - boxes[i].y1);
        }
    } else {
        pixman_color_t color = { 0, };

        if (! pixman_image_fill_boxes (PIXMAN_OP_CLEAR,
				       dst->pixman_image,
				       &color,
				       n_boxes,
				       boxes))
	{
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
	}
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_image_surface_fixup_unbounded_boxes (cairo_image_surface_t *dst,
					    const cairo_composite_rectangles_t *extents,
					    cairo_region_t *clip_region,
					    cairo_boxes_t *boxes)
{
    cairo_boxes_t clear;
    cairo_box_t box;
    cairo_status_t status;
    struct _cairo_boxes_chunk *chunk;
    int i;

    // If we have no boxes then we need to clear the entire extents
    // because we have nothing to draw.
    if (boxes->num_boxes < 1 && clip_region == NULL) {
        int x = extents->unbounded.x;
        int y = extents->unbounded.y;
        int width = extents->unbounded.width;
        int height = extents->unbounded.height;

        pixman_color_t color = { 0 };
        pixman_box32_t box = { x, y, x + width, y + height };

        if (! pixman_image_fill_boxes (PIXMAN_OP_CLEAR,
                                       dst->pixman_image,
                                       &color,
                                       1, &box)) {
            return _cairo_error (CAIRO_STATUS_NO_MEMORY);
        }
        return CAIRO_STATUS_SUCCESS;
    }

    _cairo_boxes_init (&clear);

    box.p1.x = _cairo_fixed_from_int (extents->unbounded.x + extents->unbounded.width);
    box.p1.y = _cairo_fixed_from_int (extents->unbounded.y);
    box.p2.x = _cairo_fixed_from_int (extents->unbounded.x);
    box.p2.y = _cairo_fixed_from_int (extents->unbounded.y + extents->unbounded.height);

    if (clip_region == NULL) {
	cairo_boxes_t tmp;

	_cairo_boxes_init (&tmp);

	status = _cairo_boxes_add (&tmp, &box);
	assert (status == CAIRO_STATUS_SUCCESS);

	tmp.chunks.next = &boxes->chunks;
	tmp.num_boxes += boxes->num_boxes;

	status = _cairo_bentley_ottmann_tessellate_boxes (&tmp,
							  CAIRO_FILL_RULE_WINDING,
							  &clear);

	tmp.chunks.next = NULL;
    } else {
	pixman_box32_t *pbox;

	pbox = pixman_region32_rectangles (&clip_region->rgn, &i);
	_cairo_boxes_limit (&clear, (cairo_box_t *) pbox, i);

	status = _cairo_boxes_add (&clear, &box);
	assert (status == CAIRO_STATUS_SUCCESS);

	for (chunk = &boxes->chunks; chunk != NULL; chunk = chunk->next) {
	    for (i = 0; i < chunk->count; i++) {
		status = _cairo_boxes_add (&clear, &chunk->base[i]);
		if (unlikely (status)) {
		    _cairo_boxes_fini (&clear);
		    return status;
		}
	    }
	}

	status = _cairo_bentley_ottmann_tessellate_boxes (&clear,
							  CAIRO_FILL_RULE_WINDING,
							  &clear);
    }

    if (likely (status == CAIRO_STATUS_SUCCESS)) {
	for (chunk = &clear.chunks; chunk != NULL; chunk = chunk->next) {
	    for (i = 0; i < chunk->count; i++) {
		int x1 = _cairo_fixed_integer_part (chunk->base[i].p1.x);
		int y1 = _cairo_fixed_integer_part (chunk->base[i].p1.y);
		int x2 = _cairo_fixed_integer_part (chunk->base[i].p2.x);
		int y2 = _cairo_fixed_integer_part (chunk->base[i].p2.y);

		x1 = (x1 < 0 ? 0 : x1);
		y1 = (y1 < 0 ? 0 : y1);
		if (x2 <= x1 || y2 <= y1)
		    continue;
		pixman_fill ((uint32_t *) dst->data, dst->stride / sizeof (uint32_t),
			     PIXMAN_FORMAT_BPP (dst->pixman_format),
			     x1, y1, x2 - x1, y2 - y1,
			     0);
	    }
	}
    }

    _cairo_boxes_fini (&clear);

    return status;
}

static cairo_bool_t
can_reduce_alpha_op (cairo_operator_t op)
{
    int iop = op;
    switch (iop) {
    case CAIRO_OPERATOR_OVER:
    case CAIRO_OPERATOR_SOURCE:
    case CAIRO_OPERATOR_ADD:
	return TRUE;
    default:
	return FALSE;
    }
}

static cairo_bool_t
reduce_alpha_op (cairo_image_surface_t *dst,
		 cairo_operator_t op,
		 const cairo_pattern_t *pattern)
{
    return dst->base.is_clear &&
	   dst->base.content == CAIRO_CONTENT_ALPHA &&
	   _cairo_pattern_is_opaque_solid (pattern) &&
	   can_reduce_alpha_op (op);
}

/* low level compositor */
typedef cairo_status_t
(*image_draw_func_t) (void				*closure,
		      pixman_image_t			*dst,
		      pixman_format_code_t		 dst_format,
		      cairo_operator_t			 op,
		      const cairo_pattern_t		*src,
		      int				 dst_x,
		      int				 dst_y,
		      const cairo_rectangle_int_t	*extents,
		      cairo_region_t			*clip_region);

static pixman_image_t *
_create_composite_mask_pattern (cairo_clip_t                  *clip,
				image_draw_func_t              draw_func,
				void                          *draw_closure,
				cairo_image_surface_t         *dst,
				const cairo_rectangle_int_t   *extents)
{
    cairo_region_t *clip_region = NULL;
    pixman_image_t *mask;
    cairo_status_t status;
    cairo_bool_t need_clip_surface = FALSE;

    if (clip != NULL) {
	status = _cairo_clip_get_region (clip, &clip_region);
	assert (! _cairo_status_is_error (status));

	/* The all-clipped state should never propagate this far. */
	assert (status != CAIRO_INT_STATUS_NOTHING_TO_DO);

	need_clip_surface = status == CAIRO_INT_STATUS_UNSUPPORTED;

	if (clip_region != NULL && cairo_region_num_rectangles (clip_region) == 1)
	    clip_region = NULL;
    }

    mask = pixman_image_create_bits (PIXMAN_a8, extents->width, extents->height,
				     NULL, 0);
    if (unlikely (mask == NULL))
	return NULL;

    /* Is it worth setting the clip region here? */
    if (clip_region != NULL) {
	pixman_bool_t ret;

	pixman_region32_translate (&clip_region->rgn, -extents->x, -extents->y);
	ret = pixman_image_set_clip_region32 (mask, &clip_region->rgn);
	pixman_region32_translate (&clip_region->rgn, extents->x, extents->y);

	if (! ret) {
	    pixman_image_unref (mask);
	    return NULL;
	}
    }

    status = draw_func (draw_closure,
			mask, PIXMAN_a8,
			CAIRO_OPERATOR_ADD, NULL,
			extents->x, extents->y,
			extents, NULL);
    if (unlikely (status)) {
	pixman_image_unref (mask);
	return NULL;
    }

    if (need_clip_surface) {
	cairo_surface_t *tmp;

	tmp = _cairo_image_surface_create_for_pixman_image (mask, PIXMAN_a8);
	if (unlikely (tmp->status)) {
	    pixman_image_unref (mask);
	    return NULL;
	}

	pixman_image_ref (mask);

	status = _cairo_clip_combine_with_surface (clip, tmp, extents->x, extents->y);
	cairo_surface_destroy (tmp);
	if (unlikely (status)) {
	    pixman_image_unref (mask);
	    return NULL;
	}
    }

    if (clip_region != NULL)
	pixman_image_set_clip_region (mask, NULL);

    return mask;
}

/* Handles compositing with a clip surface when the operator allows
 * us to combine the clip with the mask
 */
static cairo_status_t
_clip_and_composite_with_mask (cairo_clip_t                  *clip,
			       cairo_operator_t               op,
			       const cairo_pattern_t         *pattern,
			       image_draw_func_t              draw_func,
			       void                          *draw_closure,
			       cairo_image_surface_t         *dst,
			       const cairo_rectangle_int_t   *extents)
{
    pixman_image_t *mask;

    mask = _create_composite_mask_pattern (clip, draw_func, draw_closure, dst, extents);
    if (unlikely (mask == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    if (pattern == NULL) {
	if (dst->pixman_format == PIXMAN_a8) {
	    pixman_image_composite32 (_pixman_operator (op),
                                      mask, NULL, dst->pixman_image,
                                      0, 0, 0, 0,
                                      extents->x,      extents->y,
                                      extents->width,  extents->height);
	} else {
	    pixman_image_t *src;

	    src = _pixman_white_image ();
	    if (unlikely (src == NULL)) {
		pixman_image_unref (mask);
		return _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    }

	    pixman_image_composite32 (_pixman_operator (op),
                                      src, mask, dst->pixman_image,
                                      0, 0, 0, 0,
                                      extents->x,      extents->y,
                                      extents->width,  extents->height);
	    pixman_image_unref (src);
	}
    } else {
	pixman_image_t *src;
	int src_x, src_y;

	src = _pixman_image_for_pattern (pattern, FALSE, extents, &src_x, &src_y);
	if (unlikely (src == NULL)) {
	    pixman_image_unref (mask);
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
	}

	pixman_image_composite32 (_pixman_operator (op),
                                  src, mask, dst->pixman_image,
                                  extents->x + src_x,  extents->y + src_y,
                                  0, 0,
                                  extents->x,          extents->y,
                                  extents->width,      extents->height);
	pixman_image_unref (src);
    }

    pixman_image_unref (mask);

    return CAIRO_STATUS_SUCCESS;
}

/* Handles compositing with a clip surface when we have to do the operation
 * in two pieces and combine them together.
 */
static cairo_status_t
_clip_and_composite_combine (cairo_clip_t                  *clip,
			     cairo_operator_t               op,
			     const cairo_pattern_t         *src,
			     image_draw_func_t              draw_func,
			     void                          *draw_closure,
			     cairo_image_surface_t               *dst,
			     const cairo_rectangle_int_t   *extents)
{
    pixman_image_t *tmp;
    cairo_surface_t *clip_surface;
    int clip_x, clip_y;
    cairo_status_t status;

    tmp  = pixman_image_create_bits (dst->pixman_format,
				     extents->width, extents->height,
				     NULL, 0);
    if (unlikely (tmp == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    if (src == NULL) {
	status = (*draw_func) (draw_closure,
			       tmp, dst->pixman_format,
			       CAIRO_OPERATOR_ADD, NULL,
			       extents->x, extents->y,
			       extents, NULL);
    } else {
	/* Initialize the temporary surface from the destination surface */
	if (! dst->base.is_clear) {
	    pixman_image_composite32 (PIXMAN_OP_SRC,
                                      dst->pixman_image, NULL, tmp,
                                      extents->x, extents->y,
                                      0, 0,
                                      0, 0,
                                      extents->width, extents->height);
	}

	status = (*draw_func) (draw_closure,
			       tmp, dst->pixman_format,
			       op, src,
			       extents->x, extents->y,
			       extents, NULL);
    }
    if (unlikely (status))
	goto CLEANUP_SURFACE;

    assert (clip->path != NULL);
    clip_surface = _cairo_clip_get_surface (clip, &dst->base, &clip_x, &clip_y);
    if (unlikely (clip_surface->status))
	goto CLEANUP_SURFACE;

    if (! dst->base.is_clear) {
#if PIXMAN_HAS_OP_LERP
	pixman_image_composite32 (PIXMAN_OP_LERP,
                                  tmp,
                                  ((cairo_image_surface_t *) clip_surface)->pixman_image,
                                  dst->pixman_image,
                                  0, 0,
                                  extents->x - clip_x,
                                  extents->y - clip_y,
                                  extents->x, extents->y,
                                  extents->width, extents->height);
#else
	/* Punch the clip out of the destination */
	pixman_image_composite32 (PIXMAN_OP_OUT_REVERSE,
                                  ((cairo_image_surface_t *) clip_surface)->pixman_image,
                                  NULL, dst->pixman_image,
                                  extents->x - clip_x,
                                  extents->y - clip_y,
                                  0, 0,
                                  extents->x, extents->y,
                                  extents->width, extents->height);

	/* Now add the two results together */
	pixman_image_composite32 (PIXMAN_OP_ADD,
                                  tmp,
                                  ((cairo_image_surface_t *) clip_surface)->pixman_image,
                                  dst->pixman_image,
                                  0, 0,
                                  extents->x - clip_x,
                                  extents->y - clip_y,
                                  extents->x, extents->y,
                                  extents->width, extents->height);
#endif
    } else {
	pixman_image_composite32 (PIXMAN_OP_SRC,
                                  tmp,
                                  ((cairo_image_surface_t *) clip_surface)->pixman_image,
                                  dst->pixman_image,
                                  0, 0,
                                  extents->x - clip_x,
                                  extents->y - clip_y,
                                  extents->x, extents->y,
                                  extents->width, extents->height);
    }

 CLEANUP_SURFACE:
    pixman_image_unref (tmp);

    return status;
}

/* Handles compositing for %CAIRO_OPERATOR_SOURCE, which is special; it's
 * defined as (src IN mask IN clip) ADD (dst OUT (mask IN clip))
 */
static cairo_status_t
_clip_and_composite_source (cairo_clip_t                  *clip,
			    const cairo_pattern_t         *pattern,
			    image_draw_func_t              draw_func,
			    void                          *draw_closure,
			    cairo_image_surface_t         *dst,
			    const cairo_rectangle_int_t   *extents)
{
    pixman_image_t *mask, *src;
    int src_x, src_y;

    if (pattern == NULL) {
	cairo_region_t *clip_region;
	cairo_status_t status;

	status = draw_func (draw_closure,
			    dst->pixman_image, dst->pixman_format,
			    CAIRO_OPERATOR_SOURCE, NULL,
			    extents->x, extents->y,
			    extents, NULL);
	if (unlikely (status))
	    return status;

	if (_cairo_clip_get_region (clip, &clip_region) == CAIRO_INT_STATUS_UNSUPPORTED)
	    status = _cairo_clip_combine_with_surface (clip, &dst->base, 0, 0);

	return status;
    }

    /* Create a surface that is mask IN clip */
    mask = _create_composite_mask_pattern (clip, draw_func, draw_closure, dst, extents);
    if (unlikely (mask == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    src = _pixman_image_for_pattern (pattern, FALSE, extents, &src_x, &src_y);
    if (unlikely (src == NULL)) {
	pixman_image_unref (mask);
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    if (! dst->base.is_clear) {
#if PIXMAN_HAS_OP_LERP
	pixman_image_composite32 (PIXMAN_OP_LERP,
                                  src, mask, dst->pixman_image,
                                  extents->x + src_x, extents->y + src_y,
                                  0, 0,
                                  extents->x,     extents->y,
                                  extents->width, extents->height);
#else
	/* Compute dest' = dest OUT (mask IN clip) */
	pixman_image_composite32 (PIXMAN_OP_OUT_REVERSE,
                                  mask, NULL, dst->pixman_image,
                                  0, 0, 0, 0,
                                  extents->x,     extents->y,
                                  extents->width, extents->height);

	/* Now compute (src IN (mask IN clip)) ADD dest' */
	pixman_image_composite32 (PIXMAN_OP_ADD,
                                  src, mask, dst->pixman_image,
                                  extents->x + src_x, extents->y + src_y,
                                  0, 0,
                                  extents->x,     extents->y,
                                  extents->width, extents->height);
#endif
    } else {
	pixman_image_composite32 (PIXMAN_OP_SRC,
                                  src, mask, dst->pixman_image,
                                  extents->x + src_x, extents->y + src_y,
                                  0, 0,
                                  extents->x,     extents->y,
                                  extents->width, extents->height);
    }

    pixman_image_unref (src);
    pixman_image_unref (mask);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_clip_and_composite (cairo_image_surface_t	*dst,
		     cairo_operator_t		 op,
		     const cairo_pattern_t	*src,
		     image_draw_func_t		 draw_func,
		     void			*draw_closure,
		     cairo_composite_rectangles_t*extents,
		     cairo_clip_t		*clip)
{
    cairo_status_t status;
    cairo_region_t *clip_region = NULL;
    cairo_bool_t need_clip_surface = FALSE;

    if (clip != NULL) {
	status = _cairo_clip_get_region (clip, &clip_region);
	if (unlikely (status == CAIRO_INT_STATUS_NOTHING_TO_DO))
	    return CAIRO_STATUS_SUCCESS;
	if (unlikely (_cairo_status_is_error (status)))
	    return status;

	need_clip_surface = status == CAIRO_INT_STATUS_UNSUPPORTED;

	if (clip_region != NULL) {
	    cairo_rectangle_int_t rect;
	    cairo_bool_t is_empty;

	    cairo_region_get_extents (clip_region, &rect);
	    is_empty = ! _cairo_rectangle_intersect (&extents->unbounded, &rect);
	    if (unlikely (is_empty))
		return CAIRO_STATUS_SUCCESS;

	    is_empty = ! _cairo_rectangle_intersect (&extents->bounded, &rect);
	    if (unlikely (is_empty && extents->is_bounded))
		return CAIRO_STATUS_SUCCESS;

	    if (cairo_region_num_rectangles (clip_region) == 1)
		clip_region = NULL;
	}
    }

    if (clip_region != NULL) {
	status = _cairo_image_surface_set_clip_region (dst, clip_region);
	if (unlikely (status))
	    return status;
    }

    if (reduce_alpha_op (dst, op, src)) {
	op = CAIRO_OPERATOR_ADD;
	src = NULL;
    }

    if (op == CAIRO_OPERATOR_SOURCE) {
	status = _clip_and_composite_source (clip, src,
					     draw_func, draw_closure,
					     dst, &extents->bounded);
    } else {
	if (op == CAIRO_OPERATOR_CLEAR) {
	    src = NULL;
	    op = CAIRO_OPERATOR_DEST_OUT;
	}

	if (need_clip_surface) {
	    if (extents->is_bounded) {
		status = _clip_and_composite_with_mask (clip, op, src,
							draw_func, draw_closure,
							dst, &extents->bounded);
	    } else {
		status = _clip_and_composite_combine (clip, op, src,
						      draw_func, draw_closure,
						      dst, &extents->bounded);
	    }
	} else {
	    status = draw_func (draw_closure,
				dst->pixman_image, dst->pixman_format,
				op, src,
				0, 0,
				&extents->bounded,
				clip_region);
	}
    }

    if (status == CAIRO_STATUS_SUCCESS && ! extents->is_bounded) {
	status = _cairo_image_surface_fixup_unbounded (dst, extents,
						       need_clip_surface ? clip : NULL);
    }

    if (clip_region != NULL)
	_cairo_image_surface_unset_clip_region (dst);

    return status;
}

#define CAIRO_FIXED_16_16_MIN _cairo_fixed_from_int (-32768)
#define CAIRO_FIXED_16_16_MAX _cairo_fixed_from_int (32767)

static cairo_bool_t
_line_exceeds_16_16 (const cairo_line_t *line)
{
    return
	line->p1.x <= CAIRO_FIXED_16_16_MIN ||
	line->p1.x >= CAIRO_FIXED_16_16_MAX ||

	line->p2.x <= CAIRO_FIXED_16_16_MIN ||
	line->p2.x >= CAIRO_FIXED_16_16_MAX ||

	line->p1.y <= CAIRO_FIXED_16_16_MIN ||
	line->p1.y >= CAIRO_FIXED_16_16_MAX ||

	line->p2.y <= CAIRO_FIXED_16_16_MIN ||
	line->p2.y >= CAIRO_FIXED_16_16_MAX;
}

static void
_project_line_x_onto_16_16 (const cairo_line_t *line,
			    cairo_fixed_t top,
			    cairo_fixed_t bottom,
			    pixman_line_fixed_t *out)
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


typedef struct {
    cairo_trapezoid_t *traps;
    int num_traps;
    cairo_antialias_t antialias;
} composite_traps_info_t;

static void
_pixman_image_add_traps (pixman_image_t *image,
			 int dst_x, int dst_y,
			 composite_traps_info_t *info)
{
    cairo_trapezoid_t *t = info->traps;
    int num_traps = info->num_traps;
    while (num_traps--) {
	pixman_trapezoid_t trap;

	/* top/bottom will be clamped to surface bounds */
	trap.top = _cairo_fixed_to_16_16 (t->top);
	trap.bottom = _cairo_fixed_to_16_16 (t->bottom);

	/* However, all the other coordinates will have been left untouched so
	 * as not to introduce numerical error. Recompute them if they
	 * exceed the 16.16 limits.
	 */
	if (unlikely (_line_exceeds_16_16 (&t->left))) {
	    _project_line_x_onto_16_16 (&t->left, t->top, t->bottom, &trap.left);
	    trap.left.p1.y = trap.top;
	    trap.left.p2.y = trap.bottom;
	} else {
	    trap.left.p1.x = _cairo_fixed_to_16_16 (t->left.p1.x);
	    trap.left.p1.y = _cairo_fixed_to_16_16 (t->left.p1.y);
	    trap.left.p2.x = _cairo_fixed_to_16_16 (t->left.p2.x);
	    trap.left.p2.y = _cairo_fixed_to_16_16 (t->left.p2.y);
	}

	if (unlikely (_line_exceeds_16_16 (&t->right))) {
	    _project_line_x_onto_16_16 (&t->right, t->top, t->bottom, &trap.right);
	    trap.right.p1.y = trap.top;
	    trap.right.p2.y = trap.bottom;
	} else {
	    trap.right.p1.x = _cairo_fixed_to_16_16 (t->right.p1.x);
	    trap.right.p1.y = _cairo_fixed_to_16_16 (t->right.p1.y);
	    trap.right.p2.x = _cairo_fixed_to_16_16 (t->right.p2.x);
	    trap.right.p2.y = _cairo_fixed_to_16_16 (t->right.p2.y);
	}

	pixman_rasterize_trapezoid (image, &trap, -dst_x, -dst_y);

	t++;
    }
}

static cairo_status_t
_composite_traps (void                          *closure,
		  pixman_image_t		*dst,
		  pixman_format_code_t		 dst_format,
		  cairo_operator_t               op,
		  const cairo_pattern_t         *pattern,
		  int                            dst_x,
		  int                            dst_y,
		  const cairo_rectangle_int_t   *extents,
		  cairo_region_t		*clip_region)
{
    composite_traps_info_t *info = closure;
    pixman_image_t *src, *mask;
    pixman_format_code_t format;
    int src_x = 0, src_y = 0;
    cairo_status_t status;

    /* Special case adding trapezoids onto a mask surface; we want to avoid
     * creating an intermediate temporary mask unnecessarily.
     *
     * We make the assumption here that the portion of the trapezoids
     * contained within the surface is bounded by [dst_x,dst_y,width,height];
     * the Cairo core code passes bounds based on the trapezoid extents.
     */
    format = info->antialias == CAIRO_ANTIALIAS_NONE ? PIXMAN_a1 : PIXMAN_a8;
    if (dst_format == format &&
	(pattern == NULL ||
	 (op == CAIRO_OPERATOR_ADD && _cairo_pattern_is_opaque_solid (pattern))))
    {
	_pixman_image_add_traps (dst, dst_x, dst_y, info);
	return CAIRO_STATUS_SUCCESS;
    }

    src = _pixman_image_for_pattern (pattern, FALSE, extents, &src_x, &src_y);
    if (unlikely (src == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    mask = pixman_image_create_bits (format, extents->width, extents->height,
				     NULL, 0);
    if (unlikely (mask == NULL)) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto CLEANUP_SOURCE;
    }

    _pixman_image_add_traps (mask, extents->x, extents->y, info);
    pixman_image_composite32 (_pixman_operator (op),
                              src, mask, dst,
                              extents->x + src_x, extents->y + src_y,
                              0, 0,
                              extents->x - dst_x, extents->y - dst_y,
                              extents->width, extents->height);

    pixman_image_unref (mask);

    status = CAIRO_STATUS_SUCCESS;
 CLEANUP_SOURCE:
    pixman_image_unref (src);

    return status;
}

static inline uint32_t
color_to_uint32 (const cairo_color_t *color)
{
    return
        (color->alpha_short >> 8 << 24) |
        (color->red_short >> 8 << 16)   |
        (color->green_short & 0xff00)   |
        (color->blue_short >> 8);
}

static inline cairo_bool_t
color_to_pixel (const cairo_color_t	*color,
                pixman_format_code_t	 format,
                uint32_t		*pixel)
{
    uint32_t c;

    if (!(format == PIXMAN_a8r8g8b8     ||
          format == PIXMAN_x8r8g8b8     ||
          format == PIXMAN_a8b8g8r8     ||
          format == PIXMAN_x8b8g8r8     ||
          format == PIXMAN_b8g8r8a8     ||
          format == PIXMAN_b8g8r8x8     ||
          format == PIXMAN_r5g6b5       ||
          format == PIXMAN_b5g6r5       ||
          format == PIXMAN_a8))
    {
	return FALSE;
    }

    c = color_to_uint32 (color);

    if (PIXMAN_FORMAT_TYPE (format) == PIXMAN_TYPE_ABGR) {
	c = ((c & 0xff000000) >>  0) |
	    ((c & 0x00ff0000) >> 16) |
	    ((c & 0x0000ff00) >>  0) |
	    ((c & 0x000000ff) << 16);
    }

    if (PIXMAN_FORMAT_TYPE (format) == PIXMAN_TYPE_BGRA) {
	c = ((c & 0xff000000) >> 24) |
	    ((c & 0x00ff0000) >>  8) |
	    ((c & 0x0000ff00) <<  8) |
	    ((c & 0x000000ff) << 24);
    }

    if (format == PIXMAN_a8) {
	c = c >> 24;
    } else if (format == PIXMAN_r5g6b5 || format == PIXMAN_b5g6r5) {
	c = ((((c) >> 3) & 0x001f) |
	     (((c) >> 5) & 0x07e0) |
	     (((c) >> 8) & 0xf800));
    }

    *pixel = c;
    return TRUE;
}

static inline cairo_bool_t
pattern_to_pixel (const cairo_solid_pattern_t *solid,
		  cairo_operator_t op,
		  pixman_format_code_t format,
		  uint32_t *pixel)
{
    if (op == CAIRO_OPERATOR_CLEAR) {
	*pixel = 0;
	return TRUE;
    }

    if (solid->base.type != CAIRO_PATTERN_TYPE_SOLID)
	return FALSE;

    if (op == CAIRO_OPERATOR_OVER) {
	if (solid->color.alpha_short >= 0xff00)
	    op = CAIRO_OPERATOR_SOURCE;
    }

    if (op != CAIRO_OPERATOR_SOURCE)
	return FALSE;

    return color_to_pixel (&solid->color, format, pixel);
}

typedef struct _fill_span {
    cairo_span_renderer_t base;

    uint8_t *mask_data;
    pixman_image_t *src, *dst, *mask;
} fill_span_renderer_t;

static cairo_status_t
_fill_span (void *abstract_renderer,
	    int y, int height,
	    const cairo_half_open_span_t *spans,
	    unsigned num_spans)
{
    fill_span_renderer_t *renderer = abstract_renderer;
    uint8_t *row;
    unsigned i;

    if (num_spans == 0)
	return CAIRO_STATUS_SUCCESS;

    row = renderer->mask_data - spans[0].x;
    for (i = 0; i < num_spans - 1; i++) {
	/* We implement setting the most common single pixel wide
	 * span case to avoid the overhead of a memset call.
	 * Open coding setting longer spans didn't show a
	 * noticeable improvement over memset.
	 */
	if (spans[i+1].x == spans[i].x + 1) {
	    row[spans[i].x] = spans[i].coverage;
	} else {
	    memset (row + spans[i].x,
		    spans[i].coverage,
		    spans[i+1].x - spans[i].x);
	}
    }

    do {
	pixman_image_composite32 (PIXMAN_OP_OVER,
                                  renderer->src, renderer->mask, renderer->dst,
                                  0, 0, 0, 0,
                                  spans[0].x, y++,
                                  spans[i].x - spans[0].x, 1);
    } while (--height);

    return CAIRO_STATUS_SUCCESS;
}

/* avoid using region code to re-validate boxes */
static cairo_status_t
_fill_unaligned_boxes (cairo_image_surface_t *dst,
		       const cairo_pattern_t *pattern,
		       uint32_t pixel,
		       const cairo_boxes_t *boxes,
		       const cairo_composite_rectangles_t *extents)
{
    uint8_t buf[CAIRO_STACK_BUFFER_SIZE];
    fill_span_renderer_t renderer;
    cairo_rectangular_scan_converter_t converter;
    const struct _cairo_boxes_chunk *chunk;
    cairo_status_t status;
    int i;

    /* XXX
     * using composite for fill:
     *   spiral-box-nonalign-evenodd-fill.512    2201957    2.202
     *   spiral-box-nonalign-nonzero-fill.512     336726    0.337
     *   spiral-box-pixalign-evenodd-fill.512     352256    0.352
     *   spiral-box-pixalign-nonzero-fill.512     147056    0.147
     * using fill:
     *   spiral-box-nonalign-evenodd-fill.512    3174565    3.175
     *   spiral-box-nonalign-nonzero-fill.512     182710    0.183
     *   spiral-box-pixalign-evenodd-fill.512     353863    0.354
     *   spiral-box-pixalign-nonzero-fill.512     147402    0.147
     *
     * cairo-perf-trace seems to favour using fill.
     */

    renderer.base.render_rows = _fill_span;
    renderer.dst = dst->pixman_image;

    if ((unsigned) extents->bounded.width <= sizeof (buf)) {
	renderer.mask = pixman_image_create_bits (PIXMAN_a8,
						  extents->bounded.width, 1,
						  (uint32_t *) buf,
						  sizeof (buf));
    } else {
	renderer.mask = pixman_image_create_bits (PIXMAN_a8,
						  extents->bounded.width, 1,
						  NULL,  0);
    }
    if (unlikely (renderer.mask == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    renderer.mask_data = (uint8_t *) pixman_image_get_data (renderer.mask);

    renderer.src = _pixman_image_for_solid ((const cairo_solid_pattern_t *) pattern);
    if (unlikely (renderer.src == NULL)) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto CLEANUP_MASK;
    }

    _cairo_rectangular_scan_converter_init (&converter, &extents->bounded);

    /* first blit any aligned part of the boxes */
    for (chunk = &boxes->chunks; chunk != NULL; chunk = chunk->next) {
	const cairo_box_t *box = chunk->base;

	for (i = 0; i < chunk->count; i++) {
	    int x1 = _cairo_fixed_integer_ceil (box[i].p1.x);
	    int y1 = _cairo_fixed_integer_ceil (box[i].p1.y);
	    int x2 = _cairo_fixed_integer_floor (box[i].p2.x);
	    int y2 = _cairo_fixed_integer_floor (box[i].p2.y);

	    x1 = (x1 < 0 ? 0 : x1);
	    y1 = (y1 < 0 ? 0 : y1);
	    if (x2 > x1 && y2 > y1) {
		cairo_box_t b;

		pixman_fill ((uint32_t *) dst->data,
			     dst->stride / sizeof (uint32_t),
			     PIXMAN_FORMAT_BPP (dst->pixman_format),
			     x1, y1, x2 - x1, y2 - y1,
			     pixel);

		/* top */
		if (! _cairo_fixed_is_integer (box[i].p1.y)) {
		    b.p1.x = box[i].p1.x;
		    b.p1.y = box[i].p1.y;
		    b.p2.x = box[i].p2.x;
		    b.p2.y = _cairo_fixed_from_int (y1);

		    status = _cairo_rectangular_scan_converter_add_box (&converter, &b, 1);
		    if (unlikely (status))
			goto CLEANUP_CONVERTER;
		}

		/* left */
		if (! _cairo_fixed_is_integer (box[i].p1.x)) {
		    b.p1.x = box[i].p1.x;
		    b.p1.y = box[i].p1.y;
		    b.p2.x = _cairo_fixed_from_int (x1);
		    b.p2.y = box[i].p2.y;

		    status = _cairo_rectangular_scan_converter_add_box (&converter, &b, 1);
		    if (unlikely (status))
			goto CLEANUP_CONVERTER;
		}

		/* right */
		if (! _cairo_fixed_is_integer (box[i].p2.x)) {
		    b.p1.x = _cairo_fixed_from_int (x2);
		    b.p1.y = box[i].p1.y;
		    b.p2.x = box[i].p2.x;
		    b.p2.y = box[i].p2.y;

		    status = _cairo_rectangular_scan_converter_add_box (&converter, &b, 1);
		    if (unlikely (status))
			goto CLEANUP_CONVERTER;
		}

		/* bottom */
		if (! _cairo_fixed_is_integer (box[i].p2.y)) {
		    b.p1.x = box[i].p1.x;
		    b.p1.y = _cairo_fixed_from_int (y2);
		    b.p2.x = box[i].p2.x;
		    b.p2.y = box[i].p2.y;

		    status = _cairo_rectangular_scan_converter_add_box (&converter, &b, 1);
		    if (unlikely (status))
			goto CLEANUP_CONVERTER;
		}
	    } else {
		status = _cairo_rectangular_scan_converter_add_box (&converter, &box[i], 1);
		if (unlikely (status))
		    goto CLEANUP_CONVERTER;
	    }
	}
    }

    status = converter.base.generate (&converter.base, &renderer.base);

  CLEANUP_CONVERTER:
    converter.base.destroy (&converter.base);
    pixman_image_unref (renderer.src);
  CLEANUP_MASK:
    pixman_image_unref (renderer.mask);

    return status;
}

typedef struct _cairo_image_surface_span_renderer {
    cairo_span_renderer_t base;

    uint8_t *mask_data;
    uint32_t mask_stride;
} cairo_image_surface_span_renderer_t;

cairo_status_t
_cairo_image_surface_span (void *abstract_renderer,
			   int y, int height,
			   const cairo_half_open_span_t *spans,
			   unsigned num_spans)
{
    cairo_image_surface_span_renderer_t *renderer = abstract_renderer;
    uint8_t *row;
    unsigned i;

    if (num_spans == 0)
	return CAIRO_STATUS_SUCCESS;

    /* XXX will it be quicker to repeat the sparse memset,
     * or perform a simpler memcpy?
     * The fairly dense spiral benchmarks suggests that the sparse
     * memset is a win there as well.
     */
    row = renderer->mask_data + y * renderer->mask_stride;
    do {
	for (i = 0; i < num_spans - 1; i++) {
	    if (! spans[i].coverage)
		continue;

	    /* We implement setting rendering the most common single
	     * pixel wide span case to avoid the overhead of a memset
	     * call.  Open coding setting longer spans didn't show a
	     * noticeable improvement over memset. */
	    if (spans[i+1].x == spans[i].x + 1) {
		row[spans[i].x] = spans[i].coverage;
	    } else {
		memset (row + spans[i].x,
			spans[i].coverage,
			spans[i+1].x - spans[i].x);
	    }
	}
	row += renderer->mask_stride;
    } while (--height);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_composite_unaligned_boxes (cairo_image_surface_t *dst,
			    cairo_operator_t op,
			    const cairo_pattern_t *pattern,
			    const cairo_boxes_t *boxes,
			    const cairo_composite_rectangles_t *extents)
{
    uint8_t buf[CAIRO_STACK_BUFFER_SIZE];
    cairo_image_surface_span_renderer_t renderer;
    cairo_rectangular_scan_converter_t converter;
    pixman_image_t *mask, *src;
    cairo_status_t status;
    const struct _cairo_boxes_chunk *chunk;
    int i, src_x, src_y;

    i = CAIRO_STRIDE_FOR_WIDTH_BPP (extents->bounded.width, 8) * extents->bounded.height;
    if ((unsigned) i <= sizeof (buf)) {
	mask = pixman_image_create_bits (PIXMAN_a8,
					 extents->bounded.width,
					 extents->bounded.height,
					 (uint32_t *) buf,
					 CAIRO_STRIDE_FOR_WIDTH_BPP (extents->bounded.width, 8));
	memset (buf, 0, i);
    } else {
	mask = pixman_image_create_bits (PIXMAN_a8,
					 extents->bounded.width,
					 extents->bounded.height,
					 NULL,  0);
    }
    if (unlikely (mask == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    renderer.base.render_rows = _cairo_image_surface_span;
    renderer.mask_stride = pixman_image_get_stride (mask);
    renderer.mask_data = (uint8_t *) pixman_image_get_data (mask);
    renderer.mask_data -= extents->bounded.y * renderer.mask_stride + extents->bounded.x;

    _cairo_rectangular_scan_converter_init (&converter, &extents->bounded);

    for (chunk = &boxes->chunks; chunk != NULL; chunk = chunk->next) {
	const cairo_box_t *box = chunk->base;

	for (i = 0; i < chunk->count; i++) {
	    status = _cairo_rectangular_scan_converter_add_box (&converter, &box[i], 1);
	    if (unlikely (status))
		goto CLEANUP;
	}
    }

    status = converter.base.generate (&converter.base, &renderer.base);
    if (unlikely (status))
	goto CLEANUP;

    src = _pixman_image_for_pattern (pattern, FALSE, &extents->bounded, &src_x, &src_y);
    if (unlikely (src == NULL)) {
	status =  _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto CLEANUP;
    }

    pixman_image_composite32 (_pixman_operator (op),
                              src, mask, dst->pixman_image,
                              extents->bounded.x + src_x, extents->bounded.y + src_y,
                              0, 0,
                              extents->bounded.x, extents->bounded.y,
                              extents->bounded.width, extents->bounded.height);
    pixman_image_unref (src);

  CLEANUP:
    converter.base.destroy (&converter.base);
    pixman_image_unref (mask);

    return status;
}

static cairo_status_t
_composite_boxes (cairo_image_surface_t *dst,
		  cairo_operator_t op,
		  const cairo_pattern_t *pattern,
		  cairo_boxes_t *boxes,
		  cairo_antialias_t antialias,
		  cairo_clip_t *clip,
		  const cairo_composite_rectangles_t *extents)
{
    cairo_region_t *clip_region = NULL;
    cairo_bool_t need_clip_mask = FALSE;
    cairo_status_t status;
    struct _cairo_boxes_chunk *chunk;
    uint32_t pixel;
    int i;

    if (clip != NULL) {
	status = _cairo_clip_get_region (clip, &clip_region);
	if (unlikely (status == CAIRO_INT_STATUS_NOTHING_TO_DO))
	    return CAIRO_STATUS_SUCCESS;
	need_clip_mask = status == CAIRO_INT_STATUS_UNSUPPORTED;
	if (need_clip_mask &&
	    (op == CAIRO_OPERATOR_SOURCE || ! extents->is_bounded))
	{
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}

	if (clip_region != NULL && cairo_region_num_rectangles (clip_region) == 1)
	    clip_region = NULL;
    }

    if (antialias != CAIRO_ANTIALIAS_NONE) {
	if (! boxes->is_pixel_aligned) {
	    if (need_clip_mask)
		return CAIRO_INT_STATUS_UNSUPPORTED;

	    if (pattern_to_pixel ((cairo_solid_pattern_t *) pattern, op,
				  dst->pixman_format, &pixel))
	    {
		return _fill_unaligned_boxes (dst, pattern, pixel, boxes, extents);
	    }
	    else
	    {
		return _composite_unaligned_boxes (dst, op, pattern, boxes, extents);
	    }
	}
    }

    status = CAIRO_STATUS_SUCCESS;
    if (! need_clip_mask &&
	pattern_to_pixel ((cairo_solid_pattern_t *) pattern, op, dst->pixman_format,
			  &pixel))
    {
	for (chunk = &boxes->chunks; chunk != NULL; chunk = chunk->next) {
	    cairo_box_t *box = chunk->base;

	    for (i = 0; i < chunk->count; i++) {
		int x1 = _cairo_fixed_integer_round_down (box[i].p1.x);
		int y1 = _cairo_fixed_integer_round_down (box[i].p1.y);
		int x2 = _cairo_fixed_integer_round_down (box[i].p2.x);
		int y2 = _cairo_fixed_integer_round_down (box[i].p2.y);

		x1 = (x1 < 0 ? 0 : x1);
		y1 = (y1 < 0 ? 0 : y1);
		if (x2 <= x1 || y2 <= y1)
		    continue;

		pixman_fill ((uint32_t *) dst->data, dst->stride / sizeof (uint32_t),
			     PIXMAN_FORMAT_BPP (dst->pixman_format),
			     x1, y1, x2 - x1, y2 - y1,
			     pixel);
	    }
	}
    }
    else
    {
	pixman_image_t *src = NULL, *mask = NULL;
	int src_x, src_y, mask_x = 0, mask_y = 0;
	pixman_op_t pixman_op = _pixman_operator (op);

	if (need_clip_mask) {
	    cairo_surface_t *clip_surface;
	    int clip_x, clip_y;

	    clip_surface = _cairo_clip_get_surface (clip, &dst->base, &clip_x, &clip_y);
	    if (unlikely (clip_surface->status))
		return clip_surface->status;

	    mask_x = -clip_x;
	    mask_y = -clip_y;

	    if (op == CAIRO_OPERATOR_CLEAR) {
		pattern = NULL;
		pixman_op = PIXMAN_OP_OUT_REVERSE;
	    }

	    mask = ((cairo_image_surface_t *) clip_surface)->pixman_image;
	}

	if (pattern != NULL) {
	    src = _pixman_image_for_pattern (pattern, FALSE, &extents->bounded, &src_x, &src_y);
	    if (unlikely (src == NULL))
		return _cairo_error (CAIRO_STATUS_NO_MEMORY);
	} else {
	    src = mask;
	    src_x = mask_x;
	    src_y = mask_y;
	    mask = NULL;
	}

	for (chunk = &boxes->chunks; chunk != NULL; chunk = chunk->next) {
	    const cairo_box_t *box = chunk->base;

	    for (i = 0; i < chunk->count; i++) {
		int x1 = _cairo_fixed_integer_round_down (box[i].p1.x);
		int y1 = _cairo_fixed_integer_round_down (box[i].p1.y);
		int x2 = _cairo_fixed_integer_round_down (box[i].p2.x);
		int y2 = _cairo_fixed_integer_round_down (box[i].p2.y);

		if (x2 == x1 || y2 == y1)
		    continue;

		pixman_image_composite32 (pixman_op,
                                          src, mask, dst->pixman_image,
                                          x1 + src_x,  y1 + src_y,
                                          x1 + mask_x, y1 + mask_y,
                                          x1, y1,
                                          x2 - x1, y2 - y1);
	    }
	}

	if (pattern != NULL)
	    pixman_image_unref (src);

	if (! extents->is_bounded) {
	    status =
		_cairo_image_surface_fixup_unbounded_boxes (dst, extents,
							    clip_region, boxes);
	}
    }

    return status;
}

static cairo_status_t
_clip_and_composite_boxes (cairo_image_surface_t *dst,
			   cairo_operator_t op,
			   const cairo_pattern_t *src,
			   cairo_boxes_t *boxes,
			   cairo_antialias_t antialias,
			   cairo_composite_rectangles_t *extents,
			   cairo_clip_t *clip)
{
    cairo_traps_t traps;
    cairo_status_t status;
    composite_traps_info_t info;

    if (boxes->num_boxes == 0 && extents->is_bounded)
	return CAIRO_STATUS_SUCCESS;

    /* Use a fast path if the boxes are pixel aligned */
    status = _composite_boxes (dst, op, src, boxes, antialias, clip, extents);
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;

    /* Otherwise render via a mask and composite in the usual fashion.  */
    status = _cairo_traps_init_boxes (&traps, boxes);
    if (unlikely (status))
	return status;

    info.num_traps = traps.num_traps;
    info.traps = traps.traps;
    info.antialias = antialias;
    status = _clip_and_composite (dst, op, src,
				  _composite_traps, &info,
				  extents, clip);

    _cairo_traps_fini (&traps);
    return status;
}

static cairo_bool_t
_mono_edge_is_vertical (const cairo_line_t *line)
{
    return _cairo_fixed_integer_round_down (line->p1.x) == _cairo_fixed_integer_round_down (line->p2.x);
}

static cairo_bool_t
_traps_are_pixel_aligned (cairo_traps_t *traps,
			  cairo_antialias_t antialias)
{
    int i;

    if (antialias == CAIRO_ANTIALIAS_NONE) {
	for (i = 0; i < traps->num_traps; i++) {
	    if (! _mono_edge_is_vertical (&traps->traps[i].left)   ||
		! _mono_edge_is_vertical (&traps->traps[i].right))
	    {
		traps->maybe_region = FALSE;
		return FALSE;
	    }
	}
    } else {
	for (i = 0; i < traps->num_traps; i++) {
	    if (traps->traps[i].left.p1.x != traps->traps[i].left.p2.x   ||
		traps->traps[i].right.p1.x != traps->traps[i].right.p2.x ||
		! _cairo_fixed_is_integer (traps->traps[i].top)          ||
		! _cairo_fixed_is_integer (traps->traps[i].bottom)       ||
		! _cairo_fixed_is_integer (traps->traps[i].left.p1.x)    ||
		! _cairo_fixed_is_integer (traps->traps[i].right.p1.x))
	    {
		traps->maybe_region = FALSE;
		return FALSE;
	    }
	}
    }

    return TRUE;
}

static void
_boxes_for_traps (cairo_boxes_t *boxes,
		  cairo_traps_t *traps,
		  cairo_antialias_t antialias)
{
    int i;

    _cairo_boxes_init (boxes);

    boxes->num_boxes    = traps->num_traps;
    boxes->chunks.base  = (cairo_box_t *) traps->traps;
    boxes->chunks.count = traps->num_traps;
    boxes->chunks.size  = traps->num_traps;

    if (antialias != CAIRO_ANTIALIAS_NONE) {
	for (i = 0; i < traps->num_traps; i++) {
	    /* Note the traps and boxes alias so we need to take the local copies first. */
	    cairo_fixed_t x1 = traps->traps[i].left.p1.x;
	    cairo_fixed_t x2 = traps->traps[i].right.p1.x;
	    cairo_fixed_t y1 = traps->traps[i].top;
	    cairo_fixed_t y2 = traps->traps[i].bottom;

	    boxes->chunks.base[i].p1.x = x1;
	    boxes->chunks.base[i].p1.y = y1;
	    boxes->chunks.base[i].p2.x = x2;
	    boxes->chunks.base[i].p2.y = y2;

	    if (boxes->is_pixel_aligned) {
		boxes->is_pixel_aligned =
		    _cairo_fixed_is_integer (x1) && _cairo_fixed_is_integer (y1) &&
		    _cairo_fixed_is_integer (x2) && _cairo_fixed_is_integer (y2);
	    }
	}
    } else {
	boxes->is_pixel_aligned = TRUE;

	for (i = 0; i < traps->num_traps; i++) {
	    /* Note the traps and boxes alias so we need to take the local copies first. */
	    cairo_fixed_t x1 = traps->traps[i].left.p1.x;
	    cairo_fixed_t x2 = traps->traps[i].right.p1.x;
	    cairo_fixed_t y1 = traps->traps[i].top;
	    cairo_fixed_t y2 = traps->traps[i].bottom;

	    /* round down here to match Pixman's behavior when using traps. */
	    boxes->chunks.base[i].p1.x = _cairo_fixed_round_down (x1);
	    boxes->chunks.base[i].p1.y = _cairo_fixed_round_down (y1);
	    boxes->chunks.base[i].p2.x = _cairo_fixed_round_down (x2);
	    boxes->chunks.base[i].p2.y = _cairo_fixed_round_down (y2);
	}
    }
}

static cairo_status_t
_clip_and_composite_trapezoids (cairo_image_surface_t *dst,
				cairo_operator_t op,
				const cairo_pattern_t *src,
				cairo_traps_t *traps,
				cairo_antialias_t antialias,
				cairo_composite_rectangles_t *extents,
				cairo_clip_t *clip)
{
    composite_traps_info_t info;
    cairo_bool_t need_clip_surface = FALSE;
    cairo_status_t status;

    if (traps->num_traps == 0 && extents->is_bounded)
	return CAIRO_STATUS_SUCCESS;

    if (clip != NULL) {
	cairo_region_t *clip_region;

	status = _cairo_clip_get_region (clip, &clip_region);
	need_clip_surface = status == CAIRO_INT_STATUS_UNSUPPORTED;
    }

    if (traps->has_intersections) {
	if (traps->is_rectangular)
	    status = _cairo_bentley_ottmann_tessellate_rectangular_traps (traps, CAIRO_FILL_RULE_WINDING);
	else if (traps->is_rectilinear)
	    status = _cairo_bentley_ottmann_tessellate_rectilinear_traps (traps, CAIRO_FILL_RULE_WINDING);
	else
	    status = _cairo_bentley_ottmann_tessellate_traps (traps, CAIRO_FILL_RULE_WINDING);
	if (unlikely (status))
	    return status;
    }

    /* Use a fast path if the trapezoids consist of a simple region,
     * but we can only do this if we do not have a clip surface, or can
     * substitute the mask with the clip.
     */
    if (traps->maybe_region && _traps_are_pixel_aligned (traps, antialias) &&
	(! need_clip_surface ||
	 (extents->is_bounded && op != CAIRO_OPERATOR_SOURCE)))
    {
	cairo_boxes_t boxes;

	_boxes_for_traps (&boxes, traps, antialias);
	return _clip_and_composite_boxes (dst, op, src,
					  &boxes, antialias,
					  extents, clip);
    }

    /* No fast path, exclude self-intersections and clip trapezoids. */
    /* Otherwise render the trapezoids to a mask and composite in the usual
     * fashion.
     */
    info.traps = traps->traps;
    info.num_traps = traps->num_traps;
    info.antialias = antialias;
    return _clip_and_composite (dst, op, src,
				_composite_traps, &info,
				extents, clip);
}

static cairo_clip_path_t *
_clip_get_single_path (cairo_clip_t *clip)
{
    if (clip->path->prev == NULL)
      return clip->path;

    return NULL;
}

/* high level image interface */

static cairo_int_status_t
_cairo_image_surface_paint (void			*abstract_surface,
			    cairo_operator_t		 op,
			    const cairo_pattern_t	*source,
			    cairo_clip_t		*clip)
{
    cairo_image_surface_t *surface = abstract_surface;
    cairo_composite_rectangles_t extents;
    cairo_clip_path_t *clip_path;
    cairo_clip_t local_clip;
    cairo_bool_t have_clip = FALSE;
    cairo_box_t boxes_stack[32], *clip_boxes = boxes_stack;
    int num_boxes = ARRAY_LENGTH (boxes_stack);
    cairo_status_t status;

    cairo_rectangle_int_t rect;
    rect.x = rect.y = 0;
    rect.width = surface->width;
    rect.height = surface->height;

    status = _cairo_composite_rectangles_init_for_paint (&extents,
							 &rect,
							 op, source,
							 clip);
    if (unlikely (status))
	return status;

    if (_cairo_clip_contains_extents (clip, &extents))
	clip = NULL;

    if (clip != NULL) {
	clip = _cairo_clip_init_copy (&local_clip, clip);
	have_clip = TRUE;
    }

    status = _cairo_clip_to_boxes (&clip, &extents, &clip_boxes, &num_boxes);
    if (unlikely (status)) {
	if (have_clip)
	    _cairo_clip_fini (&local_clip);

	return status;
    }

    /* If the clip cannot be reduced to a set of boxes, we will need to
     * use a clipmask. Paint is special as it is the only operation that
     * does not implicitly use a mask, so we may be able to reduce this
     * operation to a fill...
     */
    if (clip != NULL &&
	extents.is_bounded &&
	(clip_path = _clip_get_single_path (clip)) != NULL)
    {
	status = _cairo_image_surface_fill (surface, op, source,
					    &clip_path->path,
					    clip_path->fill_rule,
					    clip_path->tolerance,
					    clip_path->antialias,
					    NULL);
    }
    else
    {
	cairo_boxes_t boxes;

	_cairo_boxes_init_for_array (&boxes, clip_boxes, num_boxes);
	status = _clip_and_composite_boxes (surface, op, source,
					    &boxes, CAIRO_ANTIALIAS_DEFAULT,
					    &extents, clip);
    }

    if (clip_boxes != boxes_stack)
	free (clip_boxes);

    if (have_clip)
	_cairo_clip_fini (&local_clip);

    return status;
}

static cairo_status_t
_composite_mask (void				*closure,
		 pixman_image_t			*dst,
		 pixman_format_code_t		 dst_format,
		 cairo_operator_t		 op,
		 const cairo_pattern_t		*src_pattern,
		 int				 dst_x,
		 int				 dst_y,
		 const cairo_rectangle_int_t	*extents,
		 cairo_region_t			*clip_region)
{
    const cairo_pattern_t *mask_pattern = closure;
    pixman_image_t *src, *mask = NULL;
    int src_x = 0, src_y = 0;
    int mask_x = 0, mask_y = 0;

    if (src_pattern != NULL) {
	src = _pixman_image_for_pattern (src_pattern, FALSE, extents, &src_x, &src_y);
	if (unlikely (src == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	mask = _pixman_image_for_pattern (mask_pattern, TRUE, extents, &mask_x, &mask_y);
	if (unlikely (mask == NULL)) {
	    pixman_image_unref (src);
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
	}

	if (mask_pattern->has_component_alpha)
	    pixman_image_set_component_alpha (mask, TRUE);
    } else {
	src = _pixman_image_for_pattern (mask_pattern, FALSE, extents, &src_x, &src_y);
	if (unlikely (src == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    pixman_image_composite32 (_pixman_operator (op), src, mask, dst,
                              extents->x + src_x,  extents->y + src_y,
                              extents->x + mask_x, extents->y + mask_y,
                              extents->x - dst_x,  extents->y - dst_y,
                              extents->width,      extents->height);

    if (mask != NULL)
	pixman_image_unref (mask);
    pixman_image_unref (src);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_image_surface_mask (void				*abstract_surface,
			   cairo_operator_t		 op,
			   const cairo_pattern_t	*source,
			   const cairo_pattern_t	*mask,
			   cairo_clip_t			*clip)
{
    cairo_image_surface_t *surface = abstract_surface;
    cairo_composite_rectangles_t extents;
    cairo_clip_t local_clip;
    cairo_bool_t have_clip = FALSE;
    cairo_status_t status;

    cairo_rectangle_int_t rect;
    rect.x = rect.y = 0;
    rect.width = surface->width;
    rect.height = surface->height;

    status = _cairo_composite_rectangles_init_for_mask (&extents,
							&rect,
							op, source, mask, clip);
    if (unlikely (status))
	return status;

    if (_cairo_clip_contains_extents (clip, &extents))
	clip = NULL;

    if (clip != NULL && extents.is_bounded) {
	clip = _cairo_clip_init_copy (&local_clip, clip);
	status = _cairo_clip_rectangle (clip, &extents.bounded);
	if (unlikely (status)) {
	    _cairo_clip_fini (&local_clip);
	    return status;
	}

	have_clip = TRUE;
    }

    status = _clip_and_composite (surface, op, source,
				  _composite_mask, (void *) mask,
				  &extents, clip);

    if (have_clip)
	_cairo_clip_fini (&local_clip);

    return status;
}

typedef struct {
    cairo_polygon_t		*polygon;
    cairo_fill_rule_t		 fill_rule;
    cairo_antialias_t		 antialias;
} composite_spans_info_t;

//#define USE_BOTOR_SCAN_CONVERTER
static cairo_status_t
_composite_spans (void                          *closure,
		  pixman_image_t		*dst,
		  pixman_format_code_t		 dst_format,
		  cairo_operator_t               op,
		  const cairo_pattern_t         *pattern,
		  int                            dst_x,
		  int                            dst_y,
		  const cairo_rectangle_int_t   *extents,
		  cairo_region_t		*clip_region)
{
    uint8_t mask_buf[CAIRO_STACK_BUFFER_SIZE];
    composite_spans_info_t *info = closure;
    cairo_image_surface_span_renderer_t renderer;
#if USE_BOTOR_SCAN_CONVERTER
    cairo_box_t box;
    cairo_botor_scan_converter_t converter;
#else
    cairo_scan_converter_t *converter;
#endif
    pixman_image_t *mask;
    cairo_status_t status;

#if USE_BOTOR_SCAN_CONVERTER
    box.p1.x = _cairo_fixed_from_int (extents->x);
    box.p1.y = _cairo_fixed_from_int (extents->y);
    box.p2.x = _cairo_fixed_from_int (extents->x + extents->width);
    box.p2.y = _cairo_fixed_from_int (extents->y + extents->height);
    _cairo_botor_scan_converter_init (&converter, &box, info->fill_rule);
    status = converter.base.add_polygon (&converter.base, info->polygon);
#else
    converter = _cairo_tor_scan_converter_create (extents->x, extents->y,
						  extents->x + extents->width,
						  extents->y + extents->height,
						  info->fill_rule);
    status = converter->add_polygon (converter, info->polygon);
#endif
    if (unlikely (status))
	goto CLEANUP_CONVERTER;

    /* TODO: support rendering to A1 surfaces (or: go add span
     * compositing to pixman.) */

    if (pattern == NULL &&
	dst_format == PIXMAN_a8 &&
	op == CAIRO_OPERATOR_SOURCE)
    {
	mask = dst;
	dst = NULL;
    }
    else
    {
	int stride = CAIRO_STRIDE_FOR_WIDTH_BPP (extents->width, 8);
	uint8_t *data = mask_buf;

	if (extents->height * stride <= (int) sizeof (mask_buf))
	    memset (data, 0, extents->height * stride);
	else
	    data = NULL, stride = 0;

	mask = pixman_image_create_bits (PIXMAN_a8,
					 extents->width,
					 extents->height,
					 (uint32_t *) data,
					 stride);
	if (unlikely (mask == NULL)) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto CLEANUP_CONVERTER;
	}
    }

    renderer.base.render_rows = _cairo_image_surface_span;
    renderer.mask_stride = pixman_image_get_stride (mask);
    renderer.mask_data = (uint8_t *) pixman_image_get_data (mask);
    if (dst != NULL)
	renderer.mask_data -= extents->y * renderer.mask_stride + extents->x;
    else
	renderer.mask_data -= dst_y * renderer.mask_stride + dst_x;

#if USE_BOTOR_SCAN_CONVERTER
    status = converter.base.generate (&converter.base, &renderer.base);
#else
    status = converter->generate (converter, &renderer.base);
#endif
    if (unlikely (status))
	goto CLEANUP_RENDERER;

    if (dst != NULL) {
	pixman_image_t *src;
	int src_x, src_y;

	src = _pixman_image_for_pattern (pattern, FALSE, extents, &src_x, &src_y);
	if (unlikely (src == NULL)) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto CLEANUP_RENDERER;
	}

	pixman_image_composite32 (_pixman_operator (op), src, mask, dst,
                                  extents->x + src_x, extents->y + src_y,
                                  0, 0, /* mask.x, mask.y */
                                  extents->x - dst_x, extents->y - dst_y,
                                  extents->width, extents->height);
	pixman_image_unref (src);
    }

 CLEANUP_RENDERER:
    if (dst != NULL)
	pixman_image_unref (mask);
 CLEANUP_CONVERTER:
#if USE_BOTOR_SCAN_CONVERTER
    converter.base.destroy (&converter.base);
#else
    converter->destroy (converter);
#endif
    return status;
}

static cairo_status_t
_clip_and_composite_polygon (cairo_image_surface_t *dst,
			     cairo_operator_t op,
			     const cairo_pattern_t *src,
			     cairo_polygon_t *polygon,
			     cairo_fill_rule_t fill_rule,
			     cairo_antialias_t antialias,
			     cairo_composite_rectangles_t *extents,
			     cairo_clip_t *clip)
{
    cairo_status_t status;

    if (polygon->num_edges == 0) {
	cairo_traps_t traps;

	if (extents->is_bounded)
	    return CAIRO_STATUS_SUCCESS;

	_cairo_traps_init (&traps);
	status = _clip_and_composite_trapezoids (dst, op, src,
						 &traps, antialias,
						 extents, clip);
	_cairo_traps_fini (&traps);

	return status;
    }

    if (_cairo_operator_bounded_by_mask(op)) {
	_cairo_box_round_to_rectangle (&polygon->extents, &extents->mask);
	if (! _cairo_rectangle_intersect (&extents->bounded, &extents->mask))
	    return CAIRO_STATUS_SUCCESS;
    }

    if (antialias != CAIRO_ANTIALIAS_NONE) {
	composite_spans_info_t info;

	info.polygon = polygon;
	info.fill_rule = fill_rule;
	info.antialias = antialias;

	status = _clip_and_composite (dst, op, src,
				      _composite_spans, &info,
				      extents, clip);
    } else {
	cairo_traps_t traps;

	_cairo_traps_init (&traps);

	/* Fall back to trapezoid fills. */
	status = _cairo_bentley_ottmann_tessellate_polygon (&traps,
							    polygon,
							    fill_rule);
	if (likely (status == CAIRO_STATUS_SUCCESS)) {
	    status = _clip_and_composite_trapezoids (dst, op, src,
						     &traps, antialias,
						     extents, clip);
	}

	_cairo_traps_fini (&traps);
    }

    return status;
}

static cairo_int_status_t
_cairo_image_surface_stroke (void			*abstract_surface,
			     cairo_operator_t		 op,
			     const cairo_pattern_t	*source,
			     cairo_path_fixed_t		*path,
			     const cairo_stroke_style_t	*style,
			     const cairo_matrix_t	*ctm,
			     const cairo_matrix_t	*ctm_inverse,
			     double			 tolerance,
			     cairo_antialias_t		 antialias,
			     cairo_clip_t		*clip)
{
    cairo_image_surface_t *surface = abstract_surface;
    cairo_composite_rectangles_t extents;
    cairo_box_t boxes_stack[32], *clip_boxes = boxes_stack;
    int num_boxes = ARRAY_LENGTH (boxes_stack);
    cairo_clip_t local_clip;
    cairo_bool_t have_clip = FALSE;
    cairo_status_t status;

    cairo_rectangle_int_t rect;
    rect.x = rect.y = 0;
    rect.width = surface->width;
    rect.height = surface->height;

    status = _cairo_composite_rectangles_init_for_stroke (&extents,
							  &rect,
							  op, source,
							  path, style, ctm,
							  clip);
    if (unlikely (status))
	return status;

    if (_cairo_clip_contains_extents (clip, &extents))
	clip = NULL;

    if (clip != NULL) {
	clip = _cairo_clip_init_copy (&local_clip, clip);
	have_clip = TRUE;
    }

    status = _cairo_clip_to_boxes (&clip, &extents, &clip_boxes, &num_boxes);
    if (unlikely (status)) {
	if (have_clip)
	    _cairo_clip_fini (&local_clip);

	return status;
    }

    status = CAIRO_INT_STATUS_UNSUPPORTED;
    if (path->is_rectilinear) {
	cairo_boxes_t boxes;

	_cairo_boxes_init (&boxes);
	_cairo_boxes_limit (&boxes, clip_boxes, num_boxes);

	status = _cairo_path_fixed_stroke_rectilinear_to_boxes (path,
								style,
								ctm,
								&boxes);
	if (likely (status == CAIRO_STATUS_SUCCESS)) {
	    status = _clip_and_composite_boxes (surface, op, source,
						&boxes, antialias,
						&extents, clip);
	}

	_cairo_boxes_fini (&boxes);
    }

    if (status == CAIRO_INT_STATUS_UNSUPPORTED) {
	cairo_polygon_t polygon;

	_cairo_polygon_init (&polygon);
	_cairo_polygon_limit (&polygon, clip_boxes, num_boxes);

	status = _cairo_path_fixed_stroke_to_polygon (path,
						      style,
						      ctm, ctm_inverse,
						      tolerance,
						      &polygon);
	if (likely (status == CAIRO_STATUS_SUCCESS)) {
	    status = _clip_and_composite_polygon (surface, op, source, &polygon,
						  CAIRO_FILL_RULE_WINDING, antialias,
						  &extents, clip);
	}

	_cairo_polygon_fini (&polygon);
    }

    if (clip_boxes != boxes_stack)
	free (clip_boxes);

    if (have_clip)
	_cairo_clip_fini (&local_clip);

    return status;
}

static cairo_int_status_t
_cairo_image_surface_fill (void				*abstract_surface,
			   cairo_operator_t		 op,
			   const cairo_pattern_t	*source,
			   cairo_path_fixed_t		*path,
			   cairo_fill_rule_t		 fill_rule,
			   double			 tolerance,
			   cairo_antialias_t		 antialias,
			   cairo_clip_t			*clip)
{
    cairo_image_surface_t *surface = abstract_surface;
    cairo_composite_rectangles_t extents;
    cairo_box_t boxes_stack[32], *clip_boxes = boxes_stack;
    cairo_clip_t local_clip;
    cairo_bool_t have_clip = FALSE;
    int num_boxes = ARRAY_LENGTH (boxes_stack);
    cairo_status_t status;

    cairo_rectangle_int_t rect;
    rect.x = rect.y = 0;
    rect.width = surface->width;
    rect.height = surface->height;

    status = _cairo_composite_rectangles_init_for_fill (&extents,
							&rect,
							op, source, path,
							clip);
    if (unlikely (status))
	return status;

    if (_cairo_clip_contains_extents (clip, &extents))
	clip = NULL;

    if (extents.is_bounded && clip != NULL) {
	cairo_clip_path_t *clip_path;

	if (((clip_path = _clip_get_single_path (clip)) != NULL) &&
	    _cairo_path_fixed_equal (&clip_path->path, path))
	{
	    clip = NULL;
	}
    }

    if (clip != NULL) {
	clip = _cairo_clip_init_copy (&local_clip, clip);
	have_clip = TRUE;
    }

    status = _cairo_clip_to_boxes (&clip, &extents, &clip_boxes, &num_boxes);
    if (unlikely (status)) {
	if (have_clip)
	    _cairo_clip_fini (&local_clip);

	return status;
    }

    if (_cairo_path_fixed_is_rectilinear_fill (path)) {
	cairo_boxes_t boxes;

	_cairo_boxes_init (&boxes);
	_cairo_boxes_limit (&boxes, clip_boxes, num_boxes);

	status = _cairo_path_fixed_fill_rectilinear_to_boxes (path,
							      fill_rule,
							      &boxes);
	if (likely (status == CAIRO_STATUS_SUCCESS)) {
	    status = _clip_and_composite_boxes (surface, op, source,
						&boxes, antialias,
						&extents, clip);
	}

	_cairo_boxes_fini (&boxes);
    } else {
	cairo_polygon_t polygon;

	assert (! path->is_empty_fill);

	_cairo_polygon_init (&polygon);
	_cairo_polygon_limit (&polygon, clip_boxes, num_boxes);

	status = _cairo_path_fixed_fill_to_polygon (path, tolerance, &polygon);
	if (likely (status == CAIRO_STATUS_SUCCESS)) {
	    status = _clip_and_composite_polygon (surface, op, source, &polygon,
						  fill_rule, antialias,
						  &extents, clip);
	}

	_cairo_polygon_fini (&polygon);
    }

    if (clip_boxes != boxes_stack)
	free (clip_boxes);

    if (have_clip)
	_cairo_clip_fini (&local_clip);

    return status;
}

typedef struct {
    cairo_scaled_font_t *font;
    cairo_glyph_t *glyphs;
    int num_glyphs;
} composite_glyphs_info_t;

static cairo_status_t
_composite_glyphs_via_mask (void			*closure,
			    pixman_image_t		*dst,
			    pixman_format_code_t	 dst_format,
			    cairo_operator_t		 op,
			    const cairo_pattern_t	*pattern,
			    int				 dst_x,
			    int				 dst_y,
			    const cairo_rectangle_int_t	*extents,
			    cairo_region_t		*clip_region)
{
    composite_glyphs_info_t *info = closure;
    cairo_scaled_font_t *font = info->font;
    cairo_glyph_t *glyphs = info->glyphs;
    int num_glyphs = info->num_glyphs;
    pixman_image_t *mask = NULL;
    pixman_image_t *src;
    pixman_image_t *white;
    pixman_format_code_t mask_format = 0; /* silence gcc */
    cairo_status_t status;
    int src_x, src_y;
    int i;

    src = _pixman_image_for_pattern (pattern, FALSE, extents, &src_x, &src_y);
    if (unlikely (src == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    white = _pixman_white_image ();
    if (unlikely (white == NULL)) {
	pixman_image_unref (src);
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    _cairo_scaled_font_freeze_cache (font);

    for (i = 0; i < num_glyphs; i++) {
	int x, y;
	cairo_image_surface_t *glyph_surface;
	cairo_scaled_glyph_t *scaled_glyph;

	status = _cairo_scaled_glyph_lookup (font, glyphs[i].index,
					     CAIRO_SCALED_GLYPH_INFO_SURFACE,
					     &scaled_glyph);

	if (unlikely (status))
	    goto CLEANUP;

	glyph_surface = scaled_glyph->surface;

	if (glyph_surface->width == 0 || glyph_surface->height == 0)
	    continue;

	/* To start, create the mask using the format from the first
	 * glyph. Later we'll deal with different formats. */
	if (mask == NULL) {
	    mask_format = glyph_surface->pixman_format;
	    mask = pixman_image_create_bits (mask_format,
					     extents->width, extents->height,
					     NULL, 0);
	    if (unlikely (mask == NULL)) {
		status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
		goto CLEANUP;
	    }

	    if (PIXMAN_FORMAT_RGB (mask_format))
		pixman_image_set_component_alpha (mask, TRUE);
	}

	/* If we have glyphs of different formats, we "upgrade" the mask
	 * to the wider of the formats. */
	if (glyph_surface->pixman_format != mask_format &&
	    PIXMAN_FORMAT_BPP (mask_format) <
	    PIXMAN_FORMAT_BPP (glyph_surface->pixman_format))
	{
	    pixman_image_t *new_mask;

	    mask_format = glyph_surface->pixman_format;
	    new_mask = pixman_image_create_bits (mask_format,
						 extents->width, extents->height,
						 NULL, 0);
	    if (unlikely (new_mask == NULL)) {
		status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
		goto CLEANUP;
	    }

	    pixman_image_composite32 (PIXMAN_OP_SRC,
                                      white, mask, new_mask,
                                      0, 0, 0, 0, 0, 0,
                                      extents->width, extents->height);

	    pixman_image_unref (mask);
	    mask = new_mask;
	    if (PIXMAN_FORMAT_RGB (mask_format))
		pixman_image_set_component_alpha (mask, TRUE);
	}

	/* round glyph locations to the nearest pixel */
	/* XXX: FRAGILE: We're ignoring device_transform scaling here. A bug? */
	x = _cairo_lround (glyphs[i].x -
			   glyph_surface->base.device_transform.x0);
	y = _cairo_lround (glyphs[i].y -
			   glyph_surface->base.device_transform.y0);
	if (glyph_surface->pixman_format == mask_format) {
	    pixman_image_composite32 (PIXMAN_OP_ADD,
                                      glyph_surface->pixman_image, NULL, mask,
                                      0, 0, 0, 0,
                                      x - extents->x, y - extents->y,
                                      glyph_surface->width,
                                      glyph_surface->height);
	} else {
	    pixman_image_composite32 (PIXMAN_OP_ADD,
                                      white, glyph_surface->pixman_image, mask,
                                      0, 0, 0, 0,
                                      x - extents->x, y - extents->y,
                                      glyph_surface->width,
                                      glyph_surface->height);
	}
    }

    pixman_image_composite32 (_pixman_operator (op),
                              src, mask, dst,
                              extents->x + src_x, extents->y + src_y,
                              0, 0,
                              extents->x - dst_x, extents->y - dst_y,
                              extents->width,     extents->height);

CLEANUP:
    _cairo_scaled_font_thaw_cache (font);
    if (mask != NULL)
	pixman_image_unref (mask);
    pixman_image_unref (src);
    pixman_image_unref (white);

    return status;
}

static cairo_status_t
_composite_glyphs (void				*closure,
		   pixman_image_t		*dst,
		   pixman_format_code_t		 dst_format,
		   cairo_operator_t		 op,
		   const cairo_pattern_t	*pattern,
		   int				 dst_x,
		   int				 dst_y,
		   const cairo_rectangle_int_t	*extents,
		   cairo_region_t		*clip_region)
{
    composite_glyphs_info_t *info = closure;
    cairo_scaled_glyph_t *glyph_cache[64];
    pixman_op_t pixman_op = _pixman_operator (op);
    pixman_image_t *src = NULL;
    int src_x = 0, src_y = 0;
    cairo_status_t status;
    int i;

    memset (glyph_cache, 0, sizeof (glyph_cache));
    status = CAIRO_STATUS_SUCCESS;

    _cairo_scaled_font_freeze_cache (info->font);
    for (i = 0; i < info->num_glyphs; i++) {
	int x, y;
	cairo_image_surface_t *glyph_surface;
	cairo_scaled_glyph_t *scaled_glyph;
	unsigned long glyph_index = info->glyphs[i].index;
	int cache_index = glyph_index % ARRAY_LENGTH (glyph_cache);

	scaled_glyph = glyph_cache[cache_index];
	if (scaled_glyph == NULL ||
	    _cairo_scaled_glyph_index (scaled_glyph) != glyph_index)
	{
	    status = _cairo_scaled_glyph_lookup (info->font, glyph_index,
						 CAIRO_SCALED_GLYPH_INFO_SURFACE,
						 &scaled_glyph);

	    if (unlikely (status))
		break;

	    glyph_cache[cache_index] = scaled_glyph;
	}

	glyph_surface = scaled_glyph->surface;
	if (glyph_surface->width && glyph_surface->height) {
	    int x1, y1, x2, y2;

	    /* round glyph locations to the nearest pixel */
	    /* XXX: FRAGILE: We're ignoring device_transform scaling here. A bug? */
	    x = _cairo_lround (info->glyphs[i].x -
			       glyph_surface->base.device_transform.x0);
	    y = _cairo_lround (info->glyphs[i].y -
			       glyph_surface->base.device_transform.y0);

	    x1 = x;
	    if (x1 < extents->x)
		x1 = extents->x;
	    x2 = x + glyph_surface->width;
	    if (x2 > extents->x + extents->width)
		x2 = extents->x + extents->width;

	    y1 = y;
	    if (y1 < extents->y)
		y1 = extents->y;
	    y2 = y + glyph_surface->height;
	    if (y2 > extents->y + extents->height)
		y2 = extents->y + extents->height;

	    if (glyph_surface->format == CAIRO_FORMAT_A8 ||
	        glyph_surface->format == CAIRO_FORMAT_A1 ||
	        (glyph_surface->format == CAIRO_FORMAT_ARGB32 &&
	         pixman_image_get_component_alpha (glyph_surface->pixman_image)))
	    {
		if (unlikely (src == NULL)) {
		    if (pattern != NULL) {
			src = _pixman_image_for_pattern (pattern, FALSE, extents, &src_x, &src_y);
			src_x -= dst_x;
			src_y -= dst_y;
		    } else {
			src = _pixman_white_image ();
		    }
		    if (unlikely (src == NULL)) {
			status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
			break;
		    }
		}

		pixman_image_composite32 (pixman_op,
					  src, glyph_surface->pixman_image, dst,
					  x1 + src_x, y1 + src_y,
					  x1 - x, y1 - y,
					  x1 - dst_x, y1 - dst_y,
					  x2 - x1, y2 - y1);
	    } else {
		pixman_image_composite32 (pixman_op,
					  glyph_surface->pixman_image, NULL, dst,
					  x1 - x, y1 - y,
					  0, 0,
					  x1 - dst_x, y1 - dst_y,
					  x2 - x1, y2 - y1);
	    }
	}
    }
    _cairo_scaled_font_thaw_cache (info->font);

    if (src != NULL)
	pixman_image_unref (src);

    return status;
}

static cairo_int_status_t
_cairo_image_surface_glyphs (void			*abstract_surface,
			     cairo_operator_t		 op,
			     const cairo_pattern_t	*source,
			     cairo_glyph_t		*glyphs,
			     int			 num_glyphs,
			     cairo_scaled_font_t	*scaled_font,
			     cairo_clip_t		*clip,
			     int *num_remaining)
{
    cairo_image_surface_t *surface = abstract_surface;
    cairo_composite_rectangles_t extents;
    composite_glyphs_info_t glyph_info;
    cairo_clip_t local_clip;
    cairo_bool_t have_clip = FALSE;
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
    // For performance reasons we don't want to use two passes for overlapping glyphs
    // on mobile
    cairo_bool_t overlap = FALSE;
#else
    cairo_bool_t overlap;
#endif
    cairo_status_t status;

    cairo_rectangle_int_t rect;
    rect.x = rect.y = 0;
    rect.width = surface->width;
    rect.height = surface->height;

    status = _cairo_composite_rectangles_init_for_glyphs (&extents,
							  &rect,
							  op, source,
							  scaled_font,
							  glyphs, num_glyphs,
							  clip,
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
							  NULL);
#else
							  &overlap);
#endif

    if (unlikely (status))
	return status;

    if (_cairo_clip_contains_rectangle (clip, &extents.mask))
	clip = NULL;

    if (clip != NULL && extents.is_bounded) {
	clip = _cairo_clip_init_copy (&local_clip, clip);
	status = _cairo_clip_rectangle (clip, &extents.bounded);
	if (unlikely (status))
	    return status;

	have_clip = TRUE;
    }

    glyph_info.font = scaled_font;
    glyph_info.glyphs = glyphs;
    glyph_info.num_glyphs = num_glyphs;

    status = _clip_and_composite (surface, op, source,
				  overlap || extents.is_bounded == 0 ? _composite_glyphs_via_mask : _composite_glyphs,
				  &glyph_info,
				  &extents, clip);

    if (have_clip)
	_cairo_clip_fini (&local_clip);

    *num_remaining = 0;
    return status;
}

static cairo_bool_t
_cairo_image_surface_get_extents (void			  *abstract_surface,
				  cairo_rectangle_int_t   *rectangle)
{
    cairo_image_surface_t *surface = abstract_surface;

    rectangle->x = 0;
    rectangle->y = 0;
    rectangle->width  = surface->width;
    rectangle->height = surface->height;

    return TRUE;
}

static void
_cairo_image_surface_get_font_options (void                  *abstract_surface,
				       cairo_font_options_t  *options)
{
    _cairo_font_options_init_default (options);

    cairo_font_options_set_hint_metrics (options, CAIRO_HINT_METRICS_ON);
    _cairo_font_options_set_round_glyph_positions (options, CAIRO_ROUND_GLYPH_POS_ON);
}

/* legacy interface kept for compatibility until surface-fallback is removed */
static cairo_status_t
_cairo_image_surface_acquire_dest_image (void                    *abstract_surface,
					 cairo_rectangle_int_t   *interest_rect,
					 cairo_image_surface_t  **image_out,
					 cairo_rectangle_int_t   *image_rect_out,
					 void                   **image_extra)
{
    cairo_image_surface_t *surface = abstract_surface;

    image_rect_out->x = 0;
    image_rect_out->y = 0;
    image_rect_out->width = surface->width;
    image_rect_out->height = surface->height;

    *image_out = surface;
    *image_extra = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_image_surface_release_dest_image (void                    *abstract_surface,
                                        cairo_rectangle_int_t   *interest_rect,
                                        cairo_image_surface_t   *image,
                                        cairo_rectangle_int_t   *image_rect,
                                        void                    *image_extra)
{
}

static cairo_status_t
_cairo_image_surface_clone_similar (void               *abstract_surface,
				    cairo_surface_t     *src,
				    int                  src_x,
				    int                  src_y,
				    int                  width,
				    int                  height,
				    int                 *clone_offset_x,
				    int                 *clone_offset_y,
				    cairo_surface_t    **clone_out)
{
    cairo_image_surface_t *surface = abstract_surface;

    if (src->backend == surface->base.backend) {
	*clone_offset_x = *clone_offset_y = 0;
	*clone_out = cairo_surface_reference (src);

	return CAIRO_STATUS_SUCCESS;
    }

    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_image_surface_composite (cairo_operator_t	 op,
				const cairo_pattern_t	*src_pattern,
				const cairo_pattern_t	*mask_pattern,
				void			*abstract_dst,
				int			 src_x,
				int			 src_y,
				int			 mask_x,
				int			 mask_y,
				int			 dst_x,
				int			 dst_y,
				unsigned int		 width,
				unsigned int		 height,
				cairo_region_t		*clip_region)
{
    cairo_image_surface_t *dst = abstract_dst;
    cairo_composite_rectangles_t extents;
    pixman_image_t *src;
    int src_offset_x, src_offset_y;
    cairo_status_t status;

    if (clip_region != NULL) {
	status = _cairo_image_surface_set_clip_region (dst, clip_region);
	if (unlikely (status))
	    return status;
    }

    extents.source.x = src_x;
    extents.source.y = src_y;
    extents.source.width  = width;
    extents.source.height = height;

    extents.mask.x = mask_x;
    extents.mask.y = mask_y;
    extents.mask.width  = width;
    extents.mask.height = height;

    extents.bounded.x = dst_x;
    extents.bounded.y = dst_y;
    extents.bounded.width  = width;
    extents.bounded.height = height;

    extents.unbounded.x = 0;
    extents.unbounded.y = 0;
    extents.unbounded.width  = dst->width;
    extents.unbounded.height = dst->height;

    if (clip_region != NULL) {
	cairo_rectangle_int_t rect;

	cairo_region_get_extents (clip_region, &rect);
	if (! _cairo_rectangle_intersect (&extents.unbounded, &rect))
	    return CAIRO_STATUS_SUCCESS;
    }

    extents.is_bounded = _cairo_operator_bounded_by_either (op);

    src = _pixman_image_for_pattern (src_pattern, FALSE, &extents.source, &src_offset_x, &src_offset_y);
    if (unlikely (src == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    status = CAIRO_STATUS_SUCCESS;
    if (mask_pattern != NULL) {
	pixman_image_t *mask;
	int mask_offset_x, mask_offset_y;

	mask = _pixman_image_for_pattern (mask_pattern, TRUE, &extents.mask, &mask_offset_x, &mask_offset_y);
	if (unlikely (mask == NULL)) {
	    pixman_image_unref (src);
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
	}

	pixman_image_composite32 (_pixman_operator (op),
                                  src, mask, dst->pixman_image,
                                  src_x + src_offset_x,
                                  src_y + src_offset_y,
                                  mask_x + mask_offset_x,
                                  mask_y + mask_offset_y,
				  dst_x, dst_y, width, height);

	pixman_image_unref (mask);
    } else {
	pixman_image_composite32 (_pixman_operator (op),
                                  src, NULL, dst->pixman_image,
                                  src_x + src_offset_x,
                                  src_y + src_offset_y,
                                  0, 0,
                                  dst_x, dst_y, width, height);
    }

    pixman_image_unref (src);

    if (! extents.is_bounded)
	status = _cairo_image_surface_fixup_unbounded (dst, &extents, NULL);

    if (clip_region != NULL)
	_cairo_image_surface_unset_clip_region (dst);

    return status;
}

static cairo_int_status_t
_cairo_image_surface_fill_rectangles (void		      *abstract_surface,
				      cairo_operator_t	       op,
				      const cairo_color_t     *color,
				      cairo_rectangle_int_t   *rects,
				      int		       num_rects)
{
    cairo_image_surface_t *surface = abstract_surface;

    pixman_color_t pixman_color;
    pixman_box32_t stack_boxes[CAIRO_STACK_ARRAY_LENGTH (pixman_box32_t)];
    pixman_box32_t *pixman_boxes = stack_boxes;
    int i;

    cairo_int_status_t status;

    if (CAIRO_INJECT_FAULT ())
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    pixman_color.red   = color->red_short;
    pixman_color.green = color->green_short;
    pixman_color.blue  = color->blue_short;
    pixman_color.alpha = color->alpha_short;

    if (num_rects > ARRAY_LENGTH (stack_boxes)) {
	pixman_boxes = _cairo_malloc_ab (num_rects, sizeof (pixman_box32_t));
	if (unlikely (pixman_boxes == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    for (i = 0; i < num_rects; i++) {
	pixman_boxes[i].x1 = rects[i].x;
	pixman_boxes[i].y1 = rects[i].y;
	pixman_boxes[i].x2 = rects[i].x + rects[i].width;
	pixman_boxes[i].y2 = rects[i].y + rects[i].height;
    }

    status = CAIRO_STATUS_SUCCESS;
    if (! pixman_image_fill_boxes (_pixman_operator (op),
                                   surface->pixman_image,
                                   &pixman_color,
                                   num_rects,
                                   pixman_boxes))
    {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    if (pixman_boxes != stack_boxes)
	free (pixman_boxes);

    return status;
}

static cairo_int_status_t
_cairo_image_surface_composite_trapezoids (cairo_operator_t	op,
					   const cairo_pattern_t *pattern,
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
    cairo_image_surface_t	*dst = abstract_dst;
    cairo_composite_rectangles_t extents;
    cairo_pattern_union_t        source_pattern;
    composite_traps_info_t	 info;
    cairo_status_t		 status;

    if (height == 0 || width == 0)
	return CAIRO_STATUS_SUCCESS;

    if (CAIRO_INJECT_FAULT ())
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    extents.source.x = src_x;
    extents.source.y = src_y;
    extents.source.width  = width;
    extents.source.height = height;

    extents.mask.x = dst_x;
    extents.mask.y = dst_y;
    extents.mask.width  = width;
    extents.mask.height = height;

    extents.bounded.x = dst_x;
    extents.bounded.y = dst_y;
    extents.bounded.width  = width;
    extents.bounded.height = height;

    extents.unbounded.x = 0;
    extents.unbounded.y = 0;
    extents.unbounded.width  = dst->width;
    extents.unbounded.height = dst->height;

    if (clip_region != NULL) {
	cairo_rectangle_int_t rect;

	cairo_region_get_extents (clip_region, &rect);
	if (! _cairo_rectangle_intersect (&extents.unbounded, &rect))
	    return CAIRO_STATUS_SUCCESS;
    }

    extents.is_bounded = _cairo_operator_bounded_by_either (op);

    if (clip_region != NULL) {
	status = _cairo_image_surface_set_clip_region (dst, clip_region);
	if (unlikely (status))
	    return status;
    }

    _cairo_pattern_init_static_copy (&source_pattern.base, pattern);
    cairo_matrix_translate (&source_pattern.base.matrix,
                            src_x - extents.bounded.x,
                            src_y - extents.bounded.y);

    info.traps = traps;
    info.num_traps = num_traps;
    info.antialias = antialias;
    status = _composite_traps (&info,
			       dst->pixman_image,
			       dst->pixman_format,
			       op,
			       &source_pattern.base,
			       0, 0,
			       &extents.bounded,
			       clip_region);

    if (status == CAIRO_STATUS_SUCCESS && ! extents.is_bounded)
	status = _cairo_image_surface_fixup_unbounded (dst, &extents, NULL);

    if (clip_region != NULL)
	_cairo_image_surface_unset_clip_region (dst);

    return status;
}

typedef struct _legacy_image_surface_span_renderer {
    cairo_span_renderer_t base;

    cairo_operator_t op;
    const cairo_pattern_t *pattern;
    cairo_antialias_t antialias;
    cairo_region_t *clip_region;

    pixman_image_t *mask;
    uint8_t *mask_data;
    uint32_t mask_stride;

    cairo_image_surface_t *dst;
    cairo_composite_rectangles_t composite_rectangles;
} legacy_image_surface_span_renderer_t;

void
_cairo_image_surface_span_render_row (
    int                                  y,
    const cairo_half_open_span_t        *spans,
    unsigned                             num_spans,
    uint8_t				*data,
    uint32_t				 stride)
{
    uint8_t *row;
    unsigned i;

    if (num_spans == 0)
	return;

    row = data + y * stride;
    for (i = 0; i < num_spans - 1; i++) {
	if (! spans[i].coverage)
	    continue;

	/* We implement setting the most common single pixel wide
	 * span case to avoid the overhead of a memset call.
	 * Open coding setting longer spans didn't show a
	 * noticeable improvement over memset.
	 */
	if (spans[i+1].x == spans[i].x + 1) {
	    row[spans[i].x] = spans[i].coverage;
	} else {
	    memset (row + spans[i].x,
		    spans[i].coverage,
		    spans[i+1].x - spans[i].x);
	}
    }
}

static cairo_status_t
_cairo_image_surface_span_renderer_render_rows (
    void				*abstract_renderer,
    int					 y,
    int					 height,
    const cairo_half_open_span_t	*spans,
    unsigned				 num_spans)
{
    legacy_image_surface_span_renderer_t *renderer = abstract_renderer;
    while (height--)
	_cairo_image_surface_span_render_row (y++, spans, num_spans, renderer->mask_data, renderer->mask_stride);
    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_image_surface_span_renderer_destroy (void *abstract_renderer)
{
    legacy_image_surface_span_renderer_t *renderer = abstract_renderer;
    if (renderer == NULL)
	return;

    pixman_image_unref (renderer->mask);

    free (renderer);
}

static cairo_status_t
_cairo_image_surface_span_renderer_finish (void *abstract_renderer)
{
    legacy_image_surface_span_renderer_t *renderer = abstract_renderer;
    cairo_composite_rectangles_t *rects = &renderer->composite_rectangles;
    cairo_image_surface_t *dst = renderer->dst;
    pixman_image_t *src;
    int src_x, src_y;
    cairo_status_t status;

    if (renderer->clip_region != NULL) {
	status = _cairo_image_surface_set_clip_region (dst, renderer->clip_region);
	if (unlikely (status))
	    return status;
    }

    src = _pixman_image_for_pattern (renderer->pattern, FALSE, &rects->bounded, &src_x, &src_y);
    if (src == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    status = CAIRO_STATUS_SUCCESS;
    pixman_image_composite32 (_pixman_operator (renderer->op),
                              src,
                              renderer->mask,
                              dst->pixman_image,
                              rects->bounded.x + src_x,
                              rects->bounded.y + src_y,
                              0, 0,
                              rects->bounded.x, rects->bounded.y,
                              rects->bounded.width, rects->bounded.height);

    if (! rects->is_bounded)
	status = _cairo_image_surface_fixup_unbounded (dst, rects, NULL);

    if (renderer->clip_region != NULL)
	 _cairo_image_surface_unset_clip_region (dst);

    return status;
}

static cairo_bool_t
_cairo_image_surface_check_span_renderer (cairo_operator_t	  op,
					  const cairo_pattern_t  *pattern,
					  void			 *abstract_dst,
					  cairo_antialias_t	  antialias)
{
    return TRUE;
    (void) op;
    (void) pattern;
    (void) abstract_dst;
    (void) antialias;
}

static cairo_span_renderer_t *
_cairo_image_surface_create_span_renderer (cairo_operator_t	 op,
					   const cairo_pattern_t  *pattern,
					   void			*abstract_dst,
					   cairo_antialias_t	 antialias,
					   const cairo_composite_rectangles_t *rects,
					   cairo_region_t *clip_region)
{
    cairo_image_surface_t *dst = abstract_dst;
    legacy_image_surface_span_renderer_t *renderer;

    renderer = calloc(1, sizeof(*renderer));
    if (unlikely (renderer == NULL))
	return _cairo_span_renderer_create_in_error (CAIRO_STATUS_NO_MEMORY);

    renderer->base.destroy = _cairo_image_surface_span_renderer_destroy;
    renderer->base.finish = _cairo_image_surface_span_renderer_finish;
    renderer->base.render_rows = _cairo_image_surface_span_renderer_render_rows;
    renderer->op = op;
    renderer->pattern = pattern;
    renderer->antialias = antialias;
    renderer->dst = dst;
    renderer->clip_region = clip_region;

    renderer->composite_rectangles = *rects;

    /* TODO: support rendering to A1 surfaces (or: go add span
     * compositing to pixman.) */
    renderer->mask = pixman_image_create_bits (PIXMAN_a8,
					       rects->bounded.width,
					       rects->bounded.height,
					       NULL, 0);
    if (renderer->mask == NULL) {
	free (renderer);
	return _cairo_span_renderer_create_in_error (CAIRO_STATUS_NO_MEMORY);
    }

    renderer->mask_stride = pixman_image_get_stride (renderer->mask);
    renderer->mask_data = (uint8_t *) pixman_image_get_data (renderer->mask) - rects->bounded.x - rects->bounded.y * renderer->mask_stride;

    return &renderer->base;
}

/**
 * _cairo_surface_is_image:
 * @surface: a #cairo_surface_t
 *
 * Checks if a surface is an #cairo_image_surface_t
 *
 * Return value: %TRUE if the surface is an image surface
 **/
cairo_bool_t
_cairo_surface_is_image (const cairo_surface_t *surface)
{
    return surface->backend == &_cairo_image_surface_backend;
}

const cairo_surface_backend_t _cairo_image_surface_backend = {
    CAIRO_SURFACE_TYPE_IMAGE,
    _cairo_image_surface_create_similar,
    _cairo_image_surface_finish,
    _cairo_image_surface_acquire_source_image,
    _cairo_image_surface_release_source_image,
    _cairo_image_surface_acquire_dest_image,
    _cairo_image_surface_release_dest_image,
    _cairo_image_surface_clone_similar,
    _cairo_image_surface_composite,
    _cairo_image_surface_fill_rectangles,
    _cairo_image_surface_composite_trapezoids,
    _cairo_image_surface_create_span_renderer,
    _cairo_image_surface_check_span_renderer,

    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_image_surface_get_extents,
    NULL, /* old_show_glyphs */
    _cairo_image_surface_get_font_options,
    NULL, /* flush */
    NULL, /* mark dirty */
    NULL, /* font_fini */
    NULL, /* glyph_fini */

    _cairo_image_surface_paint,
    _cairo_image_surface_mask,
    _cairo_image_surface_stroke,
    _cairo_image_surface_fill,
    _cairo_image_surface_glyphs,
    NULL, /* show_text_glyphs */
    NULL, /* snapshot */
    NULL, /* is_similar */
};

/* A convenience function for when one needs to coerce an image
 * surface to an alternate format. */
cairo_image_surface_t *
_cairo_image_surface_coerce (cairo_image_surface_t *surface)
{
    return _cairo_image_surface_coerce_to_format (surface,
		                                  _cairo_format_from_content (surface->base.content));
        
}

/* A convenience function for when one needs to coerce an image
 * surface to an alternate format. */
cairo_image_surface_t *
_cairo_image_surface_coerce_to_format (cairo_image_surface_t *surface,
			               cairo_format_t	      format)
{
    cairo_image_surface_t *clone;
    cairo_status_t status;

    status = surface->base.status;
    if (unlikely (status))
	return (cairo_image_surface_t *)_cairo_surface_create_in_error (status);

    if (surface->format == format)
	return (cairo_image_surface_t *)cairo_surface_reference(&surface->base);

    clone = (cairo_image_surface_t *)
	cairo_image_surface_create (format, surface->width, surface->height);
    if (unlikely (clone->base.status))
	return clone;

    pixman_image_composite32 (PIXMAN_OP_SRC,
                              surface->pixman_image, NULL, clone->pixman_image,
                              0, 0,
                              0, 0,
                              0, 0,
                              surface->width, surface->height);
    clone->base.is_clear = FALSE;

    clone->base.device_transform =
	surface->base.device_transform;
    clone->base.device_transform_inverse =
	surface->base.device_transform_inverse;

    return clone;
}

cairo_image_transparency_t
_cairo_image_analyze_transparency (cairo_image_surface_t      *image)
{
    int x, y;

    if (image->transparency != CAIRO_IMAGE_UNKNOWN)
	return image->transparency;

    if ((image->base.content & CAIRO_CONTENT_ALPHA) == 0)
	return image->transparency = CAIRO_IMAGE_IS_OPAQUE;

    if ((image->base.content & CAIRO_CONTENT_COLOR) == 0) {
	if (image->format == CAIRO_FORMAT_A1)
	    return image->transparency = CAIRO_IMAGE_HAS_BILEVEL_ALPHA;
	else
	    return image->transparency = CAIRO_IMAGE_HAS_ALPHA;
    }

    if (image->format == CAIRO_FORMAT_RGB16_565) {
	image->transparency = CAIRO_IMAGE_IS_OPAQUE;
	return CAIRO_IMAGE_IS_OPAQUE;
    }

    if (image->format != CAIRO_FORMAT_ARGB32)
	return image->transparency = CAIRO_IMAGE_HAS_ALPHA;

    image->transparency = CAIRO_IMAGE_IS_OPAQUE;
    for (y = 0; y < image->height; y++) {
	uint32_t *pixel = (uint32_t *) (image->data + y * image->stride);

	for (x = 0; x < image->width; x++, pixel++) {
	    int a = (*pixel & 0xff000000) >> 24;
	    if (a > 0 && a < 255) {
		return image->transparency = CAIRO_IMAGE_HAS_ALPHA;
	    } else if (a == 0) {
		image->transparency = CAIRO_IMAGE_HAS_BILEVEL_ALPHA;
	    }
	}
    }

    return image->transparency;
}
