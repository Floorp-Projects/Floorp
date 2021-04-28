/* cairo - a vector graphics library with display and print output
 *
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
 */

#include "cairoint.h"

#include "cairo-error-private.h"
#include "cairo-drm-i915-private.h"
#include "cairo-surface-offset-private.h"
#include "cairo-surface-subsurface-private.h"
#include "cairo-surface-snapshot-private.h"
#include "cairo-image-surface-private.h"

#if 0
static cairo_status_t
i915_packed_pixel_surface_finish (void *abstract_surface)
{
    i915_packed_pixel_surface_t *surface = abstract_surface;
    i915_device_t *device;

    device = i915_device_acquire (&surface->device->intel.base);

    intel_bo_destroy (&device->intel, surface->bo);

    if (surface->is_current_texture) {
	if (surface->is_current_texture & CURRENT_SOURCE)
	    device->current_source = NULL;
	if (surface->is_current_texture & CURRENT_MASK)
	    device->current_mask = NULL;
	device->current_n_samplers = 0;
    }

    i915_device_release (device);

    return CAIRO_STATUS_SUCCESS;
}

static const cairo_surface_backend_t i915_packed_pixel_surface_backend = {
    I915_PACKED_PIXEL_SURFACE_TYPE,
    i915_packed_pixel_surface_finish,
};

