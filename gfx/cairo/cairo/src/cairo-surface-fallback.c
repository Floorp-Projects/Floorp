/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
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
 *      Joonas Pihlaja <jpihlaja@cc.helsinki.fi>
 *	Chris Wilson <chris@chris-wilson.co.uk>
 */

#include "cairoint.h"

#include "cairo-boxes-private.h"
#include "cairo-clip-private.h"
#include "cairo-composite-rectangles-private.h"
#include "cairo-error-private.h"
#include "cairo-region-private.h"
#include "cairo-spans-private.h"
#include "cairo-surface-fallback-private.h"

typedef struct {
    cairo_surface_t *dst;
    cairo_rectangle_int_t extents;
    cairo_image_surface_t *image;
    cairo_rectangle_int_t image_rect;
    void *image_extra;
} fallback_state_t;

/**
 * _fallback_init:
 *
 * Acquire destination image surface needed for an image-based
 * fallback.
 *
 * Return value: %CAIRO_INT_STATUS_NOTHING_TO_DO if the extents are not
 * visible, %CAIRO_STATUS_SUCCESS if some portion is visible and all
 * went well, or some error status otherwise.
 **/
static cairo_int_status_t
_fallback_init (fallback_state_t *state,
		cairo_surface_t  *dst,
		int               x,
		int               y,
		int               width,
		int               height)
{
    cairo_status_t status;

    state->extents.x = x;
    state->extents.y = y;
    state->extents.width = width;
    state->extents.height = height;

    state->dst = dst;

    status = _cairo_surface_acquire_dest_image (dst, &state->extents,
						&state->image, &state->image_rect,
						&state->image_extra);
    if (unlikely (status))
	return status;


    /* XXX: This NULL value tucked away in state->image is a rather
     * ugly interface. Cleaner would be to push the
     * CAIRO_INT_STATUS_NOTHING_TO_DO value down into
     * _cairo_surface_acquire_dest_image and its backend
     * counterparts. */
    assert (state->image != NULL);

    return CAIRO_STATUS_SUCCESS;
}

static void
_fallback_fini (fallback_state_t *state)
{
    _cairo_surface_release_dest_image (state->dst, &state->extents,
				       state->image, &state->image_rect,
				       state->image_extra);
}

typedef cairo_status_t
(*cairo_draw_func_t) (void                          *closure,
		      cairo_operator_t               op,
		      const cairo_pattern_t         *src,
		      cairo_surface_t               *dst,
		      int                            dst_x,
		      int                            dst_y,
		      const cairo_rectangle_int_t   *extents,
		      cairo_region_t		    *clip_region);

static cairo_status_t
_create_composite_mask_pattern (cairo_surface_pattern_t       *mask_pattern,
				cairo_clip_t                  *clip,
				cairo_draw_func_t              draw_func,
				void                          *draw_closure,
				cairo_surface_t               *dst,
				const cairo_rectangle_int_t   *extents)
{
    cairo_surface_t *mask;
    cairo_region_t *clip_region = NULL, *fallback_region = NULL;
    cairo_status_t status;
    cairo_bool_t clip_surface = FALSE;

    if (clip != NULL) {
	status = _cairo_clip_get_region (clip, &clip_region);
	if (unlikely (_cairo_status_is_error (status) ||
		      status == CAIRO_INT_STATUS_NOTHING_TO_DO))
	{
	    return status;
	}

	clip_surface = status == CAIRO_INT_STATUS_UNSUPPORTED;
    }

    /* We need to use solid here, because to use CAIRO_OPERATOR_SOURCE with
     * a mask (as called via _cairo_surface_mask) triggers assertion failures.
     */
    mask = _cairo_surface_create_similar_solid (dst,
						CAIRO_CONTENT_ALPHA,
						extents->width,
						extents->height,
						CAIRO_COLOR_TRANSPARENT,
						TRUE);
    if (unlikely (mask->status))
	return mask->status;

    if (clip_region && (extents->x || extents->y)) {
	fallback_region = cairo_region_copy (clip_region);
	status = fallback_region->status;
	if (unlikely (status))
	    goto CLEANUP_SURFACE;

	cairo_region_translate (fallback_region,
				-extents->x,
				-extents->y);
	clip_region = fallback_region;
    }

    status = draw_func (draw_closure, CAIRO_OPERATOR_ADD,
			&_cairo_pattern_white.base, mask,
			extents->x, extents->y,
			extents,
			clip_region);
    if (unlikely (status))
	goto CLEANUP_SURFACE;

    if (clip_surface)
	status = _cairo_clip_combine_with_surface (clip, mask, extents->x, extents->y);

    _cairo_pattern_init_for_surface (mask_pattern, mask);

 CLEANUP_SURFACE:
    if (fallback_region)
        cairo_region_destroy (fallback_region);
    cairo_surface_destroy (mask);

    return status;
}

/* Handles compositing with a clip surface when the operator allows
 * us to combine the clip with the mask
 */
static cairo_status_t
_clip_and_composite_with_mask (cairo_clip_t                  *clip,
			       cairo_operator_t               op,
			       const cairo_pattern_t         *src,
			       cairo_draw_func_t              draw_func,
			       void                          *draw_closure,
			       cairo_surface_t               *dst,
			       const cairo_rectangle_int_t   *extents)
{
    cairo_surface_pattern_t mask_pattern;
    cairo_status_t status;

    status = _create_composite_mask_pattern (&mask_pattern,
					     clip,
					     draw_func, draw_closure,
					     dst, extents);
    if (likely (status == CAIRO_STATUS_SUCCESS)) {
	status = _cairo_surface_composite (op,
					   src, &mask_pattern.base, dst,
					   extents->x,     extents->y,
					   0,              0,
					   extents->x,     extents->y,
					   extents->width, extents->height,
					   NULL);

	_cairo_pattern_fini (&mask_pattern.base);
    }

    return status;
}

/* Handles compositing with a clip surface when we have to do the operation
 * in two pieces and combine them together.
 */
