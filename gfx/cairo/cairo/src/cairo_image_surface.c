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

static const cairo_surface_backend_t cairo_image_surface_backend;

static int
_cairo_format_bpp (cairo_format_t format)
{
    switch (format) {
    case CAIRO_FORMAT_A1:
	return 1;
    case CAIRO_FORMAT_A8:
	return 8;
    case CAIRO_FORMAT_RGB24:
    case CAIRO_FORMAT_ARGB32:
    default:
	return 32;
    }
}

static cairo_image_surface_t *
_cairo_image_surface_create_for_pixman_image (pixman_image_t *pixman_image,
					      cairo_format_t  format)
{
    cairo_image_surface_t *surface;

    surface = malloc (sizeof (cairo_image_surface_t));
    if (surface == NULL)
	return NULL;

    _cairo_surface_init (&surface->base, &cairo_image_surface_backend);

    surface->pixman_image = pixman_image;

    surface->format = format;
    surface->data = (char *) pixman_image_get_data (pixman_image);
    surface->owns_data = 0;

    surface->width = pixman_image_get_width (pixman_image);
    surface->height = pixman_image_get_height (pixman_image);
    surface->stride = pixman_image_get_stride (pixman_image);
    surface->depth = pixman_image_get_depth (pixman_image);

    return surface;
}

cairo_image_surface_t *
_cairo_image_surface_create_with_masks (char			*data,
					cairo_format_masks_t	*format,
					int			width,
					int			height,
					int			stride)
{
    cairo_image_surface_t *surface;
    pixman_format_t *pixman_format;
    pixman_image_t *pixman_image;

    pixman_format = pixman_format_create_masks (format->bpp,
						format->alpha_mask,
						format->red_mask,
						format->green_mask,
						format->blue_mask);

    if (pixman_format == NULL)
	return NULL;

    pixman_image = pixman_image_create_for_data ((pixman_bits_t *) data, pixman_format,
						 width, height, format->bpp, stride);

    pixman_format_destroy (pixman_format);

    if (pixman_image == NULL)
	return NULL;

    surface = _cairo_image_surface_create_for_pixman_image (pixman_image,
							    (cairo_format_t)-1);

    return surface;
}

static pixman_format_t *
_create_pixman_format (cairo_format_t format)
{
    switch (format) {
    case CAIRO_FORMAT_A1:
	return pixman_format_create (PIXMAN_FORMAT_NAME_A1);
	break;
    case CAIRO_FORMAT_A8:
	return pixman_format_create (PIXMAN_FORMAT_NAME_A8);
	break;
    case CAIRO_FORMAT_RGB24:
	return pixman_format_create (PIXMAN_FORMAT_NAME_RGB24);
	break;
    case CAIRO_FORMAT_ARGB32:
    default:
	return pixman_format_create (PIXMAN_FORMAT_NAME_ARGB32);
	break;
    }
}

/**
 * cairo_image_surface_create:
 * @format: format of pixels in the surface to create 
 * @width: width of the surface, in pixels
 * @height: height of the surface, in pixels
 * 
 * Creates an image surface of the specified format and
 * dimensions. The initial contents of the surface is undefined; you
 * must explicitely clear the buffer, using, for example,
 * cairo_rectangle() and cairo_fill() if you want it cleared.
 *
 * Return value: the newly created surface, or %NULL if it couldn't
 *   be created because of lack of memory
 **/
cairo_surface_t *
cairo_image_surface_create (cairo_format_t	format,
			    int			width,
			    int			height)
{
    cairo_image_surface_t *surface;
    pixman_format_t *pixman_format;
    pixman_image_t *pixman_image;

    pixman_format = _create_pixman_format (format);
    if (pixman_format == NULL)
	return NULL;

    pixman_image = pixman_image_create (pixman_format, width, height);

    pixman_format_destroy (pixman_format);

    if (pixman_image == NULL)
	return NULL;

    surface = _cairo_image_surface_create_for_pixman_image (pixman_image, format);

    return &surface->base;
}

/**
 * cairo_image_surface_create_for_data:
 * @data: a pointer to a buffer supplied by the application
 *    in which to write contents.
 * @format: the format of pixels in the buffer
 * @width: the width of the image to be stored in the buffer
 * @height: the height of the image to be stored in the buffer
 * @stride: the number of bytes between the start of rows
 *   in the buffer. Having this be specified separate from @width
 *   allows for padding at the end of rows, or for writing
 *   to a subportion of a larger image.
 * 
 * Creates an image surface for the provided pixel data. The output
 * buffer must be kept around until the #cairo_surface_t is destroyed
 * or cairo_surface_finish() is called on the surface.  The initial
 * contents of @buffer will be used as the inital image contents; you
 * must explicitely clear the buffer, using, for example,
 * cairo_rectangle() and cairo_fill() if you want it cleared.
 *
 * Return value: the newly created surface, or %NULL if it couldn't
 *   be created because of lack of memory
 **/
