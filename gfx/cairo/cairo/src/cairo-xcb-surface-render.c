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
 * Contributor(s):
 *	Chris Wilson <chris@chris-wilson.co.uk>
 */

#include "cairoint.h"

#include "cairo-xcb-private.h"

#include "cairo-boxes-private.h"
#include "cairo-clip-inline.h"
#include "cairo-clip-private.h"
#include "cairo-composite-rectangles-private.h"
#include "cairo-image-surface-inline.h"
#include "cairo-image-surface-private.h"
#include "cairo-list-inline.h"
#include "cairo-region-private.h"
#include "cairo-surface-offset-private.h"
#include "cairo-surface-snapshot-inline.h"
#include "cairo-surface-subsurface-private.h"
#include "cairo-traps-private.h"
#include "cairo-recording-surface-inline.h"
#include "cairo-paginated-private.h"
#include "cairo-pattern-inline.h"

#define PIXMAN_MAX_INT ((pixman_fixed_1 >> 1) - pixman_fixed_e) /* need to ensure deltas also fit */

static cairo_status_t
_clip_and_composite_boxes (cairo_xcb_surface_t *dst,
			   cairo_operator_t op,
			   const cairo_pattern_t *src,
			   cairo_boxes_t *boxes,
			   cairo_composite_rectangles_t *extents);

static inline cairo_xcb_connection_t *
_picture_to_connection (cairo_xcb_picture_t *picture)
{
    return (cairo_xcb_connection_t *) picture->base.device;
}

static void
_cairo_xcb_surface_ensure_picture (cairo_xcb_surface_t *surface);

static uint32_t
hars_petruska_f54_1_random (void)
{
#define rol(x,k) ((x << k) | (x >> (32-k)))
    static uint32_t x;
    return x = (x ^ rol (x, 5) ^ rol (x, 24)) + 0x37798849;
#undef rol
}

static cairo_status_t
_cairo_xcb_picture_finish (void *abstract_surface)
{
    cairo_xcb_picture_t *surface = abstract_surface;
    cairo_xcb_connection_t *connection = _picture_to_connection (surface);
    cairo_status_t status;

    status = _cairo_xcb_connection_acquire (connection);
    cairo_list_del (&surface->link);
    if (unlikely (status))
	return status;

    _cairo_xcb_connection_render_free_picture (connection, surface->picture);

    _cairo_xcb_connection_release (connection);

    return CAIRO_STATUS_SUCCESS;
}

static const cairo_surface_backend_t _cairo_xcb_picture_backend = {
    CAIRO_SURFACE_TYPE_XCB,
    _cairo_xcb_picture_finish,
};

static const struct xcb_render_transform_t identity_transform = {
    1 << 16, 0, 0,
    0, 1 << 16, 0,
    0, 0, 1 << 16,
};

static cairo_xcb_picture_t *
_cairo_xcb_picture_create (cairo_xcb_screen_t *screen,
			   pixman_format_code_t pixman_format,
			   xcb_render_pictformat_t xrender_format,
			   int width, int height)
{
    cairo_xcb_picture_t *surface;

    surface = _cairo_malloc (sizeof (cairo_xcb_picture_t));
    if (unlikely (surface == NULL))
	return (cairo_xcb_picture_t *)
	    _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_surface_init (&surface->base,
			 &_cairo_xcb_picture_backend,
			 &screen->connection->device,
			 _cairo_content_from_pixman_format (pixman_format),
			 FALSE); /* is_vector */

    cairo_list_add (&surface->link, &screen->pictures);

    surface->screen = screen;
    surface->picture = xcb_generate_id (screen->connection->xcb_connection);
    surface->pixman_format = pixman_format;
    surface->xrender_format = xrender_format;

    surface->x0 = surface->y0 = 0;
    surface->x = surface->y = 0;
    surface->width = width;
    surface->height = height;

    surface->transform = identity_transform;
    surface->extend = CAIRO_EXTEND_NONE;
    surface->filter = CAIRO_FILTER_NEAREST;
    surface->has_component_alpha = FALSE;

    return surface;
}

static inline cairo_bool_t
_operator_is_supported (uint32_t flags, cairo_operator_t op)
{
    if (op <= CAIRO_OPERATOR_SATURATE)
	return TRUE;

    /* Can we use PDF operators? */
#if CAIRO_XCB_RENDER_AT_LEAST(0, 11)
    if (op <= CAIRO_OPERATOR_HSL_LUMINOSITY)
	return flags & CAIRO_XCB_RENDER_HAS_PDF_OPERATORS;
#endif

    return FALSE;
}

static int
_render_operator (cairo_operator_t op)
{
#define C(x,y) case CAIRO_OPERATOR_##x: return XCB_RENDER_PICT_OP_##y
    switch (op) {
    C(CLEAR, CLEAR);
    C(SOURCE, SRC);

    C(OVER, OVER);
    C(IN, IN);
    C(OUT, OUT);
    C(ATOP, ATOP);

    C(DEST, DST);
    C(DEST_OVER, OVER_REVERSE);
    C(DEST_IN, IN_REVERSE);
    C(DEST_OUT, OUT_REVERSE);
    C(DEST_ATOP, ATOP_REVERSE);

    C(XOR, XOR);
    C(ADD, ADD);
    C(SATURATE, SATURATE);

    /* PDF operators were added in RENDER 0.11, check if the xcb headers have
     * the defines, else fall through to the default case. */
#if CAIRO_XCB_RENDER_AT_LEAST(0, 11)
#define BLEND(x,y) C(x,y)
#else
#define BLEND(x,y) case CAIRO_OPERATOR_##x:
#endif
    BLEND(MULTIPLY, MULTIPLY);
    BLEND(SCREEN, SCREEN);
    BLEND(OVERLAY, OVERLAY);
    BLEND(DARKEN, DARKEN);
    BLEND(LIGHTEN, LIGHTEN);
    BLEND(COLOR_DODGE, COLOR_DODGE);
    BLEND(COLOR_BURN, COLOR_BURN);
    BLEND(HARD_LIGHT, HARD_LIGHT);
    BLEND(SOFT_LIGHT, SOFT_LIGHT);
    BLEND(DIFFERENCE, DIFFERENCE);
    BLEND(EXCLUSION, EXCLUSION);
    BLEND(HSL_HUE, HSL_HUE);
    BLEND(HSL_SATURATION, HSL_SATURATION);
    BLEND(HSL_COLOR, HSL_COLOR);
    BLEND(HSL_LUMINOSITY, HSL_LUMINOSITY);

    default:
	ASSERT_NOT_REACHED;
	return XCB_RENDER_PICT_OP_OVER;
    }
}

