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

/**
 * _cairo_surface_acquire_source_image:
 * @surface: a #cairo_surface_t
 * @image_out: location to store a pointer to an image surface that includes at least
 *    the intersection of @interest_rect with the visible area of @surface.
 *    This surface could be @surface itself, a surface held internal to @surface,
 *    or it could be a new surface with a copy of the relevant portion of @surface.
 * @image_extra: location to store image specific backend data
 * 
 * Gets an image surface to use when drawing as a fallback when drawing with
 * @surface as a source. _cairo_surface_release_source_image() must be called
 * when finished.
 * 
 * Return value: %CAIRO_STATUS_SUCCESS if a an image was stored in @image_out.
 * %CAIRO_INT_STATUS_UNSUPPORTED if an image cannot be retrieved for the specified
 * surface. Or %CAIRO_STATUS_NO_MEMORY.
 **/
cairo_private cairo_status_t
_cairo_surface_acquire_source_image (cairo_surface_t         *surface,
				     cairo_image_surface_t  **image_out,
				     void                   **image_extra)
{
    return surface->backend->acquire_source_image (surface,  image_out, image_extra);
}

/**
 * _cairo_surface_release_source_image:
 * @surface: a #cairo_surface_t
 * @image_extra: same as return from the matching _cairo_surface_acquire_dest_image()
 * 
 * Releases any resources obtained with _cairo_surface_acquire_source_image()
 **/
cairo_private void
_cairo_surface_release_source_image (cairo_surface_t        *surface,
				     cairo_image_surface_t  *image,
				     void                   *image_extra)
{
    surface->backend->release_source_image (surface, image, image_extra);
}

/**
 * _cairo_surface_acquire_dest_image:
 * @surface: a #cairo_surface_t
 * @interest_rect: area of @surface for which fallback drawing is being done.
 *    A value of %NULL indicates that the entire surface is desired.
 * @image_out: location to store a pointer to an image surface that includes at least
 *    the intersection of @interest_rect with the visible area of @surface.
 *    This surface could be @surface itself, a surface held internal to @surface,
 *    or it could be a new surface with a copy of the relevant portion of @surface.
 * @image_rect: location to store area of the original surface occupied 
 *    by the surface stored in @image.
 * @image_extra: location to store image specific backend data
 * 
 * Retrieves a local image for a surface for implementing a fallback drawing
 * operation. After calling this function, the implementation of the fallback
 * drawing operation draws the primitive to the surface stored in @image_out
 * then calls _cairo_surface_release_dest_fallback(),
 * which, if a temporary surface was created, copies the bits back to the
 * main surface and frees the temporary surface.
 * 
 * Return value: %CAIRO_STATUS_SUCCESS or %CAIRO_STATUS_NO_MEMORY.
 *  %CAIRO_INT_STATUS_UNSUPPORTED can be returned but this will mean that
 *  the backend can't draw with fallbacks. It's possible for the routine
 *  to store NULL in @local_out and return %CAIRO_STATUS_SUCCESS;
 *  that indicates that no part of @interest_rect is visible, so no drawing
 *  is necessary. _cairo_surface_release_dest_fallback() should not be called in that
 *  case.
 **/
cairo_status_t
_cairo_surface_acquire_dest_image (cairo_surface_t         *surface,
				   cairo_rectangle_t       *interest_rect,
				   cairo_image_surface_t  **image_out,
				   cairo_rectangle_t       *image_rect,
				   void                   **image_extra)
{
    return surface->backend->acquire_dest_image (surface, interest_rect,
						 image_out, image_rect, image_extra);
}

/**
 * _cairo_surface_end_fallback:
 * @surface: a #cairo_surface_t
 * @interest_rect: same as passed to the matching _cairo_surface_acquire_dest_image()
 * @image: same as returned from the matching _cairo_surface_acquire_dest_image()
 * @image_rect: same as returned from the matching _cairo_surface_acquire_dest_image()
 * @image_extra: same as return from the matching _cairo_surface_acquire_dest_image()
 * 
 * Finishes the operation started with _cairo_surface_acquire_dest_image(), by, if
 * necessary, copying the image from @image back to @surface and freeing any
 * resources that were allocated.
 **/
void
_cairo_surface_release_dest_image (cairo_surface_t        *surface,
				   cairo_rectangle_t      *interest_rect,
				   cairo_image_surface_t  *image,
				   cairo_rectangle_t      *image_rect,
				   void                   *image_extra)
{
    surface->backend->release_dest_image (surface, interest_rect,
					  image, image_rect, image_extra);
}

/**
 * _cairo_surface_clone_similar:
 * @surface: a #cairo_surface_t
 * @src: the source image
 * @clone_out: location to store a surface compatible with @surface
 *   and with contents identical to @src. The caller must call
 *   cairo_surface_destroy() on the result.
 * 
 * Creates a surface with contents identical to @src but that
 *   can be used efficiently with @surface. If @surface and @src are
 *   already compatible then it may return a new reference to @src.
 * 
 * Return value: %CAIRO_STATUS_SUCCESS if a surface was created and stored
 *   in @clone_out. Otherwise %CAIRO_INT_STATUS_UNSUPPORTED or another
 *   error like %CAIRO_STATUS_NO_MEMORY.
 **/