cairo_surface_t *
cairo_image_surface_create_for_data (char		*data,
				     cairo_format_t	format,
				     int		width,
				     int		height,
				     int		stride)
{
    cairo_image_surface_t *surface;
    pixman_format_t *pixman_format;
    pixman_image_t *pixman_image;

    pixman_format = _create_pixman_format (format);
    if (pixman_format == NULL)
	return NULL;
    
    pixman_image = pixman_image_create_for_data ((pixman_bits_t *) data, pixman_format,
						 width, height,
						 _cairo_format_bpp (format),
						 stride);

    pixman_format_destroy (pixman_format);

    if (pixman_image == NULL)
	return NULL;

    surface = _cairo_image_surface_create_for_pixman_image (pixman_image, format);

    return &surface->base;
}

static cairo_surface_t *
_cairo_image_surface_create_similar (void		*abstract_src,
				     cairo_format_t	format,
				     int		drawable,
				     int		width,
				     int		height)
{
    return cairo_image_surface_create (format, width, height);
}

static void
_cairo_image_abstract_surface_destroy (void *abstract_surface)
{
    cairo_image_surface_t *surface = abstract_surface;

    if (surface->pixman_image)
	pixman_image_destroy (surface->pixman_image);

    if (surface->owns_data) {
	free (surface->data);
	surface->data = NULL;
    }

    free (surface);
}

void
_cairo_image_surface_assume_ownership_of_data (cairo_image_surface_t *surface)
{
    surface->owns_data = 1;
}

static double
_cairo_image_surface_pixels_per_inch (void *abstract_surface)
{
    /* XXX: We'll want a way to let the user set this. */
    return 96.0;
}

