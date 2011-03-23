/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2003 University of Southern California
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

#include "cairo-clip-private.h"
#include "cairo-region-private.h"

/* Limit on the width / height of an image surface in pixels.  This is
 * mainly determined by coordinates of things sent to pixman at the
 * moment being in 16.16 format. */
#define MAX_IMAGE_SIZE 32767

static cairo_bool_t
_cairo_image_surface_is_size_valid (int width, int height)
{
    return 0 <= width  &&  width <= MAX_IMAGE_SIZE &&
	   0 <= height && height <= MAX_IMAGE_SIZE;
}

static cairo_format_t
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
#if PIXMAN_VERSION >= PIXMAN_VERSION_ENCODE(0,11,9)
    case PIXMAN_x2b10g10r10:
    case PIXMAN_a2b10g10r10:
#endif
#if PIXMAN_VERSION >= PIXMAN_VERSION_ENCODE(0,14,1)
    case PIXMAN_b8g8r8x8:
    case PIXMAN_b8g8r8a8:
#endif
#if PIXMAN_VERSION >= PIXMAN_VERSION_ENCODE(0,15,16)
    case PIXMAN_x2r10g10b10:
    case PIXMAN_a2r10g10b10:
#endif
    default:
	return CAIRO_FORMAT_INVALID;
    }

    return CAIRO_FORMAT_INVALID;
}

static cairo_content_t
_cairo_content_from_pixman_format (pixman_format_code_t pixman_format)
{
    switch (pixman_format) {
    case PIXMAN_a8r8g8b8:
    case PIXMAN_a8b8g8r8:
    case PIXMAN_a1r5g5b5:
    case PIXMAN_a1b5g5r5:
    case PIXMAN_a4r4g4b4:
    case PIXMAN_a4b4g4r4:
    case PIXMAN_a2r2g2b2:
    case PIXMAN_a2b2g2r2:
    case PIXMAN_a1r1g1b1:
    case PIXMAN_a1b1g1r1:
#if PIXMAN_VERSION >= PIXMAN_VERSION_ENCODE(0,11,9)
    case PIXMAN_a2b10g10r10:
#endif
#if PIXMAN_VERSION >= PIXMAN_VERSION_ENCODE(0,14,1)
    case PIXMAN_b8g8r8a8:
#endif
#if PIXMAN_VERSION >= PIXMAN_VERSION_ENCODE(0,15,16)
    case PIXMAN_a2r10g10b10:
#endif
	return CAIRO_CONTENT_COLOR_ALPHA;
    case PIXMAN_x8r8g8b8:
    case PIXMAN_x8b8g8r8:
    case PIXMAN_r8g8b8:
    case PIXMAN_b8g8r8:
    case PIXMAN_r5g6b5:
    case PIXMAN_b5g6r5:
    case PIXMAN_x1r5g5b5:
    case PIXMAN_x1b5g5r5:
    case PIXMAN_x4r4g4b4:
    case PIXMAN_x4b4g4r4:
    case PIXMAN_r3g3b2:
    case PIXMAN_b2g3r3:
    case PIXMAN_c8:
    case PIXMAN_g8:
    case PIXMAN_r1g2b1:
    case PIXMAN_b1g2r1:
    case PIXMAN_c4:
    case PIXMAN_g4:
    case PIXMAN_g1:
    case PIXMAN_yuy2:
    case PIXMAN_yv12:
#if PIXMAN_VERSION >= PIXMAN_VERSION_ENCODE(0,11,9)
    case PIXMAN_x2b10g10r10:
#endif
#if PIXMAN_VERSION >= PIXMAN_VERSION_ENCODE(0,14,1)
    case PIXMAN_b8g8r8x8:
#endif
#if PIXMAN_VERSION >= PIXMAN_VERSION_ENCODE(0,15,16)
    case PIXMAN_x2r10g10b10:
#endif
	return CAIRO_CONTENT_COLOR;
    case PIXMAN_a8:
    case PIXMAN_a1:
    case PIXMAN_x4a4:
    case PIXMAN_a4:
	return CAIRO_CONTENT_ALPHA;
    }

    return CAIRO_CONTENT_COLOR_ALPHA;
}