static cairo_status_t
_clip_and_composite_combine (cairo_clip_t                  *clip,
			     cairo_operator_t               op,
			     const cairo_pattern_t         *src,
			     cairo_draw_func_t              draw_func,
			     void                          *draw_closure,
			     cairo_surface_t               *dst,
			     const cairo_rectangle_int_t   *extents)
{
    cairo_surface_t *intermediate;
    cairo_surface_pattern_t pattern;
    cairo_surface_pattern_t clip_pattern;
    cairo_surface_t *clip_surface;
    int clip_x, clip_y;
    cairo_status_t status;

    /* We'd be better off here creating a surface identical in format
     * to dst, but we have no way of getting that information. Instead
     * we ask the backend to create a similar surface of identical content,
     * in the belief that the backend will do something useful - like use
     * an identical format. For example, the xlib backend will endeavor to
     * use a compatible depth to enable core protocol routines.
     */
    intermediate =
	_cairo_surface_create_similar_scratch (dst, dst->content,
					       extents->width,
					       extents->height);
    if (intermediate == NULL) {
	intermediate =
	    _cairo_image_surface_create_with_content (dst->content,
						      extents->width,
						      extents->width);
    }
    if (unlikely (intermediate->status))
	return intermediate->status;

    /* Initialize the intermediate surface from the destination surface */
    _cairo_pattern_init_for_surface (&pattern, dst);
    status = _cairo_surface_composite (CAIRO_OPERATOR_SOURCE,
				       &pattern.base, NULL, intermediate,
				       extents->x,     extents->y,
				       0,              0,
				       0,              0,
				       extents->width, extents->height,
				       NULL);
    _cairo_pattern_fini (&pattern.base);
    if (unlikely (status))
	goto CLEANUP_SURFACE;

    status = (*draw_func) (draw_closure, op,
			   src, intermediate,
			   extents->x, extents->y,
			   extents,
			   NULL);
    if (unlikely (status))
	goto CLEANUP_SURFACE;

    assert (clip->path != NULL);
    clip_surface = _cairo_clip_get_surface (clip, dst, &clip_x, &clip_y);
    if (unlikely (clip_surface->status))
	goto CLEANUP_SURFACE;

    _cairo_pattern_init_for_surface (&clip_pattern, clip_surface);

    /* Combine that with the clip */
    status = _cairo_surface_composite (CAIRO_OPERATOR_DEST_IN,
				       &clip_pattern.base, NULL, intermediate,
				       extents->x - clip_x,
				       extents->y - clip_y,
				       0, 0,
				       0, 0,
				       extents->width, extents->height,
				       NULL);
    if (unlikely (status))
	goto CLEANUP_CLIP;

    /* Punch the clip out of the destination */
    status = _cairo_surface_composite (CAIRO_OPERATOR_DEST_OUT,
				       &clip_pattern.base, NULL, dst,
				       extents->x - clip_x,
				       extents->y - clip_y,
				       0, 0,
				       extents->x, extents->y,
				       extents->width, extents->height,
				       NULL);
    if (unlikely (status))
	goto CLEANUP_CLIP;

    /* Now add the two results together */
    _cairo_pattern_init_for_surface (&pattern, intermediate);
    status = _cairo_surface_composite (CAIRO_OPERATOR_ADD,
				       &pattern.base, NULL, dst,
				       0,              0,
				       0,              0,
				       extents->x,     extents->y,
				       extents->width, extents->height,
				       NULL);
    _cairo_pattern_fini (&pattern.base);

 CLEANUP_CLIP:
    _cairo_pattern_fini (&clip_pattern.base);
 CLEANUP_SURFACE:
    cairo_surface_destroy (intermediate);

    return status;
}

/* Handles compositing for %CAIRO_OPERATOR_SOURCE, which is special; it's
 * defined as (src IN mask IN clip) ADD (dst OUT (mask IN clip))
 */
static cairo_status_t
_clip_and_composite_source (cairo_clip_t                  *clip,
			    const cairo_pattern_t         *src,
			    cairo_draw_func_t              draw_func,
			    void                          *draw_closure,
			    cairo_surface_t               *dst,
			    const cairo_rectangle_int_t   *extents)
{
    cairo_surface_pattern_t mask_pattern;
    cairo_region_t *clip_region = NULL;
    cairo_status_t status;

    if (clip != NULL) {
	status = _cairo_clip_get_region (clip, &clip_region);
	if (unlikely (_cairo_status_is_error (status) ||
		      status == CAIRO_INT_STATUS_NOTHING_TO_DO))
	{
	    return status;
	}
    }

    /* Create a surface that is mask IN clip */
    status = _create_composite_mask_pattern (&mask_pattern,
					     clip,
					     draw_func, draw_closure,
					     dst, extents);
    if (unlikely (status))
	return status;

    /* Compute dest' = dest OUT (mask IN clip) */
    status = _cairo_surface_composite (CAIRO_OPERATOR_DEST_OUT,
				       &mask_pattern.base, NULL, dst,
				       0,              0,
				       0,              0,
				       extents->x,     extents->y,
				       extents->width, extents->height,
				       clip_region);

    if (unlikely (status))
	goto CLEANUP_MASK_PATTERN;

    /* Now compute (src IN (mask IN clip)) ADD dest' */
    status = _cairo_surface_composite (CAIRO_OPERATOR_ADD,
				       src, &mask_pattern.base, dst,
				       extents->x,     extents->y,
				       0,              0,
				       extents->x,     extents->y,
				       extents->width, extents->height,
				       clip_region);

 CLEANUP_MASK_PATTERN:
    _cairo_pattern_fini (&mask_pattern.base);
    return status;
}

static int
_cairo_rectangle_empty (const cairo_rectangle_int_t *rect)
{
    return rect->width == 0 || rect->height == 0;
}

/**
 * _clip_and_composite:
 * @clip: a #cairo_clip_t
 * @op: the operator to draw with
 * @src: source pattern
 * @draw_func: function that can be called to draw with the mask onto a surface.
 * @draw_closure: data to pass to @draw_func.
 * @dst: destination surface
 * @extents: rectangle holding a bounding box for the operation; this
 *           rectangle will be used as the size for the temporary
 *           surface.
 *
 * When there is a surface clip, we typically need to create an intermediate
 * surface. This function handles the logic of creating a temporary surface
 * drawing to it, then compositing the result onto the target surface.
 *
 * @draw_func is to called to draw the mask; it will be called no more
 * than once.
 *
 * Return value: %CAIRO_STATUS_SUCCESS if the drawing succeeded.
 **/
