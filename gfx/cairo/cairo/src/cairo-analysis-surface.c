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

cairo_int_status_t
_cairo_analysis_surface_merge_status (cairo_int_status_t status_a,
				      cairo_int_status_t status_b)
{
    /* fatal errors should be checked and propagated at source */
    assert (! _cairo_status_is_error (status_a));
    assert (! _cairo_status_is_error (status_b));

    /* return the most important status */
    if (status_a == CAIRO_INT_STATUS_UNSUPPORTED ||
	status_b == CAIRO_INT_STATUS_UNSUPPORTED)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (status_a == CAIRO_INT_STATUS_IMAGE_FALLBACK ||
	status_b == CAIRO_INT_STATUS_IMAGE_FALLBACK)
	return CAIRO_INT_STATUS_IMAGE_FALLBACK;

    if (status_a == CAIRO_INT_STATUS_ANALYZE_META_SURFACE_PATTERN ||
	status_b == CAIRO_INT_STATUS_ANALYZE_META_SURFACE_PATTERN)
	return CAIRO_INT_STATUS_ANALYZE_META_SURFACE_PATTERN;

    if (status_a == CAIRO_INT_STATUS_FLATTEN_TRANSPARENCY ||
	status_b == CAIRO_INT_STATUS_FLATTEN_TRANSPARENCY)
	return CAIRO_INT_STATUS_FLATTEN_TRANSPARENCY;

    /* at this point we have checked all the valid internal codes, so... */
    assert (status_a == CAIRO_STATUS_SUCCESS &&
	    status_b == CAIRO_STATUS_SUCCESS);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_analyze_meta_surface_pattern (cairo_analysis_surface_t	*surface,
			       cairo_pattern_t		*pattern)
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
    if (status && status != CAIRO_INT_STATUS_UNSUPPORTED)
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
_add_operation  (cairo_analysis_surface_t *surface,
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

    _cairo_box_from_rectangle (&bbox, rect);

    if (surface->has_ctm) {

	_cairo_matrix_transform_bounding_box_fixed (&surface->ctm, &bbox, NULL);

	if (bbox.p1.x == bbox.p2.x || bbox.p1.y == bbox.p2.y) {
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

	_cairo_box_round_to_rectangle (&bbox, rect);
    }

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

    cairo_surface_destroy (surface->target);

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
	backend_status = _analyze_meta_surface_pattern (surface, source);

    status = _cairo_surface_get_extents (&surface->base, &extents);
    if (status && status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;

    if (_cairo_operator_bounded_by_source (op)) {
	cairo_rectangle_int_t source_extents;
	status = _cairo_pattern_get_extents (source, &source_extents);
	if (status)
	    return status;

	_cairo_rectangle_intersect (&extents, &source_extents);
    }

    _cairo_rectangle_intersect (&extents, &surface->current_clip);

    status = _add_operation (surface, &extents, backend_status);

    return status;
}

static cairo_int_status_t
_cairo_analysis_surface_mask (void		*abstract_surface,
			      cairo_operator_t	 op,
			      cairo_pattern_t	*source,
			      cairo_pattern_t	*mask)
{
    cairo_analysis_surface_t *surface = abstract_surface;
    cairo_int_status_t	      status, backend_status;
    cairo_rectangle_int_t   extents;

    if (!surface->target->backend->mask)
	backend_status = CAIRO_INT_STATUS_UNSUPPORTED;
    else
	backend_status = (*surface->target->backend->mask) (surface->target, op,
                                                            source, mask);

    if (backend_status == CAIRO_INT_STATUS_ANALYZE_META_SURFACE_PATTERN) {
	cairo_int_status_t backend_source_status = CAIRO_STATUS_SUCCESS;
	cairo_int_status_t backend_mask_status = CAIRO_STATUS_SUCCESS;

	if (source->type == CAIRO_PATTERN_TYPE_SURFACE) {
	    cairo_surface_pattern_t *surface_pattern = (cairo_surface_pattern_t *) source;
	    if (_cairo_surface_is_meta (surface_pattern->surface)) {
		backend_source_status =
		    _analyze_meta_surface_pattern (surface, source);
		if (_cairo_status_is_error (backend_source_status))
		    return backend_source_status;
	    }
	}

	if (mask->type == CAIRO_PATTERN_TYPE_SURFACE) {
	    cairo_surface_pattern_t *surface_pattern = (cairo_surface_pattern_t *) mask;
	    if (_cairo_surface_is_meta (surface_pattern->surface)) {
		backend_mask_status =
		    _analyze_meta_surface_pattern (surface, mask);
		if (_cairo_status_is_error (backend_mask_status))
		    return backend_mask_status;
	    }
	}

	backend_status =
	    _cairo_analysis_surface_merge_status (backend_source_status,
						  backend_mask_status);
    }

    status = _cairo_surface_get_extents (&surface->base, &extents);
    if (status && status != CAIRO_INT_STATUS_UNSUPPORTED)
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

    status = _add_operation (surface, &extents, backend_status);

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
    cairo_rectangle_int_t  extents;

    if (!surface->target->backend->stroke)
	backend_status = CAIRO_INT_STATUS_UNSUPPORTED;
    else
	backend_status = (*surface->target->backend->stroke) (surface->target, op,
							      source, path, style,
							      ctm, ctm_inverse,
							      tolerance, antialias);

    if (backend_status == CAIRO_INT_STATUS_ANALYZE_META_SURFACE_PATTERN)
	backend_status = _analyze_meta_surface_pattern (surface, source);

    status = _cairo_surface_get_extents (&surface->base, &extents);
    if (status && status != CAIRO_INT_STATUS_UNSUPPORTED)
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
	cairo_box_t box;

	_cairo_box_from_rectangle (&box, &extents);

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

	_cairo_traps_extents (&traps, &box);
	_cairo_traps_fini (&traps);

        _cairo_box_round_to_rectangle (&box, &extents);
    }

    status = _add_operation (surface, &extents, backend_status);

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
    cairo_rectangle_int_t  extents;

    if (!surface->target->backend->fill)
	backend_status = CAIRO_INT_STATUS_UNSUPPORTED;
    else
	backend_status = (*surface->target->backend->fill) (surface->target, op,
						    source, path, fill_rule,
						    tolerance, antialias);

    if (backend_status == CAIRO_INT_STATUS_ANALYZE_META_SURFACE_PATTERN)
	backend_status = _analyze_meta_surface_pattern (surface, source);

    status = _cairo_surface_get_extents (&surface->base, &extents);
    if (status && status != CAIRO_INT_STATUS_UNSUPPORTED)
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
	cairo_box_t box;

	_cairo_box_from_rectangle (&box, &extents);

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

	_cairo_traps_extents (&traps, &box);
	_cairo_traps_fini (&traps);

        _cairo_box_round_to_rectangle (&box, &extents);
    }

    status = _add_operation (surface, &extents, backend_status);

    return status;
}

static cairo_int_status_t
_cairo_analysis_surface_show_glyphs (void		  *abstract_surface,
				     cairo_operator_t	   op,
				     cairo_pattern_t	  *source,
				     cairo_glyph_t	  *glyphs,
				     int		   num_glyphs,
				     cairo_scaled_font_t  *scaled_font,
				     int                  *remaining_glyphs)
{
    cairo_analysis_surface_t *surface = abstract_surface;
    cairo_status_t	     status, backend_status;
    cairo_rectangle_int_t    extents, glyph_extents;

    /* Adapted from _cairo_surface_show_glyphs */
    if (surface->target->backend->show_glyphs)
	backend_status = (*surface->target->backend->show_glyphs) (surface->target, op,
								   source,
								   glyphs, num_glyphs,
								   scaled_font,
								   remaining_glyphs);
    else if (surface->target->backend->show_text_glyphs)
	backend_status = surface->target->backend->show_text_glyphs (surface->target, op,
								     source,
								     NULL, 0,
								     glyphs, num_glyphs,
								     NULL, 0,
								     FALSE,
								     scaled_font);
    else
	backend_status = CAIRO_INT_STATUS_UNSUPPORTED;

    if (backend_status == CAIRO_INT_STATUS_ANALYZE_META_SURFACE_PATTERN)
	backend_status = _analyze_meta_surface_pattern (surface, source);

    status = _cairo_surface_get_extents (&surface->base, &extents);
    if (status && status != CAIRO_INT_STATUS_UNSUPPORTED)
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

    status = _add_operation (surface, &extents, backend_status);

    return status;
}

static cairo_bool_t
_cairo_analysis_surface_has_show_text_glyphs (void *abstract_surface)
{
    cairo_analysis_surface_t *surface = abstract_surface;

    return cairo_surface_has_show_text_glyphs (surface->target);
}

static cairo_int_status_t
_cairo_analysis_surface_show_text_glyphs (void			    *abstract_surface,
					  cairo_operator_t	     op,
					  cairo_pattern_t	    *source,
					  const char		    *utf8,
					  int			     utf8_len,
					  cairo_glyph_t		    *glyphs,
					  int			     num_glyphs,
					  const cairo_text_cluster_t *clusters,
					  int			     num_clusters,
					  cairo_text_cluster_flags_t cluster_flags,
					  cairo_scaled_font_t	    *scaled_font)
{
    cairo_analysis_surface_t *surface = abstract_surface;
    cairo_status_t	     status, backend_status;
    cairo_rectangle_int_t    extents, glyph_extents;

    /* Adapted from _cairo_surface_show_glyphs */
    backend_status = CAIRO_INT_STATUS_UNSUPPORTED;
    if (surface->target->backend->show_text_glyphs)
	backend_status = surface->target->backend->show_text_glyphs (surface->target, op,
								     source,
								     utf8, utf8_len,
								     glyphs, num_glyphs,
								     clusters, num_clusters, cluster_flags,
								     scaled_font);
    if (backend_status == CAIRO_INT_STATUS_UNSUPPORTED && surface->target->backend->show_glyphs) {
	int remaining_glyphs = num_glyphs;
	backend_status = surface->target->backend->show_glyphs (surface->target, op,
								source,
								glyphs, num_glyphs,
								scaled_font,
								&remaining_glyphs);
	glyphs += num_glyphs - remaining_glyphs;
	num_glyphs = remaining_glyphs;
	if (remaining_glyphs == 0)
	    backend_status = CAIRO_STATUS_SUCCESS;
    }

    if (backend_status == CAIRO_INT_STATUS_ANALYZE_META_SURFACE_PATTERN)
	backend_status = _analyze_meta_surface_pattern (surface, source);

    status = _cairo_surface_get_extents (&surface->base, &extents);
    if (status && status != CAIRO_INT_STATUS_UNSUPPORTED)
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

    status = _add_operation (surface, &extents, backend_status);

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
    NULL, /* create_solid_pattern_surface */
    _cairo_analysis_surface_has_show_text_glyphs,
    _cairo_analysis_surface_show_text_glyphs
};

cairo_surface_t *
_cairo_analysis_surface_create (cairo_surface_t		*target,
				int			 width,
				int			 height)
{
    cairo_analysis_surface_t *surface;
    cairo_status_t status;

    status = target->status;
    if (status)
	return _cairo_surface_create_in_error (status);

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

    surface->target = cairo_surface_reference (target);
    surface->first_op  = TRUE;
    surface->has_supported = FALSE;
    surface->has_unsupported = FALSE;

    surface->page_bbox.p1.x = 0;
    surface->page_bbox.p1.y = 0;
    surface->page_bbox.p2.x = 0;
    surface->page_bbox.p2.y = 0;

    _cairo_region_init (&surface->supported_region);
    _cairo_region_init (&surface->fallback_region);

    if (width == -1 && height == -1) {
	surface->current_clip.x      = CAIRO_RECT_INT_MIN;
	surface->current_clip.y      = CAIRO_RECT_INT_MIN;
	surface->current_clip.width  = CAIRO_RECT_INT_MAX - CAIRO_RECT_INT_MIN;
	surface->current_clip.height = CAIRO_RECT_INT_MAX - CAIRO_RECT_INT_MIN;
    } else {
	surface->current_clip.x = 0;
	surface->current_clip.y = 0;
	surface->current_clip.width = width;
	surface->current_clip.height = height;
    }

    return &surface->base;
}

void
_cairo_analysis_surface_set_ctm (cairo_surface_t *abstract_surface,
				 cairo_matrix_t  *ctm)
{
    cairo_analysis_surface_t	*surface;

    if (abstract_surface->status)
	return;

    surface = (cairo_analysis_surface_t *) abstract_surface;

    surface->ctm = *ctm;
    surface->has_ctm = !_cairo_matrix_is_identity (&surface->ctm);
}

void
_cairo_analysis_surface_get_ctm (cairo_surface_t *abstract_surface,
				 cairo_matrix_t  *ctm)
{
    cairo_analysis_surface_t	*surface = (cairo_analysis_surface_t *) abstract_surface;

    *ctm = surface->ctm;
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

/* null surface type: a surface that does nothing (has no side effects, yay!) */

static cairo_int_status_t
_return_success (void)
{
    return CAIRO_STATUS_SUCCESS;
}

/* These typedefs are just to silence the compiler... */
typedef cairo_int_status_t
(*_set_clip_region_func)	(void			*surface,
				 cairo_region_t		*region);
typedef cairo_int_status_t
(*_paint_func)			(void			*surface,
			         cairo_operator_t	 op,
				 cairo_pattern_t	*source);

typedef cairo_int_status_t
(*_mask_func)			(void			*surface,
			         cairo_operator_t	 op,
				 cairo_pattern_t	*source,
				 cairo_pattern_t	*mask);

typedef cairo_int_status_t
(*_stroke_func)			(void			*surface,
			         cairo_operator_t	 op,
				 cairo_pattern_t	*source,
				 cairo_path_fixed_t	*path,
				 cairo_stroke_style_t	*style,
				 cairo_matrix_t		*ctm,
				 cairo_matrix_t		*ctm_inverse,
				 double			 tolerance,
				 cairo_antialias_t	 antialias);

typedef cairo_int_status_t
(*_fill_func)			(void			*surface,
			         cairo_operator_t	 op,
				 cairo_pattern_t	*source,
				 cairo_path_fixed_t	*path,
				 cairo_fill_rule_t	 fill_rule,
				 double			 tolerance,
				 cairo_antialias_t	 antialias);

typedef cairo_int_status_t
(*_show_glyphs_func)		(void			*surface,
			         cairo_operator_t	 op,
				 cairo_pattern_t	*source,
				 cairo_glyph_t		*glyphs,
				 int			 num_glyphs,
				 cairo_scaled_font_t	*scaled_font,
				 int			*remaining_glyphs);

static const cairo_surface_backend_t cairo_null_surface_backend = {
    CAIRO_INTERNAL_SURFACE_TYPE_NULL,

    NULL, /* create_similar */
    NULL, /* finish */
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
    (_set_clip_region_func) _return_success, /* set_clip_region */
    NULL, /* intersect_clip_path */
    NULL, /* get_extents */
    NULL, /* old_show_glyphs */
    NULL, /* get_font_options */
    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */
    (_paint_func) _return_success,	    /* paint */
    (_mask_func) _return_success,	    /* mask */
    (_stroke_func) _return_success,	    /* stroke */
    (_fill_func) _return_success,	    /* fill */
    (_show_glyphs_func) _return_success,    /* show_glyphs */
    NULL, /* snapshot */
    NULL, /* is_similar */
    NULL, /* reset */
    NULL, /* fill_stroke */
    NULL, /* create_solid_pattern_surface */
    NULL, /* has_show_text_glyphs */
    NULL  /* show_text_glyphs */
};

cairo_surface_t *
_cairo_null_surface_create (cairo_content_t content)
{
    cairo_surface_t *surface;

    surface = malloc (sizeof (cairo_surface_t));
    if (surface == NULL) {
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
    }

    _cairo_surface_init (surface, &cairo_null_surface_backend, content);

    return surface;
}