cairo_surface_t *
_cairo_image_surface_create_for_pixman_image (pixman_image_t		*pixman_image,
					      pixman_format_code_t	 pixman_format)
{
    cairo_image_surface_t *surface;
    int width = pixman_image_get_width (pixman_image);
    int height = pixman_image_get_height (pixman_image);

    if (! _cairo_image_surface_is_size_valid (width, height)) {
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_SIZE));
    }

    surface = malloc (sizeof (cairo_image_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_surface_init (&surface->base, &_cairo_image_surface_backend,
			 _cairo_content_from_pixman_format (pixman_format));

    surface->pixman_image = pixman_image;

    surface->pixman_format = pixman_format;
    surface->format = _cairo_format_from_pixman_format (pixman_format);
    surface->data = (unsigned char *) pixman_image_get_data (pixman_image);
    surface->owns_data = FALSE;
    surface->transparency = CAIRO_IMAGE_UNKNOWN;

    surface->width = width;
    surface->height = height;
    surface->stride = pixman_image_get_stride (pixman_image);
    surface->depth = pixman_image_get_depth (pixman_image);

    surface->clip_region = NULL;

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

/* XXX: This function really should be eliminated. We don't really
 * want to advertise a cairo image surface that supports any possible
 * format. A minimal step would be to replace this function with one
 * that accepts a #cairo_internal_format_t rather than mask values. */
cairo_surface_t *
_cairo_image_surface_create_with_masks (unsigned char	       *data,
					cairo_format_masks_t   *masks,
					int			width,
					int			height,
					int			stride)
{
    pixman_format_code_t pixman_format;

    if (! _pixman_format_from_masks (masks, &pixman_format)) {
	fprintf (stderr,
		 "Error: Cairo %s does not yet support the requested image format:\n"
		 "\tDepth: %d\n"
		 "\tAlpha mask: 0x%08lx\n"
		 "\tRed   mask: 0x%08lx\n"
		 "\tGreen mask: 0x%08lx\n"
		 "\tBlue  mask: 0x%08lx\n"
#ifdef PACKAGE_BUGGREPORT
		 "Please file an enhancement request (quoting the above) at:\n"
		 PACKAGE_BUGREPORT"\n"
#endif
		 ,
		 cairo_version_string (),
		 masks->bpp, masks->alpha_mask,
		 masks->red_mask, masks->green_mask, masks->blue_mask);

	ASSERT_NOT_REACHED;
    }

    return _cairo_image_surface_create_with_pixman_format (data,
							   pixman_format,
							   width,
							   height,
							   stride);
}

static pixman_format_code_t
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

    pixman_image = pixman_image_create_bits (pixman_format, width ? width : 1, height ? height : 1,
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
    if (! CAIRO_CONTENT_VALID (content))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_CONTENT));

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

cairo_surface_t *
_cairo_image_surface_create_for_data_with_content (unsigned char	*data,
						   cairo_content_t	 content,
						   int			 width,
						   int			 height,
						   int			 stride)
{
    if (! CAIRO_CONTENT_VALID (content))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_CONTENT));

    return cairo_image_surface_create_for_data (data,
						_cairo_format_from_content (content),
						width, height, stride);
}

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
    return CAIRO_FORMAT_ARGB32;
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

    cairo_region_destroy (surface->clip_region);

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
_cairo_image_surface_clone_similar (void		*abstract_surface,
				    cairo_surface_t	*src,
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

static cairo_status_t
_cairo_image_surface_set_matrix (cairo_image_surface_t	*surface,
				 const cairo_matrix_t	*matrix,
				 double xc, double yc)
{
    pixman_transform_t pixman_transform;

    _cairo_matrix_to_pixman_matrix (matrix, &pixman_transform, xc, yc);

    if (! pixman_image_set_transform (surface->pixman_image, &pixman_transform))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_image_surface_set_filter (cairo_image_surface_t *surface,
				 cairo_filter_t filter)
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

    if (! pixman_image_set_filter (surface->pixman_image,
				   pixman_filter,
				   NULL, 0))
    {
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_image_surface_set_extend (cairo_image_surface_t *surface,
				 cairo_extend_t extend)
{
    pixman_repeat_t pixman_repeat;

    switch (extend) {
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

    pixman_image_set_repeat (surface->pixman_image, pixman_repeat);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_image_surface_set_component_alpha (cairo_image_surface_t *surface,
					  cairo_bool_t ca)
{
    pixman_image_set_component_alpha (surface->pixman_image, ca);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_image_surface_set_attributes (cairo_image_surface_t      *surface,
				     cairo_surface_attributes_t *attributes,
				     double xc, double yc)
{
    cairo_int_status_t status;

    status = _cairo_image_surface_set_matrix (surface, &attributes->matrix,
					      xc, yc);
    if (unlikely (status))
	return status;

    status = _cairo_image_surface_set_filter (surface, attributes->filter);
    if (unlikely (status))
	return status;

    status = _cairo_image_surface_set_extend (surface, attributes->extend);
    if (unlikely (status))
	return status;

    status = _cairo_image_surface_set_component_alpha (surface,
						       attributes->has_component_alpha);
    if (unlikely (status))
	return status;

    return CAIRO_STATUS_SUCCESS;
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
    if (region == surface->clip_region)
	return CAIRO_STATUS_SUCCESS;

    if (cairo_region_equal (surface->clip_region, region))
	return CAIRO_STATUS_SUCCESS;

    cairo_region_destroy (surface->clip_region);
    surface->clip_region = cairo_region_reference (region);

    if (! pixman_image_set_clip_region32 (surface->pixman_image,
					  region ? &region->rgn : NULL))
    {
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    return CAIRO_STATUS_SUCCESS;
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
    cairo_surface_attributes_t	 src_attr, mask_attr;
    cairo_image_surface_t	*dst = abstract_dst;
    cairo_image_surface_t	*src;
    cairo_image_surface_t	*mask;
    cairo_int_status_t		 status;

    status = _cairo_image_surface_set_clip_region (dst, clip_region);
    if (unlikely (status))
	return status;

    status = _cairo_pattern_acquire_surfaces (src_pattern, mask_pattern,
					      &dst->base,
					      src_x, src_y,
					      mask_x, mask_y,
					      width, height,
					      CAIRO_PATTERN_ACQUIRE_NONE,
					      (cairo_surface_t **) &src,
					      (cairo_surface_t **) &mask,
					      &src_attr, &mask_attr);
    if (unlikely (status))
	return status;

    status = _cairo_image_surface_set_attributes (src, &src_attr,
						  dst_x + width / 2.,
						  dst_y + height / 2.);
    if (unlikely (status))
	goto CLEANUP_SURFACES;

    /* we sometimes get destinations with transforms.
     * we're not equiped to deal with this */
    {
        static const pixman_transform_t id = {
           {{ pixman_fixed_1, 0, 0 },
            { 0, pixman_fixed_1, 0 },
            { 0, 0, pixman_fixed_1 }}
        };
        pixman_image_set_transform (dst->pixman_image, &id);
    }

    if (mask) {
	status = _cairo_image_surface_set_attributes (mask, &mask_attr,
						      dst_x + width / 2.,
						      dst_y + height / 2.);
	if (unlikely (status))
	    goto CLEANUP_SURFACES;

	pixman_image_composite (_pixman_operator (op),
				src->pixman_image,
				mask->pixman_image,
				dst->pixman_image,
				src_x + src_attr.x_offset,
				src_y + src_attr.y_offset,
				mask_x + mask_attr.x_offset,
				mask_y + mask_attr.y_offset,
				dst_x, dst_y,
				width, height);
    } else {
	pixman_image_composite (_pixman_operator (op),
				src->pixman_image,
				NULL,
				dst->pixman_image,
				src_x + src_attr.x_offset,
				src_y + src_attr.y_offset,
				0, 0,
				dst_x, dst_y,
				width, height);
    }

    if (! _cairo_operator_bounded_by_source (op)) {
	status = _cairo_surface_composite_fixup_unbounded (&dst->base,
							   &src_attr, src->width, src->height,
							   mask ? &mask_attr : NULL,
							   mask ? mask->width : 0,
							   mask ? mask->height : 0,
							   src_x, src_y,
							   mask_x, mask_y,
							   dst_x, dst_y, width, height,
							   clip_region);
    }

 CLEANUP_SURFACES:
    if (mask)
	_cairo_pattern_release_surface (mask_pattern, &mask->base, &mask_attr);

    _cairo_pattern_release_surface (src_pattern, &src->base, &src_attr);

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
    pixman_rectangle16_t stack_rects[CAIRO_STACK_ARRAY_LENGTH (pixman_rectangle16_t)];
    pixman_rectangle16_t *pixman_rects = stack_rects;
    int i;

    cairo_int_status_t status;

    if (CAIRO_INJECT_FAULT ())
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    pixman_color.red   = color->red_short;
    pixman_color.green = color->green_short;
    pixman_color.blue  = color->blue_short;
    pixman_color.alpha = color->alpha_short;

    status = _cairo_image_surface_set_clip_region (surface, NULL);
    assert (status == CAIRO_STATUS_SUCCESS);

    if (num_rects > ARRAY_LENGTH (stack_rects)) {
	pixman_rects = _cairo_malloc_ab (num_rects, sizeof (pixman_rectangle16_t));
	if (unlikely (pixman_rects == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    for (i = 0; i < num_rects; i++) {
	pixman_rects[i].x = rects[i].x;
	pixman_rects[i].y = rects[i].y;
	pixman_rects[i].width = rects[i].width;
	pixman_rects[i].height = rects[i].height;
    }

    /* XXX: pixman_fill_region() should be implemented */
    status = CAIRO_STATUS_SUCCESS;
    if (! pixman_image_fill_rectangles (_pixman_operator (op),
					surface->pixman_image,
					&pixman_color,
					num_rects,
					pixman_rects))
    {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    if (pixman_rects != stack_rects)
	free (pixman_rects);

    return status;
}

static pixman_format_code_t
_pixman_mask_format_from_antialias (cairo_antialias_t antialias)
{
    if (antialias == CAIRO_ANTIALIAS_NONE)
	return PIXMAN_a1;
    return PIXMAN_a8;
}

static void
_pixman_add_trapezoids (pixman_image_t *image,
	                int dst_x, int dst_y,
			const cairo_trapezoid_t *traps,
			int num_traps)
{
    while (num_traps--) {
	pixman_trapezoid_t trap;

	trap.top = _cairo_fixed_to_16_16 (traps->top);
	trap.bottom = _cairo_fixed_to_16_16 (traps->bottom);

	trap.left.p1.x = _cairo_fixed_to_16_16 (traps->left.p1.x);
	trap.left.p1.y = _cairo_fixed_to_16_16 (traps->left.p1.y);
	trap.left.p2.x = _cairo_fixed_to_16_16 (traps->left.p2.x);
	trap.left.p2.y = _cairo_fixed_to_16_16 (traps->left.p2.y);

	trap.right.p1.x = _cairo_fixed_to_16_16 (traps->right.p1.x);
	trap.right.p1.y = _cairo_fixed_to_16_16 (traps->right.p1.y);
	trap.right.p2.x = _cairo_fixed_to_16_16 (traps->right.p2.x);
	trap.right.p2.y = _cairo_fixed_to_16_16 (traps->right.p2.y);

	pixman_rasterize_trapezoid (image, &trap, -dst_x, -dst_y);

	traps++;
    }
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
    cairo_surface_attributes_t	attributes;
    cairo_image_surface_t	*dst = abstract_dst;
    cairo_image_surface_t	*src;
    cairo_int_status_t		 status;
    pixman_image_t		*mask;

    if (height == 0 || width == 0)
	return CAIRO_STATUS_SUCCESS;

    if (CAIRO_INJECT_FAULT ())
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    /* Special case adding trapezoids onto a mask surface; we want to avoid
     * creating an intermediate temporary mask unnecessarily.
     *
     * We make the assumption here that the portion of the trapezoids
     * contained within the surface is bounded by [dst_x,dst_y,width,height];
     * the Cairo core code passes bounds based on the trapezoid extents.
     *
     * Currently the check clip_region == NULL is needed for correct
     * functioning, since pixman_add_trapezoids() doesn't obey the
     * surface clip, which is a libpixman bug , but there's no harm in
     * falling through to the general case when the surface is clipped
     * since libpixman would have to generate an intermediate mask anyways.
     */
    if (op == CAIRO_OPERATOR_ADD &&
	clip_region == NULL &&
	_cairo_pattern_is_opaque_solid (pattern) &&
	dst->base.content == CAIRO_CONTENT_ALPHA &&
	antialias != CAIRO_ANTIALIAS_NONE)
    {
	_pixman_add_trapezoids (dst->pixman_image, 0, 0, traps, num_traps);
	return CAIRO_STATUS_SUCCESS;
    }

    status = _cairo_image_surface_set_clip_region (dst, clip_region);
    if (unlikely (status))
	return status;

    status = _cairo_pattern_acquire_surface (pattern, &dst->base,
					     src_x, src_y, width, height,
					     CAIRO_PATTERN_ACQUIRE_NONE,
					     (cairo_surface_t **) &src,
					     &attributes);
    if (unlikely (status))
	return status;

    status = _cairo_image_surface_set_attributes (src, &attributes,
						  dst_x + width / 2.,
						  dst_y + height / 2.);
    if (unlikely (status))
	goto CLEANUP_SOURCE;

    mask = pixman_image_create_bits (_pixman_mask_format_from_antialias (antialias),
	                             width, height, NULL, 0);
    if (unlikely (mask == NULL)) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto CLEANUP_SOURCE;
    }

    _pixman_add_trapezoids (mask, dst_x, dst_y, traps, num_traps);

    pixman_image_composite (_pixman_operator (op),
			    src->pixman_image,
			    mask,
			    dst->pixman_image,
			    src_x + attributes.x_offset,
			    src_y + attributes.y_offset,
			    0, 0,
			    dst_x, dst_y,
			    width, height);

    pixman_image_unref (mask);

    if (! _cairo_operator_bounded_by_mask (op)) {
	status = _cairo_surface_composite_shape_fixup_unbounded (&dst->base,
								 &attributes,
								 src->width, src->height,
								 width, height,
								 src_x, src_y,
								 0, 0,
								 dst_x, dst_y, width, height,
								 clip_region);
    }

 CLEANUP_SOURCE:
    _cairo_pattern_release_surface (pattern, &src->base, &attributes);

    return status;
}

typedef struct _cairo_image_surface_span_renderer {
    cairo_span_renderer_t base;

    cairo_operator_t op;
    const cairo_pattern_t *pattern;
    cairo_antialias_t antialias;

    uint8_t *mask_data;
    uint32_t mask_stride;

    cairo_image_surface_t *src;
    cairo_surface_attributes_t src_attributes;
    cairo_image_surface_t *mask;
    cairo_image_surface_t *dst;
    cairo_composite_rectangles_t composite_rectangles;
} cairo_image_surface_span_renderer_t;

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
    cairo_image_surface_span_renderer_t *renderer = abstract_renderer;
    while (height--)
	_cairo_image_surface_span_render_row (y++, spans, num_spans, renderer->mask_data, renderer->mask_stride);
    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_image_surface_span_renderer_destroy (void *abstract_renderer)
{
    cairo_image_surface_span_renderer_t *renderer = abstract_renderer;
    if (!renderer) return;

    if (renderer->src != NULL) {
	_cairo_pattern_release_surface (renderer->pattern,
					&renderer->src->base,
					&renderer->src_attributes);
    }

    if (renderer->mask != NULL)
	cairo_surface_destroy (&renderer->mask->base);

    free (renderer);
}

static cairo_status_t
_cairo_image_surface_span_renderer_finish (void *abstract_renderer)
{
    cairo_image_surface_span_renderer_t *renderer = abstract_renderer;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    if (renderer->src == NULL || renderer->mask == NULL)
	return CAIRO_STATUS_SUCCESS;

    status = cairo_surface_status (&renderer->mask->base);
    if (status == CAIRO_STATUS_SUCCESS) {
	cairo_composite_rectangles_t *rects = &renderer->composite_rectangles;
	cairo_image_surface_t *src = renderer->src;
	cairo_image_surface_t *dst = renderer->dst;
	cairo_surface_attributes_t *src_attributes = &renderer->src_attributes;
	int width = rects->width;
	int height = rects->height;

	pixman_image_composite (_pixman_operator (renderer->op),
				src->pixman_image,
				renderer->mask->pixman_image,
				dst->pixman_image,
				rects->src.x + src_attributes->x_offset,
				rects->src.y + src_attributes->y_offset,
				0, 0,		/* mask.x, mask.y */
				rects->dst.x, rects->dst.y,
				width, height);

	if (! _cairo_operator_bounded_by_mask (renderer->op)) {
	    status = _cairo_surface_composite_shape_fixup_unbounded (
		&dst->base,
		src_attributes,
		src->width, src->height,
		width, height,
		rects->src.x, rects->src.y,
		0, 0,		/* mask.x, mask.y */
		rects->dst.x, rects->dst.y,
		width, height,
		dst->clip_region);
	}
    }
    if (status != CAIRO_STATUS_SUCCESS)
	return _cairo_span_renderer_set_error (abstract_renderer,
					       status);
    return CAIRO_STATUS_SUCCESS;
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
    cairo_image_surface_span_renderer_t *renderer = calloc(1, sizeof(*renderer));
    cairo_status_t status;
    int width = rects->width;
    int height = rects->height;

    status = _cairo_image_surface_set_clip_region (dst, clip_region);
    if (unlikely (status))
	return _cairo_span_renderer_create_in_error (status);

    if (renderer == NULL)
	return _cairo_span_renderer_create_in_error (CAIRO_STATUS_NO_MEMORY);

    renderer->base.destroy = _cairo_image_surface_span_renderer_destroy;
    renderer->base.finish = _cairo_image_surface_span_renderer_finish;
    renderer->base.render_rows = _cairo_image_surface_span_renderer_render_rows;
    renderer->op = op;
    renderer->pattern = pattern;
    renderer->antialias = antialias;
    renderer->dst = dst;

    renderer->composite_rectangles = *rects;

    status = _cairo_pattern_acquire_surface (
	renderer->pattern, &renderer->dst->base,
	rects->src.x, rects->src.y,
	width, height,
	CAIRO_PATTERN_ACQUIRE_NONE,
	(cairo_surface_t **) &renderer->src,
	&renderer->src_attributes);
    if (status)
	goto unwind;

    status = _cairo_image_surface_set_attributes (
	renderer->src, &renderer->src_attributes,
	rects->dst.x + width/2, rects->dst.y + height/2);
    if (status)
	goto unwind;

    /* TODO: support rendering to A1 surfaces (or: go add span
     * compositing to pixman.) */
    renderer->mask = (cairo_image_surface_t *)
	cairo_image_surface_create (CAIRO_FORMAT_A8,
				    width, height);

    status = cairo_surface_status (&renderer->mask->base);

 unwind:
    if (status != CAIRO_STATUS_SUCCESS) {
	_cairo_image_surface_span_renderer_destroy (renderer);
	return _cairo_span_renderer_create_in_error (status);
    }

    renderer->mask_data = renderer->mask->data - rects->mask.x - rects->mask.y * renderer->mask->stride;
    renderer->mask_stride = renderer->mask->stride;
    return &renderer->base;
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

    NULL, /* paint */
    NULL, /* mask */
    NULL, /* stroke */
    NULL, /* fill */
    NULL, /* show_glyphs */
    NULL, /* snapshot */
    NULL, /* is_similar */
};

/* A convenience function for when one needs to coerce an image
 * surface to an alternate format. */
cairo_image_surface_t *
_cairo_image_surface_coerce (cairo_image_surface_t	*surface,
			     cairo_format_t		 format)
{
    cairo_image_surface_t *clone;
    cairo_surface_pattern_t pattern;
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

    _cairo_pattern_init_for_surface (&pattern, &surface->base);
    status = _cairo_surface_paint (&clone->base,
				   CAIRO_OPERATOR_SOURCE,
				   &pattern.base,
				   NULL);
    _cairo_pattern_fini (&pattern.base);

    if (unlikely (status)) {
	cairo_surface_destroy (&clone->base);
	return (cairo_image_surface_t *)_cairo_surface_create_in_error (status);
    }

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
