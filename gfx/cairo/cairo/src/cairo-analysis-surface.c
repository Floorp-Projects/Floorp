/*
 * Copyright © 2006 Keith Packard
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
 */

#include "cairoint.h"

#include "cairo-analysis-surface-private.h"
#include "cairo-paginated-surface-private.h"

typedef struct {
    cairo_surface_t base;
    int width;
    int height;

    cairo_surface_t	*target;
    
    cairo_bool_t fallback;
} cairo_analysis_surface_t;

static cairo_int_status_t
_cairo_analysis_surface_get_extents (void	 		*abstract_surface,
				     cairo_rectangle_fixed_t	*rectangle)
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
    cairo_status_t	     status;

    if (!surface->target->backend->paint)
	status = CAIRO_INT_STATUS_UNSUPPORTED;
    else
	status = (*surface->target->backend->paint) (surface->target, op,
						     source);
    if (status == CAIRO_INT_STATUS_UNSUPPORTED) {
	surface->fallback = TRUE;
	status = CAIRO_STATUS_SUCCESS;
    }
    return status;
}

static cairo_int_status_t
_cairo_analysis_surface_mask (void		*abstract_surface,
			      cairo_operator_t	 op,
			      cairo_pattern_t	*source,
			      cairo_pattern_t	*mask)
{
    cairo_analysis_surface_t *surface = abstract_surface;
    cairo_status_t	     status;

    if (!surface->target->backend->mask)
	status = CAIRO_INT_STATUS_UNSUPPORTED;
    else
	status = (*surface->target->backend->mask) (surface->target, op,
						    source, mask);
    if (status == CAIRO_INT_STATUS_UNSUPPORTED) {
	surface->fallback = TRUE;
	status = CAIRO_STATUS_SUCCESS;
    }
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
    cairo_status_t	     status;

    if (!surface->target->backend->stroke)
	status = CAIRO_INT_STATUS_UNSUPPORTED;
    else
	status = (*surface->target->backend->stroke) (surface->target, op,
						      source, path, style,
						      ctm, ctm_inverse,
						      tolerance, antialias);
    if (status == CAIRO_INT_STATUS_UNSUPPORTED) {
	surface->fallback = TRUE;
	status = CAIRO_STATUS_SUCCESS;
    }
    return status;
}

static cairo_int_status_t
_cairo_analysis_surface_fill (void			*abstract_surface,
			      cairo_operator_t		 op,
			      cairo_pattern_t		*source,
			      cairo_path_fixed_t	*path,
			      cairo_fill_rule_t	 	 fill_rule,
			      double			 tolerance,
			      cairo_antialias_t	 	 antialias)
{
    cairo_analysis_surface_t *surface = abstract_surface;
    cairo_status_t	     status;

    if (!surface->target->backend->fill)
	status = CAIRO_INT_STATUS_UNSUPPORTED;
    else
	status = (*surface->target->backend->fill) (surface->target, op,
						    source, path, fill_rule,
						    tolerance, antialias);
    if (status == CAIRO_INT_STATUS_UNSUPPORTED) {
	surface->fallback = TRUE;
	status = CAIRO_STATUS_SUCCESS;
    }
    return status;
}

static cairo_int_status_t
_cairo_analysis_surface_show_glyphs (void		  *abstract_surface,
				     cairo_operator_t	   op,
				     cairo_pattern_t	  *source,
				     const cairo_glyph_t  *glyphs,
				     int		   num_glyphs,
				     cairo_scaled_font_t  *scaled_font)
{
    cairo_analysis_surface_t *surface = abstract_surface;
    cairo_status_t	     status;

    if (!surface->target->backend->show_glyphs)
	status = CAIRO_INT_STATUS_UNSUPPORTED;
    else
	status = (*surface->target->backend->show_glyphs) (surface->target, op,
							   source,
							   glyphs, num_glyphs,
							   scaled_font);
    if (status == CAIRO_INT_STATUS_UNSUPPORTED) {
	surface->fallback = TRUE;
	status = CAIRO_STATUS_SUCCESS;
    }
    return status;
}

static const cairo_surface_backend_t cairo_analysis_surface_backend = {
    CAIRO_INTERNAL_SURFACE_TYPE_ANALYSIS,
    NULL, /* create_similar */
    NULL, /* finish_surface */
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
    NULL, /* clip_path */
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
};

cairo_private cairo_surface_t *
_cairo_analysis_surface_create (cairo_surface_t		*target,
				int			 width,
				int			 height)
{
    cairo_analysis_surface_t *surface;

    surface = malloc (sizeof (cairo_analysis_surface_t));
    if (surface == NULL)
	goto FAIL;

    /* I believe the content type here is truly arbitrary. I'm quite
     * sure nothing will ever use this value. */
    _cairo_surface_init (&surface->base, &cairo_analysis_surface_backend,
			 CAIRO_CONTENT_COLOR_ALPHA);

    surface->width = width;
    surface->height = height;

    surface->target = target;
    surface->fallback = FALSE;

    return &surface->base;
FAIL:
    _cairo_error (CAIRO_STATUS_NO_MEMORY);
    return NULL;
}

cairo_private pixman_region16_t *
_cairo_analysis_surface_get_supported (cairo_surface_t *abstract_surface)
{
    /* XXX */
    return NULL;
}

cairo_private pixman_region16_t *
_cairo_analysis_surface_get_unsupported (cairo_surface_t *abstract_surface)
{
    /* XXX */
    return NULL;
}

cairo_private cairo_bool_t
_cairo_analysis_surface_has_unsupported (cairo_surface_t *abstract_surface)
{
    cairo_analysis_surface_t	*surface = (cairo_analysis_surface_t *) abstract_surface;

    return surface->fallback;
}