static cairo_status_t
_clip_and_composite (cairo_clip_t                  *clip,
		     cairo_operator_t               op,
		     const cairo_pattern_t         *src,
		     cairo_draw_func_t              draw_func,
		     void                          *draw_closure,
		     cairo_surface_t               *dst,
		     const cairo_rectangle_int_t   *extents)
{
    cairo_status_t status;

    if (_cairo_rectangle_empty (extents))
	/* Nothing to do */
	return CAIRO_STATUS_SUCCESS;

    if (op == CAIRO_OPERATOR_CLEAR) {
	src = &_cairo_pattern_white.base;
	op = CAIRO_OPERATOR_DEST_OUT;
    }

    if (op == CAIRO_OPERATOR_SOURCE) {
	status = _clip_and_composite_source (clip,
					     src,
					     draw_func, draw_closure,
					     dst, extents);
    } else {
	cairo_bool_t clip_surface = FALSE;
	cairo_region_t *clip_region = NULL;

	if (clip != NULL) {
	    status = _cairo_clip_get_region (clip, &clip_region);
	    if (unlikely (_cairo_status_is_error (status) ||
			  status == CAIRO_INT_STATUS_NOTHING_TO_DO))
	    {
		return status;
	    }

	    clip_surface = status == CAIRO_INT_STATUS_UNSUPPORTED;
	}

	if (clip_surface) {
	    if (_cairo_operator_bounded_by_mask (op)) {
		status = _clip_and_composite_with_mask (clip, op,
							src,
							draw_func, draw_closure,
							dst, extents);
	    } else {
		status = _clip_and_composite_combine (clip, op,
						      src,
						      draw_func, draw_closure,
						      dst, extents);
	    }
	} else {
	    status = draw_func (draw_closure, op,
				src, dst,
				0, 0,
				extents,
				clip_region);
	}
    }

    return status;
}

/* Composites a region representing a set of trapezoids.
 */
static cairo_status_t
_composite_trap_region (cairo_clip_t            *clip,
			const cairo_pattern_t	*src,
			cairo_operator_t         op,
			cairo_surface_t         *dst,
			cairo_region_t          *trap_region,
			const cairo_rectangle_int_t   *extents)
{
    cairo_status_t status;
    cairo_surface_pattern_t mask_pattern;
    cairo_pattern_t *mask = NULL;
    int mask_x = 0, mask_y =0;

    if (clip != NULL) {
	cairo_surface_t *clip_surface = NULL;
	int clip_x, clip_y;

	clip_surface = _cairo_clip_get_surface (clip, dst, &clip_x, &clip_y);
	if (unlikely (clip_surface->status))
	    return clip_surface->status;

	if (op == CAIRO_OPERATOR_CLEAR) {
	    src = &_cairo_pattern_white.base;
	    op = CAIRO_OPERATOR_DEST_OUT;
	}

	_cairo_pattern_init_for_surface (&mask_pattern, clip_surface);
	mask_x = extents->x - clip_x;
	mask_y = extents->y - clip_y;
	mask = &mask_pattern.base;
    }

    status = _cairo_surface_composite (op, src, mask, dst,
				       extents->x, extents->y,
				       mask_x, mask_y,
				       extents->x, extents->y,
				       extents->width, extents->height,
				       trap_region);

    if (mask != NULL)
      _cairo_pattern_fini (mask);

    return status;
}

typedef struct {
    cairo_traps_t *traps;
    cairo_antialias_t antialias;
} cairo_composite_traps_info_t;

static cairo_status_t
_composite_traps_draw_func (void                          *closure,
			    cairo_operator_t               op,
			    const cairo_pattern_t         *src,
			    cairo_surface_t               *dst,
			    int                            dst_x,
			    int                            dst_y,
			    const cairo_rectangle_int_t   *extents,
			    cairo_region_t		  *clip_region)
{
    cairo_composite_traps_info_t *info = closure;
    cairo_status_t status;
    cairo_region_t *extents_region = NULL;

    if (dst_x != 0 || dst_y != 0)
	_cairo_traps_translate (info->traps, - dst_x, - dst_y);

    if (clip_region == NULL &&
        !_cairo_operator_bounded_by_source (op)) {
        extents_region = cairo_region_create_rectangle (extents);
        if (unlikely (extents_region->status))
            return extents_region->status;
        cairo_region_translate (extents_region, -dst_x, -dst_y);
        clip_region = extents_region;
    }

    status = _cairo_surface_composite_trapezoids (op,
                                                  src, dst, info->antialias,
                                                  extents->x,         extents->y,
                                                  extents->x - dst_x, extents->y - dst_y,
                                                  extents->width,     extents->height,
                                                  info->traps->traps,
                                                  info->traps->num_traps,
                                                  clip_region);

    if (extents_region)
        cairo_region_destroy (extents_region);

    return status;
}

enum {
    HAS_CLEAR_REGION = 0x1,
};

static cairo_status_t
_clip_and_composite_region (const cairo_pattern_t *src,
			    cairo_operator_t op,
			    cairo_surface_t *dst,
			    cairo_region_t *trap_region,
			    cairo_clip_t *clip,
			    cairo_rectangle_int_t *extents)
{
    cairo_region_t clear_region;
    unsigned int has_region = 0;
    cairo_status_t status;

    if (! _cairo_operator_bounded_by_mask (op) && clip == NULL) {
	/* If we optimize drawing with an unbounded operator to
	 * _cairo_surface_fill_rectangles() or to drawing with a
	 * clip region, then we have an additional region to clear.
	 */
	_cairo_region_init_rectangle (&clear_region, extents);
	status = cairo_region_subtract (&clear_region, trap_region);
	if (unlikely (status))
	    return status;

	if (! cairo_region_is_empty (&clear_region))
	    has_region |= HAS_CLEAR_REGION;
    }

    if ((src->type == CAIRO_PATTERN_TYPE_SOLID || op == CAIRO_OPERATOR_CLEAR) &&
	clip == NULL)
    {
	const cairo_color_t *color;

	if (op == CAIRO_OPERATOR_CLEAR)
	    color = CAIRO_COLOR_TRANSPARENT;
	else
	    color = &((cairo_solid_pattern_t *)src)->color;

	/* Solid rectangles special case */
	status = _cairo_surface_fill_region (dst, op, color, trap_region);
    } else {
	/* For a simple rectangle, we can just use composite(), for more
	 * rectangles, we have to set a clip region. The cost of rasterizing
	 * trapezoids is pretty high for most backends currently, so it's
	 * worthwhile even if a region is needed.
	 *
	 * If we have a clip surface, we set it as the mask; this only works
	 * for bounded operators other than SOURCE; for unbounded operators,
	 * clip and mask cannot be interchanged. For SOURCE, the operator
	 * as implemented by the backends is different in its handling
	 * of the mask then what we want.
	 *
	 * CAIRO_INT_STATUS_UNSUPPORTED will be returned if the region has
	 * more than rectangle and the destination doesn't support clip
	 * regions. In that case, we fall through.
	 */
	status = _composite_trap_region (clip, src, op, dst,
					 trap_region, extents);
    }

    if (has_region & HAS_CLEAR_REGION) {
	if (status == CAIRO_STATUS_SUCCESS) {
	    status = _cairo_surface_fill_region (dst,
						 CAIRO_OPERATOR_CLEAR,
						 CAIRO_COLOR_TRANSPARENT,
						 &clear_region);
	}
	_cairo_region_fini (&clear_region);
    }

    return status;
}