cairo_status_t
_cairo_surface_clone_similar (cairo_surface_t  *surface,
			      cairo_surface_t  *src,
			      cairo_surface_t **clone_out)
{
    cairo_status_t status;
    cairo_image_surface_t *image;
    void *image_extra;
    
    status = surface->backend->clone_similar (surface, src, clone_out);
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;

    status = _cairo_surface_acquire_source_image (src, &image, &image_extra);
    if (status != CAIRO_STATUS_SUCCESS)
	return status;
    
    status = surface->backend->clone_similar (surface, &image->base, clone_out);

    /* If the above failed point, we could implement a full fallback
     * using acquire_dest_image, but that's going to be very
     * inefficient compared to a backend-specific implementation of
     * clone_similar() with an image source. So we don't bother
     */
    
    _cairo_surface_release_source_image (src, image, image_extra);
    return status;
}

cairo_status_t
cairo_surface_set_matrix (cairo_surface_t *surface, cairo_matrix_t *matrix)
{
    if (surface == NULL)
	return CAIRO_STATUS_NULL_POINTER;

    return cairo_matrix_copy (&surface->matrix, matrix);
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
    return CAIRO_STATUS_SUCCESS;
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

    return CAIRO_STATUS_SUCCESS;
}
slim_hidden_def(cairo_surface_set_repeat);

typedef struct {
    cairo_surface_t *dst;
    cairo_rectangle_t extents;
    cairo_image_surface_t *image;
    cairo_rectangle_t image_rect;
    void *image_extra;
} fallback_state_t;

static cairo_status_t
_fallback_init (fallback_state_t *state,
		cairo_surface_t  *dst,
		int               x,
		int               y,
		int               width,
		int               height)
{
    state->extents.x = x;
    state->extents.y = y;
    state->extents.width = width;
    state->extents.height = height;
    
    state->dst = dst;

    return _cairo_surface_acquire_dest_image (dst, &state->extents,
					      &state->image, &state->image_rect, &state->image_extra);
}

static void
_fallback_cleanup (fallback_state_t *state)
{
    _cairo_surface_release_dest_image (state->dst, &state->extents,
				       state->image, &state->image_rect, state->image_extra);
}

static cairo_status_t
_fallback_composite (cairo_operator_t	operator,
		     cairo_pattern_t	*src,
		     cairo_pattern_t	*mask,
		     cairo_surface_t	*dst,
		     int		src_x,
		     int		src_y,
		     int		mask_x,
		     int		mask_y,
		     int		dst_x,
		     int		dst_y,
		     unsigned int	width,
		     unsigned int	height)
{
    fallback_state_t state;
    cairo_status_t status;

    status = _fallback_init (&state, dst, dst_x, dst_y, width, height);
    if (!CAIRO_OK (status) || !state.image)
	return status;

    state.image->base.backend->composite (operator, src, mask,
					  &state.image->base,
					  src_x, src_y, mask_x, mask_y,
					  dst_x - state.image_rect.x,
					  dst_y - state.image_rect.y,
					  width, height);

    _fallback_cleanup (&state);
	
    return status;
}