static cairo_status_t
_cairo_image_surface_acquire_source_image (void                    *abstract_surface,
					   cairo_image_surface_t  **image_out,
					   void                   **image_extra)
{
    *image_out = abstract_surface;
    
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
					 cairo_rectangle_t       *interest_rect,
					 cairo_image_surface_t  **image_out,
					 cairo_rectangle_t       *image_rect_out,
					 void                   **image_extra)
{
    cairo_image_surface_t *surface = abstract_surface;
    
    image_rect_out->x = 0;
    image_rect_out->y = 0;
    image_rect_out->width = surface->width;
    image_rect_out->height = surface->height;

    *image_out = surface;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_image_surface_release_dest_image (void                   *abstract_surface,
					 cairo_rectangle_t      *interest_rect,
					 cairo_image_surface_t  *image,
					 cairo_rectangle_t      *image_rect,
					 void                   *image_extra)
{
}

static cairo_status_t
_cairo_image_surface_clone_similar (void		*abstract_surface,
				    cairo_surface_t	*src,
				    cairo_surface_t    **clone_out)
{
    cairo_image_surface_t *surface = abstract_surface;

    if (src->backend == surface->base.backend) {
	*clone_out = src;
	cairo_surface_reference (src);

	return CAIRO_STATUS_SUCCESS;
    }	
    
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

cairo_status_t
_cairo_image_surface_set_matrix (cairo_image_surface_t	*surface,
				 cairo_matrix_t		*matrix)
{
    pixman_transform_t pixman_transform;

    pixman_transform.matrix[0][0] = _cairo_fixed_from_double (matrix->m[0][0]);
    pixman_transform.matrix[0][1] = _cairo_fixed_from_double (matrix->m[1][0]);
    pixman_transform.matrix[0][2] = _cairo_fixed_from_double (matrix->m[2][0]);

    pixman_transform.matrix[1][0] = _cairo_fixed_from_double (matrix->m[0][1]);
    pixman_transform.matrix[1][1] = _cairo_fixed_from_double (matrix->m[1][1]);
    pixman_transform.matrix[1][2] = _cairo_fixed_from_double (matrix->m[2][1]);

    pixman_transform.matrix[2][0] = 0;
    pixman_transform.matrix[2][1] = 0;
    pixman_transform.matrix[2][2] = _cairo_fixed_from_double (1);

    pixman_image_set_transform (surface->pixman_image, &pixman_transform);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_image_surface_set_filter (cairo_image_surface_t *surface, cairo_filter_t filter)
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
    default:
	pixman_filter = PIXMAN_FILTER_BEST;
    }

    pixman_image_set_filter (surface->pixman_image, pixman_filter);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_image_surface_set_repeat (cairo_image_surface_t *surface, int repeat)
{
    pixman_image_set_repeat (surface->pixman_image, repeat);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_image_surface_set_attributes (cairo_image_surface_t      *surface,
				     cairo_surface_attributes_t *attributes)
{
    cairo_int_status_t status;
    
    status = _cairo_image_surface_set_matrix (surface, &attributes->matrix);
    if (status)
	return status;

    switch (attributes->extend) {
    case CAIRO_EXTEND_NONE:
	_cairo_image_surface_set_repeat (surface, 0);
	break;
    case CAIRO_EXTEND_REPEAT:
	_cairo_image_surface_set_repeat (surface, 1);
	break;
    case CAIRO_EXTEND_REFLECT:
	/* XXX: Obviously wrong. */
	_cairo_image_surface_set_repeat (surface, 1);
	break;
    }
    
    status = _cairo_image_surface_set_filter (surface, attributes->filter);
    
    return status;
}

static pixman_operator_t
_pixman_operator (cairo_operator_t operator)
{
    switch (operator) {
    case CAIRO_OPERATOR_CLEAR:
	return PIXMAN_OPERATOR_CLEAR;
    case CAIRO_OPERATOR_SRC:
	return PIXMAN_OPERATOR_SRC;
    case CAIRO_OPERATOR_DST:
	return PIXMAN_OPERATOR_DST;
    case CAIRO_OPERATOR_OVER:
	return PIXMAN_OPERATOR_OVER;
    case CAIRO_OPERATOR_OVER_REVERSE:
	return PIXMAN_OPERATOR_OVER_REVERSE;
    case CAIRO_OPERATOR_IN:
	return PIXMAN_OPERATOR_IN;
    case CAIRO_OPERATOR_IN_REVERSE:
	return PIXMAN_OPERATOR_IN_REVERSE;
    case CAIRO_OPERATOR_OUT:
	return PIXMAN_OPERATOR_OUT;
    case CAIRO_OPERATOR_OUT_REVERSE:
	return PIXMAN_OPERATOR_OUT_REVERSE;
    case CAIRO_OPERATOR_ATOP:
	return PIXMAN_OPERATOR_ATOP;
    case CAIRO_OPERATOR_ATOP_REVERSE:
	return PIXMAN_OPERATOR_ATOP_REVERSE;
    case CAIRO_OPERATOR_XOR:
	return PIXMAN_OPERATOR_XOR;
    case CAIRO_OPERATOR_ADD:
	return PIXMAN_OPERATOR_ADD;
    case CAIRO_OPERATOR_SATURATE:
	return PIXMAN_OPERATOR_SATURATE;
    default:
	return PIXMAN_OPERATOR_OVER;
    }
}

static cairo_int_status_t
_cairo_image_surface_composite (cairo_operator_t	operator,
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
    cairo_image_surface_t	*dst = abstract_dst;
    cairo_image_surface_t	*src;
    cairo_image_surface_t	*mask;
    cairo_int_status_t		status;

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
    
    status = _cairo_image_surface_set_attributes (src, &src_attr);
    if (CAIRO_OK (status))
    {
	if (mask)
	{
	    status = _cairo_image_surface_set_attributes (mask, &mask_attr);
	    if (CAIRO_OK (status))
		pixman_composite (_pixman_operator (operator),
				  src->pixman_image,
				  mask->pixman_image,
				  dst->pixman_image,
				  src_x + src_attr.x_offset,
				  src_y + src_attr.y_offset,
				  mask_x + mask_attr.x_offset,
				  mask_y + mask_attr.y_offset,
				  dst_x, dst_y,
				  width, height);
	}
	else
	{
	    pixman_composite (_pixman_operator (operator),
			      src->pixman_image,
			      NULL,
			      dst->pixman_image,
			      src_x + src_attr.x_offset,
			      src_y + src_attr.y_offset,
			      0, 0,
			      dst_x, dst_y,
			      width, height);
	}
    }

    if (mask)
	_cairo_pattern_release_surface (&dst->base, &mask->base, &mask_attr);
    
    _cairo_pattern_release_surface (&dst->base, &src->base, &src_attr);
    
    return status;
}

static cairo_int_status_t
_cairo_image_surface_fill_rectangles (void			*abstract_surface,
				      cairo_operator_t		operator,
				      const cairo_color_t	*color,
				      cairo_rectangle_t		*rects,
				      int			num_rects)
{
    cairo_image_surface_t *surface = abstract_surface;

    pixman_color_t pixman_color;

    pixman_color.red   = color->red_short;
    pixman_color.green = color->green_short;
    pixman_color.blue  = color->blue_short;
    pixman_color.alpha = color->alpha_short;

    /* XXX: The pixman_rectangle_t cast is evil... it needs to go away somehow. */
    pixman_fill_rectangles (_pixman_operator(operator), surface->pixman_image,
			    &pixman_color, (pixman_rectangle_t *) rects, num_rects);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_image_surface_composite_trapezoids (cairo_operator_t	operator,
					   cairo_pattern_t	*pattern,
					   void			*abstract_dst,
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
    cairo_image_surface_t	*dst = abstract_dst;
    cairo_image_surface_t	*src;
    cairo_int_status_t		status;
    int				render_reference_x, render_reference_y;
    int				render_src_x, render_src_y;

    status = _cairo_pattern_acquire_surface (pattern, &dst->base,
					     src_x, src_y, width, height,
					     (cairo_surface_t **) &src,
					     &attributes);
    if (status)
	return status;

    if (traps[0].left.p1.y < traps[0].left.p2.y) {
	render_reference_x = _cairo_fixed_integer_floor (traps[0].left.p1.x);
	render_reference_y = _cairo_fixed_integer_floor (traps[0].left.p1.y);
    } else {
	render_reference_x = _cairo_fixed_integer_floor (traps[0].left.p2.x);
	render_reference_y = _cairo_fixed_integer_floor (traps[0].left.p2.y);
    }

    render_src_x = src_x + render_reference_x - dst_x;
    render_src_y = src_y + render_reference_y - dst_y;

    /* XXX: The pixman_trapezoid_t cast is evil and needs to go away
     * somehow. */
    status = _cairo_image_surface_set_attributes (src, &attributes);
    if (CAIRO_OK (status))
	pixman_composite_trapezoids (operator,
				     src->pixman_image,
				     dst->pixman_image,
				     render_src_x + attributes.x_offset,
				     render_src_y + attributes.y_offset,
				     (pixman_trapezoid_t *) traps, num_traps);

    _cairo_pattern_release_surface (&dst->base, &src->base, &attributes);

    return status;
}

static cairo_int_status_t
_cairo_image_surface_copy_page (void *abstract_surface)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_image_surface_show_page (void *abstract_surface)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_image_abstract_surface_set_clip_region (void *abstract_surface,
					       pixman_region16_t *region)
{
    cairo_image_surface_t *surface = (cairo_image_surface_t *) abstract_surface;

    return _cairo_image_surface_set_clip_region (surface, region);
}

cairo_int_status_t
_cairo_image_surface_set_clip_region (cairo_image_surface_t *surface,
				      pixman_region16_t *region)
{
    if (region) {
        pixman_region16_t *rcopy;

        rcopy = pixman_region_create();
        /* pixman_image_set_clip_region expects to take ownership of the
         * passed-in region, so we create a copy to give it. */
	/* XXX: I think that's probably a bug in pixman. But its
	 * memory management issues need auditing anyway, so a
	 * workaround like this is fine for now. */
        pixman_region_copy (rcopy, region);
        pixman_image_set_clip_region (surface->pixman_image, rcopy);
    } else {
        pixman_image_set_clip_region (surface->pixman_image, region);
    }

    return CAIRO_STATUS_SUCCESS;
}

/**
 * _cairo_surface_is_image:
 * @surface: a #cairo_surface_t
 * 
 * Checks if a surface is an #cairo_image_surface_t
 * 
 * Return value: True if the surface is an image surface
 **/
int
_cairo_surface_is_image (cairo_surface_t *surface)
{
    return surface->backend == &cairo_image_surface_backend;
}

static const cairo_surface_backend_t cairo_image_surface_backend = {
    _cairo_image_surface_create_similar,
    _cairo_image_abstract_surface_destroy,
    _cairo_image_surface_pixels_per_inch,
    _cairo_image_surface_acquire_source_image,
    _cairo_image_surface_release_source_image,
    _cairo_image_surface_acquire_dest_image,
    _cairo_image_surface_release_dest_image,
    _cairo_image_surface_clone_similar,
    _cairo_image_surface_composite,
    _cairo_image_surface_fill_rectangles,
    _cairo_image_surface_composite_trapezoids,
    _cairo_image_surface_copy_page,
    _cairo_image_surface_show_page,
    _cairo_image_abstract_surface_set_clip_region,
    NULL /* show_glyphs */
};
