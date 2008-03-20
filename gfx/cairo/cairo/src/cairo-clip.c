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
 *	Kristian Høgsberg <krh@redhat.com>
 */

#include "cairoint.h"
#include "cairo-clip-private.h"

static cairo_clip_path_t *
_cairo_clip_path_reference (cairo_clip_path_t *clip_path);

static void
_cairo_clip_path_destroy (cairo_clip_path_t *clip_path);

void
_cairo_clip_init (cairo_clip_t *clip, cairo_surface_t *target)
{
    if (target)
	clip->mode = _cairo_surface_get_clip_mode (target);
    else
	clip->mode = CAIRO_CLIP_MODE_MASK;

    clip->all_clipped = FALSE;

    clip->surface = NULL;
    clip->surface_rect.x = 0;
    clip->surface_rect.y = 0;
    clip->surface_rect.width = 0;
    clip->surface_rect.height = 0;

    clip->serial = 0;

    _cairo_region_init (&clip->region);
    clip->has_region = FALSE;

    clip->path = NULL;
}

cairo_status_t
_cairo_clip_init_copy (cairo_clip_t *clip, cairo_clip_t *other)
{
    clip->mode = other->mode;

    clip->all_clipped = other->all_clipped;

    clip->surface = cairo_surface_reference (other->surface);
    clip->surface_rect = other->surface_rect;

    clip->serial = other->serial;

    _cairo_region_init (&clip->region);

    if (other->has_region) {
	cairo_status_t status;
	status = _cairo_region_copy (&clip->region, &other->region);
	if (status) {
	    _cairo_region_fini (&clip->region);
	    cairo_surface_destroy (clip->surface);
	    return status;
	}
        clip->has_region = TRUE;
    } else {
        clip->has_region = FALSE;
    }

    clip->path = _cairo_clip_path_reference (other->path);

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_clip_reset (cairo_clip_t *clip)
{
    clip->all_clipped = FALSE;

    /* destroy any existing clip-region artifacts */
    cairo_surface_destroy (clip->surface);
    clip->surface = NULL;

    clip->serial = 0;

    if (clip->has_region) {
        /* _cairo_region_fini just releases the resources used but
         * doesn't bother with leaving the region in a valid state.
         * So _cairo_region_init has to be called afterwards. */
	_cairo_region_fini (&clip->region);
        _cairo_region_init (&clip->region);

        clip->has_region = FALSE;
    }

    _cairo_clip_path_destroy (clip->path);
    clip->path = NULL;
}

static void
_cairo_clip_set_all_clipped (cairo_clip_t *clip, cairo_surface_t *target)
{
    _cairo_clip_reset (clip);

    clip->all_clipped = TRUE;
    clip->serial = _cairo_surface_allocate_clip_serial (target);
}


static cairo_status_t
_cairo_clip_path_intersect_to_rectangle (cairo_clip_path_t       *clip_path,
				         cairo_rectangle_int_t   *rectangle)
{
    while (clip_path) {
        cairo_status_t status;
        cairo_traps_t traps;
        cairo_box_t extents;
        cairo_rectangle_int_t extents_rect;

        _cairo_traps_init (&traps);

        status = _cairo_path_fixed_fill_to_traps (&clip_path->path,
                                                  clip_path->fill_rule,
                                                  clip_path->tolerance,
                                                  &traps);
        if (status) {
            _cairo_traps_fini (&traps);
            return status;
        }

        _cairo_traps_extents (&traps, &extents);
        _cairo_box_round_to_rectangle (&extents, &extents_rect);
        _cairo_rectangle_intersect (rectangle, &extents_rect);

        _cairo_traps_fini (&traps);

        clip_path = clip_path->prev;
    }

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_clip_intersect_to_rectangle (cairo_clip_t            *clip,
				    cairo_rectangle_int_t *rectangle)
{
    if (!clip)
	return CAIRO_STATUS_SUCCESS;

    if (clip->all_clipped) {
	*rectangle = clip->surface_rect;
	return CAIRO_STATUS_SUCCESS;
    }

    if (clip->path) {
        cairo_status_t status;

        status = _cairo_clip_path_intersect_to_rectangle (clip->path,
                                                          rectangle);
        if (status)
            return status;
    }

    if (clip->has_region) {
	cairo_status_t status = CAIRO_STATUS_SUCCESS;
	cairo_region_t intersection;

	_cairo_region_init_rect (&intersection, rectangle);

	status = _cairo_region_intersect (&intersection, &clip->region,
					  &intersection);

	if (!status)
	    _cairo_region_get_extents (&intersection, rectangle);

        _cairo_region_fini (&intersection);

        if (status)
            return status;
    }

    if (clip->surface)
	_cairo_rectangle_intersect (rectangle, &clip->surface_rect);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_clip_intersect_to_region (cairo_clip_t      *clip,
				 cairo_region_t *region)
{
    cairo_status_t status;

    if (!clip)
	return CAIRO_STATUS_SUCCESS;

    if (clip->all_clipped) {
	cairo_region_t clip_rect;

	_cairo_region_init_rect (&clip_rect, &clip->surface_rect);

	status = _cairo_region_intersect (region, &clip_rect, region);

	_cairo_region_fini (&clip_rect);

	return status;
    }

    if (clip->path) {
	/* Intersect clip path into region. */
    }

    if (clip->has_region) {
	status = _cairo_region_intersect (region, &clip->region, region);
	if (status)
	    return status;
    }

    if (clip->surface) {
	cairo_region_t clip_rect;

	_cairo_region_init_rect (&clip_rect, &clip->surface_rect);

	status = _cairo_region_intersect (region, &clip_rect, region);

	_cairo_region_fini (&clip_rect);

        if (status)
            return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

/* Combines the region of clip->surface given by extents in
 * device backend coordinates into the given temporary surface,
 * which has its origin at dst_x, dst_y in backend coordinates
 */
cairo_status_t
_cairo_clip_combine_to_surface (cairo_clip_t                  *clip,
				cairo_operator_t              op,
				cairo_surface_t               *dst,
				int                           dst_x,
				int                           dst_y,
				const cairo_rectangle_int_t *extents)
{
    cairo_pattern_union_t pattern;
    cairo_status_t status;

    if (clip->all_clipped)
	return CAIRO_STATUS_SUCCESS;

    _cairo_pattern_init_for_surface (&pattern.surface, clip->surface);

    status = _cairo_surface_composite (op,
				       &pattern.base,
				       NULL,
				       dst,
				       extents->x - clip->surface_rect.x,
				       extents->y - clip->surface_rect.y,
				       0, 0,
				       extents->x - dst_x,
				       extents->y - dst_y,
				       extents->width, extents->height);

    _cairo_pattern_fini (&pattern.base);

    return status;
}

static cairo_status_t
_cairo_clip_intersect_path (cairo_clip_t       *clip,
			    cairo_path_fixed_t *path,
			    cairo_fill_rule_t   fill_rule,
			    double              tolerance,
			    cairo_antialias_t   antialias)
{
    cairo_clip_path_t *clip_path;
    cairo_status_t status;

    if (clip->mode != CAIRO_CLIP_MODE_PATH)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    clip_path = malloc (sizeof (cairo_clip_path_t));
    if (clip_path == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    status = _cairo_path_fixed_init_copy (&clip_path->path, path);
    if (status) {
	free (clip_path);
	return status;
    }

    CAIRO_REFERENCE_COUNT_INIT (&clip_path->ref_count, 1);
    clip_path->fill_rule = fill_rule;
    clip_path->tolerance = tolerance;
    clip_path->antialias = antialias;
    clip_path->prev = clip->path;
    clip->path = clip_path;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_clip_path_t *
_cairo_clip_path_reference (cairo_clip_path_t *clip_path)
{
    if (clip_path == NULL)
	return NULL;

    assert (CAIRO_REFERENCE_COUNT_HAS_REFERENCE (&clip_path->ref_count));

    _cairo_reference_count_inc (&clip_path->ref_count);

    return clip_path;
}

static void
_cairo_clip_path_destroy (cairo_clip_path_t *clip_path)
{
    if (clip_path == NULL)
	return;

    assert (CAIRO_REFERENCE_COUNT_HAS_REFERENCE (&clip_path->ref_count));

    if (! _cairo_reference_count_dec_and_test (&clip_path->ref_count))
	return;

    _cairo_path_fixed_fini (&clip_path->path);
    _cairo_clip_path_destroy (clip_path->prev);
    free (clip_path);
}


static cairo_int_status_t
_cairo_clip_intersect_region (cairo_clip_t    *clip,
			      cairo_traps_t   *traps,
			      cairo_surface_t *target)
{
    cairo_region_t region;
    cairo_int_status_t status;

    if (clip->all_clipped)
	return CAIRO_STATUS_SUCCESS;

    if (clip->mode != CAIRO_CLIP_MODE_REGION)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = _cairo_traps_extract_region (traps, &region);

    if (status)
	return status;

    status = CAIRO_STATUS_SUCCESS;

    if (!clip->has_region) {
        status = _cairo_region_copy (&clip->region, &region);
	if (status == CAIRO_STATUS_SUCCESS)
	    clip->has_region = TRUE;
    } else {
	cairo_region_t intersection;
        _cairo_region_init (&intersection);

	status = _cairo_region_intersect (&intersection,
		                         &clip->region,
		                         &region);

	if (status == CAIRO_STATUS_SUCCESS)
	    status = _cairo_region_copy (&clip->region, &intersection);

        _cairo_region_fini (&intersection);
    }

    clip->serial = _cairo_surface_allocate_clip_serial (target);
    _cairo_region_fini (&region);

    if (! _cairo_region_not_empty (&clip->region))
	_cairo_clip_set_all_clipped (clip, target);

    return status;
}

static cairo_status_t
_cairo_clip_intersect_mask (cairo_clip_t      *clip,
			    cairo_traps_t     *traps,
			    cairo_antialias_t antialias,
			    cairo_surface_t   *target)
{
    cairo_pattern_union_t pattern;
    cairo_box_t extents;
    cairo_rectangle_int_t surface_rect, target_rect;
    cairo_surface_t *surface;
    cairo_status_t status;

    if (clip->all_clipped)
	return CAIRO_STATUS_SUCCESS;

    /* Represent the clip as a mask surface.  We create a new surface
     * the size of the intersection of the old mask surface and the
     * extents of the new clip path. */

    _cairo_traps_extents (traps, &extents);
    _cairo_box_round_to_rectangle (&extents, &surface_rect);

    if (clip->surface != NULL)
	_cairo_rectangle_intersect (&surface_rect, &clip->surface_rect);

    /* Intersect with the target surface rectangle so we don't use
     * more memory and time than we need to. */

    status = _cairo_surface_get_extents (target, &target_rect);
    if (!status)
	_cairo_rectangle_intersect (&surface_rect, &target_rect);

    if (surface_rect.width == 0 || surface_rect.height == 0) {
	surface = NULL;
	status = CAIRO_STATUS_SUCCESS;
	if (clip->surface != NULL)
	    cairo_surface_destroy (clip->surface);
	goto DONE;
    }

    _cairo_pattern_init_solid (&pattern.solid, CAIRO_COLOR_WHITE,
			       CAIRO_CONTENT_COLOR);
    /* The clipping operation should ideally be something like the following to
     * avoid having to do as many passes over the data

	if (clip->surface != NULL) {
	    _cairo_pattern_init_for_surface (&pattern.surface, clip->surface);
	} else {
	    _cairo_pattern_init_solid (&pattern.solid, CAIRO_COLOR_WHITE,
			       CAIRO_CONTENT_COLOR);
	}
	status = _cairo_surface_composite_trapezoids (CAIRO_OPERATOR_IN,
						  &pattern.base,
						  surface,
						  antialias,
						  0, 0,
						  0, 0,
						  surface_rect.width,
						  surface_rect.height,
						  traps->traps,
						  traps->num_traps);

	However this operation is not accelerated by pixman

	I believe the best possible operation would probably an unbounded SRC
	operator.  Using SRC we could potentially avoid having to initialize
	the surface which would be ideal from an efficiency point of view.
	However, _cairo_surface_composite_trapezoids (CAIRO_OPERATOR_SOURCE) is
	bounded by the mask.

    */

    surface = _cairo_surface_create_similar_solid (target,
						   CAIRO_CONTENT_ALPHA,
						   surface_rect.width,
						   surface_rect.height,
						   CAIRO_COLOR_TRANSPARENT,
						   &pattern.base);
    if (surface->status) {
	_cairo_pattern_fini (&pattern.base);
	return surface->status;
    }

    /* Render the new clipping path into the new mask surface. */

    _cairo_traps_translate (traps, -surface_rect.x, -surface_rect.y);

    status = _cairo_surface_composite_trapezoids (CAIRO_OPERATOR_ADD,
						  &pattern.base,
						  surface,
						  antialias,
						  0, 0,
						  0, 0,
						  surface_rect.width,
						  surface_rect.height,
						  traps->traps,
						  traps->num_traps);

    _cairo_pattern_fini (&pattern.base);

    if (status) {
	cairo_surface_destroy (surface);
	return status;
    }

    /* If there was a clip surface already, combine it with the new
     * mask surface using the IN operator, so we get the intersection
     * of the old and new clipping paths. */

    if (clip->surface != NULL) {
	_cairo_pattern_init_for_surface (&pattern.surface, clip->surface);

	status = _cairo_surface_composite (CAIRO_OPERATOR_IN,
					   &pattern.base,
					   NULL,
					   surface,
					   surface_rect.x - clip->surface_rect.x,
					   surface_rect.y - clip->surface_rect.y,
					   0, 0,
					   0, 0,
					   surface_rect.width,
					   surface_rect.height);

	_cairo_pattern_fini (&pattern.base);

	if (status) {
	    cairo_surface_destroy (surface);
	    return status;
	}

	cairo_surface_destroy (clip->surface);
    }

 DONE:
    clip->surface = surface;
    clip->surface_rect = surface_rect;
    clip->serial = _cairo_surface_allocate_clip_serial (target);

    if (surface_rect.width == 0 || surface_rect.height == 0)
	_cairo_clip_set_all_clipped (clip, target);

    return status;
}

cairo_status_t
_cairo_clip_clip (cairo_clip_t       *clip,
		  cairo_path_fixed_t *path,
		  cairo_fill_rule_t   fill_rule,
		  double              tolerance,
		  cairo_antialias_t   antialias,
		  cairo_surface_t    *target)
{
    cairo_status_t status;
    cairo_traps_t traps;

    if (clip->all_clipped)
	return CAIRO_STATUS_SUCCESS;

    /* catch the empty clip path */
    if (! path->has_current_point) {
	_cairo_clip_set_all_clipped (clip, target);
	return CAIRO_STATUS_SUCCESS;
    }

    status = _cairo_clip_intersect_path (clip,
					 path, fill_rule, tolerance,
					 antialias);
    if (status == CAIRO_STATUS_SUCCESS)
        clip->serial = _cairo_surface_allocate_clip_serial (target);

    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;

    _cairo_traps_init (&traps);
    status = _cairo_path_fixed_fill_to_traps (path,
					      fill_rule,
					      tolerance,
					      &traps);
    if (status)
	goto bail;

    status = _cairo_clip_intersect_region (clip, &traps, target);
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	goto bail;

    status = _cairo_clip_intersect_mask (clip, &traps, antialias, target);

 bail:
    _cairo_traps_fini (&traps);

    return status;
}

void
_cairo_clip_translate (cairo_clip_t  *clip,
                       cairo_fixed_t  tx,
                       cairo_fixed_t  ty)
{
    if (clip->all_clipped)
	return;

    if (clip->has_region) {
        _cairo_region_translate (&clip->region,
                                 _cairo_fixed_integer_part (tx),
                                 _cairo_fixed_integer_part (ty));
    }

    if (clip->surface) {
        clip->surface_rect.x += _cairo_fixed_integer_part (tx);
        clip->surface_rect.y += _cairo_fixed_integer_part (ty);
    }

    if (clip->path) {
        cairo_clip_path_t *clip_path = clip->path;
	cairo_matrix_t matrix;

	cairo_matrix_init_translate (&matrix,
				     _cairo_fixed_to_double (tx),
				     _cairo_fixed_to_double (ty));

        while (clip_path) {
            _cairo_path_fixed_device_transform (&clip_path->path, &matrix);
            clip_path = clip_path->prev;
        }
    }
}

static cairo_status_t
_cairo_clip_path_reapply_clip_path (cairo_clip_t      *clip,
                                    cairo_clip_path_t *clip_path)
{
    cairo_status_t status;

    if (clip_path->prev) {
        status = _cairo_clip_path_reapply_clip_path (clip, clip_path->prev);
	if (status && status != CAIRO_INT_STATUS_UNSUPPORTED)
	    return status;
    }

    return _cairo_clip_intersect_path (clip,
                                       &clip_path->path,
				       clip_path->fill_rule,
				       clip_path->tolerance,
				       clip_path->antialias);
}

cairo_status_t
_cairo_clip_init_deep_copy (cairo_clip_t    *clip,
                            cairo_clip_t    *other,
                            cairo_surface_t *target)
{
    cairo_status_t status;

    _cairo_clip_init (clip, target);

    if (other->mode != clip->mode) {
        /* We should reapply the original clip path in this case, and let
         * whatever the right handling is happen */
    } else {
        if (other->has_region) {
            status = _cairo_region_copy (&clip->region, &other->region);
	    if (status)
		goto BAIL;

	    clip->has_region = TRUE;
        }

        if (other->surface) {
            status = _cairo_surface_clone_similar (target, other->surface,
					           other->surface_rect.x,
						   other->surface_rect.y,
						   other->surface_rect.width,
						   other->surface_rect.height,
						   &clip->surface);
	    if (status)
		goto BAIL;

            clip->surface_rect = other->surface_rect;
        }

        if (other->path) {
            status = _cairo_clip_path_reapply_clip_path (clip, other->path);
	    if (status && status != CAIRO_INT_STATUS_UNSUPPORTED)
		goto BAIL;
        }
    }

    return CAIRO_STATUS_SUCCESS;

BAIL:
    if (clip->has_region)
	_cairo_region_fini (&clip->region);
    if (clip->surface)
	cairo_surface_destroy (clip->surface);

    return status;
}

const cairo_rectangle_list_t _cairo_rectangles_nil =
  { CAIRO_STATUS_NO_MEMORY, NULL, 0 };
static const cairo_rectangle_list_t _cairo_rectangles_not_representable =
  { CAIRO_STATUS_CLIP_NOT_REPRESENTABLE, NULL, 0 };

static cairo_bool_t
_cairo_clip_int_rect_to_user (cairo_gstate_t *gstate,
			      cairo_rectangle_int_t *clip_rect,
			      cairo_rectangle_t *user_rect)
{
    cairo_bool_t is_tight;

    double x1 = clip_rect->x;
    double y1 = clip_rect->y;
    double x2 = clip_rect->x + clip_rect->width;
    double y2 = clip_rect->y + clip_rect->height;

    _cairo_gstate_backend_to_user_rectangle (gstate, &x1, &y1, &x2, &y2, &is_tight);

    user_rect->x = x1;
    user_rect->y = y1;
    user_rect->width = x2 - x1;
    user_rect->height = y2 - y1;

    return is_tight;
}

cairo_rectangle_list_t *
_cairo_clip_copy_rectangle_list (cairo_clip_t *clip, cairo_gstate_t *gstate)
{
    cairo_rectangle_list_t *list;
    cairo_rectangle_t *rectangles = NULL;
    int n_boxes = 0;

    if (clip->all_clipped)
	goto DONE;

    if (clip->path || clip->surface) {
	_cairo_error_throw (CAIRO_STATUS_CLIP_NOT_REPRESENTABLE);
	return (cairo_rectangle_list_t*) &_cairo_rectangles_not_representable;
    }

    if (clip->has_region) {
	cairo_box_int_t *boxes;
        int i;

	if (_cairo_region_get_boxes (&clip->region, &n_boxes, &boxes))
	    return (cairo_rectangle_list_t*) &_cairo_rectangles_nil;

	if (n_boxes) {
	    rectangles = _cairo_malloc_ab (n_boxes, sizeof (cairo_rectangle_t));
	    if (rectangles == NULL) {
		_cairo_region_boxes_fini (&clip->region, boxes);
		_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
		return (cairo_rectangle_list_t*) &_cairo_rectangles_nil;
	    }

	    for (i = 0; i < n_boxes; ++i) {
		cairo_rectangle_int_t clip_rect = { boxes[i].p1.x, boxes[i].p1.y,
						    boxes[i].p2.x - boxes[i].p1.x,
						    boxes[i].p2.y - boxes[i].p1.y };

		if (!_cairo_clip_int_rect_to_user(gstate, &clip_rect, &rectangles[i])) {
		    _cairo_error_throw (CAIRO_STATUS_CLIP_NOT_REPRESENTABLE);
		    _cairo_region_boxes_fini (&clip->region, boxes);
		    free (rectangles);
		    return (cairo_rectangle_list_t*) &_cairo_rectangles_not_representable;
		}
	    }
	}

	_cairo_region_boxes_fini (&clip->region, boxes);
    } else {
        cairo_rectangle_int_t extents;

	n_boxes = 1;

	rectangles = malloc(sizeof (cairo_rectangle_t));
	if (rectangles == NULL) {
	    _cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	    return (cairo_rectangle_list_t*) &_cairo_rectangles_nil;
	}

	if (_cairo_surface_get_extents (_cairo_gstate_get_target (gstate), &extents) ||
	    !_cairo_clip_int_rect_to_user(gstate, &extents, rectangles))
	{
	    _cairo_error_throw (CAIRO_STATUS_CLIP_NOT_REPRESENTABLE);
	    free (rectangles);
	    return (cairo_rectangle_list_t*) &_cairo_rectangles_not_representable;
	}
    }

 DONE:
    list = malloc (sizeof (cairo_rectangle_list_t));
    if (list == NULL) {
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
        free (rectangles);
        return (cairo_rectangle_list_t*) &_cairo_rectangles_nil;
    }

    list->status = CAIRO_STATUS_SUCCESS;
    list->rectangles = rectangles;
    list->num_rectangles = n_boxes;
    return list;
}

/**
 * cairo_rectangle_list_destroy:
 * @rectangle_list: a rectangle list, as obtained from cairo_copy_clip_rectangles()
 *
 * Unconditionally frees @rectangle_list and all associated
 * references. After this call, the @rectangle_list pointer must not
 * be dereferenced.
 *
 * Since: 1.4
 **/
void
cairo_rectangle_list_destroy (cairo_rectangle_list_t *rectangle_list)
{
    if (rectangle_list == NULL || rectangle_list == &_cairo_rectangles_nil ||
        rectangle_list == &_cairo_rectangles_not_representable)
        return;

    free (rectangle_list->rectangles);
    free (rectangle_list);
}
