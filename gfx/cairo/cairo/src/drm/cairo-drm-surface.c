/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 Chris Wilson
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
 * The Initial Developer of the Original Code is Chris Wilson.
 */

#include "cairoint.h"

#include "cairo-drm-private.h"

#include "cairo-error-private.h"
#include "cairo-image-surface-inline.h"

void
_cairo_drm_surface_init (cairo_drm_surface_t *surface,
			 cairo_format_t format,
			 int width, int height)
{
    surface->bo = NULL;
    surface->format = format;
    surface->width  = width;
    surface->height = height;
    surface->stride = 0;

    surface->fallback = NULL;
    surface->map_count = 0;
}

cairo_status_t
_cairo_drm_surface_finish (cairo_drm_surface_t *surface)
{
    assert (surface->fallback == NULL);

    if (surface->bo != NULL)
	cairo_drm_bo_destroy (surface->base.device, surface->bo);

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_drm_surface_get_font_options (void                  *abstract_surface,
				     cairo_font_options_t  *options)
{
    _cairo_font_options_init_default (options);

    cairo_font_options_set_hint_metrics (options, CAIRO_HINT_METRICS_ON);
}

cairo_bool_t
_cairo_drm_surface_get_extents (void *abstract_surface,
			        cairo_rectangle_int_t *rectangle)
{
    cairo_drm_surface_t *surface = abstract_surface;

    rectangle->x = 0;
    rectangle->y = 0;
    rectangle->width  = surface->width;
    rectangle->height = surface->height;

    return TRUE;
}

cairo_surface_t *
cairo_drm_surface_create (cairo_device_t *abstract_device,
			  cairo_format_t format,
			  int width, int height)
{
    cairo_drm_device_t *device = (cairo_drm_device_t *) abstract_device;
    cairo_surface_t *surface;

    if (device != NULL && device->base.status)
    {
	surface = _cairo_surface_create_in_error (device->base.status);
    }
    else if (device == NULL ||
	     device->surface.create == NULL ||
	     width == 0 || width > device->max_surface_size ||
	     height == 0 || height > device->max_surface_size)
    {
	surface = cairo_image_surface_create (format, width, height);
    }
    else if (device->base.finished)
    {
	surface = _cairo_surface_create_in_error (CAIRO_STATUS_SURFACE_FINISHED);
    }
    else
    {
	surface = device->surface.create (device, format, width, height);
	if (surface->status == CAIRO_STATUS_INVALID_SIZE)
	    surface = cairo_image_surface_create (format, width, height);
    }

    return surface;
}

cairo_surface_t *
cairo_drm_surface_create_for_name (cairo_device_t *abstract_device,
				   unsigned int name,
	                           cairo_format_t format,
				   int width, int height, int stride)
{
    cairo_drm_device_t *device = (cairo_drm_device_t *) abstract_device;
    cairo_surface_t *surface;

    if (! CAIRO_FORMAT_VALID (format))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));

    if (device != NULL && device->base.status)
    {
	surface = _cairo_surface_create_in_error (device->base.status);
    }
    else if (device == NULL || device->surface.create_for_name == NULL)
    {
	/* XXX invalid device! */
	surface = _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));
    }
    else if (width == 0 || width > device->max_surface_size ||
	     height == 0 || height > device->max_surface_size)
    {
	surface = _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_SIZE));
    }
    else if (device->base.finished)
    {
	surface = _cairo_surface_create_in_error (CAIRO_STATUS_SURFACE_FINISHED);
    }
    else
    {
	surface = device->surface.create_for_name (device,
	                                             name, format,
						     width, height, stride);
    }

    return surface;
}
slim_hidden_def (cairo_drm_surface_create_for_name);

cairo_surface_t *
cairo_drm_surface_create_from_cacheable_image (cairo_device_t *abstract_device,
	                                       cairo_surface_t *surface)
{
    cairo_drm_device_t *device = (cairo_drm_device_t *) abstract_device;

    if (surface->status) {
	surface = _cairo_surface_create_in_error (surface->status);
    } else if (device != NULL && device->base.status) {
	surface = _cairo_surface_create_in_error (device->base.status);
    } else if (device == NULL || device->surface.create_from_cacheable_image == NULL) {
	/* XXX invalid device! */
	surface = _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));
    } else if (device->base.finished) {
	surface = _cairo_surface_create_in_error (CAIRO_STATUS_SURFACE_FINISHED);
    } else {
	surface = device->surface.create_from_cacheable_image (device, surface);
    }

    return surface;
}

static cairo_drm_surface_t *
_cairo_surface_as_drm (cairo_surface_t *abstract_surface)
{
    if (unlikely (abstract_surface->status))
	return NULL;

    if (abstract_surface->type != CAIRO_SURFACE_TYPE_DRM)
	return NULL;

    return (cairo_drm_surface_t *) abstract_surface;
}

