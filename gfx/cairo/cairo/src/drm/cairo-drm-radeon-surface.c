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
#include "cairo-drm-radeon-private.h"

#include "cairo-default-context-private.h"
#include "cairo-error-private.h"
#include "cairo-image-surface-private.h"

#include <stddef.h>
#include <inttypes.h>		/* workaround for broken <drm/radeon_drm.h> */
#include <drm/radeon_drm.h>

/* Basic stub surface for radeon chipsets */

#define MAX_SIZE 2048

typedef struct _radeon_surface {
    cairo_drm_surface_t base;
} radeon_surface_t;

static inline radeon_device_t *
to_radeon_device (cairo_device_t *device)
{
    return (radeon_device_t *) device;
}

static inline radeon_bo_t *
to_radeon_bo (cairo_drm_bo_t *bo)
{
    return (radeon_bo_t *) bo;
}

static cairo_surface_t *
radeon_surface_create_similar (void			*abstract_surface,
			      cairo_content_t		 content,
			      int			 width,
			      int			 height)
{
    return cairo_image_surface_create (_cairo_format_from_content (content),
				       width, height);
}

static cairo_status_t
radeon_surface_finish (void *abstract_surface)
{
    radeon_surface_t *surface = abstract_surface;

    return _cairo_drm_surface_finish (&surface->base);
}

static cairo_status_t
radeon_surface_acquire_source_image (void *abstract_surface,
				     cairo_image_surface_t **image_out,
				     void **image_extra)
{
    radeon_surface_t *surface = abstract_surface;
    cairo_surface_t *image;
    cairo_status_t status;

    /* XXX batch flush */

    if (surface->base.fallback != NULL) {
	image = surface->base.fallback;
	goto DONE;
    }

    image = _cairo_surface_has_snapshot (&surface->base.base,
	                                 &_cairo_image_surface_backend);
    if (image != NULL)
	goto DONE;

    if (surface->base.base.backend->flush != NULL) {
	status = surface->base.base.backend->flush (surface);
	if (unlikely (status))
	    return status;
    }

    image = radeon_bo_get_image (to_radeon_device (surface->base.base.device),
				to_radeon_bo (surface->base.bo),
				&surface->base);
    status = image->status;
    if (unlikely (status))
	return status;

    _cairo_surface_attach_snapshot (&surface->base.base, image, cairo_surface_destroy);

DONE:
    *image_out = (cairo_image_surface_t *) cairo_surface_reference (image);
    *image_extra = NULL;
    return CAIRO_STATUS_SUCCESS;
}

static void
radeon_surface_release_source_image (void *abstract_surface,
				     cairo_image_surface_t *image,
				     void *image_extra)
{
    cairo_surface_destroy (&image->base);
}

static cairo_surface_t *
radeon_surface_map_to_image (radeon_surface_t *surface)
{
    if (surface->base.fallback == NULL) {
	cairo_surface_t *image;
	cairo_status_t status;
	void *ptr;

	if (surface->base.base.backend->flush != NULL) {
	    status = surface->base.base.backend->flush (surface);
	    if (unlikely (status))
		return _cairo_surface_create_in_error (status);
	}

	ptr = radeon_bo_map (to_radeon_device (surface->base.base.device),
			    to_radeon_bo (surface->base.bo));
	if (unlikely (ptr == NULL))
	    return _cairo_surface_create_in_error (CAIRO_STATUS_NO_MEMORY);

	image = cairo_image_surface_create_for_data (ptr,
						     surface->base.format,
						     surface->base.width,
						     surface->base.height,
						     surface->base.stride);
	if (unlikely (image->status)) {
	    radeon_bo_unmap (to_radeon_bo (surface->base.bo));
	    return image;
	}

	surface->base.fallback = image;
    }

    return surface->base.fallback;
}

static cairo_status_t
radeon_surface_flush (void *abstract_surface,
		      unsigned flags)
{
    radeon_surface_t *surface = abstract_surface;
    cairo_status_t status;

    if (flags)
	return CAIRO_STATUS_SUCCESS;

    if (surface->base.fallback == NULL)
	return CAIRO_STATUS_SUCCESS;

    /* kill any outstanding maps */
    cairo_surface_finish (surface->base.fallback);

    status = cairo_surface_status (surface->base.fallback);
    cairo_surface_destroy (surface->base.fallback);
    surface->base.fallback = NULL;

    radeon_bo_unmap (to_radeon_bo (surface->base.bo));

    return status;
}

static cairo_int_status_t
radeon_surface_paint (void *abstract_surface,
		     cairo_operator_t		 op,
		     const cairo_pattern_t	*source,
		     cairo_clip_t		*clip)
{
    return _cairo_surface_paint (radeon_surface_map_to_image (abstract_surface),
				 op, source, clip);
}

static cairo_int_status_t
radeon_surface_mask (void			*abstract_surface,
		    cairo_operator_t		 op,
		    const cairo_pattern_t	*source,
		    const cairo_pattern_t	*mask,
		    cairo_clip_t		*clip)
{
    return _cairo_surface_mask (radeon_surface_map_to_image (abstract_surface),
				op, source, mask, clip);
}

static cairo_int_status_t
radeon_surface_stroke (void			*abstract_surface,
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
    return _cairo_surface_stroke (radeon_surface_map_to_image (abstract_surface),
				  op, source, path, stroke_style, ctm, ctm_inverse,
				  tolerance, antialias, clip);
}

static cairo_int_status_t
radeon_surface_fill (void			*abstract_surface,
		    cairo_operator_t		 op,
		    const cairo_pattern_t	*source,
		    cairo_path_fixed_t		*path,
		    cairo_fill_rule_t		 fill_rule,
		    double			 tolerance,
		    cairo_antialias_t		 antialias,
		    cairo_clip_t		*clip)
{
    return _cairo_surface_fill (radeon_surface_map_to_image (abstract_surface),
				op, source, path, fill_rule,
				tolerance, antialias, clip);
}

