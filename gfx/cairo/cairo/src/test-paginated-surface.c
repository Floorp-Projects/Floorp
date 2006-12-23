/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2005 Red Hat, Inc
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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *	Carl Worth <cworth@cworth.org>
 */

/* This isn't a "real" surface, but just something to be used by the
 * test suite to help exercise the paginated-surface paths in cairo.
 *
 * The defining feature of this backend is that it uses a paginated
 * surface to record all operations, and then replays everything to an
 * image surface.
 *
 * It's possible that this code might serve as a good starting point
 * for someone working on bringing up a new paginated-surface-based
 * backend.
 */

#include "test-paginated-surface.h"

#include "cairoint.h"

#include "cairo-paginated-surface-private.h"

typedef struct _test_paginated_surface {
    cairo_surface_t base;
    cairo_surface_t *target;
    cairo_paginated_mode_t paginated_mode;
} test_paginated_surface_t;

static const cairo_surface_backend_t test_paginated_surface_backend;
static const cairo_paginated_surface_backend_t test_paginated_surface_paginated_backend;

cairo_surface_t *
_test_paginated_surface_create_for_data (unsigned char		*data,
					 cairo_content_t	 content,
					 int		 	 width,
					 int		 	 height,
					 int		 	 stride)
{
    cairo_status_t status;
    cairo_surface_t *target;
    test_paginated_surface_t *surface;

    target =  _cairo_image_surface_create_for_data_with_content (data, content,
								width, height,
								stride);
    status = cairo_surface_status (target);
    if (status) {
	_cairo_error (status);
	return (cairo_surface_t *) &_cairo_surface_nil;
    }

    surface = malloc (sizeof (test_paginated_surface_t));
    if (surface == NULL) {
	_cairo_error (CAIRO_STATUS_NO_MEMORY);
	return (cairo_surface_t *) &_cairo_surface_nil;
    }

    _cairo_surface_init (&surface->base, &test_paginated_surface_backend,
			 content);

    surface->target = target;

    return _cairo_paginated_surface_create (&surface->base, content, width, height,
					    &test_paginated_surface_paginated_backend);
}

