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
 *	Carl D. Worth <cworth@isi.edu>
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
_cairo_image_surface_create_for_pixman_image (pixman_image_t *pixman_image)
{
    cairo_image_surface_t *surface;

    surface = malloc (sizeof (cairo_image_surface_t));
    if (surface == NULL)
	return NULL;

    _cairo_surface_init (&surface->base, &cairo_image_surface_backend);

    surface->pixman_image = pixman_image;

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

    surface = _cairo_image_surface_create_for_pixman_image (pixman_image);

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

    surface = _cairo_image_surface_create_for_pixman_image (pixman_image);

    return &surface->base;
}

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

    surface = _cairo_image_surface_create_for_pixman_image (pixman_image);

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

static cairo_image_surface_t *
_cairo_image_surface_get_image (void *abstract_surface)
{
    cairo_image_surface_t *surface = abstract_surface;

    cairo_surface_reference (&surface->base);

    return surface;
}

static cairo_status_t
_cairo_image_surface_set_image (void			*abstract_surface,
				cairo_image_surface_t	*image)
{
    if (image == abstract_surface)
	return CAIRO_STATUS_SUCCESS;

    /* XXX: This case has not yet been implemented. We'll lie for now. */
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_image_abstract_surface_set_matrix (void			*abstract_surface,
					  cairo_matrix_t	*matrix)
{
    cairo_image_surface_t *surface = abstract_surface;
    return _cairo_image_surface_set_matrix (surface, matrix);
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

static cairo_status_t
_cairo_image_abstract_surface_set_filter (void *abstract_surface, cairo_filter_t filter)
{
    cairo_image_surface_t *surface = abstract_surface;

    return _cairo_image_surface_set_filter (surface, filter);
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

static cairo_status_t
_cairo_image_abstract_surface_set_repeat (void *abstract_surface, int repeat)
{
    cairo_image_surface_t *surface = abstract_surface;
    return _cairo_image_surface_set_repeat (surface, repeat);
}

cairo_status_t
_cairo_image_surface_set_repeat (cairo_image_surface_t *surface, int repeat)
{
    pixman_image_set_repeat (surface->pixman_image, repeat);

    return CAIRO_STATUS_SUCCESS;
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
				cairo_surface_t		*generic_src,
				cairo_surface_t		*generic_mask,
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
    cairo_image_surface_t *dst = abstract_dst;
    cairo_image_surface_t *src = (cairo_image_surface_t *) generic_src;
    cairo_image_surface_t *mask = (cairo_image_surface_t *) generic_mask;

    if (generic_src->backend != dst->base.backend ||
	(generic_mask && (generic_mask->backend != dst->base.backend)))
    {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    pixman_composite (_pixman_operator (operator),
		      src->pixman_image,
		      mask ? mask->pixman_image : NULL,
		      dst->pixman_image,
		      src_x, src_y,
		      mask_x, mask_y,
		      dst_x, dst_y,
		      width, height);

    return CAIRO_STATUS_SUCCESS;
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
					   cairo_surface_t	*generic_src,
					   void			*abstract_dst,
					   int			x_src,
					   int			y_src,
					   cairo_trapezoid_t	*traps,
					   int			num_traps)
{
    cairo_image_surface_t *dst = abstract_dst;
    cairo_image_surface_t *src = (cairo_image_surface_t *) generic_src;

    if (generic_src->backend != dst->base.backend)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* XXX: The pixman_trapezoid_t cast is evil and needs to go away somehow. */
    pixman_composite_trapezoids (operator, src->pixman_image, dst->pixman_image,
				 x_src, y_src, (pixman_trapezoid_t *) traps, num_traps);

    return CAIRO_STATUS_SUCCESS;
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

static cairo_int_status_t
_cairo_image_abstract_surface_create_pattern (void *abstract_surface,
					      cairo_pattern_t *pattern,
					      cairo_box_t *box)
{
    cairo_image_surface_t *image;

    /* Fall back to general pattern creation for surface patterns. */
    if (pattern->type == CAIRO_PATTERN_SURFACE)
	return CAIRO_INT_STATUS_UNSUPPORTED;
    
    image = _cairo_pattern_get_image (pattern, box);
    if (image) {
	pattern->source = &image->base;
	
	return CAIRO_STATUS_SUCCESS;
    } else
	return CAIRO_STATUS_NO_MEMORY;
}
  
static const cairo_surface_backend_t cairo_image_surface_backend = {
    _cairo_image_surface_create_similar,
    _cairo_image_abstract_surface_destroy,
    _cairo_image_surface_pixels_per_inch,
    _cairo_image_surface_get_image,
    _cairo_image_surface_set_image,
    _cairo_image_abstract_surface_set_matrix,
    _cairo_image_abstract_surface_set_filter,
    _cairo_image_abstract_surface_set_repeat,
    _cairo_image_surface_composite,
    _cairo_image_surface_fill_rectangles,
    _cairo_image_surface_composite_trapezoids,
    _cairo_image_surface_copy_page,
    _cairo_image_surface_show_page,
    _cairo_image_abstract_surface_set_clip_region,
    _cairo_image_abstract_surface_create_pattern,
    NULL /* show_glyphs */
};
