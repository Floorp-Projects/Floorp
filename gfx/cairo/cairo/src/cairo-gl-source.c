/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2011 Intel Corporation
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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *	Chris Wilson <chris@chris-wilson.co.uk>
 */

#include "cairoint.h"

#include "cairo-gl-private.h"

#include "cairo-surface-backend-private.h"

static cairo_status_t
_cairo_gl_source_finish (void *abstract_surface)
{
    cairo_gl_source_t *source = abstract_surface;

    _cairo_gl_operand_destroy (&source->operand);
    return CAIRO_STATUS_SUCCESS;
}

static const cairo_surface_backend_t cairo_gl_source_backend = {
    CAIRO_SURFACE_TYPE_GL,
    _cairo_gl_source_finish,
    NULL, /* read-only wrapper */
};

cairo_surface_t *
_cairo_gl_pattern_to_source (cairo_surface_t *dst,
			     const cairo_pattern_t *pattern,
			     cairo_bool_t is_mask,
			     const cairo_rectangle_int_t *extents,
			     const cairo_rectangle_int_t *sample,
			     int *src_x, int *src_y)
{
    cairo_gl_source_t *source;
    cairo_int_status_t status;

    TRACE ((stderr, "%s\n", __FUNCTION__));
    if (pattern == NULL)
	return _cairo_gl_white_source ();

    source = _cairo_malloc (sizeof (*source));
    if (unlikely (source == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_surface_init (&source->base,
			 &cairo_gl_source_backend,
			 NULL, /* device */
			 CAIRO_CONTENT_COLOR_ALPHA,
			 FALSE); /* is_vector */

    *src_x = *src_y = 0;
    status = _cairo_gl_operand_init (&source->operand, pattern,
				     (cairo_gl_surface_t *)dst,
				     sample, extents,
				     FALSE);
    if (unlikely (status)) {
	cairo_surface_destroy (&source->base);
	return _cairo_surface_create_in_error (status);
    }

    return &source->base;
}

cairo_surface_t *
_cairo_gl_white_source (void)
{
    cairo_gl_source_t *source;

    source = _cairo_malloc (sizeof (*source));
    if (unlikely (source == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_surface_init (&source->base,
			 &cairo_gl_source_backend,
			 NULL, /* device */
			 CAIRO_CONTENT_COLOR_ALPHA,
			 FALSE); /* is_vector */

    _cairo_gl_solid_operand_init (&source->operand, CAIRO_COLOR_WHITE);

    return &source->base;
}
