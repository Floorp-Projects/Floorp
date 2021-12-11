/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 Kristian Høgsberg
 * Copyright © 2009 Chris Wilson
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
 * Contributor(s):
 *	Chris Wilson <chris@chris-wilson.co.uk>
 *      Kristian Høgsberg <krh@bitplanet.net>
 */

#include "cairoint.h"

#include "cairo-error-private.h"
#include "cairo-drm-i965-private.h"
#include "cairo-surface-subsurface-private.h"
#include "cairo-surface-snapshot-private.h"

#include "cairo-drm-intel-brw-eu.h"

/* Theory of shaders:
 *
 * 3 types of rectangular inputs:
 *  (a) standard composite: x,y, use source, mask matrices to compute texcoords
 *  (b) spans: x,y, alpha, use source matrix
 *  (c) glyphs: x,y, s,t, use source matrix
 *
 * 5 types of pixel shaders:
 *  (a) Solid colour
 *  (b) Linear gradient (via 1D texture, with precomputed tex)
 *  (c) Radial gradient (per-pixel s computation, 1D texture)
 *  (d) Spans (mask only): apply opacity
 *  (e) Texture (includes glyphs).
 *
 *  Clip masks are limited to 2D textures only.
 */

/* XXX dual source blending for LERP + ComponentAlpha!!! */

#define BRW_GRF_BLOCKS(nreg)    ((nreg + 15) / 16 - 1)

#define SF_KERNEL_NUM_GRF  1
#define SF_MAX_THREADS	   24

#define PS_MAX_THREADS_CTG 50
#define PS_MAX_THREADS_BRW 32

#define URB_CS_ENTRY_SIZE     3 /* We need 4 matrices + 2 sources */
#define URB_CS_ENTRIES	      4 /* 4x sets of CONSTANT_BUFFER */

#define URB_VS_ENTRY_SIZE     1
#define URB_VS_ENTRIES	      8

#define URB_GS_ENTRY_SIZE     0
#define URB_GS_ENTRIES	      0

#define URB_CLIP_ENTRY_SIZE   0
#define URB_CLIP_ENTRIES      0

#define URB_SF_ENTRY_SIZE     1
#define URB_SF_ENTRIES	      (SF_MAX_THREADS + 1)

static void
i965_pipelined_flush (i965_device_t *device)
{
    intel_bo_t *bo, *next;

    if (device->batch.used == 0)
	return;

    OUT_BATCH (BRW_PIPE_CONTROL |
	       BRW_PIPE_CONTROL_NOWRITE |
	       BRW_PIPE_CONTROL_WC_FLUSH |
	       2);
    OUT_BATCH(0);   /* Destination address */ 
    OUT_BATCH(0);   /* Immediate data low DW */ 
    OUT_BATCH(0);   /* Immediate data high DW */ 

    cairo_list_foreach_entry_safe (bo, next, intel_bo_t, &device->flush, link) {
	bo->batch_write_domain = 0;
	cairo_list_init (&bo->link);
    }
    cairo_list_init (&device->flush);
}

