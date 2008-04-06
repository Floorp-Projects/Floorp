/*
 * Copyright © 2006 Keith Packard
 * Copyright © 2007 Adrian Johnson
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
 * The Initial Developer of the Original Code is Keith Packard
 *
 * Contributor(s):
 *      Keith Packard <keithp@keithp.com>
 *      Adrian Johnson <ajohnson@redneon.com>
 */

#include "cairoint.h"

#include "cairo-analysis-surface-private.h"
#include "cairo-paginated-private.h"
#include "cairo-region-private.h"
#include "cairo-meta-surface-private.h"

typedef struct {
    cairo_surface_t base;
    int width;
    int height;

    cairo_surface_t	*target;

    cairo_bool_t first_op;
    cairo_bool_t has_supported;
    cairo_bool_t has_unsupported;

    cairo_region_t supported_region;
    cairo_region_t fallback_region;
    cairo_rectangle_int_t current_clip;
    cairo_box_t page_bbox;

    cairo_bool_t has_ctm;
    cairo_matrix_t ctm;

} cairo_analysis_surface_t;

static cairo_int_status_t
_cairo_analysis_surface_analyze_meta_surface_pattern (cairo_analysis_surface_t *surface,
						      cairo_pattern_t	       *pattern)
{
    cairo_surface_t *analysis = &surface->base;
    cairo_surface_pattern_t *surface_pattern;
    cairo_status_t status;
    cairo_bool_t old_has_ctm;
    cairo_matrix_t old_ctm, p2d;
    cairo_rectangle_int_t old_clip;
    cairo_rectangle_int_t meta_extents;
    int old_width;
    int old_height;

    assert (pattern->type == CAIRO_PATTERN_TYPE_SURFACE);
    surface_pattern = (cairo_surface_pattern_t *) pattern;
    assert (_cairo_surface_is_meta (surface_pattern->surface));

    old_width = surface->width;
    old_height = surface->height;
    old_clip = surface->current_clip;
    status = _cairo_surface_get_extents (surface_pattern->surface, &meta_extents);
    if (status)
	return status;

    surface->width = meta_extents.width;
    surface->height = meta_extents.height;
    surface->current_clip.x = 0;
    surface->current_clip.y = 0;
    surface->current_clip.width = surface->width;
    surface->current_clip.height = surface->height;
    old_ctm = surface->ctm;
    old_has_ctm = surface->has_ctm;
    p2d = pattern->matrix;
    status = cairo_matrix_invert (&p2d);
    /* _cairo_pattern_set_matrix guarantees invertibility */
    assert (status == CAIRO_STATUS_SUCCESS);

    cairo_matrix_multiply (&surface->ctm, &p2d, &surface->ctm);
    surface->has_ctm = !_cairo_matrix_is_identity (&surface->ctm);

    status = _cairo_meta_surface_replay_and_create_regions (surface_pattern->surface,
							    analysis);
    if (status == CAIRO_STATUS_SUCCESS)
	status = analysis->status;

    surface->ctm = old_ctm;
    surface->has_ctm = old_has_ctm;
    surface->current_clip = old_clip;
    surface->width = old_width;
    surface->height = old_height;

    return status;
}

