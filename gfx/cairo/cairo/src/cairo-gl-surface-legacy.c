/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 Eric Anholt
 * Copyright © 2009 Chris Wilson
 * Copyright © 2005,2010 Red Hat, Inc
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
 */

#include "cairoint.h"

#include "cairo-composite-rectangles-private.h"
#include "cairo-default-context-private.h"
#include "cairo-error-private.h"
#include "cairo-gl-private.h"
#include "cairo-image-surface-inline.h"

cairo_status_t
_cairo_gl_surface_acquire_dest_image (void		      *abstract_surface,
				      cairo_rectangle_int_t   *interest_rect,
				      cairo_image_surface_t  **image_out,
				      cairo_rectangle_int_t   *image_rect_out,
				      void		     **image_extra)
{
    cairo_gl_surface_t *surface = abstract_surface;
    cairo_int_status_t status;

    status = _cairo_gl_surface_deferred_clear (surface);
    if (unlikely (status))
	return status;

    *image_extra = NULL;
    return _cairo_gl_surface_get_image (surface, interest_rect, image_out,
					image_rect_out);
}

void
_cairo_gl_surface_release_dest_image (void		      *abstract_surface,
				      cairo_rectangle_int_t   *interest_rect,
				      cairo_image_surface_t   *image,
				      cairo_rectangle_int_t   *image_rect,
				      void		      *image_extra)
{
    cairo_status_t status;

    status = _cairo_gl_surface_draw_image (abstract_surface, image,
					   0, 0,
					   image->width, image->height,
					   image_rect->x, image_rect->y,
					   TRUE);
    /* as we created the image, its format should be directly applicable */
    assert (status == CAIRO_STATUS_SUCCESS);

    cairo_surface_destroy (&image->base);
}

cairo_status_t
_cairo_gl_surface_clone_similar (void		     *abstract_surface,
				 cairo_surface_t     *src,
				 int                  src_x,
				 int                  src_y,
				 int                  width,
				 int                  height,
				 int                 *clone_offset_x,
				 int                 *clone_offset_y,
				 cairo_surface_t    **clone_out)
{
    cairo_gl_surface_t *surface = abstract_surface;
    cairo_int_status_t status;

    /* XXX: Use GLCopyTexImage2D to clone non-texture-surfaces */
    if (src->device == surface->base.device &&
	_cairo_gl_surface_is_texture ((cairo_gl_surface_t *) src)) {
	status = _cairo_gl_surface_deferred_clear ((cairo_gl_surface_t *)src);
	if (unlikely (status))
	    return status;

	*clone_offset_x = 0;
	*clone_offset_y = 0;
	*clone_out = cairo_surface_reference (src);

	return CAIRO_STATUS_SUCCESS;
    } else if (_cairo_surface_is_image (src)) {
	cairo_image_surface_t *image_src = (cairo_image_surface_t *)src;
	cairo_gl_surface_t *clone;

	clone = (cairo_gl_surface_t *)
		_cairo_gl_surface_create_similar (&surface->base,
						  src->content,
						  width, height);
	if (clone == NULL)
	    return UNSUPPORTED ("create_similar failed");
	if (clone->base.status)
	    return clone->base.status;

	status = _cairo_gl_surface_draw_image (clone, image_src,
					       src_x, src_y,
					       width, height,
					       0, 0, TRUE);
	if (status) {
	    cairo_surface_destroy (&clone->base);
	    return status;
	}

	*clone_out = &clone->base;
	*clone_offset_x = src_x;
	*clone_offset_y = src_y;

	return CAIRO_STATUS_SUCCESS;
    }

    return UNSUPPORTED ("unknown src surface type in clone_similar");
}