static cairo_surface_t *
i915_packed_pixel_surface_create (i915_device_t *device,
				   i915_packed_pixel_t pixel,
				   const uint8_t *data,
				   uint32_t length,
				   uint32_t width, uint32_t height)
{
    i915_packed_pixel_surface_t *surface;
    cairo_content_t content;
    uint32_t tiling, size;
    uint32_t stride, half_stride;
    uint32_t i;

    if (width > 2048 || height > 2048)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_SIZE));

    surface = _cairo_malloc (sizeof (i915_packed_pixel_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    tiling = I915_TILING_NONE; /* XXX */
    half_stride = stride = i915_tiling_stride (tiling, width/2);
    if (stride < width)
	stride *= 2 ;
    height = i915_tiling_height (tiling, height);

    switch (surface->pixel = pixel) {
    case YUV_I420:
	content = CAIRO_CONTENT_COLOR;

	surface->offset[0] = 0;
	surface->width[0] = width;
	surface->height[0] = height;
	surface->stride[0] = stride;
	surface->map0[0] = MAPSURF_8BIT | MT_8BIT_I8 | MS3_tiling (tiling);
	surface->map0[0] |= ((height - 1) << MS3_HEIGHT_SHIFT) |
			    ((width - 1)  << MS3_WIDTH_SHIFT);
	surface->map1[0] = (stride / 4 - 1) << MS4_PITCH_SHIFT;

	surface->offset[1] = stride * height;
	surface->width[1] = width / 2;
	surface->height[1] = height / 2;
	surface->stride[1] = half_stride;
	surface->map0[1] = MAPSURF_8BIT | MT_8BIT_I8 | MS3_tiling (tiling);
	surface->map0[1] |= ((height/2 - 1) << MS3_HEIGHT_SHIFT) |
			    ((width/2 - 1)  << MS3_WIDTH_SHIFT);
	surface->map1[1] = (half_stride / 4 - 1) << MS4_PITCH_SHIFT;

	if (width < half_stride) {
	    surface->offset[2] = stride * height + half_stride / 2;
	    size = stride * height + half_stride * height / 2;
	} else {
	    surface->offset[2] = stride * height + half_stride * height / 2;
	    size = stride * height + half_stride * height;
	}
	surface->width[2] = width / 2;
	surface->height[2] = height / 2;
	surface->stride[2] = half_stride;
	surface->map0[2] = MAPSURF_8BIT | MT_8BIT_I8 | MS3_tiling (tiling);
	surface->map0[2] |= ((height/2 - 1) << MS3_HEIGHT_SHIFT) |
			    ((width/2 - 1)  << MS3_WIDTH_SHIFT);
	surface->map1[2] = (half_stride / 4 - 1) << MS4_PITCH_SHIFT;
	break;

    case NONE:
    case YUV_YV12:
    case YUV_YUY2:
    case YUV_UYVY:
	ASSERT_NOT_REACHED;
	break;
    }

    _cairo_surface_init (&surface->base,
	                 &i915_packed_pixel_surface_backend,
			 content);

    surface->bo = intel_bo_create (&device->intel, size, FALSE);
    assert (surface->bo->tiling == I915_TILING_NONE);
    if (unlikely (surface->bo == NULL)) {
	free (surface);
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
    }

    if (tiling == I915_TILING_NONE) {
	intel_bo_t *bo = surface->bo;
	uint32_t dst;
	int uv;

	dst = surface->offset[0];
	if (width == stride) {
	    size = stride * height;
	    intel_bo_write (&device->intel, bo, dst, size, data);
	    data += size;
	} else {
	    for (i = 0; i < height; i++) {
		intel_bo_write (&device->intel, bo, dst, width, data);
		dst += stride;
		data += width;
	    }
	}

	for (uv = 1; uv <= 2; uv++) {
	    dst = surface->offset[uv];
	    if (width / 2 == half_stride) {
		size = half_stride * height / 2;
		intel_bo_write (&device->intel, bo, dst, size, data);
		data += size;
	    } else {
		size = width / 2;
		for (i = 0; i < height / 2; i++) {
		    intel_bo_write (&device->intel, bo, dst, size, data);
		    dst += half_stride;
		    data += size;
		}
	    }
	}
    } else {
	uint8_t *dst, *base;

	base = intel_bo_map (&device->intel, surface->bo);

	dst = base + surface->offset[0];
	if (width == stride) {
	    size = stride * height;
	    memcpy (dst, data, size);
	    data += size;
	} else {
	    for (i = 0; i < height; i++) {
		memcpy (dst, data, width);
		dst += stride;
		data += width;
	    }
	}

	dst = base + surface->offset[1];
	if (width / 2 == half_stride) {
	    size = half_stride * height / 2;
	    memcpy (dst, data, size);
	    data += size;
	} else {
	    size = width / 2;
	    for (i = 0; i < height / 2; i++) {
		memcpy (dst, data, size);
		dst += half_stride;
		data += size;
	    }
	}

	dst = base + surface->offset[2];
	if (width / 2 == half_stride) {
	    size = half_stride * height / 2;
	    memcpy (dst, data, size);
	    data += size;
	} else {
	    size = width / 2;
	    for (i = 0; i < height / 2; i++) {
		memcpy (dst, data, size);
		dst += half_stride;
		data += size;
	    }
	}
    }

    surface->device = device;
    surface->is_current_texture = 0;

    return &surface->base;
}

static cairo_int_status_t
i915_clone_yuv (i915_surface_t *surface,
		 cairo_surface_t *source,
		 int width, int height,
		 cairo_surface_t **clone_out)
{
    const uint8_t *mime_data = NULL;
    unsigned int mime_data_length;
    cairo_surface_t *clone;

    cairo_surface_get_mime_data (source, "video/x-raw-yuv/i420",
				 &mime_data, &mime_data_length);
    if (mime_data == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    clone =
	i915_packed_pixel_surface_create ((i915_device_t *) surface->base.device,
					   YUV_I420,
					   mime_data, mime_data_length,
					   width, height);
    if (clone == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;
    if (unlikely (clone->status))
	return clone->status;

    *clone_out = clone;
    return CAIRO_STATUS_SUCCESS;
}
#endif

/* Max instruction count: 4 */
static void
i915_shader_linear_color (i915_device_t *device,
			  enum i915_shader_linear_mode mode,
			  int in, int c0, int c1, int out)
{
    int tmp = FS_U0;

    switch (mode) {
    case LINEAR_TEXTURE:
	ASSERT_NOT_REACHED;
    case LINEAR_NONE:
	tmp = in;
	break;

    case LINEAR_REPEAT:
	i915_fs_frc (tmp, i915_fs_operand (in, X, X, X, X));
	break;
#if 0
    case LINEAR_REFLECT:
	/* XXX needs an extra constant: C2 [0.5, 2.0, x, x] */
	i915_fs_mul (tmp, in, 0.5);
	i915_fs_frc (tmp, i915_fs_operand_reg (tmp));
	i915_fs_mul (tmp, tmp, 2.0);
	i915_fs_add (tmp, i915_fs_operand_one (),
		     i915_fs_operand_reg_negate (tmp));
	i915_fs_cmp (tmp,
		     i915_fs_operand_reg (tmp),
		     i915_fs_operand_reg (tmp),
		     i915_fs_operand_reg_negate (tmp));
	i915_fs_add (tmp, i915_fs_operand_one (),
		     i915_fs_operand_reg_negate (tmp));
#endif
    case LINEAR_PAD:
	i915_fs_max (tmp,
		     i915_fs_operand_zero (),
		     i915_fs_operand (in, X, X, X, X));
	i915_fs_min (tmp,
		     i915_fs_operand_one (),
		     i915_fs_operand_reg (tmp));
	break;
    }

    /* interpolate */
    i915_fs_mad (out, 0,
		 i915_fs_operand (tmp, NEG_X, NEG_X, NEG_X, NEG_X),
		 i915_fs_operand_reg (c0),
		 i915_fs_operand_reg (c0));
    i915_fs_mad (out, 0,
		 i915_fs_operand (tmp, X, X, X, X),
		 i915_fs_operand_reg (c1),
		 i915_fs_operand_reg (out));
}

static void
i915_shader_radial_init (struct i915_shader_radial *r,
			 const cairo_radial_pattern_t *radial)
{
    double dx, dy, dr, r1;

    dx = radial->cd2.center.x - radial->cd1.center.x;
    dy = radial->cd2.center.y - radial->cd1.center.y;
    dr = radial->cd2.radius   - radial->cd1.radius;

    r1 = radial->cd1.radius;

    if (radial->cd2.center.x == radial->cd1.center.x &&
	radial->cd2.center.y == radial->cd1.center.y)
    {
	/* XXX dr == 0, meaningless with anything other than PAD */
	r->constants[0] = radial->cd1.center.x / dr;
	r->constants[1] = radial->cd1.center.y / dr;
	r->constants[2] = 1. / dr;
	r->constants[3] = -r1 / dr;

	r->constants[4] = 0;
	r->constants[5] = 0;
	r->constants[6] = 0;
	r->constants[7] = 0;

	r->base.mode = RADIAL_ONE;
    } else {
	r->constants[0] = -radial->cd1.center.x;
	r->constants[1] = -radial->cd1.center.y;
	r->constants[2] = r1;
	r->constants[3] = -4 * (dx*dx + dy*dy - dr*dr);

	r->constants[4] = -2 * dx;
	r->constants[5] = -2 * dy;
	r->constants[6] = -2 * r1 * dr;
	r->constants[7] = 1 / (2 * (dx*dx + dy*dy - dr*dr));

	r->base.mode = RADIAL_TWO;
    }

    r->base.matrix = radial->base.base.matrix;
}

/* Max instruction count: 10 */
static void
i915_shader_radial_coord (i915_device_t *device,
			  enum i915_shader_radial_mode mode,
			  int in, int g0, int g1, int out)
{
    switch (mode) {
    case RADIAL_ONE:
	/*
	   pdx = (x - c1x) / dr, pdy = (y - c1y) / dr;
	   r² = pdx*pdx + pdy*pdy
	   t = r²/sqrt(r²) - r1/dr;
	   */
	i915_fs_mad (FS_U0, MASK_X | MASK_Y,
		     i915_fs_operand (in, X, Y, ZERO, ZERO),
		     i915_fs_operand (g0, Z, Z, ZERO, ZERO),
		     i915_fs_operand (g0, NEG_X, NEG_Y, ZERO, ZERO));
	i915_fs_dp2add (FS_U0, MASK_X,
			i915_fs_operand (FS_U0, X, Y, ZERO, ZERO),
			i915_fs_operand (FS_U0, X, Y, ZERO, ZERO),
			i915_fs_operand_zero ());
	i915_fs_rsq (out, MASK_X, i915_fs_operand (FS_U0, X, X, X, X));
	i915_fs_mad (out, MASK_X,
		     i915_fs_operand (FS_U0, X, ZERO, ZERO, ZERO),
		     i915_fs_operand (out, X, ZERO, ZERO, ZERO),
		     i915_fs_operand (g0, W, ZERO, ZERO, ZERO));
	break;

    case RADIAL_TWO:
	/*
	   pdx = x - c1x, pdy = y - c1y;
	   A = dx² + dy² - dr²
	   B = -2*(pdx*dx + pdy*dy + r1*dr);
	   C = pdx² + pdy² - r1²;
	   det = B*B - 4*A*C;
	   t = (-B + sqrt (det)) / (2 * A)
	   */

	/* u0.x = pdx, u0.y = pdy, u[0].z = r1; */
	i915_fs_add (FS_U0,
		     i915_fs_operand (in, X, Y, ZERO, ZERO),
		     i915_fs_operand (g0, X, Y, Z, ZERO));
	/* u0.x = pdx, u0.y = pdy, u[0].z = r1, u[0].w = B; */
	i915_fs_dp3 (FS_U0, MASK_W,
		     i915_fs_operand (FS_U0, X, Y, ONE, ZERO),
		     i915_fs_operand (g1, X, Y, Z, ZERO));
	/* u1.x = pdx² + pdy² - r1²; [C] */
	i915_fs_dp3 (FS_U1, MASK_X,
		     i915_fs_operand (FS_U0, X, Y, Z, ZERO),
		     i915_fs_operand (FS_U0, X, Y, NEG_Z, ZERO));
	/* u1.x = C, u1.y = B, u1.z=-4*A; */
	i915_fs_mov_masked (FS_U1, MASK_Y, i915_fs_operand (FS_U0, W, W, W, W));
	i915_fs_mov_masked (FS_U1, MASK_Z, i915_fs_operand (g0, W, W, W, W));
	/* u1.x = B² - 4*A*C */
	i915_fs_dp2add (FS_U1, MASK_X,
			i915_fs_operand (FS_U1, X, Y, ZERO, ZERO),
			i915_fs_operand (FS_U1, Z, Y, ZERO, ZERO),
			i915_fs_operand_zero ());
	/* out.x = -B + sqrt (B² - 4*A*C),
	 * out.y = -B - sqrt (B² - 4*A*C),
	 */
	i915_fs_rsq (out, MASK_X, i915_fs_operand (FS_U1, X, X, X, X));
	i915_fs_mad (out, MASK_X | MASK_Y,
		     i915_fs_operand (out, X, X, ZERO, ZERO),
		     i915_fs_operand (FS_U1, X, NEG_X, ZERO, ZERO),
		     i915_fs_operand (FS_U0, NEG_W, NEG_W, ZERO, ZERO));
	/* out.x = (-B + sqrt (B² - 4*A*C)) / (2 * A),
	 * out.y = (-B - sqrt (B² - 4*A*C)) / (2 * A)
	 */
	i915_fs_mul (out,
		     i915_fs_operand (out, X, Y, ZERO, ZERO),
		     i915_fs_operand (g1, W, W, ZERO, ZERO));
	/* if (A > 0)
	 *   out = (-B + sqrt (B² - 4*A*C)) / (2 * A),
	 * else
	 *   out = (-B - sqrt (B² - 4*A*C)) / (2 * A)
	 */
	i915_fs_cmp (out,
		     i915_fs_operand (g1, W, ZERO, ZERO, ZERO),
		     i915_fs_operand (out, X, ZERO, ZERO, ZERO),
		     i915_fs_operand (out, Y, ZERO, ZERO, ZERO));
	break;
    }
}

/* Max instruction count: 7 */
static inline void
i915_shader_yuv_color (i915_device_t *device,
		       int y, int u, int v,
		       int c0, int c1, int c2,
		       int out)
{
    i915_fs_mov_masked (FS_U0, MASK_X, i915_fs_operand_reg (y));
    i915_fs_mov_masked (FS_U0, MASK_Y, i915_fs_operand_reg (u));
    i915_fs_mov_masked (FS_U0, MASK_Z, i915_fs_operand_reg (v));

    i915_fs_add (FS_U0,
		 i915_fs_operand_reg (FS_U0),
		 i915_fs_operand_reg (c0));
    i915_fs_dp3 (out, MASK_X,
		 i915_fs_operand_reg (FS_U0),
		 i915_fs_operand (c1, X, ZERO, Y, ZERO));
    i915_fs_dp3 (out, MASK_Z,
		 i915_fs_operand_reg (FS_U0),
		 i915_fs_operand (c1, Z, W, ZERO, ZERO));
    i915_fs_dp3 (out, MASK_Y,
		 i915_fs_operand_reg (FS_U0),
		 i915_fs_operand_reg (c2));
}

static inline uint32_t
i915_shader_channel_key (const union i915_shader_channel *channel)
{
    return (channel->type.fragment & 0x0f) | (channel->base.mode << FS_DETAILS_SHIFT);
}

static uint32_t
i915_shader_channel_get_num_tex_coords (const union i915_shader_channel *channel)
{
    switch (channel->type.fragment) {
    default:
    case FS_ZERO:
    case FS_ONE:
    case FS_CONSTANT:
    case FS_PURE:
    case FS_DIFFUSE:
	return 0;

    case FS_LINEAR:
    case FS_RADIAL:
    case FS_TEXTURE:
    case FS_SPANS:
    case FS_YUV:
	return 1;
    }
}

static uint32_t
i915_shader_get_num_tex_coords (const i915_shader_t *shader)
{
    uint32_t num_tex_coords;

    num_tex_coords = 0;

    num_tex_coords += i915_shader_channel_get_num_tex_coords (&shader->source);
    num_tex_coords += i915_shader_channel_get_num_tex_coords (&shader->mask);
    num_tex_coords += i915_shader_channel_get_num_tex_coords (&shader->clip);
    num_tex_coords += i915_shader_channel_get_num_tex_coords (&shader->dst);

    return num_tex_coords;
}

#define i915_fs_operand_impure(reg, channel, pure) \
    (reg | \
     (((pure & (1 << 0)) ? channel##_CHANNEL_VAL : ZERO_CHANNEL_VAL) << X_CHANNEL_SHIFT) | \
     (((pure & (1 << 1)) ? channel##_CHANNEL_VAL : ZERO_CHANNEL_VAL) << Y_CHANNEL_SHIFT) | \
     (((pure & (1 << 2)) ? channel##_CHANNEL_VAL : ZERO_CHANNEL_VAL) << Z_CHANNEL_SHIFT) | \
     (((pure & (1 << 3)) ? channel##_CHANNEL_VAL : ZERO_CHANNEL_VAL) << W_CHANNEL_SHIFT))

#define i915_fs_operand_pure(pure) \
    (FS_R0 | \
     (((pure & (1 << 0)) ? ONE_CHANNEL_VAL : ZERO_CHANNEL_VAL) << X_CHANNEL_SHIFT) | \
     (((pure & (1 << 1)) ? ONE_CHANNEL_VAL : ZERO_CHANNEL_VAL) << Y_CHANNEL_SHIFT) | \
     (((pure & (1 << 2)) ? ONE_CHANNEL_VAL : ZERO_CHANNEL_VAL) << Z_CHANNEL_SHIFT) | \
     (((pure & (1 << 3)) ? ONE_CHANNEL_VAL : ZERO_CHANNEL_VAL) << W_CHANNEL_SHIFT))

static void
i915_set_shader_program (i915_device_t *device,
			 const i915_shader_t *shader)
{
    uint32_t num_tex_coords;
    uint32_t num_samplers;
    uint32_t n;
    uint32_t texture_offset = 0;
    uint32_t constant_offset = 0;
    uint32_t sampler_offset = 0;
    uint32_t source_reg;
    uint32_t source_pure;
    uint32_t mask_reg;
    uint32_t out_reg;
    uint32_t dest_reg;
    FS_LOCALS;

    n = (i915_shader_channel_key (&shader->source) <<  0) |
	(i915_shader_channel_key (&shader->mask)   <<  8) |
	(i915_shader_channel_key (&shader->clip)   << 16) |
	(shader->op << 24) |
	((shader->opacity < 1.) << 30) |
	(((shader->content & CAIRO_CONTENT_ALPHA) == CAIRO_CONTENT_ALPHA) << 31);
    if (n == device->current_program)
	return;
    device->current_program = n;

    FS_BEGIN ();

    if (shader->source.type.fragment == FS_ZERO) {
	if (shader->clip.type.fragment == FS_TEXTURE) {
	    /* XXX need_combine */
	    assert (shader->mask.type.fragment == (i915_fragment_shader_t) -1);
	    i915_fs_dcl (FS_T0);
	    i915_fs_texld (FS_U0, FS_S0, FS_T0);
	    if ((shader->content & CAIRO_CONTENT_COLOR) == 0)
		i915_fs_mov (FS_OC, i915_fs_operand (FS_U0, W, W, W, W));
	    else
		i915_fs_mov (FS_OC, i915_fs_operand (FS_U0, ZERO, ZERO, ZERO, W));
	} else {
	    i915_fs_mov (FS_OC, i915_fs_operand_zero ());
	}

	FS_END ();
	return;
    }

    num_tex_coords = i915_shader_get_num_tex_coords (shader);
    for (n = 0; n < num_tex_coords; n++)
	i915_fs_dcl (FS_T0 + n);

    num_samplers =
	shader->source.base.n_samplers +
	shader->mask.base.n_samplers +
	shader->clip.base.n_samplers +
	shader->dst.base.n_samplers;
    for (n = 0; n < num_samplers; n++)
	i915_fs_dcl (FS_S0 + n);

    source_reg = ~0;
    source_pure = 0;
    out_reg = FS_R0;
    if (! shader->need_combine &&
	shader->mask.type.fragment == (i915_fragment_shader_t) -1 &&
	shader->clip.type.fragment != FS_TEXTURE &&
	shader->content != CAIRO_CONTENT_ALPHA)
    {
	out_reg = FS_OC;
    }

    switch (shader->source.type.fragment) {
    default:
    case FS_ZERO:
    case FS_SPANS:
	ASSERT_NOT_REACHED;

    case FS_PURE:
	source_pure = shader->source.solid.pure;
    case FS_ONE:
	break;

    case FS_CONSTANT:
	source_reg = FS_C0;
	constant_offset += 1;
	break;

    case FS_DIFFUSE:
	i915_fs_dcl (FS_T8);
	source_reg = FS_T8;
	break;

    case FS_LINEAR:
	i915_shader_linear_color (device, shader->source.base.mode,
				  FS_T0, /* input */
				  FS_C0, FS_C1, /* colour ramp */
				  FS_U3); /* unpremultiplied output */
	/* XXX can we defer premultiplication? */
	i915_fs_mul (out_reg,
		     i915_fs_operand_reg (FS_U3),
		     i915_fs_operand (FS_U3, W, W, W, ONE));

	constant_offset += 2;
	texture_offset += 1;
	source_reg = out_reg;
	break;

    case FS_RADIAL:
	i915_shader_radial_coord (device, shader->source.base.mode,
				  FS_T0, /* input */
				  FS_C0, FS_C1, /* gradient constants */
				  FS_R0); /* coordinate */

	i915_fs_texld (out_reg, FS_S0, FS_R0);
	constant_offset += 2;
	texture_offset += 1;
	sampler_offset += 1;
	source_reg = out_reg;
	break;

    case FS_TEXTURE:
	i915_fs_texld (out_reg, FS_S0, FS_T0);
	texture_offset += 1;
	sampler_offset += 1;
	source_reg = out_reg;
	break;

    case FS_YUV:
	/* Load samplers to temporaries. */
	i915_fs_texld (FS_R0, FS_S0, FS_T0);
	i915_fs_texld (FS_R1, FS_S1, FS_T0);
	i915_fs_texld (FS_R2, FS_S2, FS_T0);

	i915_shader_yuv_color (device,
			       FS_R0, FS_R1, FS_R2, /* y, u, v */
			       FS_C0, FS_C1, FS_C2, /* coefficients */
			       out_reg);

	constant_offset += 3;
	texture_offset += 1;
	sampler_offset += 3;
	source_reg = out_reg;
	break;
    }

    mask_reg = ~0;
    switch (shader->mask.type.fragment) {
    case FS_PURE:
    case FS_ZERO:
    case FS_YUV:
    case FS_DIFFUSE:
	ASSERT_NOT_REACHED;
    case FS_ONE:
    default:
	break;

    case FS_SPANS:
	mask_reg = FS_T0 + texture_offset;
	texture_offset += 1;
	break;

    case FS_CONSTANT:
	mask_reg = FS_C0 + constant_offset;
	constant_offset += 1;
	break;

    case FS_LINEAR:
	i915_shader_linear_color (device, shader->mask.base.mode,
				  FS_T0 + texture_offset, /* input */
				  FS_C0 + constant_offset,
				  FS_C0 + constant_offset + 1, /* colour ramp */
				  FS_R1); /* unpremultiplied output */
	constant_offset += 2;
	texture_offset += 1;
	mask_reg = FS_R1;
	break;

    case FS_RADIAL:
	i915_shader_radial_coord (device, shader->mask.base.mode,
				  FS_T0 + texture_offset, /* input */
				  FS_C0 + constant_offset,
				  FS_C0 + constant_offset + 1, /* gradient constants */
				  FS_R1); /* coordinate */

	i915_fs_texld (FS_R1, FS_S0 + sampler_offset, FS_R1);
	constant_offset += 2;
	texture_offset += 1;
	sampler_offset += 1;
	mask_reg = FS_R1;
	break;

    case FS_TEXTURE:
	i915_fs_texld (FS_R1, FS_S0 + sampler_offset, FS_T0 + texture_offset);
	texture_offset += 1;
	sampler_offset += 1;
	mask_reg = FS_R1;
	break;
    }

    if (mask_reg != ~0U) {
	if (! shader->need_combine &&
	    shader->clip.type.fragment != FS_TEXTURE &&
	    (shader->content != CAIRO_CONTENT_ALPHA || source_reg == ~0U))
	{
	    out_reg = FS_OC;
	}
	if (source_reg == ~0U) {
	    if (source_pure) {
		if (shader->mask.type.fragment == FS_SPANS) {
		    if (out_reg == FS_OC && shader->content == CAIRO_CONTENT_ALPHA) {
			if (source_pure & (1 << 3))
			    i915_fs_mov (out_reg, i915_fs_operand (mask_reg, X, X, X, X));
			else
			    i915_fs_mov (out_reg, i915_fs_operand_zero ());
		    } else {
			i915_fs_mov (out_reg,
				     i915_fs_operand_impure (mask_reg, X, source_pure));
		    }
		} else {
		    /* XXX ComponentAlpha
		       i915_fs_mov (out_reg,
		       i915_fs_operand_pure (mask_reg,
		       shader->source.solid.pure));
		       */
		    if (out_reg == FS_OC && shader->content == CAIRO_CONTENT_ALPHA) {
			if (source_pure & (1 << 3))
			    i915_fs_mov (out_reg, i915_fs_operand (mask_reg, W, W, W, W));
			else
			    i915_fs_mov (out_reg, i915_fs_operand_zero ());
		    } else {
			i915_fs_mov (out_reg,
				     i915_fs_operand_impure (mask_reg, W, source_pure));
		    }
		}
		source_reg = out_reg;
	    } else if (shader->mask.type.fragment == FS_SPANS) {
		i915_fs_mov (out_reg,
			     i915_fs_operand (mask_reg, X, X, X, X));
		source_reg = out_reg;
	    } else {
		source_reg = mask_reg;
	    }
	} else {
	    if (shader->mask.type.fragment == FS_SPANS) {
		    if (out_reg == FS_OC && shader->content == CAIRO_CONTENT_ALPHA) {
			i915_fs_mul (out_reg,
				     i915_fs_operand (source_reg, W, W, W, W),
				     i915_fs_operand (mask_reg, X, X, X, X));
		    } else {
			i915_fs_mul (out_reg,
				     i915_fs_operand_reg (source_reg),
				     i915_fs_operand (mask_reg, X, X, X, X));
		    }
	    } else {
		/* XXX ComponentAlpha
		i915_fs_mul (FS_R0,
			     i915_fs_operand_reg (source_reg),
			     i915_fs_operand_reg (mask_reg));
		 */
		if (out_reg == FS_OC && shader->content == CAIRO_CONTENT_ALPHA) {
		    i915_fs_mul (out_reg,
				 i915_fs_operand (source_reg, W, W, W, W),
				 i915_fs_operand (mask_reg, W, W, W, W));
		} else {
		    i915_fs_mul (out_reg,
				 i915_fs_operand_reg (source_reg),
				 i915_fs_operand (mask_reg, W, W, W, W));
		}
	    }

	    source_reg = out_reg;
	}
    }

    if (shader->opacity < 1.) {
	i915_fs_mul (source_reg,
		     i915_fs_operand_reg (source_reg),
		     i915_fs_operand_reg (FS_C0 + constant_offset));
	constant_offset++;
    }

    /* need to preserve order of src, mask, clip, dst */
    mask_reg = ~0;
    if (shader->clip.type.fragment == FS_TEXTURE) {
	i915_fs_texld (FS_R1, FS_S0 + sampler_offset, FS_T0 + texture_offset);
	texture_offset += 1;
	sampler_offset += 1;
	mask_reg = FS_R1;
    }

    if (shader->need_combine) {
	assert (shader->dst.type.fragment == FS_TEXTURE);

	i915_fs_texld (FS_R2, FS_S0 + sampler_offset, FS_T0 + texture_offset);
	texture_offset += 1;
	sampler_offset += 1;
	dest_reg = FS_R2;

	switch (shader->op) {
	case CAIRO_OPERATOR_CLEAR:
	case CAIRO_OPERATOR_SOURCE:
	    ASSERT_NOT_REACHED;

	case CAIRO_OPERATOR_OVER:
	    if (source_reg == ~0U) {
		/* XXX shader->source.type.fragment == FS_PURE */
		dest_reg = FS_OC;
	    } else {
		i915_fs_add (FS_U0,
			     i915_fs_operand (source_reg, NEG_W, NEG_W, NEG_W, NEG_W),
			     i915_fs_operand_one ());
		i915_fs_mul (FS_U0,
			     i915_fs_operand_reg (FS_U0),
			     dest_reg);
		i915_fs_add (FS_R3,
			     i915_fs_operand_reg (source_reg),
			     i915_fs_operand_reg (FS_U0));
		source_reg = FS_R3;
	    }
	    break;

	case CAIRO_OPERATOR_IN:
	    if (source_reg == ~0U) {
		/* XXX shader->source.type.fragment == FS_PURE */
		source_reg = dest_reg;
	    } else {
		i915_fs_mul (FS_R3,
			     i915_fs_operand_reg (source_reg),
			     dest_reg);
		source_reg = FS_R3;
	    }
	    break;

	case CAIRO_OPERATOR_OUT:
	    if (source_reg == ~0U) {
		/* XXX shader->source.type.fragment == FS_PURE */
		i915_fs_mov (FS_R3, i915_fs_operand_zero ());
		source_reg = FS_R3;
	    } else {
		i915_fs_add (FS_U0,
			     i915_fs_operand (source_reg, NEG_W, NEG_W, NEG_W, NEG_W),
			     i915_fs_operand_one ());
		i915_fs_mul (FS_R3,
			     i915_fs_operand_reg (FS_U0),
			     dest_reg);
		source_reg = FS_R3;
	    }
	    break;

	case CAIRO_OPERATOR_ATOP:

	case CAIRO_OPERATOR_DEST:
	case CAIRO_OPERATOR_DEST_OVER:
	case CAIRO_OPERATOR_DEST_IN:
	case CAIRO_OPERATOR_DEST_OUT:
	case CAIRO_OPERATOR_DEST_ATOP:

	case CAIRO_OPERATOR_XOR:
	case CAIRO_OPERATOR_ADD:
	case CAIRO_OPERATOR_SATURATE:

	case CAIRO_OPERATOR_MULTIPLY:
	case CAIRO_OPERATOR_SCREEN:
	case CAIRO_OPERATOR_OVERLAY:
	case CAIRO_OPERATOR_DARKEN:
	case CAIRO_OPERATOR_LIGHTEN:
	case CAIRO_OPERATOR_COLOR_DODGE:
	case CAIRO_OPERATOR_COLOR_BURN:
	case CAIRO_OPERATOR_HARD_LIGHT:
	case CAIRO_OPERATOR_SOFT_LIGHT:
	case CAIRO_OPERATOR_DIFFERENCE:
	case CAIRO_OPERATOR_EXCLUSION:
	case CAIRO_OPERATOR_HSL_HUE:
	case CAIRO_OPERATOR_HSL_SATURATION:
	case CAIRO_OPERATOR_HSL_COLOR:
	case CAIRO_OPERATOR_HSL_LUMINOSITY:
	    ASSERT_NOT_REACHED;
	    break;
	}
    }

    if (shader->clip.type.fragment == FS_TEXTURE) {
	assert (mask_reg != ~0U);

	if (! shader->need_combine) {
	    /* (source IN clip) */
	    if (source_reg == ~0U) {
		if (source_pure == 0) {
		    source_reg = mask_reg;
		} else {
		    out_reg = FS_OC;
		    if ((shader->content & CAIRO_CONTENT_COLOR) == 0) {
			if (source_pure & (1 << 3))
			    i915_fs_mov (out_reg, i915_fs_operand (mask_reg, W, W, W, W));
			else
			    i915_fs_mov (out_reg, i915_fs_operand_zero ());
		    } else {
			i915_fs_mov (out_reg,
				     i915_fs_operand_impure (mask_reg, W, source_pure));
		    }
		    source_reg = out_reg;
		}
	    } else if (mask_reg) {
		out_reg = FS_OC;
		if ((shader->content & CAIRO_CONTENT_COLOR) == 0) {
		    i915_fs_mul (out_reg,
				 i915_fs_operand (source_reg, W, W, W, W),
				 i915_fs_operand (mask_reg, W, W, W, W));
		} else {
		    i915_fs_mul (out_reg,
				 i915_fs_operand_reg (source_reg),
				 i915_fs_operand (mask_reg, W, W, W, W));
		}

		source_reg = out_reg;
	    }
	} else {
	    /* (source OP dest) LERP_clip dest */
	    if (source_reg == ~0U) {
		if (source_pure == 0) {
		    i915_fs_mov (FS_R3,
				 i915_fs_operand (mask_reg, W, W, W, W));
		} else {
		    i915_fs_mov (FS_R3,
				 i915_fs_operand_impure (mask_reg, W, source_pure));
		}
	    } else {
		i915_fs_mul (FS_R3,
			     i915_fs_operand_reg (source_reg),
			     i915_fs_operand (mask_reg, W, W, W, W));
	    }

	    i915_fs_add (mask_reg,
			 i915_fs_operand_one (),
			 i915_fs_operand (mask_reg, NEG_W, NEG_W, NEG_W, NEG_W));

	    if (dest_reg != FS_OC) {
		if (dest_reg == ~0U) {
		    assert (shader->dst.type.fragment == FS_TEXTURE);

		    i915_fs_texld (FS_R2, FS_S0 + sampler_offset, FS_T0 + texture_offset);
		    texture_offset += 1;
		    sampler_offset += 1;
		    dest_reg = FS_R2;
		}

		i915_fs_mul (FS_U1,
			     i915_fs_operand_reg (dest_reg),
			     i915_fs_operand_reg (mask_reg));
		mask_reg = FS_U1;
	    }

	    source_reg = FS_OC;
	    if ((shader->content & CAIRO_CONTENT_COLOR) == 0) {
		i915_fs_add (source_reg,
			     i915_fs_operand (FS_R3, W, W, W, W),
			     i915_fs_operand (mask_reg, W, W, W, W));
	    } else {
		i915_fs_add (source_reg,
			     i915_fs_operand_reg (FS_R3),
			     i915_fs_operand_reg (mask_reg));
	    }
	}
    }

    if (source_reg != FS_OC) {
	if (source_reg == ~0U) {
	    if (source_pure) {
		if ((shader->content & CAIRO_CONTENT_COLOR) == 0) {
		    if (source_pure & (1 << 3))
			i915_fs_mov (FS_OC, i915_fs_operand_one ());
		    else
			i915_fs_mov (FS_OC, i915_fs_operand_zero ());
		} else
		    i915_fs_mov (FS_OC, i915_fs_operand_pure (source_pure));
	    } else {
		i915_fs_mov (FS_OC, i915_fs_operand_one ());
	    }
	} else if ((shader->content & CAIRO_CONTENT_COLOR) == 0) {
	    i915_fs_mov (FS_OC, i915_fs_operand (source_reg, W, W, W, W));
	} else {
	    i915_fs_mov (FS_OC, i915_fs_operand_reg (source_reg));
	}
    }

    FS_END ();
}

static cairo_bool_t
i915_shader_linear_init (struct i915_shader_linear *l,
			 const cairo_linear_pattern_t *linear)
{
    double x0, y0, sf;
    double dx, dy, offset;

    dx = linear->pd2.x - linear->pd1.x;
    dy = linear->pd2.y - linear->pd1.y;
    sf = dx * dx + dy * dy;
    if (sf <= 1e-5)
	return FALSE;

    dx /= sf;
    dy /= sf;

    x0 = linear->pd1.x;
    y0 = linear->pd1.y;
    offset = dx*x0 + dy*y0;

    if (_cairo_matrix_is_identity (&linear->base.base.matrix)) {
	l->dx = dx;
	l->dy = dy;
	l->offset = -offset;
    } else {
	cairo_matrix_t m;

	cairo_matrix_init (&m, dx, 0, dy, 0, -offset, 0);
	cairo_matrix_multiply (&m, &linear->base.base.matrix, &m);
	l->dx = m.xx;
	l->dy = m.xy;
	l->offset = m.x0;
    }

    return TRUE;
}

static cairo_bool_t
i915_shader_linear_contains_rectangle (struct i915_shader_linear *l,
				       const cairo_rectangle_int_t *extents)
{
    double v;

    v = i915_shader_linear_texcoord (l,
				     extents->x,
				     extents->y);
    if (v < 0.)
	return FALSE;
    if (v > 1.)
	return FALSE;

    v = i915_shader_linear_texcoord (l,
				     extents->x + extents->width,
				     extents->y);
    if (v < 0.)
	return FALSE;
    if (v > 1.)
	return FALSE;

    v = i915_shader_linear_texcoord (l,
				     extents->x,
				     extents->y + extents->height);
    if (v < 0.)
	return FALSE;
    if (v > 1.)
	return FALSE;

    v = i915_shader_linear_texcoord (l,
				     extents->x + extents->width,
				     extents->y + extents->height);
    if (v < 0.)
	return FALSE;
    if (v > 1.)
	return FALSE;

    return TRUE;
}

#define is_pure(C,mask) (((mask) == 0) || (C) <= 0x00ff || (C) >= 0xff00)
#define is_one(C,mask) (((mask) != 0) && (C) >= 0xff00)
#define is_zero(C,mask) (((mask) != 0) && (C) <= 0x00ff)

static cairo_status_t
i915_shader_acquire_solid (i915_shader_t *shader,
			   union i915_shader_channel *src,
			   const cairo_solid_pattern_t *solid,
			   const cairo_rectangle_int_t *extents)
{
    cairo_content_t content;

    content = CAIRO_CONTENT_COLOR_ALPHA;
    src->solid.color = solid->color;
    if (content == 0 || solid->color.alpha_short <= 0x00ff)
    {
	src->base.content = CAIRO_CONTENT_ALPHA;
	src->type.fragment = FS_ZERO;
    }
    else if ((((content & CAIRO_CONTENT_COLOR) == 0)  ||
	      (solid->color.red_short >= 0xff00 &&
	       solid->color.green_short >= 0xff00 &&
	       solid->color.blue_short >= 0xff00)) &&
	     ((content & CAIRO_CONTENT_ALPHA) == 0 ||
	      solid->color.alpha_short >= 0xff00))
    {
	src->base.content = CAIRO_CONTENT_ALPHA;
	src->type.fragment = FS_ONE;
    }
    else if (is_pure (solid->color.red_short, content & CAIRO_CONTENT_COLOR) &&
	     is_pure (solid->color.green_short, content & CAIRO_CONTENT_COLOR) &&
	     is_pure (solid->color.blue_short, content & CAIRO_CONTENT_COLOR) &&
	     is_pure (solid->color.alpha_short, content & CAIRO_CONTENT_ALPHA))
    {
	src->solid.pure = 0;
	src->solid.pure |= is_one (solid->color.red_short,   content & CAIRO_CONTENT_COLOR) << 0;
	src->solid.pure |= is_one (solid->color.green_short, content & CAIRO_CONTENT_COLOR) << 1;
	src->solid.pure |= is_one (solid->color.blue_short,  content & CAIRO_CONTENT_COLOR) << 2;
	src->solid.pure |= (! is_zero (solid->color.alpha_short, content & CAIRO_CONTENT_ALPHA)) << 3;

	if (src->solid.pure == 0) {
	    src->base.content = CAIRO_CONTENT_ALPHA;
	    src->type.fragment = FS_ZERO;
	} else if (src->solid.pure == 0x7) {
	    src->base.content = CAIRO_CONTENT_ALPHA;
	    src->type.fragment = FS_ONE;
	} else {
	    src->base.content = content;
	    src->type.fragment = FS_PURE;
	    src->base.mode = src->solid.pure;
	}
    }
    else
    {
	src->base.content = content;
	src->type.fragment = src == &shader->source ? FS_DIFFUSE : FS_CONSTANT;
    }
    src->type.vertex = src->type.fragment == FS_ZERO ? VS_ZERO : VS_CONSTANT;
    src->type.pattern = PATTERN_CONSTANT;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
i915_shader_acquire_linear (i915_shader_t *shader,
			    union i915_shader_channel *src,
			    const cairo_linear_pattern_t *linear,
			    const cairo_rectangle_int_t *extents)
{
    cairo_bool_t mode = LINEAR_TEXTURE;
    cairo_status_t status;

    if (i915_shader_linear_init (&src->linear, linear) &&
	linear->base.n_stops == 2 &&
	linear->base.stops[0].offset == 0.0 &&
	linear->base.stops[1].offset == 1.0)
    {
	if (i915_shader_linear_contains_rectangle (&src->linear,
						   extents))
	{
	    /* XXX can also lerp if contained within offset range */
	    mode = LINEAR_NONE;
	}
	else switch (linear->base.base.extend) {
	case CAIRO_EXTEND_REPEAT:
	    mode = LINEAR_REPEAT;
	    break;
	case CAIRO_EXTEND_PAD:
	    mode = LINEAR_PAD;
	    break;
	case CAIRO_EXTEND_NONE:
	    break;
	case CAIRO_EXTEND_REFLECT:
	    break;
	default:
	    ASSERT_NOT_REACHED;
	    break;
	}
    }

    src->type.vertex = VS_LINEAR;
    src->type.pattern = PATTERN_LINEAR;
    src->base.texfmt = TEXCOORDFMT_1D;
    src->base.content = CAIRO_CONTENT_COLOR_ALPHA;
    src->base.mode = mode;
    if (mode == LINEAR_TEXTURE) {
	intel_buffer_t buffer;

	status = intel_gradient_render ((intel_device_t *) shader->target->intel.drm.base.device,
					&linear->base, &buffer);
	if (unlikely (status))
	    return status;

	src->type.fragment = FS_TEXTURE;
	src->base.bo = intel_bo_reference (buffer.bo);
	src->base.n_samplers = 1;
	src->base.offset[0] = buffer.offset;
	src->base.map[0] = buffer.map0;
	src->base.map[1] = buffer.map1;
	src->base.sampler[0] =
	    (MIPFILTER_NONE << SS2_MIP_FILTER_SHIFT) |
	    i915_texture_filter (CAIRO_FILTER_BILINEAR);
	src->base.sampler[1] =
	    SS3_NORMALIZED_COORDS |
	    i915_texture_extend (linear->base.base.extend);
    } else {
	src->type.fragment = FS_LINEAR;
	src->linear.color0.red   = linear->base.stops[0].color.red;
	src->linear.color0.green = linear->base.stops[0].color.green;
	src->linear.color0.blue  = linear->base.stops[0].color.blue;
	src->linear.color0.alpha = linear->base.stops[0].color.alpha;

	src->linear.color1.red   = linear->base.stops[1].color.red;
	src->linear.color1.green = linear->base.stops[1].color.green;
	src->linear.color1.blue  = linear->base.stops[1].color.blue;
	src->linear.color1.alpha = linear->base.stops[1].color.alpha;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
i915_shader_acquire_radial (i915_shader_t *shader,
			    union i915_shader_channel *src,
			    const cairo_radial_pattern_t *radial,
			    const cairo_rectangle_int_t *extents)
{
    intel_buffer_t buffer;
    cairo_status_t status;

    status = intel_gradient_render ((intel_device_t *) shader->target->intel.drm.base.device,
				    &radial->base, &buffer);
    if (unlikely (status))
	return status;

    i915_shader_radial_init (&src->radial, radial);

    src->type.vertex = VS_TEXTURE;
    src->type.fragment = FS_RADIAL;
    src->type.pattern = PATTERN_RADIAL;
    src->base.texfmt = TEXCOORDFMT_2D;

    src->base.content = CAIRO_CONTENT_COLOR_ALPHA;
    src->base.bo = intel_bo_reference (buffer.bo);
    src->base.n_samplers = 1;
    src->base.offset[0] = buffer.offset;
    src->base.map[0] = buffer.map0;
    src->base.map[1] = buffer.map1;
    src->base.sampler[0] =
	(MIPFILTER_NONE << SS2_MIP_FILTER_SHIFT) |
	i915_texture_filter (CAIRO_FILTER_BILINEAR);
    src->base.sampler[1] =
	SS3_NORMALIZED_COORDS |
	i915_texture_extend (radial->base.base.extend);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
i915_surface_clone (i915_device_t *device,
		    cairo_image_surface_t *image,
		    i915_surface_t **clone_out)
{
    i915_surface_t *clone;
    cairo_status_t status;

#if 0
    clone =
	i915_surface_create_from_cacheable_image_internal (device, image);
    if (unlikely (clone->intel.drm.base.status))
	return clone->intel.drm.base.status;
#else
    cairo_format_t format;

    format = image->format;
    if (format == CAIRO_FORMAT_A1)
	format = CAIRO_FORMAT_A8;

    clone = (i915_surface_t *)
	i915_surface_create_internal (&device->intel.base,
				      format,
				      image->width,
				      image->height,
				      I915_TILING_DEFAULT,
				      FALSE);
    if (unlikely (clone->intel.drm.base.status))
	return clone->intel.drm.base.status;

    status = intel_bo_put_image (&device->intel,
				 to_intel_bo (clone->intel.drm.bo),
				 image,
				 0, 0,
				 image->width, image->height,
				 0, 0);

    if (unlikely (status))
	return status;
#endif

    *clone_out = clone;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
i915_surface_clone_subimage (i915_device_t *device,
			     cairo_image_surface_t *image,
			     const cairo_rectangle_int_t *extents,
			     i915_surface_t **clone_out)
{
    i915_surface_t *clone;
    cairo_status_t status;
    cairo_format_t format;

    format = image->format;
    if (format == CAIRO_FORMAT_A1)
	format = CAIRO_FORMAT_A8;

    clone = (i915_surface_t *)
	i915_surface_create_internal (&device->intel.base,
				      format,
				      extents->width,
				      extents->height,
				      I915_TILING_NONE,
				      FALSE);
    if (unlikely (clone->intel.drm.base.status))
	return clone->intel.drm.base.status;

    status = intel_bo_put_image (&device->intel,
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
i915_surface_render_pattern (i915_device_t *device,
			     const cairo_surface_pattern_t *pattern,
			     const cairo_rectangle_int_t *extents,
			     i915_surface_t **clone_out)
{
    i915_surface_t *clone;
    cairo_surface_t *image;
    cairo_status_t status;
    void *ptr;

    clone = (i915_surface_t *)
	i915_surface_create_internal (&device->intel.base,
				      _cairo_format_from_content (pattern->surface->content),
				      extents->width,
				      extents->height,
				      I915_TILING_NONE,
				      FALSE);
    if (unlikely (clone->intel.drm.base.status))
	return clone->intel.drm.base.status;

    ptr = intel_bo_map (&device->intel,
			to_intel_bo (clone->intel.drm.bo));
    if (unlikely (ptr == NULL)) {
	cairo_surface_destroy (&clone->intel.drm.base);
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    image = cairo_image_surface_create_for_data (ptr,
						 clone->intel.drm.format,
						 clone->intel.drm.width,
						 clone->intel.drm.height,
						 clone->intel.drm.stride);
    if (unlikely (image->status)) {
	cairo_surface_destroy (&clone->intel.drm.base);
	return image->status;
    }

    status = _cairo_surface_offset_paint (image,
					  extents->x, extents->y,
					  CAIRO_OPERATOR_SOURCE,
					  &pattern->base,
					  NULL);
    cairo_surface_destroy (image);

    if (unlikely (status)) {
	cairo_surface_destroy (&clone->intel.drm.base);
	return status;
    }

    *clone_out = clone;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
i915_shader_acquire_solid_surface (i915_shader_t *shader,
				   union i915_shader_channel *src,
				   cairo_surface_t *surface,
				   const cairo_rectangle_int_t *extents)
{
    cairo_surface_pattern_t pattern;
    cairo_surface_t *pixel;
    cairo_image_surface_t *image;
    void *image_extra;
    cairo_status_t status;
    uint32_t argb;

    status = _cairo_surface_acquire_source_image (surface, &image, &image_extra);
    if (unlikely (status))
	return status;

    /* extract the pixel as argb32 */
    pixel = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 1, 1);
    _cairo_pattern_init_for_surface (&pattern, &image->base);
    cairo_matrix_init_translate (&pattern.base.matrix, extents->x, extents->y);
    pattern.base.filter = CAIRO_FILTER_NEAREST;
    status = _cairo_surface_paint (pixel, CAIRO_OPERATOR_SOURCE, &pattern.base, NULL);
    _cairo_pattern_fini (&pattern.base);

    _cairo_surface_release_source_image (surface, image, image_extra);

    if (unlikely (status)) {
	cairo_surface_destroy (pixel);
	return status;
    }

    image = (cairo_image_surface_t *) pixel;
    argb = *(uint32_t *) image->data;
    cairo_surface_destroy (pixel);

    if (argb >> 24 == 0) {
	_cairo_color_init_rgba (&src->solid.color, 0, 0, 0, 0);
    } else {
	uint8_t alpha = argb >> 24;

	_cairo_color_init_rgba (&src->solid.color,
				((((argb >> 16) & 0xff) * 255 + alpha / 2) / alpha) / 255.,
				((((argb >>  8) & 0xff) * 255 + alpha / 2) / alpha) / 255.,
				((((argb >>  0) & 0xff) * 255 + alpha / 2) / alpha) / 255.,
				alpha / 255.);
    }

    src->base.content = CAIRO_CONTENT_COLOR_ALPHA;
    src->type.fragment = FS_CONSTANT;
    src->type.vertex = VS_CONSTANT;
    src->type.pattern = PATTERN_CONSTANT;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
i915_shader_acquire_surface (i915_shader_t *shader,
			     union i915_shader_channel *src,
			     const cairo_surface_pattern_t *pattern,
			     const cairo_rectangle_int_t *extents)
{
    int surface_width, surface_height;
    cairo_surface_t *surface, *drm;
    cairo_extend_t extend;
    cairo_filter_t filter;
    cairo_matrix_t m;
    int src_x = 0, src_y = 0;
    cairo_surface_t *free_me = NULL;
    cairo_status_t status;
    cairo_rectangle_int_t sample;

    assert (src->type.fragment == (i915_fragment_shader_t) -1);
    drm = surface = pattern->surface;

    extend = pattern->base.extend;
    src->base.matrix = pattern->base.matrix;
    filter = pattern->base.filter;
    _cairo_pattern_sampled_area(&pattern->base, extents, sample);

    if (surface->type == CAIRO_SURFACE_TYPE_DRM) {
	if (surface->backend->type == CAIRO_SURFACE_TYPE_SUBSURFACE) {
	    drm = ((cairo_surface_subsurface_t *) surface)->target;
	} else if (surface->backend->type == CAIRO_INTERNAL_SURFACE_TYPE_SNAPSHOT) {
	    drm = ((cairo_surface_snapshot_t *) surface)->target;
	}
    }

    if (drm->type == CAIRO_SURFACE_TYPE_DRM) {
	i915_surface_t *s = (i915_surface_t *) drm;

	if (surface->backend->type == CAIRO_SURFACE_TYPE_SUBSURFACE) {
	    if (s->intel.drm.base.device == shader->target->intel.drm.base.device &&
		s != shader->target)
	    {
		cairo_surface_subsurface_t *sub = (cairo_surface_subsurface_t *) surface;
		int x;

		status = i915_surface_fallback_flush (s);
		if (unlikely (status))
		    return status;

		/* XXX blt subimage and cache snapshot */

		if (to_intel_bo (s->intel.drm.bo)->batch_write_domain) {
		    /* XXX pipelined flush of RENDER/TEXTURE cache */
		}

		src->type.fragment = FS_TEXTURE;
		src->surface.pixel = NONE;
		surface_width  = sub->extents.width;
		surface_height = sub->extents.height;

		src->base.bo = intel_bo_reference (to_intel_bo (s->intel.drm.bo));
		src->base.n_samplers = 1;

		x = sub->extents.x;
		if (s->intel.drm.format != CAIRO_FORMAT_A8)
		    x *= 4;

		/* XXX tiling restrictions upon offset? */
		src->base.offset[0] = s->offset + sub->extents.y * s->intel.drm.stride + x;
		src->base.map[0] = s->map0;
		src->base.map[0] &= ~((2047 << MS3_HEIGHT_SHIFT) | (2047 << MS3_WIDTH_SHIFT));
		src->base.map[0] |=
		    ((sub->extents.height - 1) << MS3_HEIGHT_SHIFT) |
		    ((sub->extents.width - 1)  << MS3_WIDTH_SHIFT);
		src->base.map[1] = (s->intel.drm.stride / 4 - 1) << MS4_PITCH_SHIFT;
	    }
	} else {
	    /* XXX if s == shader->dst allow if FILTER_NEAREST, EXTEND_NONE? */
	    if (s->intel.drm.base.device == shader->target->intel.drm.base.device) {
		status = i915_surface_fallback_flush (s);
		if (unlikely (status))
		    return status;

		if (s == shader->target || i915_surface_needs_tiling (s)) {
		    status = i915_surface_copy_subimage (i915_device (shader->target),
							 s, &sample, TRUE, &s);
		    if (unlikely (status))
			return status;

		    free_me = drm = &s->intel.drm.base;
		}

		src->type.fragment = FS_TEXTURE;
		src->surface.pixel = NONE;

		surface_width  = s->intel.drm.width;
		surface_height = s->intel.drm.height;

		src->base.bo = intel_bo_reference (to_intel_bo (s->intel.drm.bo));
		src->base.n_samplers = 1;
		src->base.offset[0] = s->offset;
		src->base.map[0] = s->map0;
		src->base.map[1] = s->map1;
	    }
	}
    }

    if (src->type.fragment == (i915_fragment_shader_t) -1) {
	i915_surface_t *s;

	if (extents->width == 1 && extents->height == 1) {
	    return i915_shader_acquire_solid_surface (shader, src,
						      surface, extents);
	}

	s = (i915_surface_t *)
	    _cairo_surface_has_snapshot (surface,
					 shader->target->intel.drm.base.backend);
	if (s == NULL) {
	    cairo_status_t status;

#if 0
	    /* XXX hackity hack hack */
	    status = i915_clone_yuv (surface, src,
				     image->width, image->height,
				     clone_out);
#endif

	    if (sample.width > 2048 || sample.height > 2048) {
		status = i915_surface_render_pattern (i915_device (shader->target),
						      pattern, extents,
						      &s);
		if (unlikely (status))
		    return status;

		extend = CAIRO_EXTEND_NONE;
		filter = CAIRO_FILTER_NEAREST;
		cairo_matrix_init_translate (&src->base.matrix,
					     -extents->x, -extents->y);
	    } else {
		cairo_image_surface_t *image;
		void *image_extra;

		status = _cairo_surface_acquire_source_image (surface, &image, &image_extra);
		if (unlikely (status))
		    return status;

		if (image->width  < 2048 &&
		    image->height < 2048 &&
		    sample.width  >= image->width / 4 &&
		    sample.height >= image->height /4)
		{

		    status = i915_surface_clone (i915_device (shader->target),
						 image, &s);

		    if (likely (status == CAIRO_STATUS_SUCCESS)) {
			_cairo_surface_attach_snapshot (surface,
							&s->intel.drm.base,
							intel_surface_detach_snapshot);

			status = intel_snapshot_cache_insert (&i915_device (shader->target)->intel,
							      &s->intel);
			if (unlikely (status)) {
			    cairo_surface_finish (&s->intel.drm.base);
			    cairo_surface_destroy (&s->intel.drm.base);
			}
		    }
		}
		else
		{
		    status = i915_surface_clone_subimage (i915_device (shader->target),
							  image, &sample, &s);
		    src_x = -extents->x;
		    src_y = -extents->y;
		}

		_cairo_surface_release_source_image (surface, image, image_extra);
		if (unlikely (status))
		    return status;
	    }

	    free_me = &s->intel.drm.base;
	}

	src->type.fragment = FS_TEXTURE;
	src->surface.pixel = NONE;

	src->base.bo = intel_bo_reference (to_intel_bo (s->intel.drm.bo));
	src->base.n_samplers = 1;
	src->base.offset[0] = s->offset;
	src->base.map[0] = s->map0;
	src->base.map[1] = s->map1;

	drm = &s->intel.drm.base;

	surface_width  = s->intel.drm.width;
	surface_height = s->intel.drm.height;
    }

    /* XXX transform nx1 or 1xn surfaces to 1D */

    src->type.pattern = PATTERN_TEXTURE;
    if (extend != CAIRO_EXTEND_NONE &&
	sample.x >= 0 && sample.y >= 0 &&
	sample.x + sample.width  <= surface_width &&
	sample.y + sample.height <= surface_height)
    {
	extend = CAIRO_EXTEND_NONE;
    }
    if (extend == CAIRO_EXTEND_NONE) {
	src->type.vertex = VS_TEXTURE_16;
	src->base.texfmt = TEXCOORDFMT_2D_16;
    } else {
	src->type.vertex = VS_TEXTURE;
	src->base.texfmt = TEXCOORDFMT_2D;
    }
    src->base.content = drm->content;

    src->base.sampler[0] =
	(MIPFILTER_NONE << SS2_MIP_FILTER_SHIFT) |
	i915_texture_filter (filter);
    src->base.sampler[1] =
	SS3_NORMALIZED_COORDS |
	i915_texture_extend (extend);

    /* tweak the src matrix to map from dst to texture coordinates */
    if (src_x | src_y)
	cairo_matrix_translate (&src->base.matrix, src_x, src_x);
    cairo_matrix_init_scale (&m, 1. / surface_width, 1. / surface_height);
    cairo_matrix_multiply (&src->base.matrix, &src->base.matrix, &m);

    if (free_me != NULL)
	cairo_surface_destroy (free_me);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
i915_shader_acquire_pattern (i915_shader_t *shader,
			     union i915_shader_channel *src,
			     const cairo_pattern_t *pattern,
			     const cairo_rectangle_int_t *extents)
{
    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	return i915_shader_acquire_solid (shader, src,
					  (cairo_solid_pattern_t *) pattern,
					  extents);

    case CAIRO_PATTERN_TYPE_LINEAR:
	return i915_shader_acquire_linear (shader, src,
					   (cairo_linear_pattern_t *) pattern,
					   extents);

    case CAIRO_PATTERN_TYPE_RADIAL:
	return i915_shader_acquire_radial (shader, src,
					   (cairo_radial_pattern_t *) pattern,
					   extents);

    case CAIRO_PATTERN_TYPE_SURFACE:
	return i915_shader_acquire_surface (shader, src,
					    (cairo_surface_pattern_t *) pattern,
					    extents);

    default:
	ASSERT_NOT_REACHED;
	return CAIRO_STATUS_SUCCESS;
    }
}

static uint32_t
i915_get_blend (cairo_operator_t op,
		i915_surface_t *dst)
{
#define SBLEND(X) ((BLENDFACT_##X) << S6_CBUF_SRC_BLEND_FACT_SHIFT)
#define DBLEND(X) ((BLENDFACT_##X) << S6_CBUF_DST_BLEND_FACT_SHIFT)
    static const struct blendinfo {
	cairo_bool_t dst_alpha;
	uint32_t src_blend;
	uint32_t dst_blend;
	enum {
	    BOUNDED,
	    SIMPLE,
	    XRENDER,
	} kind;
    } i915_blend_op[] = {
	{0, SBLEND (ZERO),          DBLEND (ZERO), BOUNDED}, /* Clear */
	{0, SBLEND (ONE),           DBLEND (ZERO), BOUNDED}, /* Src */

	{0, SBLEND (ONE),           DBLEND (INV_SRC_ALPHA), SIMPLE}, /* Over */
	{1, SBLEND (DST_ALPHA),     DBLEND (ZERO), XRENDER}, /* In */
	{1, SBLEND (INV_DST_ALPHA), DBLEND (ZERO), XRENDER}, /* Out */
	{1, SBLEND (DST_ALPHA),     DBLEND (INV_SRC_ALPHA), SIMPLE}, /* Atop */

	{0, SBLEND (ZERO),          DBLEND (ONE), SIMPLE}, /* Dst */
	{1, SBLEND (INV_DST_ALPHA), DBLEND (ONE), SIMPLE}, /* OverReverse */
	{0, SBLEND (ZERO),          DBLEND (SRC_ALPHA), XRENDER}, /* InReverse */
	{0, SBLEND (ZERO),          DBLEND (INV_SRC_ALPHA), SIMPLE}, /* OutReverse */
	{1, SBLEND (INV_DST_ALPHA), DBLEND (SRC_ALPHA), XRENDER}, /* AtopReverse */

	{1, SBLEND (INV_DST_ALPHA), DBLEND (INV_SRC_ALPHA), SIMPLE}, /* Xor */
	{0, SBLEND (ONE),           DBLEND (ONE), SIMPLE}, /* Add */
	//{0, 0, SBLEND (SRC_ALPHA_SATURATE),	    DBLEND (ONE), SIMPLE}, /* XXX Saturate */
    };
    uint32_t sblend, dblend;

    if (op >= ARRAY_LENGTH (i915_blend_op))
	return 0;

    if (i915_blend_op[op].kind == BOUNDED)
	return 0;

    sblend = i915_blend_op[op].src_blend;
    dblend = i915_blend_op[op].dst_blend;

    /* If there's no dst alpha channel, adjust the blend op so that we'll treat
     * it as always 1.
     */
    if ((dst->intel.drm.base.content & CAIRO_CONTENT_ALPHA) == 0 &&
	i915_blend_op[op].dst_alpha)
    {
	if (sblend == SBLEND (DST_ALPHA))
	    sblend = SBLEND (ONE);
	else if (sblend == SBLEND (INV_DST_ALPHA))
	    sblend = SBLEND (ZERO);
    }

    /* i915 engine reads 8bit color buffer into green channel in cases
       like color buffer blending etc., and also writes back green channel.
       So with dst_alpha blend we should use color factor. See spec on
       "8-bit rendering" */
    if (dst->intel.drm.format == CAIRO_FORMAT_A8 && i915_blend_op[op].dst_alpha) {
	if (sblend == SBLEND (DST_ALPHA))
	    sblend = SBLEND (DST_COLR);
	else if (sblend == SBLEND (INV_DST_ALPHA))
	    sblend = SBLEND (INV_DST_COLR);
    }

    return sblend | dblend;
#undef SBLEND
#undef DBLEND
}

static void
i915_shader_channel_init (union i915_shader_channel *channel)
{
    channel->type.vertex = (i915_vertex_shader_t) -1;
    channel->type.fragment = (i915_fragment_shader_t) -1;
    channel->type.pattern = (i915_shader_channel_t) -1;
    channel->base.texfmt = TEXCOORDFMT_NOT_PRESENT;
    channel->base.bo = NULL;
    channel->base.n_samplers = 0;
    channel->base.mode = 0;
}

static void
i915_shader_channel_fini (i915_device_t *device,
			   union i915_shader_channel *channel)
{
    switch (channel->type.pattern) {
    case PATTERN_TEXTURE:
    case PATTERN_BASE:
    case PATTERN_LINEAR:
    case PATTERN_RADIAL:
	if (channel->base.bo != NULL)
	    intel_bo_destroy (&device->intel, channel->base.bo);
	break;

    default:
    case PATTERN_CONSTANT:
	break;
    }
}

static void
i915_shader_channel_reset (i915_device_t *device,
			   union i915_shader_channel *channel)
{
    i915_shader_channel_fini (device, channel);
    i915_shader_channel_init (channel);
}

void
i915_shader_init (i915_shader_t *shader,
		  i915_surface_t *dst,
		  cairo_operator_t op,
		  double opacity)
{
    shader->committed = FALSE;
    shader->device = i915_device (dst);
    shader->target = dst;
    shader->op = op;
    shader->opacity = opacity;

    shader->blend = i915_get_blend (op, dst);
    shader->need_combine = FALSE;

    shader->content = dst->intel.drm.base.content;

    i915_shader_channel_init (&shader->source);
    i915_shader_channel_init (&shader->mask);
    i915_shader_channel_init (&shader->clip);
    i915_shader_channel_init (&shader->dst);
}

static void
i915_set_shader_samplers (i915_device_t *device,
	                  const i915_shader_t *shader)
{
    uint32_t n_samplers, n_maps, n;
    uint32_t samplers[2*4];
    uint32_t maps[4*4];
    uint32_t mask, s, m;

    n_maps =
	shader->source.base.n_samplers +
	shader->mask.base.n_samplers +
	shader->clip.base.n_samplers +
	shader->dst.base.n_samplers;
    assert (n_maps <= 4);

    if (n_maps == 0)
	return;

    n_samplers =
	!! shader->source.base.bo +
	!! shader->mask.base.bo +
	!! shader->clip.base.bo +
	!! shader->dst.base.bo;

    mask  = (1 << n_maps) - 1;

    /* We check for repeated setting of sample state mainly to catch
     * continuation of text strings across multiple show-glyphs.
     */
    s = m = 0;
    if (shader->source.base.bo != NULL) {
	samplers[s++] = shader->source.base.sampler[0];
	samplers[s++] = shader->source.base.sampler[1];
	maps[m++] = shader->source.base.bo->base.handle;
	for (n = 0; n < shader->source.base.n_samplers; n++) {
	    maps[m++] = shader->source.base.offset[n];
	    maps[m++] = shader->source.base.map[2*n+0];
	    maps[m++] = shader->source.base.map[2*n+1];
	}
    }
    if (shader->mask.base.bo != NULL) {
	samplers[s++] = shader->mask.base.sampler[0];
	samplers[s++] = shader->mask.base.sampler[1];
	maps[m++] = shader->mask.base.bo->base.handle;
	for (n = 0; n < shader->mask.base.n_samplers; n++) {
	    maps[m++] = shader->mask.base.offset[n];
	    maps[m++] = shader->mask.base.map[2*n+0];
	    maps[m++] = shader->mask.base.map[2*n+1];
	}
    }
    if (shader->clip.base.bo != NULL) {
	samplers[s++] = shader->clip.base.sampler[0];
	samplers[s++] = shader->clip.base.sampler[1];
	maps[m++] = shader->clip.base.bo->base.handle;
	for (n = 0; n < shader->clip.base.n_samplers; n++) {
	    maps[m++] = shader->clip.base.offset[n];
	    maps[m++] = shader->clip.base.map[2*n+0];
	    maps[m++] = shader->clip.base.map[2*n+1];
	}
    }
    if (shader->dst.base.bo != NULL) {
	samplers[s++] = shader->dst.base.sampler[0];
	samplers[s++] = shader->dst.base.sampler[1];
	maps[m++] = shader->dst.base.bo->base.handle;
	for (n = 0; n < shader->dst.base.n_samplers; n++) {
	    maps[m++] = shader->dst.base.offset[n];
	    maps[m++] = shader->dst.base.map[2*n+0];
	    maps[m++] = shader->dst.base.map[2*n+1];
	}
    }

    if (n_maps > device->current_n_maps ||
	memcmp (device->current_maps,
		maps,
		m * sizeof (uint32_t)))
    {
	memcpy (device->current_maps, maps, m * sizeof (uint32_t));
	device->current_n_maps = n_maps;

	if (device->current_source != NULL)
	    *device->current_source = 0;
	if (device->current_mask != NULL)
	    *device->current_mask = 0;
	if (device->current_clip != NULL)
	    *device->current_clip = 0;

#if 0
	if (shader->source.type.pattern == PATTERN_TEXTURE) {
	    switch ((int) shader->source.surface.surface->type) {
	    case CAIRO_SURFACE_TYPE_DRM:
		{
		    i915_surface_t *surface =
			(i915_surface_t *) shader->source.surface.surface;
		    device->current_source = &surface->is_current_texture;
		    surface->is_current_texture |= CURRENT_SOURCE;
		    break;
		}

	    case I915_PACKED_PIXEL_SURFACE_TYPE:
		{
		    i915_packed_pixel_surface_t *surface =
			(i915_packed_pixel_surface_t *) shader->source.surface.surface;
		    device->current_source = &surface->is_current_texture;
		    surface->is_current_texture |= CURRENT_SOURCE;
		    break;
		}

	    default:
		device->current_source = NULL;
		break;
	    }
	} else
	    device->current_source = NULL;

	if (shader->mask.type.pattern == PATTERN_TEXTURE) {
	    switch ((int) shader->mask.surface.surface->type) {
	    case CAIRO_SURFACE_TYPE_DRM:
		{
		    i915_surface_t *surface =
			(i915_surface_t *) shader->mask.surface.surface;
		    device->current_mask = &surface->is_current_texture;
		    surface->is_current_texture |= CURRENT_MASK;
		    break;
		}

	    case I915_PACKED_PIXEL_SURFACE_TYPE:
		{
		    i915_packed_pixel_surface_t *surface =
			(i915_packed_pixel_surface_t *) shader->mask.surface.surface;
		    device->current_mask = &surface->is_current_texture;
		    surface->is_current_texture |= CURRENT_MASK;
		    break;
		}

	    default:
		device->current_mask = NULL;
		break;
	    }
	} else
	    device->current_mask = NULL;
#endif

	OUT_DWORD (_3DSTATE_MAP_STATE | (3 * n_maps));
	OUT_DWORD (mask);
	for (n = 0; n < shader->source.base.n_samplers; n++) {
	    i915_batch_emit_reloc (device, shader->source.base.bo,
				   shader->source.base.offset[n],
				   I915_GEM_DOMAIN_SAMPLER, 0,
				   FALSE);
	    OUT_DWORD (shader->source.base.map[2*n+0]);
	    OUT_DWORD (shader->source.base.map[2*n+1]);
	}
	for (n = 0; n < shader->mask.base.n_samplers; n++) {
	    i915_batch_emit_reloc (device, shader->mask.base.bo,
				   shader->mask.base.offset[n],
				   I915_GEM_DOMAIN_SAMPLER, 0,
				   FALSE);
	    OUT_DWORD (shader->mask.base.map[2*n+0]);
	    OUT_DWORD (shader->mask.base.map[2*n+1]);
	}
	for (n = 0; n < shader->clip.base.n_samplers; n++) {
	    i915_batch_emit_reloc (device, shader->clip.base.bo,
				   shader->clip.base.offset[n],
				   I915_GEM_DOMAIN_SAMPLER, 0,
				   FALSE);
	    OUT_DWORD (shader->clip.base.map[2*n+0]);
	    OUT_DWORD (shader->clip.base.map[2*n+1]);
	}
	for (n = 0; n < shader->dst.base.n_samplers; n++) {
	    i915_batch_emit_reloc (device, shader->dst.base.bo,
				   shader->dst.base.offset[n],
				   I915_GEM_DOMAIN_SAMPLER, 0,
				   FALSE);
	    OUT_DWORD (shader->dst.base.map[2*n+0]);
	    OUT_DWORD (shader->dst.base.map[2*n+1]);
	}
    }

    if (n_samplers > device->current_n_samplers ||
	memcmp (device->current_samplers,
		samplers,
		s * sizeof (uint32_t)))
    {
	device->current_n_samplers = s;
	memcpy (device->current_samplers, samplers, s * sizeof (uint32_t));

	OUT_DWORD (_3DSTATE_SAMPLER_STATE | (3 * n_maps));
	OUT_DWORD (mask);
	s = 0;
	for (n = 0; n < shader->source.base.n_samplers; n++) {
	    OUT_DWORD (shader->source.base.sampler[0]);
	    OUT_DWORD (shader->source.base.sampler[1] |
		       (s << SS3_TEXTUREMAP_INDEX_SHIFT));
	    OUT_DWORD (0x0);
	    s++;
	}
	for (n = 0; n < shader->mask.base.n_samplers; n++) {
	    OUT_DWORD (shader->mask.base.sampler[0]);
	    OUT_DWORD (shader->mask.base.sampler[1] |
		       (s << SS3_TEXTUREMAP_INDEX_SHIFT));
	    OUT_DWORD (0x0);
	    s++;
	}
	for (n = 0; n < shader->clip.base.n_samplers; n++) {
	    OUT_DWORD (shader->clip.base.sampler[0]);
	    OUT_DWORD (shader->clip.base.sampler[1] |
		       (s << SS3_TEXTUREMAP_INDEX_SHIFT));
	    OUT_DWORD (0x0);
	    s++;
	}
	for (n = 0; n < shader->dst.base.n_samplers; n++) {
	    OUT_DWORD (shader->dst.base.sampler[0]);
	    OUT_DWORD (shader->dst.base.sampler[1] |
		       (s << SS3_TEXTUREMAP_INDEX_SHIFT));
	    OUT_DWORD (0x0);
	    s++;
	}
    }
}

static uint32_t
i915_shader_get_texcoords (const i915_shader_t *shader)
{
    uint32_t texcoords;
    uint32_t tu;

    texcoords = S2_TEXCOORD_NONE;
    tu = 0;
    if (shader->source.base.texfmt != TEXCOORDFMT_NOT_PRESENT) {
	texcoords &= ~S2_TEXCOORD_FMT (tu, S2_TEXCOORD_FMT0_MASK);
	texcoords |= S2_TEXCOORD_FMT (tu, shader->source.base.texfmt);
	tu++;
    }
    if (shader->mask.base.texfmt != TEXCOORDFMT_NOT_PRESENT) {
	texcoords &= ~S2_TEXCOORD_FMT (tu, S2_TEXCOORD_FMT0_MASK);
	texcoords |= S2_TEXCOORD_FMT (tu, shader->mask.base.texfmt);
	tu++;
    }
    if (shader->clip.base.texfmt != TEXCOORDFMT_NOT_PRESENT) {
	texcoords &= ~S2_TEXCOORD_FMT (tu, S2_TEXCOORD_FMT0_MASK);
	texcoords |= S2_TEXCOORD_FMT (tu, shader->clip.base.texfmt);
	tu++;
    }
    if (shader->dst.base.texfmt != TEXCOORDFMT_NOT_PRESENT) {
	texcoords &= ~S2_TEXCOORD_FMT (tu, S2_TEXCOORD_FMT0_MASK);
	texcoords |= S2_TEXCOORD_FMT (tu, shader->dst.base.texfmt);
	tu++;
    }

    return texcoords;
}

static void
i915_set_shader_mode (i915_device_t *device,
	              const i915_shader_t *shader)
{
    uint32_t texcoords;
    uint32_t mask, cnt;

    texcoords = i915_shader_get_texcoords (shader);

    mask = cnt = 0;

    if (device->current_texcoords != texcoords)
	mask |= I1_LOAD_S (2), cnt++;

    if (device->current_blend != shader->blend)
	mask |= I1_LOAD_S (6), cnt++;

    if (cnt == 0)
	return;

    OUT_DWORD (_3DSTATE_LOAD_STATE_IMMEDIATE_1 | mask | (cnt-1));

    if (device->current_texcoords != texcoords) {
	OUT_DWORD (texcoords);
	device->current_texcoords = texcoords;
    }

    if (device->current_blend != shader->blend) {
	if (shader->blend) {
	    OUT_DWORD (S6_CBUF_BLEND_ENABLE | S6_COLOR_WRITE_ENABLE |
		       (BLENDFUNC_ADD << S6_CBUF_BLEND_FUNC_SHIFT) |
		       shader->blend);
	} else {
	    OUT_DWORD (S6_COLOR_WRITE_ENABLE);
	}

	device->current_blend = shader->blend;
    }
}

static void
i915_set_constants (i915_device_t *device,
		    const uint32_t *constants,
		    uint32_t n_constants)
{
    uint32_t n;

    OUT_DWORD (_3DSTATE_PIXEL_SHADER_CONSTANTS | n_constants);
    OUT_DWORD ((1 << (n_constants >> 2)) - 1);

    for (n = 0; n < n_constants; n++)
	OUT_DWORD (constants[n]);

    device->current_n_constants = n_constants;
    memcpy (device->current_constants, constants, n_constants*4);
}

static uint32_t
pack_constants (const union i915_shader_channel *channel,
		uint32_t *constants)
{
    uint32_t count = 0, n;

    switch (channel->type.fragment) {
    case FS_ZERO:
    case FS_ONE:
    case FS_PURE:
    case FS_DIFFUSE:
	break;

    case FS_CONSTANT:
	constants[count++] = pack_float (channel->solid.color.red);
	constants[count++] = pack_float (channel->solid.color.green);
	constants[count++] = pack_float (channel->solid.color.blue);
	constants[count++] = pack_float (channel->solid.color.alpha);
	break;

    case FS_LINEAR:
	constants[count++] = pack_float (channel->linear.color0.red);
	constants[count++] = pack_float (channel->linear.color0.green);
	constants[count++] = pack_float (channel->linear.color0.blue);
	constants[count++] = pack_float (channel->linear.color0.alpha);

	constants[count++] = pack_float (channel->linear.color1.red);
	constants[count++] = pack_float (channel->linear.color1.green);
	constants[count++] = pack_float (channel->linear.color1.blue);
	constants[count++] = pack_float (channel->linear.color1.alpha);
	break;

    case FS_RADIAL:
	for (n = 0; n < ARRAY_LENGTH (channel->radial.constants); n++)
	    constants[count++] = pack_float (channel->radial.constants[n]);
	break;

    case FS_TEXTURE:
    case FS_YUV:
    case FS_SPANS:
	break;
    }

    return count;
}

static void
i915_set_shader_constants (i915_device_t *device,
	                   const i915_shader_t *shader)
{
    uint32_t constants[4*4*3+4];
    unsigned n_constants;

    n_constants = 0;
    if (shader->source.type.fragment == FS_DIFFUSE) {
	uint32_t diffuse;

	diffuse =
	    ((uint32_t)(shader->source.solid.color.alpha_short >> 8) << 24) |
	    ((shader->source.solid.color.red_short   >> 8) << 16) |
	    ((shader->source.solid.color.green_short >> 8) << 8) |
	    ((shader->source.solid.color.blue_short  >> 8) << 0);

	if (diffuse != device->current_diffuse) {
	    OUT_DWORD (_3DSTATE_DFLT_DIFFUSE_CMD);
	    OUT_DWORD (diffuse);
	    device->current_diffuse = diffuse;
	}
    } else {
	n_constants += pack_constants (&shader->source, constants + n_constants);
    }
    n_constants += pack_constants (&shader->mask, constants + n_constants);

    if (shader->opacity < 1.) {
	constants[n_constants+0] =
	    constants[n_constants+1] =
	    constants[n_constants+2] =
	    constants[n_constants+3] = pack_float (shader->opacity);
	n_constants += 4;
    }

    if (n_constants != 0 &&
	(device->current_n_constants != n_constants ||
	 memcmp (device->current_constants, constants, n_constants*4)))
    {
	i915_set_constants (device, constants, n_constants);
    }
}

static cairo_bool_t
i915_shader_needs_update (const i915_shader_t *shader,
			  const i915_device_t *device)
{
    uint32_t count, n;
    uint32_t buf[64];

    if (device->current_target != shader->target)
	return TRUE;

    count =
	!! shader->source.base.bo +
	!! shader->mask.base.bo +
	!! shader->clip.base.bo +
	!! shader->dst.base.bo;
    if (count > device->current_n_samplers)
	return TRUE;

    count =
	shader->source.base.n_samplers +
	shader->mask.base.n_samplers +
	shader->clip.base.n_samplers +
	shader->dst.base.n_samplers;
    if (count > device->current_n_maps)
	return TRUE;

    if (count) {
	count = 0;
	if (shader->source.base.bo != NULL) {
	    buf[count++] = shader->source.base.sampler[0];
	    buf[count++] = shader->source.base.sampler[1];
	}
	if (shader->mask.base.bo != NULL) {
	    buf[count++] = shader->mask.base.sampler[0];
	    buf[count++] = shader->mask.base.sampler[1];
	}
	if (shader->clip.base.bo != NULL) {
	    buf[count++] = shader->clip.base.sampler[0];
	    buf[count++] = shader->clip.base.sampler[1];
	}
	if (shader->dst.base.bo != NULL) {
	    buf[count++] = shader->dst.base.sampler[0];
	    buf[count++] = shader->dst.base.sampler[1];
	}
	if (memcmp (device->current_samplers, buf, count * sizeof (uint32_t)))
	    return TRUE;

	count = 0;
	if (shader->source.base.bo != NULL) {
	    buf[count++] = shader->source.base.bo->base.handle;
	    for (n = 0; n < shader->source.base.n_samplers; n++) {
		buf[count++] = shader->source.base.offset[n];
		buf[count++] = shader->source.base.map[2*n+0];
		buf[count++] = shader->source.base.map[2*n+1];
	    }
	}
	if (shader->mask.base.bo != NULL) {
	    buf[count++] = shader->mask.base.bo->base.handle;
	    for (n = 0; n < shader->mask.base.n_samplers; n++) {
		buf[count++] = shader->mask.base.offset[n];
		buf[count++] = shader->mask.base.map[2*n+0];
		buf[count++] = shader->mask.base.map[2*n+1];
	    }
	}
	if (shader->clip.base.bo != NULL) {
	    buf[count++] = shader->clip.base.bo->base.handle;
	    for (n = 0; n < shader->clip.base.n_samplers; n++) {
		buf[count++] = shader->clip.base.offset[n];
		buf[count++] = shader->clip.base.map[2*n+0];
		buf[count++] = shader->clip.base.map[2*n+1];
	    }
	}
	if (shader->dst.base.bo != NULL) {
	    buf[count++] = shader->dst.base.bo->base.handle;
	    for (n = 0; n < shader->dst.base.n_samplers; n++) {
		buf[count++] = shader->dst.base.offset[n];
		buf[count++] = shader->dst.base.map[2*n+0];
		buf[count++] = shader->dst.base.map[2*n+1];
	    }
	}
	if (memcmp (device->current_maps, buf, count * sizeof (uint32_t)))
	    return TRUE;
    }

    if (i915_shader_get_texcoords (shader) != device->current_texcoords)
	return TRUE;
    if (device->current_blend != shader->blend)
	return TRUE;

    count = 0;
    if (shader->source.type.fragment == FS_DIFFUSE) {
	uint32_t diffuse;

	diffuse =
	    ((uint32_t)(shader->source.solid.color.alpha_short >> 8) << 24) |
	    ((shader->source.solid.color.red_short   >> 8) << 16) |
	    ((shader->source.solid.color.green_short >> 8) << 8) |
	    ((shader->source.solid.color.blue_short  >> 8) << 0);

	if (diffuse != device->current_diffuse)
	    return TRUE;
    } else {
	count += pack_constants (&shader->source, buf + count);
    }
    count += pack_constants (&shader->mask, buf + count);

    if (count &&
	(device->current_n_constants != count ||
	 memcmp (device->current_constants, buf, count*4)))
    {
	return TRUE;
    }

    n = (i915_shader_channel_key (&shader->source) <<  0) |
	(i915_shader_channel_key (&shader->mask)   <<  8) |
	(i915_shader_channel_key (&shader->clip)   << 16) |
	(shader->op << 24) |
	((shader->opacity < 1.) << 30) |
	(((shader->content & CAIRO_CONTENT_ALPHA) == CAIRO_CONTENT_ALPHA) << 31);
    return n != device->current_program;
}

void
i915_set_dst (i915_device_t *device, i915_surface_t *dst)
{
    uint32_t size;

    if (device->current_target != dst) {
	intel_bo_t *bo;

	bo = to_intel_bo (dst->intel.drm.bo);
	assert (bo != NULL);

	OUT_DWORD (_3DSTATE_BUF_INFO_CMD);
	OUT_DWORD (BUF_3D_ID_COLOR_BACK |
		   BUF_tiling (bo->tiling) |
		   BUF_3D_PITCH (dst->intel.drm.stride));
	OUT_RELOC (dst, I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER);

	device->current_target = dst;
    }

    if (dst->colorbuf != device->current_colorbuf) {
	OUT_DWORD (_3DSTATE_DST_BUF_VARS_CMD);
	OUT_DWORD (dst->colorbuf);
	device->current_colorbuf = dst->colorbuf;
    }

    size = DRAW_YMAX (dst->intel.drm.height) | DRAW_XMAX (dst->intel.drm.width);
    if (size != device->current_size) {
	OUT_DWORD (_3DSTATE_DRAW_RECT_CMD);
	OUT_DWORD (0); /* dither */
	OUT_DWORD (0); /* top-left */
	OUT_DWORD (size);
	OUT_DWORD (0);  /* origin */
	device->current_size = size;
    }
}

static void
i915_set_shader_target (i915_device_t *device,
		        const i915_shader_t *shader)
{
    i915_set_dst (device, shader->target);
}

int
i915_shader_num_texcoords (const i915_shader_t *shader)
{
    int cnt = 0;

    switch (shader->source.base.texfmt) {
    default:
	ASSERT_NOT_REACHED;
    case TEXCOORDFMT_NOT_PRESENT: break;
    case TEXCOORDFMT_2D: cnt += 2; break;
    case TEXCOORDFMT_3D: cnt += 3; break;
    case TEXCOORDFMT_4D: cnt += 4; break;
    case TEXCOORDFMT_1D: cnt += 1; break;
    case TEXCOORDFMT_2D_16: cnt += 1; break;
    }

    switch (shader->mask.base.texfmt) {
    default:
	ASSERT_NOT_REACHED;
    case TEXCOORDFMT_NOT_PRESENT: break;
    case TEXCOORDFMT_2D: cnt += 2; break;
    case TEXCOORDFMT_3D: cnt += 3; break;
    case TEXCOORDFMT_4D: cnt += 4; break;
    case TEXCOORDFMT_1D: cnt += 1; break;
    case TEXCOORDFMT_2D_16: cnt += 1; break;
    }

    switch (shader->clip.base.texfmt) {
    default:
	ASSERT_NOT_REACHED;
    case TEXCOORDFMT_NOT_PRESENT: break;
    case TEXCOORDFMT_2D: cnt += 2; break;
    case TEXCOORDFMT_3D: cnt += 3; break;
    case TEXCOORDFMT_4D: cnt += 4; break;
    case TEXCOORDFMT_1D: cnt += 1; break;
    case TEXCOORDFMT_2D_16: cnt += 1; break;
    }

    switch (shader->dst.base.texfmt) {
    default:
	ASSERT_NOT_REACHED;
    case TEXCOORDFMT_NOT_PRESENT: break;
    case TEXCOORDFMT_2D: cnt += 2; break;
    case TEXCOORDFMT_3D: cnt += 3; break;
    case TEXCOORDFMT_4D: cnt += 4; break;
    case TEXCOORDFMT_1D: cnt += 1; break;
    case TEXCOORDFMT_2D_16: cnt += 1; break;
    }

    return cnt;
}

void
i915_shader_fini (i915_shader_t *shader)
{
    i915_device_t *device = i915_device (shader->target);

    i915_shader_channel_fini (device, &shader->source);
    i915_shader_channel_fini (device, &shader->mask);
    i915_shader_channel_fini (device, &shader->clip);
}

void
i915_shader_set_clip (i915_shader_t *shader,
		      cairo_clip_t *clip)
{
    cairo_surface_t *clip_surface;
    int clip_x, clip_y;
    union i915_shader_channel *channel;
    i915_surface_t *s;

    clip_surface = _cairo_clip_get_surface (clip, &shader->target->intel.drm.base, &clip_x, &clip_y);
    assert (clip_surface->status == CAIRO_STATUS_SUCCESS);
    assert (clip_surface->type == CAIRO_SURFACE_TYPE_DRM);

    channel = &shader->clip;
    channel->type.vertex = VS_TEXTURE_16;
    channel->base.texfmt = TEXCOORDFMT_2D_16;
    channel->base.content = CAIRO_CONTENT_ALPHA;

    channel->type.fragment = FS_TEXTURE;
    channel->surface.pixel = NONE;

    s = (i915_surface_t *) clip_surface;
    channel->base.bo = to_intel_bo (s->intel.drm.bo);
    channel->base.n_samplers = 1;
    channel->base.offset[0] = s->offset;
    channel->base.map[0] = s->map0;
    channel->base.map[1] = s->map1;

    channel->base.sampler[0] =
	(MIPFILTER_NONE << SS2_MIP_FILTER_SHIFT) |
	i915_texture_filter (CAIRO_FILTER_NEAREST);
    channel->base.sampler[1] =
	SS3_NORMALIZED_COORDS |
	i915_texture_extend (CAIRO_EXTEND_NONE);

    cairo_matrix_init_scale (&shader->clip.base.matrix,
			     1. / s->intel.drm.width,
			     1. / s->intel.drm.height);
    cairo_matrix_translate (&shader->clip.base.matrix,
			    -clip_x, -clip_y);
}

static cairo_status_t
i915_shader_check_aperture (i915_shader_t *shader,
			    i915_device_t *device)
{
    cairo_status_t status;
    intel_bo_t *bo_array[4];
    uint32_t n = 0;

    if (shader->target != device->current_target)
	bo_array[n++] = to_intel_bo (shader->target->intel.drm.bo);

    if (shader->source.base.bo != NULL)
	bo_array[n++] = shader->source.base.bo;

    if (shader->mask.base.bo != NULL)
	bo_array[n++] = shader->mask.base.bo;

    if (shader->clip.base.bo != NULL)
	bo_array[n++] = shader->clip.base.bo;

    if (n == 0 || i915_check_aperture (device, bo_array, n))
	return CAIRO_STATUS_SUCCESS;

    status = i915_batch_flush (device);
    if (unlikely (status))
	return status;

    assert (i915_check_aperture (device, bo_array, n));
    return CAIRO_STATUS_SUCCESS;
}

static void
i915_shader_combine_mask (i915_shader_t *shader, i915_device_t *device)
{
    if (shader->mask.type.fragment == (i915_fragment_shader_t) -1 ||
	shader->mask.type.fragment == FS_CONSTANT)
    {
	return;
    }

    if (shader->mask.type.fragment == FS_PURE) {
	if (shader->mask.solid.pure & (1<<3)) {
	    shader->mask.type.fragment = FS_ONE;
	} else {
	    shader->mask.type.fragment = FS_ZERO;
	}
    }

    if (shader->mask.type.fragment == FS_ONE ||
	(shader->mask.base.content & CAIRO_CONTENT_ALPHA) == 0)
    {
	i915_shader_channel_reset (device, &shader->mask);
    }

    if (shader->mask.type.fragment == FS_ZERO) {
	i915_shader_channel_fini (device, &shader->source);

	shader->source.type.fragment = FS_ZERO;
	shader->source.type.vertex = VS_ZERO;
	shader->source.base.texfmt = TEXCOORDFMT_NOT_PRESENT;
	shader->source.base.mode = 0;
	shader->source.base.n_samplers = 0;
    }

    if (shader->source.type.fragment == FS_ZERO) {
	i915_shader_channel_reset (device, &shader->mask);
	i915_shader_channel_reset (device, &shader->clip);
    }
}

static void
i915_shader_setup_dst (i915_shader_t *shader)
{
    union i915_shader_channel *channel;
    i915_surface_t *s;

    /* We need to manual blending if we have a clip surface and an unbounded op,
     * or an extended blend mode.
     */
    if (shader->need_combine ||
	(shader->op < CAIRO_OPERATOR_SATURATE &&
	 (shader->clip.type.fragment == (i915_fragment_shader_t) -1 ||
	  _cairo_operator_bounded_by_mask (shader->op))))
    {
	return;
    }

    shader->need_combine = TRUE;

    channel = &shader->dst;
    channel->type.vertex = VS_TEXTURE_16;
    channel->base.texfmt = TEXCOORDFMT_2D_16;
    channel->base.content = shader->content;

    channel->type.fragment = FS_TEXTURE;
    channel->surface.pixel = NONE;

    s = shader->target;
    channel->base.bo = to_intel_bo (s->intel.drm.bo);
    channel->base.n_samplers = 1;
    channel->base.offset[0] = s->offset;
    channel->base.map[0] = s->map0;
    channel->base.map[1] = s->map1;

    channel->base.sampler[0] =
	(MIPFILTER_NONE << SS2_MIP_FILTER_SHIFT) |
	i915_texture_filter (CAIRO_FILTER_NEAREST);
    channel->base.sampler[1] =
	SS3_NORMALIZED_COORDS |
	i915_texture_extend (CAIRO_EXTEND_NONE);

    cairo_matrix_init_scale (&shader->dst.base.matrix,
			     1. / s->intel.drm.width,
			     1. / s->intel.drm.height);
}

static void
i915_shader_combine_source (i915_shader_t *shader,
			    i915_device_t *device)
{
    if (device->last_source_fragment == shader->source.type.fragment)
	return;

    if (device->last_source_fragment == FS_DIFFUSE) {
	switch (shader->source.type.fragment) {
	case FS_ONE:
	case FS_PURE:
	case FS_CONSTANT:
	case FS_DIFFUSE:
	    shader->source.type.fragment = FS_DIFFUSE;
	    shader->source.base.mode = 0;
	    break;
	case FS_ZERO:
	case FS_LINEAR:
	case FS_RADIAL:
	case FS_TEXTURE:
	case FS_YUV:
	case FS_SPANS:
	default:
	    break;
	}
    }

    device->last_source_fragment = shader->source.type.fragment;
}

static inline float *
i915_composite_vertex (float *v,
		       const i915_shader_t *shader,
		       double x, double y)
{
    double s, t;

    /* Each vertex is:
     *   2 vertex coordinates
     *   [0-2] source texture coordinates
     *   [0-2] mask texture coordinates
     */

    *v++ = x; *v++ = y;
    switch (shader->source.type.vertex) {
    case VS_ZERO:
    case VS_CONSTANT:
	break;
    case VS_LINEAR:
	*v++ = i915_shader_linear_texcoord (&shader->source.linear, x, y);
	break;
    case VS_TEXTURE:
	s = x, t = y;
	cairo_matrix_transform_point (&shader->source.base.matrix, &s, &t);
	*v++ = s; *v++ = t;
	break;
    case VS_TEXTURE_16:
	s = x, t = y;
	cairo_matrix_transform_point (&shader->source.base.matrix, &s, &t);
	*v++ = texcoord_2d_16 (s, t);
	break;
    }
    switch (shader->mask.type.vertex) {
    case VS_ZERO:
    case VS_CONSTANT:
	break;
    case VS_LINEAR:
	*v++ = i915_shader_linear_texcoord (&shader->mask.linear, x, y);
	break;
    case VS_TEXTURE:
	s = x, t = y;
	cairo_matrix_transform_point (&shader->mask.base.matrix, &s, &t);
	*v++ = s; *v++ = t;
	break;
    case VS_TEXTURE_16:
	s = x, t = y;
	cairo_matrix_transform_point (&shader->mask.base.matrix, &s, &t);
	*v++ = texcoord_2d_16 (s, t);
	break;
    }

    return v;
}

static inline void
i915_shader_add_rectangle_general (const i915_shader_t *shader,
				   int x, int y,
				   int w, int h)
{
    float *vertices;

    vertices = i915_add_rectangle (shader->device);
    vertices = i915_composite_vertex (vertices, shader, x + w, y + h);
    vertices = i915_composite_vertex (vertices, shader, x, y + h);
    vertices = i915_composite_vertex (vertices, shader, x, y);
    /* XXX overflow! */
}

void
i915_vbo_flush (i915_device_t *device)
{
    assert (device->floats_per_vertex);
    assert (device->vertex_count);

    if (device->vbo == 0) {
	OUT_DWORD (_3DSTATE_LOAD_STATE_IMMEDIATE_1 |
		   I1_LOAD_S (0) |
		   I1_LOAD_S (1) |
		   1);
	device->vbo = device->batch.used++;
	device->vbo_max_index = device->batch.used;
	OUT_DWORD ((device->floats_per_vertex << S1_VERTEX_WIDTH_SHIFT) |
		   (device->floats_per_vertex << S1_VERTEX_PITCH_SHIFT));
    }

    OUT_DWORD (PRIM3D_RECTLIST |
	       PRIM3D_INDIRECT_SEQUENTIAL |
	       device->vertex_count);
    OUT_DWORD (device->vertex_index);

    device->vertex_index += device->vertex_count;
    device->vertex_count = 0;
}

cairo_status_t
i915_shader_commit (i915_shader_t *shader,
		    i915_device_t *device)
{
    unsigned floats_per_vertex;
    cairo_status_t status;

    assert (CAIRO_MUTEX_IS_LOCKED (device->intel.base.base.mutex));

    if (! shader->committed) {
	device->shader = shader;

	i915_shader_combine_mask (shader, device);
	i915_shader_combine_source (shader, device);
	i915_shader_setup_dst (shader);

	shader->add_rectangle = i915_shader_add_rectangle_general;

	if ((status = setjmp (shader->unwind)))
	    return status;

	shader->committed = TRUE;
    }

    if (i915_shader_needs_update (shader, device)) {
	if (i915_batch_space (device) < 256) {
	    status = i915_batch_flush (device);
	    if (unlikely (status))
		return status;
	}

	if (device->vertex_count)
	    i915_vbo_flush (device);

	status = i915_shader_check_aperture (shader, device);
	if (unlikely (status))
	    return status;

  update_shader:
	i915_set_shader_target (device, shader);
	i915_set_shader_mode (device, shader);
	i915_set_shader_samplers (device, shader);
	i915_set_shader_constants (device, shader);
	i915_set_shader_program (device, shader);
    }

    floats_per_vertex = 2 + i915_shader_num_texcoords (shader);
    if (device->floats_per_vertex == floats_per_vertex)
	return CAIRO_STATUS_SUCCESS;

    if (i915_batch_space (device) < 8) {
	status = i915_batch_flush (device);
	if (unlikely (status))
	    return status;

	goto update_shader;
    }

    if (device->vertex_count)
	i915_vbo_flush (device);

    if (device->vbo) {
	device->batch_base[device->vbo_max_index] |= device->vertex_index;
	OUT_DWORD (_3DSTATE_LOAD_STATE_IMMEDIATE_1 | I1_LOAD_S (1) | 0);
	device->vbo_max_index = device->batch.used;
	OUT_DWORD ((floats_per_vertex << S1_VERTEX_WIDTH_SHIFT) |
		   (floats_per_vertex << S1_VERTEX_PITCH_SHIFT));
    }

    device->floats_per_vertex = floats_per_vertex;
    device->rectangle_size = floats_per_vertex * 3 * sizeof (float);
    device->vertex_index =
	(device->vbo_used + 4*floats_per_vertex - 1) / (4 * floats_per_vertex);
    device->vbo_offset = 4 * device->vertex_index * floats_per_vertex;

    return CAIRO_STATUS_SUCCESS;
}