static cairo_int_status_t
_cairo_analysis_surface_add_operation  (cairo_analysis_surface_t *surface,
					cairo_rectangle_int_t    *rect,
					cairo_int_status_t        backend_status)
{
    cairo_int_status_t status;
    cairo_box_t bbox;

    if (rect->width == 0 || rect->height == 0) {
	/* Even though the operation is not visible we must be careful
	 * to not allow unsupported operations to be replayed to the
	 * backend during CAIRO_PAGINATED_MODE_RENDER */
	if (backend_status == CAIRO_STATUS_SUCCESS ||
	    backend_status == CAIRO_INT_STATUS_FLATTEN_TRANSPARENCY)
	{
	    return CAIRO_STATUS_SUCCESS;
	}
	else
	{
	    return CAIRO_INT_STATUS_IMAGE_FALLBACK;
	}
    }

    if (surface->has_ctm) {
	double x1, y1, x2, y2;

	x1 = rect->x;
	y1 = rect->y;
	x2 = rect->x + rect->width;
	y2 = rect->y + rect->height;
	_cairo_matrix_transform_bounding_box (&surface->ctm,
					      &x1, &y1, &x2, &y2,
					      NULL);
	rect->x = floor (x1);
	rect->y = floor (y1);

	x2 = ceil (x2) - rect->x;
	y2 = ceil (y2) - rect->y;
	if (x2 <= 0 || y2 <= 0) {
	    /* Even though the operation is not visible we must be
	     * careful to not allow unsupported operations to be
	     * replayed to the backend during
	     * CAIRO_PAGINATED_MODE_RENDER */
	    if (backend_status == CAIRO_STATUS_SUCCESS ||
		backend_status == CAIRO_INT_STATUS_FLATTEN_TRANSPARENCY)
	    {
		return CAIRO_STATUS_SUCCESS;
	    }
	    else
	    {
		return CAIRO_INT_STATUS_IMAGE_FALLBACK;
	    }
	}

	rect->width  = x2;
	rect->height = y2;
    }

    bbox.p1.x = _cairo_fixed_from_int (rect->x);
    bbox.p1.y = _cairo_fixed_from_int (rect->y);
    bbox.p2.x = _cairo_fixed_from_int (rect->x + rect->width);
    bbox.p2.y = _cairo_fixed_from_int (rect->y + rect->height);

    if (surface->first_op) {
	surface->first_op = FALSE;
	surface->page_bbox = bbox;
    } else {
	if (bbox.p1.x < surface->page_bbox.p1.x)
	    surface->page_bbox.p1.x = bbox.p1.x;
	if (bbox.p1.y < surface->page_bbox.p1.y)
	    surface->page_bbox.p1.y = bbox.p1.y;
	if (bbox.p2.x > surface->page_bbox.p2.x)
	    surface->page_bbox.p2.x = bbox.p2.x;
	if (bbox.p2.y > surface->page_bbox.p2.y)
	    surface->page_bbox.p2.y = bbox.p2.y;
    }

    /* If the operation is completely enclosed within the fallback
     * region there is no benefit in emitting a native operation as
     * the fallback image will be painted on top.
     */
    if (_cairo_region_contains_rectangle (&surface->fallback_region, rect) == PIXMAN_REGION_IN)
	return CAIRO_INT_STATUS_IMAGE_FALLBACK;

    if (backend_status == CAIRO_INT_STATUS_FLATTEN_TRANSPARENCY) {
	/* A status of CAIRO_INT_STATUS_FLATTEN_TRANSPARENCY indicates
	 * that the backend only supports this operation if the
	 * transparency removed. If the extents of this operation does
	 * not intersect any other native operation, the operation is
	 * natively supported and the backend will blend the
	 * transparency into the white background.
	 */
	if (_cairo_region_contains_rectangle (&surface->supported_region, rect) == PIXMAN_REGION_OUT)
	    backend_status = CAIRO_STATUS_SUCCESS;
    }

    if (backend_status == CAIRO_STATUS_SUCCESS) {
	/* Add the operation to the supported region. Operations in
	 * this region will be emitted as native operations.
	 */
	surface->has_supported = TRUE;
	status = _cairo_region_union_rect (&surface->supported_region,
					   &surface->supported_region,
					   rect);
	return status;
    }

    /* Add the operation to the unsupported region. This region will
     * be painted as an image after all native operations have been
     * emitted.
     */
    surface->has_unsupported = TRUE;
    status = _cairo_region_union_rect (&surface->fallback_region,
				       &surface->fallback_region,
				       rect);

    /* The status CAIRO_INT_STATUS_IMAGE_FALLBACK is used to indicate
     * unsupported operations to the meta surface as using
     * CAIRO_INT_STATUS_UNSUPPORTED would cause cairo-surface to
     * invoke the cairo-surface-fallback path then return
     * CAIRO_STATUS_SUCCESS.
     */
    if (status == CAIRO_STATUS_SUCCESS)
	return CAIRO_INT_STATUS_IMAGE_FALLBACK;
    else
	return status;
}