static cairo_status_t
_cairo_xcb_surface_set_clip_region (cairo_xcb_surface_t *surface,
				    cairo_region_t	*region)
{
    xcb_rectangle_t stack_rects[CAIRO_STACK_ARRAY_LENGTH (xcb_rectangle_t)];
    xcb_rectangle_t *rects = stack_rects;
    int i, num_rects;

    num_rects = cairo_region_num_rectangles (region);

    if (num_rects > ARRAY_LENGTH (stack_rects)) {
	rects = _cairo_malloc_ab (num_rects, sizeof (xcb_rectangle_t));
	if (unlikely (rects == NULL)) {
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
	}
    }

    for (i = 0; i < num_rects; i++) {
	cairo_rectangle_int_t rect;

	cairo_region_get_rectangle (region, i, &rect);

	rects[i].x = rect.x;
	rects[i].y = rect.y;
	rects[i].width  = rect.width;
	rects[i].height = rect.height;
    }

    _cairo_xcb_connection_render_set_picture_clip_rectangles (surface->connection,
							      surface->picture,
							      0, 0,
							      num_rects, rects);

    if (rects != stack_rects)
	free (rects);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_xcb_surface_clear_clip_region (cairo_xcb_surface_t *surface)
{
    uint32_t values[] = { XCB_NONE };
    _cairo_xcb_connection_render_change_picture (surface->connection,
						 surface->picture,
						 XCB_RENDER_CP_CLIP_MASK,
						 values);
}

static void
_cairo_xcb_surface_set_precision (cairo_xcb_surface_t	*surface,
				  cairo_antialias_t	 antialias)
{
    cairo_xcb_connection_t *connection = surface->connection;
    uint32_t precision;

    if (connection->force_precision != -1)
	    precision = connection->force_precision;
    else switch (antialias) {
    default:
    case CAIRO_ANTIALIAS_DEFAULT:
    case CAIRO_ANTIALIAS_GRAY:
    case CAIRO_ANTIALIAS_NONE:
    case CAIRO_ANTIALIAS_FAST:
    case CAIRO_ANTIALIAS_GOOD:
	precision = XCB_RENDER_POLY_MODE_IMPRECISE;
	break;
    case CAIRO_ANTIALIAS_SUBPIXEL:
    case CAIRO_ANTIALIAS_BEST:
	precision = XCB_RENDER_POLY_MODE_PRECISE;
	break;
    }

    if (surface->precision != precision) {
	_cairo_xcb_connection_render_change_picture (connection,
						     surface->picture,
						     XCB_RENDER_CP_POLY_MODE,
						     &precision);
	surface->precision = precision;
    }
}


static void
_cairo_xcb_surface_ensure_picture (cairo_xcb_surface_t *surface)
{
    assert (surface->fallback == NULL);
    if (surface->picture == XCB_NONE) {
	uint32_t values[1];
	uint32_t flags = 0;

	if (surface->precision != XCB_RENDER_POLY_MODE_PRECISE) {
	    flags |= XCB_RENDER_CP_POLY_MODE;
	    values[0] = surface->precision;
	}

	surface->picture = xcb_generate_id (surface->connection->xcb_connection);
	_cairo_xcb_connection_render_create_picture (surface->connection,
						     surface->picture,
						     surface->drawable,
						     surface->xrender_format,
						     flags, values);
    }
}

static cairo_xcb_picture_t *
_picture_from_image (cairo_xcb_surface_t *target,
		     xcb_render_pictformat_t format,
		     cairo_image_surface_t *image,
		     cairo_xcb_shm_info_t *shm_info)
{
    xcb_pixmap_t pixmap;
    xcb_gcontext_t gc;
    cairo_xcb_picture_t *picture;

    pixmap = _cairo_xcb_connection_create_pixmap (target->connection,
						  image->depth,
						  target->drawable,
						  image->width, image->height);

    gc = _cairo_xcb_screen_get_gc (target->screen, pixmap, image->depth);

    if (shm_info != NULL) {
	_cairo_xcb_connection_shm_put_image (target->connection,
					     pixmap, gc,
					     image->width, image->height,
					     0, 0,
					     image->width, image->height,
					     0, 0,
					     image->depth,
					     shm_info->shm,
					     shm_info->offset);
    } else {
	int len;

	/* Do we need to trim the image? */
	len = CAIRO_STRIDE_FOR_WIDTH_BPP (image->width, PIXMAN_FORMAT_BPP (image->pixman_format));
	if (len == image->stride) {
	    _cairo_xcb_connection_put_image (target->connection,
					     pixmap, gc,
					     image->width, image->height,
					     0, 0,
					     image->depth,
					     image->stride,
					     image->data);
	} else {
	    _cairo_xcb_connection_put_subimage (target->connection,
						pixmap, gc,
						0, 0,
						image->width, image->height,
						PIXMAN_FORMAT_BPP (image->pixman_format) / 8,
						image->stride,
						0, 0,
						image->depth,
						image->data);

	}
    }

    _cairo_xcb_screen_put_gc (target->screen, image->depth, gc);

    picture = _cairo_xcb_picture_create (target->screen,
					 image->pixman_format, format,
					 image->width, image->height);
    if (likely (picture->base.status == CAIRO_STATUS_SUCCESS)) {
	_cairo_xcb_connection_render_create_picture (target->connection,
						     picture->picture, pixmap, format,
						     0, 0);
    }

    xcb_free_pixmap (target->connection->xcb_connection, pixmap);

    return picture;
}

static cairo_bool_t
_pattern_is_supported (uint32_t flags,
		       const cairo_pattern_t *pattern)

{
    if (pattern->type == CAIRO_PATTERN_TYPE_SOLID)
	return TRUE;

    switch (pattern->extend) {
    default:
	ASSERT_NOT_REACHED;
    case CAIRO_EXTEND_NONE:
    case CAIRO_EXTEND_REPEAT:
	break;
    case CAIRO_EXTEND_PAD:
    case CAIRO_EXTEND_REFLECT:
	if ((flags & CAIRO_XCB_RENDER_HAS_EXTENDED_REPEAT) == 0)
	    return FALSE;
    }

    if (pattern->type == CAIRO_PATTERN_TYPE_SURFACE) {
	switch (pattern->filter) {
	case CAIRO_FILTER_FAST:
	case CAIRO_FILTER_NEAREST:
	    return (flags & CAIRO_XCB_RENDER_HAS_PICTURE_TRANSFORM) ||
		_cairo_matrix_is_integer_translation (&pattern->matrix, NULL, NULL);
	case CAIRO_FILTER_GOOD:
	    return flags & CAIRO_XCB_RENDER_HAS_FILTER_GOOD;
	case CAIRO_FILTER_BEST:
	    return flags & CAIRO_XCB_RENDER_HAS_FILTER_BEST;
	case CAIRO_FILTER_BILINEAR:
	case CAIRO_FILTER_GAUSSIAN:
	default:
	    return flags & CAIRO_XCB_RENDER_HAS_FILTERS;
	}
    } else if (pattern->type == CAIRO_PATTERN_TYPE_MESH) {
	return FALSE;
    } else { /* gradient */
	if ((flags & CAIRO_XCB_RENDER_HAS_GRADIENTS) == 0)
	    return FALSE;

	/* The RENDER specification says that the inner circle has to be
	 * completely contained inside the outer one. */
	if (pattern->type == CAIRO_PATTERN_TYPE_RADIAL &&
	    ! _cairo_radial_pattern_focus_is_inside ((cairo_radial_pattern_t *) pattern))
	{
	    return FALSE;
	}
	return TRUE;
    }
}

static void
_cairo_xcb_picture_set_matrix (cairo_xcb_picture_t *picture,
			       const cairo_matrix_t *matrix,
			       cairo_filter_t filter,
			       double xc, double yc)
{
    xcb_render_transform_t transform;
    pixman_transform_t *pixman_transform;
    cairo_int_status_t ignored;

    /* Casting between pixman_transform_t and xcb_render_transform_t is safe
     * because they happen to be the exact same type.
     */
    pixman_transform = (pixman_transform_t *) &transform;

    picture->x = picture->x0;
    picture->y = picture->y0;
    ignored = _cairo_matrix_to_pixman_matrix_offset (matrix, filter, xc, yc,
						     pixman_transform,
						     &picture->x, &picture->y);
    (void) ignored;

    if (memcmp (&picture->transform, &transform, sizeof (xcb_render_transform_t))) {
	_cairo_xcb_connection_render_set_picture_transform (_picture_to_connection (picture),
							    picture->picture,
							    &transform);

	picture->transform = transform;
    }
}

static void
_cairo_xcb_picture_set_filter (cairo_xcb_picture_t *picture,
			       cairo_filter_t filter)
{
    const char *render_filter;
    int len;

    if (picture->filter == filter)
	return;

    switch (filter) {
    case CAIRO_FILTER_FAST:
	render_filter = "fast";
	len = strlen ("fast");
	break;

    case CAIRO_FILTER_GOOD:
	render_filter = "good";
	len = strlen ("good");
	break;

    case CAIRO_FILTER_BEST:
	render_filter = "best";
	len = strlen ("best");
	break;

    case CAIRO_FILTER_NEAREST:
	render_filter = "nearest";
	len = strlen ("nearest");
	break;

    case CAIRO_FILTER_BILINEAR:
	render_filter = "bilinear";
	len = strlen ("bilinear");
	break;

    default:
	ASSERT_NOT_REACHED;
    case CAIRO_FILTER_GAUSSIAN:
	render_filter = "best";
	len = strlen ("best");
	break;
    }

    _cairo_xcb_connection_render_set_picture_filter (_picture_to_connection (picture),
						     picture->picture,
						     len, (char *) render_filter);
    picture->filter = filter;
}

static void
_cairo_xcb_picture_set_extend (cairo_xcb_picture_t *picture,
			       cairo_extend_t extend)
{
    uint32_t pa[1];

    if (picture->extend == extend)
	return;

    switch (extend) {
    default:
	ASSERT_NOT_REACHED;
    case CAIRO_EXTEND_NONE:
	pa[0] = XCB_RENDER_REPEAT_NONE;
	break;

    case CAIRO_EXTEND_REPEAT:
	pa[0] = XCB_RENDER_REPEAT_NORMAL;
	break;

    case CAIRO_EXTEND_REFLECT:
	pa[0] = XCB_RENDER_REPEAT_REFLECT;
	break;

    case CAIRO_EXTEND_PAD:
	pa[0] = XCB_RENDER_REPEAT_PAD;
	break;
    }

    _cairo_xcb_connection_render_change_picture (_picture_to_connection (picture),
						 picture->picture,
						 XCB_RENDER_CP_REPEAT, pa);
    picture->extend = extend;
}

static void
_cairo_xcb_picture_set_component_alpha (cairo_xcb_picture_t *picture,
					cairo_bool_t ca)
{
    uint32_t pa[1];

    if (picture->has_component_alpha == ca)
	return;

    pa[0] = ca;

    _cairo_xcb_connection_render_change_picture (_picture_to_connection (picture),
						 picture->picture,
						 XCB_RENDER_CP_COMPONENT_ALPHA,
						 pa);
    picture->has_component_alpha = ca;
}

static cairo_xcb_picture_t *
_solid_picture (cairo_xcb_surface_t *target,
		const cairo_color_t *color)
{
    xcb_render_color_t xcb_color;
    xcb_render_pictformat_t xrender_format;
    cairo_xcb_picture_t *picture;

    xcb_color.red   = color->red_short;
    xcb_color.green = color->green_short;
    xcb_color.blue  = color->blue_short;
    xcb_color.alpha = color->alpha_short;

    xrender_format = target->screen->connection->standard_formats[CAIRO_FORMAT_ARGB32];
    picture = _cairo_xcb_picture_create (target->screen,
					 PIXMAN_a8r8g8b8,
					 xrender_format,
					 -1, -1);
    if (unlikely (picture->base.status))
	return picture;

    if (target->connection->flags & CAIRO_XCB_RENDER_HAS_GRADIENTS) {
	_cairo_xcb_connection_render_create_solid_fill (target->connection,
							picture->picture,
							xcb_color);
    } else {
	xcb_pixmap_t pixmap;
	uint32_t values[] = { XCB_RENDER_REPEAT_NORMAL };

	pixmap = _cairo_xcb_connection_create_pixmap (target->connection,
						      32, target->drawable, 1, 1);
	_cairo_xcb_connection_render_create_picture (target->connection,
						     picture->picture,
						     pixmap,
						     xrender_format,
						     XCB_RENDER_CP_REPEAT,
						     values);
	if (target->connection->flags & CAIRO_XCB_RENDER_HAS_FILL_RECTANGLES) {
	    xcb_rectangle_t rect;

	    rect.x = rect.y = 0;
	    rect.width = rect.height = 1;

	    _cairo_xcb_connection_render_fill_rectangles (_picture_to_connection (picture),
							  XCB_RENDER_PICT_OP_SRC,
							  picture->picture,
							  xcb_color, 1, &rect);
	} else {
	    xcb_gcontext_t gc;
	    uint32_t pixel;

	    gc = _cairo_xcb_screen_get_gc (target->screen, pixmap, 32);

	    /* XXX byte ordering? */
	    pixel = (((uint32_t)color->alpha_short >> 8) << 24) |
		    ((color->red_short   >> 8) << 16) |
		    ((color->green_short >> 8) << 8) |
		    ((color->blue_short  >> 8) << 0);

	    _cairo_xcb_connection_put_image (target->connection,
					     pixmap, gc,
					     1, 1, 0, 0,
					     32, 4, &pixel);

	    _cairo_xcb_screen_put_gc (target->screen, 32, gc);
	}

	xcb_free_pixmap (target->connection->xcb_connection, pixmap);
    }

    return picture;
}

static cairo_xcb_picture_t *
_cairo_xcb_transparent_picture (cairo_xcb_surface_t *target)
{
    cairo_xcb_picture_t *picture;

    picture = (cairo_xcb_picture_t *) target->screen->stock_colors[CAIRO_STOCK_TRANSPARENT];
    if (picture == NULL) {
	picture = _solid_picture (target, CAIRO_COLOR_TRANSPARENT);
	target->screen->stock_colors[CAIRO_STOCK_TRANSPARENT] = &picture->base;
    }

    return (cairo_xcb_picture_t *) cairo_surface_reference (&picture->base);
}

static cairo_xcb_picture_t *
_cairo_xcb_black_picture (cairo_xcb_surface_t *target)
{
    cairo_xcb_picture_t *picture;

    picture = (cairo_xcb_picture_t *) target->screen->stock_colors[CAIRO_STOCK_BLACK];
    if (picture == NULL) {
	picture = _solid_picture (target, CAIRO_COLOR_BLACK);
	target->screen->stock_colors[CAIRO_STOCK_BLACK] = &picture->base;
    }

    return (cairo_xcb_picture_t *) cairo_surface_reference (&picture->base);
}

static cairo_xcb_picture_t *
_cairo_xcb_white_picture (cairo_xcb_surface_t *target)
{
    cairo_xcb_picture_t *picture;

    picture = (cairo_xcb_picture_t *) target->screen->stock_colors[CAIRO_STOCK_WHITE];
    if (picture == NULL) {
	picture = _solid_picture (target, CAIRO_COLOR_WHITE);
	target->screen->stock_colors[CAIRO_STOCK_WHITE] = &picture->base;
    }

    return (cairo_xcb_picture_t *) cairo_surface_reference (&picture->base);
}

static cairo_xcb_picture_t *
_cairo_xcb_solid_picture (cairo_xcb_surface_t *target,
			  const cairo_solid_pattern_t *pattern)
{
    cairo_xcb_picture_t *picture;
    cairo_xcb_screen_t *screen;
    int i, n_cached;

    if (pattern->color.alpha_short <= 0x00ff)
	return _cairo_xcb_transparent_picture (target);

    if (pattern->color.alpha_short >= 0xff00) {
	if (pattern->color.red_short <= 0x00ff &&
	    pattern->color.green_short <= 0x00ff &&
	    pattern->color.blue_short <= 0x00ff)
	{
	    return _cairo_xcb_black_picture (target);
	}

	if (pattern->color.red_short >= 0xff00 &&
	    pattern->color.green_short >= 0xff00 &&
	    pattern->color.blue_short >= 0xff00)
	{
	    return _cairo_xcb_white_picture (target);
	}
    }

    screen = target->screen;
    n_cached = screen->solid_cache_size;
    for (i = 0; i < n_cached; i++) {
	if (_cairo_color_equal (&screen->solid_cache[i].color, &pattern->color)) {
	    return (cairo_xcb_picture_t *) cairo_surface_reference (screen->solid_cache[i].picture);
	}
    }

    picture = _solid_picture (target, &pattern->color);
    if (unlikely (picture->base.status))
	return picture;

    if (screen->solid_cache_size < ARRAY_LENGTH (screen->solid_cache)) {
	i = screen->solid_cache_size++;
    } else {
	i = hars_petruska_f54_1_random () % ARRAY_LENGTH (screen->solid_cache);
	cairo_surface_destroy (screen->solid_cache[i].picture);
    }
    screen->solid_cache[i].picture = cairo_surface_reference (&picture->base);
    screen->solid_cache[i].color = pattern->color;

    return picture;
}

static cairo_xcb_picture_t *
_render_to_picture (cairo_xcb_surface_t *target,
		    const cairo_pattern_t *pattern,
		    const cairo_rectangle_int_t *extents)
{
    cairo_image_surface_t *image;
    cairo_xcb_shm_info_t *shm_info;
    cairo_pattern_union_t copy;
    cairo_status_t status;
    cairo_xcb_picture_t *picture;
    pixman_format_code_t pixman_format;
    xcb_render_pictformat_t xrender_format;

    /* XXX handle extend modes via tiling? */
    /* XXX alpha-only masks? */

    pixman_format = PIXMAN_a8r8g8b8;
    xrender_format = target->screen->connection->standard_formats[CAIRO_FORMAT_ARGB32];

    status = _cairo_xcb_shm_image_create (target->screen->connection,
					  pixman_format,
					  extents->width, extents->height,
					  &image, &shm_info);
    if (unlikely (status))
	return (cairo_xcb_picture_t *) _cairo_surface_create_in_error (status);

    _cairo_pattern_init_static_copy (&copy.base, pattern);
    cairo_matrix_translate (&copy.base.matrix, extents->x, extents->y);
    status = _cairo_surface_paint (&image->base,
				   CAIRO_OPERATOR_SOURCE,
				   &copy.base,
				   NULL);
    if (unlikely (status)) {
	cairo_surface_destroy (&image->base);
	return (cairo_xcb_picture_t *) _cairo_surface_create_in_error (status);
    }

    picture = _picture_from_image (target, xrender_format, image, shm_info);
    cairo_surface_destroy (&image->base);

    if (unlikely (picture->base.status))
	return picture;

    _cairo_xcb_picture_set_component_alpha (picture, pattern->has_component_alpha);
    picture->x = -extents->x;
    picture->y = -extents->y;

    return picture;
}

static xcb_render_fixed_t *
_gradient_to_xcb (const cairo_gradient_pattern_t *gradient,
		  unsigned int *n_stops,
		  char *buf, unsigned int buflen)
{
    xcb_render_fixed_t *stops;
    xcb_render_color_t *colors;
    unsigned int i;

    assert (gradient->n_stops > 0);
    *n_stops = MAX (gradient->n_stops, 2);

    if (*n_stops * (sizeof (xcb_render_fixed_t) + sizeof (xcb_render_color_t)) < buflen)
    {
	stops = (xcb_render_fixed_t *) buf;
    }
    else
    {
	stops =
	    _cairo_malloc_ab (*n_stops,
			      sizeof (xcb_render_fixed_t) + sizeof (xcb_render_color_t));
	if (unlikely (stops == NULL))
	    return NULL;
    }

    colors = (xcb_render_color_t *) (stops + *n_stops);
    for (i = 0; i < gradient->n_stops; i++) {
	stops[i] =
	    _cairo_fixed_16_16_from_double (gradient->stops[i].offset);

	colors[i].red   = gradient->stops[i].color.red_short;
	colors[i].green = gradient->stops[i].color.green_short;
	colors[i].blue  = gradient->stops[i].color.blue_short;
	colors[i].alpha = gradient->stops[i].color.alpha_short;
    }

    /* RENDER does not support gradients with less than 2 stops. If a
     * gradient has only a single stop, duplicate it to make RENDER
     * happy. */
    if (gradient->n_stops == 1) {
	stops[1] = _cairo_fixed_16_16_from_double (gradient->stops[0].offset);

	colors[1].red   = gradient->stops[0].color.red_short;
	colors[1].green = gradient->stops[0].color.green_short;
	colors[1].blue  = gradient->stops[0].color.blue_short;
	colors[1].alpha = gradient->stops[0].color.alpha_short;
    }

    return stops;
}

static cairo_xcb_picture_t *
_cairo_xcb_linear_picture (cairo_xcb_surface_t *target,
			   const cairo_linear_pattern_t *pattern,
			   const cairo_rectangle_int_t *extents)
{
    char buf[CAIRO_STACK_BUFFER_SIZE];
    xcb_render_fixed_t *stops;
    xcb_render_color_t *colors;
    xcb_render_pointfix_t p1, p2;
    cairo_matrix_t matrix;
    cairo_circle_double_t extremes[2];
    cairo_xcb_picture_t *picture;
    cairo_status_t status;
    unsigned int n_stops;

    _cairo_gradient_pattern_fit_to_range (&pattern->base, PIXMAN_MAX_INT >> 1, &matrix, extremes);

    picture = (cairo_xcb_picture_t *)
	_cairo_xcb_screen_lookup_linear_picture (target->screen, pattern);
    if (picture != NULL)
	goto setup_picture;

    stops = _gradient_to_xcb (&pattern->base, &n_stops, buf, sizeof (buf));
    if (unlikely (stops == NULL))
	return (cairo_xcb_picture_t *) _cairo_surface_create_in_error (CAIRO_STATUS_NO_MEMORY);

    picture = _cairo_xcb_picture_create (target->screen,
					 target->screen->connection->standard_formats[CAIRO_FORMAT_ARGB32],
					 PIXMAN_a8r8g8b8,
					 -1, -1);
    if (unlikely (picture->base.status)) {
	if (stops != (xcb_render_fixed_t *) buf)
	    free (stops);
	return picture;
    }
    picture->filter = CAIRO_FILTER_DEFAULT;

    colors = (xcb_render_color_t *) (stops + n_stops);

    p1.x = _cairo_fixed_16_16_from_double (extremes[0].center.x);
    p1.y = _cairo_fixed_16_16_from_double (extremes[0].center.y);
    p2.x = _cairo_fixed_16_16_from_double (extremes[1].center.x);
    p2.y = _cairo_fixed_16_16_from_double (extremes[1].center.y);

    _cairo_xcb_connection_render_create_linear_gradient (target->connection,
							 picture->picture,
							 p1, p2,
							 n_stops,
							 stops, colors);

    if (stops != (xcb_render_fixed_t *) buf)
	free (stops);

    status = _cairo_xcb_screen_store_linear_picture (target->screen,
						     pattern,
						     &picture->base);
    if (unlikely (status)) {
	cairo_surface_destroy (&picture->base);
	return (cairo_xcb_picture_t *) _cairo_surface_create_in_error (status);
    }

setup_picture:
    _cairo_xcb_picture_set_matrix (picture, &matrix,
				   pattern->base.base.filter,
				   extents->x + extents->width/2.,
				   extents->y + extents->height/2.);
    _cairo_xcb_picture_set_filter (picture, pattern->base.base.filter);
    _cairo_xcb_picture_set_extend (picture, pattern->base.base.extend);
    _cairo_xcb_picture_set_component_alpha (picture,
					    pattern->base.base.has_component_alpha);

    return picture;
}

static cairo_xcb_picture_t *
_cairo_xcb_radial_picture (cairo_xcb_surface_t *target,
			   const cairo_radial_pattern_t *pattern,
			   const cairo_rectangle_int_t *extents)
{
    char buf[CAIRO_STACK_BUFFER_SIZE];
    xcb_render_fixed_t *stops;
    xcb_render_color_t *colors;
    xcb_render_pointfix_t p1, p2;
    xcb_render_fixed_t r1, r2;
    cairo_matrix_t matrix;
    cairo_circle_double_t extremes[2];
    cairo_xcb_picture_t *picture;
    cairo_status_t status;
    unsigned int n_stops;

    _cairo_gradient_pattern_fit_to_range (&pattern->base, PIXMAN_MAX_INT >> 1, &matrix, extremes);

    picture = (cairo_xcb_picture_t *)
	_cairo_xcb_screen_lookup_radial_picture (target->screen, pattern);
    if (picture != NULL)
	goto setup_picture;

    stops = _gradient_to_xcb (&pattern->base, &n_stops, buf, sizeof (buf));
    if (unlikely (stops == NULL))
	return (cairo_xcb_picture_t *) _cairo_surface_create_in_error (CAIRO_STATUS_NO_MEMORY);

    picture = _cairo_xcb_picture_create (target->screen,
					 target->screen->connection->standard_formats[CAIRO_FORMAT_ARGB32],
					 PIXMAN_a8r8g8b8,
					 -1, -1);
    if (unlikely (picture->base.status)) {
	if (stops != (xcb_render_fixed_t *) buf)
	    free (stops);
	return picture;
    }
    picture->filter = CAIRO_FILTER_DEFAULT;

    colors = (xcb_render_color_t *) (stops + n_stops);

    p1.x = _cairo_fixed_16_16_from_double (extremes[0].center.x);
    p1.y = _cairo_fixed_16_16_from_double (extremes[0].center.y);
    p2.x = _cairo_fixed_16_16_from_double (extremes[1].center.x);
    p2.y = _cairo_fixed_16_16_from_double (extremes[1].center.y);

    r1 = _cairo_fixed_16_16_from_double (extremes[0].radius);
    r2 = _cairo_fixed_16_16_from_double (extremes[1].radius);

    _cairo_xcb_connection_render_create_radial_gradient (target->connection,
							 picture->picture,
							 p1, p2, r1, r2,
							 n_stops,
							 stops, colors);

    if (stops != (xcb_render_fixed_t *) buf)
	free (stops);

    status = _cairo_xcb_screen_store_radial_picture (target->screen,
						     pattern,
						     &picture->base);
    if (unlikely (status)) {
	cairo_surface_destroy (&picture->base);
	return (cairo_xcb_picture_t *) _cairo_surface_create_in_error (status);
    }

setup_picture:
    _cairo_xcb_picture_set_matrix (picture, &matrix,
				   pattern->base.base.filter,
				   extents->x + extents->width/2.,
				   extents->y + extents->height/2.);
    _cairo_xcb_picture_set_filter (picture, pattern->base.base.filter);
    _cairo_xcb_picture_set_extend (picture, pattern->base.base.extend);
    _cairo_xcb_picture_set_component_alpha (picture,
					    pattern->base.base.has_component_alpha);

    return picture;
}

static cairo_xcb_picture_t *
_copy_to_picture (cairo_xcb_surface_t *source)
{
    cairo_xcb_picture_t *picture;
    uint32_t values[] = { 0, 1 };

    if (source->deferred_clear) {
	cairo_status_t status = _cairo_xcb_surface_clear (source);
	if (unlikely (status))
	    return (cairo_xcb_picture_t *) _cairo_surface_create_in_error (status);
    }

    picture = _cairo_xcb_picture_create (source->screen,
					 source->xrender_format,
					 source->pixman_format,
					 source->width,
					 source->height);
    if (unlikely (picture->base.status))
	return picture;

    _cairo_xcb_connection_render_create_picture (source->connection,
						 picture->picture,
						 source->drawable,
						 source->xrender_format,
						 XCB_RENDER_CP_GRAPHICS_EXPOSURE |
						 XCB_RENDER_CP_SUBWINDOW_MODE,
						 values);

    return picture;
}

static void
_cairo_xcb_surface_setup_surface_picture(cairo_xcb_picture_t *picture,
					 const cairo_surface_pattern_t *pattern,
					 const cairo_rectangle_int_t *extents)
{
    cairo_filter_t filter;

    filter = pattern->base.filter;
    if (filter != CAIRO_FILTER_NEAREST &&
        _cairo_matrix_is_pixel_exact (&pattern->base.matrix))
    {
	filter = CAIRO_FILTER_NEAREST;
    }
    _cairo_xcb_picture_set_filter (picture, filter);

    _cairo_xcb_picture_set_matrix (picture,
				   &pattern->base.matrix, filter,
				   extents->x + extents->width/2.,
				   extents->y + extents->height/2.);


    _cairo_xcb_picture_set_extend (picture, pattern->base.extend);
    _cairo_xcb_picture_set_component_alpha (picture, pattern->base.has_component_alpha);
}

static cairo_xcb_picture_t *
record_to_picture (cairo_surface_t *target,
		   const cairo_surface_pattern_t *pattern,
		   const cairo_rectangle_int_t *extents)
{
    cairo_surface_pattern_t tmp_pattern;
    cairo_xcb_picture_t *picture;
    cairo_status_t status;
    cairo_matrix_t matrix;
    cairo_surface_t *tmp;
    cairo_surface_t *source;
    cairo_rectangle_int_t limit;
    cairo_extend_t extend;

    /* XXX: The following was once more or less copied from cairo-xlibs-ource.c,
     * record_source() and recording_pattern_get_surface(), can we share a
     * single version?
     */

    /* First get the 'real' recording surface and figure out the size for tmp */
    source = _cairo_pattern_get_source (pattern, &limit);
    assert (_cairo_surface_is_recording (source));

    if (! _cairo_matrix_is_identity (&pattern->base.matrix)) {
	double x1, y1, x2, y2;

	matrix = pattern->base.matrix;
	status = cairo_matrix_invert (&matrix);
	assert (status == CAIRO_STATUS_SUCCESS);

	x1 = limit.x;
	y1 = limit.y;
	x2 = limit.x + limit.width;
	y2 = limit.y + limit.height;

	_cairo_matrix_transform_bounding_box (&matrix,
					      &x1, &y1, &x2, &y2, NULL);

	limit.x = floor (x1);
	limit.y = floor (y1);
	limit.width  = ceil (x2) - limit.x;
	limit.height = ceil (y2) - limit.y;
    }
    extend = pattern->base.extend;
    if (_cairo_rectangle_contains_rectangle (&limit, extents))
	extend = CAIRO_EXTEND_NONE;
    if (extend == CAIRO_EXTEND_NONE && ! _cairo_rectangle_intersect (&limit, extents))
	return _cairo_xcb_transparent_picture ((cairo_xcb_surface_t *) target);

    /* Now draw the recording surface to an xcb surface */
    tmp = _cairo_surface_create_scratch (target,
                                         source->content,
                                         limit.width,
                                         limit.height,
                                         CAIRO_COLOR_TRANSPARENT);
    if (tmp->status != CAIRO_STATUS_SUCCESS) {
	return (cairo_xcb_picture_t *) tmp;
    }

    cairo_matrix_init_translate (&matrix, limit.x, limit.y);
    cairo_matrix_multiply (&matrix, &matrix, &pattern->base.matrix);

    status = _cairo_recording_surface_replay_with_clip (source,
							&matrix, tmp,
							NULL);
    if (unlikely (status)) {
	cairo_surface_destroy (tmp);
	return (cairo_xcb_picture_t *) _cairo_surface_create_in_error (status);
    }

    /* Now that we have drawn this to an xcb surface, try again with that */
    _cairo_pattern_init_static_copy (&tmp_pattern.base, &pattern->base);
    tmp_pattern.surface = tmp;
    cairo_matrix_init_translate (&tmp_pattern.base.matrix, -limit.x, -limit.y);

    picture = _copy_to_picture ((cairo_xcb_surface_t *) tmp);
    if (picture->base.status == CAIRO_STATUS_SUCCESS)
	_cairo_xcb_surface_setup_surface_picture (picture, &tmp_pattern, extents);
    cairo_surface_destroy (tmp);
    return picture;
}

static cairo_xcb_picture_t *
_cairo_xcb_surface_picture (cairo_xcb_surface_t *target,
			    const cairo_surface_pattern_t *pattern,
			    const cairo_rectangle_int_t *extents)
{
    cairo_surface_t *source = pattern->surface;
    cairo_xcb_picture_t *picture;

    picture = (cairo_xcb_picture_t *)
	_cairo_surface_has_snapshot (source, &_cairo_xcb_picture_backend);
    if (picture != NULL) {
	if (picture->screen == target->screen) {
	    picture = (cairo_xcb_picture_t *) cairo_surface_reference (&picture->base);
	    _cairo_xcb_surface_setup_surface_picture (picture, pattern, extents);
	    return picture;
	}
	picture = NULL;
    }

    if (source->type == CAIRO_SURFACE_TYPE_XCB)
    {
	if (_cairo_surface_is_xcb(source)) {
	    cairo_xcb_surface_t *xcb = (cairo_xcb_surface_t *) source;
	    if (xcb->screen == target->screen && xcb->fallback == NULL) {
		picture = _copy_to_picture ((cairo_xcb_surface_t *) source);
		if (unlikely (picture->base.status))
		    return picture;
	    }
	} else if (source->backend->type == CAIRO_SURFACE_TYPE_SUBSURFACE) {
	    cairo_surface_subsurface_t *sub = (cairo_surface_subsurface_t *) source;
	    cairo_xcb_surface_t *xcb = (cairo_xcb_surface_t *) sub->target;

	    /* XXX repeat interval with source clipping? */
	    if (FALSE && xcb->screen == target->screen && xcb->fallback == NULL) {
		xcb_rectangle_t rect;

		picture = _copy_to_picture (xcb);
		if (unlikely (picture->base.status))
		    return picture;

		rect.x = sub->extents.x;
		rect.y = sub->extents.y;
		rect.width  = sub->extents.width;
		rect.height = sub->extents.height;

		_cairo_xcb_connection_render_set_picture_clip_rectangles (xcb->connection,
									  picture->picture,
									  0, 0,
									  1, &rect);
		picture->x0 = rect.x;
		picture->y0 = rect.y;
		picture->width  = rect.width;
		picture->height = rect.height;
	    }
	} else if (_cairo_surface_is_snapshot (source)) {
	    cairo_surface_snapshot_t *snap = (cairo_surface_snapshot_t *) source;
	    cairo_xcb_surface_t *xcb = (cairo_xcb_surface_t *) snap->target;

	    if (xcb->screen == target->screen && xcb->fallback == NULL) {
		picture = _copy_to_picture (xcb);
		if (unlikely (picture->base.status))
		    return picture;
	    }
	}
    }
#if CAIRO_HAS_XLIB_XCB_FUNCTIONS
    else if (source->type == CAIRO_SURFACE_TYPE_XLIB)
    {
	if (source->backend->type == CAIRO_SURFACE_TYPE_XLIB) {
	    cairo_xcb_surface_t *xcb = ((cairo_xlib_xcb_surface_t *) source)->xcb;
	    if (xcb->screen == target->screen && xcb->fallback == NULL) {
		picture = _copy_to_picture (xcb);
		if (unlikely (picture->base.status))
		    return picture;
	    }
	} else if (source->backend->type == CAIRO_SURFACE_TYPE_SUBSURFACE) {
	    cairo_surface_subsurface_t *sub = (cairo_surface_subsurface_t *) source;
	    cairo_xcb_surface_t *xcb = ((cairo_xlib_xcb_surface_t *) sub->target)->xcb;

	    if (FALSE && xcb->screen == target->screen && xcb->fallback == NULL) {
		xcb_rectangle_t rect;

		picture = _copy_to_picture (xcb);
		if (unlikely (picture->base.status))
		    return picture;

		rect.x = sub->extents.x;
		rect.y = sub->extents.y;
		rect.width  = sub->extents.width;
		rect.height = sub->extents.height;

		_cairo_xcb_connection_render_set_picture_clip_rectangles (xcb->connection,
									  picture->picture,
									  0, 0,
									  1, &rect);
		picture->x0 = rect.x;
		picture->y0 = rect.y;
		picture->width  = rect.width;
		picture->height = rect.height;
	    }
	} else if (_cairo_surface_is_snapshot (source)) {
	    cairo_surface_snapshot_t *snap = (cairo_surface_snapshot_t *) source;
	    cairo_xcb_surface_t *xcb = ((cairo_xlib_xcb_surface_t *) snap->target)->xcb;

	    if (xcb->screen == target->screen && xcb->fallback == NULL) {
		picture = _copy_to_picture (xcb);
		if (unlikely (picture->base.status))
		    return picture;
	    }
	}
    }
#endif
    else if (source->type == CAIRO_SURFACE_TYPE_RECORDING)
    {
	/* We have to skip the call to attach_snapshot() because we possibly
	 * only drew part of the recording surface.
	 * TODO: When can we safely attach a snapshot?
	 */
	return record_to_picture(&target->base, pattern, extents);
    }

    if (picture == NULL) {
	cairo_image_surface_t *image;
	void *image_extra;
	cairo_status_t status;

	status = _cairo_surface_acquire_source_image (source, &image, &image_extra);
	if (unlikely (status))
	    return (cairo_xcb_picture_t *) _cairo_surface_create_in_error (status);

	if (image->format != CAIRO_FORMAT_INVALID &&
	    image->format < ARRAY_LENGTH (target->screen->connection->standard_formats)) {
	    xcb_render_pictformat_t format;

	    format = target->screen->connection->standard_formats[image->format];

	    picture = _picture_from_image (target, format, image, NULL);
	    _cairo_surface_release_source_image (source, image, image_extra);
	} else {
	    cairo_image_surface_t *conv;
	    xcb_render_pictformat_t render_format;

	    /* XXX XRenderPutImage! */

	    conv = _cairo_image_surface_coerce (image);
	    _cairo_surface_release_source_image (source, image, image_extra);
	    if (unlikely (conv->base.status))
		return (cairo_xcb_picture_t *) conv;

	    render_format = target->screen->connection->standard_formats[conv->format];
	    picture = _picture_from_image (target, render_format, conv, NULL);
	    cairo_surface_destroy (&conv->base);
	}

	if (unlikely (picture->base.status))
	    return picture;
    }

    /* XXX: This causes too many problems and bugs, let's skip it for now. */
#if 0
    _cairo_surface_attach_snapshot (source,
				    &picture->base,
				    NULL);
#endif

    _cairo_xcb_surface_setup_surface_picture (picture, pattern, extents);
    return picture;
}

static cairo_xcb_picture_t *
_cairo_xcb_picture_for_pattern (cairo_xcb_surface_t *target,
				const cairo_pattern_t *pattern,
				const cairo_rectangle_int_t *extents)
{
    if (pattern == NULL)
	return _cairo_xcb_white_picture (target);

    if (! _pattern_is_supported (target->connection->flags, pattern))
	return _render_to_picture (target, pattern, extents);

    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	return _cairo_xcb_solid_picture (target, (cairo_solid_pattern_t *) pattern);

    case CAIRO_PATTERN_TYPE_LINEAR:
	return _cairo_xcb_linear_picture (target,
					  (cairo_linear_pattern_t *) pattern,
					  extents);

    case CAIRO_PATTERN_TYPE_RADIAL:
	return _cairo_xcb_radial_picture (target,
					  (cairo_radial_pattern_t *) pattern,
					  extents);

    case CAIRO_PATTERN_TYPE_SURFACE:
	return _cairo_xcb_surface_picture (target,
					   (cairo_surface_pattern_t *) pattern,
					   extents);
    default:
	ASSERT_NOT_REACHED;
    case CAIRO_PATTERN_TYPE_MESH:
    case CAIRO_PATTERN_TYPE_RASTER_SOURCE:
	return _render_to_picture (target, pattern, extents);
    }
}

