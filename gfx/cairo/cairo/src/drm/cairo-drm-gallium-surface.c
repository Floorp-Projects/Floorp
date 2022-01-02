/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 Chris Wilson
 * Copyright © 2009 Eric Anholt
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
#include "cairo-default-context-private.h"
#include "cairo-error-private.h"

#include <dlfcn.h>

#include <state_tracker/drm_api.h>
#include <pipe/p_format.h>
#include <pipe/p_screen.h>
#include <pipe/p_context.h>
#include <pipe/p_state.h>

#include <util/u_inlines.h>

typedef struct _gallium_surface gallium_surface_t;
typedef struct _gallium_device gallium_device_t;

struct _gallium_device {
    cairo_drm_device_t drm;

    void *dlhandle;
    struct drm_api *api;

    struct pipe_screen *screen;
    struct pipe_context *pipe;

    int max_size;
};

struct _gallium_surface {
    cairo_drm_surface_t drm;

    enum pipe_format pipe_format;

    struct pipe_resource *texture;
    struct pipe_transfer *map_transfer;

    cairo_surface_t *fallback;
};

static cairo_surface_t *
gallium_surface_create_internal (gallium_device_t *device,
				 enum pipe_format format,
				 int width, int height);

static inline gallium_device_t *
gallium_device (gallium_surface_t *surface)
{
    return (gallium_device_t *) surface->drm.base.device;
}

static cairo_format_t
_cairo_format_from_pipe_format (enum pipe_format format)
{
    switch ((int) format) {
    case PIPE_FORMAT_A8_UNORM:
	return CAIRO_FORMAT_A8;
    case PIPE_FORMAT_A8R8G8B8_UNORM:
	return CAIRO_FORMAT_ARGB32;
    default:
	return CAIRO_FORMAT_INVALID;
    }
}

static enum pipe_format
pipe_format_from_format (cairo_format_t format)
{
    switch ((int) format) {
    case CAIRO_FORMAT_A8:
	return PIPE_FORMAT_A8_UNORM;
    case CAIRO_FORMAT_ARGB32:
	return PIPE_FORMAT_A8R8G8B8_UNORM;
    default:
	return (enum pipe_format) -1;
    }
}

static enum pipe_format
pipe_format_from_content (cairo_content_t content)
{
    if (content == CAIRO_CONTENT_ALPHA)
	return PIPE_FORMAT_A8_UNORM;
    else
	return PIPE_FORMAT_A8R8G8B8_UNORM;
}

static cairo_bool_t
format_is_supported_destination (gallium_device_t *device,
	                         enum pipe_format format)
{
    if (format == (enum pipe_format) -1)
	return FALSE;

    return device->screen->is_format_supported (device->screen,
					        format,
						0,
						PIPE_BIND_RENDER_TARGET,
						0);
}

#if 0
static cairo_bool_t
format_is_supported_source (gallium_device_t *device,
	                    enum pipe_format format)
{
    return device->screen->is_format_supported (device->screen,
					        format,
						0,
						PIPE_BIND_SAMPLER_VIEW,
						0);
}
#endif

static cairo_surface_t *
gallium_surface_create_similar (void			*abstract_src,
				cairo_content_t		 content,
				int			 width,
				int			 height)
{
    gallium_surface_t *other = abstract_src;
    gallium_device_t *device = gallium_device (other);
    enum pipe_format pipe_format;
    cairo_surface_t *surface = NULL;
    cairo_status_t status;

    status = cairo_device_acquire (&device->drm.base);
    if (unlikely (status))
	return _cairo_surface_create_in_error (status);

    if (MAX (width, height) > device->max_size)
	goto RELEASE;

    if (content == other->drm.base.content)
	pipe_format = other->pipe_format;
    else
	pipe_format = pipe_format_from_content (content);

    if (! format_is_supported_destination (device, pipe_format))
	goto RELEASE;

    surface = gallium_surface_create_internal (device,
					       pipe_format,
					       width, height);

RELEASE:
    cairo_device_release (&device->drm.base);

    return surface;
}

static cairo_status_t
gallium_surface_finish (void *abstract_surface)
{
    gallium_surface_t *surface = abstract_surface;
    gallium_device_t *device = gallium_device (surface);
    cairo_status_t status;

    status = cairo_device_acquire (&device->drm.base);
    if (likely (status == CAIRO_STATUS_SUCCESS)) {
	pipe_resource_reference (&surface->texture, NULL);
	cairo_device_release (&device->drm.base);
    }

    return _cairo_drm_surface_finish (&surface->drm);
}