static cairo_status_t
_cairo_analysis_surface_finish (void *abstract_surface)
{
    cairo_analysis_surface_t	*surface = (cairo_analysis_surface_t *) abstract_surface;

    _cairo_region_fini (&surface->supported_region);
    _cairo_region_fini (&surface->fallback_region);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_analysis_surface_intersect_clip_path (void		*abstract_surface,
					     cairo_path_fixed_t *path,
					     cairo_fill_rule_t   fill_rule,
					     double		 tolerance,
					     cairo_antialias_t   antialias)
{
    cairo_analysis_surface_t *surface = abstract_surface;
    double                    x1, y1, x2, y2;
    cairo_rectangle_int_t   extent;

    if (path == NULL) {
	surface->current_clip.x = 0;
	surface->current_clip.y = 0;
	surface->current_clip.width  = surface->width;
	surface->current_clip.height = surface->height;
    } else {
	cairo_status_t status;

	status = _cairo_path_fixed_bounds (path, &x1, &y1, &x2, &y2, tolerance);
	if (status)
	    return status;

	extent.x = floor (x1);
	extent.y = floor (y1);
	extent.width  = ceil (x2) - extent.x;
	extent.height = ceil (y2) - extent.y;

	_cairo_rectangle_intersect (&surface->current_clip, &extent);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_analysis_surface_get_extents (void			*abstract_surface,
				     cairo_rectangle_int_t	*rectangle)
{
    cairo_analysis_surface_t *surface = abstract_surface;

    return _cairo_surface_get_extents (surface->target, rectangle);
}

static cairo_int_status_t
_cairo_analysis_surface_paint (void			*abstract_surface,
			      cairo_operator_t		op,
			      cairo_pattern_t		*source)
{
    cairo_analysis_surface_t *surface = abstract_surface;
    cairo_status_t	     status, backend_status;
    cairo_rectangle_int_t  extents;

    if (!surface->target->backend->paint)
	backend_status = CAIRO_INT_STATUS_UNSUPPORTED;
    else
	backend_status = (*surface->target->backend->paint) (surface->target, op,
                                                             source);

    if (backend_status == CAIRO_INT_STATUS_ANALYZE_META_SURFACE_PATTERN)
	backend_status = _cairo_analysis_surface_analyze_meta_surface_pattern (surface,
									       source);

    status = _cairo_surface_get_extents (&surface->base, &extents);
    if (status)
	return status;

    if (_cairo_operator_bounded_by_source (op)) {
	cairo_rectangle_int_t source_extents;
	status = _cairo_pattern_get_extents (source, &source_extents);
	if (status)
	    return status;

	_cairo_rectangle_intersect (&extents, &source_extents);
    }

    _cairo_rectangle_intersect (&extents, &surface->current_clip);

    status = _cairo_analysis_surface_add_operation (surface, &extents, backend_status);

    return status;
}

static cairo_int_status_t
_cairo_analysis_surface_mask (void		*abstract_surface,
			      cairo_operator_t	 op,
			      cairo_pattern_t	*source,
			      cairo_pattern_t	*mask)
{
    cairo_analysis_surface_t *surface = abstract_surface;
    cairo_status_t	      status, backend_status;
    cairo_rectangle_int_t   extents;

    if (!surface->target->backend->mask)
	backend_status = CAIRO_INT_STATUS_UNSUPPORTED;
    else
	backend_status = (*surface->target->backend->mask) (surface->target, op,
                                                            source, mask);

    if (backend_status == CAIRO_INT_STATUS_ANALYZE_META_SURFACE_PATTERN) {
	if (source->type == CAIRO_PATTERN_TYPE_SURFACE) {
	    cairo_surface_pattern_t *surface_pattern = (cairo_surface_pattern_t *) source;
	    if (_cairo_surface_is_meta (surface_pattern->surface))
		backend_status = _cairo_analysis_surface_analyze_meta_surface_pattern (surface,
										       source);
	    if (backend_status != CAIRO_STATUS_SUCCESS &&
		backend_status != CAIRO_INT_STATUS_IMAGE_FALLBACK)
		return backend_status;
	}

	if (mask->type == CAIRO_PATTERN_TYPE_SURFACE) {
	    cairo_surface_pattern_t *surface_pattern = (cairo_surface_pattern_t *) mask;
	    if (_cairo_surface_is_meta (surface_pattern->surface))
		backend_status = _cairo_analysis_surface_analyze_meta_surface_pattern (surface,
										       mask);
	    if (backend_status != CAIRO_STATUS_SUCCESS &&
		backend_status != CAIRO_INT_STATUS_IMAGE_FALLBACK)
		return backend_status;
	}
    }

    status = _cairo_surface_get_extents (&surface->base, &extents);
    if (status)
	return status;

    if (_cairo_operator_bounded_by_source (op)) {
	cairo_rectangle_int_t source_extents;
	status = _cairo_pattern_get_extents (source, &source_extents);
	if (status)
	    return status;

	_cairo_rectangle_intersect (&extents, &source_extents);

	status = _cairo_pattern_get_extents (mask, &source_extents);
	if (status)
	    return status;

	_cairo_rectangle_intersect (&extents, &source_extents);
    }

    _cairo_rectangle_intersect (&extents, &surface->current_clip);

    status = _cairo_analysis_surface_add_operation (surface, &extents, backend_status);

    return status;
}

static cairo_int_status_t
_cairo_analysis_surface_stroke (void			*abstract_surface,
				cairo_operator_t	 op,
				cairo_pattern_t		*source,
				cairo_path_fixed_t	*path,
				cairo_stroke_style_t	*style,
				cairo_matrix_t		*ctm,
				cairo_matrix_t		*ctm_inverse,
				double			 tolerance,
				cairo_antialias_t	 antialias)
{
    cairo_analysis_surface_t *surface = abstract_surface;
    cairo_status_t	     status, backend_status;
    cairo_traps_t            traps;
    cairo_box_t              box;
    cairo_rectangle_int_t  extents;

    if (!surface->target->backend->stroke)
	backend_status = CAIRO_INT_STATUS_UNSUPPORTED;
    else
	backend_status = (*surface->target->backend->stroke) (surface->target, op,
							      source, path, style,
							      ctm, ctm_inverse,
							      tolerance, antialias);

    if (backend_status == CAIRO_INT_STATUS_ANALYZE_META_SURFACE_PATTERN)
	backend_status = _cairo_analysis_surface_analyze_meta_surface_pattern (surface,
									       source);

    status = _cairo_surface_get_extents (&surface->base, &extents);
    if (status)
	return status;

    if (_cairo_operator_bounded_by_source (op)) {
	cairo_rectangle_int_t source_extents;
	status = _cairo_pattern_get_extents (source, &source_extents);
	if (status)
	    return status;

	_cairo_rectangle_intersect (&extents, &source_extents);
    }

    _cairo_rectangle_intersect (&extents, &surface->current_clip);

    if (_cairo_operator_bounded_by_mask (op)) {
	box.p1.x = _cairo_fixed_from_int (extents.x);
	box.p1.y = _cairo_fixed_from_int (extents.y);
	box.p2.x = _cairo_fixed_from_int (extents.x + extents.width);
	box.p2.y = _cairo_fixed_from_int (extents.y + extents.height);

	_cairo_traps_init (&traps);
	_cairo_traps_limit (&traps, &box);
	status = _cairo_path_fixed_stroke_to_traps (path,
						    style,
						    ctm, ctm_inverse,
						    tolerance,
						    &traps);
	if (status) {
	    _cairo_traps_fini (&traps);
	    return status;
	}

	if (traps.num_traps == 0) {
	    extents.x = 0;
	    extents.y = 0;
	    extents.width = 0;
	    extents.height = 0;
	} else {
	    _cairo_traps_extents (&traps, &box);
	    extents.x = _cairo_fixed_integer_floor (box.p1.x);
	    extents.y = _cairo_fixed_integer_floor (box.p1.y);
	    extents.width = _cairo_fixed_integer_ceil (box.p2.x) - extents.x;
	    extents.height = _cairo_fixed_integer_ceil (box.p2.y) - extents.y;
	}
	_cairo_traps_fini (&traps);
    }

    status = _cairo_analysis_surface_add_operation (surface, &extents, backend_status);

    return status;
}

static cairo_int_status_t
_cairo_analysis_surface_fill (void			*abstract_surface,
			      cairo_operator_t		 op,
			      cairo_pattern_t		*source,
			      cairo_path_fixed_t	*path,
			      cairo_fill_rule_t		 fill_rule,
			      double			 tolerance,
			      cairo_antialias_t		 antialias)
{
    cairo_analysis_surface_t *surface = abstract_surface;
    cairo_status_t	     status, backend_status;
    cairo_traps_t            traps;
    cairo_box_t              box;
    cairo_rectangle_int_t  extents;

    if (!surface->target->backend->fill)
	backend_status = CAIRO_INT_STATUS_UNSUPPORTED;
    else
	backend_status = (*surface->target->backend->fill) (surface->target, op,
						    source, path, fill_rule,
						    tolerance, antialias);

    if (backend_status == CAIRO_INT_STATUS_ANALYZE_META_SURFACE_PATTERN)
	backend_status = _cairo_analysis_surface_analyze_meta_surface_pattern (surface,
									       source);

    status = _cairo_surface_get_extents (&surface->base, &extents);
    if (status)
	return status;

    if (_cairo_operator_bounded_by_source (op)) {
	cairo_rectangle_int_t source_extents;
	status = _cairo_pattern_get_extents (source, &source_extents);
	if (status)
	    return status;

	_cairo_rectangle_intersect (&extents, &source_extents);
    }

    _cairo_rectangle_intersect (&extents, &surface->current_clip);

    if (_cairo_operator_bounded_by_mask (op)) {
	box.p1.x = _cairo_fixed_from_int (extents.x);
	box.p1.y = _cairo_fixed_from_int (extents.y);
	box.p2.x = _cairo_fixed_from_int (extents.x + extents.width);
	box.p2.y = _cairo_fixed_from_int (extents.y + extents.height);

	_cairo_traps_init (&traps);
	_cairo_traps_limit (&traps, &box);
	status = _cairo_path_fixed_fill_to_traps (path,
						  fill_rule,
						  tolerance,
						  &traps);
	if (status) {
	    _cairo_traps_fini (&traps);
	    return status;
	}

	if (traps.num_traps == 0) {
	    extents.x = 0;
	    extents.y = 0;
	    extents.width = 0;
	    extents.height = 0;
	} else {
	    _cairo_traps_extents (&traps, &box);
	    extents.x = _cairo_fixed_integer_floor (box.p1.x);
	    extents.y = _cairo_fixed_integer_floor (box.p1.y);
	    extents.width = _cairo_fixed_integer_ceil (box.p2.x) - extents.x;
	    extents.height = _cairo_fixed_integer_ceil (box.p2.y) - extents.y;
	}

	_cairo_traps_fini (&traps);
    }

    status = _cairo_analysis_surface_add_operation (surface, &extents, backend_status);

    return status;
}

static cairo_int_status_t
_cairo_analysis_surface_show_glyphs (void		  *abstract_surface,
				     cairo_operator_t	   op,
				     cairo_pattern_t	  *source,
				     cairo_glyph_t	  *glyphs,
				     int		   num_glyphs,
				     cairo_scaled_font_t  *scaled_font)
{
    cairo_analysis_surface_t *surface = abstract_surface;
    cairo_status_t	     status, backend_status;
    cairo_rectangle_int_t    extents, glyph_extents;

    if (!surface->target->backend->show_glyphs)
	backend_status = CAIRO_INT_STATUS_UNSUPPORTED;
    else
	backend_status = (*surface->target->backend->show_glyphs) (surface->target, op,
							   source,
							   glyphs, num_glyphs,
							   scaled_font);

    if (backend_status == CAIRO_INT_STATUS_ANALYZE_META_SURFACE_PATTERN)
	backend_status = _cairo_analysis_surface_analyze_meta_surface_pattern (surface,
									       source);

    status = _cairo_surface_get_extents (&surface->base, &extents);
    if (status)
	return status;

    if (_cairo_operator_bounded_by_source (op)) {
	cairo_rectangle_int_t source_extents;
	status = _cairo_pattern_get_extents (source, &source_extents);
	if (status)
	    return status;

	_cairo_rectangle_intersect (&extents, &source_extents);
    }

    _cairo_rectangle_intersect (&extents, &surface->current_clip);

    if (_cairo_operator_bounded_by_mask (op)) {
	status = _cairo_scaled_font_glyph_device_extents (scaled_font,
							  glyphs,
							  num_glyphs,
							  &glyph_extents);
	if (status)
	    return status;

	_cairo_rectangle_intersect (&extents, &glyph_extents);
    }

    status = _cairo_analysis_surface_add_operation (surface, &extents, backend_status);

    return status;
}

static const cairo_surface_backend_t cairo_analysis_surface_backend = {
    CAIRO_INTERNAL_SURFACE_TYPE_ANALYSIS,
    NULL, /* create_similar */
    _cairo_analysis_surface_finish,
    NULL, /* acquire_source_image */
    NULL, /* release_source_image */
    NULL, /* acquire_dest_image */
    NULL, /* release_dest_image */
    NULL, /* clone_similar */
    NULL, /* composite */
    NULL, /* fill_rectangles */
    NULL, /* composite_trapezoids */
    NULL, /* copy_page */
    NULL, /* show_page */
    NULL, /* set_clip_region */
    _cairo_analysis_surface_intersect_clip_path,
    _cairo_analysis_surface_get_extents,
    NULL, /* old_show_glyphs */
    NULL, /* get_font_options */
    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */
    _cairo_analysis_surface_paint,
    _cairo_analysis_surface_mask,
    _cairo_analysis_surface_stroke,
    _cairo_analysis_surface_fill,
    _cairo_analysis_surface_show_glyphs,
    NULL, /* snapshot */
    NULL, /* is_similar */
    NULL, /* reset */
    NULL, /* fill_stroke */
};

cairo_surface_t *
_cairo_analysis_surface_create (cairo_surface_t		*target,
				int			 width,
				int			 height)
{
    cairo_analysis_surface_t *surface;

    surface = malloc (sizeof (cairo_analysis_surface_t));
    if (surface == NULL)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    /* I believe the content type here is truly arbitrary. I'm quite
     * sure nothing will ever use this value. */
    _cairo_surface_init (&surface->base, &cairo_analysis_surface_backend,
			 CAIRO_CONTENT_COLOR_ALPHA);

    surface->width = width;
    surface->height = height;
    cairo_matrix_init_identity (&surface->ctm);
    surface->has_ctm = FALSE;

    surface->target = target;
    surface->first_op  = TRUE;
    surface->has_supported = FALSE;
    surface->has_unsupported = FALSE;
    _cairo_region_init (&surface->supported_region);
    _cairo_region_init (&surface->fallback_region);

    surface->current_clip.x = 0;
    surface->current_clip.y = 0;
    surface->current_clip.width = width;
    surface->current_clip.height = height;

    return &surface->base;
}

cairo_region_t *
_cairo_analysis_surface_get_supported (cairo_surface_t *abstract_surface)
{
    cairo_analysis_surface_t	*surface = (cairo_analysis_surface_t *) abstract_surface;

    return &surface->supported_region;
}

cairo_region_t *
_cairo_analysis_surface_get_unsupported (cairo_surface_t *abstract_surface)
{
    cairo_analysis_surface_t	*surface = (cairo_analysis_surface_t *) abstract_surface;

    return &surface->fallback_region;
}

cairo_bool_t
_cairo_analysis_surface_has_supported (cairo_surface_t *abstract_surface)
{
    cairo_analysis_surface_t	*surface = (cairo_analysis_surface_t *) abstract_surface;

    return surface->has_supported;
}

cairo_bool_t
_cairo_analysis_surface_has_unsupported (cairo_surface_t *abstract_surface)
{
    cairo_analysis_surface_t	*surface = (cairo_analysis_surface_t *) abstract_surface;

    return surface->has_unsupported;
}

void
_cairo_analysis_surface_get_bounding_box (cairo_surface_t *abstract_surface,
					  cairo_box_t     *bbox)
{
    cairo_analysis_surface_t	*surface = (cairo_analysis_surface_t *) abstract_surface;

    *bbox = surface->page_bbox;
}