COMPILE_TIME_ASSERT (sizeof (xcb_rectangle_t) <= sizeof (cairo_box_t));

static cairo_status_t
_render_fill_boxes (void			*abstract_dst,
		    cairo_operator_t		 op,
		    const cairo_color_t		*color,
		    cairo_boxes_t		*boxes)
{
    cairo_xcb_surface_t *dst = abstract_dst;
    xcb_rectangle_t stack_xrects[CAIRO_STACK_ARRAY_LENGTH (xcb_rectangle_t)];
    xcb_rectangle_t *xrects = stack_xrects;
    xcb_render_color_t render_color;
    int render_op = _render_operator (op);
    struct _cairo_boxes_chunk *chunk;
    int max_count;

    render_color.red   = color->red_short;
    render_color.green = color->green_short;
    render_color.blue  = color->blue_short;
    render_color.alpha = color->alpha_short;

    max_count = 0;
    for (chunk = &boxes->chunks; chunk != NULL; chunk = chunk->next) {
	if (chunk->count > max_count)
	    max_count = chunk->count;
    }
    if (max_count > ARRAY_LENGTH (stack_xrects)) {
	xrects = _cairo_malloc_ab (max_count, sizeof (xcb_rectangle_t));
	if (unlikely (xrects == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    for (chunk = &boxes->chunks; chunk != NULL; chunk = chunk->next) {
	int i, j;

	for (i = j = 0; i < chunk->count; i++) {
	    int x1 = _cairo_fixed_integer_round_down (chunk->base[i].p1.x);
	    int y1 = _cairo_fixed_integer_round_down (chunk->base[i].p1.y);
	    int x2 = _cairo_fixed_integer_round_down (chunk->base[i].p2.x);
	    int y2 = _cairo_fixed_integer_round_down (chunk->base[i].p2.y);

	    if (x2 > x1 && y2 > y1) {
		xrects[j].x = x1;
		xrects[j].y = y1;
		xrects[j].width  = x2 - x1;
		xrects[j].height = y2 - y1;
		j++;
	    }
	}

	if (j) {
	    _cairo_xcb_connection_render_fill_rectangles
		(dst->connection,
		 render_op, dst->picture,
		 render_color, j, xrects);
	}
    }

    if (xrects != stack_xrects)
	free (xrects);

    return CAIRO_STATUS_SUCCESS;
}

/* pixel aligned, non-overlapping boxes */
static cairo_int_status_t
_render_composite_boxes (cairo_xcb_surface_t	*dst,
			 cairo_operator_t	 op,
			 const cairo_pattern_t	*src_pattern,
			 const cairo_pattern_t	*mask_pattern,
			 const cairo_rectangle_int_t *extents,
			 const cairo_boxes_t *boxes)
{
    cairo_xcb_picture_t *src, *mask;
    const struct _cairo_boxes_chunk *chunk;
    xcb_rectangle_t stack_boxes[CAIRO_STACK_ARRAY_LENGTH (xcb_rectangle_t)];
    xcb_rectangle_t *clip_boxes;
    cairo_rectangle_int_t stack_extents;
    cairo_status_t status;
    int num_boxes;
    int render_op;

    render_op = _render_operator (op);

    if (src_pattern == NULL) {
	src_pattern = mask_pattern;
	mask_pattern = NULL;
    }

    /* amalgamate into a single Composite call by setting a clip region */
    clip_boxes = stack_boxes;
    if (boxes->num_boxes > ARRAY_LENGTH (stack_boxes)) {
	clip_boxes = _cairo_malloc_ab (boxes->num_boxes, sizeof (xcb_rectangle_t));
	if (unlikely (clip_boxes == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    src = _cairo_xcb_picture_for_pattern (dst, src_pattern, extents);
    status = src->base.status;
    if (unlikely (status))
	goto cleanup_boxes;

    num_boxes = 0;
    for (chunk = &boxes->chunks; chunk != NULL; chunk = chunk->next) {
	const cairo_box_t *box = chunk->base;
	int i;

	for (i = 0; i < chunk->count; i++) {
	    int x = _cairo_fixed_integer_round_down (box[i].p1.x);
	    int y = _cairo_fixed_integer_round_down (box[i].p1.y);
	    int width  = _cairo_fixed_integer_round_down (box[i].p2.x) - x;
	    int height = _cairo_fixed_integer_round_down (box[i].p2.y) - y;

	    if (width && height) {
		clip_boxes[num_boxes].x = x;
		clip_boxes[num_boxes].y = y;
		clip_boxes[num_boxes].width = width;
		clip_boxes[num_boxes].height = height;
		num_boxes++;
	    }
	}
    }

    if (num_boxes) {
	if (num_boxes > 1) {
	    _cairo_xcb_connection_render_set_picture_clip_rectangles (dst->connection,
								      dst->picture,
								      0, 0,
								      num_boxes,
								      clip_boxes);
	} else {
	    stack_extents.x = clip_boxes[0].x;
	    stack_extents.y = clip_boxes[0].y;
	    stack_extents.width  = clip_boxes[0].width;
	    stack_extents.height = clip_boxes[0].height;
	    extents = &stack_extents;
	}

	if (mask_pattern != NULL) {
	    mask = _cairo_xcb_picture_for_pattern (dst, mask_pattern, extents);
	    status = mask->base.status;
	    if (unlikely (status))
		goto cleanup_clip;

	    _cairo_xcb_connection_render_composite (dst->connection,
						    render_op,
						    src->picture,
						    mask->picture,
						    dst->picture,
						    src->x + extents->x, src->y + extents->y,
						    mask->x + extents->x, mask->y + extents->y,
						    extents->x, extents->y,
						    extents->width, extents->height);

	    cairo_surface_destroy (&mask->base);
	} else {
	    _cairo_xcb_connection_render_composite (dst->connection,
						    render_op,
						    src->picture,
						    XCB_NONE,
						    dst->picture,
						    src->x + extents->x, src->y + extents->y,
						    0, 0,
						    extents->x, extents->y,
						    extents->width, extents->height);
	}

cleanup_clip:

	if (num_boxes > 1)
	    _cairo_xcb_surface_clear_clip_region (dst);
    }

    cairo_surface_destroy (&src->base);

cleanup_boxes:

    if (clip_boxes != stack_boxes)
	free (clip_boxes);

    return status;
}


#define CAIRO_FIXED_16_16_MIN _cairo_fixed_from_int (-32768)
#define CAIRO_FIXED_16_16_MAX _cairo_fixed_from_int (32767)

static cairo_bool_t
_line_exceeds_16_16 (const cairo_line_t *line)
{
    return
	line->p1.x <= CAIRO_FIXED_16_16_MIN ||
	line->p1.x >= CAIRO_FIXED_16_16_MAX ||

	line->p2.x <= CAIRO_FIXED_16_16_MIN ||
	line->p2.x >= CAIRO_FIXED_16_16_MAX ||

	line->p1.y <= CAIRO_FIXED_16_16_MIN ||
	line->p1.y >= CAIRO_FIXED_16_16_MAX ||

	line->p2.y <= CAIRO_FIXED_16_16_MIN ||
	line->p2.y >= CAIRO_FIXED_16_16_MAX;
}

static void
_project_line_x_onto_16_16 (const cairo_line_t *line,
			    cairo_fixed_t top,
			    cairo_fixed_t bottom,
			    xcb_render_linefix_t *out)
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

typedef struct {
    cairo_traps_t traps;
    cairo_antialias_t antialias;
} composite_traps_info_t;

COMPILE_TIME_ASSERT (sizeof (xcb_render_trapezoid_t) <= sizeof (cairo_trapezoid_t));

static cairo_int_status_t
_composite_traps (void *closure,
		  cairo_xcb_surface_t	*dst,
		  cairo_operator_t	 op,
		  const cairo_pattern_t	*pattern,
		  int dst_x, int dst_y,
		  const cairo_rectangle_int_t *extents,
		  cairo_clip_t		*clip)
{
    composite_traps_info_t *info = closure;
    const cairo_traps_t *traps = &info->traps;
    cairo_xcb_picture_t *src;
    cairo_format_t format;
    xcb_render_pictformat_t xrender_format;
    xcb_render_trapezoid_t *xtraps;
    int render_reference_x, render_reference_y;
    cairo_status_t status;
    int i;

    if (dst->deferred_clear) {
	status = _cairo_xcb_surface_clear (dst);
	if (unlikely (status))
		return status;
    }

    src = _cairo_xcb_picture_for_pattern (dst, pattern, extents);
    if (unlikely (src->base.status))
	return src->base.status;

    if (info->antialias == CAIRO_ANTIALIAS_NONE)
	format = CAIRO_FORMAT_A1;
    else
	format = CAIRO_FORMAT_A8;
    xrender_format = dst->screen->connection->standard_formats[format];

    xtraps = (xcb_render_trapezoid_t *) traps->traps;
    for (i = 0; i < traps->num_traps; i++) {
	cairo_trapezoid_t t = traps->traps[i];

	/* top/bottom will be clamped to surface bounds */
	xtraps[i].top = _cairo_fixed_to_16_16 (t.top);
	xtraps[i].top -= dst_y << 16;
	xtraps[i].bottom = _cairo_fixed_to_16_16 (t.bottom);
	xtraps[i].bottom -= dst_y << 16;

	/* However, all the other coordinates will have been left untouched so
	 * as not to introduce numerical error. Recompute them if they
	 * exceed the 16.16 limits.
	 */
	if (unlikely (_line_exceeds_16_16 (&t.left))) {
	    _project_line_x_onto_16_16 (&t.left,
					t.top,
					t.bottom,
					&xtraps[i].left);
	    xtraps[i].left.p1.y = xtraps[i].top;
	    xtraps[i].left.p2.y = xtraps[i].bottom;
	} else {
	    xtraps[i].left.p1.x = _cairo_fixed_to_16_16 (t.left.p1.x);
	    xtraps[i].left.p1.y = _cairo_fixed_to_16_16 (t.left.p1.y);
	    xtraps[i].left.p2.x = _cairo_fixed_to_16_16 (t.left.p2.x);
	    xtraps[i].left.p2.y = _cairo_fixed_to_16_16 (t.left.p2.y);
	}
	xtraps[i].left.p1.x -= dst_x << 16;
	xtraps[i].left.p1.y -= dst_y << 16;
	xtraps[i].left.p2.x -= dst_x << 16;
	xtraps[i].left.p2.y -= dst_y << 16;

	if (unlikely (_line_exceeds_16_16 (&t.right))) {
	    _project_line_x_onto_16_16 (&t.right,
					t.top,
					t.bottom,
					&xtraps[i].right);
	    xtraps[i].right.p1.y = xtraps[i].top;
	    xtraps[i].right.p2.y = xtraps[i].bottom;
	} else {
	    xtraps[i].right.p1.x = _cairo_fixed_to_16_16 (t.right.p1.x);
	    xtraps[i].right.p1.y = _cairo_fixed_to_16_16 (t.right.p1.y);
	    xtraps[i].right.p2.x = _cairo_fixed_to_16_16 (t.right.p2.x);
	    xtraps[i].right.p2.y = _cairo_fixed_to_16_16 (t.right.p2.y);
	}
	xtraps[i].right.p1.x -= dst_x << 16;
	xtraps[i].right.p1.y -= dst_y << 16;
	xtraps[i].right.p2.x -= dst_x << 16;
	xtraps[i].right.p2.y -= dst_y << 16;
    }

    if (xtraps[0].left.p1.y < xtraps[0].left.p2.y) {
	render_reference_x = xtraps[0].left.p1.x >> 16;
	render_reference_y = xtraps[0].left.p1.y >> 16;
    } else {
	render_reference_x = xtraps[0].left.p2.x >> 16;
	render_reference_y = xtraps[0].left.p2.y >> 16;
    }
    render_reference_x += src->x + dst_x;
    render_reference_y += src->y + dst_y;

    _cairo_xcb_surface_set_precision (dst, info->antialias);
    _cairo_xcb_connection_render_trapezoids (dst->connection,
					     _render_operator (op),
					     src->picture,
					     dst->picture,
					     xrender_format,
					     render_reference_x,
					     render_reference_y,
					     traps->num_traps, xtraps);

    cairo_surface_destroy (&src->base);

    return CAIRO_STATUS_SUCCESS;
}

/* low-level composite driver */

static cairo_xcb_surface_t *
get_clip_surface (const cairo_clip_t *clip,
		  cairo_xcb_surface_t *target,
		  int *tx, int *ty)
{
    cairo_surface_t *surface;
    cairo_status_t status;

    surface = _cairo_surface_create_scratch (&target->base,
					    CAIRO_CONTENT_ALPHA,
					    clip->extents.width,
					    clip->extents.height,
					    CAIRO_COLOR_WHITE);
    if (unlikely (surface->status))
	return (cairo_xcb_surface_t *) surface;

    assert (surface->backend == &_cairo_xcb_surface_backend);
    status = _cairo_clip_combine_with_surface (clip, surface,
					       clip->extents.x, clip->extents.y);
    if (unlikely (status)) {
	cairo_surface_destroy (surface);
	surface = _cairo_surface_create_in_error (status);
    }

    *tx = clip->extents.x;
    *ty = clip->extents.y;

    return (cairo_xcb_surface_t *) surface;
}

typedef cairo_int_status_t
(*xcb_draw_func_t) (void				*closure,
		    cairo_xcb_surface_t			*dst,
		    cairo_operator_t			 op,
		    const cairo_pattern_t		*src,
		    int					 dst_x,
		    int					 dst_y,
		    const cairo_rectangle_int_t		*extents,
		    cairo_clip_t			*clip);

static void do_unaligned_row(void (*blt)(void *closure,
					 int16_t x, int16_t y,
					 int16_t w, int16_t h,
					 uint16_t coverage),
			     void *closure,
			     const cairo_box_t *b,
			     int tx, int y, int h,
			     uint16_t coverage)
{
    int x1 = _cairo_fixed_integer_part (b->p1.x) - tx;
    int x2 = _cairo_fixed_integer_part (b->p2.x) - tx;
    if (x2 > x1) {
	if (! _cairo_fixed_is_integer (b->p1.x)) {
	    blt(closure, x1, y, 1, h,
		coverage * (256 - _cairo_fixed_fractional_part (b->p1.x)));
	    x1++;
	}

	if (x2 > x1)
	    blt(closure, x1, y, x2-x1, h, (coverage << 8) - (coverage >> 8));

	if (! _cairo_fixed_is_integer (b->p2.x))
	    blt(closure, x2, y, 1, h,
		coverage * _cairo_fixed_fractional_part (b->p2.x));
    } else
	blt(closure, x1, y, 1, h,
	    coverage * (b->p2.x - b->p1.x));
}

static void do_unaligned_box(void (*blt)(void *closure,
					 int16_t x, int16_t y,
					 int16_t w, int16_t h,
					 uint16_t coverage),
			     void *closure,
			     const cairo_box_t *b, int tx, int ty)
{
    int y1 = _cairo_fixed_integer_part (b->p1.y) - ty;
    int y2 = _cairo_fixed_integer_part (b->p2.y) - ty;
    if (y2 > y1) {
	if (! _cairo_fixed_is_integer (b->p1.y)) {
	    do_unaligned_row(blt, closure, b, tx, y1, 1,
			     256 - _cairo_fixed_fractional_part (b->p1.y));
	    y1++;
	}

	if (y2 > y1)
	    do_unaligned_row(blt, closure, b, tx, y1, y2-y1, 256);

	if (! _cairo_fixed_is_integer (b->p2.y))
	    do_unaligned_row(blt, closure, b, tx, y2, 1,
			     _cairo_fixed_fractional_part (b->p2.y));
    } else
	do_unaligned_row(blt, closure, b, tx, y1, 1,
			 b->p2.y - b->p1.y);
}


static void blt_in(void *closure,
		   int16_t x, int16_t y,
		   int16_t w, int16_t h,
		   uint16_t coverage)
{
    cairo_xcb_surface_t *mask = closure;
    xcb_render_color_t color;
    xcb_rectangle_t rect;

    if (coverage == 0xffff)
	return;

    color.red = color.green = color.blue = 0;
    color.alpha = coverage;

    rect.x = x;
    rect.y = y;
    rect.width  = w;
    rect.height = h;

    _cairo_xcb_connection_render_fill_rectangles (mask->connection,
						  XCB_RENDER_PICT_OP_IN,
						  mask->picture,
						  color, 1, &rect);
}

static cairo_xcb_surface_t *
_create_composite_mask (cairo_clip_t		*clip,
			xcb_draw_func_t		 draw_func,
			xcb_draw_func_t		 mask_func,
			void			*draw_closure,
			cairo_xcb_surface_t	*dst,
			const cairo_rectangle_int_t*extents)
{
    cairo_xcb_surface_t *surface;
    cairo_bool_t need_clip_combine;
    cairo_int_status_t status;

    surface = (cairo_xcb_surface_t *)
	_cairo_xcb_surface_create_similar (dst, CAIRO_CONTENT_ALPHA,
					   extents->width, extents->height);
    if (unlikely (surface->base.status))
	return surface;

    _cairo_xcb_surface_ensure_picture (surface);

    surface->deferred_clear_color = *CAIRO_COLOR_TRANSPARENT;
    surface->deferred_clear = TRUE;
    surface->base.is_clear = TRUE;

    if (mask_func) {
	status = mask_func (draw_closure, surface,
			    CAIRO_OPERATOR_ADD, NULL,
			    extents->x, extents->y,
			    extents, clip);
	if (likely (status != CAIRO_INT_STATUS_UNSUPPORTED))
	    return surface;
    }

    /* Is it worth setting the clip region here? */
    status = draw_func (draw_closure, surface,
			CAIRO_OPERATOR_ADD, NULL,
			extents->x, extents->y,
			extents, NULL);
    if (unlikely (status)) {
	cairo_surface_destroy (&surface->base);
	return (cairo_xcb_surface_t *) _cairo_surface_create_in_error (status);
    }

    if (surface->connection->flags & CAIRO_XCB_RENDER_HAS_FILL_RECTANGLES) {
	int i;

	for (i = 0; i < clip->num_boxes; i++) {
	    cairo_box_t *b = &clip->boxes[i];

	    if (! _cairo_fixed_is_integer (b->p1.x) ||
		! _cairo_fixed_is_integer (b->p1.y) ||
		! _cairo_fixed_is_integer (b->p2.x) ||
		! _cairo_fixed_is_integer (b->p2.y))
	    {
		do_unaligned_box(blt_in, surface, b, extents->x, extents->y);
	    }
	}

	need_clip_combine = clip->path != NULL;
    } else
	need_clip_combine = ! _cairo_clip_is_region (clip);

    if (need_clip_combine) {
	status = _cairo_clip_combine_with_surface (clip, &surface->base,
						   extents->x, extents->y);
	if (unlikely (status)) {
	    cairo_surface_destroy (&surface->base);
	    return (cairo_xcb_surface_t *) _cairo_surface_create_in_error (status);
	}
    }

    return surface;
}

/* Handles compositing with a clip surface when the operator allows
 * us to combine the clip with the mask
 */
static cairo_status_t
_clip_and_composite_with_mask (cairo_clip_t		*clip,
			       cairo_operator_t		 op,
			       const cairo_pattern_t	*pattern,
			       xcb_draw_func_t		 draw_func,
			       xcb_draw_func_t		 mask_func,
			       void			*draw_closure,
			       cairo_xcb_surface_t	*dst,
			       const cairo_rectangle_int_t*extents)
{
    cairo_xcb_surface_t *mask;
    cairo_xcb_picture_t *src;

    mask = _create_composite_mask (clip,
				   draw_func, mask_func, draw_closure,
				   dst, extents);
    if (unlikely (mask->base.status))
	return mask->base.status;

    if (pattern != NULL || dst->base.content != CAIRO_CONTENT_ALPHA) {
	src = _cairo_xcb_picture_for_pattern (dst, pattern, extents);
	if (unlikely (src->base.status)) {
	    cairo_surface_destroy (&mask->base);
	    return src->base.status;
	}

	_cairo_xcb_connection_render_composite (dst->connection,
						_render_operator (op),
						src->picture,
						mask->picture,
						dst->picture,
						extents->x + src->x, extents->y + src->y,
						0, 0,
						extents->x,      extents->y,
						extents->width,  extents->height);

	cairo_surface_destroy (&src->base);
    } else {
	_cairo_xcb_connection_render_composite (dst->connection,
						_render_operator (op),
						mask->picture,
						XCB_NONE,
						dst->picture,
						0, 0,
						0, 0,
						extents->x,      extents->y,
						extents->width,  extents->height);
    }
    cairo_surface_destroy (&mask->base);

    return CAIRO_STATUS_SUCCESS;
}

/* Handles compositing with a clip surface when we have to do the operation
 * in two pieces and combine them together.
 */
static cairo_status_t
_clip_and_composite_combine (cairo_clip_t		*clip,
			     cairo_operator_t		 op,
			     const cairo_pattern_t	*pattern,
			     xcb_draw_func_t		 draw_func,
			     void			*draw_closure,
			     cairo_xcb_surface_t	*dst,
			     const cairo_rectangle_int_t*extents)
{
    cairo_xcb_surface_t *tmp;
    cairo_xcb_surface_t *clip_surface;
    int clip_x = 0, clip_y = 0;
    xcb_render_picture_t clip_picture;
    cairo_status_t status;

    tmp = (cairo_xcb_surface_t *)
	_cairo_xcb_surface_create_similar (dst, dst->base.content,
					   extents->width, extents->height);
    if (unlikely (tmp->base.status))
	return tmp->base.status;

    /* create_similar() could have done a fallback to an image surface */
    assert (tmp->base.backend == &_cairo_xcb_surface_backend);

    _cairo_xcb_surface_ensure_picture (tmp);

    if (pattern == NULL) {
	status = (*draw_func) (draw_closure, tmp,
			       CAIRO_OPERATOR_ADD, NULL,
			       extents->x, extents->y,
			       extents, NULL);
    } else {
	/* Initialize the temporary surface from the destination surface */
	if (! dst->base.is_clear ||
	    (dst->connection->flags & CAIRO_XCB_RENDER_HAS_FILL_RECTANGLES) == 0)
	{
	    /* XCopyArea may actually be quicker here.
	     * A good driver should translate if appropriate.
	     */
	    _cairo_xcb_connection_render_composite (dst->connection,
						    XCB_RENDER_PICT_OP_SRC,
						    dst->picture,
						    XCB_NONE,
						    tmp->picture,
						    extents->x,      extents->y,
						    0, 0,
						    0, 0,
						    extents->width,  extents->height);
	}
	else
	{
	    xcb_render_color_t clear;
	    xcb_rectangle_t xrect;

	    clear.red = clear.green = clear.blue = clear.alpha = 0;

	    xrect.x = xrect.y = 0;
	    xrect.width  = extents->width;
	    xrect.height = extents->height;

	    _cairo_xcb_connection_render_fill_rectangles (dst->connection,
							  XCB_RENDER_PICT_OP_CLEAR,
							  dst->picture,
							  clear, 1, &xrect);
	}

	status = (*draw_func) (draw_closure, tmp, op, pattern,
			       extents->x, extents->y,
			       extents, NULL);
    }
    if (unlikely (status))
	goto CLEANUP_SURFACE;

    clip_surface = get_clip_surface (clip, dst, &clip_x, &clip_y);
    status = clip_surface->base.status;
    if (unlikely (status))
	goto CLEANUP_SURFACE;

    assert (clip_surface->base.backend == &_cairo_xcb_surface_backend);
    clip_picture = clip_surface->picture;
    assert (clip_picture != XCB_NONE);

    if (dst->base.is_clear) {
	_cairo_xcb_connection_render_composite (dst->connection,
						XCB_RENDER_PICT_OP_SRC,
						tmp->picture, clip_picture, dst->picture,
						0, 0,
						0, 0,
						extents->x,      extents->y,
						extents->width,  extents->height);
    } else {
	/* Punch the clip out of the destination */
	_cairo_xcb_connection_render_composite (dst->connection,
						XCB_RENDER_PICT_OP_OUT_REVERSE,
						clip_picture, XCB_NONE, dst->picture,
						extents->x - clip_x,
						extents->y - clip_y,
						0, 0,
						extents->x,     extents->y,
						extents->width, extents->height);

	/* Now add the two results together */
	_cairo_xcb_connection_render_composite (dst->connection,
						XCB_RENDER_PICT_OP_ADD,
						tmp->picture, clip_picture, dst->picture,
						0, 0,
						extents->x - clip_x,
						extents->y - clip_y,
						extents->x,     extents->y,
						extents->width, extents->height);
    }
    cairo_surface_destroy (&clip_surface->base);

 CLEANUP_SURFACE:
    cairo_surface_destroy (&tmp->base);

    return status;
}

/* Handles compositing for %CAIRO_OPERATOR_SOURCE, which is special; it's
 * defined as (src IN mask IN clip) ADD (dst OUT (mask IN clip))
 */
static cairo_status_t
_clip_and_composite_source (cairo_clip_t		*clip,
			    const cairo_pattern_t	*pattern,
			    xcb_draw_func_t		 draw_func,
			    xcb_draw_func_t		 mask_func,
			    void			*draw_closure,
			    cairo_xcb_surface_t		*dst,
			    const cairo_rectangle_int_t	*extents)
{
    cairo_xcb_surface_t *mask;
    cairo_xcb_picture_t *src;

    /* Create a surface that is mask IN clip */
    mask = _create_composite_mask (clip,
				   draw_func, mask_func, draw_closure,
				   dst, extents);
    if (unlikely (mask->base.status))
	return mask->base.status;

    src = _cairo_xcb_picture_for_pattern (dst, pattern, extents);
    if (unlikely (src->base.status)) {
	cairo_surface_destroy (&mask->base);
	return src->base.status;
    }

    if (dst->base.is_clear) {
	_cairo_xcb_connection_render_composite (dst->connection,
						XCB_RENDER_PICT_OP_SRC,
						src->picture,
						mask->picture,
						dst->picture,
						extents->x + src->x, extents->y + src->y,
						0, 0,
						extents->x,      extents->y,
						extents->width,  extents->height);
    } else {
	/* Compute dest' = dest OUT (mask IN clip) */
	_cairo_xcb_connection_render_composite (dst->connection,
						XCB_RENDER_PICT_OP_OUT_REVERSE,
						mask->picture,
						XCB_NONE,
						dst->picture,
						0, 0, 0, 0,
						extents->x,     extents->y,
						extents->width, extents->height);

	/* Now compute (src IN (mask IN clip)) ADD dest' */
	_cairo_xcb_connection_render_composite (dst->connection,
						XCB_RENDER_PICT_OP_ADD,
						src->picture,
						mask->picture,
						dst->picture,
						extents->x + src->x, extents->y + src->y,
						0, 0,
						extents->x,     extents->y,
						extents->width, extents->height);
    }

    cairo_surface_destroy (&src->base);
    cairo_surface_destroy (&mask->base);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t
can_reduce_alpha_op (cairo_operator_t op)
{
    int iop = op;
    switch (iop) {
    case CAIRO_OPERATOR_OVER:
    case CAIRO_OPERATOR_SOURCE:
    case CAIRO_OPERATOR_ADD:
	return TRUE;
    default:
	return FALSE;
    }
}

static cairo_bool_t
reduce_alpha_op (cairo_surface_t *dst,
		 cairo_operator_t op,
		 const cairo_pattern_t *pattern)
{
    return dst->is_clear &&
	   dst->content == CAIRO_CONTENT_ALPHA &&
	   _cairo_pattern_is_opaque_solid (pattern) &&
	   can_reduce_alpha_op (op);
}

static cairo_status_t
_cairo_xcb_surface_fixup_unbounded (cairo_xcb_surface_t *dst,
				    const cairo_composite_rectangles_t *rects)
{
    xcb_rectangle_t xrects[4];
    int n;

    if (rects->bounded.width  == rects->unbounded.width &&
	rects->bounded.height == rects->unbounded.height)
    {
	return CAIRO_STATUS_SUCCESS;
    }

    n = 0;
    if (rects->bounded.width == 0 || rects->bounded.height == 0) {
	xrects[n].x = rects->unbounded.x;
	xrects[n].width = rects->unbounded.width;
	xrects[n].y = rects->unbounded.y;
	xrects[n].height = rects->unbounded.height;
	n++;
    } else {
	/* top */
	if (rects->bounded.y != rects->unbounded.y) {
	    xrects[n].x = rects->unbounded.x;
	    xrects[n].width = rects->unbounded.width;
	    xrects[n].y = rects->unbounded.y;
	    xrects[n].height = rects->bounded.y - rects->unbounded.y;
	    n++;
	}
	/* left */
	if (rects->bounded.x != rects->unbounded.x) {
	    xrects[n].x = rects->unbounded.x;
	    xrects[n].width = rects->bounded.x - rects->unbounded.x;
	    xrects[n].y = rects->bounded.y;
	    xrects[n].height = rects->bounded.height;
	    n++;
	}
	/* right */
	if (rects->bounded.x + rects->bounded.width != rects->unbounded.x + rects->unbounded.width) {
	    xrects[n].x = rects->bounded.x + rects->bounded.width;
	    xrects[n].width = rects->unbounded.x + rects->unbounded.width - xrects[n].x;
	    xrects[n].y = rects->bounded.y;
	    xrects[n].height = rects->bounded.height;
	    n++;
	}
	/* bottom */
	if (rects->bounded.y + rects->bounded.height != rects->unbounded.y + rects->unbounded.height) {
	    xrects[n].x = rects->unbounded.x;
	    xrects[n].width = rects->unbounded.width;
	    xrects[n].y = rects->bounded.y + rects->bounded.height;
	    xrects[n].height = rects->unbounded.y + rects->unbounded.height - xrects[n].y;
	    n++;
	}
    }

    if (dst->connection->flags & CAIRO_XCB_RENDER_HAS_FILL_RECTANGLES) {
	xcb_render_color_t color;

	color.red   = 0;
	color.green = 0;
	color.blue  = 0;
	color.alpha = 0;

	_cairo_xcb_connection_render_fill_rectangles (dst->connection,
						      XCB_RENDER_PICT_OP_CLEAR,
						      dst->picture,
						      color, n, xrects);
    } else {
	int i;
	cairo_xcb_picture_t *src;

	src = _cairo_xcb_transparent_picture (dst);
	if (unlikely (src->base.status))
	    return src->base.status;

	for (i = 0; i < n; i++) {
	    _cairo_xcb_connection_render_composite (dst->connection,
						    XCB_RENDER_PICT_OP_CLEAR,
						    src->picture, XCB_NONE, dst->picture,
						    0, 0,
						    0, 0,
						    xrects[i].x, xrects[i].y,
						    xrects[i].width, xrects[i].height);
	}
	cairo_surface_destroy (&src->base);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xcb_surface_fixup_unbounded_with_mask (cairo_xcb_surface_t *dst,
					      const cairo_composite_rectangles_t *rects,
					      cairo_clip_t *clip)
{
    cairo_xcb_surface_t *mask;
    int mask_x = 0, mask_y = 0;

    mask = get_clip_surface (clip, dst, &mask_x, &mask_y);
    if (unlikely (mask->base.status))
	return mask->base.status;

    /* top */
    if (rects->bounded.y != rects->unbounded.y) {
	int x = rects->unbounded.x;
	int y = rects->unbounded.y;
	int width = rects->unbounded.width;
	int height = rects->bounded.y - y;

	_cairo_xcb_connection_render_composite (dst->connection,
						XCB_RENDER_PICT_OP_OUT_REVERSE,
						mask->picture, XCB_NONE, dst->picture,
						x - mask_x, y - mask_y,
						0, 0,
						x, y,
						width, height);
    }

    /* left */
    if (rects->bounded.x != rects->unbounded.x) {
	int x = rects->unbounded.x;
	int y = rects->bounded.y;
	int width = rects->bounded.x - x;
	int height = rects->bounded.height;

	_cairo_xcb_connection_render_composite (dst->connection,
						XCB_RENDER_PICT_OP_OUT_REVERSE,
						mask->picture, XCB_NONE, dst->picture,
						x - mask_x, y - mask_y,
						0, 0,
						x, y,
						width, height);
    }

    /* right */
    if (rects->bounded.x + rects->bounded.width != rects->unbounded.x + rects->unbounded.width) {
	int x = rects->bounded.x + rects->bounded.width;
	int y = rects->bounded.y;
	int width = rects->unbounded.x + rects->unbounded.width - x;
	int height = rects->bounded.height;

	_cairo_xcb_connection_render_composite (dst->connection,
						XCB_RENDER_PICT_OP_OUT_REVERSE,
						mask->picture, XCB_NONE, dst->picture,
						x - mask_x, y - mask_y,
						0, 0,
						x, y,
						width, height);
    }

    /* bottom */
    if (rects->bounded.y + rects->bounded.height != rects->unbounded.y + rects->unbounded.height) {
	int x = rects->unbounded.x;
	int y = rects->bounded.y + rects->bounded.height;
	int width = rects->unbounded.width;
	int height = rects->unbounded.y + rects->unbounded.height - y;

	_cairo_xcb_connection_render_composite (dst->connection,
						XCB_RENDER_PICT_OP_OUT_REVERSE,
						mask->picture, XCB_NONE, dst->picture,
						x - mask_x, y - mask_y,
						0, 0,
						x, y,
						width, height);
    }

    cairo_surface_destroy (&mask->base);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xcb_surface_fixup_unbounded_boxes (cairo_xcb_surface_t *dst,
					  const cairo_composite_rectangles_t *extents,
					  cairo_clip_t *clip,
					  cairo_boxes_t *boxes)
{
    cairo_boxes_t clear;
    cairo_box_t box;
    cairo_status_t status;
    struct _cairo_boxes_chunk *chunk;
    int i;

    if (boxes->num_boxes <= 1 && clip == NULL)
	return _cairo_xcb_surface_fixup_unbounded (dst, extents);

    _cairo_boxes_init (&clear);

    box.p1.x = _cairo_fixed_from_int (extents->unbounded.x + extents->unbounded.width);
    box.p1.y = _cairo_fixed_from_int (extents->unbounded.y);
    box.p2.x = _cairo_fixed_from_int (extents->unbounded.x);
    box.p2.y = _cairo_fixed_from_int (extents->unbounded.y + extents->unbounded.height);

    if (clip == NULL) {
	cairo_boxes_t tmp;

	_cairo_boxes_init (&tmp);

	status = _cairo_boxes_add (&tmp, CAIRO_ANTIALIAS_DEFAULT, &box);
	assert (status == CAIRO_STATUS_SUCCESS);

	tmp.chunks.next = &boxes->chunks;
	tmp.num_boxes += boxes->num_boxes;

	status = _cairo_bentley_ottmann_tessellate_boxes (&tmp,
							  CAIRO_FILL_RULE_WINDING,
							  &clear);

	tmp.chunks.next = NULL;
    } else {
	_cairo_boxes_init_with_clip (&clear, clip);

	status = _cairo_boxes_add (&clear, CAIRO_ANTIALIAS_DEFAULT, &box);
	assert (status == CAIRO_STATUS_SUCCESS);

	for (chunk = &boxes->chunks; chunk != NULL; chunk = chunk->next) {
	    for (i = 0; i < chunk->count; i++) {
		status = _cairo_boxes_add (&clear,
					   CAIRO_ANTIALIAS_DEFAULT,
					   &chunk->base[i]);
		if (unlikely (status)) {
		    _cairo_boxes_fini (&clear);
		    return status;
		}
	    }
	}

	status = _cairo_bentley_ottmann_tessellate_boxes (&clear,
							  CAIRO_FILL_RULE_WINDING,
							  &clear);
    }

    if (likely (status == CAIRO_STATUS_SUCCESS)) {
	if (dst->connection->flags & CAIRO_XCB_RENDER_HAS_FILL_RECTANGLES)
	    status = _render_fill_boxes (dst,
					 CAIRO_OPERATOR_CLEAR,
					 CAIRO_COLOR_TRANSPARENT,
					 &clear);
	else
	    status = _cairo_xcb_surface_core_fill_boxes (dst,
							 CAIRO_COLOR_TRANSPARENT,
							 &clear);
    }

    _cairo_boxes_fini (&clear);

    return status;
}

cairo_status_t
_cairo_xcb_surface_clear (cairo_xcb_surface_t *dst)
{
    xcb_gcontext_t gc;
    xcb_rectangle_t rect;
    cairo_status_t status;

    status = _cairo_xcb_connection_acquire (dst->connection);
    if (unlikely (status))
	return status;

    rect.x = rect.y = 0;
    rect.width  = dst->width;
    rect.height = dst->height;

    if (dst->connection->flags & CAIRO_XCB_RENDER_HAS_FILL_RECTANGLES) {
	xcb_render_color_t color;
	uint8_t op;

	color.red   = dst->deferred_clear_color.red_short;
	color.green = dst->deferred_clear_color.green_short;
	color.blue  = dst->deferred_clear_color.blue_short;
	color.alpha = dst->deferred_clear_color.alpha_short;

	if (color.alpha == 0)
	    op = XCB_RENDER_PICT_OP_CLEAR;
	else
	    op = XCB_RENDER_PICT_OP_SRC;

	_cairo_xcb_surface_ensure_picture (dst);
	_cairo_xcb_connection_render_fill_rectangles (dst->connection,
						      op, dst->picture, color,
						      1, &rect);
    } else {
	gc = _cairo_xcb_screen_get_gc (dst->screen, dst->drawable, dst->depth);

	/* XXX color */
	_cairo_xcb_connection_poly_fill_rectangle (dst->connection,
						   dst->drawable, gc,
						   1, &rect);

	_cairo_xcb_screen_put_gc (dst->screen, dst->depth, gc);
    }

    _cairo_xcb_connection_release (dst->connection);

    dst->deferred_clear = FALSE;
    return CAIRO_STATUS_SUCCESS;
}

enum {
    NEED_CLIP_REGION = 0x1,
    NEED_CLIP_SURFACE = 0x2,
    FORCE_CLIP_REGION = 0x4,
};

static cairo_bool_t
need_bounded_clip (cairo_composite_rectangles_t *extents)
{
    unsigned int flags = NEED_CLIP_REGION;
    if (! _cairo_clip_is_region (extents->clip))
	flags |= NEED_CLIP_SURFACE;
    return flags;
}

static cairo_bool_t
need_unbounded_clip (cairo_composite_rectangles_t *extents)
{
    unsigned int flags = 0;
    if (! extents->is_bounded) {
	flags |= NEED_CLIP_REGION;
	if (! _cairo_clip_is_region (extents->clip))
	    flags |= NEED_CLIP_SURFACE;
    }
    if (extents->clip->path != NULL)
	flags |= NEED_CLIP_SURFACE;
    return flags;
}

static cairo_status_t
_clip_and_composite (cairo_xcb_surface_t	*dst,
		     cairo_operator_t		 op,
		     const cairo_pattern_t	*src,
		     xcb_draw_func_t		 draw_func,
		     xcb_draw_func_t		 mask_func,
		     void			*draw_closure,
		     cairo_composite_rectangles_t*extents,
		     unsigned int need_clip)
{
    cairo_region_t *clip_region = NULL;
    cairo_status_t status;

    status = _cairo_xcb_connection_acquire (dst->connection);
    if (unlikely (status))
	return status;

    if (dst->deferred_clear) {
	status = _cairo_xcb_surface_clear (dst);
	if (unlikely (status)) {
	    _cairo_xcb_connection_release (dst->connection);
	    return status;
	}
    }

    _cairo_xcb_surface_ensure_picture (dst);

    if (need_clip & NEED_CLIP_REGION) {
	clip_region = _cairo_clip_get_region (extents->clip);
	if ((need_clip & FORCE_CLIP_REGION) == 0 && clip_region != NULL &&
	    cairo_region_contains_rectangle (clip_region,
					     &extents->unbounded) == CAIRO_REGION_OVERLAP_IN)
	    clip_region = NULL;
	if (clip_region != NULL) {
	    status = _cairo_xcb_surface_set_clip_region (dst, clip_region);
	    if (unlikely (status)) {
		_cairo_xcb_connection_release (dst->connection);
		return status;
	    }
	}
    }

    if (reduce_alpha_op (&dst->base, op, src)) {
	op = CAIRO_OPERATOR_ADD;
	src = NULL;
    }

    if (extents->bounded.width != 0 && extents->bounded.height != 0) {
	if (op == CAIRO_OPERATOR_SOURCE) {
	    status = _clip_and_composite_source (extents->clip, src,
						 draw_func, mask_func, draw_closure,
						 dst, &extents->bounded);
	} else {
	    if (op == CAIRO_OPERATOR_CLEAR) {
		op = CAIRO_OPERATOR_DEST_OUT;
		src = NULL;
	    }

	    if (need_clip & NEED_CLIP_SURFACE) {
		if (extents->is_bounded) {
		    status = _clip_and_composite_with_mask (extents->clip, op, src,
							    draw_func,
							    mask_func,
							    draw_closure,
							    dst, &extents->bounded);
		} else {
		    status = _clip_and_composite_combine (extents->clip, op, src,
							  draw_func, draw_closure,
							  dst, &extents->bounded);
		}
	    } else {
		status = draw_func (draw_closure,
				    dst, op, src,
				    0, 0,
				    &extents->bounded,
				    extents->clip);
	    }
	}
    }

    if (status == CAIRO_STATUS_SUCCESS && ! extents->is_bounded) {
	if (need_clip & NEED_CLIP_SURFACE)
	    status = _cairo_xcb_surface_fixup_unbounded_with_mask (dst, extents, extents->clip);
	else
	    status = _cairo_xcb_surface_fixup_unbounded (dst, extents);
    }

    if (clip_region)
	_cairo_xcb_surface_clear_clip_region (dst);

    _cairo_xcb_connection_release (dst->connection);

    return status;
}

static cairo_status_t
_core_boxes (cairo_xcb_surface_t *dst,
	     cairo_operator_t op,
	     const cairo_pattern_t *src,
	     cairo_boxes_t *boxes,
	     const cairo_composite_rectangles_t *extents)
{
    if (! boxes->is_pixel_aligned)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (! _cairo_clip_is_region (extents->clip))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (op == CAIRO_OPERATOR_CLEAR)
	return _cairo_xcb_surface_core_fill_boxes (dst, CAIRO_COLOR_TRANSPARENT, boxes);

    if (op == CAIRO_OPERATOR_OVER) {
	if (dst->base.is_clear || _cairo_pattern_is_opaque (src, &extents->bounded))
	    op = CAIRO_OPERATOR_SOURCE;
    }
    if (op != CAIRO_OPERATOR_SOURCE)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (src->type == CAIRO_PATTERN_TYPE_SOLID) {
	return _cairo_xcb_surface_core_fill_boxes (dst,
						   &((cairo_solid_pattern_t *) src)->color,
						   boxes);
    }

    return _cairo_xcb_surface_core_copy_boxes (dst, src, &extents->bounded, boxes);
}

static cairo_status_t
_composite_boxes (cairo_xcb_surface_t *dst,
		  cairo_operator_t op,
		  const cairo_pattern_t *src,
		  cairo_boxes_t *boxes,
		  const cairo_composite_rectangles_t *extents)
{
    cairo_clip_t *clip = extents->clip;
    cairo_bool_t need_clip_mask = ! _cairo_clip_is_region (clip);
    cairo_status_t status;

    /* If the boxes are not pixel-aligned, we will need to compute a real mask */
    if (! boxes->is_pixel_aligned)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (need_clip_mask &&
	(! extents->is_bounded || op == CAIRO_OPERATOR_SOURCE))
    {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    status = _cairo_xcb_connection_acquire (dst->connection);
    if (unlikely (status))
	return status;

    _cairo_xcb_surface_ensure_picture (dst);
    if (dst->connection->flags & CAIRO_XCB_RENDER_HAS_FILL_RECTANGLES && ! need_clip_mask &&
	(op == CAIRO_OPERATOR_CLEAR || src->type == CAIRO_PATTERN_TYPE_SOLID))
    {
	const cairo_color_t *color;

	if (op == CAIRO_OPERATOR_CLEAR)
	    color = CAIRO_COLOR_TRANSPARENT;
	else
	    color = &((cairo_solid_pattern_t *) src)->color;

	status = _render_fill_boxes (dst, op, color, boxes);
    }
    else
    {
	cairo_surface_pattern_t mask;

	if (need_clip_mask) {
	    cairo_xcb_surface_t *clip_surface;
	    int clip_x = 0, clip_y = 0;

	    clip_surface = get_clip_surface (extents->clip, dst,
					     &clip_x, &clip_y);
	    if (unlikely (clip_surface->base.status))
		return clip_surface->base.status;

	    _cairo_pattern_init_for_surface (&mask, &clip_surface->base);
	    mask.base.filter = CAIRO_FILTER_NEAREST;
	    cairo_matrix_init_translate (&mask.base.matrix,
					 -clip_x,
					 -clip_y);
	    cairo_surface_destroy (&clip_surface->base);

	    if (op == CAIRO_OPERATOR_CLEAR) {
		src = NULL;
		op = CAIRO_OPERATOR_DEST_OUT;
	    }
	}

	status = _render_composite_boxes (dst, op, src,
					  need_clip_mask ? &mask.base : NULL,
					  &extents->bounded, boxes);

	if (need_clip_mask)
	    _cairo_pattern_fini (&mask.base);
    }

    if (status == CAIRO_STATUS_SUCCESS && ! extents->is_bounded) {
	status =
	    _cairo_xcb_surface_fixup_unbounded_boxes (dst, extents,
						      clip, boxes);
    }

    _cairo_xcb_connection_release (dst->connection);

    return status;
}

static cairo_bool_t
cairo_boxes_for_each_box (cairo_boxes_t *boxes,
			  cairo_bool_t (*func) (cairo_box_t *box,
						void *data),
			  void *data)
{
    struct _cairo_boxes_chunk *chunk;
    int i;

    for (chunk = &boxes->chunks; chunk != NULL; chunk = chunk->next) {
	for (i = 0; i < chunk->count; i++)
	    if (! func (&chunk->base[i], data))
		return FALSE;
    }

    return TRUE;
}

struct _image_contains_box {
    int width, height;
    int tx, ty;
};

static cairo_bool_t image_contains_box (cairo_box_t *box, void *closure)
{
    struct _image_contains_box *data = closure;

    /* The box is pixel-aligned so the truncation is safe. */
    return
	_cairo_fixed_integer_part (box->p1.x) + data->tx >= 0 &&
	_cairo_fixed_integer_part (box->p1.y) + data->ty >= 0 &&
	_cairo_fixed_integer_part (box->p2.x) + data->tx <= data->width &&
	_cairo_fixed_integer_part (box->p2.y) + data->ty <= data->height;
}

struct _image_upload_box {
    cairo_xcb_surface_t *surface;
    cairo_image_surface_t *image;
    xcb_gcontext_t gc;
    int tx, ty;
};

static cairo_bool_t image_upload_box (cairo_box_t *box, void *closure)
{
    const struct _image_upload_box *iub = closure;
    /* The box is pixel-aligned so the truncation is safe. */
    int x = _cairo_fixed_integer_part (box->p1.x);
    int y = _cairo_fixed_integer_part (box->p1.y);
    int width  = _cairo_fixed_integer_part (box->p2.x - box->p1.x);
    int height = _cairo_fixed_integer_part (box->p2.y - box->p1.y);
    int bpp = PIXMAN_FORMAT_BPP (iub->image->pixman_format);
    int len = CAIRO_STRIDE_FOR_WIDTH_BPP (width, bpp);
    if (len == iub->image->stride) {
	_cairo_xcb_connection_put_image (iub->surface->connection,
					 iub->surface->drawable,
					 iub->gc,
					 width, height,
					 x, y,
					 iub->image->depth,
					 iub->image->stride,
					 iub->image->data +
					 (y + iub->ty) * iub->image->stride +
					 (x + iub->tx) * bpp/8);
    } else {
	_cairo_xcb_connection_put_subimage (iub->surface->connection,
					    iub->surface->drawable,
					    iub->gc,
					    x + iub->tx,
					    y + iub->ty,
					    width, height,
					    bpp / 8,
					    iub->image->stride,
					    x, y,
					    iub->image->depth,
					    iub->image->data);
    }

    return TRUE;
}

static cairo_status_t
_upload_image_inplace (cairo_xcb_surface_t *surface,
		       const cairo_pattern_t *source,
		       cairo_boxes_t *boxes)
{
    const cairo_surface_pattern_t *pattern;
    struct _image_contains_box icb;
    struct _image_upload_box iub;
    cairo_image_surface_t *image;
    cairo_status_t status;
    int tx, ty;

    if (! boxes->is_pixel_aligned)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (source->type != CAIRO_PATTERN_TYPE_SURFACE)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    pattern = (const cairo_surface_pattern_t *) source;
    if (! _cairo_surface_is_image (pattern->surface))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* Have we already upload this image to a pixmap? */
    {
	cairo_xcb_picture_t *snapshot;

	snapshot = (cairo_xcb_picture_t *)
	    _cairo_surface_has_snapshot (pattern->surface, &_cairo_xcb_picture_backend);
	if (snapshot != NULL) {
	    if (snapshot->screen == surface->screen)
		return CAIRO_INT_STATUS_UNSUPPORTED;
	}
    }

    image = (cairo_image_surface_t *) pattern->surface;
    if (image->format == CAIRO_FORMAT_INVALID)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (image->depth != surface->depth)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (! _cairo_matrix_is_integer_translation (&source->matrix, &tx, &ty))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* Check that the data is entirely within the image */
    icb.width = image->width;
    icb.height = image->height;
    icb.tx = tx;
    icb.ty = ty;
    if (! cairo_boxes_for_each_box (boxes, image_contains_box, &icb))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (surface->deferred_clear) {
	status = _cairo_xcb_surface_clear (surface);
	if (unlikely (status))
	    return status;
    }

    status = _cairo_xcb_connection_acquire (surface->connection);
    if (unlikely (status))
	return status;

    iub.surface = surface;
    iub.image = image;
    iub.gc = _cairo_xcb_screen_get_gc (surface->screen,
				       surface->drawable,
				       image->depth);
    iub.tx = tx;
    iub.ty = ty;
    cairo_boxes_for_each_box (boxes, image_upload_box, &iub);

    _cairo_xcb_screen_put_gc (surface->screen, image->depth, iub.gc);
    _cairo_xcb_connection_release (surface->connection);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
trim_extents_to_traps (cairo_composite_rectangles_t *extents,
		       cairo_traps_t *traps)
{
    cairo_box_t box;

    /* X trims the affected area to the extents of the trapezoids, so
     * we need to compensate when fixing up the unbounded area.
    */
    _cairo_traps_extents (traps, &box);
    return _cairo_composite_rectangles_intersect_mask_extents (extents, &box);
}

static cairo_bool_t
_mono_edge_is_vertical (const cairo_line_t *line)
{
    return _cairo_fixed_integer_round_down (line->p1.x) == _cairo_fixed_integer_round_down (line->p2.x);
}

static cairo_bool_t
_traps_are_pixel_aligned (cairo_traps_t *traps,
			  cairo_antialias_t antialias)
{
    int i;

    if (antialias == CAIRO_ANTIALIAS_NONE) {
	for (i = 0; i < traps->num_traps; i++) {
	    if (! _mono_edge_is_vertical (&traps->traps[i].left)   ||
		! _mono_edge_is_vertical (&traps->traps[i].right))
	    {
		traps->maybe_region = FALSE;
		return FALSE;
	    }
	}
    } else {
	for (i = 0; i < traps->num_traps; i++) {
	    if (traps->traps[i].left.p1.x != traps->traps[i].left.p2.x   ||
		traps->traps[i].right.p1.x != traps->traps[i].right.p2.x ||
		! _cairo_fixed_is_integer (traps->traps[i].top)          ||
		! _cairo_fixed_is_integer (traps->traps[i].bottom)       ||
		! _cairo_fixed_is_integer (traps->traps[i].left.p1.x)    ||
		! _cairo_fixed_is_integer (traps->traps[i].right.p1.x))
	    {
		traps->maybe_region = FALSE;
		return FALSE;
	    }
	}
    }

    return TRUE;
}

static void
_boxes_for_traps (cairo_boxes_t *boxes,
		  cairo_traps_t *traps,
		  cairo_antialias_t antialias)
{
    int i, j;

    _cairo_boxes_init (boxes);

    boxes->chunks.base  = (cairo_box_t *) traps->traps;
    boxes->chunks.size  = traps->num_traps;

    if (antialias != CAIRO_ANTIALIAS_NONE) {
	for (i = j = 0; i < traps->num_traps; i++) {
	    /* Note the traps and boxes alias so we need to take the local copies first. */
	    cairo_fixed_t x1 = traps->traps[i].left.p1.x;
	    cairo_fixed_t x2 = traps->traps[i].right.p1.x;
	    cairo_fixed_t y1 = traps->traps[i].top;
	    cairo_fixed_t y2 = traps->traps[i].bottom;

	    if (x1 == x2 || y1 == y2)
		    continue;

	    boxes->chunks.base[j].p1.x = x1;
	    boxes->chunks.base[j].p1.y = y1;
	    boxes->chunks.base[j].p2.x = x2;
	    boxes->chunks.base[j].p2.y = y2;
	    j++;

	    if (boxes->is_pixel_aligned) {
		boxes->is_pixel_aligned =
		    _cairo_fixed_is_integer (x1) && _cairo_fixed_is_integer (y1) &&
		    _cairo_fixed_is_integer (x2) && _cairo_fixed_is_integer (y2);
	    }
	}
    } else {
	boxes->is_pixel_aligned = TRUE;

	for (i = j = 0; i < traps->num_traps; i++) {
	    /* Note the traps and boxes alias so we need to take the local copies first. */
	    cairo_fixed_t x1 = traps->traps[i].left.p1.x;
	    cairo_fixed_t x2 = traps->traps[i].right.p1.x;
	    cairo_fixed_t y1 = traps->traps[i].top;
	    cairo_fixed_t y2 = traps->traps[i].bottom;

	    /* round down here to match Pixman's behavior when using traps. */
	    boxes->chunks.base[j].p1.x = _cairo_fixed_round_down (x1);
	    boxes->chunks.base[j].p1.y = _cairo_fixed_round_down (y1);
	    boxes->chunks.base[j].p2.x = _cairo_fixed_round_down (x2);
	    boxes->chunks.base[j].p2.y = _cairo_fixed_round_down (y2);

	    j += (boxes->chunks.base[j].p1.x != boxes->chunks.base[j].p2.x &&
		  boxes->chunks.base[j].p1.y != boxes->chunks.base[j].p2.y);
	}
    }

    boxes->num_boxes    = j;
    boxes->chunks.count = j;
}

static cairo_status_t
_composite_polygon (cairo_xcb_surface_t *dst,
		    cairo_operator_t op,
		    const cairo_pattern_t *source,
		    cairo_polygon_t *polygon,
		    cairo_antialias_t antialias,
		    cairo_fill_rule_t fill_rule,
		    cairo_composite_rectangles_t *extents)
{
    composite_traps_info_t traps;
    cairo_bool_t clip_surface = ! _cairo_clip_is_region (extents->clip);
    cairo_region_t *clip_region = _cairo_clip_get_region (extents->clip);
    cairo_status_t status;

    if (polygon->num_edges == 0) {
	status = CAIRO_STATUS_SUCCESS;

	if (! extents->is_bounded) {
	    if (cairo_region_contains_rectangle (clip_region, &extents->unbounded) == CAIRO_REGION_OVERLAP_IN)
		clip_region = NULL;

	    if (clip_surface == FALSE) {
		if (clip_region != NULL) {
		    status = _cairo_xcb_surface_set_clip_region (dst, clip_region);
		    if (unlikely (status))
			return status;
		}

		status = _cairo_xcb_surface_fixup_unbounded (dst, extents);

		if (clip_region != NULL)
		    _cairo_xcb_surface_clear_clip_region (dst);
	    } else {
		status = _cairo_xcb_surface_fixup_unbounded_with_mask (dst,
								       extents,
								       extents->clip);
	    }
	}

	return status;
    }

    if (extents->clip->path != NULL && extents->is_bounded) {
	cairo_polygon_t clipper;
	cairo_fill_rule_t clipper_fill_rule;
	cairo_antialias_t clipper_antialias;

	status = _cairo_clip_get_polygon (extents->clip,
					  &clipper,
					  &clipper_fill_rule,
					  &clipper_antialias);
	if (likely (status == CAIRO_STATUS_SUCCESS)) {
	    if (clipper_antialias == antialias) {
		status = _cairo_polygon_intersect (polygon, fill_rule,
						   &clipper, clipper_fill_rule);
		if (likely (status == CAIRO_STATUS_SUCCESS)) {
		    cairo_clip_t * clip = _cairo_clip_copy_region (extents->clip);
		    _cairo_clip_destroy (extents->clip);
		    extents->clip = clip;

		    fill_rule = CAIRO_FILL_RULE_WINDING;
		}
		_cairo_polygon_fini (&clipper);
	    }
	}
    }

    _cairo_traps_init (&traps.traps);

    status = _cairo_bentley_ottmann_tessellate_polygon (&traps.traps, polygon, fill_rule);
    if (unlikely (status))
	goto CLEANUP_TRAPS;

    if (traps.traps.has_intersections) {
	if (traps.traps.is_rectangular)
	    status = _cairo_bentley_ottmann_tessellate_rectangular_traps (&traps.traps, CAIRO_FILL_RULE_WINDING);
	else if (traps.traps.is_rectilinear)
	    status = _cairo_bentley_ottmann_tessellate_rectilinear_traps (&traps.traps, CAIRO_FILL_RULE_WINDING);
	else
	    status = _cairo_bentley_ottmann_tessellate_traps (&traps.traps, CAIRO_FILL_RULE_WINDING);
	if (unlikely (status))
	    goto CLEANUP_TRAPS;
    }

    /* Use a fast path if the trapezoids consist of a simple region,
     * but we can only do this if we do not have a clip surface, or can
     * substitute the mask with the clip.
     */
    if (traps.traps.maybe_region &&
	_traps_are_pixel_aligned (&traps.traps, antialias) &&
	(! clip_surface ||
	 (extents->is_bounded && op != CAIRO_OPERATOR_SOURCE)))
    {
	cairo_boxes_t boxes;

	_boxes_for_traps (&boxes, &traps.traps, antialias);
	status = _clip_and_composite_boxes (dst, op, source, &boxes, extents);
    }
    else
    {
	/* Otherwise render the trapezoids to a mask and composite in the usual
	 * fashion.
	 */
	traps.antialias = antialias;
	status = trim_extents_to_traps (extents, &traps.traps);
	if (likely (status == CAIRO_STATUS_SUCCESS)) {
	    unsigned int flags = 0;

	    /* For unbounded operations, the X11 server will estimate the
	     * affected rectangle and apply the operation to that. However,
	     * there are cases where this is an overestimate (e.g. the
	     * clip-fill-{eo,nz}-unbounded test).
	     *
	     * The clip will trim that overestimate to our expectations.
	     */
	    if (! extents->is_bounded)
		flags |= FORCE_CLIP_REGION;

	    status = _clip_and_composite (dst, op, source, _composite_traps,
					  NULL, &traps, extents,
					  need_unbounded_clip (extents) | flags);
	}
    }

CLEANUP_TRAPS:
    _cairo_traps_fini (&traps.traps);

    return status;
}

static cairo_status_t
_clip_and_composite_boxes (cairo_xcb_surface_t *dst,
			   cairo_operator_t op,
			   const cairo_pattern_t *src,
			   cairo_boxes_t *boxes,
			   cairo_composite_rectangles_t *extents)
{
    composite_traps_info_t info;
    cairo_int_status_t status;

    if (boxes->num_boxes == 0 && extents->is_bounded)
	return CAIRO_STATUS_SUCCESS;

    if (boxes->is_pixel_aligned && _cairo_clip_is_region (extents->clip) &&
	(op == CAIRO_OPERATOR_SOURCE ||
	 (dst->base.is_clear && (op == CAIRO_OPERATOR_OVER || op == CAIRO_OPERATOR_ADD))))
    {
	if (boxes->num_boxes == 1 &&
	    extents->bounded.width  == dst->width &&
	    extents->bounded.height == dst->height)
	{
	    op = CAIRO_OPERATOR_SOURCE;
	    dst->deferred_clear = FALSE;
	}

	status = _upload_image_inplace (dst, src, boxes);
	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    return status;
    }

    /* Can we reduce drawing through a clip-mask to simply drawing the clip? */
    if (dst->connection->flags & CAIRO_XCB_RENDER_HAS_COMPOSITE_TRAPEZOIDS &&
	    extents->clip->path != NULL && extents->is_bounded) {
	cairo_polygon_t polygon;
	cairo_fill_rule_t fill_rule;
	cairo_antialias_t antialias;
	cairo_clip_t *clip;

	clip = _cairo_clip_copy (extents->clip);
	clip = _cairo_clip_intersect_boxes (clip, boxes);
	if (_cairo_clip_is_all_clipped (clip))
		return CAIRO_INT_STATUS_NOTHING_TO_DO;

	status = _cairo_clip_get_polygon (clip, &polygon,
					  &fill_rule, &antialias);
	_cairo_clip_path_destroy (clip->path);
	clip->path = NULL;
	if (likely (status == CAIRO_INT_STATUS_SUCCESS)) {
	    cairo_clip_t *saved_clip = extents->clip;
	    extents->clip = clip;
	    status = _composite_polygon (dst, op, src,
					 &polygon,
					 antialias,
					 fill_rule,
					 extents);
	    clip = extents->clip;
	    extents->clip = saved_clip;
	    _cairo_polygon_fini (&polygon);
	}
	if (clip)
	    _cairo_clip_destroy (clip);

	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    return status;
    }

    if (dst->deferred_clear) {
	status = _cairo_xcb_surface_clear (dst);
	if (unlikely (status))
	    return status;
    }

    if (boxes->is_pixel_aligned &&
	_cairo_clip_is_region (extents->clip) &&
	op == CAIRO_OPERATOR_SOURCE) {
	status = _upload_image_inplace (dst, src, boxes);
	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    return status;
    }

    if ((dst->connection->flags & CAIRO_XCB_RENDER_HAS_COMPOSITE) == 0)
	return _core_boxes (dst, op, src, boxes, extents);

    /* Use a fast path if the boxes are pixel aligned */
    status = _composite_boxes (dst, op, src, boxes, extents);
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;

    if ((dst->connection->flags & CAIRO_XCB_RENDER_HAS_COMPOSITE_TRAPEZOIDS) == 0)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    /* Otherwise render via a mask and composite in the usual fashion.  */
    status = _cairo_traps_init_boxes (&info.traps, boxes);
    if (unlikely (status))
	return status;

    info.antialias = CAIRO_ANTIALIAS_DEFAULT;
    status = trim_extents_to_traps (extents, &info.traps);
    if (status == CAIRO_INT_STATUS_SUCCESS) {
	status = _clip_and_composite (dst, op, src,
				      _composite_traps, NULL, &info,
				      extents, need_unbounded_clip (extents));
    }

    _cairo_traps_fini (&info.traps);
    return status;
}

static cairo_int_status_t
_composite_mask (void				*closure,
		 cairo_xcb_surface_t		*dst,
		 cairo_operator_t		 op,
		 const cairo_pattern_t		*src_pattern,
		 int				 dst_x,
		 int				 dst_y,
		 const cairo_rectangle_int_t	*extents,
		 cairo_clip_t			*clip)
{
    const cairo_pattern_t *mask_pattern = closure;
    cairo_xcb_picture_t *src, *mask = NULL;
    cairo_status_t status;

    if (dst->base.is_clear) {
	if (op == CAIRO_OPERATOR_OVER || op == CAIRO_OPERATOR_ADD)
	    op = CAIRO_OPERATOR_SOURCE;
    }

    if (op == CAIRO_OPERATOR_SOURCE && clip == NULL)
	dst->deferred_clear = FALSE;

    if (dst->deferred_clear) {
	status = _cairo_xcb_surface_clear (dst);
	if (unlikely (status))
		return status;
    }

    if (src_pattern != NULL) {
	src = _cairo_xcb_picture_for_pattern (dst, src_pattern, extents);
	if (unlikely (src->base.status))
	    return src->base.status;

	mask = _cairo_xcb_picture_for_pattern (dst, mask_pattern, extents);
	if (unlikely (mask->base.status)) {
	    cairo_surface_destroy (&src->base);
	    return mask->base.status;
	}

	_cairo_xcb_connection_render_composite (dst->connection,
						_render_operator (op),
						src->picture,
						mask->picture,
						dst->picture,
						extents->x + src->x,  extents->y + src->y,
						extents->x + mask->x, extents->y + mask->y,
						extents->x - dst_x,   extents->y - dst_y,
						extents->width,       extents->height);
	cairo_surface_destroy (&mask->base);
	cairo_surface_destroy (&src->base);
    } else {
	src = _cairo_xcb_picture_for_pattern (dst, mask_pattern, extents);
	if (unlikely (src->base.status))
	    return src->base.status;

	_cairo_xcb_connection_render_composite (dst->connection,
						_render_operator (op),
						src->picture,
						XCB_NONE,
						dst->picture,
						extents->x + src->x,  extents->y + src->y,
						0, 0,
						extents->x - dst_x,   extents->y - dst_y,
						extents->width,       extents->height);
	cairo_surface_destroy (&src->base);
    }

    return CAIRO_STATUS_SUCCESS;
}

struct composite_box_info {
    cairo_xcb_surface_t *dst;
    cairo_xcb_picture_t *src;
    uint8_t op;
};

static void composite_box(void *closure,
			  int16_t x, int16_t y,
			  int16_t w, int16_t h,
			  uint16_t coverage)
{
    struct composite_box_info *info = closure;

    if (coverage < 0xff00) {
	cairo_xcb_picture_t *mask;
	cairo_color_t color;

	color.red_short = color.green_short = color.blue_short = 0;
	color.alpha_short = coverage;

	mask = _solid_picture (info->dst, &color);
	if (likely (mask->base.status == CAIRO_STATUS_SUCCESS)) {
	    _cairo_xcb_connection_render_composite (info->dst->connection,
						    info->op,
						    info->src->picture,
						    mask->picture,
						    info->dst->picture,
						    x + info->src->x,  y + info->src->y,
						    0,                 0,
						    x,                 y,
						    w,                 h);
	}
	cairo_surface_destroy (&mask->base);
    } else {
	_cairo_xcb_connection_render_composite (info->dst->connection,
						info->op,
						info->src->picture,
						XCB_NONE,
						info->dst->picture,
						x + info->src->x,  y + info->src->y,
						0,                 0,
						x,                 y,
						w,                 h);
    }
}

static cairo_int_status_t
_composite_mask_clip_boxes (void			*closure,
			    cairo_xcb_surface_t		*dst,
			    cairo_operator_t		 op,
			    const cairo_pattern_t	*src_pattern,
			    int				 dst_x,
			    int				 dst_y,
			    const cairo_rectangle_int_t	*extents,
			    cairo_clip_t		*clip)
{
    struct composite_box_info info;
    cairo_status_t status;
    int i;

    assert (src_pattern == NULL);
    assert (op == CAIRO_OPERATOR_ADD);
    assert (dst->base.is_clear);

    if (clip->num_boxes > 1) {
	status = _cairo_xcb_surface_clear (dst);
	if (unlikely (status))
	    return status;
    }

    info.op = XCB_RENDER_PICT_OP_SRC;
    info.dst = dst;
    info.src = _cairo_xcb_picture_for_pattern (dst, closure, extents);
    if (unlikely (info.src->base.status))
	return info.src->base.status;

    info.src->x += dst_x;
    info.src->y += dst_y;

    for (i = 0; i < clip->num_boxes; i++)
	do_unaligned_box(composite_box, &info, &clip->boxes[i], dst_x, dst_y);
    cairo_surface_destroy (&info.src->base);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_composite_mask_clip (void				*closure,
		      cairo_xcb_surface_t		*dst,
		      cairo_operator_t			 op,
		      const cairo_pattern_t		*src_pattern,
		      int				 dst_x,
		      int				 dst_y,
		      const cairo_rectangle_int_t	*extents,
		      cairo_clip_t			*clip)
{
    const cairo_pattern_t *mask_pattern = closure;
    cairo_polygon_t polygon;
    cairo_fill_rule_t fill_rule;
    composite_traps_info_t info;
    cairo_status_t status;

    assert (src_pattern == NULL);
    assert (op == CAIRO_OPERATOR_ADD);
    assert (dst->base.is_clear);

    status = _cairo_clip_get_polygon (clip, &polygon,
				      &fill_rule, &info.antialias);
    if (unlikely (status))
	return status;

    _cairo_traps_init (&info.traps);
    status = _cairo_bentley_ottmann_tessellate_polygon (&info.traps,
							&polygon,
							fill_rule);
    _cairo_polygon_fini (&polygon);
    if (unlikely (status))
	return status;

    if (info.traps.has_intersections) {
	if (info.traps.is_rectangular)
	    status = _cairo_bentley_ottmann_tessellate_rectangular_traps (&info.traps, CAIRO_FILL_RULE_WINDING);
	else if (info.traps.is_rectilinear)
	    status = _cairo_bentley_ottmann_tessellate_rectilinear_traps (&info.traps, CAIRO_FILL_RULE_WINDING);
	else
	    status = _cairo_bentley_ottmann_tessellate_traps (&info.traps, CAIRO_FILL_RULE_WINDING);
	if (unlikely (status)) {
	    _cairo_traps_fini (&info.traps);
	    return status;
	}
    }

    status = _composite_traps (&info,
			       dst, CAIRO_OPERATOR_SOURCE, mask_pattern,
			       dst_x, dst_y,
			       extents, NULL);
    _cairo_traps_fini (&info.traps);

    return status;
}

struct composite_opacity_info {
    uint8_t op;
    cairo_xcb_surface_t *dst;
    cairo_xcb_picture_t *src;
    double opacity;
};

static void composite_opacity(void *closure,
			      int16_t x, int16_t y,
			      int16_t w, int16_t h,
			      uint16_t coverage)
{
    struct composite_opacity_info *info = closure;
    cairo_xcb_picture_t *mask;
    cairo_color_t color;

    color.red_short = color.green_short = color.blue_short = 0;
    color.alpha_short = info->opacity * coverage;

    mask = _solid_picture (info->dst, &color);
    if (likely (mask->base.status == CAIRO_STATUS_SUCCESS)) {
	if (info->src) {
	    _cairo_xcb_connection_render_composite (info->dst->connection,
						    info->op,
						    info->src->picture,
						    mask->picture,
						    info->dst->picture,
						    x + info->src->x,  y + info->src->y,
						    0,                 0,
						    x,                 y,
						    w,                 h);
	} else {
	    _cairo_xcb_connection_render_composite (info->dst->connection,
						    info->op,
						    mask->picture,
						    XCB_NONE,
						    info->dst->picture,
						    0,                 0,
						    0,                 0,
						    x,                 y,
						    w,                 h);
	}
    }

    cairo_surface_destroy (&mask->base);
}

static cairo_int_status_t
_composite_opacity_boxes (void				*closure,
			  cairo_xcb_surface_t		*dst,
			  cairo_operator_t		 op,
			  const cairo_pattern_t		*src_pattern,
			  int				 dst_x,
			  int				 dst_y,
			  const cairo_rectangle_int_t	*extents,
			  cairo_clip_t			*clip)
{
    const cairo_solid_pattern_t *mask_pattern = closure;
    struct composite_opacity_info info;
    cairo_status_t status;
    int i;

    if (dst->base.is_clear) {
	if (op == CAIRO_OPERATOR_OVER || op == CAIRO_OPERATOR_ADD)
	    op = CAIRO_OPERATOR_SOURCE;
    }

    if (op == CAIRO_OPERATOR_SOURCE &&
	(clip == NULL ||
	 (clip->extents.width >= extents->width &&
	  clip->extents.height >= extents->height)))
	dst->deferred_clear = FALSE;

    if (dst->deferred_clear) {
	status = _cairo_xcb_surface_clear (dst);
	if (unlikely (status))
	    return status;
    }

    info.op = _render_operator (op);
    info.dst = dst;

    if (src_pattern != NULL) {
	info.src = _cairo_xcb_picture_for_pattern (dst, src_pattern, extents);
	if (unlikely (info.src->base.status))
	    return info.src->base.status;
    } else
	info.src = NULL;

    info.opacity = mask_pattern->color.alpha;

    /* XXX for lots of boxes create a clip region for the fully opaque areas */
    if (clip) {
	for (i = 0; i < clip->num_boxes; i++)
	    do_unaligned_box(composite_opacity, &info,
			     &clip->boxes[i], dst_x, dst_y);
    } else {
	composite_opacity(&info,
			  extents->x - dst_x,
			  extents->y - dst_y,
			  extents->width,
			  extents->height,
			  0xffff);
    }
    cairo_surface_destroy (&info.src->base);

    return CAIRO_STATUS_SUCCESS;
}

/* high level rasteriser -> compositor */

cairo_int_status_t
_cairo_xcb_render_compositor_paint (const cairo_compositor_t     *compositor,
				    cairo_composite_rectangles_t *composite)
{
    cairo_xcb_surface_t *surface = (cairo_xcb_surface_t *) composite->surface;
    cairo_operator_t op = composite->op;
    cairo_pattern_t *source = &composite->source_pattern.base;
    cairo_boxes_t boxes;
    cairo_status_t status;

    if (unlikely (! _operator_is_supported (surface->connection->flags, op)))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if ((surface->connection->flags & (CAIRO_XCB_RENDER_HAS_COMPOSITE_TRAPEZOIDS |
				       CAIRO_XCB_RENDER_HAS_COMPOSITE)) == 0)
    {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    if (composite->clip == NULL &&
	source->type == CAIRO_PATTERN_TYPE_SOLID &&
	(op == CAIRO_OPERATOR_SOURCE ||
	 op == CAIRO_OPERATOR_CLEAR ||
	 (surface->base.is_clear &&
	  (op == CAIRO_OPERATOR_ADD || op == CAIRO_OPERATOR_OVER))))
    {
	surface->deferred_clear = TRUE;
	surface->deferred_clear_color = composite->source_pattern.solid.color;
	return CAIRO_STATUS_SUCCESS;
    }

     _cairo_clip_steal_boxes(composite->clip, &boxes);
     status = _clip_and_composite_boxes (surface, op, source, &boxes, composite);
     _cairo_clip_unsteal_boxes (composite->clip, &boxes);

    return status;
}

cairo_int_status_t
_cairo_xcb_render_compositor_mask (const cairo_compositor_t     *compositor,
				   cairo_composite_rectangles_t *composite)
{
    cairo_xcb_surface_t *surface = (cairo_xcb_surface_t *) composite->surface;
    cairo_operator_t op = composite->op;
    cairo_pattern_t *source = &composite->source_pattern.base;
    cairo_pattern_t *mask = &composite->mask_pattern.base;
    cairo_status_t status;

    if (unlikely (! _operator_is_supported (surface->connection->flags, op)))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if ((surface->connection->flags & CAIRO_XCB_RENDER_HAS_COMPOSITE) == 0)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (mask->type == CAIRO_PATTERN_TYPE_SOLID &&
	composite->clip->path == NULL &&
	! _cairo_clip_is_region (composite->clip)) {
	status = _clip_and_composite (surface, op, source,
				      _composite_opacity_boxes,
				      _composite_opacity_boxes,
				      (void *) mask,
				      composite, need_unbounded_clip (composite));
    } else {
	xcb_draw_func_t mask_func = NULL;
	if (surface->connection->flags & CAIRO_XCB_RENDER_HAS_COMPOSITE_TRAPEZOIDS)
	    mask_func = composite->clip->path ? _composite_mask_clip : _composite_mask_clip_boxes;
	status = _clip_and_composite (surface, op, source,
				      _composite_mask, mask_func,
				      (void *) mask,
				      composite, need_bounded_clip (composite));
    }

    return status;
}

static cairo_int_status_t
_cairo_xcb_surface_render_stroke_as_polygon (cairo_xcb_surface_t	*dst,
					     cairo_operator_t		 op,
					     const cairo_pattern_t	*source,
					     const cairo_path_fixed_t		*path,
					     const cairo_stroke_style_t	*stroke_style,
					     const cairo_matrix_t	*ctm,
					     const cairo_matrix_t	*ctm_inverse,
					     double			 tolerance,
					     cairo_antialias_t		 antialias,
					     cairo_composite_rectangles_t *extents)
{
    cairo_polygon_t polygon;
    cairo_status_t status;

    _cairo_polygon_init_with_clip (&polygon, extents->clip);
    status = _cairo_path_fixed_stroke_to_polygon (path,
						  stroke_style,
						  ctm, ctm_inverse,
						  tolerance,
						  &polygon);
    if (likely (status == CAIRO_STATUS_SUCCESS)) {
	status = _composite_polygon (dst, op, source,
				     &polygon, antialias,
				     CAIRO_FILL_RULE_WINDING,
				     extents);
    }
    _cairo_polygon_fini (&polygon);

    return status;
}

static cairo_status_t
_cairo_xcb_surface_render_stroke_via_mask (cairo_xcb_surface_t		*dst,
					   cairo_operator_t		 op,
					   const cairo_pattern_t	*source,
					   const cairo_path_fixed_t		*path,
					   const cairo_stroke_style_t	*stroke_style,
					   const cairo_matrix_t		*ctm,
					   const cairo_matrix_t		*ctm_inverse,
					   double			 tolerance,
					   cairo_antialias_t		 antialias,
					   cairo_composite_rectangles_t *extents)
{
    cairo_surface_t *image;
    cairo_status_t status;
    cairo_clip_t *clip;
    int x, y;

    x = extents->bounded.x;
    y = extents->bounded.y;
    image = _cairo_xcb_surface_create_similar_image (dst, CAIRO_FORMAT_A8,
						     extents->bounded.width,
						     extents->bounded.height);
    if (unlikely (image->status))
	return image->status;

    clip = _cairo_clip_copy_region (extents->clip);
    status = _cairo_surface_offset_stroke (image, x, y,
					   CAIRO_OPERATOR_ADD,
					   &_cairo_pattern_white.base,
					   path, stroke_style,
					   ctm, ctm_inverse,
					   tolerance, antialias,
					   clip);
    _cairo_clip_destroy (clip);
    if (likely (status == CAIRO_STATUS_SUCCESS)) {
	cairo_surface_pattern_t mask;

	_cairo_pattern_init_for_surface (&mask, image);
	mask.base.filter = CAIRO_FILTER_NEAREST;

	cairo_matrix_init_translate (&mask.base.matrix, -x, -y);
	status = _clip_and_composite (dst, op, source,
				      _composite_mask, NULL, &mask.base,
				      extents, need_bounded_clip (extents));
	_cairo_pattern_fini (&mask.base);
    }

    cairo_surface_finish (image);
    cairo_surface_destroy (image);

    return status;
}

cairo_int_status_t
_cairo_xcb_render_compositor_stroke (const cairo_compositor_t     *compositor,
				     cairo_composite_rectangles_t *composite,
				     const cairo_path_fixed_t     *path,
				     const cairo_stroke_style_t   *style,
				     const cairo_matrix_t         *ctm,
				     const cairo_matrix_t         *ctm_inverse,
				     double                        tolerance,
				     cairo_antialias_t             antialias)
{
    cairo_xcb_surface_t *surface = (cairo_xcb_surface_t *) composite->surface;
    cairo_operator_t op = composite->op;
    cairo_pattern_t *source = &composite->source_pattern.base;
    cairo_int_status_t status;

    if (unlikely (! _operator_is_supported (surface->connection->flags, op)))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if ((surface->connection->flags & (CAIRO_XCB_RENDER_HAS_COMPOSITE_TRAPEZOIDS |
			   CAIRO_XCB_RENDER_HAS_COMPOSITE)) == 0)
    {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    status = CAIRO_INT_STATUS_UNSUPPORTED;
    if (_cairo_path_fixed_stroke_is_rectilinear (path)) {
	cairo_boxes_t boxes;

	_cairo_boxes_init_with_clip (&boxes, composite->clip);
	status = _cairo_path_fixed_stroke_rectilinear_to_boxes (path,
								style,
								ctm,
								antialias,
								&boxes);
	if (likely (status == CAIRO_INT_STATUS_SUCCESS)) {
	    status = _clip_and_composite_boxes (surface, op, source,
						&boxes, composite);
	}
	_cairo_boxes_fini (&boxes);
    }

    if (status == CAIRO_INT_STATUS_UNSUPPORTED) {
	if (surface->connection->flags & CAIRO_XCB_RENDER_HAS_COMPOSITE_TRAPEZOIDS) {
	    status = _cairo_xcb_surface_render_stroke_as_polygon (surface, op, source,
								  path, style,
								  ctm, ctm_inverse,
								  tolerance, antialias,
								  composite);
	} else if (surface->connection->flags & CAIRO_XCB_RENDER_HAS_COMPOSITE) {
	    status = _cairo_xcb_surface_render_stroke_via_mask (surface, op, source,
								path, style,
								ctm, ctm_inverse,
								tolerance, antialias,
								composite);
	} else {
	    ASSERT_NOT_REACHED;
	}
    }

    return status;
}

static cairo_status_t
_cairo_xcb_surface_render_fill_as_polygon (cairo_xcb_surface_t	*dst,
					   cairo_operator_t	 op,
					   const cairo_pattern_t*source,
					   const cairo_path_fixed_t	*path,
					   cairo_fill_rule_t	 fill_rule,
					   double		 tolerance,
					   cairo_antialias_t	 antialias,
					   cairo_composite_rectangles_t *extents)
{
    cairo_polygon_t polygon;
    cairo_status_t status;

    _cairo_polygon_init_with_clip (&polygon, extents->clip);
    status = _cairo_path_fixed_fill_to_polygon (path, tolerance, &polygon);
    if (likely (status == CAIRO_STATUS_SUCCESS)) {
	status = _composite_polygon (dst, op, source,
				     &polygon,
				     antialias,
				     fill_rule,
				     extents);
    }
    _cairo_polygon_fini (&polygon);

    return status;
}

static cairo_status_t
_cairo_xcb_surface_render_fill_via_mask (cairo_xcb_surface_t	*dst,
					 cairo_operator_t	 op,
					 const cairo_pattern_t	*source,
					 const cairo_path_fixed_t	*path,
					 cairo_fill_rule_t	 fill_rule,
					 double			 tolerance,
					 cairo_antialias_t	 antialias,
					 cairo_composite_rectangles_t *extents)
{
    cairo_surface_t *image;
    cairo_status_t status;
    cairo_clip_t *clip;
    int x, y;

    x = extents->bounded.x;
    y = extents->bounded.y;
    image = _cairo_xcb_surface_create_similar_image (dst, CAIRO_FORMAT_A8,
						     extents->bounded.width,
						     extents->bounded.height);
    if (unlikely (image->status))
	return image->status;

    clip = _cairo_clip_copy_region (extents->clip);
    status = _cairo_surface_offset_fill (image, x, y,
					 CAIRO_OPERATOR_ADD,
					 &_cairo_pattern_white.base,
					 path, fill_rule, tolerance, antialias,
					 clip);
    _cairo_clip_destroy (clip);
    if (likely (status == CAIRO_STATUS_SUCCESS)) {
	cairo_surface_pattern_t mask;

	_cairo_pattern_init_for_surface (&mask, image);
	mask.base.filter = CAIRO_FILTER_NEAREST;

	cairo_matrix_init_translate (&mask.base.matrix, -x, -y);
	status = _clip_and_composite (dst, op, source,
				      _composite_mask, NULL, &mask.base,
				      extents, need_bounded_clip (extents));

	_cairo_pattern_fini (&mask.base);
    }

    cairo_surface_finish (image);
    cairo_surface_destroy (image);

    return status;
}

cairo_int_status_t
_cairo_xcb_render_compositor_fill (const cairo_compositor_t     *compositor,
				   cairo_composite_rectangles_t *composite,
				   const cairo_path_fixed_t     *path,
				   cairo_fill_rule_t             fill_rule,
				   double                        tolerance,
				   cairo_antialias_t             antialias)
{
    cairo_xcb_surface_t *surface = (cairo_xcb_surface_t *) composite->surface;
    cairo_operator_t op = composite->op;
    cairo_pattern_t *source = &composite->source_pattern.base;
    cairo_int_status_t status;

    if (unlikely (! _operator_is_supported (surface->connection->flags, op)))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if ((surface->connection->flags & (CAIRO_XCB_RENDER_HAS_COMPOSITE_TRAPEZOIDS |
				       CAIRO_XCB_RENDER_HAS_COMPOSITE)) == 0)
    {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    status = CAIRO_INT_STATUS_UNSUPPORTED;
    if (_cairo_path_fixed_fill_is_rectilinear (path)) {
	cairo_boxes_t boxes;

	_cairo_boxes_init_with_clip (&boxes, composite->clip);
	status = _cairo_path_fixed_fill_rectilinear_to_boxes (path,
							      fill_rule,
							      antialias,
							      &boxes);
	if (likely (status == CAIRO_INT_STATUS_SUCCESS)) {
	    status = _clip_and_composite_boxes (surface, op, source,
						&boxes, composite);
	}
	_cairo_boxes_fini (&boxes);
    }

    if (status == CAIRO_INT_STATUS_UNSUPPORTED) {
	if (surface->connection->flags & CAIRO_XCB_RENDER_HAS_COMPOSITE_TRAPEZOIDS) {
	    status = _cairo_xcb_surface_render_fill_as_polygon (surface, op, source, path,
								fill_rule, tolerance, antialias,
								composite);
	} else if (surface->connection->flags & CAIRO_XCB_RENDER_HAS_COMPOSITE) {
	    status = _cairo_xcb_surface_render_fill_via_mask (surface, op, source, path,
							      fill_rule, tolerance, antialias,
							      composite);
	} else {
	    ASSERT_NOT_REACHED;
	}
    }

    return status;
}

static cairo_status_t
_cairo_xcb_surface_render_glyphs_via_mask (cairo_xcb_surface_t		*dst,
					   cairo_operator_t		 op,
					   const cairo_pattern_t	*source,
					   cairo_scaled_font_t		*scaled_font,
					   cairo_glyph_t		*glyphs,
					   int				 num_glyphs,
					   cairo_composite_rectangles_t *extents)
{
    cairo_surface_t *image;
    cairo_content_t content;
    cairo_status_t status;
    cairo_clip_t *clip;
    int x, y;

    content = CAIRO_CONTENT_ALPHA;
    if (scaled_font->options.antialias == CAIRO_ANTIALIAS_SUBPIXEL)
	content = CAIRO_CONTENT_COLOR_ALPHA;

    x = extents->bounded.x;
    y = extents->bounded.y;
    image = _cairo_xcb_surface_create_similar_image (dst,
						     _cairo_format_from_content (content),
						     extents->bounded.width,
						     extents->bounded.height);
    if (unlikely (image->status))
	return image->status;

    clip = _cairo_clip_copy_region (extents->clip);
    status = _cairo_surface_offset_glyphs (image, x, y,
					   CAIRO_OPERATOR_ADD,
					   &_cairo_pattern_white.base,
					   scaled_font, glyphs, num_glyphs,
					   clip);
    _cairo_clip_destroy (clip);
    if (likely (status == CAIRO_STATUS_SUCCESS)) {
	cairo_surface_pattern_t mask;

	_cairo_pattern_init_for_surface (&mask, image);
	mask.base.filter = CAIRO_FILTER_NEAREST;
	if (content & CAIRO_CONTENT_COLOR)
	    mask.base.has_component_alpha = TRUE;

	cairo_matrix_init_translate (&mask.base.matrix, -x, -y);
	status = _clip_and_composite (dst, op, source,
				      _composite_mask, NULL, &mask.base,
				      extents, need_bounded_clip (extents));

	_cairo_pattern_fini (&mask.base);
    }

    cairo_surface_finish (image);
    cairo_surface_destroy (image);

    return status;
}

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
} cairo_xcb_glyph_t;

/* compile-time assert that #cairo_xcb_glyph_t is the same size as #cairo_glyph_t */
COMPILE_TIME_ASSERT (sizeof (cairo_xcb_glyph_t) == sizeof (cairo_glyph_t));

typedef struct {
    cairo_scaled_font_t *font;
    cairo_xcb_glyph_t *glyphs;
    int num_glyphs;
    cairo_bool_t use_mask;
} composite_glyphs_info_t;

static cairo_status_t
_can_composite_glyphs (cairo_xcb_surface_t *dst,
		       cairo_rectangle_int_t *extents,
		       cairo_scaled_font_t *scaled_font,
		       cairo_glyph_t *glyphs,
		       int *num_glyphs)
{
#define GLYPH_CACHE_SIZE 64
    cairo_box_t bbox_cache[GLYPH_CACHE_SIZE];
    unsigned long glyph_cache[GLYPH_CACHE_SIZE];
#undef GLYPH_CACHE_SIZE
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_glyph_t *glyphs_end, *valid_glyphs;
    const int max_glyph_size = dst->connection->maximum_request_length - 64;

    /* We must initialize the cache with values that cannot match the
     * "hash" to guarantee that when compared for the first time they
     * will result in a mismatch. The hash function is simply modulus,
     * so we cannot use 0 in glyph_cache[0], but we can use it in all
     * other array cells.
     */
    memset (glyph_cache, 0, sizeof (glyph_cache));
    glyph_cache[0] = 1;

    /* Scan for oversized glyphs or glyphs outside the representable
     * range and fallback in that case, discard glyphs outside of the
     * image.
     */
    valid_glyphs = glyphs;
    for (glyphs_end = glyphs + *num_glyphs; glyphs != glyphs_end; glyphs++) {
	double x1, y1, x2, y2;
	cairo_scaled_glyph_t *glyph;
	cairo_box_t *bbox;
	int width, height, len;
	int g;

	g = glyphs->index % ARRAY_LENGTH (glyph_cache);
	if (glyph_cache[g] != glyphs->index) {
	    status = _cairo_scaled_glyph_lookup (scaled_font,
						 glyphs->index,
						 CAIRO_SCALED_GLYPH_INFO_METRICS,
                                                 NULL, /* foreground color */
						 &glyph);
	    if (unlikely (status))
		break;

	    glyph_cache[g] = glyphs->index;
	    bbox_cache[g] = glyph->bbox;
	}
	bbox = &bbox_cache[g];

	/* Drop glyphs outside the clipping */
	x1 = _cairo_fixed_to_double (bbox->p1.x);
	y1 = _cairo_fixed_to_double (bbox->p1.y);
	y2 = _cairo_fixed_to_double (bbox->p2.y);
	x2 = _cairo_fixed_to_double (bbox->p2.x);
	if (unlikely (glyphs->x + x2 <= extents->x ||
		      glyphs->y + y2 <= extents->y ||
		      glyphs->x + x1 >= extents->x + extents->width ||
		      glyphs->y + y1 >= extents->y + extents->height))
	{
	    (*num_glyphs)--;
	    continue;
	}

	/* XRenderAddGlyph does not handle a glyph surface larger than
	 * the extended maximum XRequest size.
	 */
	width  = _cairo_fixed_integer_ceil (bbox->p2.x - bbox->p1.x);
	height = _cairo_fixed_integer_ceil (bbox->p2.y - bbox->p1.y);
	len = CAIRO_STRIDE_FOR_WIDTH_BPP (width, 32) * height;
	if (unlikely (len >= max_glyph_size)) {
	    status = CAIRO_INT_STATUS_UNSUPPORTED;
	    break;
	}

	/* The glyph coordinates must be representable in an int16_t.
	 * When possible, they will be expressed as an offset from the
	 * previous glyph, otherwise they will be an offset from the
	 * operation extents or from the surface origin. If the last
	 * two options are not valid, fallback.
	 */
	if (unlikely (glyphs->x > INT16_MAX ||
		      glyphs->y > INT16_MAX ||
		      glyphs->x - extents->x < INT16_MIN ||
		      glyphs->y - extents->y < INT16_MIN))
	{
	    status = CAIRO_INT_STATUS_UNSUPPORTED;
	    break;
	}


	if (unlikely (valid_glyphs != glyphs))
	    *valid_glyphs = *glyphs;
	valid_glyphs++;
    }

    if (unlikely (valid_glyphs != glyphs)) {
	for (; glyphs != glyphs_end; glyphs++) {
	    *valid_glyphs = *glyphs;
	    valid_glyphs++;
	}
    }

    return status;
}

/* Start a new element for the first glyph,
 * or for any glyph that has unexpected position,
 * or if current element has too many glyphs
 * (Xrender limits each element to 252 glyphs, we limit them to 128)
 *
 * These same conditions need to be mirrored between
 * _cairo_xcb_surface_emit_glyphs and _emit_glyph_chunks
 */
#define _start_new_glyph_elt(count, glyph) \
    (((count) & 127) == 0 || (glyph)->i.x || (glyph)->i.y)

/* sz_xGlyphtElt required alignment to a 32-bit boundary, so ensure we have
 * enough room for padding */
typedef struct {
    uint8_t   len;
    uint8_t   pad1;
    uint16_t  pad2;
    int16_t   deltax;
    int16_t   deltay;
} x_glyph_elt_t;
#define _cairo_sz_x_glyph_elt_t (sizeof (x_glyph_elt_t) + 4)

static void
_cairo_xcb_font_destroy (cairo_xcb_font_t *font)
{
    int i;

    for (i = 0; i < NUM_GLYPHSETS; i++) {
	cairo_xcb_font_glyphset_info_t *info;

	info = &font->glyphset_info[i];
	free (info->pending_free_glyphs);
    }

    cairo_list_del (&font->base.link);
    cairo_list_del (&font->link);

    _cairo_xcb_connection_destroy (font->connection);

    free (font);
}

static void
_cairo_xcb_font_fini (cairo_scaled_font_private_t *abstract_private,
		      cairo_scaled_font_t *scaled_font)
{
    cairo_xcb_font_t *font_private = (cairo_xcb_font_t *)abstract_private;
    cairo_xcb_connection_t *connection;
    cairo_bool_t have_connection;
    cairo_status_t status;
    int i;

    connection = font_private->connection;

    status = _cairo_xcb_connection_acquire (connection);
    have_connection = status == CAIRO_STATUS_SUCCESS;

    for (i = 0; i < NUM_GLYPHSETS; i++) {
	cairo_xcb_font_glyphset_info_t *info;

	info = &font_private->glyphset_info[i];
	if (info->glyphset && status == CAIRO_STATUS_SUCCESS) {
	    _cairo_xcb_connection_render_free_glyph_set (connection,
							 info->glyphset);
	}
    }

    if (have_connection)
	_cairo_xcb_connection_release (connection);

    _cairo_xcb_font_destroy (font_private);
}


static cairo_xcb_font_t *
_cairo_xcb_font_create (cairo_xcb_connection_t *connection,
			cairo_scaled_font_t  *font)
{
    cairo_xcb_font_t	*priv;
    int i;

    priv = _cairo_malloc (sizeof (cairo_xcb_font_t));
    if (unlikely (priv == NULL))
	return NULL;

    _cairo_scaled_font_attach_private (font, &priv->base, connection,
				       _cairo_xcb_font_fini);

    priv->scaled_font = font;
    priv->connection = _cairo_xcb_connection_reference (connection);
    cairo_list_add (&priv->link, &connection->fonts);

    for (i = 0; i < NUM_GLYPHSETS; i++) {
	cairo_xcb_font_glyphset_info_t *info = &priv->glyphset_info[i];
	switch (i) {
	case GLYPHSET_INDEX_ARGB32: info->format = CAIRO_FORMAT_ARGB32; break;
	case GLYPHSET_INDEX_A8:     info->format = CAIRO_FORMAT_A8;     break;
	case GLYPHSET_INDEX_A1:     info->format = CAIRO_FORMAT_A1;     break;
	default:                    ASSERT_NOT_REACHED;                          break;
	}
	info->xrender_format = 0;
	info->glyphset = XCB_NONE;
	info->pending_free_glyphs = NULL;
    }

    return priv;
}

void
_cairo_xcb_font_close (cairo_xcb_font_t *font)
{
    cairo_scaled_font_t	*scaled_font;

    scaled_font = font->scaled_font;

    //scaled_font->surface_private = NULL;
    _cairo_scaled_font_reset_cache (scaled_font);

    _cairo_xcb_font_destroy (font);
}

static void
_cairo_xcb_render_free_glyphs (cairo_xcb_connection_t *connection,
			       cairo_xcb_font_glyphset_free_glyphs_t *to_free)
{
    _cairo_xcb_connection_render_free_glyphs (connection,
					      to_free->glyphset,
					      to_free->glyph_count,
					      to_free->glyph_indices);
}

static int
_cairo_xcb_get_glyphset_index_for_format (cairo_format_t format)
{
    if (format == CAIRO_FORMAT_A8)
        return GLYPHSET_INDEX_A8;
    if (format == CAIRO_FORMAT_A1)
        return GLYPHSET_INDEX_A1;

    assert (format == CAIRO_FORMAT_ARGB32);
    return GLYPHSET_INDEX_ARGB32;
}



static inline cairo_xcb_font_t *
_cairo_xcb_font_get (const cairo_xcb_connection_t *c,
		     cairo_scaled_font_t *font)
{
    return (cairo_xcb_font_t *)_cairo_scaled_font_find_private (font, c);
}


static cairo_xcb_font_glyphset_info_t *
_cairo_xcb_scaled_font_get_glyphset_info_for_format (cairo_xcb_connection_t *c,
						     cairo_scaled_font_t *font,
						     cairo_format_t       format)
{
    cairo_xcb_font_t *priv;
    cairo_xcb_font_glyphset_info_t *info;
    int glyphset_index;

    glyphset_index = _cairo_xcb_get_glyphset_index_for_format (format);

    priv = _cairo_xcb_font_get (c, font);
    if (priv == NULL) {
	priv = _cairo_xcb_font_create (c, font);
	if (priv == NULL)
	    return NULL;
    }

    info = &priv->glyphset_info[glyphset_index];
    if (info->glyphset == XCB_NONE) {
	info->glyphset = xcb_generate_id (c->xcb_connection);
	info->xrender_format = c->standard_formats[info->format];

	_cairo_xcb_connection_render_create_glyph_set (c,
						       info->glyphset,
						       info->xrender_format);
    }

    return info;
}

static cairo_bool_t
_cairo_xcb_glyphset_info_has_pending_free_glyph (
				cairo_xcb_font_glyphset_info_t *info,
				unsigned long glyph_index)
{
    if (info->pending_free_glyphs != NULL) {
	cairo_xcb_font_glyphset_free_glyphs_t *to_free;
	int i;

	to_free = info->pending_free_glyphs;
	for (i = 0; i < to_free->glyph_count; i++) {
	    if (to_free->glyph_indices[i] == glyph_index) {
		to_free->glyph_count--;
		memmove (&to_free->glyph_indices[i],
			 &to_free->glyph_indices[i+1],
			 (to_free->glyph_count - i) * sizeof (to_free->glyph_indices[0]));
		return TRUE;
	    }
	}
    }

    return FALSE;
}

typedef struct {
    cairo_scaled_glyph_private_t base;

    cairo_xcb_font_glyphset_info_t *glyphset;
} cairo_xcb_glyph_private_t;

static cairo_xcb_font_glyphset_info_t *
_cairo_xcb_scaled_font_get_glyphset_info_for_pending_free_glyph (cairo_xcb_connection_t *c,
					       cairo_scaled_font_t *font,
					       unsigned long glyph_index,
					       cairo_image_surface_t *surface)
{
    cairo_xcb_font_t *priv;
    int i;

    priv = _cairo_xcb_font_get (c, font);
    if (priv == NULL)
	return NULL;

    if (surface != NULL) {
        i = _cairo_xcb_get_glyphset_index_for_format (surface->format);

	if (_cairo_xcb_glyphset_info_has_pending_free_glyph (
						&priv->glyphset_info[i],
						glyph_index))
	{
	    return &priv->glyphset_info[i];
	}
    } else {
	for (i = 0; i < NUM_GLYPHSETS; i++) {
	    if (_cairo_xcb_glyphset_info_has_pending_free_glyph (
						&priv->glyphset_info[i],
						glyph_index))
	    {
		return &priv->glyphset_info[i];
	    }
	}
    }

    return NULL;
}

static void
_cairo_xcb_glyph_fini (cairo_scaled_glyph_private_t *glyph_private,
		       cairo_scaled_glyph_t *glyph,
		       cairo_scaled_font_t  *font)
{
    cairo_xcb_glyph_private_t *priv = (cairo_xcb_glyph_private_t *)glyph_private;

    if (! font->finished) {
	cairo_xcb_font_glyphset_info_t *info = priv->glyphset;
	cairo_xcb_font_glyphset_free_glyphs_t *to_free;
	cairo_xcb_font_t *font_private;

	font_private = _cairo_xcb_font_get (glyph_private->key, font);
	assert (font_private);

	to_free = info->pending_free_glyphs;
	if (to_free != NULL &&
	    to_free->glyph_count == ARRAY_LENGTH (to_free->glyph_indices))
	{
	    _cairo_xcb_render_free_glyphs (font_private->connection, to_free);
	    to_free = info->pending_free_glyphs = NULL;
	}

	if (to_free == NULL) {
	    to_free = _cairo_malloc (sizeof (cairo_xcb_font_glyphset_free_glyphs_t));
	    if (unlikely (to_free == NULL)) {
		_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
		return; /* XXX cannot propagate failure */
	    }

	    to_free->glyphset = info->glyphset;
	    to_free->glyph_count = 0;
	    info->pending_free_glyphs = to_free;
	}

	to_free->glyph_indices[to_free->glyph_count++] =
	    _cairo_scaled_glyph_index (glyph);
    }

    cairo_list_del (&glyph_private->link);
    free (glyph_private);
}


static cairo_status_t
_cairo_xcb_glyph_attach (cairo_xcb_connection_t  *c,
			 cairo_scaled_glyph_t  *glyph,
			 cairo_xcb_font_glyphset_info_t *info)
{
    cairo_xcb_glyph_private_t *priv;

    priv = _cairo_malloc (sizeof (*priv));
    if (unlikely (priv == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    _cairo_scaled_glyph_attach_private (glyph, &priv->base, c,
					_cairo_xcb_glyph_fini);
    priv->glyphset = info;

    glyph->dev_private = info;
    glyph->dev_private_key = c;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_xcb_surface_add_glyph (cairo_xcb_connection_t *connection,
			       cairo_scaled_font_t   *font,
			       cairo_scaled_glyph_t **scaled_glyph_out)
{
    xcb_render_glyphinfo_t glyph_info;
    uint32_t glyph_index;
    uint8_t *data;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_scaled_glyph_t *scaled_glyph = *scaled_glyph_out;
    cairo_image_surface_t *glyph_surface = scaled_glyph->surface;
    cairo_bool_t already_had_glyph_surface;
    cairo_xcb_font_glyphset_info_t *info;

    glyph_index = _cairo_scaled_glyph_index (scaled_glyph);

    /* check to see if we have a pending XRenderFreeGlyph for this glyph */
    info = _cairo_xcb_scaled_font_get_glyphset_info_for_pending_free_glyph (connection, font, glyph_index, glyph_surface);
    if (info != NULL)
	return _cairo_xcb_glyph_attach (connection, scaled_glyph, info);

    if (glyph_surface == NULL) {
	status = _cairo_scaled_glyph_lookup (font,
					     glyph_index,
					     CAIRO_SCALED_GLYPH_INFO_METRICS |
					     CAIRO_SCALED_GLYPH_INFO_SURFACE,
                                             NULL, /* foreground color */
					     scaled_glyph_out);
	if (unlikely (status))
	    return status;

	scaled_glyph = *scaled_glyph_out;
	glyph_surface = scaled_glyph->surface;
	already_had_glyph_surface = FALSE;
    } else {
	already_had_glyph_surface = TRUE;
    }

    info = _cairo_xcb_scaled_font_get_glyphset_info_for_format (connection,
								font,
								glyph_surface->format);
    if (unlikely (info == NULL)) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto BAIL;
    }

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
	glyph_surface = _cairo_image_surface_coerce_to_format (glyph_surface,
						               info->format);
	status = glyph_surface->base.status;
	if (unlikely (status))
	    goto BAIL;
    }

    /* XXX: FRAGILE: We're ignore device_transform scaling here. A bug? */
    glyph_info.x = _cairo_lround (glyph_surface->base.device_transform.x0);
    glyph_info.y = _cairo_lround (glyph_surface->base.device_transform.y0);
    glyph_info.width  = glyph_surface->width;
    glyph_info.height = glyph_surface->height;
    glyph_info.x_off = scaled_glyph->x_advance;
    glyph_info.y_off = scaled_glyph->y_advance;

    data = glyph_surface->data;

    /* flip formats around */
    switch (_cairo_xcb_get_glyphset_index_for_format (scaled_glyph->surface->format)) {
    case GLYPHSET_INDEX_A1:
	/* local bitmaps are always stored with bit == byte */
	if (_cairo_is_little_endian() != (connection->root->bitmap_format_bit_order == XCB_IMAGE_ORDER_LSB_FIRST)) {
	    int		    c = glyph_surface->stride * glyph_surface->height;
	    const uint8_t *d;
	    uint8_t *new, *n;

	    if (c == 0)
		break;

	    new = _cairo_malloc (c);
	    if (unlikely (new == NULL)) {
		status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
		goto BAIL;
	    }

	    n = new;
	    d = data;
	    do {
		uint8_t b = *d++;
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
	if (_cairo_is_little_endian() != (connection->root->image_byte_order == XCB_IMAGE_ORDER_LSB_FIRST)) {
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

    _cairo_xcb_connection_render_add_glyphs (connection,
					     info->glyphset,
					     1, &glyph_index, &glyph_info,
					     glyph_surface->stride * glyph_surface->height,
					     data);

    if (data != glyph_surface->data)
	free (data);

    status = _cairo_xcb_glyph_attach (connection, scaled_glyph, info);

 BAIL:
    if (glyph_surface != scaled_glyph->surface)
	cairo_surface_destroy (&glyph_surface->base);

    /* If the scaled glyph didn't already have a surface attached
     * to it, release the created surface now that we have it
     * uploaded to the X server.  If the surface has already been
     * there (e.g. because image backend requested it), leave it in
     * the cache
     */
    if (! already_had_glyph_surface)
	_cairo_scaled_glyph_set_surface (scaled_glyph, font, NULL);

    return status;
}

typedef void (*cairo_xcb_render_composite_text_func_t)
	      (cairo_xcb_connection_t       *connection,
	       uint8_t                          op,
	       xcb_render_picture_t src,
	       xcb_render_picture_t dst,
	       xcb_render_pictformat_t mask_format,
	       xcb_render_glyphset_t glyphset,
	       int16_t                          src_x,
	       int16_t                          src_y,
	       uint32_t                          len,
	       uint8_t                        *cmd);


static cairo_status_t
_emit_glyphs_chunk (cairo_xcb_surface_t *dst,
		    cairo_operator_t op,
		    cairo_xcb_picture_t *src,
		    /* info for this chunk */
		    cairo_xcb_glyph_t *glyphs,
		    int num_glyphs,
		    int width,
		    int estimated_req_size,
		    cairo_xcb_font_glyphset_info_t *info,
		    xcb_render_pictformat_t mask_format)
{
    cairo_xcb_render_composite_text_func_t composite_text_func;
    uint8_t stack_buf[CAIRO_STACK_BUFFER_SIZE];
    uint8_t *buf = stack_buf;
    x_glyph_elt_t *elt = NULL; /* silence compiler */
    uint32_t len;
    int i;

    if (estimated_req_size > ARRAY_LENGTH (stack_buf)) {
	buf = _cairo_malloc (estimated_req_size);
	if (unlikely (buf == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    len = 0;
    for (i = 0; i < num_glyphs; i++) {
      if (_start_new_glyph_elt (i, &glyphs[i])) {
	  if (len & 3)
	      len += 4 - (len & 3);

	  elt = (x_glyph_elt_t *) (buf + len);
	  elt->len = 0;
	  elt->deltax = glyphs[i].i.x;
	  elt->deltay = glyphs[i].i.y;
	  len += sizeof (x_glyph_elt_t);
      }

      switch (width) {
      case 1: *(uint8_t *) (buf + len) = glyphs[i].index; break;
      case 2: *(uint16_t *) (buf + len) = glyphs[i].index; break;
      default:
      case 4: *(uint32_t *) (buf + len) = glyphs[i].index; break;
      }
      len += width;
      elt->len++;
    }
    if (len & 3)
	len += 4 - (len & 3);

    switch (width) {
    case 1:
	composite_text_func = _cairo_xcb_connection_render_composite_glyphs_8;
	break;
    case 2:
	composite_text_func = _cairo_xcb_connection_render_composite_glyphs_16;
	break;
    default:
    case 4:
	composite_text_func = _cairo_xcb_connection_render_composite_glyphs_32;
	break;
    }
    composite_text_func (dst->connection,
			 _render_operator (op),
			 src->picture,
			 dst->picture,
			 mask_format,
			 info->glyphset,
			 src->x + glyphs[0].i.x,
			 src->y + glyphs[0].i.y,
			 len, buf);

    if (buf != stack_buf)
      free (buf);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_composite_glyphs (void				*closure,
		  cairo_xcb_surface_t		*dst,
		  cairo_operator_t		 op,
		  const cairo_pattern_t		*pattern,
		  int				 dst_x,
		  int				 dst_y,
		  const cairo_rectangle_int_t	*extents,
		  cairo_clip_t			*clip)
{
    composite_glyphs_info_t *info = closure;
    cairo_scaled_glyph_t *glyph_cache[64];
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_fixed_t x = 0, y = 0;
    cairo_xcb_font_glyphset_info_t *glyphset_info = NULL, *this_glyphset_info;
    const unsigned int max_request_size = dst->connection->maximum_request_length - 64;
    cairo_xcb_picture_t *src;

    unsigned long max_index = 0;
    int width = 1;

    unsigned int request_size = 0;
    int i;

    if (dst->deferred_clear) {
	status = _cairo_xcb_surface_clear (dst);
	if (unlikely (status))
		return status;
    }

    src = _cairo_xcb_picture_for_pattern (dst, pattern, extents);
    if (unlikely (src->base.status))
	return src->base.status;

    memset (glyph_cache, 0, sizeof (glyph_cache));

    for (i = 0; i < info->num_glyphs; i++) {
	cairo_scaled_glyph_t *glyph;
	unsigned long glyph_index = info->glyphs[i].index;
	int cache_index = glyph_index % ARRAY_LENGTH (glyph_cache);
	int old_width = width;
	int this_x, this_y;

	glyph = glyph_cache[cache_index];
	if (glyph == NULL ||
	    _cairo_scaled_glyph_index (glyph) != glyph_index)
	{
	    status = _cairo_scaled_glyph_lookup (info->font,
						 glyph_index,
						 CAIRO_SCALED_GLYPH_INFO_METRICS,
                                                 NULL, /* foreground color */
						 &glyph);
	    if (unlikely (status)) {
		cairo_surface_destroy (&src->base);
		return status;
	    }

	    /* Send unseen glyphs to the server */
	    if (glyph->dev_private_key != dst->connection) {
		status = _cairo_xcb_surface_add_glyph (dst->connection,
						       info->font,
						       &glyph);
		if (unlikely (status)) {
		    cairo_surface_destroy (&src->base);
		    return status;
		}
	    }

	    glyph_cache[cache_index] = glyph;
	}

	this_x = _cairo_lround (info->glyphs[i].d.x) - dst_x;
	this_y = _cairo_lround (info->glyphs[i].d.y) - dst_y;

	this_glyphset_info = glyph->dev_private;
	if (glyphset_info == NULL)
	    glyphset_info = this_glyphset_info;

	/* Update max glyph index */
	if (glyph_index > max_index) {
	    max_index = glyph_index;
	    if (max_index >= 65536)
		width = 4;
	    else if (max_index >= 256)
		width = 2;
	    if (width != old_width)
		request_size += (width - old_width) * i;
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
	 * representable as 16-bit offset in _can_composite_glyphs().
	 */
	if (request_size + width > max_request_size - _cairo_sz_x_glyph_elt_t ||
	    this_x - x > INT16_MAX || this_x - x < INT16_MIN ||
	    this_y - y > INT16_MAX || this_y - y < INT16_MIN ||
	    this_glyphset_info != glyphset_info)
	{
	    status = _emit_glyphs_chunk (dst, op, src,
					 info->glyphs, i,
					 old_width, request_size,
					 glyphset_info,
					 info->use_mask ? glyphset_info->xrender_format : 0);
	    if (unlikely (status)) {
		cairo_surface_destroy (&src->base);
		return status;
	    }

	    info->glyphs += i;
	    info->num_glyphs -= i;
	    i = 0;

	    max_index = info->glyphs[0].index;
	    width = max_index < 256 ? 1 : max_index < 65536 ? 2 : 4;

	    request_size = 0;

	    x = y = 0;
	    glyphset_info = this_glyphset_info;
	}

	/* Convert absolute glyph position to relative-to-current-point
	 * position */
	info->glyphs[i].i.x = this_x - x;
	info->glyphs[i].i.y = this_y - y;

	/* Start a new element for the first glyph,
	 * or for any glyph that has unexpected position,
	 * or if current element has too many glyphs.
	 *
	 * These same conditions are mirrored in _emit_glyphs_chunk().
	 */
      if (_start_new_glyph_elt (i, &info->glyphs[i]))
	    request_size += _cairo_sz_x_glyph_elt_t;

	/* adjust current-position */
	x = this_x + glyph->x_advance;
	y = this_y + glyph->y_advance;

	request_size += width;
    }

    if (i) {
	status = _emit_glyphs_chunk (dst, op, src,
				     info->glyphs, i,
				     width, request_size,
				     glyphset_info,
				     info->use_mask ? glyphset_info->xrender_format : 0);
    }

    cairo_surface_destroy (&src->base);

    return status;
}

cairo_int_status_t
_cairo_xcb_render_compositor_glyphs (const cairo_compositor_t     *compositor,
				     cairo_composite_rectangles_t *composite,
				     cairo_scaled_font_t          *scaled_font,
				     cairo_glyph_t                *glyphs,
				     int                           num_glyphs,
				     cairo_bool_t                  overlap)
{
    cairo_xcb_surface_t *surface = (cairo_xcb_surface_t *) composite->surface;
    cairo_operator_t op = composite->op;
    cairo_pattern_t *source = &composite->source_pattern.base;
    cairo_int_status_t status;

    if (unlikely (! _operator_is_supported (surface->connection->flags, op)))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if ((surface->connection->flags & (CAIRO_XCB_RENDER_HAS_COMPOSITE_GLYPHS | CAIRO_XCB_RENDER_HAS_COMPOSITE)) == 0)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    status = CAIRO_INT_STATUS_UNSUPPORTED;
    if (surface->connection->flags & CAIRO_XCB_RENDER_HAS_COMPOSITE_GLYPHS) {
	_cairo_scaled_font_freeze_cache (scaled_font);

	status = _can_composite_glyphs (surface, &composite->bounded,
					scaled_font, glyphs, &num_glyphs);
	if (likely (status == CAIRO_INT_STATUS_SUCCESS)) {
	    composite_glyphs_info_t info;
	    unsigned flags = 0;

	    info.font = scaled_font;
	    info.glyphs = (cairo_xcb_glyph_t *) glyphs;
	    info.num_glyphs = num_glyphs;
	    info.use_mask =
		overlap ||
		! composite->is_bounded ||
		! _cairo_clip_is_region(composite->clip);

	    if (composite->mask.width  > composite->unbounded.width ||
		composite->mask.height > composite->unbounded.height)
	    {
		/* Glyphs are tricky since we do not directly control the
		 * geometry and their inked extents depend on the
		 * individual glyph-surface size. We must set a clip region
		 * so that the X server can trim the glyphs appropriately.
		 */
		flags |= FORCE_CLIP_REGION;
	    }
	    status = _clip_and_composite (surface, op, source,
					  _composite_glyphs, NULL,
					  &info, composite,
					  need_bounded_clip (composite) |
					  flags);
	}

	_cairo_scaled_font_thaw_cache (scaled_font);
    }

    if (status == CAIRO_INT_STATUS_UNSUPPORTED) {
	assert (surface->connection->flags & CAIRO_XCB_RENDER_HAS_COMPOSITE);
	status =
	    _cairo_xcb_surface_render_glyphs_via_mask (surface, op, source,
						       scaled_font, glyphs, num_glyphs,
						       composite);
    }

    return status;
}