/* Creates a cairo-gl pattern surface for the given trapezoids */
static cairo_status_t
_cairo_gl_get_traps_pattern (cairo_gl_surface_t *dst,
			     int dst_x, int dst_y,
			     int width, int height,
			     cairo_trapezoid_t *traps,
			     int num_traps,
			     cairo_antialias_t antialias,
			     cairo_surface_pattern_t *pattern)
{
    pixman_format_code_t pixman_format;
    pixman_image_t *image;
    cairo_surface_t *surface;
    int i;

    pixman_format = antialias != CAIRO_ANTIALIAS_NONE ? PIXMAN_a8 : PIXMAN_a1,
    image = pixman_image_create_bits (pixman_format, width, height, NULL, 0);
    if (unlikely (image == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    for (i = 0; i < num_traps; i++) {
	pixman_trapezoid_t trap;

	trap.top = _cairo_fixed_to_16_16 (traps[i].top);
	trap.bottom = _cairo_fixed_to_16_16 (traps[i].bottom);

	trap.left.p1.x = _cairo_fixed_to_16_16 (traps[i].left.p1.x);
	trap.left.p1.y = _cairo_fixed_to_16_16 (traps[i].left.p1.y);
	trap.left.p2.x = _cairo_fixed_to_16_16 (traps[i].left.p2.x);
	trap.left.p2.y = _cairo_fixed_to_16_16 (traps[i].left.p2.y);

	trap.right.p1.x = _cairo_fixed_to_16_16 (traps[i].right.p1.x);
	trap.right.p1.y = _cairo_fixed_to_16_16 (traps[i].right.p1.y);
	trap.right.p2.x = _cairo_fixed_to_16_16 (traps[i].right.p2.x);
	trap.right.p2.y = _cairo_fixed_to_16_16 (traps[i].right.p2.y);

	pixman_rasterize_trapezoid (image, &trap, -dst_x, -dst_y);
    }

    surface = _cairo_image_surface_create_for_pixman_image (image,
							    pixman_format);
    if (unlikely (surface->status)) {
	pixman_image_unref (image);
	return surface->status;
    }

    _cairo_pattern_init_for_surface (pattern, surface);
    cairo_surface_destroy (surface);

    return CAIRO_STATUS_SUCCESS;
}

cairo_int_status_t
_cairo_gl_surface_composite (cairo_operator_t		  op,
			     const cairo_pattern_t	 *src,
			     const cairo_pattern_t	 *mask,
			     void			 *abstract_dst,
			     int			  src_x,
			     int			  src_y,
			     int			  mask_x,
			     int			  mask_y,
			     int			  dst_x,
			     int			  dst_y,
			     unsigned int		  width,
			     unsigned int		  height,
			     cairo_region_t		 *clip_region)
{
    cairo_gl_surface_t *dst = abstract_dst;
    cairo_gl_context_t *ctx;
    cairo_status_t status;
    cairo_gl_composite_t setup;
    cairo_rectangle_int_t rect = { dst_x, dst_y, width, height };
    int dx, dy;

    status = _cairo_gl_surface_deferred_clear (dst);
    if (unlikely (status))
	    return status;

    if (op == CAIRO_OPERATOR_SOURCE &&
	mask == NULL &&
	src->type == CAIRO_PATTERN_TYPE_SURFACE &&
	_cairo_surface_is_image (((cairo_surface_pattern_t *) src)->surface) &&
	_cairo_matrix_is_integer_translation (&src->matrix, &dx, &dy)) {
	cairo_image_surface_t *image = (cairo_image_surface_t *)
	    ((cairo_surface_pattern_t *) src)->surface;
	dx += src_x;
	dy += src_y;
	if (dx >= 0 &&
	    dy >= 0 &&
	    dx + width <= (unsigned int) image->width &&
	    dy + height <= (unsigned int) image->height) {
	    status = _cairo_gl_surface_draw_image (dst, image,
						   dx, dy,
						   width, height,
						   dst_x, dst_y, TRUE);
	    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
		return status;
	}
    }

    status = _cairo_gl_composite_init (&setup, op, dst,
				       mask && mask->has_component_alpha,
				       &rect);
    if (unlikely (status))
	goto CLEANUP;

    status = _cairo_gl_composite_set_source (&setup, src,
					     src_x, src_y,
					     dst_x, dst_y,
					     width, height);
    if (unlikely (status))
	goto CLEANUP;

    status = _cairo_gl_composite_set_mask (&setup, mask,
					   mask_x, mask_y,
					   dst_x, dst_y,
					   width, height);
    if (unlikely (status))
	goto CLEANUP;

    status = _cairo_gl_composite_begin (&setup, &ctx);
    if (unlikely (status))
	goto CLEANUP;

    if (clip_region != NULL) {
	int i, num_rectangles;

	num_rectangles = cairo_region_num_rectangles (clip_region);

	for (i = 0; i < num_rectangles; i++) {
	    cairo_rectangle_int_t rect;

	    cairo_region_get_rectangle (clip_region, i, &rect);
	    _cairo_gl_composite_emit_rect (ctx,
					   rect.x,              rect.y,
					   rect.x + rect.width, rect.y + rect.height,
					   0);
	}
    } else {
	_cairo_gl_composite_emit_rect (ctx,
				       dst_x,         dst_y,
				       dst_x + width, dst_y + height,
				       0);
    }

    status = _cairo_gl_context_release (ctx, status);

  CLEANUP:
    _cairo_gl_composite_fini (&setup);

    return status;
}

cairo_int_status_t
_cairo_gl_surface_composite_trapezoids (cairo_operator_t op,
					const cairo_pattern_t *pattern,
					void *abstract_dst,
					cairo_antialias_t antialias,
					int src_x, int src_y,
					int dst_x, int dst_y,
					unsigned int width,
					unsigned int height,
					cairo_trapezoid_t *traps,
					int num_traps,
					cairo_region_t *clip_region)
{
    cairo_gl_surface_t *dst = abstract_dst;
    cairo_surface_pattern_t traps_pattern;
    cairo_int_status_t status;

    if (! _cairo_gl_operator_is_supported (op))
	return UNSUPPORTED ("unsupported operator");

    status = _cairo_gl_surface_deferred_clear (dst);
    if (unlikely (status))
	    return status;

    status = _cairo_gl_get_traps_pattern (dst,
					  dst_x, dst_y, width, height,
					  traps, num_traps, antialias,
					  &traps_pattern);
    if (unlikely (status))
	return status;

    status = _cairo_gl_surface_composite (op,
					  pattern, &traps_pattern.base, dst,
					  src_x, src_y,
					  0, 0,
					  dst_x, dst_y,
					  width, height,
					  clip_region);

    _cairo_pattern_fini (&traps_pattern.base);

    assert (status != CAIRO_INT_STATUS_UNSUPPORTED);
    return status;
}

cairo_int_status_t
_cairo_gl_surface_fill_rectangles (void			   *abstract_dst,
				   cairo_operator_t	    op,
				   const cairo_color_t     *color,
				   cairo_rectangle_int_t   *rects,
				   int			    num_rects)
{
    cairo_gl_surface_t *dst = abstract_dst;
    cairo_solid_pattern_t solid;
    cairo_gl_context_t *ctx;
    cairo_status_t status;
    cairo_gl_composite_t setup;
    int i;

    status = _cairo_gl_surface_deferred_clear (dst);
    if (unlikely (status))
	    return status;

    status = _cairo_gl_composite_init (&setup, op, dst,
				       FALSE,
				       /* XXX */ NULL);
    if (unlikely (status))
	goto CLEANUP;

    _cairo_pattern_init_solid (&solid, color);
    status = _cairo_gl_composite_set_source (&setup, &solid.base,
					     0, 0,
					     0, 0,
					     0, 0);
    if (unlikely (status))
	goto CLEANUP;

    status = _cairo_gl_composite_set_mask (&setup, NULL,
					   0, 0,
					   0, 0,
					   0, 0);
    if (unlikely (status))
	goto CLEANUP;

    status = _cairo_gl_composite_begin (&setup, &ctx);
    if (unlikely (status))
	goto CLEANUP;

    for (i = 0; i < num_rects; i++) {
	_cairo_gl_composite_emit_rect (ctx,
				       rects[i].x,
				       rects[i].y,
				       rects[i].x + rects[i].width,
				       rects[i].y + rects[i].height,
				       0);
    }

    status = _cairo_gl_context_release (ctx, status);

  CLEANUP:
    _cairo_gl_composite_fini (&setup);

    return status;
}

typedef struct _cairo_gl_surface_span_renderer {
    cairo_span_renderer_t base;

    cairo_gl_composite_t setup;

    int xmin, xmax;
    int ymin, ymax;

    cairo_gl_context_t *ctx;
} cairo_gl_surface_span_renderer_t;

static cairo_status_t
_cairo_gl_render_bounded_spans (void *abstract_renderer,
				int y, int height,
				const cairo_half_open_span_t *spans,
				unsigned num_spans)
{
    cairo_gl_surface_span_renderer_t *renderer = abstract_renderer;

    if (num_spans == 0)
	return CAIRO_STATUS_SUCCESS;

    do {
	if (spans[0].coverage) {
	    _cairo_gl_composite_emit_rect (renderer->ctx,
					   spans[0].x, y,
					   spans[1].x, y + height,
					   spans[0].coverage);
	}

	spans++;
    } while (--num_spans > 1);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gl_render_unbounded_spans (void *abstract_renderer,
				  int y, int height,
				  const cairo_half_open_span_t *spans,
				  unsigned num_spans)
{
    cairo_gl_surface_span_renderer_t *renderer = abstract_renderer;

    if (y > renderer->ymin) {
	_cairo_gl_composite_emit_rect (renderer->ctx,
				       renderer->xmin, renderer->ymin,
				       renderer->xmax, y,
				       0);
    }

    if (num_spans == 0) {
	_cairo_gl_composite_emit_rect (renderer->ctx,
				       renderer->xmin, y,
				       renderer->xmax, y + height,
				       0);
    } else {
	if (spans[0].x != renderer->xmin) {
	    _cairo_gl_composite_emit_rect (renderer->ctx,
					   renderer->xmin, y,
					   spans[0].x,     y + height,
					   0);
	}

	do {
	    _cairo_gl_composite_emit_rect (renderer->ctx,
					   spans[0].x, y,
					   spans[1].x, y + height,
					   spans[0].coverage);
	    spans++;
	} while (--num_spans > 1);

	if (spans[0].x != renderer->xmax) {
	    _cairo_gl_composite_emit_rect (renderer->ctx,
					   spans[0].x,     y,
					   renderer->xmax, y + height,
					   0);
	}
    }

    renderer->ymin = y + height;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gl_finish_unbounded_spans (void *abstract_renderer)
{
    cairo_gl_surface_span_renderer_t *renderer = abstract_renderer;

    if (renderer->ymax > renderer->ymin) {
	_cairo_gl_composite_emit_rect (renderer->ctx,
				       renderer->xmin, renderer->ymin,
				       renderer->xmax, renderer->ymax,
				       0);
    }

    return _cairo_gl_context_release (renderer->ctx, CAIRO_STATUS_SUCCESS);
}

static cairo_status_t
_cairo_gl_finish_bounded_spans (void *abstract_renderer)
{
    cairo_gl_surface_span_renderer_t *renderer = abstract_renderer;

    return _cairo_gl_context_release (renderer->ctx, CAIRO_STATUS_SUCCESS);
}

static void
_cairo_gl_surface_span_renderer_destroy (void *abstract_renderer)
{
    cairo_gl_surface_span_renderer_t *renderer = abstract_renderer;

    if (!renderer)
	return;

    _cairo_gl_composite_fini (&renderer->setup);

    free (renderer);
}

cairo_bool_t
_cairo_gl_surface_check_span_renderer (cairo_operator_t	       op,
				       const cairo_pattern_t  *pattern,
				       void		      *abstract_dst,
				       cairo_antialias_t       antialias)
{
    if (! _cairo_gl_operator_is_supported (op))
	return FALSE;

    return TRUE;

    (void) pattern;
    (void) abstract_dst;
    (void) antialias;
}

cairo_span_renderer_t *
_cairo_gl_surface_create_span_renderer (cairo_operator_t	 op,
					const cairo_pattern_t	*src,
					void			*abstract_dst,
					cairo_antialias_t	 antialias,
					const cairo_composite_rectangles_t *rects)
{
    cairo_gl_surface_t *dst = abstract_dst;
    cairo_gl_surface_span_renderer_t *renderer;
    cairo_status_t status;
    const cairo_rectangle_int_t *extents;

    status = _cairo_gl_surface_deferred_clear (dst);
    if (unlikely (status))
	return _cairo_span_renderer_create_in_error (status);

    renderer = calloc (1, sizeof (*renderer));
    if (unlikely (renderer == NULL))
	return _cairo_span_renderer_create_in_error (CAIRO_STATUS_NO_MEMORY);

    renderer->base.destroy = _cairo_gl_surface_span_renderer_destroy;
    if (rects->is_bounded) {
	renderer->base.render_rows = _cairo_gl_render_bounded_spans;
	renderer->base.finish =      _cairo_gl_finish_bounded_spans;
	extents = &rects->bounded;
    } else {
	renderer->base.render_rows = _cairo_gl_render_unbounded_spans;
	renderer->base.finish =      _cairo_gl_finish_unbounded_spans;
	extents = &rects->unbounded;
    }
    renderer->xmin = extents->x;
    renderer->xmax = extents->x + extents->width;
    renderer->ymin = extents->y;
    renderer->ymax = extents->y + extents->height;

    status = _cairo_gl_composite_init (&renderer->setup,
				       op, dst,
				       FALSE, extents);
    if (unlikely (status))
	goto FAIL;

    status = _cairo_gl_composite_set_source (&renderer->setup, src,
					     extents->x, extents->y,
					     extents->x, extents->y,
					     extents->width, extents->height);
    if (unlikely (status))
	goto FAIL;

    _cairo_gl_composite_set_spans (&renderer->setup);
    _cairo_gl_composite_set_clip_region (&renderer->setup,
					 _cairo_clip_get_region (rects->clip));

    status = _cairo_gl_composite_begin (&renderer->setup, &renderer->ctx);
    if (unlikely (status))
	goto FAIL;

    return &renderer->base;

FAIL:
    _cairo_gl_composite_fini (&renderer->setup);
    free (renderer);
    return _cairo_span_renderer_create_in_error (status);
}