static cairo_surface_t *
gallium_surface_map_to_image (gallium_surface_t *surface)
{
    gallium_device_t *device = gallium_device (surface);
    cairo_status_t status;
    void *ptr = NULL;

    status = cairo_device_acquire (&device->drm.base);
    if (unlikely (status))
	return _cairo_surface_create_in_error (status);

    surface->map_transfer =
	  pipe_get_transfer (device->pipe,
			     surface->texture, 0, 0, 0,
			     PIPE_TRANSFER_MAP_DIRECTLY |
			     PIPE_TRANSFER_READ_WRITE,
			     0, 0,
			     surface->drm.width,
			     surface->drm.height);
    if (likely (surface->map_transfer != NULL))
	ptr = device->pipe->transfer_map (device->pipe, surface->map_transfer);

    cairo_device_release (&device->drm.base);

    if (unlikely (ptr == NULL)) {
	if (surface->map_transfer != NULL) {
	    device->pipe->transfer_destroy (device->pipe,
					    surface->map_transfer);
	    surface->map_transfer = NULL;
	}
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
    }

    return cairo_image_surface_create_for_data (ptr,
						surface->drm.format,
						surface->drm.width,
						surface->drm.height,
						surface->map_transfer->stride);
}

static cairo_status_t
gallium_surface_acquire_source_image (void *abstract_surface,
				      cairo_image_surface_t **image_out,
				      void **image_extra)
{
    gallium_surface_t *surface = abstract_surface;
    gallium_device_t *device = gallium_device (surface);
    cairo_format_t format;
    cairo_surface_t *image;
    cairo_status_t status;
    struct pipe_transfer *transfer;
    void *ptr;

    if (surface->fallback != NULL) {
	*image_out = (cairo_image_surface_t *)
	    cairo_surface_reference (surface->fallback);
	*image_extra = NULL;
	return CAIRO_STATUS_SUCCESS;
    }

    if (unlikely (surface->drm.width == 0 || surface->drm.height == 0)) {
	image = cairo_image_surface_create (surface->drm.format, 0, 0);
	if (unlikely (image->status))
	    return image->status;

	*image_out = (cairo_image_surface_t *) image;
	*image_extra = NULL;
	return CAIRO_STATUS_SUCCESS;
    }

    format = _cairo_format_from_pipe_format (surface->pipe_format);
    if (format == CAIRO_FORMAT_INVALID)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = cairo_device_acquire (&device->drm.base);
    if (unlikely (status))
	return status;

    transfer = pipe_get_transfer (device->pipe,
				  surface->texture, 0, 0, 0,
				  PIPE_TRANSFER_READ,
				  0, 0,
				  surface->drm.width,
				  surface->drm.height);
    ptr = device->pipe->transfer_map (device->pipe, transfer);
    cairo_device_release (&device->drm.base);

    image = cairo_image_surface_create_for_data (ptr, format,
						 surface->drm.width,
						 surface->drm.height,
						 surface->drm.stride);
    if (unlikely (image->status))
	return image->status;

    *image_out = (cairo_image_surface_t *) image;
    *image_extra = transfer;
    return CAIRO_STATUS_SUCCESS;
}

static void
gallium_surface_release_source_image (void *abstract_surface,
				      cairo_image_surface_t *image,
				      void *image_extra)
{
    cairo_surface_destroy (&image->base);

    if (image_extra != NULL) {
	gallium_device_t *device = gallium_device (abstract_surface);

	device->pipe->transfer_unmap (device->pipe, image_extra);
	device->pipe->transfer_destroy (device->pipe, image_extra);
    }
}

static cairo_status_t
gallium_surface_flush (void *abstract_surface,
		       unsigned flags)
{
    gallium_surface_t *surface = abstract_surface;
    gallium_device_t *device = gallium_device (surface);
    cairo_status_t status;

    if (flags)
	return CAIRO_STATUS_SUCCESS;

    if (surface->fallback == NULL) {
	device->pipe->flush (device->pipe,
			     PIPE_FLUSH_RENDER_CACHE,
			     NULL);
	return CAIRO_STATUS_SUCCESS;
    }

    /* kill any outstanding maps */
    cairo_surface_finish (surface->fallback);

    status = cairo_device_acquire (&device->drm.base);
    if (likely (status == CAIRO_STATUS_SUCCESS)) {
	device->pipe->transfer_unmap (device->pipe,
				      surface->map_transfer);
	device->pipe->transfer_destroy (device->pipe,
					surface->map_transfer);
	surface->map_transfer = NULL;
	cairo_device_release (&device->drm.base);
    }

    status = cairo_surface_status (surface->fallback);
    cairo_surface_destroy (surface->fallback);
    surface->fallback = NULL;

    return status;
}