static cairo_int_status_t
radeon_surface_glyphs (void			*abstract_surface,
		      cairo_operator_t		 op,
		      const cairo_pattern_t	*source,
		      cairo_glyph_t		*glyphs,
		      int			 num_glyphs,
		      cairo_scaled_font_t	*scaled_font,
		      cairo_clip_t		*clip,
		      int *num_remaining)
{
    *num_remaining = 0;
    return _cairo_surface_show_text_glyphs (radeon_surface_map_to_image (abstract_surface),
					    op, source,
					    NULL, 0,
					    glyphs, num_glyphs,
					    NULL, 0, 0,
					    scaled_font, clip);
}

static const cairo_surface_backend_t radeon_surface_backend = {
    CAIRO_SURFACE_TYPE_DRM,
    _cairo_default_context_create,

    radeon_surface_create_similar,
    radeon_surface_finish,

    NULL,
    radeon_surface_acquire_source_image,
    radeon_surface_release_source_image,

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

    radeon_surface_flush,
    NULL, /* mark dirty */
    NULL, NULL, /* font/glyph fini */

    radeon_surface_paint,
    radeon_surface_mask,
    radeon_surface_stroke,
    radeon_surface_fill,
    radeon_surface_glyphs,
};

static void
radeon_surface_init (radeon_surface_t *surface,
		     cairo_drm_device_t *device,
		     cairo_format_t format,
		     int width, int height)
{
    _cairo_surface_init (&surface->base.base,
			 &radeon_surface_backend,
			 &device->base,
			 _cairo_content_from_format (format),
			FALSE);
    _cairo_drm_surface_init (&surface->base, format, width, height);
}

static cairo_surface_t *
radeon_surface_create_internal (cairo_drm_device_t *device,
				cairo_format_t format,
				int width, int height)
{
    radeon_surface_t *surface;
    cairo_status_t status;

    surface = _cairo_malloc (sizeof (radeon_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    radeon_surface_init (surface, device, format, width, height);

    if (width && height) {
	surface->base.stride =
	    cairo_format_stride_for_width (surface->base.format, width);

	surface->base.bo = radeon_bo_create (to_radeon_device (&device->base),
					     surface->base.stride * height,
					     RADEON_GEM_DOMAIN_GTT);

	if (unlikely (surface->base.bo == NULL)) {
	    status = _cairo_drm_surface_finish (&surface->base);
	    free (surface);
	    return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
	}
    }

    return &surface->base.base;
}

static cairo_surface_t *
radeon_surface_create (cairo_drm_device_t *device,
		       cairo_format_t format,
		       int width, int height)
{
    switch (format) {
    default:
    case CAIRO_FORMAT_INVALID:
    case CAIRO_FORMAT_A1:
    case CAIRO_FORMAT_RGB16_565:
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));
    case CAIRO_FORMAT_ARGB32:
    case CAIRO_FORMAT_RGB24:
    case CAIRO_FORMAT_A8:
	break;
    }

    return radeon_surface_create_internal (device, format, width, height);
}

static cairo_surface_t *
radeon_surface_create_for_name (cairo_drm_device_t *device,
			      unsigned int name,
			      cairo_format_t format,
			      int width, int height, int stride)
{
    radeon_surface_t *surface;
    cairo_status_t status;

    switch (format) {
    default:
    case CAIRO_FORMAT_INVALID:
    case CAIRO_FORMAT_A1:
    case CAIRO_FORMAT_RGB16_565:
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));
    case CAIRO_FORMAT_ARGB32:
    case CAIRO_FORMAT_RGB24:
    case CAIRO_FORMAT_A8:
	break;
    }

    if (stride < cairo_format_stride_for_width (format, width))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_STRIDE));

    surface = _cairo_malloc (sizeof (radeon_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    radeon_surface_init (surface, device, format, width, height);

    if (width && height) {
	surface->base.stride = stride;

	surface->base.bo = radeon_bo_create_for_name (to_radeon_device (&device->base),
						      name);

	if (unlikely (surface->base.bo == NULL)) {
	    status = _cairo_drm_surface_finish (&surface->base);
	    free (surface);
	    return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
	}
    }

    return &surface->base.base;
}

static void
radeon_device_destroy (void *data)
{
    radeon_device_t *device = data;

    radeon_device_fini (device);

    free (data);
}

cairo_drm_device_t *
_cairo_drm_radeon_device_create (int fd, dev_t dev, int vendor_id, int chip_id)
{
    radeon_device_t *device;
    uint64_t gart_size, vram_size;
    cairo_status_t status;

    if (! radeon_info (fd, &gart_size, &vram_size))
	return NULL;

    device = _cairo_malloc (sizeof (radeon_device_t));
    if (device == NULL)
	return _cairo_drm_device_create_in_error (CAIRO_STATUS_NO_MEMORY);

    status = radeon_device_init (device, fd);
    if (unlikely (status)) {
	free (device);
	return _cairo_drm_device_create_in_error (status);
    }

    device->base.surface.create = radeon_surface_create;
    device->base.surface.create_for_name = radeon_surface_create_for_name;
    device->base.surface.create_from_cacheable_image = NULL;
    device->base.surface.flink = _cairo_drm_surface_flink;
    device->base.surface.enable_scan_out = NULL;

    device->base.device.flush = NULL;
    device->base.device.throttle = NULL;
    device->base.device.destroy = radeon_device_destroy;

    device->vram_limit = vram_size;
    device->gart_limit = gart_size;

    return _cairo_drm_device_init (&device->base, fd, dev, vendor_id, chip_id, MAX_SIZE);
}
