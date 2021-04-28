/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 Eric Anholt
 * Copyright © 2009 Chris Wilson
 * Copyright © 2005,2010 Red Hat, Inc
 * Copyright © 2011 Linaro Limited
 * Copyright © 2011 Samsung Electronics
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
 *	Benjamin Otte <otte@gnome.org>
 *	Carl Worth <cworth@cworth.org>
 *	Chris Wilson <chris@chris-wilson.co.uk>
 *	Eric Anholt <eric@anholt.net>
 *	Alexandros Frantzis <alexandros.frantzis@linaro.org>
 *	Henry Song <hsong@sisa.samsung.com>
 *	Martin Robinson <mrobinson@igalia.com>
 */

#include "cairoint.h"

#include "cairo-gl-private.h"

#include "cairo-composite-rectangles-private.h"
#include "cairo-clip-private.h"
#include "cairo-error-private.h"
#include "cairo-image-surface-private.h"

/* FIXME: Copy of same routine in cairo-gl-msaa-compositor.c */
static cairo_int_status_t
_draw_int_rect (cairo_gl_context_t      *ctx,
		cairo_gl_composite_t    *setup,
		cairo_rectangle_int_t   *rect)
{
    cairo_box_t box;
    cairo_point_t quad[4];

    _cairo_box_from_rectangle (&box, rect);
    quad[0].x = box.p1.x;
    quad[0].y = box.p1.y;
    quad[1].x = box.p1.x;
    quad[1].y = box.p2.y;
    quad[2].x = box.p2.x;
    quad[2].y = box.p2.y;
    quad[3].x = box.p2.x;
    quad[3].y = box.p1.y;

    return _cairo_gl_composite_emit_quad_as_tristrip (ctx, setup, quad);
}

static cairo_int_status_t
_blit_texture_to_renderbuffer (cairo_gl_surface_t *surface)
{
    cairo_gl_context_t *ctx = NULL;
    cairo_gl_composite_t setup;
    cairo_surface_pattern_t pattern;
    cairo_rectangle_int_t extents;
    cairo_int_status_t status;

    /* FIXME: This only permits blit when glesv3 is enabled.  But note that
       glesv2 with the ANGLE extension should also be able to support this feature,
       so once the ANGLE support code is in place this check can be relaxed. */
    if (((cairo_gl_context_t *)surface->base.device)->gl_flavor != CAIRO_GL_FLAVOR_ES3)
	return CAIRO_INT_STATUS_SUCCESS;

    if (! surface->content_in_texture)
	return CAIRO_INT_STATUS_SUCCESS;

    memset (&setup, 0, sizeof (cairo_gl_composite_t));

    status = _cairo_gl_composite_set_operator (&setup,
					       CAIRO_OPERATOR_SOURCE,
					       FALSE);

    if (status)
	return status;

    setup.dst = surface;
    setup.clip_region = surface->clip_region;

    _cairo_pattern_init_for_surface (&pattern, &surface->base);
    status = _cairo_gl_composite_set_source (&setup, &pattern.base,
					     NULL, NULL, FALSE);
    _cairo_pattern_fini (&pattern.base);

    if (unlikely (status))
	goto FAIL;

    _cairo_gl_composite_set_multisample (&setup);

    status = _cairo_gl_composite_begin (&setup, &ctx);

    if (unlikely (status))
	goto FAIL;

    extents.x = extents.y = 0;
    extents.width = surface->width;
    extents.height = surface->height;

    status = _draw_int_rect (ctx, &setup, &extents);

    if (status == CAIRO_INT_STATUS_SUCCESS)
	surface->content_in_texture = FALSE;

FAIL:
    _cairo_gl_composite_fini (&setup);

    if (ctx) {
	_cairo_gl_composite_flush (ctx);
	status = _cairo_gl_context_release (ctx, status);
    }

    return status;
}

cairo_int_status_t
_cairo_gl_composite_set_source (cairo_gl_composite_t *setup,
				const cairo_pattern_t *pattern,
				const cairo_rectangle_int_t *sample,
				const cairo_rectangle_int_t *extents,
				cairo_bool_t use_texgen)
{
    _cairo_gl_operand_destroy (&setup->src);
    return _cairo_gl_operand_init (&setup->src, pattern, setup->dst,
				   sample, extents, use_texgen);
}

void
_cairo_gl_composite_set_source_operand (cairo_gl_composite_t *setup,
					const cairo_gl_operand_t *source)
{
    cairo_int_status_t status;

    _cairo_gl_operand_destroy (&setup->src);
    _cairo_gl_operand_copy (&setup->src, source);

    if (source->type == CAIRO_GL_OPERAND_TEXTURE)
	status = _cairo_gl_surface_resolve_multisampling (source->texture.surface);
}

void
_cairo_gl_composite_set_solid_source (cairo_gl_composite_t *setup,
				      const cairo_color_t *color)
{
    _cairo_gl_operand_destroy (&setup->src);
    _cairo_gl_solid_operand_init (&setup->src, color);
}

cairo_int_status_t
_cairo_gl_composite_set_mask (cairo_gl_composite_t *setup,
			      const cairo_pattern_t *pattern,
			      const cairo_rectangle_int_t *sample,
			      const cairo_rectangle_int_t *extents,
			      cairo_bool_t use_texgen)
{
    _cairo_gl_operand_destroy (&setup->mask);
    if (pattern == NULL)
	return CAIRO_STATUS_SUCCESS;

    return _cairo_gl_operand_init (&setup->mask, pattern, setup->dst,
				   sample, extents, use_texgen);
}

void
_cairo_gl_composite_set_mask_operand (cairo_gl_composite_t *setup,
				      const cairo_gl_operand_t *mask)
{
    cairo_int_status_t status;
    _cairo_gl_operand_destroy (&setup->mask);
    if (mask) {
	_cairo_gl_operand_copy (&setup->mask, mask);
	if (mask->type == CAIRO_GL_OPERAND_TEXTURE)
	    status = _cairo_gl_surface_resolve_multisampling (mask->texture.surface);
    }
}

void
_cairo_gl_composite_set_spans (cairo_gl_composite_t *setup)
{
    setup->spans = TRUE;
}

void
_cairo_gl_composite_set_multisample (cairo_gl_composite_t *setup)
{
    setup->multisample = TRUE;
}

void
_cairo_gl_composite_set_clip_region (cairo_gl_composite_t *setup,
				     cairo_region_t *clip_region)
{
    setup->clip_region = clip_region;
}

