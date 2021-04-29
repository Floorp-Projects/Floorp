/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
 * Copyright © 2005 Red Hat, Inc.
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
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 *	Behdad Esfahbod <behdad@behdad.org>
 *	Chris Wilson <chris@chris-wilson.co.uk>
 *	Karl Tomlinson <karlt+@karlt.net>, Mozilla Corporation
 */

#include "cairoint.h"

#if !CAIRO_HAS_XLIB_XCB_FUNCTIONS

#include "cairo-xlib-private.h"

#include "cairo-compositor-private.h"
#include "cairo-damage-private.h"
#include "cairo-image-surface-private.h"
#include "cairo-list-inline.h"
#include "cairo-pattern-private.h"
#include "cairo-pixman-private.h"
#include "cairo-traps-private.h"
#include "cairo-tristrip-private.h"

static cairo_int_status_t
check_composite (const cairo_composite_rectangles_t *extents)
{
    cairo_xlib_display_t *display = ((cairo_xlib_surface_t *)extents->surface)->display;

    if (! CAIRO_RENDER_SUPPORTS_OPERATOR (display, extents->op))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
acquire (void *abstract_dst)
{
    cairo_xlib_surface_t *dst = abstract_dst;
    cairo_int_status_t status;

    status = _cairo_xlib_display_acquire (dst->base.device, &dst->display);
    if (unlikely (status))
        return status;

    dst->dpy = dst->display->display;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
release (void *abstract_dst)
{
    cairo_xlib_surface_t *dst = abstract_dst;

    cairo_device_release (&dst->display->base);
    dst->dpy = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
set_clip_region (void *_surface,
		 cairo_region_t *region)
{
    cairo_xlib_surface_t *surface = _surface;

    _cairo_xlib_surface_ensure_picture (surface);

    if (region != NULL) {
	XRectangle stack_rects[CAIRO_STACK_ARRAY_LENGTH (XRectangle)];
	XRectangle *rects = stack_rects;
	int n_rects, i;

	n_rects = cairo_region_num_rectangles (region);
	if (n_rects > ARRAY_LENGTH (stack_rects)) {
	    rects = _cairo_malloc_ab (n_rects, sizeof (XRectangle));
	    if (unlikely (rects == NULL))
		return _cairo_error (CAIRO_STATUS_NO_MEMORY);
	}
	for (i = 0; i < n_rects; i++) {
	    cairo_rectangle_int_t rect;

	    cairo_region_get_rectangle (region, i, &rect);

	    rects[i].x = rect.x;
	    rects[i].y = rect.y;
	    rects[i].width  = rect.width;
	    rects[i].height = rect.height;
	}
	XRenderSetPictureClipRectangles (surface->dpy,
					 surface->picture,
					 0, 0,
					 rects, n_rects);
	if (rects != stack_rects)
	    free (rects);
    } else {
	XRenderPictureAttributes pa;
	pa.clip_mask = None;
	XRenderChangePicture (surface->dpy,
			      surface->picture,
			      CPClipMask, &pa);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
copy_image_boxes (void *_dst,
		  cairo_image_surface_t *image,
		  cairo_boxes_t *boxes,
		  int dx, int dy)
{
    cairo_xlib_surface_t *dst = _dst;
    struct _cairo_boxes_chunk *chunk;
    cairo_int_status_t status;
    Pixmap src;
    GC gc;
    int i, j;

    assert (image->depth == dst->depth);

    status = acquire (dst);
    if (unlikely (status))
	return status;

    status = _cairo_xlib_surface_get_gc (dst->display, dst, &gc);
    if (unlikely (status)) {
	release (dst);
	return status;
    }

    src = _cairo_xlib_shm_surface_get_pixmap (&image->base);
    if (boxes->num_boxes == 1) {
	int x1 = _cairo_fixed_integer_part (boxes->chunks.base[0].p1.x);
	int y1 = _cairo_fixed_integer_part (boxes->chunks.base[0].p1.y);
	int x2 = _cairo_fixed_integer_part (boxes->chunks.base[0].p2.x);
	int y2 = _cairo_fixed_integer_part (boxes->chunks.base[0].p2.y);

	_cairo_xlib_shm_surface_mark_active (&image->base);
	XCopyArea (dst->dpy, src, dst->drawable, gc,
		   x1 + dx, y1 + dy,
		   x2 - x1, y2 - y1,
		   x1,      y1);
    } else {
	XRectangle stack_rects[CAIRO_STACK_ARRAY_LENGTH (XRectangle)];
	XRectangle *rects = stack_rects;

	if (boxes->num_boxes > ARRAY_LENGTH (stack_rects)) {
	    rects = _cairo_malloc_ab (boxes->num_boxes, sizeof (XRectangle));
	    if (unlikely (rects == NULL))
		return _cairo_error (CAIRO_STATUS_NO_MEMORY);
	}

	j = 0;
	for (chunk = &boxes->chunks; chunk; chunk = chunk->next) {
	    for (i = 0; i < chunk->count; i++) {
		int x1 = _cairo_fixed_integer_part (chunk->base[i].p1.x);
		int y1 = _cairo_fixed_integer_part (chunk->base[i].p1.y);
		int x2 = _cairo_fixed_integer_part (chunk->base[i].p2.x);
		int y2 = _cairo_fixed_integer_part (chunk->base[i].p2.y);

		if (x2 > x1 && y2 > y1) {
		    rects[j].x = x1;
		    rects[j].y = y1;
		    rects[j].width  = x2 - x1;
		    rects[j].height = y2 - y1;
		    j++;
		}
	    }
	}

	XSetClipRectangles (dst->dpy, gc, 0, 0, rects, j, Unsorted);
	_cairo_xlib_shm_surface_mark_active (&image->base);
	XCopyArea (dst->dpy, src, dst->drawable, gc,
		   0, 0, image->width, image->height, -dx, -dy);
	XSetClipMask (dst->dpy, gc, None);

	if (rects != stack_rects)
	    free (rects);
    }

    _cairo_xlib_surface_put_gc (dst->display, dst, gc);
    release (dst);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t
boxes_cover_surface (cairo_boxes_t *boxes,
		     cairo_xlib_surface_t *surface)
{
    cairo_box_t *b;

    if (boxes->num_boxes != 1)
	    return FALSE;

    b = &boxes->chunks.base[0];

    if (_cairo_fixed_integer_part (b->p1.x) > 0 ||
	_cairo_fixed_integer_part (b->p1.y) > 0)
	return FALSE;

    if (_cairo_fixed_integer_part (b->p2.x) < surface->width ||
	_cairo_fixed_integer_part (b->p2.y) < surface->height)
	return FALSE;

    return TRUE;
}

static cairo_int_status_t
draw_image_boxes (void *_dst,
		  cairo_image_surface_t *image,
		  cairo_boxes_t *boxes,
		  int dx, int dy)
{
    cairo_xlib_surface_t *dst = _dst;
    struct _cairo_boxes_chunk *chunk;
    cairo_image_surface_t *shm = NULL;
    cairo_int_status_t status;
    int i;

    if (image->base.device == dst->base.device) {
	if (image->depth != dst->depth)
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	if (_cairo_xlib_shm_surface_get_pixmap (&image->base))
	    return copy_image_boxes (dst, image, boxes, dx, dy);

	goto draw_image_boxes;
    }

    if (boxes_cover_surface (boxes, dst))
	shm = (cairo_image_surface_t *) _cairo_xlib_surface_get_shm (dst, TRUE);
    if (shm) {
	for (chunk = &boxes->chunks; chunk; chunk = chunk->next) {
	    for (i = 0; i < chunk->count; i++) {
		cairo_box_t *b = &chunk->base[i];
		cairo_rectangle_int_t r;

		r.x = _cairo_fixed_integer_part (b->p1.x);
		r.y = _cairo_fixed_integer_part (b->p1.y);
		r.width = _cairo_fixed_integer_part (b->p2.x) - r.x;
		r.height = _cairo_fixed_integer_part (b->p2.y) - r.y;

		if (shm->pixman_format != image->pixman_format ||
		    ! pixman_blt ((uint32_t *)image->data, (uint32_t *)shm->data,
				  image->stride / sizeof (uint32_t),
				  shm->stride / sizeof (uint32_t),
				  PIXMAN_FORMAT_BPP (image->pixman_format),
				  PIXMAN_FORMAT_BPP (shm->pixman_format),
				  r.x + dx, r.y + dy,
				  r.x, r.y,
				  r.width, r.height))
		{
		    pixman_image_composite32 (PIXMAN_OP_SRC,
					      image->pixman_image, NULL, shm->pixman_image,
					      r.x + dx, r.y + dy,
					      0, 0,
					      r.x, r.y,
					      r.width, r.height);
		}

		shm->base.damage =
		    _cairo_damage_add_rectangle (shm->base.damage, &r);
	    }
	}
	dst->base.is_clear = FALSE;
	dst->fallback++;
	dst->base.serial++;
	return CAIRO_INT_STATUS_NOTHING_TO_DO;
    }

    if (image->depth == dst->depth &&
	((cairo_xlib_display_t *)dst->display)->shm) {
	cairo_box_t extents;
	cairo_rectangle_int_t r;

	_cairo_boxes_extents (boxes, &extents);
	_cairo_box_round_to_rectangle (&extents, &r);

	shm = (cairo_image_surface_t *)
	    _cairo_xlib_surface_create_shm (dst, image->pixman_format,
					    r.width, r.height);
	if (shm) {
	    int tx = -r.x, ty = -r.y;

	    assert (shm->pixman_format == image->pixman_format);
	    for (chunk = &boxes->chunks; chunk; chunk = chunk->next) {
		for (i = 0; i < chunk->count; i++) {
		    cairo_box_t *b = &chunk->base[i];

		    r.x = _cairo_fixed_integer_part (b->p1.x);
		    r.y = _cairo_fixed_integer_part (b->p1.y);
		    r.width  = _cairo_fixed_integer_part (b->p2.x) - r.x;
		    r.height = _cairo_fixed_integer_part (b->p2.y) - r.y;

		    if (! pixman_blt ((uint32_t *)image->data, (uint32_t *)shm->data,
				      image->stride / sizeof (uint32_t),
				      shm->stride / sizeof (uint32_t),
				      PIXMAN_FORMAT_BPP (image->pixman_format),
				      PIXMAN_FORMAT_BPP (shm->pixman_format),
				      r.x + dx, r.y + dy,
				      r.x + tx, r.y + ty,
				      r.width, r.height))
		    {
			pixman_image_composite32 (PIXMAN_OP_SRC,
						  image->pixman_image, NULL, shm->pixman_image,
						  r.x + dx, r.y + dy,
						  0, 0,
						  r.x + tx, r.y + ty,
						  r.width, r.height);
		    }
		}
	    }

	    dx = tx;
	    dy = ty;
	    image = shm;

	    if (_cairo_xlib_shm_surface_get_pixmap (&image->base)) {
		status = copy_image_boxes (dst, image, boxes, dx, dy);
		if (status != CAIRO_INT_STATUS_UNSUPPORTED)
		    goto out;
	    }
	}
    }

draw_image_boxes:
    status = CAIRO_STATUS_SUCCESS;
    for (chunk = &boxes->chunks; chunk; chunk = chunk->next) {
	for (i = 0; i < chunk->count; i++) {
	    cairo_box_t *b = &chunk->base[i];
	    int x1 = _cairo_fixed_integer_part (b->p1.x);
	    int y1 = _cairo_fixed_integer_part (b->p1.y);
	    int x2 = _cairo_fixed_integer_part (b->p2.x);
	    int y2 = _cairo_fixed_integer_part (b->p2.y);
	    if (_cairo_xlib_surface_draw_image (dst, image,
						x1 + dx, y1 + dy,
						x2 - x1, y2 - y1,
						x1, y1)) {
		status = CAIRO_INT_STATUS_UNSUPPORTED;
		goto out;
	    }
	}
    }

out:
    cairo_surface_destroy (&shm->base);
    return status;
}

static cairo_int_status_t
copy_boxes (void *_dst,
	    cairo_surface_t *_src,
	    cairo_boxes_t *boxes,
	    const cairo_rectangle_int_t *extents,
	    int dx, int dy)
{
    cairo_xlib_surface_t *dst = _dst;
    cairo_xlib_surface_t *src = (cairo_xlib_surface_t *)_src;
    struct _cairo_boxes_chunk *chunk;
    cairo_int_status_t status;
    GC gc;
    Drawable d;
    int i, j;

    if (! _cairo_xlib_surface_same_screen  (dst, src))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (dst->depth != src->depth)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = acquire (dst);
    if (unlikely (status))
	return status;

    status = _cairo_xlib_surface_get_gc (dst->display, dst, &gc);
    if (unlikely (status)) {
	release (dst);
	return status;
    }

    if (src->fallback && src->shm->damage->dirty) {
	assert (src != dst);
	d = _cairo_xlib_shm_surface_get_pixmap (src->shm);
	assert (d != 0);
    } else {
	if (! src->owns_pixmap) {
	    XGCValues gcv;

	    gcv.subwindow_mode = IncludeInferiors;
	    XChangeGC (dst->display->display, gc, GCSubwindowMode, &gcv);
	}
	d = src->drawable;
    }

    if (boxes->num_boxes == 1) {
	int x1 = _cairo_fixed_integer_part (boxes->chunks.base[0].p1.x);
	int y1 = _cairo_fixed_integer_part (boxes->chunks.base[0].p1.y);
	int x2 = _cairo_fixed_integer_part (boxes->chunks.base[0].p2.x);
	int y2 = _cairo_fixed_integer_part (boxes->chunks.base[0].p2.y);

	XCopyArea (dst->dpy, d, dst->drawable, gc,
		   x1 + dx, y1 + dy,
		   x2 - x1, y2 - y1,
		   x1,      y1);
    } else {
	/* We can only have a single control for subwindow_mode on the
	 * GC. If we have a Window destination, we need to set ClipByChildren,
	 * but if we have a Window source, we need IncludeInferiors. If we have
	 * both a Window destination and source, we must fallback. There is
	 * no convenient way to detect if a drawable is a Pixmap or Window,
	 * therefore we can only rely on those surfaces that we created
	 * ourselves to be Pixmaps, and treat everything else as a potential
	 * Window.
	 */
	if (src == dst || (!src->owns_pixmap && !dst->owns_pixmap)) {
	    for (chunk = &boxes->chunks; chunk; chunk = chunk->next) {
		for (i = 0; i < chunk->count; i++) {
		    int x1 = _cairo_fixed_integer_part (chunk->base[i].p1.x);
		    int y1 = _cairo_fixed_integer_part (chunk->base[i].p1.y);
		    int x2 = _cairo_fixed_integer_part (chunk->base[i].p2.x);
		    int y2 = _cairo_fixed_integer_part (chunk->base[i].p2.y);
		    XCopyArea (dst->dpy, d, dst->drawable, gc,
			       x1 + dx, y1 + dy,
			       x2 - x1, y2 - y1,
			       x1,      y1);
		}
	    }
	} else {
	    XRectangle stack_rects[CAIRO_STACK_ARRAY_LENGTH (XRectangle)];
	    XRectangle *rects = stack_rects;

	    if (boxes->num_boxes > ARRAY_LENGTH (stack_rects)) {
		rects = _cairo_malloc_ab (boxes->num_boxes, sizeof (XRectangle));
		if (unlikely (rects == NULL))
		    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    }

	    j = 0;
	    for (chunk = &boxes->chunks; chunk; chunk = chunk->next) {
		for (i = 0; i < chunk->count; i++) {
		    int x1 = _cairo_fixed_integer_part (chunk->base[i].p1.x);
		    int y1 = _cairo_fixed_integer_part (chunk->base[i].p1.y);
		    int x2 = _cairo_fixed_integer_part (chunk->base[i].p2.x);
		    int y2 = _cairo_fixed_integer_part (chunk->base[i].p2.y);

		    rects[j].x = x1;
		    rects[j].y = y1;
		    rects[j].width  = x2 - x1;
		    rects[j].height = y2 - y1;
		    j++;
		}
	    }
	    assert (j == boxes->num_boxes);

	    XSetClipRectangles (dst->dpy, gc, 0, 0, rects, j, Unsorted);

	    XCopyArea (dst->dpy, d, dst->drawable, gc,
		       extents->x + dx, extents->y + dy,
		       extents->width,  extents->height,
		       extents->x,      extents->y);

	    XSetClipMask (dst->dpy, gc, None);

	    if (rects != stack_rects)
		free (rects);
	}
    }

    if (src->fallback && src->shm->damage->dirty) {
	_cairo_xlib_shm_surface_mark_active (src->shm);
    } else if (! src->owns_pixmap) {
	XGCValues gcv;

	gcv.subwindow_mode = ClipByChildren;
	XChangeGC (dst->display->display, gc, GCSubwindowMode, &gcv);
    }

    _cairo_xlib_surface_put_gc (dst->display, dst, gc);
    release (dst);
    return CAIRO_STATUS_SUCCESS;
}

static int
_render_operator (cairo_operator_t op)
{
    switch (op) {
    case CAIRO_OPERATOR_CLEAR:
	return PictOpClear;

    case CAIRO_OPERATOR_SOURCE:
	return PictOpSrc;
    case CAIRO_OPERATOR_OVER:
	return PictOpOver;
    case CAIRO_OPERATOR_IN:
	return PictOpIn;
    case CAIRO_OPERATOR_OUT:
	return PictOpOut;
    case CAIRO_OPERATOR_ATOP:
	return PictOpAtop;

    case CAIRO_OPERATOR_DEST:
	return PictOpDst;
    case CAIRO_OPERATOR_DEST_OVER:
	return PictOpOverReverse;
    case CAIRO_OPERATOR_DEST_IN:
	return PictOpInReverse;
    case CAIRO_OPERATOR_DEST_OUT:
	return PictOpOutReverse;
    case CAIRO_OPERATOR_DEST_ATOP:
	return PictOpAtopReverse;

    case CAIRO_OPERATOR_XOR:
	return PictOpXor;
    case CAIRO_OPERATOR_ADD:
	return PictOpAdd;
    case CAIRO_OPERATOR_SATURATE:
	return PictOpSaturate;

    case CAIRO_OPERATOR_MULTIPLY:
	return PictOpMultiply;
    case CAIRO_OPERATOR_SCREEN:
	return PictOpScreen;
    case CAIRO_OPERATOR_OVERLAY:
	return PictOpOverlay;
    case CAIRO_OPERATOR_DARKEN:
	return PictOpDarken;
    case CAIRO_OPERATOR_LIGHTEN:
	return PictOpLighten;
    case CAIRO_OPERATOR_COLOR_DODGE:
	return PictOpColorDodge;
    case CAIRO_OPERATOR_COLOR_BURN:
	return PictOpColorBurn;
    case CAIRO_OPERATOR_HARD_LIGHT:
	return PictOpHardLight;
    case CAIRO_OPERATOR_SOFT_LIGHT:
	return PictOpSoftLight;
    case CAIRO_OPERATOR_DIFFERENCE:
	return PictOpDifference;
    case CAIRO_OPERATOR_EXCLUSION:
	return PictOpExclusion;
    case CAIRO_OPERATOR_HSL_HUE:
	return PictOpHSLHue;
    case CAIRO_OPERATOR_HSL_SATURATION:
	return PictOpHSLSaturation;
    case CAIRO_OPERATOR_HSL_COLOR:
	return PictOpHSLColor;
    case CAIRO_OPERATOR_HSL_LUMINOSITY:
	return PictOpHSLLuminosity;

    default:
	ASSERT_NOT_REACHED;
	return PictOpOver;
    }
}

static cairo_bool_t
fill_reduces_to_source (cairo_operator_t op,
			const cairo_color_t *color,
			cairo_xlib_surface_t *dst)
{
    if (dst->base.is_clear || CAIRO_COLOR_IS_OPAQUE (color)) {
	if (op == CAIRO_OPERATOR_OVER)
	    return TRUE;
	if (op == CAIRO_OPERATOR_ADD)
	    return (dst->base.content & CAIRO_CONTENT_COLOR) == 0;
    }

    return FALSE;
}

static cairo_int_status_t
fill_rectangles (void				*abstract_surface,
		 cairo_operator_t		 op,
		 const cairo_color_t		*color,
		 cairo_rectangle_int_t		*rects,
		 int				 num_rects)
{
    cairo_xlib_surface_t *dst = abstract_surface;
    XRenderColor render_color;
    int i;

    //X_DEBUG ((display->display, "fill_rectangles (dst=%x)", (unsigned int) surface->drawable));

    if (fill_reduces_to_source (op, color, dst))
	op = CAIRO_OPERATOR_SOURCE;

    if (!CAIRO_RENDER_HAS_FILL_RECTANGLES(dst->display)) {
	cairo_int_status_t status;

	status = CAIRO_INT_STATUS_UNSUPPORTED;
	if (op == CAIRO_OPERATOR_SOURCE)
	    status = _cairo_xlib_core_fill_rectangles (dst, color, num_rects, rects);
	return status;
    }

    render_color.red   = color->red_short;
    render_color.green = color->green_short;
    render_color.blue  = color->blue_short;
    render_color.alpha = color->alpha_short;

    _cairo_xlib_surface_ensure_picture (dst);
    if (num_rects == 1) {
	/* Take advantage of the protocol compaction that libXrender performs
	 * to amalgamate sequences of XRenderFillRectangle().
	 */
	XRenderFillRectangle (dst->dpy,
			      _render_operator (op),
			      dst->picture,
			      &render_color,
			      rects->x, rects->y,
			      rects->width, rects->height);
    } else {
	XRectangle stack_xrects[CAIRO_STACK_ARRAY_LENGTH (XRectangle)];
	XRectangle *xrects = stack_xrects;

	if (num_rects > ARRAY_LENGTH (stack_xrects)) {
	    xrects = _cairo_malloc_ab (num_rects, sizeof (XRectangle));
	    if (unlikely (xrects == NULL))
		return _cairo_error (CAIRO_STATUS_NO_MEMORY);
	}

	for (i = 0; i < num_rects; i++) {
	    xrects[i].x = rects[i].x;
	    xrects[i].y = rects[i].y;
	    xrects[i].width  = rects[i].width;
	    xrects[i].height = rects[i].height;
	}

	XRenderFillRectangles (dst->dpy,
			       _render_operator (op),
			       dst->picture,
			       &render_color, xrects, num_rects);

	if (xrects != stack_xrects)
	    free (xrects);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
fill_boxes (void		*abstract_surface,
	    cairo_operator_t	 op,
	    const cairo_color_t	*color,
	    cairo_boxes_t	*boxes)
{
    cairo_xlib_surface_t *dst = abstract_surface;
    XRenderColor render_color;

    if (fill_reduces_to_source (op, color, dst))
	op = CAIRO_OPERATOR_SOURCE;

    if (!CAIRO_RENDER_HAS_FILL_RECTANGLES(dst->display)) {
	cairo_int_status_t status;

	status = CAIRO_INT_STATUS_UNSUPPORTED;
	if (op == CAIRO_OPERATOR_SOURCE)
	    status = _cairo_xlib_core_fill_boxes (dst, color, boxes);
	return status;
    }

    render_color.red   = color->red_short;
    render_color.green = color->green_short;
    render_color.blue  = color->blue_short;
    render_color.alpha = color->alpha_short;

    _cairo_xlib_surface_ensure_picture (dst);
    if (boxes->num_boxes == 1) {
	int x1 = _cairo_fixed_integer_part (boxes->chunks.base[0].p1.x);
	int y1 = _cairo_fixed_integer_part (boxes->chunks.base[0].p1.y);
	int x2 = _cairo_fixed_integer_part (boxes->chunks.base[0].p2.x);
	int y2 = _cairo_fixed_integer_part (boxes->chunks.base[0].p2.y);

	/* Take advantage of the protocol compaction that libXrender performs
	 * to amalgamate sequences of XRenderFillRectangle().
	 */
	XRenderFillRectangle (dst->dpy,
			      _render_operator (op),
			      dst->picture,
			      &render_color,
			      x1, y1,
			      x2 - x1, y2 - y1);
    } else {
	XRectangle stack_xrects[CAIRO_STACK_ARRAY_LENGTH (XRectangle)];
	XRectangle *xrects = stack_xrects;
	struct _cairo_boxes_chunk *chunk;
	int i, j;

	if (boxes->num_boxes > ARRAY_LENGTH (stack_xrects)) {
	    xrects = _cairo_malloc_ab (boxes->num_boxes, sizeof (XRectangle));
	    if (unlikely (xrects == NULL))
		return _cairo_error (CAIRO_STATUS_NO_MEMORY);
	}

	j = 0;
	for (chunk = &boxes->chunks; chunk; chunk = chunk->next) {
	    for (i = 0; i < chunk->count; i++) {
		int x1 = _cairo_fixed_integer_part (chunk->base[i].p1.x);
		int y1 = _cairo_fixed_integer_part (chunk->base[i].p1.y);
		int x2 = _cairo_fixed_integer_part (chunk->base[i].p2.x);
		int y2 = _cairo_fixed_integer_part (chunk->base[i].p2.y);

		xrects[j].x = x1;
		xrects[j].y = y1;
		xrects[j].width  = x2 - x1;
		xrects[j].height = y2 - y1;
		j++;
	    }
	}

	XRenderFillRectangles (dst->dpy,
			       _render_operator (op),
			       dst->picture,
			       &render_color, xrects, j);

	if (xrects != stack_xrects)
	    free (xrects);
    }

    return CAIRO_STATUS_SUCCESS;
}

#if 0
check_composite ()
    operation = _categorize_composite_operation (dst, op, src_pattern,
						 mask_pattern != NULL);
    if (operation == DO_UNSUPPORTED)
	return UNSUPPORTED ("unsupported operation");

    //X_DEBUG ((display->display, "composite (dst=%x)", (unsigned int) dst->drawable));

    operation = _recategorize_composite_operation (dst, op, src, &src_attr,
						   mask_pattern != NULL);
    if (operation == DO_UNSUPPORTED) {
	status = UNSUPPORTED ("unsupported operation");
	goto BAIL;
    }
#endif

static cairo_int_status_t
composite (void *abstract_dst,
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
    cairo_xlib_surface_t *dst = abstract_dst;
    cairo_xlib_source_t *src = (cairo_xlib_source_t *)abstract_src;

    op = _render_operator (op);

    _cairo_xlib_surface_ensure_picture (dst);
    if (abstract_mask) {
	cairo_xlib_source_t *mask = (cairo_xlib_source_t *)abstract_mask;

	XRenderComposite (dst->dpy, op,
			  src->picture, mask->picture, dst->picture,
			  src_x,  src_y,
			  mask_x, mask_y,
			  dst_x,  dst_y,
			  width,  height);
    } else {
	XRenderComposite (dst->dpy, op,
			  src->picture, 0, dst->picture,
			  src_x, src_y,
			  0, 0,
			  dst_x, dst_y,
			  width, height);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
lerp (void *abstract_dst,
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
    cairo_xlib_surface_t *dst = abstract_dst;
    cairo_xlib_source_t *src = (cairo_xlib_source_t *)abstract_src;
    cairo_xlib_source_t *mask = (cairo_xlib_source_t *)abstract_mask;

    _cairo_xlib_surface_ensure_picture (dst);
    XRenderComposite (dst->dpy, PictOpOutReverse,
		      mask->picture, None, dst->picture,
		      mask_x, mask_y,
		      0,      0,
		      dst_x,  dst_y,
		      width,  height);
    XRenderComposite (dst->dpy, PictOpAdd,
		      src->picture, mask->picture, dst->picture,
		      src_x,  src_y,
		      mask_x, mask_y,
		      dst_x,  dst_y,
		      width,  height);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
composite_boxes (void			*abstract_dst,
		 cairo_operator_t	 op,
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
    cairo_xlib_surface_t *dst = abstract_dst;
    Picture src = ((cairo_xlib_source_t *)abstract_src)->picture;
    Picture mask = abstract_mask ? ((cairo_xlib_source_t *)abstract_mask)->picture : 0;
    XRectangle stack_rects[CAIRO_STACK_ARRAY_LENGTH (XRectangle)];
    XRectangle *rects = stack_rects;
    struct _cairo_boxes_chunk *chunk;
    int i, j;

    op = _render_operator (op);
    _cairo_xlib_surface_ensure_picture (dst);
    if (boxes->num_boxes == 1) {
	int x1 = _cairo_fixed_integer_part (boxes->chunks.base[0].p1.x);
	int y1 = _cairo_fixed_integer_part (boxes->chunks.base[0].p1.y);
	int x2 = _cairo_fixed_integer_part (boxes->chunks.base[0].p2.x);
	int y2 = _cairo_fixed_integer_part (boxes->chunks.base[0].p2.y);

	XRenderComposite (dst->dpy, op,
			  src, mask, dst->picture,
			  x1 + src_x,	y1 + src_y,
			  x1 + mask_x,	y1 + mask_y,
			  x1 - dst_x,	y1 - dst_y,
			  x2 - x1,	y2 - y1);
	return CAIRO_STATUS_SUCCESS;
    }

    if (boxes->num_boxes > ARRAY_LENGTH (stack_rects)) {
	rects = _cairo_malloc_ab (boxes->num_boxes, sizeof (XRectangle));
	if (unlikely (rects == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    j = 0;
    for (chunk = &boxes->chunks; chunk; chunk = chunk->next) {
	for (i = 0; i < chunk->count; i++) {
	    int x1 = _cairo_fixed_integer_part (chunk->base[i].p1.x);
	    int y1 = _cairo_fixed_integer_part (chunk->base[i].p1.y);
	    int x2 = _cairo_fixed_integer_part (chunk->base[i].p2.x);
	    int y2 = _cairo_fixed_integer_part (chunk->base[i].p2.y);

	    rects[j].x = x1 - dst_x;
	    rects[j].y = y1 - dst_y;
	    rects[j].width  = x2 - x1;
	    rects[j].height = y2 - y1;
	    j++;
	}
    }
    assert (j == boxes->num_boxes);

    XRenderSetPictureClipRectangles (dst->dpy,
				     dst->picture,
				     0, 0,
				     rects, j);
    if (rects != stack_rects)
	free (rects);

    XRenderComposite (dst->dpy, op,
		      src, mask, dst->picture,
		      extents->x + src_x,  extents->y + src_y,
		      extents->x + mask_x, extents->y + mask_y,
		      extents->x - dst_x,  extents->y - dst_y,
		      extents->width,      extents->height);

    set_clip_region (dst, NULL);

    return CAIRO_STATUS_SUCCESS;
}

/* font rendering */

void
_cairo_xlib_font_close (cairo_xlib_font_t *priv)
{
    cairo_xlib_display_t *display = (cairo_xlib_display_t *)priv->base.key;
    int i;

    /* XXX All I really want is to do is zap my glyphs... */
    _cairo_scaled_font_reset_cache (priv->font);

    for (i = 0; i < NUM_GLYPHSETS; i++) {
	cairo_xlib_font_glyphset_t *info;

	info = &priv->glyphset[i];
	if (info->glyphset)
	    XRenderFreeGlyphSet (display->display, info->glyphset);
    }

    /* XXX locking */
    cairo_list_del (&priv->link);
    cairo_list_del (&priv->base.link);
    free (priv);
}

static void
_cairo_xlib_font_fini (cairo_scaled_font_private_t *abstract_private,
		       cairo_scaled_font_t *font)
{
    cairo_xlib_font_t *priv = (cairo_xlib_font_t *) abstract_private;
    cairo_status_t status;
    cairo_xlib_display_t *display;
    int i;

    cairo_list_del (&priv->base.link);
    cairo_list_del (&priv->link);

    status = _cairo_xlib_display_acquire (priv->device, &display);
    if (unlikely (status)) /* this should be impossible but leak just in case */
	goto BAIL;

    for (i = 0; i < NUM_GLYPHSETS; i++) {
	cairo_xlib_font_glyphset_t *info;

	info = &priv->glyphset[i];
	if (info->glyphset)
	    XRenderFreeGlyphSet (display->display, info->glyphset);
    }

    cairo_device_release (&display->base);
BAIL:
    cairo_device_destroy (priv->device);
    free (priv);
}

static cairo_xlib_font_t *
_cairo_xlib_font_create (cairo_xlib_display_t *display,
			 cairo_scaled_font_t  *font)
{
    cairo_xlib_font_t *priv;
    int i;

    priv = _cairo_malloc (sizeof (cairo_xlib_font_t));
    if (unlikely (priv == NULL))
	return NULL;

    _cairo_scaled_font_attach_private (font, &priv->base, display,
				       _cairo_xlib_font_fini);

    priv->device = cairo_device_reference (&display->base);
    priv->font = font;
    cairo_list_add (&priv->link, &display->fonts);

    for (i = 0; i < NUM_GLYPHSETS; i++) {
	cairo_xlib_font_glyphset_t *info = &priv->glyphset[i];
	switch (i) {
	case GLYPHSET_INDEX_ARGB32: info->format = CAIRO_FORMAT_ARGB32; break;
	case GLYPHSET_INDEX_A8:     info->format = CAIRO_FORMAT_A8;     break;
	case GLYPHSET_INDEX_A1:     info->format = CAIRO_FORMAT_A1;     break;
	default:                    ASSERT_NOT_REACHED;                          break;
	}
	info->xrender_format = NULL;
	info->glyphset = None;
	info->to_free.count = 0;
    }

    return priv;
}

static int
_cairo_xlib_get_glyphset_index_for_format (cairo_format_t format)
{
    if (format == CAIRO_FORMAT_A8)
        return GLYPHSET_INDEX_A8;
    if (format == CAIRO_FORMAT_A1)
        return GLYPHSET_INDEX_A1;

    assert (format == CAIRO_FORMAT_ARGB32);
    return GLYPHSET_INDEX_ARGB32;
}

static inline cairo_xlib_font_t *
_cairo_xlib_font_get (const cairo_xlib_display_t *display,
		      cairo_scaled_font_t *font)
{
    return (cairo_xlib_font_t *)_cairo_scaled_font_find_private (font, display);
}

typedef struct {
    cairo_scaled_glyph_private_t base;


    cairo_xlib_font_glyphset_t *glyphset;
} cairo_xlib_glyph_private_t;

static void
_cairo_xlib_glyph_fini (cairo_scaled_glyph_private_t *glyph_private,
			cairo_scaled_glyph_t *glyph,
			cairo_scaled_font_t  *font)
{
    cairo_xlib_glyph_private_t *priv = (cairo_xlib_glyph_private_t *)glyph_private;

    if (! font->finished) {
	cairo_xlib_font_t *font_private;
	struct _cairo_xlib_font_glyphset_free_glyphs *to_free;
	cairo_xlib_font_glyphset_t *info;

	font_private = _cairo_xlib_font_get (glyph_private->key, font);
	assert (font_private);

	info = priv->glyphset;
	to_free = &info->to_free;
	if (to_free->count == ARRAY_LENGTH (to_free->indices)) {
	    cairo_xlib_display_t *display;

	    if (_cairo_xlib_display_acquire (font_private->device,
					     &display) == CAIRO_STATUS_SUCCESS) {
		XRenderFreeGlyphs (display->display,
				   info->glyphset,
				   to_free->indices,
				   to_free->count);
		cairo_device_release (&display->base);
	    }

	    to_free->count = 0;
	}

	to_free->indices[to_free->count++] = glyph->hash_entry.hash;
    }

    cairo_list_del (&glyph_private->link);
    free (glyph_private);
}

static cairo_status_t
_cairo_xlib_glyph_attach (cairo_xlib_display_t	*display,
			  cairo_scaled_glyph_t	*glyph,
			  cairo_xlib_font_glyphset_t *info)
{
    cairo_xlib_glyph_private_t *priv;

    priv = _cairo_malloc (sizeof (*priv));
    if (unlikely (priv == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    _cairo_scaled_glyph_attach_private (glyph, &priv->base, display,
					_cairo_xlib_glyph_fini);
    priv->glyphset = info;

    glyph->dev_private = info;
    glyph->dev_private_key = display;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_xlib_font_glyphset_t *
_cairo_xlib_font_get_glyphset_info_for_format (cairo_xlib_display_t *display,
					       cairo_scaled_font_t *font,
					       cairo_format_t       format)
{
    cairo_xlib_font_t *priv;
    cairo_xlib_font_glyphset_t *info;
    int glyphset_index;

    glyphset_index = _cairo_xlib_get_glyphset_index_for_format (format);

    priv = _cairo_xlib_font_get (display, font);
    if (priv == NULL) {
	priv = _cairo_xlib_font_create (display, font);
	if (priv == NULL)
	    return NULL;
    }

    info = &priv->glyphset[glyphset_index];
    if (info->glyphset == None) {
	info->xrender_format =
	    _cairo_xlib_display_get_xrender_format (display, info->format);
	info->glyphset = XRenderCreateGlyphSet (display->display,
						info->xrender_format);
    }

    return info;
}

static cairo_bool_t
has_pending_free_glyph (cairo_xlib_font_glyphset_t *info,
			unsigned long glyph_index)
{
    struct _cairo_xlib_font_glyphset_free_glyphs *to_free;
    int i;

    to_free = &info->to_free;
    for (i = 0; i < to_free->count; i++) {
	if (to_free->indices[i] == glyph_index) {
	    to_free->count--;
	    memmove (&to_free->indices[i],
		     &to_free->indices[i+1],
		     (to_free->count - i) * sizeof (to_free->indices[0]));
	    return TRUE;
	}
    }

    return FALSE;
}

static cairo_xlib_font_glyphset_t *
find_pending_free_glyph (cairo_xlib_display_t *display,
			 cairo_scaled_font_t *font,
			 unsigned long glyph_index,
			 cairo_image_surface_t *surface)
{
    cairo_xlib_font_t *priv;
    int i;

    priv = _cairo_xlib_font_get (display, font);
    if (priv == NULL)
	return NULL;

    if (surface != NULL) {
	i = _cairo_xlib_get_glyphset_index_for_format (surface->format);
	if (has_pending_free_glyph (&priv->glyphset[i], glyph_index))
	    return &priv->glyphset[i];
    } else {
	for (i = 0; i < NUM_GLYPHSETS; i++) {
	    if (has_pending_free_glyph (&priv->glyphset[i], glyph_index))
		return &priv->glyphset[i];
	}
    }

    return NULL;
}

static cairo_status_t
_cairo_xlib_surface_add_glyph (cairo_xlib_display_t *display,
			       cairo_scaled_font_t   *font,
			       cairo_scaled_glyph_t **pscaled_glyph)
{
    XGlyphInfo glyph_info;
    unsigned long glyph_index;
    unsigned char *data;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_scaled_glyph_t *glyph = *pscaled_glyph;
    cairo_image_surface_t *glyph_surface = glyph->surface;
    cairo_bool_t already_had_glyph_surface;
    cairo_xlib_font_glyphset_t *info;

    glyph_index = glyph->hash_entry.hash;

    /* check to see if we have a pending XRenderFreeGlyph for this glyph */
    info = find_pending_free_glyph (display, font, glyph_index, glyph_surface);
    if (info != NULL)
	return _cairo_xlib_glyph_attach (display, glyph, info);

    if (glyph_surface == NULL) {
	status = _cairo_scaled_glyph_lookup (font,
					     glyph_index,
					     CAIRO_SCALED_GLYPH_INFO_METRICS |
					     CAIRO_SCALED_GLYPH_INFO_SURFACE,
					     pscaled_glyph);
	if (unlikely (status))
	    return status;

	glyph = *pscaled_glyph;
	glyph_surface = glyph->surface;
	already_had_glyph_surface = FALSE;
    } else {
	already_had_glyph_surface = TRUE;
    }

    info = _cairo_xlib_font_get_glyphset_info_for_format (display, font,
							  glyph_surface->format);

#if 0
    /* If the glyph surface has zero height or width, we create
     * a clear 1x1 surface, to avoid various X server bugs.
     */
    if (glyph_surface->width == 0 || glyph_surface->height == 0) {
	cairo_surface_t *tmp_surface;

	tmp_surface = cairo_image_surface_create (info->format, 1, 1);
	status = tmp_surface->status;
	if (unlikely (status))
	    goto BAIL;

	tmp_surface->device_transform = glyph_surface->base.device_transform;
	tmp_surface->device_transform_inverse = glyph_surface->base.device_transform_inverse;

	glyph_surface = (cairo_image_surface_t *) tmp_surface;
    }
#endif

    /* If the glyph format does not match the font format, then we
     * create a temporary surface for the glyph image with the font's
     * format.
     */
    if (glyph_surface->format != info->format) {
	cairo_surface_pattern_t pattern;
	cairo_surface_t *tmp_surface;

	tmp_surface = cairo_image_surface_create (info->format,
						  glyph_surface->width,
						  glyph_surface->height);
	status = tmp_surface->status;
	if (unlikely (status))
	    goto BAIL;

	tmp_surface->device_transform = glyph_surface->base.device_transform;
	tmp_surface->device_transform_inverse = glyph_surface->base.device_transform_inverse;

	_cairo_pattern_init_for_surface (&pattern, &glyph_surface->base);
	status = _cairo_surface_paint (tmp_surface,
				       CAIRO_OPERATOR_SOURCE, &pattern.base,
				       NULL);
	_cairo_pattern_fini (&pattern.base);

	glyph_surface = (cairo_image_surface_t *) tmp_surface;

	if (unlikely (status))
	    goto BAIL;
    }

    /* XXX: FRAGILE: We're ignore device_transform scaling here. A bug? */
    glyph_info.x = _cairo_lround (glyph_surface->base.device_transform.x0);
    glyph_info.y = _cairo_lround (glyph_surface->base.device_transform.y0);
    glyph_info.width = glyph_surface->width;
    glyph_info.height = glyph_surface->height;
    glyph_info.xOff = glyph->x_advance;
    glyph_info.yOff = glyph->y_advance;

    data = glyph_surface->data;

    /* flip formats around */
    switch (_cairo_xlib_get_glyphset_index_for_format (glyph->surface->format)) {
    case GLYPHSET_INDEX_A1:
	/* local bitmaps are always stored with bit == byte */
	if (_cairo_is_little_endian() != (BitmapBitOrder (display->display) == LSBFirst)) {
	    int		    c = glyph_surface->stride * glyph_surface->height;
	    unsigned char   *d;
	    unsigned char   *new, *n;

	    if (c == 0)
		break;

	    new = _cairo_malloc (c);
	    if (!new) {
		status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
		goto BAIL;
	    }
	    n = new;
	    d = data;
	    do {
		char	b = *d++;
		b = ((b << 1) & 0xaa) | ((b >> 1) & 0x55);
		b = ((b << 2) & 0xcc) | ((b >> 2) & 0x33);
		b = ((b << 4) & 0xf0) | ((b >> 4) & 0x0f);
		*n++ = b;
	    } while (--c);
	    data = new;
	}
	break;
    case GLYPHSET_INDEX_A8:
	break;
    case GLYPHSET_INDEX_ARGB32:
	if (_cairo_is_little_endian() != (ImageByteOrder (display->display) == LSBFirst)) {
	    unsigned int c = glyph_surface->stride * glyph_surface->height / 4;
	    const uint32_t *d;
	    uint32_t *new, *n;

	    if (c == 0)
		break;

	    new = _cairo_malloc (4 * c);
	    if (unlikely (new == NULL)) {
		status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
		goto BAIL;
	    }
	    n = new;
	    d = (uint32_t *) data;
	    do {
		*n++ = bswap_32 (*d);
		d++;
	    } while (--c);
	    data = (uint8_t *) new;
	}
	break;
    default:
	ASSERT_NOT_REACHED;
	break;
    }
    /* XXX assume X server wants pixman padding. Xft assumes this as well */

    XRenderAddGlyphs (display->display, info->glyphset,
		      &glyph_index, &glyph_info, 1,
		      (char *) data,
		      glyph_surface->stride * glyph_surface->height);

    if (data != glyph_surface->data)
	free (data);

    status = _cairo_xlib_glyph_attach (display, glyph, info);

 BAIL:
    if (glyph_surface != glyph->surface)
	cairo_surface_destroy (&glyph_surface->base);

    /* if the scaled glyph didn't already have a surface attached
     * to it, release the created surface now that we have it
     * uploaded to the X server.  If the surface has already been
     * there (eg. because image backend requested it), leave it in
     * the cache
     */
    if (!already_had_glyph_surface)
	_cairo_scaled_glyph_set_surface (glyph, font, NULL);

    return status;
}

typedef void (*cairo_xrender_composite_text_func_t)
	      (Display                      *dpy,
	       int                          op,
	       Picture                      src,
	       Picture                      dst,
	       _Xconst XRenderPictFormat    *maskFormat,
	       int                          xSrc,
	       int                          ySrc,
	       int                          xDst,
	       int                          yDst,
	       _Xconst XGlyphElt8           *elts,
	       int                          nelt);

/* Build a struct of the same size of #cairo_glyph_t that can be used both as
 * an input glyph with double coordinates, and as "working" glyph with
 * integer from-current-point offsets. */
typedef union {
    cairo_glyph_t d;
    unsigned long index;
    struct {
        unsigned long index;
        int x;
        int y;
    } i;
} cairo_xlib_glyph_t;

/* compile-time assert that #cairo_xlib_glyph_t is the same size as #cairo_glyph_t */
COMPILE_TIME_ASSERT (sizeof (cairo_xlib_glyph_t) == sizeof (cairo_glyph_t));

/* Start a new element for the first glyph,
 * or for any glyph that has unexpected position,
 * or if current element has too many glyphs
 * (Xrender limits each element to 252 glyphs, we limit them to 128)
 *
 * These same conditions need to be mirrored between
 * _cairo_xlib_surface_emit_glyphs and _emit_glyph_chunks
 */
#define _start_new_glyph_elt(count, glyph) \
    (((count) & 127) == 0 || (glyph)->i.x || (glyph)->i.y)

static cairo_status_t
_emit_glyphs_chunk (cairo_xlib_display_t *display,
                    cairo_xlib_surface_t *dst,
		    int dst_x, int dst_y,
		    cairo_xlib_glyph_t *glyphs,
		    int num_glyphs,
		    cairo_scaled_font_t *font,
		    cairo_bool_t use_mask,
		    cairo_operator_t op,
		    cairo_xlib_source_t *src,
		    int src_x, int src_y,
		    /* info for this chunk */
		    int num_elts,
		    int width,
		    cairo_xlib_font_glyphset_t *info)
{
    /* Which XRenderCompositeText function to use */
    cairo_xrender_composite_text_func_t composite_text_func;
    int size;

    /* Element buffer stuff */
    XGlyphElt8 *elts;
    XGlyphElt8 stack_elts[CAIRO_STACK_ARRAY_LENGTH (XGlyphElt8)];

    /* Reuse the input glyph array for output char generation */
    char *char8 = (char *) glyphs;
    unsigned short *char16 = (unsigned short *) glyphs;
    unsigned int *char32 = (unsigned int *) glyphs;

    int i;
    int nelt; /* Element index */
    int n; /* Num output glyphs in current element */
    int j; /* Num output glyphs so far */

    switch (width) {
    case 1:
	/* don't cast the 8-variant, to catch possible mismatches */
	composite_text_func = XRenderCompositeText8;
	size = sizeof (char);
	break;
    case 2:
	composite_text_func = (cairo_xrender_composite_text_func_t) XRenderCompositeText16;
	size = sizeof (unsigned short);
	break;
    default:
    case 4:
	composite_text_func = (cairo_xrender_composite_text_func_t) XRenderCompositeText32;
	size = sizeof (unsigned int);
    }

    /* Allocate element array */
    if (num_elts <= ARRAY_LENGTH (stack_elts)) {
      elts = stack_elts;
    } else {
      elts = _cairo_malloc_ab (num_elts, sizeof (XGlyphElt8));
      if (unlikely (elts == NULL))
	  return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    /* Fill them in */
    nelt = 0;
    n = 0;
    j = 0;
    for (i = 0; i < num_glyphs; i++) {
      /* Start a new element for first output glyph,
       * or for any glyph that has unexpected position,
       * or if current element has too many glyphs.
       *
       * These same conditions are mirrored in _cairo_xlib_surface_emit_glyphs()
       */
      if (_start_new_glyph_elt (j, &glyphs[i])) {
	  if (j) {
	      elts[nelt].nchars = n;
	      nelt++;
	      n = 0;
	  }
	  elts[nelt].chars = char8 + size * j;
	  elts[nelt].glyphset = info->glyphset;
	  elts[nelt].xOff = glyphs[i].i.x;
	  elts[nelt].yOff = glyphs[i].i.y;
      }

      switch (width) {
      case 1: char8 [j] = (char)           glyphs[i].index; break;
      case 2: char16[j] = (unsigned short) glyphs[i].index; break;
      default:
      case 4: char32[j] = (unsigned int)   glyphs[i].index; break;
      }

      n++;
      j++;
    }

    if (n) {
	elts[nelt].nchars = n;
	nelt++;
    }

    /* Check that we agree with _cairo_xlib_surface_emit_glyphs() on the
     * expected number of xGlyphElts.  */
    assert (nelt == num_elts);

    composite_text_func (display->display, op,
			 src->picture,
			 dst->picture,
			 use_mask ? info->xrender_format : NULL,
			 src_x + elts[0].xOff + dst_x,
			 src_y + elts[0].yOff + dst_y,
			 elts[0].xOff, elts[0].yOff,
			 (XGlyphElt8 *) elts, nelt);

    if (elts != stack_elts)
      free (elts);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
check_composite_glyphs (const cairo_composite_rectangles_t *extents,
			cairo_scaled_font_t *font,
			cairo_glyph_t *glyphs,
			int *num_glyphs)
{
    cairo_xlib_surface_t *dst = (cairo_xlib_surface_t *)extents->surface;
    cairo_xlib_display_t *display = dst->display;
    int max_request_size, size;

    TRACE ((stderr, "%s\n", __FUNCTION__));

    if (! CAIRO_RENDER_SUPPORTS_OPERATOR (display, extents->op))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* The glyph coordinates must be representable in an int16_t.
     * When possible, they will be expressed as an offset from the
     * previous glyph, otherwise they will be an offset from the
     * surface origin. If we can't guarantee this to be possible,
     * fallback.
     */
    if (extents->bounded.x + extents->bounded.width > INT16_MAX ||
	extents->bounded.y + extents->bounded.height> INT16_MAX ||
	extents->bounded.x < INT16_MIN ||
	extents->bounded.y < INT16_MIN)
    {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    /* Approximate the size of the largest glyph and fallback if we can not
     * upload it to the xserver.
     */
    size = ceil (font->max_scale);
    size = 4 * size * size;
    max_request_size = (XExtendedMaxRequestSize (display->display) ? XExtendedMaxRequestSize (display->display)
			: XMaxRequestSize (display->display)) * 4 -
	sz_xRenderAddGlyphsReq -
	sz_xGlyphInfo          -
	8;
    if (size >= max_request_size)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    return CAIRO_STATUS_SUCCESS;
}

/* sz_xGlyphtElt required alignment to a 32-bit boundary, so ensure we have
 * enough room for padding */
#define _cairo_sz_xGlyphElt (sz_xGlyphElt + 4)

#define PHASE(x) ((int)(floor (4 * (x + 0.125)) - 4 * floor (x + 0.125)))
#define POSITION(x) ((int) floor (x + 0.125))

static cairo_int_status_t
composite_glyphs (void				*surface,
		  cairo_operator_t		 op,
		  cairo_surface_t		*_src,
		  int				 src_x,
		  int				 src_y,
		  int				 dst_x,
		  int				 dst_y,
		  cairo_composite_glyphs_info_t *info)
{
    cairo_xlib_surface_t *dst = surface;
    cairo_xlib_glyph_t *glyphs = (cairo_xlib_glyph_t *)info->glyphs;
    cairo_xlib_source_t *src = (cairo_xlib_source_t *)_src;
    cairo_xlib_display_t *display = dst->display;
    cairo_int_status_t status = CAIRO_INT_STATUS_SUCCESS;
    cairo_scaled_glyph_t *glyph;
    cairo_fixed_t x = dst_x, y = dst_y;
    cairo_xlib_font_glyphset_t *glyphset = NULL, *this_glyphset_info;

    unsigned long max_index = 0;
    int width = 1;
    int num_elts = 0;
    int num_out_glyphs = 0;
    int num_glyphs = info->num_glyphs;

    int max_request_size = XMaxRequestSize (display->display) * 4
			 - MAX (sz_xRenderCompositeGlyphs8Req,
				MAX(sz_xRenderCompositeGlyphs16Req,
				    sz_xRenderCompositeGlyphs32Req));
    int request_size = 0;
    int i;

    op = _render_operator (op),
    _cairo_xlib_surface_ensure_picture (dst);
    for (i = 0; i < num_glyphs; i++) {
        unsigned long xphase, yphase;
	int this_x, this_y;
	int old_width;

        xphase = PHASE(glyphs[i].d.x);
        yphase = PHASE(glyphs[i].d.y);

        glyphs[i].index |= (xphase << 24) | (yphase << 26);

	status = _cairo_scaled_glyph_lookup (info->font,
					     glyphs[i].index,
					     CAIRO_SCALED_GLYPH_INFO_METRICS,
					     &glyph);
	if (unlikely (status))
	    return status;

	this_x = POSITION (glyphs[i].d.x);
	this_y = POSITION (glyphs[i].d.y);

	/* Send unsent glyphs to the server */
	if (glyph->dev_private_key != display) {
	    status = _cairo_xlib_surface_add_glyph (display, info->font, &glyph);
	    if (unlikely (status))
		return status;
	}

	this_glyphset_info = glyph->dev_private;
	if (!glyphset)
	    glyphset = this_glyphset_info;

	/* The invariant here is that we can always flush the glyphs
	 * accumulated before this one, using old_width, and they
	 * would fit in the request.
	 */
	old_width = width;

	/* Update max glyph index */
	if (glyphs[i].index > max_index) {
	    max_index = glyphs[i].index;
	    if (max_index >= 65536)
	      width = 4;
	    else if (max_index >= 256)
	      width = 2;
	    if (width != old_width)
	      request_size += (width - old_width) * num_out_glyphs;
	}

	/* If we will pass the max request size by adding this glyph,
	 * flush current glyphs.  Note that we account for a
	 * possible element being added below.
	 *
	 * Also flush if changing glyphsets, as Xrender limits one mask
	 * format per request, so we can either break up, or use a
	 * wide-enough mask format.  We do the former.  One reason to
	 * prefer the latter is the fact that Xserver ADDs all glyphs
	 * to the mask first, and then composes that to final surface,
	 * though it's not a big deal.
	 *
	 * If the glyph has a coordinate which cannot be represented
	 * as a 16-bit offset from the previous glyph, flush the
	 * current chunk. The current glyph will be the first one in
	 * the next chunk, thus its coordinates will be an offset from
	 * the destination origin. This offset is guaranteed to be
	 * representable as 16-bit offset (otherwise we would have
	 * fallen back).
	 */
	if (request_size + width > max_request_size - _cairo_sz_xGlyphElt ||
	    this_x - x > INT16_MAX || this_x - x < INT16_MIN ||
	    this_y - y > INT16_MAX || this_y - y < INT16_MIN ||
	    (this_glyphset_info != glyphset)) {
	    status = _emit_glyphs_chunk (display, dst, dst_x, dst_y,
					 glyphs, i, info->font, info->use_mask,
					 op, src, src_x, src_y,
					 num_elts, old_width, glyphset);
	    if (unlikely (status))
		return status;

	    glyphs += i;
	    num_glyphs -= i;
	    i = 0;
	    max_index = glyphs[i].index;
	    width = max_index < 256 ? 1 : max_index < 65536 ? 2 : 4;
	    request_size = 0;
	    num_elts = 0;
	    num_out_glyphs = 0;
	    x = y = 0;
	    glyphset = this_glyphset_info;
	}

	/* Convert absolute glyph position to relative-to-current-point
	 * position */
	glyphs[i].i.x = this_x - x;
	glyphs[i].i.y = this_y - y;

	/* Start a new element for the first glyph,
	 * or for any glyph that has unexpected position,
	 * or if current element has too many glyphs.
	 *
	 * These same conditions are mirrored in _emit_glyphs_chunk().
	 */
      if (_start_new_glyph_elt (num_out_glyphs, &glyphs[i])) {
	    num_elts++;
	    request_size += _cairo_sz_xGlyphElt;
	}

	/* adjust current-position */
	x = this_x + glyph->x_advance;
	y = this_y + glyph->y_advance;

	num_out_glyphs++;
	request_size += width;
    }

    if (num_elts) {
	status = _emit_glyphs_chunk (display, dst, dst_x, dst_y,
				     glyphs, i, info->font, info->use_mask,
				     op, src, src_x, src_y,
				     num_elts, width, glyphset);
    }

    return status;
}

const cairo_compositor_t *
_cairo_xlib_mask_compositor_get (void)
{
    static cairo_atomic_once_t once = CAIRO_ATOMIC_ONCE_INIT;
    static cairo_mask_compositor_t compositor;

    if (_cairo_atomic_init_once_enter(&once)) {
	_cairo_mask_compositor_init (&compositor,
				     _cairo_xlib_fallback_compositor_get ());

	compositor.acquire = acquire;
	compositor.release = release;
	compositor.set_clip_region = set_clip_region;
	compositor.pattern_to_surface = _cairo_xlib_source_create_for_pattern;
	compositor.draw_image_boxes = draw_image_boxes;
	compositor.fill_rectangles = fill_rectangles;
	compositor.fill_boxes = fill_boxes;
	compositor.copy_boxes = copy_boxes;
	compositor.check_composite = check_composite;
	compositor.composite = composite;
	//compositor.check_composite_boxes = check_composite_boxes;
	compositor.composite_boxes = composite_boxes;
	compositor.check_composite_glyphs = check_composite_glyphs;
	compositor.composite_glyphs = composite_glyphs;

	_cairo_atomic_init_once_leave(&once);
    }

    return &compositor.base;
}

#define CAIRO_FIXED_16_16_MIN -32768
#define CAIRO_FIXED_16_16_MAX 32767

static cairo_bool_t
line_exceeds_16_16 (const cairo_line_t *line)
{
    return
	line->p1.x < _cairo_fixed_from_int (CAIRO_FIXED_16_16_MIN) ||
	line->p1.x > _cairo_fixed_from_int (CAIRO_FIXED_16_16_MAX) ||
	line->p2.x < _cairo_fixed_from_int (CAIRO_FIXED_16_16_MIN) ||
	line->p2.x > _cairo_fixed_from_int (CAIRO_FIXED_16_16_MAX) ||
	line->p1.y < _cairo_fixed_from_int (CAIRO_FIXED_16_16_MIN) ||
	line->p1.y > _cairo_fixed_from_int (CAIRO_FIXED_16_16_MAX) ||
	line->p2.y < _cairo_fixed_from_int (CAIRO_FIXED_16_16_MIN) ||
	line->p2.y > _cairo_fixed_from_int (CAIRO_FIXED_16_16_MAX);
}

static void
project_line_x_onto_16_16 (const cairo_line_t *line,
			    cairo_fixed_t top,
			    cairo_fixed_t bottom,
			    XLineFixed *out)
{
    cairo_point_double_t p1, p2;
    double m;

    p1.x = _cairo_fixed_to_double (line->p1.x);
    p1.y = _cairo_fixed_to_double (line->p1.y);

    p2.x = _cairo_fixed_to_double (line->p2.x);
    p2.y = _cairo_fixed_to_double (line->p2.y);

    m = (p2.x - p1.x) / (p2.y - p1.y);
    out->p1.x = _cairo_fixed_16_16_from_double (p1.x + m * _cairo_fixed_to_double (top - line->p1.y));
    out->p2.x = _cairo_fixed_16_16_from_double (p1.x + m * _cairo_fixed_to_double (bottom - line->p1.y));
}
#if 0
static cairo_int_status_T
check_composite_trapezoids ()
{
    operation = _categorize_composite_operation (dst, op, pattern, TRUE);
    if (operation == DO_UNSUPPORTED)
	return UNSUPPORTED ("unsupported operation");

    operation = _recategorize_composite_operation (dst, op, src,
						   &attributes, TRUE);
    if (operation == DO_UNSUPPORTED) {
	status = UNSUPPORTED ("unsupported operation");
	goto BAIL;
    }

}
#endif

static cairo_int_status_t
composite_traps (void			*abstract_dst,
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
    cairo_xlib_surface_t	*dst = abstract_dst;
    cairo_xlib_display_t        *display = dst->display;
    cairo_xlib_source_t		*src = (cairo_xlib_source_t *)abstract_src;
    XRenderPictFormat		*pict_format;
    XTrapezoid xtraps_stack[CAIRO_STACK_ARRAY_LENGTH (XTrapezoid)];
    XTrapezoid *xtraps = xtraps_stack;
    int dx, dy;
    int i;

    //X_DEBUG ((display->display, "composite_trapezoids (dst=%x)", (unsigned int) dst->drawable));

    if (traps->num_traps == 0)
	return CAIRO_STATUS_SUCCESS;

    if (dst->base.is_clear &&
	(op == CAIRO_OPERATOR_OVER || op == CAIRO_OPERATOR_ADD))
    {
	op = CAIRO_OPERATOR_SOURCE;
    }

    pict_format =
	_cairo_xlib_display_get_xrender_format (display,
						antialias == CAIRO_ANTIALIAS_NONE ?  CAIRO_FORMAT_A1 : CAIRO_FORMAT_A8);

    if (traps->num_traps > ARRAY_LENGTH (xtraps_stack)) {
	xtraps = _cairo_malloc_ab (traps->num_traps, sizeof (XTrapezoid));
	if (unlikely (xtraps == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    dx = -dst_x << 16;
    dy = -dst_y << 16;
    for (i = 0; i < traps->num_traps; i++) {
	cairo_trapezoid_t *t = &traps->traps[i];

	/* top/bottom will be clamped to surface bounds */
	xtraps[i].top = _cairo_fixed_to_16_16(t->top) + dy;
	xtraps[i].bottom = _cairo_fixed_to_16_16(t->bottom) + dy;

	/* However, all the other coordinates will have been left untouched so
	 * as not to introduce numerical error. Recompute them if they
	 * exceed the 16.16 limits.
	 */
	if (unlikely (line_exceeds_16_16 (&t->left))) {
	    project_line_x_onto_16_16 (&t->left, t->top, t->bottom,
					&xtraps[i].left);
	    xtraps[i].left.p1.x += dx;
	    xtraps[i].left.p2.x += dx;
	    xtraps[i].left.p1.y = xtraps[i].top;
	    xtraps[i].left.p2.y = xtraps[i].bottom;
	} else {
	    xtraps[i].left.p1.x = _cairo_fixed_to_16_16(t->left.p1.x) + dx;
	    xtraps[i].left.p1.y = _cairo_fixed_to_16_16(t->left.p1.y) + dy;
	    xtraps[i].left.p2.x = _cairo_fixed_to_16_16(t->left.p2.x) + dx;
	    xtraps[i].left.p2.y = _cairo_fixed_to_16_16(t->left.p2.y) + dy;
	}

	if (unlikely (line_exceeds_16_16 (&t->right))) {
	    project_line_x_onto_16_16 (&t->right, t->top, t->bottom,
				       &xtraps[i].right);
	    xtraps[i].right.p1.x += dx;
	    xtraps[i].right.p2.x += dx;
	    xtraps[i].right.p1.y = xtraps[i].top;
	    xtraps[i].right.p2.y = xtraps[i].bottom;
	} else {
	    xtraps[i].right.p1.x = _cairo_fixed_to_16_16(t->right.p1.x) + dx;
	    xtraps[i].right.p1.y = _cairo_fixed_to_16_16(t->right.p1.y) + dy;
	    xtraps[i].right.p2.x = _cairo_fixed_to_16_16(t->right.p2.x) + dx;
	    xtraps[i].right.p2.y = _cairo_fixed_to_16_16(t->right.p2.y) + dy;
	}
    }

    if (xtraps[0].left.p1.y < xtraps[0].left.p2.y) {
	src_x += _cairo_fixed_16_16_floor (xtraps[0].left.p1.x);
	src_y += _cairo_fixed_16_16_floor (xtraps[0].left.p1.y);
    } else {
	src_x += _cairo_fixed_16_16_floor (xtraps[0].left.p2.x);
	src_y += _cairo_fixed_16_16_floor (xtraps[0].left.p2.y);
    }
    src_x += dst_x;
    src_y += dst_y;

    _cairo_xlib_surface_ensure_picture (dst);
    _cairo_xlib_surface_set_precision (dst, antialias);
    XRenderCompositeTrapezoids (dst->dpy,
				_render_operator (op),
				src->picture, dst->picture,
				pict_format,
				src_x, src_y,
				xtraps, traps->num_traps);

    if (xtraps != xtraps_stack)
	free (xtraps);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
composite_tristrip (void		*abstract_dst,
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
    cairo_xlib_surface_t	*dst = abstract_dst;
    cairo_xlib_display_t        *display = dst->display;
    cairo_xlib_source_t		*src = (cairo_xlib_source_t *)abstract_src;
    XRenderPictFormat		*pict_format;
    XPointFixed points_stack[CAIRO_STACK_ARRAY_LENGTH (XPointFixed)];
    XPointFixed *points = points_stack;
    int dx, dy;
    int i;

    //X_DEBUG ((display->display, "composite_trapezoids (dst=%x)", (unsigned int) dst->drawable));

    pict_format =
	_cairo_xlib_display_get_xrender_format (display,
						antialias == CAIRO_ANTIALIAS_NONE ?  CAIRO_FORMAT_A1 : CAIRO_FORMAT_A8);

    if (strip->num_points > ARRAY_LENGTH (points_stack)) {
	points = _cairo_malloc_ab (strip->num_points, sizeof (XPointFixed));
	if (unlikely (points == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    dx = -dst_x << 16;
    dy = -dst_y << 16;
    for (i = 0; i < strip->num_points; i++) {
	cairo_point_t *p = &strip->points[i];

	points[i].x = _cairo_fixed_to_16_16(p->x) + dx;
	points[i].y = _cairo_fixed_to_16_16(p->y) + dy;
    }

    src_x += _cairo_fixed_16_16_floor (points[0].x) + dst_x;
    src_y += _cairo_fixed_16_16_floor (points[0].y) + dst_y;

    _cairo_xlib_surface_ensure_picture (dst);
    _cairo_xlib_surface_set_precision (dst, antialias);
    XRenderCompositeTriStrip (dst->dpy,
			      _render_operator (op),
			      src->picture, dst->picture,
			      pict_format,
			      src_x, src_y,
			      points, strip->num_points);

    if (points != points_stack)
	free (points);

    return CAIRO_STATUS_SUCCESS;
}

const cairo_compositor_t *
_cairo_xlib_traps_compositor_get (void)
{
    static cairo_atomic_once_t once = CAIRO_ATOMIC_ONCE_INIT;
    static cairo_traps_compositor_t compositor;

    if (_cairo_atomic_init_once_enter(&once)) {
	_cairo_traps_compositor_init (&compositor,
				      _cairo_xlib_mask_compositor_get ());

	compositor.acquire = acquire;
	compositor.release = release;
	compositor.set_clip_region = set_clip_region;
	compositor.pattern_to_surface = _cairo_xlib_source_create_for_pattern;
	compositor.draw_image_boxes = draw_image_boxes;
	compositor.copy_boxes = copy_boxes;
	compositor.fill_boxes = fill_boxes;
	compositor.check_composite = check_composite;
	compositor.composite = composite;
	compositor.lerp = lerp;
	//compositor.check_composite_boxes = check_composite_boxes;
	compositor.composite_boxes = composite_boxes;
	//compositor.check_composite_traps = check_composite_traps;
	compositor.composite_traps = composite_traps;
	//compositor.check_composite_tristrip = check_composite_tristrip;
	compositor.composite_tristrip = composite_tristrip;
	compositor.check_composite_glyphs = check_composite_glyphs;
	compositor.composite_glyphs = composite_glyphs;

	_cairo_atomic_init_once_leave(&once);
    }

    return &compositor.base;
}

#endif /* !CAIRO_HAS_XLIB_XCB_FUNCTIONS */
