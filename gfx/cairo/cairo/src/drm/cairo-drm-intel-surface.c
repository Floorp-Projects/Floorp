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
 */

#include "cairoint.h"

#include "cairo-drm-private.h"
#include "cairo-drm-intel-private.h"

#include "cairo-default-context-private.h"
#include "cairo-error-private.h"
#include "cairo-image-surface-private.h"

/* Basic generic/stub surface for intel chipsets */

#define MAX_SIZE 2048

static cairo_surface_t *
intel_surface_create_similar (void			*abstract_surface,
			      cairo_content_t	 content,
			      int			 width,
			      int			 height)
{
    return cairo_image_surface_create (_cairo_format_from_content (content),
				       width, height);
}

cairo_status_t
intel_surface_finish (void *abstract_surface)
{
    intel_surface_t *surface = abstract_surface;

    intel_bo_in_flight_add (to_intel_device (surface->drm.base.device),
			    to_intel_bo (surface->drm.bo));
    return _cairo_drm_surface_finish (&surface->drm);
}

static void
surface_finish_and_destroy (cairo_surface_t *surface)
{
    cairo_surface_finish (surface);
    cairo_surface_destroy (surface);
}

cairo_status_t
intel_surface_acquire_source_image (void *abstract_surface,
				    cairo_image_surface_t **image_out,
				    void **image_extra)
{
    intel_surface_t *surface = abstract_surface;
    cairo_surface_t *image;
    cairo_status_t status;
    void *ptr;

    if (surface->drm.fallback != NULL) {
	image = surface->drm.fallback;
	goto DONE;
    }

    image = _cairo_surface_has_snapshot (&surface->drm.base,
	                                 &_cairo_image_surface_backend);
    if (image != NULL)
	goto DONE;

    if (surface->drm.base.backend->flush != NULL) {
	status = surface->drm.base.backend->flush (surface);
	if (unlikely (status))
	    return status;
    }

    ptr = intel_bo_map (to_intel_device (surface->drm.base.device),
			to_intel_bo (surface->drm.bo));
    if (unlikely (ptr == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    image = cairo_image_surface_create_for_data (ptr,
						 surface->drm.format,
						 surface->drm.width,
						 surface->drm.height,
						 surface->drm.stride);
    if (unlikely (image->status))
	return image->status;

    _cairo_surface_attach_snapshot (&surface->drm.base, image, surface_finish_and_destroy);

DONE:
    *image_out = (cairo_image_surface_t *) cairo_surface_reference (image);
    *image_extra = NULL;
    return CAIRO_STATUS_SUCCESS;
}

void
intel_surface_release_source_image (void *abstract_surface,
				    cairo_image_surface_t *image,
				    void *image_extra)
{
    cairo_surface_destroy (&image->base);
}

cairo_surface_t *
intel_surface_map_to_image (void *abstract_surface)
{
    intel_surface_t *surface = abstract_surface;

    if (surface->drm.fallback == NULL) {
	cairo_surface_t *image;
	cairo_status_t status;
	void *ptr;

	if (surface->drm.base.backend->flush != NULL) {
	    status = surface->drm.base.backend->flush (surface);
	    if (unlikely (status))
		return _cairo_surface_create_in_error (status);
	}

	ptr = intel_bo_map (to_intel_device (surface->drm.base.device),
			    to_intel_bo (surface->drm.bo));
	if (unlikely (ptr == NULL))
	    return _cairo_surface_create_in_error (CAIRO_STATUS_NO_MEMORY);

	image = cairo_image_surface_create_for_data (ptr,
						     surface->drm.format,
						     surface->drm.width,
						     surface->drm.height,
						     surface->drm.stride);
	if (unlikely (image->status))
	    return image;

	surface->drm.fallback = image;
    }

    return surface->drm.fallback;
}

cairo_status_t
intel_surface_flush (void *abstract_surface, unsigned flags)
{
    intel_surface_t *surface = abstract_surface;
    cairo_status_t status;

    if (flags)
	return CAIRO_STATUS_SUCCESS;

    if (surface->drm.fallback == NULL)
	return CAIRO_STATUS_SUCCESS;

    /* kill any outstanding maps */
    cairo_surface_finish (surface->drm.fallback);

    status = cairo_surface_status (surface->drm.fallback);
    cairo_surface_destroy (surface->drm.fallback);
    surface->drm.fallback = NULL;

    return status;
}

static cairo_int_status_t
intel_surface_paint (void *abstract_surface,
		     cairo_operator_t		 op,
		     const cairo_pattern_t	*source,
		     cairo_clip_t		*clip)
{
    return _cairo_surface_paint (intel_surface_map_to_image (abstract_surface),
				 op, source, clip);
}

static cairo_int_status_t
intel_surface_mask (void			*abstract_surface,
		    cairo_operator_t		 op,
		    const cairo_pattern_t	*source,
		    const cairo_pattern_t	*mask,
		    cairo_clip_t		*clip)
{
    return _cairo_surface_mask (intel_surface_map_to_image (abstract_surface),
				op, source, mask, clip);
}

static cairo_int_status_t
intel_surface_stroke (void			*abstract_surface,
		      cairo_operator_t		 op,
		      const cairo_pattern_t	*source,
		      cairo_path_fixed_t	*path,
		      const cairo_stroke_style_t	*stroke_style,
		      const cairo_matrix_t		*ctm,
		      const cairo_matrix_t		*ctm_inverse,
		      double			 tolerance,
		      cairo_antialias_t		 antialias,
		      cairo_clip_t		*clip)
{
    return _cairo_surface_stroke (intel_surface_map_to_image (abstract_surface),
				  op, source, path, stroke_style, ctm, ctm_inverse,
				  tolerance, antialias, clip);
}

static cairo_int_status_t
intel_surface_fill (void			*abstract_surface,
		    cairo_operator_t		 op,
		    const cairo_pattern_t	*source,
		    cairo_path_fixed_t		*path,
		    cairo_fill_rule_t		 fill_rule,
		    double			 tolerance,
		    cairo_antialias_t		 antialias,
		    cairo_clip_t		*clip)
{
    return _cairo_surface_fill (intel_surface_map_to_image (abstract_surface),
				op, source, path, fill_rule,
				tolerance, antialias, clip);
}

static cairo_int_status_t
intel_surface_glyphs (void			*abstract_surface,
		      cairo_operator_t		 op,
		      const cairo_pattern_t	*source,
		      cairo_glyph_t		*glyphs,
		      int			 num_glyphs,
		      cairo_scaled_font_t	*scaled_font,
		      cairo_clip_t		*clip,
		      int *num_remaining)
{
    *num_remaining = 0;
    return _cairo_surface_show_text_glyphs (intel_surface_map_to_image (abstract_surface),
					    op, source,
					    NULL, 0,
					    glyphs, num_glyphs,
					    NULL, 0, 0,
					    scaled_font, clip);
}

static const cairo_surface_backend_t intel_surface_backend = {
    CAIRO_SURFACE_TYPE_DRM,
    _cairo_default_context_create,

    intel_surface_create_similar,
    intel_surface_finish,

    NULL,
    intel_surface_acquire_source_image,
    intel_surface_release_source_image,

    NULL, NULL, NULL,
    NULL, /* composite */
    NULL, /* fill */
    NULL, /* trapezoids */
    NULL, /* span */
    NULL, /* check-span */

    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_drm_surface_get_extents,
    NULL, /* old-glyphs */
    _cairo_drm_surface_get_font_options,

    intel_surface_flush,
    NULL, /* mark dirty */
    NULL, NULL, /* font/glyph fini */

    intel_surface_paint,
    intel_surface_mask,
    intel_surface_stroke,
    intel_surface_fill,
    intel_surface_glyphs,
};

void
intel_surface_init (intel_surface_t *surface,
		    const cairo_surface_backend_t *backend,
		    cairo_drm_device_t *device,
		    cairo_format_t format,
		    int width, int height)
{
    _cairo_surface_init (&surface->drm.base,
			 backend,
			 &device->base,
			 _cairo_content_from_format (format),
			 FALSE);

    _cairo_drm_surface_init (&surface->drm, format, width, height);

    surface->snapshot_cache_entry.hash = 0;
}

static cairo_surface_t *
intel_surface_create (cairo_drm_device_t *device,
		      cairo_format_t format,
		      int width, int height)
{
    intel_surface_t *surface;
    cairo_status_t status;

    surface = _cairo_malloc (sizeof (intel_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    intel_surface_init (surface, &intel_surface_backend, device,
			format, width, height);

    if (width && height) {
	/* Vol I, p134: size restrictions for textures */
	width  = (width  + 3) & -4;
	height = (height + 1) & -2;
	surface->drm.stride =
	    cairo_format_stride_for_width (surface->drm.format, width);
	surface->drm.bo = &intel_bo_create (to_intel_device (&device->base),
					    surface->drm.stride * height,
					    surface->drm.stride * height,
					    TRUE, I915_TILING_NONE, surface->drm.stride)->base;
	if (surface->drm.bo == NULL) {
	    status = _cairo_drm_surface_finish (&surface->drm);
	    free (surface);
	    return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
	}
    }

    return &surface->drm.base;
}

static cairo_surface_t *
intel_surface_create_for_name (cairo_drm_device_t *device,
			       unsigned int name,
			       cairo_format_t format,
			       int width, int height, int stride)
{
    intel_surface_t *surface;
    cairo_status_t status;

    switch (format) {
    default:
    case CAIRO_FORMAT_INVALID:
    case CAIRO_FORMAT_A1:
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));
    case CAIRO_FORMAT_ARGB32:
    case CAIRO_FORMAT_RGB16_565:
    case CAIRO_FORMAT_RGB24:
    case CAIRO_FORMAT_A8:
	break;
    }

    if (stride < cairo_format_stride_for_width (format, width))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_STRIDE));

    surface = _cairo_malloc (sizeof (intel_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    intel_surface_init (surface, &intel_surface_backend,
			device, format, width, height);

    if (width && height) {
	surface->drm.stride = stride;

	surface->drm.bo = &intel_bo_create_for_name (to_intel_device (&device->base),
						      name)->base;
	if (unlikely (surface->drm.bo == NULL)) {
	    status = _cairo_drm_surface_finish (&surface->drm);
	    free (surface);
	    return _cairo_surface_create_in_error (_cairo_error
						   (CAIRO_STATUS_NO_MEMORY));
	}
    }

    return &surface->drm.base;
}

static cairo_status_t
intel_surface_enable_scan_out (void *abstract_surface)
{
    intel_surface_t *surface = abstract_surface;

    if (unlikely (surface->drm.bo == NULL))
	return _cairo_error (CAIRO_STATUS_INVALID_SIZE);

    to_intel_bo (surface->drm.bo)->tiling = I915_TILING_X;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
intel_device_throttle (cairo_drm_device_t *device)
{
    intel_throttle (to_intel_device (&device->base));
    return CAIRO_STATUS_SUCCESS;
}

static void
intel_device_destroy (void *data)
{
    intel_device_t *device = data;

    intel_device_fini (device);

    free (data);
}

cairo_drm_device_t *
_cairo_drm_intel_device_create (int fd, dev_t dev, int vendor_id, int chip_id)
{
    intel_device_t *device;
    cairo_status_t status;

    if (! intel_info (fd, NULL))
	return NULL;

    device = _cairo_malloc (sizeof (intel_device_t));
    if (unlikely (device == NULL))
	return (cairo_drm_device_t *) _cairo_device_create_in_error (CAIRO_STATUS_NO_MEMORY);

    status = intel_device_init (device, fd);
    if (unlikely (status)) {
	free (device);
	return (cairo_drm_device_t *) _cairo_device_create_in_error (status);
    }

    device->base.surface.create = intel_surface_create;
    device->base.surface.create_for_name = intel_surface_create_for_name;
    device->base.surface.create_from_cacheable_image = NULL;
    device->base.surface.flink = _cairo_drm_surface_flink;
    device->base.surface.enable_scan_out = intel_surface_enable_scan_out;

    device->base.surface.map_to_image = intel_surface_map_to_image;

    device->base.device.flush = NULL;
    device->base.device.throttle = intel_device_throttle;
    device->base.device.destroy = intel_device_destroy;

    return _cairo_drm_device_init (&device->base,
				   fd, dev,
				   vendor_id, chip_id,
				   MAX_SIZE);
}
