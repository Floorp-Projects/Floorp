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
 *	Carl D. Worth <cworth@isi.edu>
 */

#include <stdlib.h>

#include "cairoint.h"

void
_cairo_surface_init (cairo_surface_t			*surface,
		     const cairo_surface_backend_t	*backend)
{
    surface->backend = backend;

    surface->ref_count = 1;

    _cairo_matrix_init (&surface->matrix);
    surface->filter = CAIRO_FILTER_NEAREST;
    surface->repeat = 0;
}

cairo_surface_t *
cairo_surface_create_for_image (char		*data,
				cairo_format_t	format,
				int		width,
				int		height,
				int		stride)
{
    return cairo_image_surface_create_for_data (data, format, width, height, stride);
}
slim_hidden_def(cairo_surface_create_for_image);

cairo_surface_t *
_cairo_surface_create_similar_scratch (cairo_surface_t	*other,
				       cairo_format_t	format,
				       int		drawable,
				       int		width,
				       int		height)
{
    if (other == NULL)
	return NULL;

    return other->backend->create_similar (other, format, drawable,
					   width, height);
}

cairo_surface_t *
cairo_surface_create_similar (cairo_surface_t	*other,
			      cairo_format_t	format,
			      int		width,
			      int		height)
{
    cairo_color_t empty;

    if (other == NULL)
	return NULL;

    _cairo_color_init (&empty);
    _cairo_color_set_rgb (&empty, 0., 0., 0.);
    _cairo_color_set_alpha (&empty, 0.);

    return _cairo_surface_create_similar_solid (other, format, width, height, &empty);
}

cairo_surface_t *
_cairo_surface_create_similar_solid (cairo_surface_t	*other,
				     cairo_format_t	format,
				     int		width,
				     int		height,
				     cairo_color_t	*color)
{
    cairo_status_t status;
    cairo_surface_t *surface;

    surface = _cairo_surface_create_similar_scratch (other, format, 1,
						     width, height);
    
    if (surface == NULL)
	surface = cairo_image_surface_create (format, width, height);
    
    status = _cairo_surface_fill_rectangle (surface,
					    CAIRO_OPERATOR_SRC, color,
					    0, 0, width, height);
    if (status) {
	cairo_surface_destroy (surface);
	return NULL;
    }

    return surface;
}

void
cairo_surface_reference (cairo_surface_t *surface)
{
    if (surface == NULL)
	return;

    surface->ref_count++;
}

void
cairo_surface_destroy (cairo_surface_t *surface)
{
    if (surface == NULL)
	return;

    surface->ref_count--;
    if (surface->ref_count)
	return;

    if (surface->backend->destroy)
	surface->backend->destroy (surface);
}
slim_hidden_def(cairo_surface_destroy);

double
_cairo_surface_pixels_per_inch (cairo_surface_t *surface)
{
    return surface->backend->pixels_per_inch (surface);
}

cairo_image_surface_t *
_cairo_surface_get_image (cairo_surface_t *surface)
{
    return surface->backend->get_image (surface);
}

cairo_status_t
_cairo_surface_set_image (cairo_surface_t *surface, cairo_image_surface_t *image)
{
    return surface->backend->set_image (surface, image);
}

cairo_status_t
cairo_surface_set_matrix (cairo_surface_t *surface, cairo_matrix_t *matrix)
{
    if (surface == NULL)
	return CAIRO_STATUS_NULL_POINTER;

    cairo_matrix_copy (&surface->matrix, matrix);

    return surface->backend->set_matrix (surface, matrix);
}
slim_hidden_def(cairo_surface_set_matrix);

cairo_status_t
cairo_surface_get_matrix (cairo_surface_t *surface, cairo_matrix_t *matrix)
{
    if (surface == NULL)
	return CAIRO_STATUS_NULL_POINTER;

    return cairo_matrix_copy (matrix, &surface->matrix);
}
slim_hidden_def(cairo_surface_get_matrix);

cairo_status_t
cairo_surface_set_filter (cairo_surface_t *surface, cairo_filter_t filter)
{
    if (surface == NULL)
	return CAIRO_STATUS_NULL_POINTER;

    surface->filter = filter;
    return surface->backend->set_filter (surface, filter);
}

cairo_filter_t
cairo_surface_get_filter (cairo_surface_t *surface)
{
    return surface->filter;
}

