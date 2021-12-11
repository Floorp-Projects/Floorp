/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
 * Copyright © 2005 Red Hat, Inc.
 * Copyright © 2011 Intel Corporation
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
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Henry Song <hsong@sisa.samsung.com>
 *	Martin Robinson <mrobinson@igalia.com>
 */

#include "cairoint.h"

#include "cairo-clip-inline.h"
#include "cairo-composite-rectangles-private.h"
#include "cairo-compositor-private.h"
#include "cairo-gl-private.h"
#include "cairo-path-private.h"
#include "cairo-traps-private.h"

static cairo_bool_t
can_use_msaa_compositor (cairo_gl_surface_t *surface,
			 cairo_antialias_t antialias);

static void
query_surface_capabilities (cairo_gl_surface_t *surface);

struct _tristrip_composite_info {
    cairo_gl_composite_t	setup;
    cairo_gl_context_t		*ctx;
};

static cairo_int_status_t
_draw_trap (cairo_gl_context_t		*ctx,
	    cairo_gl_composite_t	*setup,
	    cairo_trapezoid_t		*trap)
{
    cairo_point_t quad[4];

    quad[0].x = _cairo_edge_compute_intersection_x_for_y (&trap->left.p1,
							  &trap->left.p2,
							  trap->top);
    quad[0].y = trap->top;

    quad[1].x = _cairo_edge_compute_intersection_x_for_y (&trap->left.p1,
						      &trap->left.p2,
						      trap->bottom);
    quad[1].y = trap->bottom;

    quad[2].x = _cairo_edge_compute_intersection_x_for_y (&trap->right.p1,
						      &trap->right.p2,
						      trap->bottom);
    quad[2].y = trap->bottom;

    quad[3].x = _cairo_edge_compute_intersection_x_for_y (&trap->right.p1,
						      &trap->right.p2,
						      trap->top);
    quad[3].y = trap->top;
    return _cairo_gl_composite_emit_quad_as_tristrip (ctx, setup, quad);
}

static cairo_int_status_t
_draw_traps (cairo_gl_context_t		*ctx,
	     cairo_gl_composite_t	*setup,
	     cairo_traps_t		*traps)
{
    cairo_int_status_t status = CAIRO_STATUS_SUCCESS;
    int i;

    for (i = 0; i < traps->num_traps; i++) {
	cairo_trapezoid_t *trap = traps->traps + i;
	if (unlikely ((status = _draw_trap (ctx, setup, trap))))
	    return status;
    }

   return status;
}