static cairo_status_t
i965_shader_acquire_solid (i965_shader_t *shader,
			   union i965_shader_channel *src,
			   const cairo_solid_pattern_t *solid,
			   const cairo_rectangle_int_t *extents)
{
    src->type.fragment = FS_CONSTANT;
    src->type.vertex = VS_NONE;
    src->type.pattern = PATTERN_SOLID;

    src->base.content = _cairo_color_get_content (&solid->color);
    src->base.constants[0] = solid->color.red   * solid->color.alpha;
    src->base.constants[1] = solid->color.green * solid->color.alpha;
    src->base.constants[2] = solid->color.blue  * solid->color.alpha;
    src->base.constants[3] = solid->color.alpha;
    src->base.constants_size = 4;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
i965_shader_acquire_linear (i965_shader_t *shader,
			    union i965_shader_channel *src,
			    const cairo_linear_pattern_t *linear,
			    const cairo_rectangle_int_t *extents)
{
    intel_buffer_t buffer;
    cairo_status_t status;
    double x0, y0, sf;
    double dx, dy, offset;

    status = intel_gradient_render (&i965_device (shader->target)->intel,
				    &linear->base, &buffer);
    if (unlikely (status))
	return status;

    src->type.vertex = VS_NONE;
    src->type.pattern = PATTERN_LINEAR;
    src->type.fragment = FS_LINEAR;
    src->base.bo = buffer.bo;
    src->base.content = CAIRO_CONTENT_COLOR_ALPHA;
    src->base.format = buffer.format;
    src->base.width  = buffer.width;
    src->base.height = buffer.height;
    src->base.stride = buffer.stride;
    src->base.filter = i965_filter (CAIRO_FILTER_BILINEAR);
    src->base.extend = i965_extend (linear->base.base.extend);

    dx = linear->pd2.x - linear->pd1.x;
    dy = linear->pd2.y - linear->pd1.y;
    sf = 1. / (dx * dx + dy * dy);
    dx *= sf;
    dy *= sf;

    x0 = linear->pd1.x;
    y0 = linear->pd1.y;
    offset = dx*x0 + dy*y0;

    if (_cairo_matrix_is_identity (&linear->base.base.matrix)) {
	src->base.matrix.xx = dx;
	src->base.matrix.xy = dy;
	src->base.matrix.x0 = -offset;
    } else {
	cairo_matrix_t m;

	cairo_matrix_init (&m, dx, 0, dy, 0, -offset, 0);
	cairo_matrix_multiply (&src->base.matrix, &linear->base.base.matrix, &m);
    }
    src->base.matrix.yx = 0.;
    src->base.matrix.yy = 1.;
    src->base.matrix.y0 = 0.;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
i965_shader_acquire_radial (i965_shader_t *shader,
			    union i965_shader_channel *src,
			    const cairo_radial_pattern_t *radial,
			    const cairo_rectangle_int_t *extents)
{
    intel_buffer_t buffer;
    cairo_status_t status;
    double dx, dy, dr, r1;

    status = intel_gradient_render (&i965_device (shader->target)->intel,
				    &radial->base, &buffer);
    if (unlikely (status))
	return status;

    src->type.vertex = VS_NONE;
    src->type.pattern = PATTERN_RADIAL;
    src->type.fragment = FS_RADIAL;
    src->base.bo = buffer.bo;
    src->base.content = CAIRO_CONTENT_COLOR_ALPHA;
    src->base.format = buffer.format;
    src->base.width  = buffer.width;
    src->base.height = buffer.height;
    src->base.stride = buffer.stride;
    src->base.filter = i965_filter (CAIRO_FILTER_BILINEAR);
    src->base.extend = i965_extend (radial->base.base.extend);

    dx = radial->cd2.center.x - radial->cd1.center.x;
    dy = radial->cd2.center.y - radial->cd1.center.y;
    dr = radial->cd2.radius   - radial->cd1.radius;

    r1 = radial->cd1.radius;

    if (FALSE && (radial->cd2.center.x == radial->cd1.center.x &&
		  radial->cd2.center.y == radial->cd1.center.y))
    {
	/* XXX dr == 0, meaningless with anything other than PAD */
	src->base.constants[0] = radial->cd1.center.x / dr;
	src->base.constants[1] = radial->cd1.center.y / dr;
	src->base.constants[2] = 1. / dr;
	src->base.constants[3] = -r1 / dr;

	src->base.constants_size = 4;
	src->base.mode = RADIAL_ONE;
    } else {
	src->base.constants[0] = -radial->cd1.center.x;
	src->base.constants[1] = -radial->cd1.center.y;
	src->base.constants[2] = r1;
	src->base.constants[3] = -4 * (dx*dx + dy*dy - dr*dr);

	src->base.constants[4] = -2 * dx;
	src->base.constants[5] = -2 * dy;
	src->base.constants[6] = -2 * r1 * dr;
	src->base.constants[7] = 1 / (2 * (dx*dx + dy*dy - dr*dr));

	src->base.constants_size = 8;
	src->base.mode = RADIAL_TWO;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
i965_surface_clone (i965_device_t *device,
		    cairo_image_surface_t *image,
		    i965_surface_t **clone_out)
{
    i965_surface_t *clone;
    cairo_status_t status;

    clone = (i965_surface_t *)
	i965_surface_create_internal (&device->intel.base,
				      image->base.content,
				      image->width,
				      image->height,
				      I965_TILING_DEFAULT,
				      FALSE);
    if (unlikely (clone->intel.drm.base.status))
	return clone->intel.drm.base.status;

    status = intel_bo_put_image (&device->intel,
				 to_intel_bo (clone->intel.drm.bo),
				 image,
				 0, 0,
				 image->width, image->height,
				 0, 0);

    if (unlikely (status)) {
	cairo_surface_destroy (&clone->intel.drm.base);
	return status;
    }

    status = intel_snapshot_cache_insert (&device->intel, &clone->intel);
    if (unlikely (status)) {
	cairo_surface_destroy (&clone->intel.drm.base);
	return status;
    }

    _cairo_surface_attach_snapshot (&image->base,
				    &clone->intel.drm.base,
				    intel_surface_detach_snapshot);

    *clone_out = clone;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
i965_surface_clone_subimage (i965_device_t *device,
			     cairo_image_surface_t *image,
			     const cairo_rectangle_int_t *extents,
			     i965_surface_t **clone_out)
{
    i965_surface_t *clone;
    cairo_status_t status;

    clone = (i965_surface_t *)
	i965_surface_create_internal (&device->intel.base,
				      image->base.content,
				      extents->width,
				      extents->height,
				      I965_TILING_DEFAULT,
				      FALSE);
    if (unlikely (clone->intel.drm.base.status))
	return clone->intel.drm.base.status;

    status = intel_bo_put_image (to_intel_device (clone->intel.drm.base.device),
				 to_intel_bo (clone->intel.drm.bo),
				 image,
				 extents->x, extents->y,
				 extents->width, extents->height,
				 0, 0);
    if (unlikely (status))
	return status;

    *clone_out = clone;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
i965_shader_acquire_solid_surface (i965_shader_t *shader,
				   union i965_shader_channel *src,
				   cairo_surface_t *surface,
				   const cairo_rectangle_int_t *extents)
{
    cairo_image_surface_t *image;
    void *image_extra;
    cairo_status_t status;
    uint32_t argb;

    status = _cairo_surface_acquire_source_image (surface, &image, &image_extra);
    if (unlikely (status))
	return status;

    if (image->format != CAIRO_FORMAT_ARGB32) {
	cairo_surface_t *pixel;
	cairo_surface_pattern_t pattern;

	/* extract the pixel as argb32 */
	pixel = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 1, 1);
	_cairo_pattern_init_for_surface (&pattern, &image->base);
	cairo_matrix_init_translate (&pattern.base.matrix, extents->x, extents->y);
	pattern.base.filter = CAIRO_FILTER_NEAREST;
	status = _cairo_surface_paint (pixel, CAIRO_OPERATOR_SOURCE, &pattern.base, NULL);
	_cairo_pattern_fini (&pattern.base);

	if (unlikely (status)) {
	    _cairo_surface_release_source_image (surface, image, image_extra);
	    cairo_surface_destroy (pixel);
	    return status;
	}

	argb = *(uint32_t *) ((cairo_image_surface_t *) pixel)->data;
	cairo_surface_destroy (pixel);
    } else {
	argb = ((uint32_t *) (image->data + extents->y * image->stride))[extents->x];
    }

    _cairo_surface_release_source_image (surface, image, image_extra);

    if (argb >> 24 == 0)
	argb = 0;

    src->base.constants[0] = ((argb >> 16) & 0xff) / 255.;
    src->base.constants[1] = ((argb >>  8) & 0xff) / 255.;
    src->base.constants[2] = ((argb >>  0) & 0xff) / 255.;
    src->base.constants[3] = ((argb >> 24) & 0xff) / 255.;
    src->base.constants_size = 4;

    src->base.content  = CAIRO_CONTENT_COLOR_ALPHA;
    if (CAIRO_ALPHA_IS_OPAQUE(src->base.constants[3]))
	src->base.content &= ~CAIRO_CONTENT_ALPHA;
    src->type.fragment = FS_CONSTANT;
    src->type.vertex   = VS_NONE;
    src->type.pattern  = PATTERN_SOLID;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
i965_shader_acquire_surface (i965_shader_t *shader,
			     union i965_shader_channel *src,
			     const cairo_surface_pattern_t *pattern,
			     const cairo_rectangle_int_t *extents)
{
    cairo_surface_t *surface, *drm;
    cairo_matrix_t m;
    cairo_status_t status;
    int src_x = 0, src_y = 0;

    assert (src->type.fragment == FS_NONE);
    drm = surface = pattern->surface;

    if (surface->type == CAIRO_SURFACE_TYPE_DRM) {
	if (surface->backend->type == CAIRO_SURFACE_TYPE_SUBSURFACE) {
	    drm = ((cairo_surface_subsurface_t *) surface)->target;
	} else if (surface->backend->type == CAIRO_INTERNAL_SURFACE_TYPE_SNAPSHOT) {
	    drm = ((cairo_surface_snapshot_t *) surface)->target;
	}
    }

    src->type.pattern = PATTERN_SURFACE;
    src->surface.surface = NULL;
    if (drm->type == CAIRO_SURFACE_TYPE_DRM) {
	i965_surface_t *s = (i965_surface_t *) drm;

	if (surface->backend->type == CAIRO_SURFACE_TYPE_SUBSURFACE) {
	    if (s->intel.drm.base.device == shader->target->intel.drm.base.device) {
		cairo_surface_subsurface_t *sub = (cairo_surface_subsurface_t *) surface;
		if (s != shader->target) {
		    int x;

		    if (s->intel.drm.fallback != NULL) {
			status = intel_surface_flush (s, 0);
			if (unlikely (status))
			    return status;
		    }

		    if (to_intel_bo (s->intel.drm.bo)->batch_write_domain)
			i965_pipelined_flush (i965_device (s));

		    src->type.fragment = FS_SURFACE;

		    src->base.bo = to_intel_bo (s->intel.drm.bo);
		    src->base.format = s->intel.drm.format;
		    src->base.content = s->intel.drm.base.content;
		    src->base.width = sub->extents.width;
		    src->base.height = sub->extents.height;
		    src->base.stride = s->intel.drm.stride;

		    x = sub->extents.x;
		    if (s->intel.drm.format != CAIRO_FORMAT_A8)
			x *= 4;

		    /* XXX tiling restrictions upon offset? */
		    //src->base.offset[0] = s->offset + sub->extents.y * s->intel.drm.stride + x;
		} else {
		    i965_surface_t *clone;
		    cairo_surface_pattern_t pattern;

		    clone = (i965_surface_t *)
			i965_surface_create_internal ((cairo_drm_device_t *) s->intel.drm.base.device,
						      s->intel.drm.base.content,
						      sub->extents.width,
						      sub->extents.height,
						      I965_TILING_DEFAULT,
						      TRUE);
		    if (unlikely (clone->intel.drm.base.status))
			return clone->intel.drm.base.status;

		    _cairo_pattern_init_for_surface (&pattern, &s->intel.drm.base);
		    pattern.base.filter = CAIRO_FILTER_NEAREST;
		    cairo_matrix_init_translate (&pattern.base.matrix,
						 sub->extents.x, sub->extents.y);

		    status = _cairo_surface_paint (&clone->intel.drm.base,
						   CAIRO_OPERATOR_SOURCE,
						   &pattern.base,
						   NULL);

		    _cairo_pattern_fini (&pattern.base);

		    if (unlikely (status)) {
			cairo_surface_destroy (&clone->intel.drm.base);
			return status;
		    }

		    i965_pipelined_flush (i965_device (s));
		    src->type.fragment = FS_SURFACE;

		    src->base.bo = to_intel_bo (clone->intel.drm.bo);
		    src->base.format = clone->intel.drm.format;
		    src->base.content = clone->intel.drm.base.content;
		    src->base.width = clone->intel.drm.width;
		    src->base.height = clone->intel.drm.height;
		    src->base.stride = clone->intel.drm.stride;

		    src->surface.surface = &clone->intel.drm.base;
		}

		src_x = sub->extents.x;
		src_y = sub->extents.y;
	    }
	} else {
	    if (s->intel.drm.base.device == shader->target->intel.drm.base.device) {
		if (s != shader->target) {
		    if (s->intel.drm.fallback != NULL) {
			status = intel_surface_flush (s, 0);
			if (unlikely (status))
			    return status;
		    }

		    if (to_intel_bo (s->intel.drm.bo)->batch_write_domain)
			i965_pipelined_flush (i965_device (s));

		    src->type.fragment = FS_SURFACE;

		    src->base.bo = to_intel_bo (s->intel.drm.bo);
		    src->base.format = s->intel.drm.format;
		    src->base.content = s->intel.drm.base.content;
		    src->base.width = s->intel.drm.width;
		    src->base.height = s->intel.drm.height;
		    src->base.stride = s->intel.drm.stride;
		} else {
		    i965_surface_t *clone;
		    cairo_surface_pattern_t pattern;

		    clone = (i965_surface_t *)
			i965_surface_create_internal ((cairo_drm_device_t *) s->intel.drm.base.device,
						      s->intel.drm.base.content,
						      s->intel.drm.width,
						      s->intel.drm.height,
						      I965_TILING_DEFAULT,
						      TRUE);
		    if (unlikely (clone->intel.drm.base.status))
			return clone->intel.drm.base.status;

		    _cairo_pattern_init_for_surface (&pattern, &s->intel.drm.base);
		    pattern.base.filter = CAIRO_FILTER_NEAREST;
		    status = _cairo_surface_paint (&clone->intel.drm.base,
						   CAIRO_OPERATOR_SOURCE,
						   &pattern.base,
						   NULL);

		    _cairo_pattern_fini (&pattern.base);

		    if (unlikely (status)) {
			cairo_surface_destroy (&clone->intel.drm.base);
			return status;
		    }

		    i965_pipelined_flush (i965_device (s));
		    src->type.fragment = FS_SURFACE;

		    src->base.bo = to_intel_bo (clone->intel.drm.bo);
		    src->base.format = clone->intel.drm.format;
		    src->base.content = clone->intel.drm.base.content;
		    src->base.width = clone->intel.drm.width;
		    src->base.height = clone->intel.drm.height;
		    src->base.stride = clone->intel.drm.stride;

		    src->surface.surface = &clone->intel.drm.base;
		}
	    }
	}
    }

    if (src->type.fragment == FS_NONE) {
	i965_surface_t *s;

	if (extents->width == 1 && extents->height == 1) {
	    return i965_shader_acquire_solid_surface (shader, src,
						      surface, extents);
	}

	s = (i965_surface_t *)
	    _cairo_surface_has_snapshot (surface,
					 shader->target->intel.drm.base.backend);
	if (s != NULL) {
	    i965_device_t *device = i965_device (shader->target);
	    intel_bo_t *bo = to_intel_bo (s->intel.drm.bo);

	    if (bo->purgeable &&
		! intel_bo_madvise (&device->intel, bo, I915_MADV_WILLNEED))
	    {
		_cairo_surface_detach_snapshot (&s->intel.drm.base);
		s = NULL;
	    }

	    if (s != NULL)
		cairo_surface_reference (&s->intel.drm.base);
	}

	if (s == NULL) {
	    cairo_image_surface_t *image;
	    void *image_extra;
	    cairo_status_t status;

	    status = _cairo_surface_acquire_source_image (surface, &image, &image_extra);
	    if (unlikely (status))
		return status;

	    if (image->width < 8192 && image->height < 8192) {
		status = i965_surface_clone (i965_device (shader->target), image, &s);
	    } else {
		status = i965_surface_clone_subimage (i965_device (shader->target),
						      image, extents, &s);
		src_x = -extents->x;
		src_y = -extents->y;
	    }

	    _cairo_surface_release_source_image (surface, image, image_extra);

	    if (unlikely (status))
		return status;

	    /* XXX? */
	    //intel_bo_mark_purgeable (to_intel_bo (s->intel.drm.bo), TRUE);
	}

	src->type.fragment = FS_SURFACE;

	src->base.bo = to_intel_bo (s->intel.drm.bo);
	src->base.content = s->intel.drm.base.content;
	src->base.format = s->intel.drm.format;
	src->base.width  = s->intel.drm.width;
	src->base.height = s->intel.drm.height;
	src->base.stride = s->intel.drm.stride;

	src->surface.surface = &s->intel.drm.base;

	drm = &s->intel.drm.base;
    }

    /* XXX transform nx1 or 1xn surfaces to 1D? */

    src->type.vertex = VS_NONE;

    src->base.extend = i965_extend (pattern->base.extend);
    if (pattern->base.extend == CAIRO_EXTEND_NONE &&
	extents->x >= 0 && extents->y >= 0 &&
	extents->x + extents->width  <= src->base.width &&
	extents->y + extents->height <= src->base.height)
    {
	/* Convert a wholly contained NONE to a REFLECT as the contiguous sampler
	 * cannot not handle CLAMP_BORDER textures.
	 */
	src->base.extend = i965_extend (CAIRO_EXTEND_REFLECT);
	/* XXX also need to check |u,v| < 3 */
    }

    src->base.filter = i965_filter (pattern->base.filter);
    if (_cairo_matrix_is_pixel_exact (&pattern->base.matrix))
	src->base.filter = i965_filter (CAIRO_FILTER_NEAREST);

    /* tweak the src matrix to map from dst to texture coordinates */
    src->base.matrix = pattern->base.matrix;
    if (src_x | src_y)
	cairo_matrix_translate (&src->base.matrix, src_x, src_x);
    cairo_matrix_init_scale (&m, 1. / src->base.width, 1. / src->base.height);
    cairo_matrix_multiply (&src->base.matrix, &src->base.matrix, &m);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
i965_shader_acquire_pattern (i965_shader_t *shader,
			     union i965_shader_channel *src,
			     const cairo_pattern_t *pattern,
			     const cairo_rectangle_int_t *extents)
{
    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	return i965_shader_acquire_solid (shader, src,
					  (cairo_solid_pattern_t *) pattern,
					  extents);

    case CAIRO_PATTERN_TYPE_LINEAR:
	return i965_shader_acquire_linear (shader, src,
					   (cairo_linear_pattern_t *) pattern,
					   extents);

    case CAIRO_PATTERN_TYPE_RADIAL:
	return i965_shader_acquire_radial (shader, src,
					   (cairo_radial_pattern_t *) pattern,
					   extents);

    case CAIRO_PATTERN_TYPE_SURFACE:
	return i965_shader_acquire_surface (shader, src,
					    (cairo_surface_pattern_t *) pattern,
					    extents);

    default:
	ASSERT_NOT_REACHED;
	return CAIRO_STATUS_SUCCESS;
    }
}

static void
i965_shader_channel_init (union i965_shader_channel *channel)
{
    channel->type.vertex = VS_NONE;
    channel->type.fragment = FS_NONE;
    channel->type.pattern = PATTERN_NONE;

    channel->base.mode = 0;
    channel->base.bo = NULL;
    channel->base.filter = i965_extend (CAIRO_FILTER_NEAREST);
    channel->base.extend = i965_extend (CAIRO_EXTEND_NONE);
    channel->base.has_component_alpha = 0;
    channel->base.constants_size = 0;
}

void
i965_shader_init (i965_shader_t *shader,
		  i965_surface_t *dst,
		  cairo_operator_t op)
{
    shader->committed = FALSE;
    shader->device = i965_device (dst);
    shader->target = dst;
    shader->op = op;
    shader->constants_size = 0;

    shader->need_combine = FALSE;

    i965_shader_channel_init (&shader->source);
    i965_shader_channel_init (&shader->mask);
    i965_shader_channel_init (&shader->clip);
    i965_shader_channel_init (&shader->dst);
}

void
i965_shader_fini (i965_shader_t *shader)
{
    if (shader->source.type.pattern == PATTERN_SURFACE)
	cairo_surface_destroy (shader->source.surface.surface);
    if (shader->mask.type.pattern == PATTERN_SURFACE)
	cairo_surface_destroy (shader->mask.surface.surface);
    if (shader->clip.type.pattern == PATTERN_SURFACE)
	cairo_surface_destroy (shader->clip.surface.surface);
    if (shader->dst.type.pattern == PATTERN_SURFACE)
	cairo_surface_destroy (shader->dst.surface.surface);
}

void
i965_shader_set_clip (i965_shader_t *shader,
		      cairo_clip_t *clip)
{
    cairo_surface_t *clip_surface;
    int clip_x, clip_y;
    union i965_shader_channel *channel;
    i965_surface_t *s;

    clip_surface = _cairo_clip_get_surface (clip, &shader->target->intel.drm.base, &clip_x, &clip_y);
    assert (clip_surface->status == CAIRO_STATUS_SUCCESS);
    assert (clip_surface->type == CAIRO_SURFACE_TYPE_DRM);
    s = (i965_surface_t *) clip_surface;

    if (to_intel_bo (s->intel.drm.bo)->batch_write_domain)
	i965_pipelined_flush (i965_device (s));

    channel = &shader->clip;
    channel->type.pattern = PATTERN_BASE;
    channel->type.vertex  = VS_NONE;
    channel->type.fragment = FS_SURFACE;

    channel->base.bo = to_intel_bo (s->intel.drm.bo);
    channel->base.content = CAIRO_CONTENT_ALPHA;
    channel->base.format = CAIRO_FORMAT_A8;
    channel->base.width  = s->intel.drm.width;
    channel->base.height = s->intel.drm.height;
    channel->base.stride = s->intel.drm.stride;

    channel->base.extend = i965_extend (CAIRO_EXTEND_NONE);
    channel->base.filter = i965_filter (CAIRO_FILTER_NEAREST);

    cairo_matrix_init_scale (&shader->clip.base.matrix,
			     1. / s->intel.drm.width,
			     1. / s->intel.drm.height);

    cairo_matrix_translate (&shader->clip.base.matrix,
			    -clip_x, -clip_y);
}

static cairo_bool_t
i965_shader_check_aperture (i965_shader_t *shader,
			    i965_device_t *device)
{
    uint32_t size = device->exec.gtt_size;

    if (shader->target != device->target) {
	const intel_bo_t *bo = to_intel_bo (shader->target->intel.drm.bo);
	if (bo->exec == NULL)
	    size += bo->base.size;
    }

    if (shader->source.base.bo != NULL && shader->source.base.bo != device->source) {
	const intel_bo_t *bo = to_intel_bo (shader->target->intel.drm.bo);
	if (bo->exec == NULL)
	    size += bo->base.size;
    }

    if (shader->mask.base.bo != NULL && shader->mask.base.bo != device->mask) {
	const intel_bo_t *bo = to_intel_bo (shader->target->intel.drm.bo);
	if (bo->exec == NULL)
	    size += bo->base.size;
    }

    if (shader->clip.base.bo != NULL && shader->clip.base.bo != device->clip) {
	const intel_bo_t *bo = to_intel_bo (shader->target->intel.drm.bo);
	if (bo->exec == NULL)
	    size += bo->base.size;
    }

    return size <= device->intel.gtt_avail_size;
}

static cairo_status_t
i965_shader_setup_dst (i965_shader_t *shader)
{
    union i965_shader_channel *channel;
    i965_surface_t *s, *clone;

    /* We need to manual blending if we have a clip surface and an unbounded op,
     * or an extended blend mode.
     */
    if (shader->need_combine ||
	(shader->op < CAIRO_OPERATOR_SATURATE &&
	 (shader->clip.type.fragment == FS_NONE ||
	  _cairo_operator_bounded_by_mask (shader->op))))
    {
	return CAIRO_STATUS_SUCCESS;
    }

    shader->need_combine = TRUE;

    s = shader->target;

    /* we need to allocate a new render target and use the original as a source */
    clone = (i965_surface_t *)
	i965_surface_create_internal ((cairo_drm_device_t *) s->intel.drm.base.device,
				      s->intel.drm.base.content,
				      s->intel.drm.width,
				      s->intel.drm.height,
				      I965_TILING_DEFAULT,
				      TRUE);
    if (unlikely (clone->intel.drm.base.status))
	return clone->intel.drm.base.status;

    if (to_intel_bo (s->intel.drm.bo)->batch_write_domain)
	i965_pipelined_flush (i965_device (s));

    channel = &shader->dst;

    channel->type.vertex = VS_NONE;
    channel->type.fragment = FS_SURFACE;
    channel->type.pattern = PATTERN_SURFACE;

    /* swap buffer objects */
    channel->base.bo = to_intel_bo (s->intel.drm.bo);
    s->intel.drm.bo = ((cairo_drm_surface_t *) clone)->bo;
    ((cairo_drm_surface_t *) clone)->bo = &channel->base.bo->base;

    channel->base.content = s->intel.drm.base.content;
    channel->base.format  = s->intel.drm.format;
    channel->base.width   = s->intel.drm.width;
    channel->base.height  = s->intel.drm.height;
    channel->base.stride  = s->intel.drm.stride;

    channel->base.filter = i965_filter (CAIRO_FILTER_NEAREST);
    channel->base.extend = i965_extend (CAIRO_EXTEND_NONE);

    cairo_matrix_init_scale (&channel->base.matrix,
			     1. / s->intel.drm.width,
			     1. / s->intel.drm.height);

    channel->surface.surface = &clone->intel.drm.base;

    s->intel.drm.base.content = clone->intel.drm.base.content;
    s->intel.drm.format = clone->intel.drm.format;
    assert (s->intel.drm.width == clone->intel.drm.width);
    assert (s->intel.drm.height == clone->intel.drm.height);
    s->intel.drm.stride = clone->intel.drm.stride;

    return CAIRO_STATUS_SUCCESS;
}

static inline void
constant_add_float (i965_shader_t *shader, float v)
{
    shader->constants[shader->constants_size++] = v;
}

static inline void
i965_shader_copy_channel_constants (i965_shader_t *shader,
				    const union i965_shader_channel *channel)
{
    if (channel->base.constants_size) {
	assert (shader->constants_size + channel->base.constants_size < ARRAY_LENGTH (shader->constants));

	memcpy (shader->constants + shader->constants_size,
		channel->base.constants,
		sizeof (float) * channel->base.constants_size);
	shader->constants_size += channel->base.constants_size;
    }
}

static void
i965_shader_setup_channel_constants (i965_shader_t *shader,
				     const union i965_shader_channel *channel)
{
    switch (channel->type.fragment) {
    case FS_NONE:
    case FS_CONSTANT:
	/* no plane equations */
	break;

    case FS_LINEAR:
	constant_add_float (shader, channel->base.matrix.xx);
	constant_add_float (shader, channel->base.matrix.xy);
	constant_add_float (shader, 0);
	constant_add_float (shader, channel->base.matrix.x0);
	break;

    case FS_RADIAL:
    case FS_SURFACE:
	constant_add_float (shader, channel->base.matrix.xx);
	constant_add_float (shader, channel->base.matrix.xy);
	constant_add_float (shader, 0);
	constant_add_float (shader, channel->base.matrix.x0);

	constant_add_float (shader, channel->base.matrix.yx);
	constant_add_float (shader, channel->base.matrix.yy);
	constant_add_float (shader, 0);
	constant_add_float (shader, channel->base.matrix.y0);
	break;

    case FS_SPANS:
    case FS_GLYPHS:
	/* use pue from SF */
	break;
    }

    i965_shader_copy_channel_constants (shader, channel);
}

static void
i965_shader_setup_constants (i965_shader_t *shader)
{
    i965_shader_setup_channel_constants (shader, &shader->source);
    i965_shader_setup_channel_constants (shader, &shader->mask);
    i965_shader_setup_channel_constants (shader, &shader->clip);
    i965_shader_setup_channel_constants (shader, &shader->dst);
    assert (shader->constants_size < ARRAY_LENGTH (shader->constants));
}

/*
 * Highest-valued BLENDFACTOR used in i965_blend_op.
 *
 * This leaves out BRW_BLENDFACTOR_INV_DST_COLOR,
 * BRW_BLENDFACTOR_INV_CONST_{COLOR,ALPHA},
 * BRW_BLENDFACTOR_INV_SRC1_{COLOR,ALPHA}
 */
#define BRW_BLENDFACTOR_COUNT (BRW_BLENDFACTOR_INV_DST_ALPHA + 1)

static void
i965_shader_get_blend_cntl (const i965_shader_t *shader,
			    uint32_t *sblend, uint32_t *dblend)
{
    static const struct blendinfo {
	cairo_bool_t dst_alpha;
	cairo_bool_t src_alpha;
	uint32_t src_blend;
	uint32_t dst_blend;
    } i965_blend_op[] = {
	/* CAIRO_OPERATOR_CLEAR treat as SOURCE with transparent */
	{0, 0, BRW_BLENDFACTOR_ONE,          BRW_BLENDFACTOR_ZERO},
	/* CAIRO_OPERATOR_SOURCE */
	{0, 0, BRW_BLENDFACTOR_ONE,           BRW_BLENDFACTOR_ZERO},
	/* CAIRO_OPERATOR_OVER */
	{0, 1, BRW_BLENDFACTOR_ONE,           BRW_BLENDFACTOR_INV_SRC_ALPHA},
	/* CAIRO_OPERATOR_IN */
	{1, 0, BRW_BLENDFACTOR_DST_ALPHA,     BRW_BLENDFACTOR_ZERO},
	/* CAIRO_OPERATOR_OUT */
	{1, 0, BRW_BLENDFACTOR_INV_DST_ALPHA, BRW_BLENDFACTOR_ZERO},
	/* CAIRO_OPERATOR_ATOP */
	{1, 1, BRW_BLENDFACTOR_DST_ALPHA,     BRW_BLENDFACTOR_INV_SRC_ALPHA},

	/* CAIRO_OPERATOR_DEST */
	{0, 0, BRW_BLENDFACTOR_ZERO,          BRW_BLENDFACTOR_ONE},
	/* CAIRO_OPERATOR_DEST_OVER */
	{1, 0, BRW_BLENDFACTOR_INV_DST_ALPHA, BRW_BLENDFACTOR_ONE},
	/* CAIRO_OPERATOR_DEST_IN */
	{0, 1, BRW_BLENDFACTOR_ZERO,          BRW_BLENDFACTOR_SRC_ALPHA},
	/* CAIRO_OPERATOR_DEST_OUT */
	{0, 1, BRW_BLENDFACTOR_ZERO,          BRW_BLENDFACTOR_INV_SRC_ALPHA},
	/* CAIRO_OPERATOR_DEST_ATOP */
	{1, 1, BRW_BLENDFACTOR_INV_DST_ALPHA, BRW_BLENDFACTOR_SRC_ALPHA},
	/* CAIRO_OPERATOR_XOR */
	{1, 1, BRW_BLENDFACTOR_INV_DST_ALPHA, BRW_BLENDFACTOR_INV_SRC_ALPHA},
	/* CAIRO_OPERATOR_ADD */
	{0, 0, BRW_BLENDFACTOR_ONE,           BRW_BLENDFACTOR_ONE},
    };
    const struct blendinfo *op = &i965_blend_op[shader->op];

    *sblend = op->src_blend;
    *dblend = op->dst_blend;

    /* If there's no dst alpha channel, adjust the blend op so that we'll treat
     * it as always 1.
     */
    if (shader->target->intel.drm.base.content == CAIRO_CONTENT_COLOR &&
	op->dst_alpha)
    {
	if (*sblend == BRW_BLENDFACTOR_DST_ALPHA)
	    *sblend = BRW_BLENDFACTOR_ONE;
	else if (*sblend == BRW_BLENDFACTOR_INV_DST_ALPHA)
	    *sblend = BRW_BLENDFACTOR_ZERO;
    }
}

static void
emit_wm_subpans_to_pixels (struct brw_compile *compile,
			   int tmp)
{
    /* Inputs:
     * R1.5 x/y of upper-left pixel of subspan 3
     * R1.4 x/y of upper-left pixel of subspan 2
     * R1.3 x/y of upper-left pixel of subspan 1
     * R1.2 x/y of upper-left pixel of subspan 0
     *
     * Outputs:
     * M1,2: u
     * M3,4: v
     *
     * upper left, upper right, lower left, lower right.
     */

    /* compute pixel locations for each subspan */
    brw_set_compression_control (compile, BRW_COMPRESSION_NONE);
    brw_ADD (compile,
	     brw_vec8_grf (tmp),
	     brw_reg (BRW_GENERAL_REGISTER_FILE, 1, 4,
		      BRW_REGISTER_TYPE_UW,
		      BRW_VERTICAL_STRIDE_2,
		      BRW_WIDTH_4,
		      BRW_HORIZONTAL_STRIDE_0,
		      BRW_SWIZZLE_NOOP,
		      WRITEMASK_XYZW),
	     brw_imm_vf4 (VF_ZERO, VF_ONE, VF_ZERO, VF_ONE));
    brw_ADD (compile,
	     brw_vec8_grf (tmp+1),
	     brw_reg (BRW_GENERAL_REGISTER_FILE, 1, 8,
		      BRW_REGISTER_TYPE_UW,
		      BRW_VERTICAL_STRIDE_2,
		      BRW_WIDTH_4,
		      BRW_HORIZONTAL_STRIDE_0,
		      BRW_SWIZZLE_NOOP,
		      WRITEMASK_XYZW),
	     brw_imm_vf4 (VF_ZERO, VF_ONE, VF_ZERO, VF_ONE));
    brw_ADD (compile,
	     brw_vec8_grf (tmp+2),
	     brw_reg (BRW_GENERAL_REGISTER_FILE, 1, 5,
		      BRW_REGISTER_TYPE_UW,
		      BRW_VERTICAL_STRIDE_2,
		      BRW_WIDTH_4,
		      BRW_HORIZONTAL_STRIDE_0,
		      BRW_SWIZZLE_NOOP,
		      WRITEMASK_XYZW),
	     brw_imm_vf4 (VF_ZERO, VF_ZERO, VF_ONE, VF_ONE));
    brw_ADD (compile,
	     brw_vec8_grf (tmp+3),
	     brw_reg (BRW_GENERAL_REGISTER_FILE, 1, 9,
		      BRW_REGISTER_TYPE_UW,
		      BRW_VERTICAL_STRIDE_2,
		      BRW_WIDTH_4,
		      BRW_HORIZONTAL_STRIDE_0,
		      BRW_SWIZZLE_NOOP,
		      WRITEMASK_XYZW),
	     brw_imm_vf4 (VF_ZERO, VF_ZERO, VF_ONE, VF_ONE));
    brw_set_compression_control (compile, BRW_COMPRESSION_COMPRESSED);
}

static void
emit_wm_affine (struct brw_compile *compile,
		int tmp, int reg, int msg)
{
    emit_wm_subpans_to_pixels (compile, tmp);

    brw_LINE (compile,
	      brw_null_reg (),
	      brw_vec1_grf (reg, 0),
	      brw_vec8_grf (tmp));
    brw_MAC (compile,
	     brw_message_reg (msg + 1),
	     brw_vec1_grf (reg, 1),
	     brw_vec8_grf (tmp+2));

    brw_LINE (compile,
	      brw_null_reg (),
	      brw_vec1_grf (reg, 4),
	      brw_vec8_grf (tmp));
    brw_MAC (compile,
	     brw_message_reg (msg + 3),
	     brw_vec1_grf (reg, 5),
	     brw_vec8_grf (tmp+2));
}

static void
emit_wm_glyph (struct brw_compile *compile,
	       int tmp, int vue, int msg)
{
    emit_wm_subpans_to_pixels (compile, tmp);

    brw_MUL (compile,
	     brw_null_reg (),
	     brw_vec8_grf (tmp),
	     brw_imm_f (1./1024));
    brw_ADD (compile,
	     brw_message_reg (msg + 1),
	     brw_acc_reg (),
	     brw_vec1_grf (vue, 0));

    brw_MUL (compile,
	     brw_null_reg (),
	     brw_vec8_grf (tmp + 2),
	     brw_imm_f (1./1024));
    brw_ADD (compile,
	     brw_message_reg (msg + 3),
	     brw_acc_reg (),
	     brw_vec1_grf (vue, 1));
}

static void
emit_wm_load_constant (struct brw_compile *compile,
		       int reg,
		       struct brw_reg *result)
{
    int n;

    for (n = 0; n < 4; n++) {
	result[n] = result[n+4] = brw_reg (BRW_GENERAL_REGISTER_FILE, reg, n,
					   BRW_REGISTER_TYPE_F,
					   BRW_VERTICAL_STRIDE_0,
					   BRW_WIDTH_1,
					   BRW_HORIZONTAL_STRIDE_0,
					   BRW_SWIZZLE_XXXX,
					   WRITEMASK_XYZW);
    }
}

static void
emit_wm_load_opacity (struct brw_compile *compile,
		      int reg,
		      struct brw_reg *result)
{
    result[0] = result[1] = result[2] = result[3] =
	result[4] = result[5] = result[6] = result[7] =
	brw_reg (BRW_GENERAL_REGISTER_FILE, reg, 0,
		 BRW_REGISTER_TYPE_F,
		 BRW_VERTICAL_STRIDE_0,
		 BRW_WIDTH_1,
		 BRW_HORIZONTAL_STRIDE_1,
		 BRW_SWIZZLE_XXXX,
		 WRITEMASK_XYZW);
}

static void
emit_wm_load_linear (struct brw_compile *compile,
		     int tmp, int reg, int msg)
{
    emit_wm_subpans_to_pixels (compile, tmp);

    brw_LINE (compile,
	      brw_null_reg(),
	      brw_vec1_grf (reg, 0),
	      brw_vec8_grf (tmp));
    brw_MAC (compile,
	     brw_message_reg(msg + 1),
	     brw_vec1_grf (reg, 1),
	     brw_vec8_grf (tmp + 2));
}

static void
emit_wm_load_radial (struct brw_compile *compile,
		     int reg, int msg)

{
    struct brw_reg c1x = brw_vec1_grf (reg, 0);
    struct brw_reg c1y = brw_vec1_grf (reg, 1);
    struct brw_reg minus_r_sq = brw_vec1_grf (reg, 3);
    struct brw_reg cdx = brw_vec1_grf (reg, 4);
    struct brw_reg cdy = brw_vec1_grf (reg, 5);
    struct brw_reg neg_4a = brw_vec1_grf (reg + 1, 0);
    struct brw_reg inv_2a = brw_vec1_grf (reg + 1, 1);

    struct brw_reg tmp_x = brw_uw16_grf (30, 0);
    struct brw_reg tmp_y = brw_uw16_grf (28, 0);
    struct brw_reg det = brw_vec8_grf (22);
    struct brw_reg b = brw_vec8_grf (20);
    struct brw_reg c = brw_vec8_grf (18);
    struct brw_reg pdx = brw_vec8_grf (16);
    struct brw_reg pdy = brw_vec8_grf (14);
    struct brw_reg t = brw_message_reg (msg + 1);

    /* cdx = (c₂x - c₁x)
     * cdy = (c₂y - c₁y)
     *  dr =  r₂-r₁
     * pdx =  px - c₁x
     * pdy =  py - c₁y
     *
     * A = cdx² + cdy² - dr²
     * B = -2·(pdx·cdx + pdy·cdy + r₁·dr)
     * C = pdx² + pdy² - r₁²
     *
     * t = (-2·B ± ⎷(B² - 4·A·C)) / 2·A
     */

    brw_ADD (compile, pdx, vec8 (tmp_x), negate (c1x));
    brw_ADD (compile, pdy, vec8 (tmp_y), negate (c1y));

    brw_LINE (compile, brw_null_reg (), cdx, pdx);
    brw_MAC (compile, b, cdy, pdy);

    brw_MUL (compile, brw_null_reg (), pdx, pdx);
    brw_MAC (compile, c, pdy, pdy);
    brw_ADD (compile, c, c, minus_r_sq);

    brw_MUL (compile, brw_null_reg (), b, b);
    brw_MAC (compile, det, neg_4a, c);

    /* XXX use rsqrt like i915?, it's faster and we need to mac anyway */
    brw_math (compile,
	      det,
	      BRW_MATH_FUNCTION_SQRT,
	      BRW_MATH_SATURATE_NONE,
	      2,
	      det,
	      BRW_MATH_DATA_VECTOR,
	      BRW_MATH_PRECISION_FULL);

    /* XXX cmp, +- */

    brw_ADD (compile, det, negate (det), negate (b));
    brw_ADD (compile, det, det, negate (b));
    brw_MUL (compile, t, det, inv_2a);
}

static int
emit_wm_sample (struct brw_compile *compile,
		union i965_shader_channel *channel,
		int sampler,
		int msg_base, int msg_len,
		int dst,
		struct brw_reg *result)
{
    int response_len, mask;

    if (channel->base.content == CAIRO_CONTENT_ALPHA) {
	mask = 0x7000;
	response_len = 2;
	result[0] = result[1] = result[2] = result[3] = brw_vec8_grf (dst);
	result[4] = result[5] = result[6] = result[7] = brw_vec8_grf (dst + 1);
    } else {
	mask = 0;
	response_len = 8;
	result[0] = brw_vec8_grf (dst + 0);
	result[1] = brw_vec8_grf (dst + 2);
	result[2] = brw_vec8_grf (dst + 4);
	result[3] = brw_vec8_grf (dst + 6);
	result[4] = brw_vec8_grf (dst + 1);
	result[5] = brw_vec8_grf (dst + 3);
	result[6] = brw_vec8_grf (dst + 5);
	result[7] = brw_vec8_grf (dst + 7);
    }

    brw_set_compression_control (compile, BRW_COMPRESSION_NONE);

    brw_set_mask_control (compile, BRW_MASK_DISABLE);
    brw_MOV (compile,
	     get_element_ud (brw_vec8_grf (0), 2),
	     brw_imm_ud (mask));
    brw_set_mask_control (compile, BRW_MASK_ENABLE);

    brw_SAMPLE (compile,
		brw_uw16_grf (dst, 0),
		msg_base,
		brw_uw8_grf (0, 0),
		sampler + 1, /* binding table */
		sampler,
		WRITEMASK_XYZW,
		BRW_SAMPLER_MESSAGE_SIMD16_SAMPLE,
		response_len,
		msg_len,
		0 /* eot */);

    brw_set_compression_control (compile, BRW_COMPRESSION_COMPRESSED);

    return response_len;
}

#define MAX_MSG_REGISTER 16

static void
emit_wm_load_channel (struct brw_compile *compile,
		      union i965_shader_channel *channel,
		      int *vue,
		      int *cue,
		      int *msg,
		      int *sampler,
		      int *grf,
		      struct brw_reg *result)
{
    switch (channel->type.fragment) {
    case FS_NONE:
	break;

    case FS_CONSTANT:
	emit_wm_load_constant (compile, *cue, result);
	*cue += 1;
	break;

    case FS_RADIAL:
	emit_wm_load_radial (compile, *cue, *msg);
	*cue += 2;

	if (*msg + 3 > MAX_MSG_REGISTER)
	    *msg = 1;

	*grf += emit_wm_sample (compile, channel, *sampler, *msg, 3, *grf, result);
	*sampler += 1;
	*msg += 3;
	break;

    case FS_LINEAR:
	emit_wm_load_linear (compile, *grf, *cue, *msg);
	*cue += 1;

	if (*msg + 3 > MAX_MSG_REGISTER)
	    *msg = 1;

	*grf += emit_wm_sample (compile, channel, *sampler, *msg, 3, *grf, result);
	*sampler += 1;
	*msg += 3;
	break;

    case FS_SURFACE:
	emit_wm_affine (compile, *grf, *cue, *msg);
	*cue += 2;

	if (*msg + 5 > MAX_MSG_REGISTER)
	    *msg = 1;

	*grf += emit_wm_sample (compile, channel, *sampler, *msg, 5, *grf, result);
	*sampler += 1;
	*msg += 5;
	break;

    case FS_SPANS:
	emit_wm_load_opacity (compile, *vue, result);
	*vue += 1;
	break;

    case FS_GLYPHS:
	emit_wm_glyph (compile, *grf, *vue, *msg);
	*vue += 1;

	if (*msg + 5 > MAX_MSG_REGISTER)
	    *msg = 1;

	*grf += emit_wm_sample (compile, channel, *sampler, *msg, 5, *grf, result);
	*sampler += 1;
	*msg += 5;
	break;
    }
}

static unsigned long
i965_wm_kernel_hash (const i965_shader_t *shader)
{
    unsigned long hash;

    hash =
	(shader->source.type.fragment & 0xff) |
	(shader->mask.type.fragment & 0xff) << 8 |
	(shader->clip.type.fragment & 0xff) << 16;
    if (shader->need_combine)
	hash |= (1u + shader->op) << 24;

    return hash;
}

static void
i965_wm_kernel_init (struct i965_wm_kernel *key,
		     const i965_shader_t *shader)
{
    key->entry.hash = i965_wm_kernel_hash (shader);
}

static uint32_t
i965_shader_const_urb_length (i965_shader_t *shader)
{
    const int lengths[] = { 0, 1, 1, 4, 2, 0, 0 };
    int count = 0; /* 128-bit/16-byte increments */

    count += lengths[shader->source.type.fragment];
    count += lengths[shader->mask.type.fragment];
    count += lengths[shader->clip.type.fragment];
    count += lengths[shader->dst.type.fragment];

    return (count + 1) / 2; /* 256-bit/32-byte increments */
}

static uint32_t
i965_shader_pue_length (i965_shader_t *shader)
{
    return 1 + (shader->mask.type.vertex != VS_NONE);
}

static uint32_t
create_wm_kernel (i965_device_t *device,
		  i965_shader_t *shader,
		  int *num_reg)
{
    struct brw_compile compile;
    struct brw_reg source[8], mask[8], clip[8], dst[8];
    const uint32_t *program;
    uint32_t size;
    int msg, cue, vue, grf, sampler;
    int i;

    struct i965_wm_kernel key, *cache;
    cairo_status_t status;
    uint32_t offset;

    i965_wm_kernel_init (&key, shader);
    cache = _cairo_hash_table_lookup (device->wm_kernels, &key.entry);
    if (cache != NULL)
	return cache->offset;

    brw_compile_init (&compile, device->is_g4x);

    if (key.entry.hash == FS_CONSTANT &&
	to_intel_bo (shader->target->intel.drm.bo)->tiling)
    {
	struct brw_instruction *insn;

	assert (i965_shader_const_urb_length (shader) == 1);
	brw_MOV (&compile, brw_message4_reg (2), brw_vec4_grf (2, 0));
	grf = 3;

	brw_push_insn_state (&compile);
	brw_set_mask_control (&compile, BRW_MASK_DISABLE); /* ? */
	brw_MOV (&compile,
		 retype (brw_message_reg (1), BRW_REGISTER_TYPE_UD),
		 retype (brw_vec8_grf (1), BRW_REGISTER_TYPE_UD));
	brw_pop_insn_state (&compile);

	insn = brw_next_instruction (&compile, BRW_OPCODE_SEND);
	insn->header.predicate_control = 0;
	insn->header.compression_control = BRW_COMPRESSION_NONE;
	insn->header.destreg__conditonalmod = 0;

	brw_instruction_set_destination (insn,
					 retype (vec16 (brw_acc_reg ()),
						 BRW_REGISTER_TYPE_UW));

	brw_instruction_set_source0 (insn,
				     retype (brw_vec8_grf (0),
					     BRW_REGISTER_TYPE_UW));

	brw_instruction_set_dp_write_message (insn,
					      0,
					      BRW_DATAPORT_RENDER_TARGET_WRITE_SIMD16_SINGLE_SOURCE_REPLICATED, /* msg_control */
					      BRW_DATAPORT_WRITE_MESSAGE_RENDER_TARGET_WRITE, /* msg_type */
					      3,
					      1,	/* pixel scoreboard */
					      0,
					      TRUE);
    }
    else
    {
	msg = 1;
	cue = 2;
	vue = cue + i965_shader_const_urb_length (shader);
	grf = vue + i965_shader_pue_length (shader);
	sampler = 0;

	brw_set_compression_control (&compile, BRW_COMPRESSION_COMPRESSED);
	emit_wm_load_channel (&compile, &shader->source,
			      &vue, &cue, &msg, &sampler, &grf,
			      source);
	emit_wm_load_channel (&compile, &shader->mask,
			      &vue, &cue, &msg, &sampler, &grf,
			      mask);
	emit_wm_load_channel (&compile, &shader->clip,
			      &vue, &cue, &msg, &sampler, &grf,
			      clip);
	emit_wm_load_channel (&compile, &shader->dst,
			      &vue, &cue, &msg, &sampler, &grf,
			      dst);
	brw_set_compression_control (&compile, BRW_COMPRESSION_NONE);

	if (shader->need_combine) {
	    if (shader->mask.type.fragment != FS_NONE &&
		shader->clip.type.fragment != FS_NONE)
	    {
		for (i = 0; i < 8; i++)
		    brw_MUL (&compile, mask[i], mask[i], clip[i]);
	    }

	    /* XXX LERP ! */
	    for (i = 0; i < 8; i++)
		brw_MOV (&compile, brw_message_reg (2 + i), source[i]);
	} else {
	    if (shader->mask.type.fragment != FS_NONE) {
		if (shader->clip.type.fragment != FS_NONE) {
		    for (i = 0; i < 8; i++)
			brw_MUL (&compile, mask[i], mask[i], clip[i]);
		}

		for (i = 0; i < 8; i++)
		    brw_MUL (&compile, brw_message_reg (2 + i), source[i], mask[i]);
	    } else {
		if (shader->clip.type.fragment != FS_NONE) {
		    for (i = 0; i < 8; i++)
			brw_MUL (&compile, brw_message_reg (2 + i), source[i], clip[i]);
		} else {
		    for (i = 0; i < 8; i++)
			brw_MOV (&compile, brw_message_reg (2 + i), source[i]);
		}
	    }
	}

	brw_push_insn_state (&compile);
	brw_set_mask_control (&compile, BRW_MASK_DISABLE); /* ? */
	brw_MOV (&compile,
		 retype (brw_message_reg (1), BRW_REGISTER_TYPE_UD),
		 retype (brw_vec8_grf (1), BRW_REGISTER_TYPE_UD));
	brw_pop_insn_state (&compile);

	brw_fb_WRITE (&compile,
		      retype (vec16 (brw_acc_reg ()), BRW_REGISTER_TYPE_UW),
		      0,		/* base reg */
		      retype (brw_vec8_grf (0), BRW_REGISTER_TYPE_UW),
		      0,		/* binding table index */
		      2 + 8,	/* msg length */
		      0,		/* response length */
		      TRUE);	/* EOT */
    }

    program = brw_get_program (&compile, &size);
    *num_reg = grf;

    i965_stream_align (&device->general, 64);
    offset = i965_stream_emit (&device->general, program, size);

    cache = _cairo_freelist_alloc (&device->wm_kernel_freelist);
    if (likely (cache != NULL)) {
	i965_wm_kernel_init (cache, shader);
	cache->offset = offset;
	status = _cairo_hash_table_insert (device->wm_kernels, &cache->entry);
	if (unlikely (status))
	    _cairo_freelist_free (&device->wm_kernel_freelist, cache);
    }

    return offset;
}

static uint32_t
create_sf_kernel (i965_device_t *device,
		  i965_shader_t *shader)
{
    struct brw_compile compile;
    const uint32_t *program;
    uint32_t size;
    int msg_len;

    brw_compile_init (&compile, device->is_g4x);

    switch (shader->mask.type.vertex) {
    default:
    case VS_NONE:
	/* use curb plane eq in WM */
	msg_len = 1;
	break;

    case VS_SPANS:
	/* just a constant opacity */
	brw_MOV (&compile,
		 brw_message4_reg (1),
		 brw_vec4_grf (3, 0));
	msg_len = 2;
	break;

    case VS_GLYPHS:
	/* an offset+sf into the glyph cache */
	brw_MOV (&compile,
		 brw_acc_reg (),
		 brw_vec2_grf (3, 0));
	brw_MAC (&compile,
		 brw_message4_reg (1),
		 negate (brw_vec2_grf (1, 4)),
		 brw_imm_f (1./1024));
	msg_len = 2;
	break;
    }

    brw_urb_WRITE (&compile,
		   brw_null_reg (),
		   0,
		   brw_vec8_grf (0), /* r0, will be copied to m0 */
		   0,	/* allocate */
		   1,	/* used */
		   msg_len,
		   0,	/* response len */
		   1,	/* eot */
		   1,	/* writes complete */
		   0,	/* offset */
		   BRW_URB_SWIZZLE_NONE);

    program = brw_get_program (&compile, &size);

    i965_stream_align (&device->general, 64);
    return i965_stream_emit (&device->general, program, size);
}

static uint32_t
i965_sf_kernel (const i965_shader_t *shader)
{
    return shader->mask.type.vertex;
}

static void
i965_sf_state_init (struct i965_sf_state *key,
		    const i965_shader_t *shader)
{
    key->entry.hash = i965_sf_kernel (shader);
}

cairo_bool_t
i965_sf_state_equal (const void *A, const void *B)
{
    const cairo_hash_entry_t *a = A, *b = B;
    return a->hash == b->hash;
}

/*
 * Sets up the SF state pointing at an SF kernel.
 *
 * The SF kernel does coord interp: for each attribute,
 * calculate dA/dx and dA/dy.  Hand these interpolation coefficients
 * back to SF which then hands pixels off to WM.
 */
static uint32_t
gen4_create_sf_state (i965_device_t *device,
		      i965_shader_t *shader)
{
    struct brw_sf_unit_state *state;
    struct i965_sf_state key, *cache;
    cairo_status_t status;
    uint32_t offset;

    i965_sf_state_init (&key, shader);
    if (i965_sf_state_equal (&key, &device->sf_state))
	return device->sf_state.offset;

    cache = _cairo_hash_table_lookup (device->sf_states, &key.entry);
    if (cache != NULL) {
	offset = cache->offset;
	goto DONE;
    }

    offset = create_sf_kernel (device, shader);

    state = i965_stream_alloc (&device->general, 32, sizeof (*state));
    memset (state, 0, sizeof (*state));

    state->thread0.grf_reg_count = BRW_GRF_BLOCKS (3);
    assert ((offset & 63) == 0);
    state->thread0.kernel_start_pointer = offset >> 6;
    state->sf1.single_program_flow = 1;
    state->thread3.urb_entry_read_length = 1; /* 1 URB per vertex */
    state->thread3.urb_entry_read_offset = 1;
    state->thread3.dispatch_grf_start_reg = 3;
    state->thread4.max_threads = SF_MAX_THREADS - 1;
    state->thread4.urb_entry_allocation_size = URB_SF_ENTRY_SIZE - 1;
    state->thread4.nr_urb_entries = URB_SF_ENTRIES;
    state->sf6.dest_org_vbias = 0x8;
    state->sf6.dest_org_hbias = 0x8;

    offset = i965_stream_offsetof (&device->general, state);

    cache = _cairo_freelist_alloc (&device->sf_freelist);
    if (likely (cache != NULL)) {
	i965_sf_state_init (cache, shader);
	cache->offset = offset;
	status = _cairo_hash_table_insert (device->sf_states, &cache->entry);
	if (unlikely (status))
	    _cairo_freelist_free (&device->sf_freelist, cache);
    }

  DONE:
    i965_sf_state_init (&device->sf_state, shader);
    device->sf_state.offset = offset;

    return offset;
}

static unsigned long
i965_shader_sampler_hash (const i965_shader_t *shader)
{
    unsigned long hash = 0;
    unsigned int offset = 0;

    if (shader->source.base.bo != NULL) {
	hash |= (shader->source.base.filter << offset) |
	        (shader->source.base.extend << (offset + 4));
	offset += 8;
    }

    if (shader->mask.base.bo != NULL) {
	hash |= (shader->mask.base.filter << offset) |
	        (shader->mask.base.extend << (offset + 4));
	offset += 8;
    }

    if (shader->clip.base.bo != NULL) {
	hash |= (shader->clip.base.filter << offset) |
	        (shader->clip.base.extend << (offset + 4));
	offset += 8;
    }

    if (shader->dst.base.bo != NULL) {
	hash |= (shader->dst.base.filter << offset) |
	        (shader->dst.base.extend << (offset + 4));
	offset += 8;
    }

    return hash;
}

static void
i965_sampler_init (struct i965_sampler *key,
		   const i965_shader_t *shader)
{
    key->entry.hash = i965_shader_sampler_hash (shader);
}

static void
emit_sampler_channel (i965_device_t *device,
		      const union i965_shader_channel *channel,
		      uint32_t border_color)
{
    struct brw_sampler_state *state;

    state = i965_stream_alloc (&device->general, 0, sizeof (*state));
    memset (state, 0, sizeof (*state));

    state->ss0.lod_preclamp = 1; /* GL mode */

    state->ss0.border_color_mode = BRW_BORDER_COLOR_MODE_LEGACY;

    state->ss0.min_filter = channel->base.filter;
    state->ss0.mag_filter = channel->base.filter;

    state->ss1.r_wrap_mode = channel->base.extend;
    state->ss1.s_wrap_mode = channel->base.extend;
    state->ss1.t_wrap_mode = channel->base.extend;

    assert ((border_color & 31) == 0);
    state->ss2.border_color_pointer = border_color >> 5;
}

static uint32_t
emit_sampler_state_table (i965_device_t *device,
			  i965_shader_t *shader)
{
    struct i965_sampler key, *cache;
    cairo_status_t status;
    uint32_t offset;

    if (device->border_color_offset == (uint32_t) -1) {
	struct brw_sampler_legacy_border_color *border_color;

	border_color = i965_stream_alloc (&device->general, 32,
					  sizeof (*border_color));
	border_color->color[0] = 0; /* R */
	border_color->color[1] = 0; /* G */
	border_color->color[2] = 0; /* B */
	border_color->color[3] = 0; /* A */

	device->border_color_offset = i965_stream_offsetof (&device->general,
							    border_color);
    } else {
	i965_sampler_init (&key, shader);
	cache = _cairo_hash_table_lookup (device->samplers, &key.entry);
	if (cache != NULL)
	    return cache->offset;
    }

    i965_stream_align (&device->general, 32);
    offset = device->general.used;
    if (shader->source.base.bo != NULL) {
	emit_sampler_channel (device,
			      &shader->source,
			      device->border_color_offset);
    }
    if (shader->mask.base.bo != NULL) {
	emit_sampler_channel (device,
			      &shader->mask,
			      device->border_color_offset);
    }
    if (shader->clip.base.bo != NULL) {
	emit_sampler_channel (device,
			      &shader->clip,
			      device->border_color_offset);
    }
    if (shader->dst.base.bo != NULL) {
	emit_sampler_channel (device,
			      &shader->dst,
			      device->border_color_offset);
    }

    cache = _cairo_freelist_alloc (&device->sampler_freelist);
    if (likely (cache != NULL)) {
	i965_sampler_init (cache, shader);
	cache->offset = offset;
	status = _cairo_hash_table_insert (device->samplers, &cache->entry);
	if (unlikely (status))
	    _cairo_freelist_free (&device->sampler_freelist, cache);
    }

    return offset;
}

static void
i965_cc_state_init (struct i965_cc_state *key,
		    const i965_shader_t *shader)
{
    uint32_t src_blend, dst_blend;

    if (shader->need_combine)
	src_blend = dst_blend = 0;
    else
	i965_shader_get_blend_cntl (shader, &src_blend, &dst_blend);

    key->entry.hash = src_blend | ((dst_blend & 0xffff) << 16);
}

cairo_bool_t
i965_cc_state_equal (const void *A, const void *B)
{
    const cairo_hash_entry_t *a = A, *b = B;
    return a->hash == b->hash;
}

static uint32_t
cc_state_emit (i965_device_t *device, i965_shader_t *shader)
{
    struct brw_cc_unit_state *state;
    struct i965_cc_state key, *cache;
    cairo_status_t status;
    uint32_t src_blend, dst_blend;
    uint32_t offset;

    i965_cc_state_init (&key, shader);
    if (i965_cc_state_equal (&key, &device->cc_state))
	return device->cc_state.offset;

    cache = _cairo_hash_table_lookup (device->cc_states, &key.entry);
    if (cache != NULL) {
	offset = cache->offset;
	goto DONE;
    }

    if (shader->need_combine)
	src_blend = dst_blend = 0;
    else
	i965_shader_get_blend_cntl (shader, &src_blend, &dst_blend);

    state = i965_stream_alloc (&device->general, 64, sizeof (*state));
    memset (state, 0, sizeof (*state));

    /* XXX Note errata, need to flush render cache when blend_enable 0 -> 1 */
    /* XXX 2 source blend */
    state->cc3.blend_enable = ! shader->need_combine;
    state->cc5.ia_blend_function = BRW_BLENDFUNCTION_ADD;
    state->cc5.ia_src_blend_factor  = src_blend;
    state->cc5.ia_dest_blend_factor = dst_blend;
    state->cc6.blend_function = BRW_BLENDFUNCTION_ADD;
    state->cc6.clamp_post_alpha_blend = 1;
    state->cc6.clamp_pre_alpha_blend  = 1;
    state->cc6.src_blend_factor  = src_blend;
    state->cc6.dest_blend_factor = dst_blend;

    offset = i965_stream_offsetof (&device->general, state);

    cache = _cairo_freelist_alloc (&device->cc_freelist);
    if (likely (cache != NULL)) {
	i965_cc_state_init (cache, shader);
	cache->offset = offset;
	status = _cairo_hash_table_insert (device->cc_states, &cache->entry);
	if (unlikely (status))
	    _cairo_freelist_free (&device->cc_freelist, cache);
    }

  DONE:
    i965_cc_state_init (&device->cc_state, shader);
    device->cc_state.offset = offset;

    return offset;
}

static void
i965_wm_state_init (struct i965_wm_state *key,
		    const i965_shader_t *shader)
{
    key->kernel = i965_wm_kernel_hash (shader);
    key->sampler = i965_shader_sampler_hash (shader);

    key->entry.hash = key->kernel ^ ((key->sampler) << 16 | (key->sampler >> 16));
}

cairo_bool_t
i965_wm_state_equal (const void *A, const void *B)
{
    const struct i965_wm_state *a = A, *b = B;

    if (a->entry.hash != b->entry.hash)
	return FALSE;

    return a->kernel == b->kernel && a->sampler == b->sampler;
}

static int
i965_shader_binding_table_count (i965_shader_t *shader)
{
    int count;

    count = 1;
    if (shader->source.type.fragment != FS_CONSTANT)
	count++;
    switch (shader->mask.type.fragment) {
    case FS_NONE:
    case FS_CONSTANT:
    case FS_SPANS:
	break;
    case FS_LINEAR:
    case FS_RADIAL:
    case FS_SURFACE:
    case FS_GLYPHS:
	count++;
    }
    if (shader->clip.type.fragment == FS_SURFACE)
	count++;
    if (shader->dst.type.fragment == FS_SURFACE)
	count++;

    return count;
}

static uint32_t
gen4_create_wm_state (i965_device_t *device,
		      i965_shader_t *shader)
{
    struct brw_wm_unit_state *state;
    uint32_t sampler;
    uint32_t kernel;

    struct i965_wm_state key, *cache;
    cairo_status_t status;
    int num_reg;

    i965_wm_state_init (&key, shader);
    if (i965_wm_state_equal (&key, &device->wm_state))
	return device->wm_state.offset;

    cache = _cairo_hash_table_lookup (device->wm_states, &key.entry);
    if (cache != NULL) {
	device->wm_state = *cache;
	return cache->offset;
    }

    kernel = create_wm_kernel (device, shader, &num_reg);
    sampler = emit_sampler_state_table (device, shader);

    state = i965_stream_alloc (&device->general, 32, sizeof (*state));
    memset (state, 0, sizeof (*state));
    state->thread0.grf_reg_count = BRW_GRF_BLOCKS (num_reg);
    assert ((kernel & 63) == 0);
    state->thread0.kernel_start_pointer = kernel >> 6;

    state->thread3.dispatch_grf_start_reg = 2;

    state->wm4.sampler_count = 1; /* 1-4 samplers used */
    assert ((sampler & 31) == 0);
    state->wm4.sampler_state_pointer = sampler >> 5;
    if (device->is_g4x)
	state->wm5.max_threads = PS_MAX_THREADS_CTG - 1;
    else
	state->wm5.max_threads = PS_MAX_THREADS_BRW - 1;
    state->wm5.thread_dispatch_enable = 1;

    if (device->is_g4x) {
	/* XXX contiguous 32 pixel dispatch */
    }
    state->wm5.enable_16_pix = 1;
    /* 8 pixel dispatch and friends */
    //state->wm5.early_depth_test = 1;

    state->thread1.binding_table_entry_count = i965_shader_binding_table_count(shader);
    state->thread3.urb_entry_read_length = i965_shader_pue_length (shader);
    state->thread3.const_urb_entry_read_length = i965_shader_const_urb_length (shader);

    key.offset = i965_stream_offsetof (&device->general, state);

    cache = _cairo_freelist_alloc (&device->wm_state_freelist);
    if (likely (cache != NULL)) {
	*cache = key;
	status = _cairo_hash_table_insert (device->wm_states, &cache->entry);
	if (unlikely (status))
	    _cairo_freelist_free (&device->wm_state_freelist, cache);
    }

    device->wm_state = key;
    return key.offset;
}

static uint32_t
vs_unit_state_emit (i965_device_t *device)
{
    if (device->vs_offset == (uint32_t) -1) {
	struct brw_vs_unit_state *state;

	/* Set up the vertex shader to be disabled (passthrough) */
	state = i965_stream_alloc (&device->general, 32, sizeof (*state));
	memset (state, 0, sizeof (*state));

	state->thread4.nr_urb_entries = URB_VS_ENTRIES;
	state->thread4.urb_entry_allocation_size = URB_VS_ENTRY_SIZE - 1;
	state->vs6.vert_cache_disable = 1;

	device->vs_offset = i965_stream_offsetof (&device->general, state);
    }

    return device->vs_offset;
}

static uint32_t
i965_get_card_format (cairo_format_t format)
{
    switch (format) {
    case CAIRO_FORMAT_ARGB32:
	return BRW_SURFACEFORMAT_B8G8R8A8_UNORM;
    case CAIRO_FORMAT_RGB24:
	return BRW_SURFACEFORMAT_B8G8R8X8_UNORM;
    case CAIRO_FORMAT_RGB16_565:
	return BRW_SURFACEFORMAT_B5G6R5_UNORM;
    case CAIRO_FORMAT_A8:
	return BRW_SURFACEFORMAT_A8_UNORM;
    case CAIRO_FORMAT_A1:
    case CAIRO_FORMAT_INVALID:
    default:
	ASSERT_NOT_REACHED;
	return 0;
    }
}

static uint32_t
i965_get_dest_format (cairo_format_t format)
{
    switch (format) {
    case CAIRO_FORMAT_ARGB32:
    case CAIRO_FORMAT_RGB24:
        return BRW_SURFACEFORMAT_B8G8R8A8_UNORM;
    case CAIRO_FORMAT_RGB16_565:
        return BRW_SURFACEFORMAT_B5G6R5_UNORM;
    case CAIRO_FORMAT_A8:
        return BRW_SURFACEFORMAT_A8_UNORM;
    case CAIRO_FORMAT_A1:
    case CAIRO_FORMAT_INVALID:
    default:
	ASSERT_NOT_REACHED;
	return 0;
    }
}

/* XXX silly inline due to compiler bug... */
static inline void
i965_stream_add_pending_relocation (i965_stream_t *stream,
				    uint32_t target_offset,
				    uint32_t read_domains,
				    uint32_t write_domain,
				    uint32_t delta)
{
    int n;

    n = stream->num_pending_relocations++;
    assert (n < stream->max_pending_relocations);

    stream->pending_relocations[n].offset = target_offset;
    stream->pending_relocations[n].read_domains = read_domains;
    stream->pending_relocations[n].write_domain = write_domain;
    stream->pending_relocations[n].delta = delta;
}

static uint32_t
emit_surface_state (i965_device_t *device,
		    cairo_bool_t is_target,
		    intel_bo_t *bo,
		    cairo_format_t format,
		    int width, int height, int stride,
		    int type)
{
    struct brw_surface_state *state;
    uint32_t write_domain, read_domains;
    uint32_t offset;

    state = i965_stream_alloc (&device->surface, 32, sizeof (*state));
    memset (state, 0, sizeof (*state));

    state->ss0.surface_type = type;
    if (is_target)
	state->ss0.surface_format = i965_get_dest_format (format);
    else
	state->ss0.surface_format = i965_get_card_format (format);

    state->ss0.data_return_format = BRW_SURFACERETURNFORMAT_FLOAT32;
    state->ss0.color_blend = 1;
    if (is_target && device->is_g4x)
	state->ss0.render_cache_read_mode = 1;

    state->ss1.base_addr = bo->offset;

    state->ss2.height = height - 1;
    state->ss2.width  = width  - 1;
    state->ss3.pitch  = stride - 1;
    state->ss3.tile_walk = bo->tiling == I915_TILING_Y;
    state->ss3.tiled_surface = bo->tiling != I915_TILING_NONE;

    if (is_target) {
	read_domains = I915_GEM_DOMAIN_RENDER;
	write_domain = I915_GEM_DOMAIN_RENDER;
    } else {
	read_domains = I915_GEM_DOMAIN_SAMPLER;
	write_domain = 0;
    }

    offset = i965_stream_offsetof (&device->surface, state);
    i965_emit_relocation (device, &device->surface,
			  bo, 0,
			  read_domains, write_domain,
			  offset + offsetof (struct brw_surface_state, ss1.base_addr));
    return offset;
}

static uint32_t
emit_surface_state_for_shader (i965_device_t *device,
			       const union i965_shader_channel *channel)
{
    int type = BRW_SURFACE_2D;

    assert (channel->type.fragment != FS_NONE);
    assert (channel->type.fragment != FS_CONSTANT);

    if (channel->type.fragment != FS_SURFACE)
	type = BRW_SURFACE_1D;

    return emit_surface_state (device, FALSE,
			       channel->base.bo,
			       channel->base.format,
			       channel->base.width,
			       channel->base.height,
			       channel->base.stride,
			       type);
}

cairo_bool_t
i965_wm_binding_equal (const void *A,
		       const void *B)
{
    const struct i965_wm_binding *a = A, *b = B;

    if (a->entry.hash != b->entry.hash)
	return FALSE;

    if (a->size != b->size)
	return FALSE;

    return memcmp (a->table, b->table, sizeof (uint32_t) * a->size) == 0;
}

static void
i965_wm_binding_init (struct i965_wm_binding *state,
		      const uint32_t *table,
		      int size)
{
    int n;

    state->entry.hash = size;
    state->size = size;

    for (n = 0; n < size; n++) {
	state->table[n] = table[n];
	state->entry.hash ^= (table[n] << (8 * n)) |
	                     (table[n] >> (32 - (8*n)));
    }
}

static uint32_t
emit_binding_table (i965_device_t *device,
		    i965_shader_t *shader)
{
    intel_bo_t *bo;
    struct i965_wm_binding key, *cache;
    uint32_t *table;
    int n = 0;

    table = i965_stream_alloc (&device->surface, 32, 5 * sizeof (uint32_t));
    if (shader->target->stream != device->surface.serial) {
	shader->target->stream = device->surface.serial;
	shader->target->offset = emit_surface_state (device,
						     TRUE,
						     to_intel_bo (shader->target->intel.drm.bo),
						     shader->target->intel.drm.format,
						     shader->target->intel.drm.width,
						     shader->target->intel.drm.height,
						     shader->target->intel.drm.stride,
						     BRW_SURFACE_2D);
    }
    table[n++] = shader->target->offset;

    bo = shader->source.base.bo;
    if (bo != NULL) {
	if (bo->opaque0 != device->surface.serial) {
	    bo->opaque0 = device->surface.serial;
	    bo->opaque1 = emit_surface_state_for_shader (device, &shader->source);
	}
	table[n++] = bo->opaque1;
    }

    bo = shader->mask.base.bo;
    if (bo != NULL) {
	if (bo->opaque0 != device->surface.serial) {
	    bo->opaque0 = device->surface.serial;
	    bo->opaque1 = emit_surface_state_for_shader (device, &shader->mask);
	}
	table[n++] = bo->opaque1;
    }

    bo = shader->clip.base.bo;
    if (bo != NULL) {
	if (bo->opaque0 != device->surface.serial) {
	    bo->opaque0 = device->surface.serial;
	    bo->opaque1 = emit_surface_state_for_shader (device, &shader->clip);
	}
	table[n++] = bo->opaque1;
    }

    bo = shader->dst.base.bo;
    if (bo != NULL) {
	if (bo->opaque0 != device->surface.serial) {
	    bo->opaque0 = device->surface.serial;
	    bo->opaque1 = emit_surface_state_for_shader (device, &shader->dst);
	}
	table[n++] = bo->opaque1;
    }

    i965_wm_binding_init (&key, table, n);
    key.offset = i965_stream_offsetof (&device->surface, table);

    if (i965_wm_binding_equal (&key, &device->wm_binding)) {
	device->surface.used = key.offset;
	return device->wm_binding.offset;
    }

    cache = _cairo_hash_table_lookup (device->wm_bindings, &key.entry);
    if (cache != NULL) {
	device->surface.used = key.offset;
	key.offset = cache->offset;
    }

    device->wm_binding = key;
    return key.offset;
}

static void
i965_emit_invariants (i965_device_t *device)
{
    OUT_BATCH (BRW_CS_URB_STATE | 0);
    OUT_BATCH (((URB_CS_ENTRY_SIZE-1) << 4) | (URB_CS_ENTRIES << 0));
}

static void
i965_emit_urb_fences (i965_device_t *device)
{
    int urb_vs_start, urb_vs_size;
    int urb_gs_start, urb_gs_size;
    int urb_clip_start, urb_clip_size;
    int urb_sf_start, urb_sf_size;
    int urb_cs_start, urb_cs_size;

    if (device->have_urb_fences)
	return;

    /* URB fence */
    urb_vs_start = 0;
    urb_vs_size = URB_VS_ENTRIES * URB_VS_ENTRY_SIZE;
    urb_gs_start = urb_vs_start + urb_vs_size;
    urb_gs_size = URB_GS_ENTRIES * URB_GS_ENTRY_SIZE;
    urb_clip_start = urb_gs_start + urb_gs_size;
    urb_clip_size = URB_CLIP_ENTRIES * URB_CLIP_ENTRY_SIZE;
    urb_sf_start = urb_clip_start + urb_clip_size;
    urb_sf_size = URB_SF_ENTRIES * URB_SF_ENTRY_SIZE;
    urb_cs_start = urb_sf_start + urb_sf_size;
    urb_cs_size = URB_CS_ENTRIES * URB_CS_ENTRY_SIZE;

    /* erratum: URB_FENCE must not cross a 64-byte cache-line */
    while ((device->batch.used & 63) > 64-12)
	OUT_BATCH (MI_NOOP);
    OUT_BATCH (BRW_URB_FENCE |
	       UF0_CS_REALLOC |
	       UF0_SF_REALLOC |
	       UF0_CLIP_REALLOC |
	       UF0_GS_REALLOC |
	       UF0_VS_REALLOC |
	       1);
    OUT_BATCH (((urb_clip_start + urb_clip_size) << UF1_CLIP_FENCE_SHIFT) |
	       ((urb_gs_start + urb_gs_size) << UF1_GS_FENCE_SHIFT) |
	       ((urb_vs_start + urb_vs_size) << UF1_VS_FENCE_SHIFT));
    OUT_BATCH (((urb_cs_start + urb_cs_size) << UF2_CS_FENCE_SHIFT) |
	       ((urb_sf_start + urb_sf_size) << UF2_SF_FENCE_SHIFT));

    device->have_urb_fences = TRUE;
    device->constants_size = 0;
}

static void
i965_emit_base (i965_device_t *device)
{
    OUT_BATCH (BRW_STATE_BASE_ADDRESS | 4);
    if (likely (device->general.num_pending_relocations == 0)) {
	i965_stream_add_pending_relocation (&device->general,
					    device->batch.used,
					    I915_GEM_DOMAIN_INSTRUCTION, 0,
					    BASE_ADDRESS_MODIFY);
    }
    OUT_BATCH (0); /* pending relocation */

    if (likely (device->surface.num_pending_relocations == 0)) {
	i965_stream_add_pending_relocation (&device->surface,
					    device->batch.used,
					    I915_GEM_DOMAIN_INSTRUCTION, 0,
					    BASE_ADDRESS_MODIFY);
    }
    OUT_BATCH (0); /* pending relocation */

    OUT_BATCH (0 | BASE_ADDRESS_MODIFY);
    /* general state max addr, disabled */
    OUT_BATCH (0x10000000 | BASE_ADDRESS_MODIFY);
    /* media object state max addr, disabled */
    OUT_BATCH (0x10000000 | BASE_ADDRESS_MODIFY);
}

static void
i965_emit_vertex_element (i965_device_t *device,
			  i965_shader_t *shader)
{
    uint32_t offset;
    uint32_t type;
    int nelem;

    type = 0;
    nelem = 1;
    if (shader->mask.type.vertex == VS_SPANS ||
	shader->mask.type.vertex == VS_GLYPHS)
    {
	type = shader->mask.type.vertex;
	nelem++;
    }

    if (type == device->vertex_type)
	return;
    device->vertex_type = type;

    offset = 0;

    OUT_BATCH (BRW_3DSTATE_VERTEX_ELEMENTS | ((2 * nelem) - 1));
    OUT_BATCH ((0 << VE0_VERTEX_BUFFER_INDEX_SHIFT) |
	       VE0_VALID |
	       (BRW_SURFACEFORMAT_R32G32_FLOAT	<< VE0_FORMAT_SHIFT) |
	       (offset				<< VE0_OFFSET_SHIFT));
    OUT_BATCH ((BRW_VFCOMPONENT_STORE_SRC	<< VE1_VFCOMPONENT_0_SHIFT) |
	       (BRW_VFCOMPONENT_STORE_SRC	<< VE1_VFCOMPONENT_1_SHIFT) |
	       (BRW_VFCOMPONENT_STORE_0		<< VE1_VFCOMPONENT_2_SHIFT) |
	       (BRW_VFCOMPONENT_STORE_1_FLT	<< VE1_VFCOMPONENT_3_SHIFT) |
	       (4 << VE1_DESTINATION_ELEMENT_OFFSET_SHIFT));
    offset += 8;

    assert (shader->source.type.vertex == VS_NONE);
    switch (shader->mask.type.vertex) {
    default:
    case VS_NONE:
	break;

    case VS_SPANS:
	OUT_BATCH((0 << VE0_VERTEX_BUFFER_INDEX_SHIFT) |
		  VE0_VALID |
		  (BRW_SURFACEFORMAT_R32_FLOAT << VE0_FORMAT_SHIFT) |
		  (offset			<< VE0_OFFSET_SHIFT));
	OUT_BATCH((BRW_VFCOMPONENT_STORE_SRC	<< VE1_VFCOMPONENT_0_SHIFT) |
		  (BRW_VFCOMPONENT_NOSTORE	<< VE1_VFCOMPONENT_1_SHIFT) |
		  (BRW_VFCOMPONENT_NOSTORE	<< VE1_VFCOMPONENT_2_SHIFT) |
		  (BRW_VFCOMPONENT_NOSTORE	<< VE1_VFCOMPONENT_3_SHIFT) |
		  (8 << VE1_DESTINATION_ELEMENT_OFFSET_SHIFT));

	offset += 4;
	break;

    case VS_GLYPHS:
	OUT_BATCH((0 << VE0_VERTEX_BUFFER_INDEX_SHIFT) |
		  VE0_VALID |
		  (BRW_SURFACEFORMAT_R16G16_FLOAT << VE0_FORMAT_SHIFT) |
		  (offset			<< VE0_OFFSET_SHIFT));
	OUT_BATCH((BRW_VFCOMPONENT_STORE_SRC	<< VE1_VFCOMPONENT_0_SHIFT) |
		  (BRW_VFCOMPONENT_STORE_SRC	<< VE1_VFCOMPONENT_1_SHIFT) |
		  (BRW_VFCOMPONENT_NOSTORE	<< VE1_VFCOMPONENT_2_SHIFT) |
		  (BRW_VFCOMPONENT_NOSTORE	<< VE1_VFCOMPONENT_3_SHIFT) |
		  (8 << VE1_DESTINATION_ELEMENT_OFFSET_SHIFT));

	offset += 4;
	break;
    }
    assert (shader->clip.type.vertex == VS_NONE);
    assert (shader->dst.type.vertex == VS_NONE);

    device->vertex_size = offset;
    i965_stream_align (&device->vertex, device->vertex_size);
    device->vertex.committed = device->vertex.used;

    device->rectangle_size = 3 * offset;
}

static cairo_bool_t
i965_shader_needs_surface_update (const i965_shader_t *shader,
				  const i965_device_t *device)
{
    return device->target != shader->target || shader->target->stream == 0 ||
	(shader->source.base.bo != NULL && device->source != shader->source.base.bo) ||
	(shader->mask.base.bo != NULL && device->mask != shader->mask.base.bo) ||
	(shader->clip.base.bo != NULL && device->clip != shader->clip.base.bo);
}

static cairo_bool_t
i965_shader_needs_constants_update (const i965_shader_t *shader,
				    const i965_device_t *device)
{
    if (shader->constants_size == 0)
	return FALSE;

    if (device->constants_size != shader->constants_size)
	return TRUE;

    return memcmp (device->constants,
		   shader->constants,
		   sizeof (float) * shader->constants_size);
}

static cairo_bool_t
i965_shader_needs_state_update (const i965_shader_t *shader,
				const i965_device_t *device)
{
    union {
	struct i965_sf_state sf;
	struct i965_wm_state wm;
	struct i965_cc_state cc;
    } state;

    i965_sf_state_init (&state.sf, shader);
    if (! i965_sf_state_equal (&state.sf, &device->sf_state))
	return TRUE;

    i965_wm_state_init (&state.wm, shader);
    if (! i965_wm_state_equal (&state.wm, &device->wm_state))
	return TRUE;

    i965_cc_state_init (&state.cc, shader);
    if (! i965_cc_state_equal (&state.cc, &device->cc_state))
	return TRUE;

    return FALSE;
}

static void
i965_emit_composite (i965_device_t *device,
		     i965_shader_t *shader)
{
    uint32_t draw_rectangle;

    if (i965_shader_needs_surface_update (shader, device)) {
	uint32_t offset;

	offset = emit_binding_table (device, shader);

	/* Only the PS uses the binding table */
	OUT_BATCH (BRW_3DSTATE_BINDING_TABLE_POINTERS | 4);
	OUT_BATCH (0); /* vs */
	OUT_BATCH (0); /* gs */
	OUT_BATCH (0); /* clip */
	OUT_BATCH (0); /* sf */
	OUT_BATCH (offset);

	device->target = shader->target;
	device->source = shader->source.base.bo;
	device->mask = shader->mask.base.bo;
	device->clip = shader->clip.base.bo;
    }

    /* The drawing rectangle clipping is always on.  Set it to values that
     * shouldn't do any clipping.
     */
    draw_rectangle = DRAW_YMAX (shader->target->intel.drm.height) |
	             DRAW_XMAX (shader->target->intel.drm.width);
    if (draw_rectangle != device->draw_rectangle) {
	OUT_BATCH (BRW_3DSTATE_DRAWING_RECTANGLE | 2);
	OUT_BATCH (0x00000000);	/* ymin, xmin */
	OUT_BATCH (draw_rectangle);
	OUT_BATCH (0x00000000);	/* yorigin, xorigin */
	device->draw_rectangle = draw_rectangle;
    }

    /* skip the depth buffer */
    /* skip the polygon stipple */
    /* skip the polygon stipple offset */
    /* skip the line stipple */

    /* Set the pointers to the 3d pipeline state */
    if (i965_shader_needs_state_update (shader, device)) {
	OUT_BATCH (BRW_3DSTATE_PIPELINED_POINTERS | 5);
	OUT_BATCH (vs_unit_state_emit (device));
	OUT_BATCH (BRW_GS_DISABLE);
	OUT_BATCH (BRW_CLIP_DISABLE);
	OUT_BATCH (gen4_create_sf_state (device, shader));
	OUT_BATCH (gen4_create_wm_state (device, shader));
	OUT_BATCH (cc_state_emit (device, shader));

	/* Once the units are initialized, we need to setup the fences */
	i965_emit_urb_fences (device);
    }

    if (i965_shader_needs_constants_update (shader, device)) {
	uint32_t size = (sizeof (float) * shader->constants_size + 63) & -64;

	/* XXX reuse clear/black/white
	 * ht!
	*/

	/* XXX CONSTANT_BUFFER Address Offset Disable? INSTPM? */

	assert (size <= 64 * URB_CS_ENTRY_SIZE);
	assert (((sizeof (float) * shader->constants_size + 31) & -32) == 32 * i965_shader_const_urb_length (shader));

	device->constants = i965_stream_alloc (&device->surface, 64, size);
	memcpy (device->constants, shader->constants, size);
	device->constants_size = shader->constants_size;

	OUT_BATCH (BRW_CONSTANT_BUFFER | (1 << 8));
	OUT_BATCH (i965_stream_offsetof (&device->surface, device->constants) + size / 64 - 1);
    }

    i965_emit_vertex_element (device, shader);
}

void
i965_flush_vertices (i965_device_t *device)
{
    int vertex_count, vertex_start;

    if (device->vertex.used == device->vertex.committed)
	return;

    assert (device->vertex.used > device->vertex.committed);

    vertex_start = device->vertex.committed / device->vertex_size;
    vertex_count =
	(device->vertex.used - device->vertex.committed) / device->vertex_size;

    assert (vertex_count);

    if (device->vertex_size != device->last_vertex_size) {
	i965_stream_add_pending_relocation (&device->vertex,
					    device->batch.used + 8,
					    I915_GEM_DOMAIN_VERTEX, 0,
					    0);

	OUT_BATCH (BRW_3DSTATE_VERTEX_BUFFERS | 3);
	OUT_BATCH ((0 << VB0_BUFFER_INDEX_SHIFT) |
		   VB0_VERTEXDATA |
		   (device->vertex_size << VB0_BUFFER_PITCH_SHIFT));
	OUT_BATCH (0); /* pending relocation */
	OUT_BATCH (0);
	OUT_BATCH (0);
	device->last_vertex_size = device->vertex_size;
    }

    OUT_BATCH (BRW_3DPRIMITIVE |
	       BRW_3DPRIMITIVE_VERTEX_SEQUENTIAL |
	       (_3DPRIM_RECTLIST << BRW_3DPRIMITIVE_TOPOLOGY_SHIFT) |
	       (0 << 9) |
	       4);
    OUT_BATCH (vertex_count);  /* vertex count per instance */
    OUT_BATCH (vertex_start);  /* start vertex offset */
    OUT_BATCH (1); /* single instance */
    OUT_BATCH (0);
    OUT_BATCH (0);

    device->vertex.committed = device->vertex.used;
}

void
i965_finish_vertices (i965_device_t *device)
{
    cairo_status_t status;

    i965_flush_vertices (device);

    i965_stream_commit (device, &device->vertex);

    if (! i965_shader_check_aperture (device->shader, device)) {
	status = i965_device_flush (device);
	if (unlikely (status))
	    longjmp (device->shader->unwind, status);

	status = i965_shader_commit (device->shader, device);
	assert (status == CAIRO_STATUS_SUCCESS);
    }

    device->last_vertex_size = 0;
}

static cairo_bool_t
i965_shader_needs_update (const i965_shader_t *shader,
			  const i965_device_t *device)
{
    if (i965_shader_needs_surface_update (shader, device))
	return TRUE;

    if (i965_shader_needs_constants_update (shader, device))
	return TRUE;

    return i965_shader_needs_state_update (shader, device);
}

static void
i965_shader_reduce (i965_shader_t *shader,
		    const i965_device_t *device)
{
    if (shader->op == CAIRO_OPERATOR_OVER &&
	(i965_wm_kernel_hash (shader) & ~0xff) == 0 &&
	(shader->source.base.content & CAIRO_CONTENT_ALPHA) == 0)
    {
	shader->op = CAIRO_OPERATOR_SOURCE;
    }
}

cairo_status_t
i965_shader_commit (i965_shader_t *shader,
		    i965_device_t *device)
{
    cairo_status_t status;

    if (! shader->committed) {
	device->shader = shader;

	status = i965_shader_setup_dst (shader);
	if (unlikely (status))
	    return status;

	i965_shader_setup_constants (shader);
	i965_shader_reduce (shader, device);

	if ((status = setjmp (shader->unwind)))
	    return status;

	shader->committed = TRUE;
    }

    if (! i965_shader_needs_update (shader, device))
	return CAIRO_STATUS_SUCCESS;

    /* XXX too many guestimates about likely maximum sizes */
recheck:
    if (device->batch.used + 128 > device->batch.size ||
	! i965_shader_check_aperture (shader, device))
    {
	status = i965_device_flush (device);
	if (unlikely (status))
	    longjmp (shader->unwind, status);
    }

    i965_flush_vertices (device);

    if (unlikely (device->surface.used + 128 > device->surface.size ||
		  device->surface.num_relocations + 4 > device->surface.max_relocations))
    {
	i965_stream_commit (device, &device->surface);
	goto recheck;
    }

    if (unlikely (device->general.used + 512 > device->general.size)) {
	i965_stream_commit (device, &device->general);
	i965_general_state_reset (device);
	goto recheck;
    }

    if (unlikely (device->batch.used == 0))
	i965_emit_invariants (device);

    if (unlikely (device->surface.num_pending_relocations == 0 ||
		  device->general.num_pending_relocations == 0))
    {
	i965_emit_base (device);
    }

    i965_emit_composite (device, shader);

    return CAIRO_STATUS_SUCCESS;
}

void
i965_clipped_vertices (i965_device_t *device,
		       struct i965_vbo *vbo,
		       cairo_region_t *clip_region)
{
    int i, num_rectangles, size;
    cairo_status_t status;

    if (vbo->count == 0)
	return;

    num_rectangles = cairo_region_num_rectangles (clip_region);
    assert (num_rectangles);

    if (vbo->next ||
	vbo->count * device->vertex_size + device->vertex.used > device->vertex.size)
    {
	i965_finish_vertices (device);

	size = device->rectangle_size;
	do {
	    for (i = 0; i < num_rectangles; i++) {
		cairo_rectangle_int_t rect;

		cairo_region_get_rectangle (clip_region, i, &rect);

		if (unlikely (device->vertex.used + size > device->vertex.size ||
			      device->batch.used + 64 > device->batch.size ||
			      ! i965_shader_check_aperture (device->shader, device)))
		{
		    status = i965_device_flush (device);
		    if (unlikely (status))
			longjmp (device->shader->unwind, status);

		    status = i965_shader_commit (device->shader, device);
		    assert (status == CAIRO_STATUS_SUCCESS);
		}

		i965_emit_relocation (device, &device->batch,
				      vbo->bo, 0,
				      I915_GEM_DOMAIN_VERTEX, 0,
				      device->batch.used + 8);

		OUT_BATCH (BRW_3DSTATE_VERTEX_BUFFERS | 3);
		OUT_BATCH ((0 << VB0_BUFFER_INDEX_SHIFT) |
			   VB0_VERTEXDATA |
			   (device->vertex_size << VB0_BUFFER_PITCH_SHIFT));
		OUT_BATCH (vbo->bo->offset);
		OUT_BATCH (0);
		OUT_BATCH (0);

		/* XXX scissor? */
		OUT_BATCH (BRW_3DSTATE_DRAWING_RECTANGLE | 2);
		OUT_BATCH (DRAW_YMIN (rect.y) | DRAW_XMIN (rect.x));
		OUT_BATCH (DRAW_YMAX (rect.y + rect.height) |
			   DRAW_XMAX (rect.x + rect.width));
		OUT_BATCH (0x00000000);	/* yorigin, xorigin */

		OUT_BATCH (BRW_3DPRIMITIVE |
			   BRW_3DPRIMITIVE_VERTEX_SEQUENTIAL |
			   (_3DPRIM_RECTLIST << BRW_3DPRIMITIVE_TOPOLOGY_SHIFT) |
			   (0 << 9) |
			   4);
		OUT_BATCH (vbo->count);  /* vertex count per instance */
		OUT_BATCH (0);  /* start vertex offset */
		OUT_BATCH (1); /* single instance */
		OUT_BATCH (0);
		OUT_BATCH (0);
	    }
	} while ((vbo = vbo->next) != NULL);
	assert (device->last_vertex_size == 0);
    } else {
	int vertex_start, vertex_count;
	void *ptr;

	vertex_start = device->vertex.committed / device->vertex_size;
	vertex_count = vbo->count;

	size = vertex_count * device->vertex_size;
	ptr = intel_bo_map (&device->intel, vbo->bo);
	memcpy (device->vertex.data + device->vertex.used, ptr, size);
	device->vertex.committed = device->vertex.used += size;

	for (i = 0; i < num_rectangles; i++) {
	    cairo_rectangle_int_t rect;

	    cairo_region_get_rectangle (clip_region, i, &rect);

	    /* XXX scissor? */
	    OUT_BATCH (BRW_3DSTATE_DRAWING_RECTANGLE | 2);
	    OUT_BATCH (DRAW_YMIN (rect.y) | DRAW_XMIN (rect.x));
	    OUT_BATCH (DRAW_YMAX (rect.y + rect.height) |
		       DRAW_XMAX (rect.x + rect.width));
	    OUT_BATCH (0x00000000);	/* yorigin, xorigin */

	    OUT_BATCH (BRW_3DPRIMITIVE |
		       BRW_3DPRIMITIVE_VERTEX_SEQUENTIAL |
		       (_3DPRIM_RECTLIST << BRW_3DPRIMITIVE_TOPOLOGY_SHIFT) |
		       (0 << 9) |
		       4);
	    OUT_BATCH (vertex_count);  /* vertex count per instance */
	    OUT_BATCH (vertex_start);  /* start vertex offset */
	    OUT_BATCH (1); /* single instance */
	    OUT_BATCH (0);
	    OUT_BATCH (0);
	}
    }

    device->draw_rectangle = 0;
}