/* XXX: NYI
cairo_status_t
cairo_surface_clip_rectangle (cairo_surface_t *surface,
			      int x, int y,
			      int width, int height)
{

}
*/

/* XXX: NYI
cairo_status_t
cairo_surface_clip_restore (cairo_surface_t *surface);
*/

cairo_status_t
cairo_surface_set_repeat (cairo_surface_t *surface, int repeat)
{
    if (surface == NULL)
	return CAIRO_STATUS_NULL_POINTER;

    surface->repeat = repeat;

    return surface->backend->set_repeat (surface, repeat);
}
slim_hidden_def(cairo_surface_set_repeat);

cairo_status_t
_cairo_surface_composite (cairo_operator_t	operator,
			  cairo_surface_t	*src,
			  cairo_surface_t	*mask,
			  cairo_surface_t	*dst,
			  int			src_x,
			  int			src_y,
			  int			mask_x,
			  int			mask_y,
			  int			dst_x,
			  int			dst_y,
			  unsigned int		width,
			  unsigned int		height)
{
    cairo_int_status_t status;
    cairo_image_surface_t *src_image, *mask_image = 0, *dst_image;

    status = dst->backend->composite (operator,
				      src, mask, dst,
				      src_x, src_y,
				      mask_x, mask_y,
				      dst_x, dst_y,
				      width, height);
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;

    src_image = _cairo_surface_get_image (src);
    if (mask)
	mask_image = _cairo_surface_get_image (mask);
    dst_image = _cairo_surface_get_image (dst);

    dst_image->base.backend->composite (operator,
					&src_image->base,
					mask ? &mask_image->base : NULL,
					dst_image,
					src_x, src_y,
					mask_x, mask_y,
					dst_x, dst_y,
					width, height);

    status = _cairo_surface_set_image (dst, dst_image);

    cairo_surface_destroy (&src_image->base);
    if (mask)
	cairo_surface_destroy (&mask_image->base);
    cairo_surface_destroy (&dst_image->base);

    return status;
}

cairo_status_t
_cairo_surface_fill_rectangle (cairo_surface_t	*surface,
			       cairo_operator_t	operator,
			       cairo_color_t	*color,
			       int		x,
			       int		y,
			       int		width,
			       int		height)
{
    cairo_rectangle_t rect;

    rect.x = x;
    rect.y = y;
    rect.width = width;
    rect.height = height;

    return _cairo_surface_fill_rectangles (surface, operator, color, &rect, 1);
}

cairo_status_t
_cairo_surface_fill_rectangles (cairo_surface_t		*surface,
				cairo_operator_t	operator,
				const cairo_color_t	*color,
				cairo_rectangle_t	*rects,
				int			num_rects)
{
    cairo_int_status_t status;
    cairo_image_surface_t *surface_image;

    if (num_rects == 0)
	return CAIRO_STATUS_SUCCESS;

    status = surface->backend->fill_rectangles (surface,
						operator,
						color,
						rects, num_rects);
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;

    surface_image = _cairo_surface_get_image (surface);

    surface_image->base.backend->fill_rectangles (surface_image,
						  operator,
						  color,
						  rects, num_rects);

    status = _cairo_surface_set_image (surface, surface_image);

    cairo_surface_destroy (&surface_image->base);

    return status;
}

cairo_status_t
_cairo_surface_composite_trapezoids (cairo_operator_t		operator,
				     cairo_surface_t		*src,
				     cairo_surface_t		*dst,
				     int			x_src,
				     int			y_src,
				     cairo_trapezoid_t		*traps,
				     int			num_traps)
{
    cairo_int_status_t status;
    cairo_image_surface_t *src_image, *dst_image;

    status = dst->backend->composite_trapezoids (operator,
						 src, dst,
						 x_src, y_src,
						 traps, num_traps);
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;

    src_image = _cairo_surface_get_image (src);
    dst_image = _cairo_surface_get_image (dst);

    dst_image->base.backend->composite_trapezoids (operator,
						   &src_image->base,
						   dst_image,
						   x_src, y_src,
						   traps, num_traps);

    status = _cairo_surface_set_image (dst, dst_image);

    cairo_surface_destroy (&src_image->base);
    cairo_surface_destroy (&dst_image->base);

    return status;
}