/* avoid using region code to re-validate boxes */
static cairo_status_t
_fill_rectangles (cairo_surface_t *dst,
		  cairo_operator_t op,
		  const cairo_pattern_t *src,
		  cairo_traps_t *traps,
		  cairo_clip_t *clip)
{
    const cairo_color_t *color;
    cairo_rectangle_int_t stack_rects[CAIRO_STACK_ARRAY_LENGTH (cairo_rectangle_int_t)];
    cairo_rectangle_int_t *rects = stack_rects;
    cairo_status_t status;
    int i;

    if (! traps->is_rectilinear || ! traps->maybe_region)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* XXX: convert clip region to geometric boxes? */
    if (clip != NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* XXX: fallback for the region_subtract() operation */
    if (! _cairo_operator_bounded_by_mask (op))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (! (src->type == CAIRO_PATTERN_TYPE_SOLID || op == CAIRO_OPERATOR_CLEAR))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (traps->has_intersections) {
	if (traps->is_rectangular) {
	    status = _cairo_bentley_ottmann_tessellate_rectangular_traps (traps, CAIRO_FILL_RULE_WINDING);
	} else {
	    status = _cairo_bentley_ottmann_tessellate_rectilinear_traps (traps, CAIRO_FILL_RULE_WINDING);
	}
	if (unlikely (status))
	    return status;
    }

    for (i = 0; i < traps->num_traps; i++) {
	if (! _cairo_fixed_is_integer (traps->traps[i].top)          ||
	    ! _cairo_fixed_is_integer (traps->traps[i].bottom)       ||
	    ! _cairo_fixed_is_integer (traps->traps[i].left.p1.x)    ||
	    ! _cairo_fixed_is_integer (traps->traps[i].right.p1.x))
	{
	    traps->maybe_region = FALSE;
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}
    }

    if (traps->num_traps > ARRAY_LENGTH (stack_rects)) {
	rects = _cairo_malloc_ab (traps->num_traps,
				  sizeof (cairo_rectangle_int_t));
	if (unlikely (rects == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    for (i = 0; i < traps->num_traps; i++) {
	int x1 = _cairo_fixed_integer_part (traps->traps[i].left.p1.x);
	int y1 = _cairo_fixed_integer_part (traps->traps[i].top);
	int x2 = _cairo_fixed_integer_part (traps->traps[i].right.p1.x);
	int y2 = _cairo_fixed_integer_part (traps->traps[i].bottom);

	rects[i].x = x1;
	rects[i].y = y1;
	rects[i].width = x2 - x1;
	rects[i].height = y2 - y1;
    }

    if (op == CAIRO_OPERATOR_CLEAR)
	color = CAIRO_COLOR_TRANSPARENT;
    else
	color = &((cairo_solid_pattern_t *)src)->color;

    status =  _cairo_surface_fill_rectangles (dst, op, color, rects, i);

    if (rects != stack_rects)
	free (rects);

    return status;
}

/* fast-path for very common composite of a single rectangle */
static cairo_status_t
_composite_rectangle (cairo_surface_t *dst,
		      cairo_operator_t op,
		      const cairo_pattern_t *src,
		      cairo_traps_t *traps,
		      cairo_clip_t *clip)
{
    cairo_rectangle_int_t rect;

    if (clip != NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (traps->num_traps > 1 || ! traps->is_rectilinear || ! traps->maybe_region)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (! _cairo_fixed_is_integer (traps->traps[0].top)          ||
	! _cairo_fixed_is_integer (traps->traps[0].bottom)       ||
	! _cairo_fixed_is_integer (traps->traps[0].left.p1.x)    ||
	! _cairo_fixed_is_integer (traps->traps[0].right.p1.x))
    {
	traps->maybe_region = FALSE;
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    rect.x = _cairo_fixed_integer_part (traps->traps[0].left.p1.x);
    rect.y = _cairo_fixed_integer_part (traps->traps[0].top);
    rect.width  = _cairo_fixed_integer_part (traps->traps[0].right.p1.x) - rect.x;
    rect.height = _cairo_fixed_integer_part (traps->traps[0].bottom) - rect.y;

    return _cairo_surface_composite (op, src, NULL, dst,
				     rect.x, rect.y,
				     0, 0,
				     rect.x, rect.y,
				     rect.width, rect.height,
				     NULL);
}

/* Warning: This call modifies the coordinates of traps */
static cairo_status_t
_clip_and_composite_trapezoids (const cairo_pattern_t *src,
				cairo_operator_t op,
				cairo_surface_t *dst,
				cairo_traps_t *traps,
				cairo_antialias_t antialias,
				cairo_clip_t *clip,
				cairo_rectangle_int_t *extents)
{
    cairo_composite_traps_info_t traps_info;
    cairo_region_t *clip_region = NULL;
    cairo_bool_t clip_surface = FALSE;
    cairo_status_t status;

    if (traps->num_traps == 0 && _cairo_operator_bounded_by_mask (op))
	return CAIRO_STATUS_SUCCESS;

    if (clip != NULL) {
	status = _cairo_clip_get_region (clip, &clip_region);
	if (unlikely (_cairo_status_is_error (status)))
	    return status;
	if (unlikely (status == CAIRO_INT_STATUS_NOTHING_TO_DO))
	    return CAIRO_STATUS_SUCCESS;

	clip_surface = status == CAIRO_INT_STATUS_UNSUPPORTED;
    }

    /* Use a fast path if the trapezoids consist of a simple region,
     * but we can only do this if we do not have a clip surface, or can
     * substitute the mask with the clip.
     */
    if (! clip_surface ||
	(_cairo_operator_bounded_by_mask (op) && op != CAIRO_OPERATOR_SOURCE))
    {
	cairo_region_t *trap_region = NULL;

        if (_cairo_operator_bounded_by_source (op)) {
            status = _fill_rectangles (dst, op, src, traps, clip);
            if (status != CAIRO_INT_STATUS_UNSUPPORTED)
                return status;

            status = _composite_rectangle (dst, op, src, traps, clip);
            if (status != CAIRO_INT_STATUS_UNSUPPORTED)
                return status;
        }

	status = _cairo_traps_extract_region (traps, &trap_region);
	if (unlikely (_cairo_status_is_error (status)))
	    return status;

	if (trap_region != NULL) {
	    status = cairo_region_intersect_rectangle (trap_region, extents);
	    if (unlikely (status)) {
		cairo_region_destroy (trap_region);
		return status;
	    }

	    if (clip_region != NULL) {
		status = cairo_region_intersect (trap_region, clip_region);
		if (unlikely (status)) {
		    cairo_region_destroy (trap_region);
		    return status;
		}
	    }

	    if (_cairo_operator_bounded_by_mask (op)) {
		cairo_rectangle_int_t trap_extents;

		cairo_region_get_extents (trap_region, &trap_extents);
		if (! _cairo_rectangle_intersect (extents, &trap_extents)) {
		    cairo_region_destroy (trap_region);
		    return CAIRO_STATUS_SUCCESS;
		}
	    }

	    status = _clip_and_composite_region (src, op, dst,
						 trap_region,
						 clip_surface ? clip : NULL,
						 extents);
	    cairo_region_destroy (trap_region);

	    if (likely (status != CAIRO_INT_STATUS_UNSUPPORTED))
		return status;
	}
    }

    /* No fast path, exclude self-intersections and clip trapezoids. */
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

    /* Otherwise render the trapezoids to a mask and composite in the usual
     * fashion.
     */
    traps_info.traps = traps;
    traps_info.antialias = antialias;

    return _clip_and_composite (clip, op, src,
				_composite_traps_draw_func,
				&traps_info, dst, extents);
}

cairo_status_t
_cairo_surface_fallback_paint (cairo_surface_t		*surface,
			       cairo_operator_t		 op,
			       const cairo_pattern_t	*source,
			       cairo_clip_t		*clip)
{
    cairo_composite_rectangles_t extents;
    cairo_rectangle_int_t rect;
    cairo_clip_path_t *clip_path = clip ? clip->path : NULL;
    cairo_box_t boxes_stack[32], *clip_boxes = boxes_stack;
    cairo_boxes_t  boxes;
    int num_boxes = ARRAY_LENGTH (boxes_stack);
    cairo_status_t status;
    cairo_traps_t traps;

    if (!_cairo_surface_get_extents (surface, &rect))
        ASSERT_NOT_REACHED;

    status = _cairo_composite_rectangles_init_for_paint (&extents,
							 &rect,
							 op, source,
							 clip);
    if (unlikely (status))
	return status;

    if (_cairo_clip_contains_extents (clip, &extents))
	clip = NULL;

    status = _cairo_clip_to_boxes (&clip, &extents, &clip_boxes, &num_boxes);
    if (unlikely (status))
	return status;

    /* If the clip cannot be reduced to a set of boxes, we will need to
     * use a clipmask. Paint is special as it is the only operation that
     * does not implicitly use a mask, so we may be able to reduce this
     * operation to a fill...
     */
    if (clip != NULL && clip_path->prev == NULL &&
	_cairo_operator_bounded_by_mask (op))
    {
	return _cairo_surface_fill (surface, op, source,
				    &clip_path->path,
				    clip_path->fill_rule,
				    clip_path->tolerance,
				    clip_path->antialias,
				    NULL);
    }

    /* meh, surface-fallback is dying anyway... */
    _cairo_boxes_init_for_array (&boxes, clip_boxes, num_boxes);
    status = _cairo_traps_init_boxes (&traps, &boxes);
    if (unlikely (status))
	goto CLEANUP_BOXES;

    status = _clip_and_composite_trapezoids (source, op, surface,
					     &traps, CAIRO_ANTIALIAS_DEFAULT,
					     clip,
                                             extents.is_bounded ? &extents.bounded : &extents.unbounded);
    _cairo_traps_fini (&traps);

CLEANUP_BOXES:
    if (clip_boxes != boxes_stack)
	free (clip_boxes);

    return status;
}

static cairo_status_t
_cairo_surface_mask_draw_func (void                        *closure,
			       cairo_operator_t             op,
			       const cairo_pattern_t       *src,
			       cairo_surface_t             *dst,
			       int                          dst_x,
			       int                          dst_y,
			       const cairo_rectangle_int_t *extents,
			       cairo_region_t		   *clip_region)
{
    cairo_pattern_t *mask = closure;
    cairo_status_t status;
    cairo_region_t *extents_region = NULL;

    if (clip_region == NULL &&
        !_cairo_operator_bounded_by_source (op)) {
        extents_region = cairo_region_create_rectangle (extents);
        if (unlikely (extents_region->status))
            return extents_region->status;
        cairo_region_translate (extents_region, -dst_x, -dst_y);
        clip_region = extents_region;
    }

    if (src) {
	status = _cairo_surface_composite (op,
                                           src, mask, dst,
                                           extents->x,         extents->y,
                                           extents->x,         extents->y,
                                           extents->x - dst_x, extents->y - dst_y,
                                           extents->width,     extents->height,
                                           clip_region);
    } else {
	status = _cairo_surface_composite (op,
                                           mask, NULL, dst,
                                           extents->x,         extents->y,
                                           0,                  0, /* unused */
                                           extents->x - dst_x, extents->y - dst_y,
                                           extents->width,     extents->height,
                                           clip_region);
    }

    if (extents_region)
        cairo_region_destroy (extents_region);

    return status;
}

cairo_status_t
_cairo_surface_fallback_mask (cairo_surface_t		*surface,
			      cairo_operator_t		 op,
			      const cairo_pattern_t	*source,
			      const cairo_pattern_t	*mask,
			      cairo_clip_t		*clip)
{
    cairo_composite_rectangles_t extents;
    cairo_rectangle_int_t rect;
    cairo_status_t status;

    if (!_cairo_surface_get_extents (surface, &rect))
        ASSERT_NOT_REACHED;

    status = _cairo_composite_rectangles_init_for_mask (&extents,
							&rect,
							op, source, mask, clip);
    if (unlikely (status))
	return status;

    if (_cairo_clip_contains_extents (clip, &extents))
	clip = NULL;

    if (clip != NULL && extents.is_bounded) {
	status = _cairo_clip_rectangle (clip, &extents.bounded);
	if (unlikely (status))
	    return status;
    }

    return _clip_and_composite (clip, op, source,
				_cairo_surface_mask_draw_func,
				(void *) mask,
				surface,
                                extents.is_bounded ? &extents.bounded : &extents.unbounded);
}

cairo_status_t
_cairo_surface_fallback_stroke (cairo_surface_t		*surface,
				cairo_operator_t	 op,
				const cairo_pattern_t	*source,
				cairo_path_fixed_t	*path,
				const cairo_stroke_style_t	*stroke_style,
				const cairo_matrix_t		*ctm,
				const cairo_matrix_t		*ctm_inverse,
				double			 tolerance,
				cairo_antialias_t	 antialias,
				cairo_clip_t		*clip)
{
    cairo_polygon_t polygon;
    cairo_traps_t traps;
    cairo_box_t boxes_stack[32], *clip_boxes = boxes_stack;
    int num_boxes = ARRAY_LENGTH (boxes_stack);
    cairo_composite_rectangles_t extents;
    cairo_rectangle_int_t rect;
    cairo_status_t status;

    if (!_cairo_surface_get_extents (surface, &rect))
        ASSERT_NOT_REACHED;

    status = _cairo_composite_rectangles_init_for_stroke (&extents,
							  &rect,
							  op, source,
							  path, stroke_style, ctm,
							  clip);
    if (unlikely (status))
	return status;

    if (_cairo_clip_contains_extents (clip, &extents))
	clip = NULL;

    status = _cairo_clip_to_boxes (&clip, &extents, &clip_boxes, &num_boxes);
    if (unlikely (status))
	return status;

    _cairo_polygon_init (&polygon);
    _cairo_polygon_limit (&polygon, clip_boxes, num_boxes);

    _cairo_traps_init (&traps);
    _cairo_traps_limit (&traps, clip_boxes, num_boxes);

    if (path->is_rectilinear) {
	status = _cairo_path_fixed_stroke_rectilinear_to_traps (path,
								stroke_style,
								ctm,
								&traps);
	if (likely (status == CAIRO_STATUS_SUCCESS))
	    goto DO_TRAPS;

	if (_cairo_status_is_error (status))
	    goto CLEANUP;
    }

    status = _cairo_path_fixed_stroke_to_polygon (path,
						  stroke_style,
						  ctm, ctm_inverse,
						  tolerance,
						  &polygon);
    if (unlikely (status))
	goto CLEANUP;

    if (polygon.num_edges == 0)
	goto DO_TRAPS;

    if (_cairo_operator_bounded_by_mask (op)) {
	_cairo_box_round_to_rectangle (&polygon.extents, &extents.mask);
	if (! _cairo_rectangle_intersect (&extents.bounded, &extents.mask))
	    goto CLEANUP;
    }

    /* Fall back to trapezoid fills. */
    status = _cairo_bentley_ottmann_tessellate_polygon (&traps,
							&polygon,
							CAIRO_FILL_RULE_WINDING);
    if (unlikely (status))
	goto CLEANUP;

  DO_TRAPS:
    status = _clip_and_composite_trapezoids (source, op, surface,
					     &traps, antialias,
					     clip,
                                             extents.is_bounded ? &extents.bounded : &extents.unbounded);
  CLEANUP:
    _cairo_traps_fini (&traps);
    _cairo_polygon_fini (&polygon);
    if (clip_boxes != boxes_stack)
	free (clip_boxes);

    return status;
}

cairo_status_t
_cairo_surface_fallback_fill (cairo_surface_t		*surface,
			      cairo_operator_t		 op,
			      const cairo_pattern_t	*source,
			      cairo_path_fixed_t	*path,
			      cairo_fill_rule_t		 fill_rule,
			      double			 tolerance,
			      cairo_antialias_t		 antialias,
			      cairo_clip_t		*clip)
{
    cairo_polygon_t polygon;
    cairo_traps_t traps;
    cairo_box_t boxes_stack[32], *clip_boxes = boxes_stack;
    int num_boxes = ARRAY_LENGTH (boxes_stack);
    cairo_bool_t is_rectilinear;
    cairo_composite_rectangles_t extents;
    cairo_rectangle_int_t rect;
    cairo_status_t status;

    if (!_cairo_surface_get_extents (surface, &rect))
        ASSERT_NOT_REACHED;

    status = _cairo_composite_rectangles_init_for_fill (&extents,
							&rect,
							op, source, path,
							clip);
    if (unlikely (status))
	return status;

    if (_cairo_clip_contains_extents (clip, &extents))
	clip = NULL;

    status = _cairo_clip_to_boxes (&clip, &extents, &clip_boxes, &num_boxes);
    if (unlikely (status))
	return status;

    _cairo_traps_init (&traps);
    _cairo_traps_limit (&traps, clip_boxes, num_boxes);

    _cairo_polygon_init (&polygon);
    _cairo_polygon_limit (&polygon, clip_boxes, num_boxes);

    if (path->is_empty_fill)
	goto DO_TRAPS;

    is_rectilinear = _cairo_path_fixed_is_rectilinear_fill (path);
    if (is_rectilinear) {
	status = _cairo_path_fixed_fill_rectilinear_to_traps (path,
							      fill_rule,
							      &traps);
	if (likely (status == CAIRO_STATUS_SUCCESS))
	    goto DO_TRAPS;

	if (_cairo_status_is_error (status))
	    goto CLEANUP;
    }

    status = _cairo_path_fixed_fill_to_polygon (path, tolerance, &polygon);
    if (unlikely (status))
	goto CLEANUP;

    if (polygon.num_edges == 0)
	goto DO_TRAPS;

    if (_cairo_operator_bounded_by_mask (op)) {
	_cairo_box_round_to_rectangle (&polygon.extents, &extents.mask);
	if (! _cairo_rectangle_intersect (&extents.bounded, &extents.mask))
	    goto CLEANUP;
    }

    if (is_rectilinear) {
	status = _cairo_bentley_ottmann_tessellate_rectilinear_polygon (&traps,
									&polygon,
									fill_rule);
	if (likely (status == CAIRO_STATUS_SUCCESS))
	    goto DO_TRAPS;

	if (unlikely (_cairo_status_is_error (status)))
	    goto CLEANUP;
    }

    /* Fall back to trapezoid fills. */
    status = _cairo_bentley_ottmann_tessellate_polygon (&traps,
							&polygon,
							fill_rule);
    if (unlikely (status))
	goto CLEANUP;

  DO_TRAPS:
    status = _clip_and_composite_trapezoids (source, op, surface,
					     &traps, antialias,
					     clip,
                                             extents.is_bounded ? &extents.bounded : &extents.unbounded);
  CLEANUP:
    _cairo_traps_fini (&traps);
    _cairo_polygon_fini (&polygon);
    if (clip_boxes != boxes_stack)
	free (clip_boxes);

    return status;
}

typedef struct {
    cairo_scaled_font_t *font;
    cairo_glyph_t *glyphs;
    int num_glyphs;
} cairo_show_glyphs_info_t;

static cairo_status_t
_cairo_surface_old_show_glyphs_draw_func (void                          *closure,
					  cairo_operator_t               op,
					  const cairo_pattern_t         *src,
					  cairo_surface_t               *dst,
					  int                            dst_x,
					  int                            dst_y,
					  const cairo_rectangle_int_t	*extents,
					  cairo_region_t		*clip_region)
{
    cairo_show_glyphs_info_t *glyph_info = closure;
    cairo_status_t status;
    cairo_region_t *extents_region = NULL;

    if (clip_region == NULL &&
        !_cairo_operator_bounded_by_source (op)) {
        extents_region = cairo_region_create_rectangle (extents);
        if (unlikely (extents_region->status))
            return extents_region->status;
        cairo_region_translate (extents_region, -dst_x, -dst_y);
        clip_region = extents_region;
    }

    /* Modifying the glyph array is fine because we know that this function
     * will be called only once, and we've already made a copy of the
     * glyphs in the wrapper.
     */
    if (dst_x != 0 || dst_y != 0) {
	int i;

	for (i = 0; i < glyph_info->num_glyphs; ++i) {
	    ((cairo_glyph_t *) glyph_info->glyphs)[i].x -= dst_x;
	    ((cairo_glyph_t *) glyph_info->glyphs)[i].y -= dst_y;
	}
    }

    status = _cairo_surface_old_show_glyphs (glyph_info->font, op, src,
					     dst,
					     extents->x, extents->y,
					     extents->x - dst_x,
					     extents->y - dst_y,
					     extents->width,
					     extents->height,
					     glyph_info->glyphs,
					     glyph_info->num_glyphs,
					     clip_region);

    if (status == CAIRO_INT_STATUS_UNSUPPORTED) {
	status = _cairo_scaled_font_show_glyphs (glyph_info->font,
                                                 op,
                                                 src, dst,
                                                 extents->x,         extents->y,
                                                 extents->x - dst_x,
                                                 extents->y - dst_y,
                                                 extents->width,     extents->height,
                                                 glyph_info->glyphs,
                                                 glyph_info->num_glyphs,
                                                 clip_region);
    }

    if (extents_region)
        cairo_region_destroy (extents_region);

    return status;
}

cairo_status_t
_cairo_surface_fallback_show_glyphs (cairo_surface_t		*surface,
				     cairo_operator_t		 op,
				     const cairo_pattern_t	*source,
				     cairo_glyph_t		*glyphs,
				     int			 num_glyphs,
				     cairo_scaled_font_t	*scaled_font,
				     cairo_clip_t		*clip)
{
    cairo_show_glyphs_info_t glyph_info;
    cairo_composite_rectangles_t extents;
    cairo_rectangle_int_t rect;
    cairo_status_t status;

    if (!_cairo_surface_get_extents (surface, &rect))
        ASSERT_NOT_REACHED;

    status = _cairo_composite_rectangles_init_for_glyphs (&extents,
							  &rect,
							  op, source,
							  scaled_font,
							  glyphs, num_glyphs,
							  clip,
							  NULL);
    if (unlikely (status))
	return status;

    if (_cairo_clip_contains_rectangle (clip, &extents.mask))
	clip = NULL;

    if (clip != NULL && extents.is_bounded) {
	status = _cairo_clip_rectangle (clip, &extents.bounded);
	if (unlikely (status))
	    return status;
    }

    glyph_info.font = scaled_font;
    glyph_info.glyphs = glyphs;
    glyph_info.num_glyphs = num_glyphs;

    return _clip_and_composite (clip, op, source,
				_cairo_surface_old_show_glyphs_draw_func,
				&glyph_info,
				surface,
                                extents.is_bounded ? &extents.bounded : &extents.unbounded);
}

cairo_surface_t *
_cairo_surface_fallback_snapshot (cairo_surface_t *surface)
{
    cairo_surface_t *snapshot;
    cairo_status_t status;
    cairo_format_t format;
    cairo_surface_pattern_t pattern;
    cairo_image_surface_t *image;
    void *image_extra;

    status = _cairo_surface_acquire_source_image (surface,
						  &image, &image_extra);
    if (unlikely (status))
	return _cairo_surface_create_in_error (status);

    format = image->format;
    if (format == CAIRO_FORMAT_INVALID) {
	/* Non-standard images formats can be generated when retrieving
	 * images from unusual xservers, for example.
	 */
	format = _cairo_format_from_content (image->base.content);
    }
    snapshot = cairo_image_surface_create (format,
					   image->width,
					   image->height);
    if (cairo_surface_status (snapshot)) {
	_cairo_surface_release_source_image (surface, image, image_extra);
	return snapshot;
    }

    _cairo_pattern_init_for_surface (&pattern, &image->base);
    status = _cairo_surface_paint (snapshot,
				   CAIRO_OPERATOR_SOURCE,
				   &pattern.base,
				   NULL);
    _cairo_pattern_fini (&pattern.base);
    _cairo_surface_release_source_image (surface, image, image_extra);
    if (unlikely (status)) {
	cairo_surface_destroy (snapshot);
	return _cairo_surface_create_in_error (status);
    }

    return snapshot;
}

cairo_status_t
_cairo_surface_fallback_composite (cairo_operator_t		 op,
				   const cairo_pattern_t	*src,
				   const cairo_pattern_t	*mask,
				   cairo_surface_t		*dst,
				   int				 src_x,
				   int				 src_y,
				   int				 mask_x,
				   int				 mask_y,
				   int				 dst_x,
				   int				 dst_y,
				   unsigned int			 width,
				   unsigned int			 height,
				   cairo_region_t		*clip_region)
{
    fallback_state_t state;
    cairo_region_t *fallback_region = NULL;
    cairo_status_t status;

    status = _fallback_init (&state, dst, dst_x, dst_y, width, height);
    if (unlikely (status))
	return status;

    /* We know this will never fail with the image backend; but
     * instead of calling into it directly, we call
     * _cairo_surface_composite so that we get the correct device
     * offset handling.
     */

    if (clip_region != NULL && (state.image_rect.x || state.image_rect.y)) {
	fallback_region = cairo_region_copy (clip_region);
	status = fallback_region->status;
	if (unlikely (status))
	    goto FAIL;

	cairo_region_translate (fallback_region,
				-state.image_rect.x,
				-state.image_rect.y);
	clip_region = fallback_region;
    }

    status = _cairo_surface_composite (op, src, mask,
				       &state.image->base,
				       src_x, src_y, mask_x, mask_y,
				       dst_x - state.image_rect.x,
				       dst_y - state.image_rect.y,
				       width, height,
				       clip_region);
  FAIL:
    if (fallback_region != NULL)
	cairo_region_destroy (fallback_region);
    _fallback_fini (&state);

    return status;
}

cairo_status_t
_cairo_surface_fallback_fill_rectangles (cairo_surface_t         *surface,
					 cairo_operator_t	  op,
					 const cairo_color_t	 *color,
					 cairo_rectangle_int_t   *rects,
					 int			  num_rects)
{
    fallback_state_t state;
    cairo_rectangle_int_t *offset_rects = NULL;
    cairo_status_t status;
    int x1, y1, x2, y2;
    int i;

    assert (surface->snapshot_of == NULL);

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
	if (rects[i].x < x1)
	    x1 = rects[i].x;
	if (rects[i].y < y1)
	    y1 = rects[i].y;

	if ((int) (rects[i].x + rects[i].width) > x2)
	    x2 = rects[i].x + rects[i].width;
	if ((int) (rects[i].y + rects[i].height) > y2)
	    y2 = rects[i].y + rects[i].height;
    }

    status = _fallback_init (&state, surface, x1, y1, x2 - x1, y2 - y1);
    if (unlikely (status))
	return status;

    /* If the fetched image isn't at 0,0, we need to offset the rectangles */

    if (state.image_rect.x != 0 || state.image_rect.y != 0) {
	offset_rects = _cairo_malloc_ab (num_rects, sizeof (cairo_rectangle_int_t));
	if (unlikely (offset_rects == NULL)) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto DONE;
	}

	for (i = 0; i < num_rects; i++) {
	    offset_rects[i].x = rects[i].x - state.image_rect.x;
	    offset_rects[i].y = rects[i].y - state.image_rect.y;
	    offset_rects[i].width = rects[i].width;
	    offset_rects[i].height = rects[i].height;
	}

	rects = offset_rects;
    }

    status = _cairo_surface_fill_rectangles (&state.image->base,
					     op, color,
					     rects, num_rects);

    free (offset_rects);

 DONE:
    _fallback_fini (&state);

    return status;
}

cairo_status_t
_cairo_surface_fallback_composite_trapezoids (cairo_operator_t		op,
					      const cairo_pattern_t    *pattern,
					      cairo_surface_t	       *dst,
					      cairo_antialias_t		antialias,
					      int			src_x,
					      int			src_y,
					      int			dst_x,
					      int			dst_y,
					      unsigned int		width,
					      unsigned int		height,
					      cairo_trapezoid_t	       *traps,
					      int			num_traps,
					      cairo_region_t		*clip_region)
{
    fallback_state_t state;
    cairo_region_t *fallback_region = NULL;
    cairo_trapezoid_t *offset_traps = NULL;
    cairo_status_t status;

    status = _fallback_init (&state, dst, dst_x, dst_y, width, height);
    if (unlikely (status))
	return status;

    /* If the destination image isn't at 0,0, we need to offset the trapezoids */

    if (state.image_rect.x != 0 || state.image_rect.y != 0) {
	offset_traps = _cairo_malloc_ab (num_traps, sizeof (cairo_trapezoid_t));
	if (offset_traps == NULL) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto FAIL;
	}

	_cairo_trapezoid_array_translate_and_scale (offset_traps, traps, num_traps,
                                                    - state.image_rect.x, - state.image_rect.y,
                                                    1.0, 1.0);
	traps = offset_traps;

	/* similarly we need to adjust the region */
	if (clip_region != NULL) {
	    fallback_region = cairo_region_copy (clip_region);
	    status = fallback_region->status;
	    if (unlikely (status))
		goto FAIL;

	    cairo_region_translate (fallback_region,
				    -state.image_rect.x,
				    -state.image_rect.y);
	    clip_region = fallback_region;
	}
    }

    status = _cairo_surface_composite_trapezoids (op, pattern,
					          &state.image->base,
						  antialias,
						  src_x, src_y,
						  dst_x - state.image_rect.x,
						  dst_y - state.image_rect.y,
						  width, height,
						  traps, num_traps,
						  clip_region);
 FAIL:
    if (offset_traps != NULL)
	free (offset_traps);

    if (fallback_region != NULL)
	cairo_region_destroy (fallback_region);

    _fallback_fini (&state);

    return status;
}

cairo_status_t
_cairo_surface_fallback_clone_similar (cairo_surface_t	*surface,
				       cairo_surface_t	*src,
				       int		 src_x,
				       int		 src_y,
				       int		 width,
				       int		 height,
				       int		*clone_offset_x,
				       int		*clone_offset_y,
				       cairo_surface_t **clone_out)
{
    cairo_surface_t *new_surface;
    cairo_surface_pattern_t pattern;
    cairo_status_t status;

    new_surface = _cairo_surface_create_similar_scratch (surface,
							 src->content,
							 width, height);
    if (new_surface == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;
    if (unlikely (new_surface->status))
	return new_surface->status;

    /* We have to copy these here, so that the coordinate spaces are correct */
    new_surface->device_transform = src->device_transform;
    new_surface->device_transform_inverse = src->device_transform_inverse;

    _cairo_pattern_init_for_surface (&pattern, src);
    cairo_matrix_init_translate (&pattern.base.matrix, src_x, src_y);
    pattern.base.filter = CAIRO_FILTER_NEAREST;

    status = _cairo_surface_paint (new_surface,
				   CAIRO_OPERATOR_SOURCE,
				   &pattern.base,
				   NULL);
    _cairo_pattern_fini (&pattern.base);

    if (unlikely (status)) {
	cairo_surface_destroy (new_surface);
	return status;
    }

    *clone_offset_x = src_x;
    *clone_offset_y = src_y;
    *clone_out = new_surface;
    return CAIRO_STATUS_SUCCESS;
}