cairo_status_t
cairo_drm_surface_enable_scan_out (cairo_surface_t *abstract_surface)
{
    cairo_drm_surface_t *surface;
    cairo_drm_device_t *device;

    surface = _cairo_surface_as_drm (abstract_surface);
    if (unlikely (surface == NULL))
	return _cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
    if (unlikely (surface->base.finished))
	return _cairo_error (CAIRO_STATUS_SURFACE_FINISHED);

    device = (cairo_drm_device_t *) surface->base.device;
    if (device->surface.enable_scan_out == NULL)
	return CAIRO_STATUS_SUCCESS;

    if (unlikely (device->base.finished))
	return _cairo_error (CAIRO_STATUS_SURFACE_FINISHED);

    return device->surface.enable_scan_out (abstract_surface);
}

unsigned int
cairo_drm_surface_get_handle (cairo_surface_t *abstract_surface)
{
    cairo_drm_surface_t *surface;

    surface = _cairo_surface_as_drm (abstract_surface);
    if (surface == NULL) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return 0;
    }

    return surface->bo->handle;
}

cairo_int_status_t
_cairo_drm_surface_flink (void *abstract_surface)
{
    cairo_drm_surface_t *surface = abstract_surface;

    return _cairo_drm_bo_flink ((cairo_drm_device_t *) surface->base.device,
				surface->bo);
}

unsigned int
cairo_drm_surface_get_name (cairo_surface_t *abstract_surface)
{
    cairo_drm_surface_t *surface;
    cairo_drm_device_t *device;
    cairo_status_t status;

    surface = _cairo_surface_as_drm (abstract_surface);
    if (surface == NULL) {
	_cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return 0;
    }

    if (surface->bo->name)
	return surface->bo->name;

    device = (cairo_drm_device_t *) surface->base.device;
    if (device->surface.flink == NULL)
	return 0;

    status = device->surface.flink (abstract_surface);
    if (status) {
	if (_cairo_status_is_error (status))
	    status = _cairo_surface_set_error (abstract_surface, status);

	return 0;
    }

    return surface->bo->name;
}

cairo_format_t
cairo_drm_surface_get_format (cairo_surface_t *abstract_surface)
{
    cairo_drm_surface_t *surface;

    surface = _cairo_surface_as_drm (abstract_surface);
    if (surface == NULL)
	return cairo_image_surface_get_format (abstract_surface);

    return surface->format;
}

int
cairo_drm_surface_get_width (cairo_surface_t *abstract_surface)
{
    cairo_drm_surface_t *surface;

    surface = _cairo_surface_as_drm (abstract_surface);
    if (surface == NULL)
	return cairo_image_surface_get_width (abstract_surface);

    return surface->width;
}

int
cairo_drm_surface_get_height (cairo_surface_t *abstract_surface)
{
    cairo_drm_surface_t *surface;

    surface = _cairo_surface_as_drm (abstract_surface);
    if (surface == NULL)
	return cairo_image_surface_get_height (abstract_surface);

    return surface->height;
}

int
cairo_drm_surface_get_stride (cairo_surface_t *abstract_surface)
{
    cairo_drm_surface_t *surface;

    surface = _cairo_surface_as_drm (abstract_surface);
    if (surface == NULL)
	return cairo_image_surface_get_stride (abstract_surface);

    return surface->stride;
}

/* XXX drm or general surface layer? naming? */
cairo_surface_t *
cairo_drm_surface_map_to_image (cairo_surface_t *abstract_surface)
{
    cairo_drm_surface_t *surface;
    cairo_drm_device_t *device;
    cairo_status_t status;

    if (unlikely (abstract_surface->status))
	return _cairo_surface_create_in_error (abstract_surface->status);

    surface = _cairo_surface_as_drm (abstract_surface);
    if (surface == NULL) {
	if (_cairo_surface_is_image (abstract_surface))
	    return cairo_surface_reference (abstract_surface);

	status = _cairo_surface_set_error (abstract_surface,
					   CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return _cairo_surface_create_in_error (status);
    }

    surface->map_count++;
    device = (cairo_drm_device_t *) surface->base.device;
    return cairo_surface_reference (device->surface.map_to_image (surface));
}

void
cairo_drm_surface_unmap (cairo_surface_t *abstract_surface,
	                 cairo_surface_t *image)
{
    cairo_drm_surface_t *surface;

    surface = _cairo_surface_as_drm (abstract_surface);
    if (surface == NULL) {
	if (_cairo_surface_is_image (abstract_surface))
	    cairo_surface_destroy (image);
	else
	    _cairo_error_throw (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
	return;
    }

    /* XXX assert image belongs to drm */
    //assert (image == drm->fallback);
    cairo_surface_destroy (image);

    assert (surface->map_count > 0);
    if (--surface->map_count == 0)
	cairo_surface_flush (&surface->base);
}