static cairo_int_status_t
gallium_surface_paint (void			*abstract_surface,
			  cairo_operator_t	 op,
			  const cairo_pattern_t	*source,
			  cairo_clip_t		*clip)
{
    gallium_surface_t *surface = abstract_surface;

    if (surface->fallback == NULL) {
	/* XXX insert magic */
	surface->fallback = gallium_surface_map_to_image (surface);
    }

    return _cairo_surface_paint (surface->fallback, op, source, clip);
}

static cairo_int_status_t
gallium_surface_mask (void			*abstract_surface,
			 cairo_operator_t	 op,
			 const cairo_pattern_t	*source,
			 const cairo_pattern_t	*mask,
			 cairo_clip_t		*clip)
{
    gallium_surface_t *surface = abstract_surface;

    if (surface->fallback == NULL) {
	/* XXX insert magic */
	surface->fallback = gallium_surface_map_to_image (surface);
    }

    return _cairo_surface_mask (surface->fallback,
				op, source, mask,
				clip);
}

static cairo_int_status_t
gallium_surface_stroke (void				*abstract_surface,
			   cairo_operator_t		 op,
			   const cairo_pattern_t	*source,
			   cairo_path_fixed_t		*path,
			   const cairo_stroke_style_t	*style,
			   const cairo_matrix_t		*ctm,
			   const cairo_matrix_t		*ctm_inverse,
			   double			 tolerance,
			   cairo_antialias_t		 antialias,
			   cairo_clip_t			*clip)
{
    gallium_surface_t *surface = abstract_surface;

    if (surface->fallback == NULL) {
	/* XXX insert magic */
	surface->fallback = gallium_surface_map_to_image (surface);
    }

    return _cairo_surface_stroke (surface->fallback,
				  op, source,
				  path, style,
				  ctm, ctm_inverse,
				  tolerance, antialias,
				  clip);
}

static cairo_int_status_t
gallium_surface_fill (void			*abstract_surface,
			 cairo_operator_t	 op,
			 const cairo_pattern_t	*source,
			 cairo_path_fixed_t	*path,
			 cairo_fill_rule_t	 fill_rule,
			 double			 tolerance,
			 cairo_antialias_t	 antialias,
			 cairo_clip_t		*clip)
{
    gallium_surface_t *surface = abstract_surface;

    if (surface->fallback == NULL) {
	/* XXX insert magic */
	surface->fallback = gallium_surface_map_to_image (surface);
    }

    return _cairo_surface_fill (surface->fallback,
				op, source,
				path, fill_rule,
				tolerance, antialias,
				clip);
}

static cairo_int_status_t
gallium_surface_glyphs (void				*abstract_surface,
			   cairo_operator_t		 op,
			   const cairo_pattern_t	*source,
			   cairo_glyph_t		*glyphs,
			   int				 num_glyphs,
			   cairo_scaled_font_t		*scaled_font,
			   cairo_clip_t			*clip,
			   int *num_remaining)
{
    gallium_surface_t *surface = abstract_surface;

    *num_remaining = 0;

    if (surface->fallback == NULL) {
	/* XXX insert magic */
	surface->fallback = gallium_surface_map_to_image (surface);
    }

    return _cairo_surface_show_text_glyphs (surface->fallback,
					    op, source,
					    NULL, 0,
					    glyphs, num_glyphs,
					    NULL, 0, 0,
					    scaled_font,
					    clip);
}

static const cairo_surface_backend_t gallium_surface_backend = {
    CAIRO_SURFACE_TYPE_DRM,
    _cairo_default_context_create,

    gallium_surface_create_similar,
    gallium_surface_finish,

    NULL,
    gallium_surface_acquire_source_image,
    gallium_surface_release_source_image,

    NULL, //gallium_surface_acquire_dest_image,
    NULL, //gallium_surface_release_dest_image,
    NULL, //gallium_surface_clone_similar,
    NULL, //gallium_surface_composite,
    NULL, //gallium_surface_fill_rectangles,
    NULL, //gallium_surface_composite_trapezoids,
    NULL, //gallium_surface_create_span_renderer,
    NULL, //gallium_surface_check_span_renderer,
    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_drm_surface_get_extents,
    NULL, /* old_show_glyphs */
    _cairo_drm_surface_get_font_options,
    gallium_surface_flush,
    NULL, /* mark_dirty_rectangle */
    NULL, //gallium_surface_scaled_font_fini,
    NULL, //gallium_surface_scaled_glyph_fini,

    gallium_surface_paint,
    gallium_surface_mask,
    gallium_surface_stroke,
    gallium_surface_fill,
    gallium_surface_glyphs,

    NULL, /* snapshot */

    NULL, /* is_similar */

    NULL, /* reset */
};