static cairo_status_t
_test_paginated_surface_finish (void *abstract_surface)
{
    test_paginated_surface_t *surface = abstract_surface;

    cairo_surface_destroy (surface->target);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_test_paginated_surface_set_clip_region (void *abstract_surface,
					 pixman_region16_t *region)
{
    test_paginated_surface_t *surface = abstract_surface;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return CAIRO_STATUS_SUCCESS;

    /* XXX: The whole surface backend clipping interface is a giant
     * disaster right now. In particular, its uncleanness shows up
     * when trying to implement one surface that wraps another one (as
     * we are doing here).
     *
     * Here are two of the problems that show up:
     *
     * 1. The most critical piece of information in all this stuff,
     * the "clip" isn't getting passed to the backend
     * functions. Instead the generic surface layer is caching that as
     * surface->clip. This is a problem for surfaces like this one
     * that do wrapping. Our base surface will have the clip set, but
     * our target's surface will not.
     *
     * 2. We're here in our backend's set_clip_region function, and we
     * want to call into our target surface's set_clip_region.
     * Generally, we would do this by calling an equivalent
     * _cairo_surface function, but _cairo_surface_set_clip_region
     * does not have the same signature/semantics, (it has the
     * clip_serial stuff as well).
     *
     * We kludge around each of these by manually copying the clip
     * object from our base surface into the target's base surface
     * (yuck!) and by reaching directly into the image surface's
     * set_clip_region instead of calling into the generic
     * _cairo_surface_set_clip_region (double yuck!).
     */

    surface->target->clip = surface->base.clip;

    return _cairo_image_surface_set_clip_region (surface->target, region);
}

static cairo_int_status_t
_test_paginated_surface_get_extents (void			*abstract_surface,
				     cairo_rectangle_int16_t	*rectangle)
{
    test_paginated_surface_t *surface = abstract_surface;

    return _cairo_surface_get_extents (surface->target, rectangle);
}

static cairo_int_status_t
_test_paginated_surface_paint (void		*abstract_surface,
			       cairo_operator_t	 op,
			       cairo_pattern_t	*source)
{
    test_paginated_surface_t *surface = abstract_surface;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return CAIRO_STATUS_SUCCESS;

    return _cairo_surface_paint (surface->target, op, source);
}

static cairo_int_status_t
_test_paginated_surface_mask (void		*abstract_surface,
			      cairo_operator_t	 op,
			      cairo_pattern_t	*source,
			      cairo_pattern_t	*mask)
{
    test_paginated_surface_t *surface = abstract_surface;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return CAIRO_STATUS_SUCCESS;

    return _cairo_surface_mask (surface->target, op, source, mask);
}

static cairo_int_status_t
_test_paginated_surface_stroke (void			*abstract_surface,
				cairo_operator_t	 op,
				cairo_pattern_t		*source,
				cairo_path_fixed_t	*path,
				cairo_stroke_style_t	*style,
				cairo_matrix_t		*ctm,
				cairo_matrix_t		*ctm_inverse,
				double			 tolerance,
				cairo_antialias_t	 antialias)
{
    test_paginated_surface_t *surface = abstract_surface;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return CAIRO_STATUS_SUCCESS;

    return _cairo_surface_stroke (surface->target, op, source,
				  path, style,
				  ctm, ctm_inverse,
				  tolerance, antialias);
}

static cairo_int_status_t
_test_paginated_surface_fill (void			*abstract_surface,
			      cairo_operator_t		 op,
			      cairo_pattern_t		*source,
			      cairo_path_fixed_t	*path,
			      cairo_fill_rule_t		 fill_rule,
			      double			 tolerance,
			      cairo_antialias_t		 antialias)
{
    test_paginated_surface_t *surface = abstract_surface;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return CAIRO_STATUS_SUCCESS;

    return _cairo_surface_fill (surface->target, op, source,
				path, fill_rule,
				tolerance, antialias);
}

static cairo_int_status_t
_test_paginated_surface_show_glyphs (void			*abstract_surface,
				     cairo_operator_t		 op,
				     cairo_pattern_t		*source,
				     cairo_glyph_t		*glyphs,
				     int			 num_glyphs,
				     cairo_scaled_font_t	*scaled_font)
{
    test_paginated_surface_t *surface = abstract_surface;

    if (surface->paginated_mode == CAIRO_PAGINATED_MODE_ANALYZE)
	return CAIRO_STATUS_SUCCESS;

    return _cairo_surface_show_glyphs (surface->target, op, source,
				       glyphs, num_glyphs, scaled_font);
}

static void
_test_paginated_surface_set_paginated_mode (void			*abstract_surface,
					    cairo_paginated_mode_t	 mode)
{
    test_paginated_surface_t *surface = abstract_surface;

    surface->paginated_mode = mode;
}

static const cairo_surface_backend_t test_paginated_surface_backend = {
    CAIRO_INTERNAL_SURFACE_TYPE_TEST_PAGINATED,

    /* Since we are a paginated user, we get to regard most of the
     * surface backend interface as historical cruft and ignore it. */

    NULL, /* create_similar */
    _test_paginated_surface_finish,
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
    _test_paginated_surface_set_clip_region,
    NULL, /* intersect_clip_path */
    _test_paginated_surface_get_extents,
    NULL, /* old_show_glyphs */
    NULL, /* get_font_options */
    NULL, /* flush */
    NULL, /* mark_dirty_rectangle */
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */

    /* Here is the more "modern" section of the surface backend
     * interface which is mostly just drawing functions */

    _test_paginated_surface_paint,
    _test_paginated_surface_mask,
    _test_paginated_surface_stroke,
    _test_paginated_surface_fill,
    _test_paginated_surface_show_glyphs,
    NULL /* snapshot */
};

static const cairo_paginated_surface_backend_t test_paginated_surface_paginated_backend = {
    NULL, /* start_page */
    _test_paginated_surface_set_paginated_mode
};