void
_cairo_gl_composite_set_clip (cairo_gl_composite_t *setup,
			      cairo_clip_t *clip)
{
    setup->clip = clip;
}

static void
_cairo_gl_composite_bind_to_shader (cairo_gl_context_t   *ctx,
				    cairo_gl_composite_t *setup)
{
    _cairo_gl_shader_bind_matrix4f(ctx, ctx->current_shader->mvp_location,
				   ctx->modelviewprojection_matrix);
    _cairo_gl_operand_bind_to_shader (ctx, &setup->src,  CAIRO_GL_TEX_SOURCE);
    _cairo_gl_operand_bind_to_shader (ctx, &setup->mask, CAIRO_GL_TEX_MASK);
}

static void
_cairo_gl_texture_set_filter (cairo_gl_context_t *ctx,
			      GLuint              target,
			      cairo_filter_t      filter)
{
    switch (filter) {
    case CAIRO_FILTER_FAST:
    case CAIRO_FILTER_NEAREST:
	glTexParameteri (target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri (target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	break;
    case CAIRO_FILTER_GOOD:
    case CAIRO_FILTER_BEST:
    case CAIRO_FILTER_BILINEAR:
	glTexParameteri (target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri (target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	break;
    default:
    case CAIRO_FILTER_GAUSSIAN:
	ASSERT_NOT_REACHED;
    }
}

static void
_cairo_gl_texture_set_extend (cairo_gl_context_t *ctx,
			      GLuint              target,
			      cairo_extend_t      extend)
{
    GLint wrap_mode;
    assert (! _cairo_gl_device_requires_power_of_two_textures (&ctx->base) ||
	    (extend != CAIRO_EXTEND_REPEAT && extend != CAIRO_EXTEND_REFLECT));

    switch (extend) {
    case CAIRO_EXTEND_NONE:
	if (ctx->gl_flavor == CAIRO_GL_FLAVOR_ES3 ||
	    ctx->gl_flavor == CAIRO_GL_FLAVOR_ES2)
	    wrap_mode = GL_CLAMP_TO_EDGE;
	else
	    wrap_mode = GL_CLAMP_TO_BORDER;
	break;
    case CAIRO_EXTEND_PAD:
	wrap_mode = GL_CLAMP_TO_EDGE;
	break;
    case CAIRO_EXTEND_REPEAT:
	if (ctx->has_npot_repeat)
	    wrap_mode = GL_REPEAT;
	else
	    wrap_mode = GL_CLAMP_TO_EDGE;
	break;
    case CAIRO_EXTEND_REFLECT:
	if (ctx->has_npot_repeat)
	    wrap_mode = GL_MIRRORED_REPEAT;
	else
	    wrap_mode = GL_CLAMP_TO_EDGE;
	break;
    default:
	wrap_mode = 0;
    }

    if (likely (wrap_mode)) {
	glTexParameteri (target, GL_TEXTURE_WRAP_S, wrap_mode);
	glTexParameteri (target, GL_TEXTURE_WRAP_T, wrap_mode);
    }
}


static void
_cairo_gl_context_setup_operand (cairo_gl_context_t *ctx,
				 cairo_gl_tex_t      tex_unit,
				 cairo_gl_operand_t *operand,
				 unsigned int        vertex_offset,
				 cairo_bool_t        vertex_size_changed)
{
    cairo_gl_dispatch_t *dispatch = &ctx->dispatch;
    cairo_bool_t needs_setup;

    /* XXX: we need to do setup when switching from shaders
     * to no shaders (or back) */
    needs_setup = vertex_size_changed;
    needs_setup |= _cairo_gl_operand_needs_setup (&ctx->operands[tex_unit],
						 operand,
						 vertex_offset);

    if (needs_setup) {
	_cairo_gl_composite_flush (ctx);
	_cairo_gl_context_destroy_operand (ctx, tex_unit);
    }

    memcpy (&ctx->operands[tex_unit], operand, sizeof (cairo_gl_operand_t));
    ctx->operands[tex_unit].vertex_offset = vertex_offset;

    if (! needs_setup)
	return;

    switch (operand->type) {
    default:
    case CAIRO_GL_OPERAND_COUNT:
	ASSERT_NOT_REACHED;
    case CAIRO_GL_OPERAND_NONE:
	break;
	/* fall through */
    case CAIRO_GL_OPERAND_CONSTANT:
	break;
    case CAIRO_GL_OPERAND_TEXTURE:
	glActiveTexture (GL_TEXTURE0 + tex_unit);
	glBindTexture (ctx->tex_target, operand->texture.tex);
	_cairo_gl_texture_set_extend (ctx, ctx->tex_target,
				      operand->texture.attributes.extend);
	_cairo_gl_texture_set_filter (ctx, ctx->tex_target,
				      operand->texture.attributes.filter);

	if (! operand->texture.texgen) {
	    dispatch->VertexAttribPointer (CAIRO_GL_TEXCOORD0_ATTRIB_INDEX + tex_unit, 2,
					   GL_FLOAT, GL_FALSE, ctx->vertex_size,
					   ctx->vb + vertex_offset);
	    dispatch->EnableVertexAttribArray (CAIRO_GL_TEXCOORD0_ATTRIB_INDEX + tex_unit);
	}
	break;
    case CAIRO_GL_OPERAND_LINEAR_GRADIENT:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_A0:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_NONE:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_EXT:
	glActiveTexture (GL_TEXTURE0 + tex_unit);
	glBindTexture (ctx->tex_target, operand->gradient.gradient->tex);
	_cairo_gl_texture_set_extend (ctx, ctx->tex_target, operand->gradient.extend);
	_cairo_gl_texture_set_filter (ctx, ctx->tex_target, CAIRO_FILTER_BILINEAR);

	if (! operand->gradient.texgen) {
	    dispatch->VertexAttribPointer (CAIRO_GL_TEXCOORD0_ATTRIB_INDEX + tex_unit, 2,
					   GL_FLOAT, GL_FALSE, ctx->vertex_size,
					   ctx->vb + vertex_offset);
	    dispatch->EnableVertexAttribArray (CAIRO_GL_TEXCOORD0_ATTRIB_INDEX + tex_unit);
	}
	break;
    }
}

static void
_cairo_gl_context_setup_spans (cairo_gl_context_t *ctx,
			       cairo_bool_t        spans_enabled,
			       unsigned int        vertex_size,
			       unsigned int        vertex_offset)
{
    cairo_gl_dispatch_t *dispatch = &ctx->dispatch;

    if (! spans_enabled) {
	dispatch->DisableVertexAttribArray (CAIRO_GL_COLOR_ATTRIB_INDEX);
	ctx->spans = FALSE;
	return;
    }

    dispatch->VertexAttribPointer (CAIRO_GL_COLOR_ATTRIB_INDEX, 4,
				   GL_UNSIGNED_BYTE, GL_TRUE, vertex_size,
				   ctx->vb + vertex_offset);
    dispatch->EnableVertexAttribArray (CAIRO_GL_COLOR_ATTRIB_INDEX);
    ctx->spans = TRUE;
}

void
_cairo_gl_context_destroy_operand (cairo_gl_context_t *ctx,
				   cairo_gl_tex_t tex_unit)
{
    cairo_gl_dispatch_t *dispatch = &ctx->dispatch;

    if  (!_cairo_gl_context_is_flushed (ctx))
	_cairo_gl_composite_flush (ctx);

    switch (ctx->operands[tex_unit].type) {
    default:
    case CAIRO_GL_OPERAND_COUNT:
	ASSERT_NOT_REACHED;
    case CAIRO_GL_OPERAND_NONE:
	break;
	/* fall through */
    case CAIRO_GL_OPERAND_CONSTANT:
	break;
    case CAIRO_GL_OPERAND_TEXTURE:
	dispatch->DisableVertexAttribArray (CAIRO_GL_TEXCOORD0_ATTRIB_INDEX + tex_unit);
	break;
    case CAIRO_GL_OPERAND_LINEAR_GRADIENT:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_A0:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_NONE:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_EXT:
	dispatch->DisableVertexAttribArray (CAIRO_GL_TEXCOORD0_ATTRIB_INDEX + tex_unit);
	break;
    }

    memset (&ctx->operands[tex_unit], 0, sizeof (cairo_gl_operand_t));
}

static void
_cairo_gl_set_operator (cairo_gl_context_t *ctx,
			cairo_operator_t    op,
			cairo_bool_t        component_alpha)
{
    struct {
	GLenum src;
	GLenum dst;
    } blend_factors[] = {
	{ GL_ZERO, GL_ZERO }, /* Clear */
	{ GL_ONE, GL_ZERO }, /* Source */
	{ GL_ONE, GL_ONE_MINUS_SRC_ALPHA }, /* Over */
	{ GL_DST_ALPHA, GL_ZERO }, /* In */
	{ GL_ONE_MINUS_DST_ALPHA, GL_ZERO }, /* Out */
	{ GL_DST_ALPHA, GL_ONE_MINUS_SRC_ALPHA }, /* Atop */

	{ GL_ZERO, GL_ONE }, /* Dest */
	{ GL_ONE_MINUS_DST_ALPHA, GL_ONE }, /* DestOver */
	{ GL_ZERO, GL_SRC_ALPHA }, /* DestIn */
	{ GL_ZERO, GL_ONE_MINUS_SRC_ALPHA }, /* DestOut */
	{ GL_ONE_MINUS_DST_ALPHA, GL_SRC_ALPHA }, /* DestAtop */

	{ GL_ONE_MINUS_DST_ALPHA, GL_ONE_MINUS_SRC_ALPHA }, /* Xor */
	{ GL_ONE, GL_ONE }, /* Add */
    };
    GLenum src_factor, dst_factor;

    assert (op < ARRAY_LENGTH (blend_factors));
    /* different dst and component_alpha changes cause flushes elsewhere */
    if (ctx->current_operator != op)
	_cairo_gl_composite_flush (ctx);
    ctx->current_operator = op;

    src_factor = blend_factors[op].src;
    dst_factor = blend_factors[op].dst;

    /* Even when the user requests CAIRO_CONTENT_COLOR, we use GL_RGBA
     * due to texture filtering of GL_CLAMP_TO_BORDER.  So fix those
     * bits in that case.
     */
    if (ctx->current_target->base.content == CAIRO_CONTENT_COLOR) {
	if (src_factor == GL_ONE_MINUS_DST_ALPHA)
	    src_factor = GL_ZERO;
	if (src_factor == GL_DST_ALPHA)
	    src_factor = GL_ONE;
    }

    if (component_alpha) {
	if (dst_factor == GL_ONE_MINUS_SRC_ALPHA)
	    dst_factor = GL_ONE_MINUS_SRC_COLOR;
	if (dst_factor == GL_SRC_ALPHA)
	    dst_factor = GL_SRC_COLOR;
    }

    if (ctx->current_target->base.content == CAIRO_CONTENT_ALPHA) {
	glBlendFuncSeparate (GL_ZERO, GL_ZERO, src_factor, dst_factor);
    } else if (ctx->current_target->base.content == CAIRO_CONTENT_COLOR) {
	glBlendFuncSeparate (src_factor, dst_factor, GL_ONE, GL_ONE);
    } else {
	glBlendFunc (src_factor, dst_factor);
    }
}

static cairo_status_t
_cairo_gl_composite_begin_component_alpha  (cairo_gl_context_t *ctx,
					    cairo_gl_composite_t *setup)
{
    cairo_gl_shader_t *pre_shader = NULL;
    cairo_status_t status;

    /* For CLEAR, cairo's rendering equation (quoting Owen's description in:
     * https://lists.cairographics.org/archives/cairo/2005-August/004992.html)
     * is:
     *     mask IN clip ? src OP dest : dest
     * or more simply:
     *     mask IN CLIP ? 0 : dest
     *
     * where the ternary operator A ? B : C is (A * B) + ((1 - A) * C).
     *
     * The model we use in _cairo_gl_set_operator() is Render's:
     *     src IN mask IN clip OP dest
     * which would boil down to:
     *     0 (bounded by the extents of the drawing).
     *
     * However, we can do a Render operation using an opaque source
     * and DEST_OUT to produce:
     *    1 IN mask IN clip DEST_OUT dest
     * which is
     *    mask IN clip ? 0 : dest
     */
    if (setup->op == CAIRO_OPERATOR_CLEAR) {
	_cairo_gl_solid_operand_init (&setup->src, CAIRO_COLOR_WHITE);
	setup->op = CAIRO_OPERATOR_DEST_OUT;
    }

    /*
     * implements component-alpha %CAIRO_OPERATOR_OVER using two passes of
     * the simpler operations %CAIRO_OPERATOR_DEST_OUT and %CAIRO_OPERATOR_ADD.
     *
     * From http://anholt.livejournal.com/32058.html:
     *
     * The trouble is that component-alpha rendering requires two different sources
     * for blending: one for the source value to the blender, which is the
     * per-channel multiplication of source and mask, and one for the source alpha
     * for multiplying with the destination channels, which is the multiplication
     * of the source channels by the mask alpha. So the equation for Over is:
     *
     * dst.A = src.A * mask.A + (1 - (src.A * mask.A)) * dst.A
     * dst.R = src.R * mask.R + (1 - (src.A * mask.R)) * dst.R
     * dst.G = src.G * mask.G + (1 - (src.A * mask.G)) * dst.G
     * dst.B = src.B * mask.B + (1 - (src.A * mask.B)) * dst.B
     *
     * But we can do some simpler operations, right? How about PictOpOutReverse,
     * which has a source factor of 0 and dest factor of (1 - source alpha). We
     * can get the source alpha value (srca.X = src.A * mask.X) out of the texture
     * blenders pretty easily. So we can do a component-alpha OutReverse, which
     * gets us:
     *
     * dst.A = 0 + (1 - (src.A * mask.A)) * dst.A
     * dst.R = 0 + (1 - (src.A * mask.R)) * dst.R
     * dst.G = 0 + (1 - (src.A * mask.G)) * dst.G
     * dst.B = 0 + (1 - (src.A * mask.B)) * dst.B
     *
     * OK. And if an op doesn't use the source alpha value for the destination
     * factor, then we can do the channel multiplication in the texture blenders
     * to get the source value, and ignore the source alpha that we wouldn't use.
     * We've supported this in the Radeon driver for a long time. An example would
     * be PictOpAdd, which does:
     *
     * dst.A = src.A * mask.A + dst.A
     * dst.R = src.R * mask.R + dst.R
     * dst.G = src.G * mask.G + dst.G
     * dst.B = src.B * mask.B + dst.B
     *
     * Hey, this looks good! If we do a PictOpOutReverse and then a PictOpAdd right
     * after it, we get:
     *
     * dst.A = src.A * mask.A + ((1 - (src.A * mask.A)) * dst.A)
     * dst.R = src.R * mask.R + ((1 - (src.A * mask.R)) * dst.R)
     * dst.G = src.G * mask.G + ((1 - (src.A * mask.G)) * dst.G)
     * dst.B = src.B * mask.B + ((1 - (src.A * mask.B)) * dst.B)
     *
     * This two-pass trickery could be avoided using a new GL extension that
     * lets two values come out of the shader and into the blend unit.
     */
    if (setup->op == CAIRO_OPERATOR_OVER) {
	setup->op = CAIRO_OPERATOR_ADD;
	status = _cairo_gl_get_shader_by_type (ctx,
					       &setup->src,
					       &setup->mask,
					       setup->spans,
					       CAIRO_GL_SHADER_IN_CA_SOURCE_ALPHA,
					       &pre_shader);
	if (unlikely (status))
	    return status;
    }

    if (ctx->pre_shader != pre_shader)
	_cairo_gl_composite_flush (ctx);
    ctx->pre_shader = pre_shader;

    return CAIRO_STATUS_SUCCESS;
}

static void
_scissor_to_doubles (cairo_gl_surface_t	*surface,
		     double x1, double y1,
		     double x2, double y2)
{
    double height;

    height = y2 - y1;
    if (_cairo_gl_surface_is_texture (surface) == FALSE)
	y1 = surface->height - (y1 + height);
    glScissor (x1, y1, x2 - x1, height);
    glEnable (GL_SCISSOR_TEST);
}

void
_cairo_gl_scissor_to_rectangle (cairo_gl_surface_t *surface,
		       const cairo_rectangle_int_t *r)
{
    _scissor_to_doubles (surface, r->x, r->y, r->x+r->width, r->y+r->height);
}

static void
_scissor_to_box (cairo_gl_surface_t	*surface,
		 const cairo_box_t	*box)
{
    double x1, y1, x2, y2;
    _cairo_box_to_doubles (box, &x1, &y1, &x2, &y2);
    _scissor_to_doubles (surface, x1, y1, x2, y2);
}

static cairo_bool_t
_cairo_gl_composite_setup_vbo (cairo_gl_context_t *ctx,
			       unsigned int size_per_vertex)
{
    cairo_bool_t vertex_size_changed = ctx->vertex_size != size_per_vertex;
    if (vertex_size_changed) {
	ctx->vertex_size = size_per_vertex;
	_cairo_gl_composite_flush (ctx);
    }

    if (_cairo_gl_context_is_flushed (ctx)) {
	ctx->dispatch.VertexAttribPointer (CAIRO_GL_VERTEX_ATTRIB_INDEX, 2,
					   GL_FLOAT, GL_FALSE, size_per_vertex,
					   ctx->vb);
	ctx->dispatch.EnableVertexAttribArray (CAIRO_GL_VERTEX_ATTRIB_INDEX);
    }

    return vertex_size_changed;
}

static void
_disable_stencil_buffer (void)
{
    glDisable (GL_STENCIL_TEST);
    glDepthMask (GL_FALSE);
}

static cairo_int_status_t
_cairo_gl_composite_setup_painted_clipping (cairo_gl_composite_t *setup,
					    cairo_gl_context_t *ctx,
					    int vertex_size)
{
    cairo_int_status_t status = CAIRO_INT_STATUS_SUCCESS;

    cairo_gl_surface_t *dst = setup->dst;
    cairo_clip_t *clip = setup->clip;

    if (clip->num_boxes == 1 && clip->path == NULL) {
	_scissor_to_box (dst, &clip->boxes[0]);
	goto disable_stencil_buffer_and_return;
    }

    if (! _cairo_gl_ensure_stencil (ctx, setup->dst)) {
	status = CAIRO_INT_STATUS_UNSUPPORTED;
	goto disable_stencil_buffer_and_return;
    }

    /* We only want to clear the part of the stencil buffer
     * that we are about to use. It also does not hurt to
     * scissor around the painted clip. */
    _cairo_gl_scissor_to_rectangle (dst, _cairo_clip_get_extents (clip));

    /* The clip is not rectangular, so use the stencil buffer. */
    glDepthMask (GL_TRUE);
    glEnable (GL_STENCIL_TEST);

    /* Texture surfaces have private depth/stencil buffers, so we can
     * rely on any previous clip being cached there. */
    if (_cairo_gl_surface_is_texture (setup->dst)) {
	cairo_clip_t *old_clip = setup->dst->clip_on_stencil_buffer;
	if (_cairo_clip_equal (old_clip, setup->clip))
	    goto activate_stencil_buffer_and_return;

	if (old_clip) {
	    _cairo_clip_destroy (setup->dst->clip_on_stencil_buffer);
	}

	setup->dst->clip_on_stencil_buffer = _cairo_clip_copy (setup->clip);
    }

    glClearStencil (0);
    glClear (GL_STENCIL_BUFFER_BIT);

    glStencilOp (GL_REPLACE, GL_REPLACE, GL_REPLACE);
    glStencilFunc (GL_EQUAL, 1, 0xffffffff);
    glColorMask (0, 0, 0, 0);

    status = _cairo_gl_msaa_compositor_draw_clip (ctx, setup, clip);

    if (unlikely (status)) {
	glColorMask (1, 1, 1, 1);
	goto disable_stencil_buffer_and_return;
    }

    /* We want to only render to the stencil buffer, so draw everything now.
       Flushing also unbinds the VBO, which we want to rebind for regular
       drawing. */
    _cairo_gl_composite_flush (ctx);
    _cairo_gl_composite_setup_vbo (ctx, vertex_size);

activate_stencil_buffer_and_return:
    glColorMask (1, 1, 1, 1);
    glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP);
    glStencilFunc (GL_EQUAL, 1, 0xffffffff);
    return CAIRO_INT_STATUS_SUCCESS;

disable_stencil_buffer_and_return:
    _disable_stencil_buffer ();
    return status;
}

static cairo_int_status_t
_cairo_gl_composite_setup_clipping (cairo_gl_composite_t *setup,
				    cairo_gl_context_t *ctx,
				    int vertex_size)
{
    cairo_bool_t clip_changing = TRUE;
    cairo_bool_t clip_region_changing = TRUE;

    if (! ctx->clip && ! setup->clip && ! setup->clip_region && ! ctx->clip_region)
	goto disable_all_clipping;

    clip_changing = ! _cairo_clip_equal (ctx->clip, setup->clip);
    clip_region_changing = ! cairo_region_equal (ctx->clip_region, setup->clip_region);
    if (! _cairo_gl_context_is_flushed (ctx) &&
	(clip_region_changing || clip_changing))
	_cairo_gl_composite_flush (ctx);

    assert (!setup->clip_region || !setup->clip);

    /* setup->clip is only used by the msaa compositor and setup->clip_region
     * only by the other compositors, so it's safe to wait to clean up obsolete
     * clips. */
    if (clip_region_changing) {
	cairo_region_destroy (ctx->clip_region);
	ctx->clip_region = cairo_region_reference (setup->clip_region);
    }
    if (clip_changing) {
	_cairo_clip_destroy (ctx->clip);
	ctx->clip = _cairo_clip_copy (setup->clip);
    }

    /* For clip regions, we scissor right before drawing. */
    if (setup->clip_region)
	goto disable_all_clipping;

    if (setup->clip)
	return _cairo_gl_composite_setup_painted_clipping (setup, ctx,
							   vertex_size);
disable_all_clipping:
    _disable_stencil_buffer ();
    glDisable (GL_SCISSOR_TEST);
    return CAIRO_INT_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gl_set_operands_and_operator (cairo_gl_composite_t *setup,
				     cairo_gl_context_t *ctx)
{
    unsigned int dst_size, src_size, mask_size, vertex_size;
    cairo_status_t status;
    cairo_gl_shader_t *shader;
    cairo_bool_t component_alpha;
    cairo_bool_t vertex_size_changed;

    component_alpha =
	setup->mask.type == CAIRO_GL_OPERAND_TEXTURE &&
	setup->mask.texture.attributes.has_component_alpha;

    /* Do various magic for component alpha */
    if (component_alpha) {
	status = _cairo_gl_composite_begin_component_alpha (ctx, setup);
	if (unlikely (status))
	    return status;
     } else {
	if (ctx->pre_shader) {
	    _cairo_gl_composite_flush (ctx);
	    ctx->pre_shader = NULL;
	}
    }

    status = _cairo_gl_get_shader_by_type (ctx,
					   &setup->src,
					   &setup->mask,
					   setup->spans,
					   component_alpha ?
					   CAIRO_GL_SHADER_IN_CA_SOURCE :
					   CAIRO_GL_SHADER_IN_NORMAL,
					   &shader);
    if (unlikely (status)) {
	ctx->pre_shader = NULL;
	return status;
    }
    if (ctx->current_shader != shader)
	_cairo_gl_composite_flush (ctx);

    status = CAIRO_STATUS_SUCCESS;

    dst_size = 2 * sizeof (GLfloat);
    src_size = _cairo_gl_operand_get_vertex_size (&setup->src);
    mask_size = _cairo_gl_operand_get_vertex_size (&setup->mask);
    vertex_size = dst_size + src_size + mask_size;

    if (setup->spans)
	vertex_size += sizeof (GLfloat);

    vertex_size_changed = _cairo_gl_composite_setup_vbo (ctx, vertex_size);

    _cairo_gl_context_setup_operand (ctx, CAIRO_GL_TEX_SOURCE, &setup->src, dst_size, vertex_size_changed);
    _cairo_gl_context_setup_operand (ctx, CAIRO_GL_TEX_MASK, &setup->mask, dst_size + src_size, vertex_size_changed);

    _cairo_gl_context_setup_spans (ctx, setup->spans, vertex_size,
				   dst_size + src_size + mask_size);

    _cairo_gl_set_operator (ctx, setup->op, component_alpha);

    if (_cairo_gl_context_is_flushed (ctx)) {
	if (ctx->pre_shader) {
	    _cairo_gl_set_shader (ctx, ctx->pre_shader);
	    _cairo_gl_composite_bind_to_shader (ctx, setup);
	}
	_cairo_gl_set_shader (ctx, shader);
	_cairo_gl_composite_bind_to_shader (ctx, setup);
    }

    return status;
}

cairo_status_t
_cairo_gl_composite_begin (cairo_gl_composite_t *setup,
			   cairo_gl_context_t **ctx_out)
{
    cairo_gl_context_t *ctx;
    cairo_status_t status;

    assert (setup->dst);

    status = _cairo_gl_context_acquire (setup->dst->base.device, &ctx);
    if (unlikely (status))
	return status;

    _cairo_gl_context_set_destination (ctx, setup->dst, setup->multisample);
    glEnable (GL_BLEND);

    status = _cairo_gl_set_operands_and_operator (setup, ctx);
    if (unlikely (status))
	goto FAIL;

    status = _cairo_gl_composite_setup_clipping (setup, ctx, ctx->vertex_size);
    if (unlikely (status))
	goto FAIL;

    *ctx_out = ctx;

FAIL:
    if (unlikely (status))
	status = _cairo_gl_context_release (ctx, status);

    return status;
}

static inline void
_cairo_gl_composite_draw_tristrip (cairo_gl_context_t *ctx)
{
    cairo_array_t* indices = &ctx->tristrip_indices;
    const unsigned short *indices_array = _cairo_array_index_const (indices, 0);

    if (ctx->pre_shader) {
	cairo_gl_shader_t *prev_shader = ctx->current_shader;

	_cairo_gl_set_shader (ctx, ctx->pre_shader);
	_cairo_gl_set_operator (ctx, CAIRO_OPERATOR_DEST_OUT, TRUE);
	glDrawElements (GL_TRIANGLE_STRIP, _cairo_array_num_elements (indices), GL_UNSIGNED_SHORT, indices_array);

	_cairo_gl_set_shader (ctx, prev_shader);
	_cairo_gl_set_operator (ctx, CAIRO_OPERATOR_ADD, TRUE);
    }

    glDrawElements (GL_TRIANGLE_STRIP, _cairo_array_num_elements (indices), GL_UNSIGNED_SHORT, indices_array);
    _cairo_array_truncate (indices, 0);
}

static inline void
_cairo_gl_composite_draw_triangles (cairo_gl_context_t *ctx,
				    unsigned int count)
{
    if (! ctx->pre_shader) {
	glDrawArrays (GL_TRIANGLES, 0, count);
    } else {
	cairo_gl_shader_t *prev_shader = ctx->current_shader;

	_cairo_gl_set_shader (ctx, ctx->pre_shader);
	_cairo_gl_set_operator (ctx, CAIRO_OPERATOR_DEST_OUT, TRUE);
	glDrawArrays (GL_TRIANGLES, 0, count);

	_cairo_gl_set_shader (ctx, prev_shader);
	_cairo_gl_set_operator (ctx, CAIRO_OPERATOR_ADD, TRUE);
	glDrawArrays (GL_TRIANGLES, 0, count);
    }
}

static void
_cairo_gl_composite_draw_triangles_with_clip_region (cairo_gl_context_t *ctx,
						     unsigned int count)
{
    int i, num_rectangles;

    if (!ctx->clip_region) {
	_cairo_gl_composite_draw_triangles (ctx, count);
	return;
    }

    num_rectangles = cairo_region_num_rectangles (ctx->clip_region);
    for (i = 0; i < num_rectangles; i++) {
	cairo_rectangle_int_t rect;

	cairo_region_get_rectangle (ctx->clip_region, i, &rect);

	_cairo_gl_scissor_to_rectangle (ctx->current_target, &rect);
	_cairo_gl_composite_draw_triangles (ctx, count);
    }
}

static void
_cairo_gl_composite_unmap_vertex_buffer (cairo_gl_context_t *ctx)
{
    ctx->vb_offset = 0;
}

void
_cairo_gl_composite_flush (cairo_gl_context_t *ctx)
{
    unsigned int count;
    int i;

    if (_cairo_gl_context_is_flushed (ctx))
	return;

    count = ctx->vb_offset / ctx->vertex_size;
    _cairo_gl_composite_unmap_vertex_buffer (ctx);

    if (ctx->primitive_type == CAIRO_GL_PRIMITIVE_TYPE_TRISTRIPS) {
	_cairo_gl_composite_draw_tristrip (ctx);
    } else {
	assert (ctx->primitive_type == CAIRO_GL_PRIMITIVE_TYPE_TRIANGLES);
	_cairo_gl_composite_draw_triangles_with_clip_region (ctx, count);
    }

    for (i = 0; i < ARRAY_LENGTH (ctx->glyph_cache); i++)
	_cairo_gl_glyph_cache_unlock (&ctx->glyph_cache[i]);
}

static void
_cairo_gl_composite_prepare_buffer (cairo_gl_context_t *ctx,
				    unsigned int n_vertices,
				    cairo_gl_primitive_type_t primitive_type)
{
    if (ctx->primitive_type != primitive_type) {
	_cairo_gl_composite_flush (ctx);
	ctx->primitive_type = primitive_type;
    }

    assert(ctx->vbo_size > 0);
    if (ctx->vb_offset + n_vertices * ctx->vertex_size > ctx->vbo_size)
	_cairo_gl_composite_flush (ctx);
}

static inline void
_cairo_gl_composite_emit_vertex (cairo_gl_context_t *ctx,
				 GLfloat x, GLfloat y)
{
    GLfloat *vb = (GLfloat *) (void *) &ctx->vb[ctx->vb_offset];

    *vb++ = x;
    *vb++ = y;

    _cairo_gl_operand_emit (&ctx->operands[CAIRO_GL_TEX_SOURCE], &vb, x, y);
    _cairo_gl_operand_emit (&ctx->operands[CAIRO_GL_TEX_MASK  ], &vb, x, y);

    ctx->vb_offset += ctx->vertex_size;
}

static inline void
_cairo_gl_composite_emit_alpha_vertex (cairo_gl_context_t *ctx,
				       GLfloat x, GLfloat y, uint8_t alpha)
{
    GLfloat *vb = (GLfloat *) (void *) &ctx->vb[ctx->vb_offset];
    union fi {
	float f;
	GLbyte bytes[4];
    } fi;

    *vb++ = x;
    *vb++ = y;

    _cairo_gl_operand_emit (&ctx->operands[CAIRO_GL_TEX_SOURCE], &vb, x, y);
    _cairo_gl_operand_emit (&ctx->operands[CAIRO_GL_TEX_MASK  ], &vb, x, y);

    fi.bytes[0] = 0;
    fi.bytes[1] = 0;
    fi.bytes[2] = 0;
    fi.bytes[3] = alpha;
    *vb++ = fi.f;

    ctx->vb_offset += ctx->vertex_size;
}

static void
_cairo_gl_composite_emit_point (cairo_gl_context_t	*ctx,
				const cairo_point_t	*point)
{
    _cairo_gl_composite_emit_vertex (ctx,
				     _cairo_fixed_to_double (point->x),
				     _cairo_fixed_to_double (point->y));
}

static void
_cairo_gl_composite_emit_rect (cairo_gl_context_t *ctx,
			       GLfloat x1, GLfloat y1,
			       GLfloat x2, GLfloat y2)
{
    _cairo_gl_composite_prepare_buffer (ctx, 6,
					CAIRO_GL_PRIMITIVE_TYPE_TRIANGLES);

    _cairo_gl_composite_emit_vertex (ctx, x1, y1);
    _cairo_gl_composite_emit_vertex (ctx, x2, y1);
    _cairo_gl_composite_emit_vertex (ctx, x1, y2);

    _cairo_gl_composite_emit_vertex (ctx, x2, y1);
    _cairo_gl_composite_emit_vertex (ctx, x2, y2);
    _cairo_gl_composite_emit_vertex (ctx, x1, y2);
}

cairo_gl_emit_rect_t
_cairo_gl_context_choose_emit_rect (cairo_gl_context_t *ctx)
{
    return _cairo_gl_composite_emit_rect;
}

void
_cairo_gl_context_emit_rect (cairo_gl_context_t *ctx,
			     GLfloat x1, GLfloat y1,
			     GLfloat x2, GLfloat y2)
{
    _cairo_gl_composite_emit_rect (ctx, x1, y1, x2, y2);
}

static void
_cairo_gl_composite_emit_span (cairo_gl_context_t *ctx,
			       GLfloat x1, GLfloat y1,
			       GLfloat x2, GLfloat y2,
			       uint8_t alpha)
{
    _cairo_gl_composite_prepare_buffer (ctx, 6,
					CAIRO_GL_PRIMITIVE_TYPE_TRIANGLES);

    _cairo_gl_composite_emit_alpha_vertex (ctx, x1, y1, alpha);
    _cairo_gl_composite_emit_alpha_vertex (ctx, x2, y1, alpha);
    _cairo_gl_composite_emit_alpha_vertex (ctx, x1, y2, alpha);

    _cairo_gl_composite_emit_alpha_vertex (ctx, x2, y1, alpha);
    _cairo_gl_composite_emit_alpha_vertex (ctx, x2, y2, alpha);
    _cairo_gl_composite_emit_alpha_vertex (ctx, x1, y2, alpha);
}

static void
_cairo_gl_composite_emit_solid_span (cairo_gl_context_t *ctx,
				     GLfloat x1, GLfloat y1,
				     GLfloat x2, GLfloat y2,
				     uint8_t alpha)
{
    GLfloat *v;
    union fi {
	float f;
	GLbyte bytes[4];
    } fi;

    _cairo_gl_composite_prepare_buffer (ctx, 6,
					CAIRO_GL_PRIMITIVE_TYPE_TRIANGLES);
    v = (GLfloat *) (void *) &ctx->vb[ctx->vb_offset];

    v[15] = v[ 6] = v[0] = x1;
    v[10] = v[ 4] = v[1] = y1;
    v[12] = v[ 9] = v[3] = x2;
    v[16] = v[13] = v[7] = y2;

    fi.bytes[0] = 0;
    fi.bytes[1] = 0;
    fi.bytes[2] = 0;
    fi.bytes[3] = alpha;
    v[17] =v[14] = v[11] = v[8] = v[5] = v[2] = fi.f;

    ctx->vb_offset += 6*3 * sizeof(GLfloat);
}

cairo_gl_emit_span_t
_cairo_gl_context_choose_emit_span (cairo_gl_context_t *ctx)
{
    if (ctx->operands[CAIRO_GL_TEX_MASK].type != CAIRO_GL_OPERAND_NONE) {
	    switch (ctx->operands[CAIRO_GL_TEX_MASK].type) {
	    default:
	    case CAIRO_GL_OPERAND_COUNT:
		    ASSERT_NOT_REACHED;
	    case CAIRO_GL_OPERAND_NONE:
	    case CAIRO_GL_OPERAND_CONSTANT:
		    break;

	    case CAIRO_GL_OPERAND_LINEAR_GRADIENT:
	    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_A0:
	    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_NONE:
	    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_EXT:
		    if (!ctx->operands[CAIRO_GL_TEX_MASK].gradient.texgen)
			    return _cairo_gl_composite_emit_span;
		    break;

	    case CAIRO_GL_OPERAND_TEXTURE:
		    if (!ctx->operands[CAIRO_GL_TEX_MASK].texture.texgen)
			    return _cairo_gl_composite_emit_span;
		    break;
	    }
    }

    switch (ctx->operands[CAIRO_GL_TEX_SOURCE].type) {
    default:
    case CAIRO_GL_OPERAND_COUNT:
	ASSERT_NOT_REACHED;
    case CAIRO_GL_OPERAND_NONE:
    case CAIRO_GL_OPERAND_CONSTANT:
	break;

    case CAIRO_GL_OPERAND_LINEAR_GRADIENT:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_A0:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_NONE:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_EXT:
	if (!ctx->operands[CAIRO_GL_TEX_SOURCE].gradient.texgen)
		return _cairo_gl_composite_emit_span;
	break;

    case CAIRO_GL_OPERAND_TEXTURE:
	if (!ctx->operands[CAIRO_GL_TEX_SOURCE].texture.texgen)
		return _cairo_gl_composite_emit_span;
    }

    return _cairo_gl_composite_emit_solid_span;
}

static inline void
_cairo_gl_composite_emit_glyph_vertex (cairo_gl_context_t *ctx,
				       GLfloat x, GLfloat y,
				       GLfloat glyph_x, GLfloat glyph_y)
{
    GLfloat *vb = (GLfloat *) (void *) &ctx->vb[ctx->vb_offset];

    *vb++ = x;
    *vb++ = y;

    _cairo_gl_operand_emit (&ctx->operands[CAIRO_GL_TEX_SOURCE], &vb, x, y);

    *vb++ = glyph_x;
    *vb++ = glyph_y;

    ctx->vb_offset += ctx->vertex_size;
}

static void
_cairo_gl_composite_emit_glyph (cairo_gl_context_t *ctx,
				GLfloat x1, GLfloat y1,
				GLfloat x2, GLfloat y2,
				GLfloat glyph_x1, GLfloat glyph_y1,
				GLfloat glyph_x2, GLfloat glyph_y2)
{
    _cairo_gl_composite_prepare_buffer (ctx, 6,
					CAIRO_GL_PRIMITIVE_TYPE_TRIANGLES);

    _cairo_gl_composite_emit_glyph_vertex (ctx, x1, y1, glyph_x1, glyph_y1);
    _cairo_gl_composite_emit_glyph_vertex (ctx, x2, y1, glyph_x2, glyph_y1);
    _cairo_gl_composite_emit_glyph_vertex (ctx, x1, y2, glyph_x1, glyph_y2);

    _cairo_gl_composite_emit_glyph_vertex (ctx, x2, y1, glyph_x2, glyph_y1);
    _cairo_gl_composite_emit_glyph_vertex (ctx, x2, y2, glyph_x2, glyph_y2);
    _cairo_gl_composite_emit_glyph_vertex (ctx, x1, y2, glyph_x1, glyph_y2);
}

static void
_cairo_gl_composite_emit_solid_glyph (cairo_gl_context_t *ctx,
				      GLfloat x1, GLfloat y1,
				      GLfloat x2, GLfloat y2,
				      GLfloat glyph_x1, GLfloat glyph_y1,
				      GLfloat glyph_x2, GLfloat glyph_y2)
{
    GLfloat *v;

    _cairo_gl_composite_prepare_buffer (ctx, 6,
					CAIRO_GL_PRIMITIVE_TYPE_TRIANGLES);

    v = (GLfloat *) (void *) &ctx->vb[ctx->vb_offset];

    v[20] = v[ 8] = v[0] = x1;
    v[13] = v[ 5] = v[1] = y1;
    v[22] = v[10] = v[2] = glyph_x1;
    v[15] = v[ 7] = v[3] = glyph_y1;

    v[16] = v[12] = v[4] = x2;
    v[18] = v[14] = v[6] = glyph_x2;

    v[21] = v[17] = v[ 9] = y2;
    v[23] = v[19] = v[11] = glyph_y2;

    ctx->vb_offset += 4 * 6 * sizeof (GLfloat);
}

cairo_gl_emit_glyph_t
_cairo_gl_context_choose_emit_glyph (cairo_gl_context_t *ctx)
{
    switch (ctx->operands[CAIRO_GL_TEX_SOURCE].type) {
    default:
    case CAIRO_GL_OPERAND_COUNT:
	ASSERT_NOT_REACHED;
    case CAIRO_GL_OPERAND_NONE:
    case CAIRO_GL_OPERAND_CONSTANT:
	return _cairo_gl_composite_emit_solid_glyph;

    case CAIRO_GL_OPERAND_LINEAR_GRADIENT:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_A0:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_NONE:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_EXT:
    case CAIRO_GL_OPERAND_TEXTURE:
	return _cairo_gl_composite_emit_glyph;
    }
}

void
_cairo_gl_composite_fini (cairo_gl_composite_t *setup)
{
    _cairo_gl_operand_destroy (&setup->src);
    _cairo_gl_operand_destroy (&setup->mask);
}

cairo_status_t
_cairo_gl_composite_set_operator (cairo_gl_composite_t *setup,
				  cairo_operator_t op,
				  cairo_bool_t assume_component_alpha)
{
    if (assume_component_alpha) {
	if (op != CAIRO_OPERATOR_CLEAR &&
	    op != CAIRO_OPERATOR_OVER &&
	    op != CAIRO_OPERATOR_ADD)
	    return UNSUPPORTED ("unsupported component alpha operator");
    } else {
	if (! _cairo_gl_operator_is_supported (op))
	    return UNSUPPORTED ("unsupported operator");
    }

    setup->op = op;
    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gl_composite_init (cairo_gl_composite_t *setup,
			  cairo_operator_t op,
			  cairo_gl_surface_t *dst,
			  cairo_bool_t assume_component_alpha)
{
    cairo_status_t status;

    status = _blit_texture_to_renderbuffer (dst);

    memset (setup, 0, sizeof (cairo_gl_composite_t));

    status = _cairo_gl_composite_set_operator (setup, op,
					       assume_component_alpha);
    if (status)
	return status;

    setup->dst = dst;
    setup->clip_region = dst->clip_region;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_gl_composite_append_vertex_indices (cairo_gl_context_t	*ctx,
					   int			 number_of_new_indices)
{
    cairo_int_status_t status = CAIRO_INT_STATUS_SUCCESS;
    cairo_array_t *indices = &ctx->tristrip_indices;
    int number_of_indices = _cairo_array_num_elements (indices);
    unsigned short current_vertex_index = 0;
    int i;

    assert (number_of_new_indices > 0);

    /* If any preexisting triangle triangle strip indices exist on this
       context, we insert a set of degenerate triangles from the last
       preexisting vertex to our first one. */
    if (number_of_indices > 0) {
	const unsigned short *indices_array = _cairo_array_index_const (indices, 0);
	current_vertex_index = indices_array[number_of_indices - 1];

	status = _cairo_array_append (indices, &current_vertex_index);
	if (unlikely (status))
	    return status;

	current_vertex_index++;
	status =_cairo_array_append (indices, &current_vertex_index);
	if (unlikely (status))
	    return status;
    }

    for (i = 0; i < number_of_new_indices; i++) {
	status = _cairo_array_append (indices, &current_vertex_index);
	current_vertex_index++;
	if (unlikely (status))
	    return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

cairo_int_status_t
_cairo_gl_composite_emit_quad_as_tristrip (cairo_gl_context_t	*ctx,
					   cairo_gl_composite_t	*setup,
					   const cairo_point_t	quad[4])
{
    _cairo_gl_composite_prepare_buffer (ctx, 4,
					CAIRO_GL_PRIMITIVE_TYPE_TRISTRIPS);

    _cairo_gl_composite_emit_point (ctx, &quad[0]);
    _cairo_gl_composite_emit_point (ctx, &quad[1]);

    /* Cairo stores quad vertices in counter-clockwise order, but we need to
       emit them from top to bottom in the triangle strip, so we need to reverse
       the order of the last two vertices. */
    _cairo_gl_composite_emit_point (ctx, &quad[3]);
    _cairo_gl_composite_emit_point (ctx, &quad[2]);

    return _cairo_gl_composite_append_vertex_indices (ctx, 4);
}

cairo_int_status_t
_cairo_gl_composite_emit_triangle_as_tristrip (cairo_gl_context_t	*ctx,
					       cairo_gl_composite_t	*setup,
					       const cairo_point_t	 triangle[3])
{
    _cairo_gl_composite_prepare_buffer (ctx, 3,
					CAIRO_GL_PRIMITIVE_TYPE_TRISTRIPS);

    _cairo_gl_composite_emit_point (ctx, &triangle[0]);
    _cairo_gl_composite_emit_point (ctx, &triangle[1]);
    _cairo_gl_composite_emit_point (ctx, &triangle[2]);
    return _cairo_gl_composite_append_vertex_indices (ctx, 3);
}