static cairo_int_status_t
_draw_int_rect (cairo_gl_context_t	*ctx,
		cairo_gl_composite_t	*setup,
		cairo_rectangle_int_t	*rect)
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
_draw_triangle_fan (cairo_gl_context_t		*ctx,
		    cairo_gl_composite_t	*setup,
		    const cairo_point_t		*midpt,
		    const cairo_point_t		*points,
		    int				 npoints)
{
    int i;

    /* Our strategy here is to not even try to build a triangle fan, but to
       draw each triangle as if it was an unconnected member of a triangle strip. */
    for (i = 1; i < npoints; i++) {
	cairo_int_status_t status;
	cairo_point_t triangle[3];

	triangle[0] = *midpt;
	triangle[1] = points[i - 1];
	triangle[2] = points[i];

	status = _cairo_gl_composite_emit_triangle_as_tristrip (ctx, setup, triangle);
	if (unlikely (status))
	    return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_clip_to_traps (cairo_clip_t *clip,
		cairo_traps_t *traps)
{
    cairo_int_status_t status;
    cairo_polygon_t polygon;
    cairo_antialias_t antialias;
    cairo_fill_rule_t fill_rule;

    _cairo_traps_init (traps);

    if (clip->num_boxes == 1 && clip->path == NULL) {
	cairo_boxes_t boxes;
	_cairo_boxes_init_for_array (&boxes, clip->boxes, clip->num_boxes);
	return _cairo_traps_init_boxes (traps, &boxes);
    }

    status = _cairo_clip_get_polygon (clip, &polygon, &fill_rule, &antialias);
    if (unlikely (status))
	return status;

    /* We ignore the antialias mode of the clip here, since the user requested
     * unantialiased rendering of their path and we expect that this stencil
     * based rendering of the clip to be a reasonable approximation to
     * the intersection between that clip and the path.
     *
     * In other words, what the user expects when they try to perform
     * a geometric intersection between an unantialiased polygon and an
     * antialiased polygon is open to interpretation. And we choose the fast
     * option.
     */

    _cairo_traps_init (traps);
    status = _cairo_bentley_ottmann_tessellate_polygon (traps,
							&polygon,
							fill_rule);
    _cairo_polygon_fini (&polygon);

    return status;
}

cairo_int_status_t
_cairo_gl_msaa_compositor_draw_clip (cairo_gl_context_t *ctx,
				     cairo_gl_composite_t *setup,
				     cairo_clip_t *clip)
{
    cairo_int_status_t status;
    cairo_traps_t traps;

    status = _clip_to_traps (clip, &traps);
    if (unlikely (status))
	return status;
    status = _draw_traps (ctx, setup, &traps);

    _cairo_traps_fini (&traps);
    return status;
}

static cairo_bool_t
_should_use_unbounded_surface (cairo_composite_rectangles_t *composite)
{
    cairo_gl_surface_t *dst = (cairo_gl_surface_t *) composite->surface;
    cairo_rectangle_int_t *source = &composite->source;

    if (composite->is_bounded)
	return FALSE;

    /* This isn't just an optimization. It also detects when painting is used
       to paint back the unbounded surface, preventing infinite recursion. */
    return ! (source->x <= 0 && source->y <= 0 &&
	      source->height + source->y >= dst->height &&
	      source->width + source->x >= dst->width);
}

static cairo_surface_t*
_prepare_unbounded_surface (cairo_gl_surface_t *dst)
{

    cairo_surface_t* surface = cairo_gl_surface_create (dst->base.device,
							dst->base.content,
							dst->width,
							dst->height);
    if (surface == NULL)
	return NULL;
    if (unlikely (surface->status)) {
	cairo_surface_destroy (surface);
	return NULL;
    }
    return surface;
}

static cairo_int_status_t
_paint_back_unbounded_surface (const cairo_compositor_t		*compositor,
			       cairo_composite_rectangles_t	*composite,
			       cairo_surface_t			*surface)
{
    cairo_gl_surface_t *dst = (cairo_gl_surface_t *) composite->surface;
    cairo_int_status_t status;

    cairo_pattern_t *pattern = cairo_pattern_create_for_surface (surface);
    if (unlikely (pattern->status)) {
	status = pattern->status;
	goto finish;
    }

    status = _cairo_compositor_paint (compositor, &dst->base,
				      composite->op, pattern,
				      composite->clip);

finish:
    cairo_pattern_destroy (pattern);
    cairo_surface_destroy (surface);
    return status;
}

static cairo_bool_t
can_use_msaa_compositor (cairo_gl_surface_t *surface,
			 cairo_antialias_t antialias)
{
    cairo_gl_flavor_t gl_flavor = ((cairo_gl_context_t *) surface->base.device)->gl_flavor;

    query_surface_capabilities (surface);
    if (! surface->supports_stencil)
	return FALSE;

    /* Multisampling OpenGL ES surfaces only maintain one multisampling
       framebuffer and thus must use the spans compositor to do non-antialiased
       rendering. */
    if ((gl_flavor == CAIRO_GL_FLAVOR_ES3 ||
	 gl_flavor == CAIRO_GL_FLAVOR_ES2)
	 && surface->supports_msaa
	 && surface->num_samples > 1
	 && antialias == CAIRO_ANTIALIAS_NONE)
	return FALSE;

    /* The MSAA compositor has a single-sample mode, so we can
       support non-antialiased rendering. */
    if (antialias == CAIRO_ANTIALIAS_NONE)
	return TRUE;

    if (antialias == CAIRO_ANTIALIAS_FAST || antialias == CAIRO_ANTIALIAS_DEFAULT)
	return surface->supports_msaa;
    return FALSE;
}

static void
_cairo_gl_msaa_compositor_set_clip (cairo_composite_rectangles_t *composite,
				    cairo_gl_composite_t *setup)
{
    if (_cairo_composite_rectangles_can_reduce_clip (composite, composite->clip))
	return;
    _cairo_gl_composite_set_clip (setup, composite->clip);
}

/* Masking with the SOURCE operator requires two passes. In the first
 * pass we use the mask as the source to get:
 * result = (1 - ma) * dst
 * In the second pass we use the add operator to achieve:
 * result = (src * ma) + dst
 * Combined this produces:
 * result = (src * ma) + (1 - ma) * dst
 */
static cairo_int_status_t
_cairo_gl_msaa_compositor_mask_source_operator (const cairo_compositor_t *compositor,
						cairo_composite_rectangles_t *composite)
{
    cairo_gl_composite_t setup;
    cairo_gl_surface_t *dst = (cairo_gl_surface_t *) composite->surface;
    cairo_gl_context_t *ctx = NULL;
    cairo_int_status_t status;

    cairo_clip_t *clip = composite->clip;
    cairo_traps_t traps;

    /* If we have a non-rectangular clip, we can avoid using the stencil buffer
     * for clipping and just draw the clip polygon. */
    if (clip) {
	status = _clip_to_traps (clip, &traps);
	if (unlikely (status)) {
	    _cairo_traps_fini (&traps);
	    return status;
	}
    }

    status = _cairo_gl_composite_init (&setup,
				       CAIRO_OPERATOR_DEST_OUT,
				       dst,
				       FALSE /* assume_component_alpha */);
    if (unlikely (status))
	return status;
    status = _cairo_gl_composite_set_source (&setup,
					     &composite->mask_pattern.base,
					     &composite->mask_sample_area,
					     &composite->bounded,
					     FALSE);
    if (unlikely (status))
	goto finish;
    _cairo_gl_composite_set_multisample (&setup);
    status = _cairo_gl_composite_begin (&setup, &ctx);
    if (unlikely (status))
	goto finish;

    if (! clip)
	status = _draw_int_rect (ctx, &setup, &composite->bounded);
    else
	status = _draw_traps (ctx, &setup, &traps);
    if (unlikely (status))
	goto finish;

    /* Now draw the second pass. */
    status = _cairo_gl_composite_set_operator (&setup, CAIRO_OPERATOR_ADD,
					       FALSE /* assume_component_alpha */);
    if (unlikely (status))
	goto finish;
    status = _cairo_gl_composite_set_source (&setup,
					     &composite->source_pattern.base,
					     &composite->source_sample_area,
					     &composite->bounded,
					     FALSE);
    if (unlikely (status))
	goto finish;
    status = _cairo_gl_composite_set_mask (&setup,
					   &composite->mask_pattern.base,
					   &composite->source_sample_area,
					   &composite->bounded,
					   FALSE);
    if (unlikely (status))
	goto finish;

    _cairo_gl_context_set_destination (ctx, dst, setup.multisample);

    status = _cairo_gl_set_operands_and_operator (&setup, ctx);
    if (unlikely (status))
	goto finish;

    if (! clip)
	status = _draw_int_rect (ctx, &setup, &composite->bounded);
    else
	status = _draw_traps (ctx, &setup, &traps);

finish:
    _cairo_gl_composite_fini (&setup);
    if (ctx)
	status = _cairo_gl_context_release (ctx, status);
    if (clip)
	_cairo_traps_fini (&traps);

    return status;
}

static cairo_int_status_t
_cairo_gl_msaa_compositor_mask (const cairo_compositor_t	*compositor,
				cairo_composite_rectangles_t	*composite)
{
    cairo_gl_composite_t setup;
    cairo_gl_surface_t *dst = (cairo_gl_surface_t *) composite->surface;
    cairo_gl_context_t *ctx = NULL;
    cairo_int_status_t status;
    cairo_operator_t op = composite->op;
    cairo_clip_t *clip = composite->clip;

    if (! can_use_msaa_compositor (dst, CAIRO_ANTIALIAS_DEFAULT))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (composite->op == CAIRO_OPERATOR_CLEAR &&
	composite->original_mask_pattern != NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* GL compositing operators cannot properly represent a mask operation
       using the SOURCE compositing operator in one pass. This only matters if
       there actually is a mask (there isn't in a paint operation) and if the
       mask isn't totally opaque. */
    if (op == CAIRO_OPERATOR_SOURCE &&
	 composite->original_mask_pattern != NULL &&
	! _cairo_pattern_is_opaque (&composite->mask_pattern.base,
				    &composite->mask_sample_area)) {

	if (! _cairo_pattern_is_opaque (&composite->source_pattern.base,
				      &composite->source_sample_area)) {
	    return _cairo_gl_msaa_compositor_mask_source_operator (compositor, composite);
	}

	/* If the source is opaque the operation reduces to OVER. */
	op = CAIRO_OPERATOR_OVER;
    }

    if (_should_use_unbounded_surface (composite)) {
	cairo_surface_t* surface = _prepare_unbounded_surface (dst);

	if (unlikely (surface == NULL))
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	/* This may be a paint operation. */
	if (composite->original_mask_pattern == NULL) {
	    status = _cairo_compositor_paint (compositor, surface,
					      CAIRO_OPERATOR_SOURCE,
					      &composite->source_pattern.base,
					      NULL);
	} else {
	    status = _cairo_compositor_mask (compositor, surface,
					     CAIRO_OPERATOR_SOURCE,
					     &composite->source_pattern.base,
					     &composite->mask_pattern.base,
					     NULL);
	}

	if (unlikely (status)) {
	    cairo_surface_destroy (surface);
	    return status;
	}

	return _paint_back_unbounded_surface (compositor, composite, surface);
    }

    status = _cairo_gl_composite_init (&setup,
				       op,
				       dst,
				       FALSE /* assume_component_alpha */);
    if (unlikely (status))
	return status;

    status = _cairo_gl_composite_set_source (&setup,
					     &composite->source_pattern.base,
					     &composite->source_sample_area,
					     &composite->bounded,
					     FALSE);
    if (unlikely (status))
	goto finish;

    if (composite->original_mask_pattern != NULL) {
	status = _cairo_gl_composite_set_mask (&setup,
					       &composite->mask_pattern.base,
					       &composite->mask_sample_area,
					       &composite->bounded,
					       FALSE);
    }
    if (unlikely (status))
	goto finish;

    /* We always use multisampling here, because we do not yet have the smarts
       to calculate when the clip or the source requires it. */
     _cairo_gl_composite_set_multisample (&setup);

    status = _cairo_gl_composite_begin (&setup, &ctx);
    if (unlikely (status))
	goto finish;

    if (! clip)
	status = _draw_int_rect (ctx, &setup, &composite->bounded);
    else
	status = _cairo_gl_msaa_compositor_draw_clip (ctx, &setup, clip);

finish:
    _cairo_gl_composite_fini (&setup);

    if (ctx)
	status = _cairo_gl_context_release (ctx, status);

    return status;
}

static cairo_int_status_t
_cairo_gl_msaa_compositor_paint (const cairo_compositor_t	*compositor,
				 cairo_composite_rectangles_t	*composite)
{
    return _cairo_gl_msaa_compositor_mask (compositor, composite);
}

static cairo_status_t
_stroke_shaper_add_triangle (void			*closure,
			     const cairo_point_t	 triangle[3])
{
    struct _tristrip_composite_info *info = closure;
    return _cairo_gl_composite_emit_triangle_as_tristrip (info->ctx,
							  &info->setup,
							  triangle);
}

static cairo_status_t
_stroke_shaper_add_triangle_fan (void			*closure,
				 const cairo_point_t	*midpoint,
				 const cairo_point_t	*points,
				 int			 npoints)
{
    struct _tristrip_composite_info *info = closure;
    return _draw_triangle_fan (info->ctx, &info->setup,
			       midpoint, points, npoints);
}

static cairo_status_t
_stroke_shaper_add_quad (void			*closure,
			 const cairo_point_t	 quad[4])
{
    struct _tristrip_composite_info *info = closure;
    return _cairo_gl_composite_emit_quad_as_tristrip (info->ctx, &info->setup,
						      quad);
}

static cairo_int_status_t
_prevent_overlapping_strokes (cairo_gl_context_t		*ctx,
			      cairo_gl_composite_t		*setup,
			      cairo_composite_rectangles_t	*composite,
			      const cairo_path_fixed_t		*path,
			      const cairo_stroke_style_t	*style,
			      const cairo_matrix_t		*ctm)
{
    cairo_rectangle_int_t stroke_extents;

    if (! _cairo_gl_ensure_stencil (ctx, setup->dst))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (_cairo_pattern_is_opaque (&composite->source_pattern.base,
				  &composite->source_sample_area))
	return CAIRO_INT_STATUS_SUCCESS;

   if (glIsEnabled (GL_STENCIL_TEST) == FALSE) {
	cairo_bool_t scissor_was_enabled;

       /* In case we have pending operations we have to flush before
	  adding the stencil buffer. */
       _cairo_gl_composite_flush (ctx);

	/* Enable the stencil buffer, even if we are not using it for clipping,
	   so we can use it below to prevent overlapping shapes. We initialize
	   it all to one here which represents infinite clip. */
	glDepthMask (GL_TRUE);
	glEnable (GL_STENCIL_TEST);

	/* We scissor here so that we don't have to clear the entire stencil
	 * buffer. If the scissor test is already enabled, it was enabled
	 * for clipping. In that case, instead of calculating an intersection,
	 * we just reuse it, and risk clearing too much. */
	scissor_was_enabled = glIsEnabled (GL_SCISSOR_TEST);
	if (! scissor_was_enabled) {
	    _cairo_path_fixed_approximate_stroke_extents (path, style, ctm,
							  FALSE, /* is_vector */
							  &stroke_extents);
	    _cairo_gl_scissor_to_rectangle (setup->dst, &stroke_extents);
	}
	glClearStencil (1);
	glClear (GL_STENCIL_BUFFER_BIT);
	if (! scissor_was_enabled)
	    glDisable (GL_SCISSOR_TEST);

	glStencilFunc (GL_EQUAL, 1, 1);
    }

    /* This means that once we draw to a particular pixel nothing else can
       be drawn there until the stencil buffer is reset or the stencil test
       is disabled. */
    glStencilOp (GL_ZERO, GL_ZERO, GL_ZERO);

    _cairo_clip_destroy (setup->dst->clip_on_stencil_buffer);
    setup->dst->clip_on_stencil_buffer = NULL;

    return CAIRO_INT_STATUS_SUCCESS;
}

static void
query_surface_capabilities (cairo_gl_surface_t *surface)
{
    GLint samples, stencil_bits;
    cairo_gl_context_t *ctx;
    cairo_int_status_t status;

    /* Texture surfaces are create in such a way that they always
       have stencil and multisample bits if possible, so we don't
       need to query their capabilities lazily. */
    if (_cairo_gl_surface_is_texture (surface))
	return;
    if (surface->stencil_and_msaa_caps_initialized)
	return;

    surface->stencil_and_msaa_caps_initialized = TRUE;
    surface->supports_stencil = FALSE;
    surface->supports_msaa = FALSE;

    status = _cairo_gl_context_acquire (surface->base.device, &ctx);
    if (unlikely (status))
	return;

    _cairo_gl_context_set_destination (ctx, surface, FALSE);

    glGetIntegerv(GL_SAMPLES, &samples);
    glGetIntegerv(GL_STENCIL_BITS, &stencil_bits);
    surface->supports_stencil = stencil_bits > 0;
    surface->supports_msaa = samples > 1;
    surface->num_samples = samples;

    status = _cairo_gl_context_release (ctx, status);
}

static cairo_int_status_t
_cairo_gl_msaa_compositor_stroke (const cairo_compositor_t	*compositor,
				  cairo_composite_rectangles_t	*composite,
				  const cairo_path_fixed_t	*path,
				  const cairo_stroke_style_t	*style,
				  const cairo_matrix_t		*ctm,
				  const cairo_matrix_t		*ctm_inverse,
				  double			 tolerance,
				  cairo_antialias_t		 antialias)
{
    cairo_int_status_t status;
    cairo_gl_surface_t *dst = (cairo_gl_surface_t *) composite->surface;
    struct _tristrip_composite_info info;

    if (! can_use_msaa_compositor (dst, antialias))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (composite->is_bounded == FALSE) {
	cairo_surface_t* surface = _prepare_unbounded_surface (dst);

	if (unlikely (surface == NULL))
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	status = _cairo_compositor_stroke (compositor, surface,
					   CAIRO_OPERATOR_SOURCE,
					   &composite->source_pattern.base,
					   path, style, ctm, ctm_inverse,
					   tolerance, antialias, NULL);
	if (unlikely (status)) {
	    cairo_surface_destroy (surface);
	    return status;
	}

	return _paint_back_unbounded_surface (compositor, composite, surface);
    }

    status = _cairo_gl_composite_init (&info.setup,
				       composite->op,
				       dst,
				       FALSE /* assume_component_alpha */);
    if (unlikely (status))
	return status;

    info.ctx = NULL;

    status = _cairo_gl_composite_set_source (&info.setup,
					     &composite->source_pattern.base,
					     &composite->source_sample_area,
					     &composite->bounded,
					     FALSE);
    if (unlikely (status))
	goto finish;

    _cairo_gl_msaa_compositor_set_clip (composite, &info.setup);
    if (antialias != CAIRO_ANTIALIAS_NONE)
	_cairo_gl_composite_set_multisample (&info.setup);

    status = _cairo_gl_composite_begin (&info.setup, &info.ctx);
    if (unlikely (status))
	goto finish;

    status = _prevent_overlapping_strokes (info.ctx, &info.setup,
					   composite, path, style, ctm);
    if (unlikely (status))
	goto finish;

    status = _cairo_path_fixed_stroke_to_shaper ((cairo_path_fixed_t *) path,
						 style,
						 ctm,
						 ctm_inverse,
						 tolerance,
						 _stroke_shaper_add_triangle,
						 _stroke_shaper_add_triangle_fan,
						 _stroke_shaper_add_quad,
						 &info);
    if (unlikely (status))
	goto finish;

finish:
    _cairo_gl_composite_fini (&info.setup);

    if (info.ctx)
	status = _cairo_gl_context_release (info.ctx, status);

    return status;
}

static cairo_int_status_t
_draw_simple_quad_path (cairo_gl_context_t *ctx,
			cairo_gl_composite_t *setup,
			const cairo_path_fixed_t *path)
{
    cairo_point_t triangle[3];
    cairo_int_status_t status;
    const cairo_point_t *points;

    points = cairo_path_head (path)->points;
    triangle[0] = points[0];
    triangle[1] = points[1];
    triangle[2] = points[2];
    status = _cairo_gl_composite_emit_triangle_as_tristrip (ctx, setup, triangle);
    if (status)
	return status;

    triangle[0] = points[2];
    triangle[1] = points[3];
    triangle[2] = points[0];
    return _cairo_gl_composite_emit_triangle_as_tristrip (ctx, setup, triangle);
}

static cairo_int_status_t
_cairo_gl_msaa_compositor_fill (const cairo_compositor_t	*compositor,
				cairo_composite_rectangles_t	*composite,
				const cairo_path_fixed_t	*path,
				cairo_fill_rule_t		 fill_rule,
				double				 tolerance,
				cairo_antialias_t		 antialias)
{
    cairo_gl_composite_t setup;
    cairo_gl_surface_t *dst = (cairo_gl_surface_t *) composite->surface;
    cairo_gl_context_t *ctx = NULL;
    cairo_int_status_t status;
    cairo_traps_t traps;
    cairo_bool_t draw_path_with_traps;

    if (! can_use_msaa_compositor (dst, antialias))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (composite->is_bounded == FALSE) {
	cairo_surface_t* surface = _prepare_unbounded_surface (dst);

	if (unlikely (surface == NULL))
	    return CAIRO_INT_STATUS_UNSUPPORTED;


	status = _cairo_compositor_fill (compositor, surface,
					 CAIRO_OPERATOR_SOURCE,
					 &composite->source_pattern.base,
					 path, fill_rule, tolerance,
					 antialias, NULL);

	if (unlikely (status)) {
	    cairo_surface_destroy (surface);
	    return status;
	}

	return _paint_back_unbounded_surface (compositor, composite, surface);
    }

    draw_path_with_traps = ! _cairo_path_fixed_is_simple_quad (path);

    if (draw_path_with_traps) {
	_cairo_traps_init (&traps);
	status = _cairo_path_fixed_fill_to_traps (path, fill_rule, tolerance, &traps);
	if (unlikely (status))
	    goto cleanup_traps;
    }

    status = _cairo_gl_composite_init (&setup,
				       composite->op,
				       dst,
				       FALSE /* assume_component_alpha */);
    if (unlikely (status))
	goto cleanup_traps;

    status = _cairo_gl_composite_set_source (&setup,
					     &composite->source_pattern.base,
					     &composite->source_sample_area,
					     &composite->bounded,
					     FALSE);
    if (unlikely (status))
	goto cleanup_setup;

    _cairo_gl_msaa_compositor_set_clip (composite, &setup);
    if (antialias != CAIRO_ANTIALIAS_NONE)
	_cairo_gl_composite_set_multisample (&setup);

    status = _cairo_gl_composite_begin (&setup, &ctx);
    if (unlikely (status))
	goto cleanup_setup;

    if (! draw_path_with_traps)
	status = _draw_simple_quad_path (ctx, &setup, path);
    else
	status = _draw_traps (ctx, &setup, &traps);
    if (unlikely (status))
	goto cleanup_setup;

cleanup_setup:
    _cairo_gl_composite_fini (&setup);

    if (ctx)
	status = _cairo_gl_context_release (ctx, status);

cleanup_traps:
    if (draw_path_with_traps)
	_cairo_traps_fini (&traps);

    return status;
}

static cairo_int_status_t
_cairo_gl_msaa_compositor_glyphs (const cairo_compositor_t	*compositor,
				  cairo_composite_rectangles_t	*composite,
				  cairo_scaled_font_t		*scaled_font,
				  cairo_glyph_t			*glyphs,
				  int				 num_glyphs,
				  cairo_bool_t			 overlap)
{
    cairo_int_status_t status;
    cairo_surface_t *src = NULL;
    int src_x, src_y;
    cairo_composite_glyphs_info_t info;

    cairo_gl_surface_t *dst = (cairo_gl_surface_t *) composite->surface;

    query_surface_capabilities (dst);
    if (! dst->supports_stencil)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (composite->op == CAIRO_OPERATOR_CLEAR)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (composite->is_bounded == FALSE) {
	cairo_surface_t* surface = _prepare_unbounded_surface (dst);

	if (unlikely (surface == NULL))
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	status = _cairo_compositor_glyphs (compositor, surface,
					   CAIRO_OPERATOR_SOURCE,
					   &composite->source_pattern.base,
					   glyphs, num_glyphs,
					   scaled_font, composite->clip);

	if (unlikely (status)) {
	    cairo_surface_destroy (surface);
	    return status;
	}

	return _paint_back_unbounded_surface (compositor, composite, surface);
    }

    src = _cairo_gl_pattern_to_source (&dst->base,
				       &composite->source_pattern.base,
				       FALSE,
				       &composite->bounded,
				       &composite->source_sample_area,
				       &src_x, &src_y);
    if (unlikely (src->status)) {
	status = src->status;
	goto finish;
    }

    status = _cairo_gl_check_composite_glyphs (composite,
					       scaled_font, glyphs,
					       &num_glyphs);
    if (unlikely (status != CAIRO_INT_STATUS_SUCCESS))
	goto finish;

    info.font = scaled_font;
    info.glyphs = glyphs;
    info.num_glyphs = num_glyphs;
    info.use_mask = overlap || ! composite->is_bounded ||
		    composite->op == CAIRO_OPERATOR_SOURCE;
    info.extents = composite->bounded;

    _cairo_scaled_font_freeze_cache (scaled_font);
    status = _cairo_gl_composite_glyphs_with_clip (dst, composite->op,
						   src, src_x, src_y,
						   0, 0, &info,
						   composite->clip);

    _cairo_scaled_font_thaw_cache (scaled_font);

finish:
    if (src)
	cairo_surface_destroy (src);

    return status;
}

static void
_cairo_gl_msaa_compositor_init (cairo_compositor_t	 *compositor,
				const cairo_compositor_t *delegate)
{
    compositor->delegate = delegate;

    compositor->paint = _cairo_gl_msaa_compositor_paint;
    compositor->mask = _cairo_gl_msaa_compositor_mask;
    compositor->fill = _cairo_gl_msaa_compositor_fill;
    compositor->stroke = _cairo_gl_msaa_compositor_stroke;
    compositor->glyphs = _cairo_gl_msaa_compositor_glyphs;
}

const cairo_compositor_t *
_cairo_gl_msaa_compositor_get (void)
{
    static cairo_atomic_once_t once = CAIRO_ATOMIC_ONCE_INIT;
    static cairo_compositor_t compositor;
    if (_cairo_atomic_init_once_enter(&once)) {
	_cairo_gl_msaa_compositor_init (&compositor,
					_cairo_gl_span_compositor_get ());
	_cairo_atomic_init_once_leave(&once);
    }

    return &compositor;
}