cairo_status_t
_cairo_surface_composite (cairo_operator_t	operator,
			  cairo_pattern_t	*src,
			  cairo_pattern_t	*mask,
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

    status = dst->backend->composite (operator,
				      src, mask, dst,
				      src_x, src_y,
				      mask_x, mask_y,
				      dst_x, dst_y,
				      width, height);
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;

    return _fallback_composite (operator,
				src, mask, dst,
				src_x, src_y,
				mask_x, mask_y,
				dst_x, dst_y,
				width, height);
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

static cairo_status_t
_fallback_fill_rectangles (cairo_surface_t	*surface,
			   cairo_operator_t	operator,
			   const cairo_color_t	*color,
			   cairo_rectangle_t	*rects,
			   int			num_rects)
{
    fallback_state_t state;
    cairo_rectangle_t *offset_rects = NULL;
    cairo_status_t status;
    int x1, y1, x2, y2;
    int i;

    if (num_rects <= 0)
	return CAIRO_STATUS_SUCCESS;

    /* Compute the bounds of the rectangles, so that we know what area of the
     * destination surface to fetch
     */
    x1 = rects[0].x;
    y1 = rects[0].y;
    x2 = rects[0].x + rects[0].width;
    y2 = rects[0].y + rects[0].height;
    
    for (i = 1; i < num_rects; i++) {
	if (rects[0].x < x1)
	    x1 = rects[0].x;
	if (rects[0].y < y1)
	    y1 = rects[0].y;

	if (rects[0].x + rects[0].width > x2)
	    x2 = rects[0].x + rects[0].width;
	if (rects[0].y + rects[0].height > y2)
	    y2 = rects[0].y + rects[0].height;
    }

    status = _fallback_init (&state, surface, x1, y1, x2 - x1, y2 - y1);
    if (!CAIRO_OK (status) || !state.image)
	return status;

    /* If the fetched image isn't at 0,0, we need to offset the rectangles */
    
    if (state.image_rect.x != 0 || state.image_rect.y != 0) {
	offset_rects = malloc (sizeof (cairo_rectangle_t) * num_rects);
	if (!offset_rects) {
	    status = CAIRO_STATUS_NO_MEMORY;
	    goto FAIL;
	}

	for (i = 0; i < num_rects; i++) {
	    offset_rects[i].x = rects[i].x - state.image_rect.x;
	    offset_rects[i].y = rects[i].y - state.image_rect.y;
	    offset_rects[i].width = rects[i].width;
	    offset_rects[i].height = rects[i].height;
	}

	rects = offset_rects;
    }

    state.image->base.backend->fill_rectangles (&state.image->base, operator, color,
						rects, num_rects);

    if (offset_rects)
	free (offset_rects);

 FAIL:
    _fallback_cleanup (&state);
    
    return status;
}

cairo_status_t
_cairo_surface_fill_rectangles (cairo_surface_t		*surface,
				cairo_operator_t	operator,
				const cairo_color_t	*color,
				cairo_rectangle_t	*rects,
				int			num_rects)
{
    cairo_int_status_t status;

    if (num_rects == 0)
	return CAIRO_STATUS_SUCCESS;

    status = surface->backend->fill_rectangles (surface,
						operator,
						color,
						rects, num_rects);
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;

    return _fallback_fill_rectangles (surface, operator, color, rects, num_rects);
}

static cairo_status_t
_fallback_composite_trapezoids (cairo_operator_t	operator,
				cairo_pattern_t		*pattern,
				cairo_surface_t		*dst,
				int			src_x,
				int			src_y,
				int			dst_x,
				int			dst_y,
				unsigned int		width,
				unsigned int		height,
				cairo_trapezoid_t	*traps,
				int			num_traps)
{
    fallback_state_t state;
    cairo_trapezoid_t *offset_traps = NULL;
    cairo_status_t status;
    int i;

    status = _fallback_init (&state, dst, dst_x, dst_y, width, height);
    if (!CAIRO_OK (status) || !state.image)
	return status;

    /* If the destination image isn't at 0,0, we need to offset the trapezoids */
    
    if (state.image_rect.x != 0 || state.image_rect.y != 0) {

	cairo_fixed_t xoff = _cairo_fixed_from_int (state.image_rect.x);
	cairo_fixed_t yoff = _cairo_fixed_from_int (state.image_rect.y);
	
	offset_traps = malloc (sizeof (cairo_trapezoid_t) * num_traps);
	if (!offset_traps) {
	    status = CAIRO_STATUS_NO_MEMORY;
	    goto FAIL;
	}

	for (i = 0; i < num_traps; i++) {
	    offset_traps[i].top = traps[i].top - yoff;
	    offset_traps[i].bottom = traps[i].bottom - yoff;
	    offset_traps[i].left.p1.x = traps[i].left.p1.x - xoff;
	    offset_traps[i].left.p1.y = traps[i].left.p1.y - yoff;
	    offset_traps[i].left.p2.x = traps[i].left.p2.x - xoff;
	    offset_traps[i].left.p2.y = traps[i].left.p2.y - yoff;
	    offset_traps[i].right.p1.x = traps[i].right.p1.x - xoff;
	    offset_traps[i].right.p1.y = traps[i].right.p1.y - yoff;
	    offset_traps[i].right.p2.x = traps[i].right.p2.x - xoff;
	    offset_traps[i].right.p2.y = traps[i].right.p2.y - yoff;
	}

	traps = offset_traps;
    }

    state.image->base.backend->composite_trapezoids (operator, pattern,
						     &state.image->base,
						     src_x, src_y,
						     dst_x - state.image_rect.x,
						     dst_y - state.image_rect.y,
						     width, height, traps, num_traps);
    if (offset_traps)
	free (offset_traps);

 FAIL:
    _fallback_cleanup (&state);
    
    return status;
}


cairo_status_t
_cairo_surface_composite_trapezoids (cairo_operator_t		operator,
				     cairo_pattern_t		*pattern,
				     cairo_surface_t		*dst,
				     int			src_x,
				     int			src_y,
				     int			dst_x,
				     int			dst_y,
				     unsigned int		width,
				     unsigned int		height,
				     cairo_trapezoid_t		*traps,
				     int			num_traps)
{
    cairo_int_status_t status;

    status = dst->backend->composite_trapezoids (operator,
						 pattern, dst,
						 src_x, src_y,
						 dst_x, dst_y,
						 width, height,
						 traps, num_traps);
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;

    return _fallback_composite_trapezoids (operator, pattern, dst,
					   src_x, src_y,
					   dst_x, dst_y,
					   width, height,
					   traps, num_traps);
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