static int
gallium_format_stride_for_width (enum pipe_format format, int width)
{
    int stride;

    stride = 1024; /* XXX fugly */
    while (stride < width)
	stride *= 2;

    if (format == PIPE_FORMAT_A8R8G8B8_UNORM)
	stride *= 4;

    return stride;
}

static cairo_drm_bo_t *
_gallium_fake_bo_create (uint32_t size, uint32_t name)
{
    cairo_drm_bo_t *bo;

    /* XXX integrate with winsys handle */

    bo = _cairo_malloc (sizeof (cairo_drm_bo_t));

    CAIRO_REFERENCE_COUNT_INIT (&bo->ref_count, 1);
    bo->name = name;
    bo->handle = 0;
    bo->size = size;

    return bo;
}

static void
_gallium_fake_bo_release (void *dev, void *bo)
{
    free (bo);
}

static cairo_surface_t *
gallium_surface_create_internal (gallium_device_t *device,
				 enum pipe_format pipe_format,
				 int width, int height)
{
    gallium_surface_t *surface;
    struct pipe_resource template;
    cairo_status_t status;
    cairo_format_t format;
    int stride, size;

    surface = _cairo_malloc (sizeof (gallium_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    format = _cairo_format_from_pipe_format (pipe_format);
    _cairo_surface_init (&surface->drm.base,
			 &gallium_surface_backend,
			 &device->drm.base,
			 _cairo_content_from_format (format));
    _cairo_drm_surface_init (&surface->drm, format, width, height);

    stride = gallium_format_stride_for_width (pipe_format, width);
    size = stride * height;

    surface->drm.stride = stride;
    surface->drm.bo = _gallium_fake_bo_create (size, 0);

    memset(&template, 0, sizeof(template));
    template.target = PIPE_TEXTURE_2D;
    template.format = pipe_format;
    template.width0 = width;
    template.height0 = height;
    template.depth0 = 1;
    template.last_level = 0;
    template.bind = PIPE_BIND_RENDER_TARGET;
    surface->texture = device->screen->resource_create (device->screen,
							&template);

    if (unlikely (surface->texture == NULL)) {
	status = _cairo_drm_surface_finish (&surface->drm);
	free (surface);
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
    }

    surface->pipe_format = pipe_format;
    surface->texture = NULL;

    return &surface->drm.base;
}

static cairo_surface_t *
gallium_surface_create (cairo_drm_device_t *base_dev,
			cairo_format_t format,
			int width, int height)
{
    gallium_device_t *device = (gallium_device_t *) base_dev;
    cairo_surface_t *surface;
    enum pipe_format pipe_format;
    cairo_status_t status;

    status = cairo_device_acquire (&device->drm.base);

    if (MAX (width, height) > device->max_size) {
	surface = _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_SIZE));
	goto RELEASE;
    }

    pipe_format = pipe_format_from_format (format);
    if (! format_is_supported_destination (device, pipe_format)) {
	surface = _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));
	goto RELEASE;
    }

    surface = gallium_surface_create_internal (device,
					       pipe_format,
					       width, height);

RELEASE:
    cairo_device_release (&device->drm.base);

    return surface;
}

#if 0
static cairo_surface_t *
gallium_surface_create_for_name (cairo_drm_device_t *base_dev,
				 unsigned int name,
				 cairo_format_t format,
				 int width, int height, int stride)
{
    gallium_device_t *device;
    gallium_surface_t *surface;
    cairo_status_t status;
    cairo_content_t content;

    surface = _cairo_malloc (sizeof (gallium_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    switch (format) {
    default:
    case CAIRO_FORMAT_INVALID:
    case CAIRO_FORMAT_A1:
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));
    case CAIRO_FORMAT_A8:
	surface->pipe_format = PIPE_FORMAT_A8_UNORM;
	break;
    case CAIRO_FORMAT_RGB24:
    case CAIRO_FORMAT_ARGB32:
	surface->pipe_format = PIPE_FORMAT_A8R8G8B8_UNORM;
	break;
    }

    status = cairo_device_acquire (&device->drm.base);

    if (MAX (width, height) > device->max_size) {
	cairo_device_release (&device->drm.base);
	free (surface);
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_SIZE));
    }

    if (! format_is_supported_destination (device, surface->pipe_format)) {
	cairo_device_release (&device->drm.base);
	free (surface);
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));
    }

    content = _cairo_content_from_format (format);
    _cairo_surface_init (&surface->drm.base,
			 &gallium_surface_backend,
			 content);
    _cairo_drm_surface_init (&surface->drm, base_dev);

    surface->drm.bo = _gallium_fake_bo_create (height * stride, name);

    surface->drm.width  = width;
    surface->drm.height = height;
    surface->drm.stride = stride;

