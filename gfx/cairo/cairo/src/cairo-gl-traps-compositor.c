/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 Eric Anholt
 * Copyright © 2009 Chris Wilson
 * Copyright © 2005,2010 Red Hat, Inc
 * Copyright © 2011 Intel Corporation
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

#include "cairo-gl-private.h"

#include "cairo-composite-rectangles-private.h"
#include "cairo-compositor-private.h"
#include "cairo-default-context-private.h"
#include "cairo-error-private.h"
#include "cairo-image-surface-private.h"
#include "cairo-spans-compositor-private.h"
#include "cairo-surface-backend-private.h"
#include "cairo-surface-offset-private.h"

static cairo_int_status_t
acquire (void *abstract_dst)
{
    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
release (void *abstract_dst)
{
    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
set_clip_region (void *_surface,
		 cairo_region_t *region)
{
    cairo_gl_surface_t *surface = _surface;

    surface->clip_region = region;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
draw_image_boxes (void *_dst,
		  cairo_image_surface_t *image,
		  cairo_boxes_t *boxes,
		  int dx, int dy)
{
    cairo_gl_surface_t *dst = _dst;
    struct _cairo_boxes_chunk *chunk;
    int i;

    for (chunk = &boxes->chunks; chunk; chunk = chunk->next) {
	for (i = 0; i < chunk->count; i++) {
	    cairo_box_t *b = &chunk->base[i];
	    int x = _cairo_fixed_integer_part (b->p1.x);
	    int y = _cairo_fixed_integer_part (b->p1.y);
	    int w = _cairo_fixed_integer_part (b->p2.x) - x;
	    int h = _cairo_fixed_integer_part (b->p2.y) - y;
	    cairo_status_t status;

	    status = _cairo_gl_surface_draw_image (dst, image,
						   x + dx, y + dy,
						   w, h,
						   x, y,
						   TRUE);
	    if (unlikely (status))
		return status;
	}
    }

    return CAIRO_STATUS_SUCCESS;
}

static void
emit_aligned_boxes (cairo_gl_context_t *ctx,
		    const cairo_boxes_t *boxes)
{
    const struct _cairo_boxes_chunk *chunk;
    cairo_gl_emit_rect_t emit = _cairo_gl_context_choose_emit_rect (ctx);
    int i;

    for (chunk = &boxes->chunks; chunk; chunk = chunk->next) {
	for (i = 0; i < chunk->count; i++) {
	    int x1 = _cairo_fixed_integer_part (chunk->base[i].p1.x);
	    int y1 = _cairo_fixed_integer_part (chunk->base[i].p1.y);
	    int x2 = _cairo_fixed_integer_part (chunk->base[i].p2.x);
	    int y2 = _cairo_fixed_integer_part (chunk->base[i].p2.y);
	    emit (ctx, x1, y1, x2, y2);
	}
    }
}

static cairo_int_status_t
fill_boxes (void		*_dst,
	    cairo_operator_t	 op,
	    const cairo_color_t	*color,
	    cairo_boxes_t	*boxes)
{
    cairo_gl_composite_t setup;
    cairo_gl_context_t *ctx;
    cairo_int_status_t status;

    status = _cairo_gl_composite_init (&setup, op, _dst, FALSE);
    if (unlikely (status))
	goto FAIL;

   _cairo_gl_composite_set_solid_source (&setup, color);

    status = _cairo_gl_composite_begin (&setup, &ctx);
    if (unlikely (status))
	goto FAIL;

    emit_aligned_boxes (ctx, boxes);
    status = _cairo_gl_context_release (ctx, CAIRO_STATUS_SUCCESS);

FAIL:
    _cairo_gl_composite_fini (&setup);
    return status;
}

static cairo_int_status_t
composite_boxes (void			*_dst,
		 cairo_operator_t	op,
		 cairo_surface_t	*abstract_src,
		 cairo_surface_t	*abstract_mask,
		 int			src_x,
		 int			src_y,
		 int			mask_x,
		 int			mask_y,
		 int			dst_x,
		 int			dst_y,
		 cairo_boxes_t		*boxes,
		 const cairo_rectangle_int_t  *extents)
{
    cairo_gl_composite_t setup;
    cairo_gl_context_t *ctx;
    cairo_int_status_t status;

    status = _cairo_gl_composite_init (&setup, op, _dst, FALSE);
    if (unlikely (status))
	goto FAIL;

    _cairo_gl_composite_set_source_operand (&setup,
					    source_to_operand (abstract_src));
    _cairo_gl_operand_translate (&setup.src, dst_x-src_x, dst_y-src_y);

    _cairo_gl_composite_set_mask_operand (&setup,
					  source_to_operand (abstract_mask));
    _cairo_gl_operand_translate (&setup.mask, dst_x-mask_x, dst_y-mask_y);

    status = _cairo_gl_composite_begin (&setup, &ctx);
    if (unlikely (status))
	goto FAIL;

    emit_aligned_boxes (ctx, boxes);
    status = _cairo_gl_context_release (ctx, CAIRO_STATUS_SUCCESS);

FAIL:
    _cairo_gl_composite_fini (&setup);
    return status;
}

static cairo_int_status_t
composite (void			*_dst,
	   cairo_operator_t	op,
	   cairo_surface_t	*abstract_src,
	   cairo_surface_t	*abstract_mask,
	   int			src_x,
	   int			src_y,
	   int			mask_x,
	   int			mask_y,
	   int			dst_x,
	   int			dst_y,
	   unsigned int		width,
	   unsigned int		height)
{
    cairo_gl_composite_t setup;
    cairo_gl_context_t *ctx;
    cairo_int_status_t status;

    status = _cairo_gl_composite_init (&setup, op, _dst, FALSE);
    if (unlikely (status))
	goto FAIL;

    _cairo_gl_composite_set_source_operand (&setup,
					    source_to_operand (abstract_src));
    _cairo_gl_operand_translate (&setup.src, dst_x-src_x, dst_y-src_y);

    _cairo_gl_composite_set_mask_operand (&setup,
					  source_to_operand (abstract_mask));
    _cairo_gl_operand_translate (&setup.mask, dst_x-mask_x, dst_y-mask_y);

    status = _cairo_gl_composite_begin (&setup, &ctx);
    if (unlikely (status))
	goto FAIL;

    /* XXX clip */
    _cairo_gl_context_emit_rect (ctx, dst_x, dst_y, dst_x+width, dst_y+height);
    status = _cairo_gl_context_release (ctx, CAIRO_STATUS_SUCCESS);

FAIL:
    _cairo_gl_composite_fini (&setup);
    return status;
}

static cairo_int_status_t
lerp (void			*dst,
      cairo_surface_t		*src,
      cairo_surface_t		*mask,
      int			src_x,
      int			src_y,
      int			mask_x,
      int			mask_y,
      int			dst_x,
      int			dst_y,
      unsigned int		width,
      unsigned int		height)
{
    cairo_int_status_t status;

    /* we could avoid some repetition... */
    status = composite (dst, CAIRO_OPERATOR_DEST_OUT, mask, NULL,
			mask_x, mask_y,
			0, 0,
			dst_x, dst_y,
			width, height);
    if (unlikely (status))
	return status;

    status = composite (dst, CAIRO_OPERATOR_ADD, src, mask,
			src_x, src_y,
			mask_x, mask_y,
			dst_x, dst_y,
			width, height);
    if (unlikely (status))
	return status;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
traps_to_operand (void *_dst,
		  const cairo_rectangle_int_t *extents,
		  cairo_antialias_t	antialias,
		  cairo_traps_t		*traps,
		  cairo_gl_operand_t	*operand,
		  int dst_x, int dst_y)
{
    pixman_format_code_t pixman_format;
    pixman_image_t *pixman_image;
    cairo_surface_t *image, *mask;
    cairo_surface_pattern_t pattern;
    cairo_status_t status;

    pixman_format = antialias != CAIRO_ANTIALIAS_NONE ? PIXMAN_a8 : PIXMAN_a1;
    pixman_image = pixman_image_create_bits (pixman_format,
					     extents->width,
					     extents->height,
					     NULL, 0);
    if (unlikely (pixman_image == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    _pixman_image_add_traps (pixman_image, extents->x, extents->y, traps);
    image = _cairo_image_surface_create_for_pixman_image (pixman_image,
							  pixman_format);
    if (unlikely (image->status)) {
	pixman_image_unref (pixman_image);
	return image->status;
    }

    mask = _cairo_surface_create_scratch (_dst,
					  CAIRO_CONTENT_COLOR_ALPHA,
					  extents->width,
					  extents->height,
					  NULL);
    if (unlikely (mask->status)) {
	cairo_surface_destroy (image);
	return mask->status;
    }

    status = _cairo_gl_surface_draw_image ((cairo_gl_surface_t *)mask,
					   (cairo_image_surface_t *)image,
					   0, 0,
					   extents->width, extents->height,
					   0, 0,
					   TRUE);
    cairo_surface_destroy (image);

    if (unlikely (status))
	goto error;

    _cairo_pattern_init_for_surface (&pattern, mask);
    cairo_matrix_init_translate (&pattern.base.matrix,
				 -extents->x+dst_x, -extents->y+dst_y);
    pattern.base.filter = CAIRO_FILTER_NEAREST;
    pattern.base.extend = CAIRO_EXTEND_NONE;
    status = _cairo_gl_operand_init (operand, &pattern.base, _dst,
				     &_cairo_unbounded_rectangle,
				     &_cairo_unbounded_rectangle,
				     FALSE);
    _cairo_pattern_fini (&pattern.base);

    if (unlikely (status))
	goto error;

    operand->texture.owns_surface = (cairo_gl_surface_t *)mask;
    return CAIRO_STATUS_SUCCESS;

error:
    cairo_surface_destroy (mask);
    return status;
}

static cairo_int_status_t
composite_traps (void			*_dst,
		 cairo_operator_t	op,
		 cairo_surface_t	*abstract_src,
		 int			src_x,
		 int			src_y,
		 int			dst_x,
		 int			dst_y,
		 const cairo_rectangle_int_t *extents,
		 cairo_antialias_t	antialias,
		 cairo_traps_t		*traps)
{
    cairo_gl_composite_t setup;
    cairo_gl_context_t *ctx;
    cairo_int_status_t status;

    status = _cairo_gl_composite_init (&setup, op, _dst, FALSE);
    if (unlikely (status))
	goto FAIL;

    _cairo_gl_composite_set_source_operand (&setup,
					    source_to_operand (abstract_src));
    _cairo_gl_operand_translate (&setup.src, -src_x-dst_x, -src_y-dst_y);
    status = traps_to_operand (_dst, extents, antialias, traps, &setup.mask, dst_x, dst_y);
    if (unlikely (status))
	goto FAIL;

    status = _cairo_gl_composite_begin (&setup, &ctx);
    if (unlikely (status))
	goto FAIL;

    /* XXX clip */
    _cairo_gl_context_emit_rect (ctx,
				 extents->x-dst_x, extents->y-dst_y,
				 extents->x-dst_x+extents->width,
				 extents->y-dst_y+extents->height);
    status = _cairo_gl_context_release (ctx, CAIRO_STATUS_SUCCESS);

FAIL:
    _cairo_gl_composite_fini (&setup);
    return status;
}

static cairo_gl_surface_t *
tristrip_to_surface (void *_dst,
		  const cairo_rectangle_int_t *extents,
		  cairo_antialias_t	antialias,
		  cairo_tristrip_t	*strip)
{
    pixman_format_code_t pixman_format;
    pixman_image_t *pixman_image;
    cairo_surface_t *image, *mask;
    cairo_status_t status;

    pixman_format = antialias != CAIRO_ANTIALIAS_NONE ? PIXMAN_a8 : PIXMAN_a1,
    pixman_image = pixman_image_create_bits (pixman_format,
					     extents->width,
					     extents->height,
					     NULL, 0);
    if (unlikely (pixman_image == NULL))
	return (cairo_gl_surface_t *)_cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _pixman_image_add_tristrip (pixman_image, extents->x, extents->y, strip);
    image = _cairo_image_surface_create_for_pixman_image (pixman_image,
							  pixman_format);
    if (unlikely (image->status)) {
	pixman_image_unref (pixman_image);
	return (cairo_gl_surface_t *)image;
    }

    mask = _cairo_surface_create_scratch (_dst,
					  CAIRO_CONTENT_COLOR_ALPHA,
					  extents->width,
					  extents->height,
					  NULL);
    if (unlikely (mask->status)) {
	cairo_surface_destroy (image);
	return (cairo_gl_surface_t *)mask;
    }

    status = _cairo_gl_surface_draw_image ((cairo_gl_surface_t *)mask,
					   (cairo_image_surface_t *)image,
					   0, 0,
					   extents->width, extents->height,
					   0, 0,
					   TRUE);
    cairo_surface_destroy (image);
    if (unlikely (status)) {
	cairo_surface_destroy (mask);
	return (cairo_gl_surface_t*)_cairo_surface_create_in_error (status);
    }

    return (cairo_gl_surface_t*)mask;
}

static cairo_int_status_t
composite_tristrip (void		*_dst,
		    cairo_operator_t	op,
		    cairo_surface_t	*abstract_src,
		    int			src_x,
		    int			src_y,
		    int			dst_x,
		    int			dst_y,
		    const cairo_rectangle_int_t *extents,
		    cairo_antialias_t	antialias,
		    cairo_tristrip_t	*strip)
{
    cairo_gl_composite_t setup;
    cairo_gl_context_t *ctx;
    cairo_gl_surface_t *mask;
    cairo_int_status_t status;

    mask = tristrip_to_surface (_dst, extents, antialias, strip);
    if (unlikely (mask->base.status))
	return mask->base.status;

    status = _cairo_gl_composite_init (&setup, op, _dst, FALSE);
    if (unlikely (status))
	goto FAIL;

    _cairo_gl_composite_set_source_operand (&setup,
					    source_to_operand (abstract_src));

    //_cairo_gl_composite_set_mask_surface (&setup, mask, 0, 0);

    status = _cairo_gl_composite_begin (&setup, &ctx);
    if (unlikely (status))
	goto FAIL;

    /* XXX clip */
    _cairo_gl_context_emit_rect (ctx,
				 dst_x, dst_y,
				 dst_x+extents->width,
				 dst_y+extents->height);
    status = _cairo_gl_context_release (ctx, CAIRO_STATUS_SUCCESS);

FAIL:
    _cairo_gl_composite_fini (&setup);
    cairo_surface_destroy (&mask->base);
    return status;
}

static cairo_int_status_t
check_composite (const cairo_composite_rectangles_t *extents)
{
    if (! _cairo_gl_operator_is_supported (extents->op))
	return UNSUPPORTED ("unsupported operator");

    return CAIRO_STATUS_SUCCESS;
}

const cairo_compositor_t *
_cairo_gl_traps_compositor_get (void)
{
    static cairo_atomic_once_t once = CAIRO_ATOMIC_ONCE_INIT;
    static cairo_traps_compositor_t compositor;

    if (_cairo_atomic_init_once_enter(&once)) {
	_cairo_traps_compositor_init (&compositor, &_cairo_fallback_compositor);
	compositor.acquire = acquire;
	compositor.release = release;
	compositor.set_clip_region = set_clip_region;
	compositor.pattern_to_surface = _cairo_gl_pattern_to_source;
	compositor.draw_image_boxes = draw_image_boxes;
	//compositor.copy_boxes = copy_boxes;
	compositor.fill_boxes = fill_boxes;
	compositor.check_composite = check_composite;
	compositor.composite = composite;
	compositor.lerp = lerp;
	//compositor.check_composite_boxes = check_composite_boxes;
	compositor.composite_boxes = composite_boxes;
	//compositor.check_composite_traps = check_composite_traps;
	compositor.composite_traps = composite_traps;
	//compositor.check_composite_tristrip = check_composite_traps;
	compositor.composite_tristrip = composite_tristrip;
	compositor.check_composite_glyphs = _cairo_gl_check_composite_glyphs;
	compositor.composite_glyphs = _cairo_gl_composite_glyphs;

	_cairo_atomic_init_once_leave(&once);
    }

    return &compositor.base;
}
