/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
 * Copyright © 2005 Red Hat, Inc.
 * Copyright © 2009 Intel Corporation
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
 *      Chris Wilson <chris@chris-wilson.co.uk>
 */

#include "cairoint.h"

#include "cairo-error-private.h"
#include "cairo-surface-snapshot-private.h"

static cairo_status_t
_cairo_surface_snapshot_finish (void *abstract_surface)
{
    cairo_surface_snapshot_t *surface = abstract_surface;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    if (surface->clone != NULL) {
	cairo_surface_finish (surface->clone);
	status = surface->clone->status;

	cairo_surface_destroy (surface->clone);
    }

    return status;
}

static cairo_status_t
_cairo_surface_snapshot_acquire_source_image (void                    *abstract_surface,
					      cairo_image_surface_t  **image_out,
					      void                   **extra_out)
{
    cairo_surface_snapshot_t *surface = abstract_surface;

    return _cairo_surface_acquire_source_image (surface->target, image_out, extra_out);
}

static void
_cairo_surface_snapshot_release_source_image (void                   *abstract_surface,
					      cairo_image_surface_t  *image,
					      void                   *extra)
{
    cairo_surface_snapshot_t *surface = abstract_surface;

    _cairo_surface_release_source_image (surface->target, image, extra);
}

static cairo_bool_t
_cairo_surface_snapshot_get_extents (void                  *abstract_surface,
				     cairo_rectangle_int_t *extents)
{
    cairo_surface_snapshot_t *surface = abstract_surface;

    return _cairo_surface_get_extents (surface->target, extents);
}

static const cairo_surface_backend_t _cairo_surface_snapshot_backend = {
    CAIRO_INTERNAL_SURFACE_TYPE_SNAPSHOT,

    NULL, /* create similar */
    _cairo_surface_snapshot_finish,

    _cairo_surface_snapshot_acquire_source_image,
    _cairo_surface_snapshot_release_source_image,
    NULL, NULL, /* acquire, release dest */
    NULL, /* clone similar */
    NULL, /* composite */
    NULL, /* fill rectangles */
    NULL, /* composite trapezoids */
    NULL, /* create span renderer */
    NULL, /* check span renderer */
    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_surface_snapshot_get_extents,
};

static void
_cairo_surface_snapshot_copy_on_write (cairo_surface_t *surface)
{
    cairo_surface_snapshot_t *snapshot = (cairo_surface_snapshot_t *) surface;
    cairo_image_surface_t *image;
    cairo_image_surface_t *clone;
    void *extra;
    cairo_status_t status;

    /* We need to make an image copy of the original surface since the
     * snapshot may exceed the lifetime of the original device, i.e.
     * when we later need to use the snapshot the data may have already
     * been lost.
     */

    status = _cairo_surface_acquire_source_image (snapshot->target, &image, &extra);
    if (unlikely (status)) {
	snapshot->target = _cairo_surface_create_in_error (status);
	status = _cairo_surface_set_error (surface, status);
	return;
    }

    clone = (cairo_image_surface_t *)
	_cairo_image_surface_create_with_pixman_format (NULL,
							image->pixman_format,
							image->width,
							image->height,
							0);
    if (likely (clone->base.status == CAIRO_STATUS_SUCCESS)) {
	if (clone->stride == image->stride) {
	    memcpy (clone->data, image->data, image->stride * image->height);
	} else {
	    pixman_image_composite32 (PIXMAN_OP_SRC,
				      image->pixman_image, NULL, clone->pixman_image,
				      0, 0,
				      0, 0,
				      0, 0,
				      image->width, image->height);
	}
	clone->base.is_clear = FALSE;

	snapshot->clone = &clone->base;
    } else {
	snapshot->clone = &clone->base;
	status = _cairo_surface_set_error (surface, clone->base.status);
    }

    _cairo_surface_release_source_image (snapshot->target, image, extra);
    snapshot->target = snapshot->clone;
    snapshot->base.type = snapshot->target->type;
}

/**
 * _cairo_surface_snapshot
 * @surface: a #cairo_surface_t
 *
 * Make an immutable reference to @surface. It is an error to call a
 * surface-modifying function on the result of this function. The
 * resulting 'snapshot' is a lazily copied-on-write surface i.e. it
 * remains a reference to the original surface until that surface is
 * written to again, at which time a copy is made of the original surface
 * and the snapshot then points to that instead. Multiple snapshots of the
 * same unmodified surface point to the same copy.
 *
 * The caller owns the return value and should call
 * cairo_surface_destroy() when finished with it. This function will not
 * return %NULL, but will return a nil surface instead.
 *
 * Return value: The snapshot surface. Note that the return surface
 * may not necessarily be of the same type as @surface.
 **/
cairo_surface_t *
_cairo_surface_snapshot (cairo_surface_t *surface)
{
    cairo_surface_snapshot_t *snapshot;
    cairo_status_t status;

    if (unlikely (surface->status))
	return _cairo_surface_create_in_error (surface->status);

    if (surface->finished)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_SURFACE_FINISHED));

    if (surface->snapshot_of != NULL)
	return cairo_surface_reference (surface);

    if (surface->backend->snapshot != NULL) {
	cairo_surface_t *snap;

	snap = _cairo_surface_has_snapshot (surface, surface->backend);
	if (snap != NULL)
	    return cairo_surface_reference (snap);

	snap = surface->backend->snapshot (surface);
	if (snap != NULL) {
	    if (unlikely (snap->status))
		return snap;

	    status = _cairo_surface_copy_mime_data (snap, surface);
	    if (unlikely (status)) {
		cairo_surface_destroy (snap);
		return _cairo_surface_create_in_error (status);
	    }

	    snap->device_transform = surface->device_transform;
	    snap->device_transform_inverse = surface->device_transform_inverse;

	    cairo_surface_attach_snapshot (surface, snap, NULL);

	    return snap;
	}
    }

    snapshot = (cairo_surface_snapshot_t *)
	_cairo_surface_has_snapshot (surface, &_cairo_surface_snapshot_backend);
    if (snapshot != NULL)
	return cairo_surface_reference (&snapshot->base);

    snapshot = malloc (sizeof (cairo_surface_snapshot_t));
    if (unlikely (snapshot == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_SURFACE_FINISHED));

    _cairo_surface_init (&snapshot->base,
			 &_cairo_surface_snapshot_backend,
			 NULL, /* device */
			 surface->content);
    snapshot->base.type = surface->type;

    snapshot->target = surface;
    snapshot->clone = NULL;

    status = _cairo_surface_copy_mime_data (&snapshot->base, surface);
    if (unlikely (status)) {
	cairo_surface_destroy (&snapshot->base);
	return _cairo_surface_create_in_error (status);
    }

    snapshot->base.device_transform = surface->device_transform;
    snapshot->base.device_transform_inverse = surface->device_transform_inverse;

    cairo_surface_attach_snapshot (surface,
				    &snapshot->base,
				    _cairo_surface_snapshot_copy_on_write);

    return &snapshot->base;
}