#if 0
    /* XXX screen->create_from_handle */
    surface->buffer = device->api->buffer_from_handle (device->api,
						       device->screen,
						       "cairo-gallium alien",
						       name);
    if (unlikely (surface->buffer == NULL)) {
	status = _cairo_drm_surface_finish (&surface->drm);
	cairo_device_release (&device->drm.base);
	free (surface);
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
    }
#endif

    surface->texture = NULL;

    surface->fallback = NULL;

    cairo_device_release (&device->drm.base);

    return &surface->drm.base;
}

static cairo_int_status_t
gallium_surface_flink (void *abstract_surface)
{
    gallium_surface_t *surface = abstract_surface;
    gallium_device_t *device;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    status = cairo_device_acquire (&device->drm.base);
    if (! device->api->global_handle_from_buffer (device->api,
						  device->screen,
						  surface->buffer,
						  &surface->drm.bo->name))
    {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }
    cairo_device_release (&device->drm.base);

    return status;
}
#endif

static void
gallium_device_destroy (void *abstract_device)
{
    gallium_device_t *device = abstract_device;

    device->pipe->destroy (device->pipe);
    device->screen->destroy (device->screen);
    device->api->destroy (device->api);

    dlclose (device->dlhandle);
    free (device);
}

cairo_drm_device_t *
_cairo_drm_gallium_device_create (int fd, dev_t dev, int vendor_id, int chip_id)
{
    gallium_device_t *device;
    cairo_status_t status;
    void *handle;
    const char *libdir;
    char buf[4096];
    struct drm_api *(*ctor) (void);

    /* XXX need search path + probe */
    libdir = getenv ("CAIRO_GALLIUM_LIBDIR");
    if (libdir == NULL)
	libdir = "/usr/lib/dri";
    buf[snprintf (buf, sizeof (buf)-1, "%s/i915_dri.so", libdir)] = '\0';

    handle = dlopen (buf, RTLD_LAZY);
    if (handle == NULL)
	return NULL;

    ctor = dlsym (handle, "drm_api_create");
    if (ctor == NULL) {
	dlclose (handle);
	return NULL;
    }

    device = _cairo_malloc (sizeof (gallium_device_t));
    if (device == NULL) {
	dlclose (handle);
	return _cairo_drm_device_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
    }

    device->dlhandle = handle;

    device->drm.surface.create = gallium_surface_create;
    device->drm.surface.create_for_name = NULL;
    //device->drm.surface.create_for_name = gallium_surface_create_for_name;
    device->drm.surface.enable_scan_out = NULL;
    //device->drm.surface.flink = gallium_surface_flink;
    device->drm.surface.flink = NULL;

    device->drm.device.flush = NULL;
    device->drm.device.throttle = NULL;
    device->drm.device.destroy = gallium_device_destroy;

    device->drm.bo.release = _gallium_fake_bo_release;

    device->api = ctor ();
    if (device->api == NULL) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto CLEANUP;
    }

    device->screen = device->api->create_screen (device->api, fd, NULL);
    if (device->screen == NULL) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto CLEANUP_API;
    }

    device->max_size = 1 << device->screen->get_param (device->screen,
						       PIPE_CAP_MAX_TEXTURE_2D_LEVELS);

    device->pipe = device->screen->context_create (device->screen, device);
    if (device->pipe == NULL) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto CLEANUP_SCREEN;
    }

    return _cairo_drm_device_init (&device->drm,
				   fd, dev,
				   0, 0,
				   device->max_size);

CLEANUP_SCREEN:
    device->screen->destroy (device->screen);
CLEANUP_API:
    device->api->destroy (device->api);
CLEANUP:
    free (device);
    dlclose (handle);
    return _cairo_drm_device_create_in_error (status);
}