cairo_status_t
_cairo_surface_copy_page (cairo_surface_t *surface)
{
    cairo_int_status_t status;

    status = surface->backend->copy_page (surface);
    /* It's fine if some backends just don't support this. */
    if (status == CAIRO_INT_STATUS_UNSUPPORTED)
	return CAIRO_STATUS_SUCCESS;
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_surface_show_page (cairo_surface_t *surface)
{
    cairo_int_status_t status;

    status = surface->backend->show_page (surface);
    /* It's fine if some backends just don't support this. */
    if (status == CAIRO_INT_STATUS_UNSUPPORTED)
	return CAIRO_STATUS_SUCCESS;
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_surface_set_clip_region (cairo_surface_t *surface, pixman_region16_t *region)
{
    return surface->backend->set_clip_region (surface, region);
}

cairo_status_t
_cairo_surface_create_pattern (cairo_surface_t *surface,
			       cairo_pattern_t *pattern,
			       cairo_box_t *box)
{
    cairo_int_status_t status;

    status = surface->backend->create_pattern (surface, pattern, box);
  
    /* The backend cannot accelerate this pattern, lets create an
       unaccelerated source instead. */
    if (status == CAIRO_INT_STATUS_UNSUPPORTED) {

	status = CAIRO_STATUS_SUCCESS;
	switch (pattern->type) {
	case CAIRO_PATTERN_LINEAR:
	case CAIRO_PATTERN_RADIAL: {
	    cairo_image_surface_t *image;
      
	    image = _cairo_pattern_get_image (pattern, box);
	    if (image) {
		pattern->source = &image->base;
        
		return CAIRO_STATUS_SUCCESS;
	    } else
		return CAIRO_STATUS_NO_MEMORY;
      
	} break;
	case CAIRO_PATTERN_SOLID:
	    pattern->source =
		_cairo_surface_create_similar_solid (surface,
						     CAIRO_FORMAT_ARGB32,
						     1, 1,
						     &pattern->color);
	    if (pattern->source) {
		cairo_surface_set_repeat (pattern->source, 1);
        
		return CAIRO_STATUS_SUCCESS;
	    } else
		return CAIRO_STATUS_NO_MEMORY;
	    break;    
	case CAIRO_PATTERN_SURFACE:
	    status = CAIRO_INT_STATUS_UNSUPPORTED;

	    /* handle pattern opacity */
	    if (pattern->color.alpha != 1.0) {
		double x = box->p1.x >> 16;
		double y = box->p1.y >> 16;
		int width = ((box->p2.x + 65535) >> 16) - (box->p1.x >> 16);
		int height = ((box->p2.y + 65535) >> 16) - (box->p1.y >> 16);
		cairo_pattern_t alpha;
        
		pattern->source =
		    cairo_surface_create_similar (surface,
						  CAIRO_FORMAT_ARGB32,
						  width, height);
		if (pattern->source) {
		    _cairo_pattern_init_solid (&alpha, 1.0, 1.0, 1.0);
		    _cairo_pattern_set_alpha (&alpha, pattern->color.alpha);
          
		    status = _cairo_surface_create_pattern (pattern->source,
							    &alpha, box);
          
		    if (status == CAIRO_STATUS_SUCCESS) {
			int save_repeat = pattern->u.surface.surface->repeat;

			if (pattern->extend == CAIRO_EXTEND_REPEAT ||
			    pattern->u.surface.surface->repeat == 1)
			    cairo_surface_set_repeat (pattern->u.surface.surface, 1);
			else
			    cairo_surface_set_repeat (pattern->u.surface.surface, 0);
			
			status =
			    _cairo_surface_composite (CAIRO_OPERATOR_OVER,
						      pattern->u.surface.surface,
						      alpha.source,
						      pattern->source,
						      0, 0, 0, 0, 0, 0,
						      width, height);

			cairo_surface_set_repeat (pattern->u.surface.surface,
						  save_repeat);
            
			if (status == CAIRO_STATUS_SUCCESS)
			    _cairo_pattern_set_source_offset (pattern, x, y);
			else
			    cairo_surface_destroy (pattern->source);
		    }
          
		    _cairo_pattern_fini (&alpha);
		}
	    }

	    if (status != CAIRO_STATUS_SUCCESS) {
		pattern->source = pattern->u.surface.surface;
		cairo_surface_reference (pattern->u.surface.surface);
		
		return CAIRO_STATUS_SUCCESS;
	    }
	    break;
	}
    }
  
    return status;
}
