/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
 * Copyright © 2005 Red Hat, Inc.
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
#include "cairo-xlib-surface-private.h"

#include "cairo-error-private.h"
#include "cairo-image-surface-inline.h"
#include "cairo-paginated-private.h"
#include "cairo-pattern-inline.h"
#include "cairo-recording-surface-private.h"
#include "cairo-surface-backend-private.h"
#include "cairo-surface-offset-private.h"
#include "cairo-surface-observer-private.h"
#include "cairo-surface-snapshot-inline.h"
#include "cairo-surface-subsurface-inline.h"

#define PIXMAN_MAX_INT ((pixman_fixed_1 >> 1) - pixman_fixed_e) /* need to ensure deltas also fit */

static cairo_xlib_surface_t *
unwrap_source (const cairo_surface_pattern_t *pattern)
{
    cairo_rectangle_int_t limits;
    return (cairo_xlib_surface_t *)_cairo_pattern_get_source (pattern, &limits);
}

static cairo_status_t
_cairo_xlib_source_finish (void *abstract_surface)
{
    cairo_xlib_source_t *source = abstract_surface;

    XRenderFreePicture (source->dpy, source->picture);
    if (source->pixmap)
	    XFreePixmap (source->dpy, source->pixmap);
    return CAIRO_STATUS_SUCCESS;
}

static const cairo_surface_backend_t cairo_xlib_source_backend = {
    CAIRO_SURFACE_TYPE_XLIB,
    _cairo_xlib_source_finish,
    NULL, /* read-only wrapper */
};

static cairo_status_t
_cairo_xlib_proxy_finish (void *abstract_surface)
{
    cairo_xlib_proxy_t *proxy = abstract_surface;

    _cairo_xlib_shm_surface_mark_active (proxy->owner);
    XRenderFreePicture (proxy->source.dpy, proxy->source.picture);
    if (proxy->source.pixmap)
	    XFreePixmap (proxy->source.dpy, proxy->source.pixmap);
    cairo_surface_destroy (proxy->owner);
    return CAIRO_STATUS_SUCCESS;
}

static const cairo_surface_backend_t cairo_xlib_proxy_backend = {
    CAIRO_SURFACE_TYPE_XLIB,
    _cairo_xlib_proxy_finish,
    NULL, /* read-only wrapper */
};

static cairo_surface_t *
source (cairo_xlib_surface_t *dst, Picture picture, Pixmap pixmap)
{
    cairo_xlib_source_t *source;

    if (picture == None)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    source = _cairo_malloc (sizeof (*source));
    if (unlikely (source == NULL)) {
	XRenderFreePicture (dst->display->display, picture);
	if (pixmap)
		XFreePixmap (dst->display->display, pixmap);
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
    }

    _cairo_surface_init (&source->base,
			 &cairo_xlib_source_backend,
			 NULL, /* device */
			 CAIRO_CONTENT_COLOR_ALPHA,
			 FALSE); /* is_vector */

    /* The source exists only within an operation */
    source->picture = picture;
    source->pixmap = pixmap;
    source->dpy = dst->display->display;

    return &source->base;
}

static uint32_t
hars_petruska_f54_1_random (void)
{
#define rol(x,k) ((x << k) | (x >> (32-k)))
    static uint32_t x;
    return x = (x ^ rol (x, 5) ^ rol (x, 24)) + 0x37798849;
#undef rol
}

static const XTransform identity = {
    {
	{ 1 << 16, 0x00000, 0x00000 },
	{ 0x00000, 1 << 16, 0x00000 },
	{ 0x00000, 0x00000, 1 << 16 },
    }
};

static cairo_bool_t
picture_set_matrix (cairo_xlib_display_t *display,
		    Picture picture,
		    const cairo_matrix_t *matrix,
		    cairo_filter_t        filter,
		    double                xc,
		    double                yc,
		    int                  *x_offset,
		    int                  *y_offset)
{
    XTransform xtransform;
    pixman_transform_t *pixman_transform;
    cairo_int_status_t status;

    /* Casting between pixman_transform_t and XTransform is safe because
     * they happen to be the exact same type.
     */
    pixman_transform = (pixman_transform_t *) &xtransform;
    status = _cairo_matrix_to_pixman_matrix_offset (matrix, filter, xc, yc,
						    pixman_transform,
						    x_offset, y_offset);
    if (status == CAIRO_INT_STATUS_NOTHING_TO_DO)
	return TRUE;
    if (unlikely (status != CAIRO_INT_STATUS_SUCCESS))
	return FALSE;

    if (memcmp (&xtransform, &identity, sizeof (XTransform)) == 0)
	return TRUE;

    /* a late check in case we perturb the matrix too far */
    if (! CAIRO_RENDER_HAS_PICTURE_TRANSFORM (display))
	return FALSE;

    XRenderSetPictureTransform (display->display, picture, &xtransform);
    return TRUE;
}

static cairo_status_t
picture_set_filter (Display *dpy,
		    Picture picture,
		    cairo_filter_t filter)
{
    const char *render_filter;

    switch (filter) {
    case CAIRO_FILTER_FAST:
	render_filter = FilterFast;
	break;
    case CAIRO_FILTER_GOOD:
	render_filter = FilterGood;
	break;
    case CAIRO_FILTER_BEST:
	render_filter = FilterBest;
	break;
    case CAIRO_FILTER_NEAREST:
	render_filter = FilterNearest;
	break;
    case CAIRO_FILTER_BILINEAR:
	render_filter = FilterBilinear;
	break;
    case CAIRO_FILTER_GAUSSIAN:
	/* XXX: The GAUSSIAN value has no implementation in cairo
	 * whatsoever, so it was really a mistake to have it in the
	 * API. We could fix this by officially deprecating it, or
	 * else inventing semantics and providing an actual
	 * implementation for it. */
    default:
	render_filter = FilterBest;
	break;
    }

    XRenderSetPictureFilter (dpy, picture, (char *) render_filter, NULL, 0);
    return CAIRO_STATUS_SUCCESS;
}

static int
extend_to_repeat (cairo_extend_t extend)
{
    switch (extend) {
    default:
	ASSERT_NOT_REACHED;
    case CAIRO_EXTEND_NONE:
	return RepeatNone;
    case CAIRO_EXTEND_REPEAT:
	return RepeatNormal;
    case CAIRO_EXTEND_REFLECT:
	return RepeatReflect;
    case CAIRO_EXTEND_PAD:
	return RepeatPad;
    }
}

static cairo_bool_t
picture_set_properties (cairo_xlib_display_t *display,
			Picture picture,
			const cairo_pattern_t *pattern,
			const cairo_matrix_t *matrix,
			const cairo_rectangle_int_t *extents,
			int *x_off, int *y_off)
{
    XRenderPictureAttributes pa;
    int mask = 0;

    if (! picture_set_matrix (display, picture, matrix, pattern->filter,
			      extents->x + extents->width / 2,
			      extents->y + extents->height / 2,
			      x_off, y_off))
	return FALSE;

    picture_set_filter (display->display, picture, pattern->filter);

    if (pattern->has_component_alpha) {
	pa.component_alpha = 1;
	mask |= CPComponentAlpha;
    }

    if (pattern->extend != CAIRO_EXTEND_NONE) {
	pa.repeat = extend_to_repeat (pattern->extend);
	mask |= CPRepeat;
    }

    if (mask)
	XRenderChangePicture (display->display, picture, mask, &pa);

    return TRUE;
}

static cairo_surface_t *
render_pattern (cairo_xlib_surface_t *dst,
		const cairo_pattern_t *pattern,
		cairo_bool_t is_mask,
		const cairo_rectangle_int_t *extents,
		int *src_x, int *src_y)
{
    Display *dpy = dst->display->display;
    cairo_xlib_surface_t *src;
    cairo_image_surface_t *image;
    cairo_status_t status;
    cairo_rectangle_int_t map_extents;

    src = (cairo_xlib_surface_t *)
	_cairo_surface_create_scratch (&dst->base,
				       is_mask ? CAIRO_CONTENT_ALPHA : CAIRO_CONTENT_COLOR_ALPHA,
				       extents->width,
				       extents->height,
				       NULL);
    if (src->base.type != CAIRO_SURFACE_TYPE_XLIB) {
	cairo_surface_destroy (&src->base);
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
    }

    map_extents = *extents;
    map_extents.x = map_extents.y = 0;

    image = _cairo_surface_map_to_image (&src->base, &map_extents);
    status = _cairo_surface_offset_paint (&image->base, extents->x, extents->y,
					  CAIRO_OPERATOR_SOURCE, pattern,
					  NULL);
    status = _cairo_surface_unmap_image (&src->base, image);
    if (unlikely (status)) {
	cairo_surface_destroy (&src->base);
	return _cairo_surface_create_in_error (status);
    }

    status = _cairo_xlib_surface_put_shm (src);
    if (unlikely (status)) {
	cairo_surface_destroy (&src->base);
	return _cairo_surface_create_in_error (status);
    }

    src->picture = XRenderCreatePicture (dpy,
					 src->drawable, src->xrender_format,
					 0, NULL);

    *src_x = -extents->x;
    *src_y = -extents->y;
    return &src->base;
}

static cairo_surface_t *
gradient_source (cairo_xlib_surface_t *dst,
		 const cairo_gradient_pattern_t *gradient,
		 cairo_bool_t is_mask,
		 const cairo_rectangle_int_t *extents,
		 int *src_x, int *src_y)
{
    cairo_xlib_display_t *display = dst->display;
    cairo_matrix_t matrix = gradient->base.matrix;
    char buf[CAIRO_STACK_BUFFER_SIZE];
    cairo_circle_double_t extremes[2];
    XFixed *stops;
    XRenderColor *colors;
    Picture picture;
    unsigned int i, n_stops;

    /* The RENDER specification says that the inner circle has
     * to be completely contained inside the outer one. */
    if (gradient->base.type == CAIRO_PATTERN_TYPE_RADIAL &&
	! _cairo_radial_pattern_focus_is_inside ((cairo_radial_pattern_t *) gradient))
	return render_pattern (dst, &gradient->base, is_mask, extents, src_x, src_y);

    assert (gradient->n_stops > 0);
    n_stops = MAX (gradient->n_stops, 2);

    if (n_stops < sizeof (buf) / (sizeof (XFixed) + sizeof (XRenderColor)))
    {
	stops = (XFixed *) buf;
    }
    else
    {
	stops =
	    _cairo_malloc_ab (n_stops,
			      sizeof (XFixed) + sizeof (XRenderColor));
	if (unlikely (stops == NULL))
	    return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
    }

    colors = (XRenderColor *) (stops + n_stops);
    for (i = 0; i < gradient->n_stops; i++) {
	stops[i] =
	    _cairo_fixed_16_16_from_double (gradient->stops[i].offset);

	colors[i].red   = gradient->stops[i].color.red_short;
	colors[i].green = gradient->stops[i].color.green_short;
	colors[i].blue  = gradient->stops[i].color.blue_short;
	colors[i].alpha = gradient->stops[i].color.alpha_short;
    }

    /* RENDER does not support gradients with less than 2
     * stops. If a gradient has only a single stop, duplicate
     * it to make RENDER happy. */
    if (gradient->n_stops == 1) {
	stops[1] =
	    _cairo_fixed_16_16_from_double (gradient->stops[0].offset);

	colors[1].red   = gradient->stops[0].color.red_short;
	colors[1].green = gradient->stops[0].color.green_short;
	colors[1].blue  = gradient->stops[0].color.blue_short;
	colors[1].alpha = gradient->stops[0].color.alpha_short;
    }

#if 0
    /* For some weird reason the X server is sometimes getting
     * CreateGradient requests with bad length. So far I've only seen
     * XRenderCreateLinearGradient request with 4 stops sometime end up
     * with length field matching 0 stops at the server side. I've
     * looked at the libXrender code and I can't see anything that
     * could cause this behavior. However, for some reason having a
     * XSync call here seems to avoid the issue so I'll keep it here
     * until it's solved.
     */
    XSync (display->display, False);
#endif

    _cairo_gradient_pattern_fit_to_range (gradient, PIXMAN_MAX_INT >> 1, &matrix, extremes);

    if (gradient->base.type == CAIRO_PATTERN_TYPE_LINEAR) {
	XLinearGradient grad;

	grad.p1.x = _cairo_fixed_16_16_from_double (extremes[0].center.x);
	grad.p1.y = _cairo_fixed_16_16_from_double (extremes[0].center.y);
	grad.p2.x = _cairo_fixed_16_16_from_double (extremes[1].center.x);
	grad.p2.y = _cairo_fixed_16_16_from_double (extremes[1].center.y);

	picture = XRenderCreateLinearGradient (display->display, &grad,
					       stops, colors,
					       n_stops);
    } else {
	XRadialGradient grad;

	grad.inner.x      = _cairo_fixed_16_16_from_double (extremes[0].center.x);
	grad.inner.y      = _cairo_fixed_16_16_from_double (extremes[0].center.y);
	grad.inner.radius = _cairo_fixed_16_16_from_double (extremes[0].radius);
	grad.outer.x      = _cairo_fixed_16_16_from_double (extremes[1].center.x);
	grad.outer.y      = _cairo_fixed_16_16_from_double (extremes[1].center.y);
	grad.outer.radius = _cairo_fixed_16_16_from_double (extremes[1].radius);

	picture = XRenderCreateRadialGradient (display->display, &grad,
					       stops, colors,
					       n_stops);
    }

    if (stops != (XFixed *) buf)
	free (stops);

    *src_x = *src_y = 0;
    if (! picture_set_properties (display, picture,
				  &gradient->base, &gradient->base.matrix,
				  extents,
				  src_x, src_y)) {
	XRenderFreePicture (display->display, picture);
	return render_pattern (dst, &gradient->base, is_mask, extents, src_x, src_y);
    }

    return source (dst, picture, None);
}

static cairo_surface_t *
color_source (cairo_xlib_surface_t *dst, const cairo_color_t *color)
{
    Display *dpy = dst->display->display;
    XRenderColor xcolor;
    Picture picture;
    Pixmap pixmap = None;

    xcolor.red   = color->red_short;
    xcolor.green = color->green_short;
    xcolor.blue  = color->blue_short;
    xcolor.alpha = color->alpha_short;

    if (CAIRO_RENDER_HAS_GRADIENTS(dst->display)) {
	picture = XRenderCreateSolidFill (dpy, &xcolor);
    } else {
	XRenderPictureAttributes pa;
	int mask = 0;

	pa.repeat = RepeatNormal;
	mask |= CPRepeat;

	pixmap = XCreatePixmap (dpy, dst->drawable, 1, 1, 32);
	picture = XRenderCreatePicture (dpy, pixmap,
					_cairo_xlib_display_get_xrender_format (dst->display, CAIRO_FORMAT_ARGB32),
					mask, &pa);

	if (CAIRO_RENDER_HAS_FILL_RECTANGLES(dst->display)) {
	    XRectangle r = { 0, 0, 1, 1};
	    XRenderFillRectangles (dpy, PictOpSrc, picture, &xcolor, &r, 1);
	} else {
	    XGCValues gcv;
	    GC gc;

	    gc = _cairo_xlib_screen_get_gc (dst->display, dst->screen,
					    32, pixmap);
	    if (unlikely (gc == NULL)) {
		XFreePixmap (dpy, pixmap);
		return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
	    }

	    gcv.foreground = 0;
	    gcv.foreground |= (uint32_t)color->alpha_short >> 8 << 24;
	    gcv.foreground |= color->red_short   >> 8 << 16;
	    gcv.foreground |= color->green_short >> 8 << 8;
	    gcv.foreground |= color->blue_short  >> 8 << 0;
	    gcv.fill_style = FillSolid;

	    XChangeGC (dpy, gc, GCFillStyle | GCForeground, &gcv);
	    XFillRectangle (dpy, pixmap, gc, 0, 0, 1, 1);

	    _cairo_xlib_screen_put_gc (dst->display, dst->screen, 32, gc);
	}
    }

    return source (dst, picture, pixmap);
}

static cairo_surface_t *
alpha_source (cairo_xlib_surface_t *dst, uint8_t alpha)
{
    cairo_xlib_display_t *display = dst->display;

    if (display->alpha[alpha] == NULL) {
	cairo_color_t color;

	color.red_short = color.green_short = color.blue_short = 0;
	color.alpha_short = alpha << 8 | alpha;

	display->alpha[alpha] = color_source (dst, &color);
    }

    return cairo_surface_reference (display->alpha[alpha]);
}

static cairo_surface_t *
white_source (cairo_xlib_surface_t *dst)
{
    cairo_xlib_display_t *display = dst->display;

    if (display->white == NULL)
	display->white = color_source (dst, CAIRO_COLOR_WHITE);

    return cairo_surface_reference (display->white);
}

static cairo_surface_t *
opaque_source (cairo_xlib_surface_t *dst, const cairo_color_t *color)
{
    cairo_xlib_display_t *display = dst->display;
    uint32_t pixel =
	0xff000000 |
	color->red_short   >> 8 << 16 |
	color->green_short >> 8 << 8 |
	color->blue_short  >> 8 << 0;
    int i;

    if (display->last_solid_cache[0].color == pixel)
	return cairo_surface_reference (display->solid[display->last_solid_cache[0].index]);

    for (i = 0; i < 16; i++) {
	if (display->solid_cache[i] == pixel)
	    goto done;
    }

    i = hars_petruska_f54_1_random () % 16;
    cairo_surface_destroy (display->solid[i]);

    display->solid[i] = color_source (dst, color);
    display->solid_cache[i] = pixel;

done:
    display->last_solid_cache[0].color = pixel;
    display->last_solid_cache[0].index = i;
    return cairo_surface_reference (display->solid[i]);
}

static cairo_surface_t *
transparent_source (cairo_xlib_surface_t *dst, const cairo_color_t *color)
{
    cairo_xlib_display_t *display = dst->display;
    uint32_t pixel =
	(uint32_t)color->alpha_short >> 8 << 24 |
	color->red_short   >> 8 << 16 |
	color->green_short >> 8 << 8 |
	color->blue_short  >> 8 << 0;
    int i;

    if (display->last_solid_cache[1].color == pixel) {
    assert (display->solid[display->last_solid_cache[1].index]);
	return cairo_surface_reference (display->solid[display->last_solid_cache[1].index]);
    }

    for (i = 16; i < 32; i++) {
	if (display->solid_cache[i] == pixel)
	    goto done;
    }

    i = 16 + (hars_petruska_f54_1_random () % 16);
    cairo_surface_destroy (display->solid[i]);

    display->solid[i] = color_source (dst, color);
    display->solid_cache[i] = pixel;

done:
    display->last_solid_cache[1].color = pixel;
    display->last_solid_cache[1].index = i;
    assert (display->solid[i]);
    return cairo_surface_reference (display->solid[i]);
}

static cairo_surface_t *
solid_source (cairo_xlib_surface_t *dst,
	      const cairo_color_t *color)
{
    if ((color->red_short | color->green_short | color->blue_short) <= 0xff)
	return alpha_source (dst, color->alpha_short >> 8);

    if (CAIRO_ALPHA_SHORT_IS_OPAQUE (color->alpha_short)) {
	if (color->red_short >= 0xff00 && color->green_short >= 0xff00 && color->blue_short >= 0xff00)
	    return white_source (dst);

	return opaque_source (dst, color);
    } else
	return transparent_source (dst, color);
}

static cairo_xlib_source_t *init_source (cairo_xlib_surface_t *dst,
					 cairo_xlib_surface_t *src)
{
    Display *dpy = dst->display->display;
    cairo_xlib_source_t *source = &src->embedded_source;

    /* As these are frequent and meant to be fast, we track pictures for
     * native surface and minimise update requests.
     */
    if (source->picture == None) {
	XRenderPictureAttributes pa;

	_cairo_surface_init (&source->base,
			     &cairo_xlib_source_backend,
			     NULL, /* device */
			     CAIRO_CONTENT_COLOR_ALPHA,
			     FALSE); /* is_vector */

	pa.subwindow_mode = IncludeInferiors;
	source->picture = XRenderCreatePicture (dpy,
						src->drawable,
						src->xrender_format,
						CPSubwindowMode, &pa);

	source->has_component_alpha = 0;
	source->has_matrix = 0;
	source->filter = CAIRO_FILTER_NEAREST;
	source->extend = CAIRO_EXTEND_NONE;
    }

    return (cairo_xlib_source_t *) cairo_surface_reference (&source->base);
}

static cairo_surface_t *
embedded_source (cairo_xlib_surface_t *dst,
		 const cairo_surface_pattern_t *pattern,
		 const cairo_rectangle_int_t *extents,
		 int *src_x, int *src_y,
		 cairo_xlib_source_t *source)
{
    Display *dpy = dst->display->display;
    cairo_int_status_t status;
    XTransform xtransform;
    XRenderPictureAttributes pa;
    unsigned mask = 0;

    status = _cairo_matrix_to_pixman_matrix_offset (&pattern->base.matrix,
						    pattern->base.filter,
						    extents->x + extents->width / 2,
						    extents->y + extents->height / 2,
						    (pixman_transform_t *)&xtransform,
						    src_x, src_y);

    if (status == CAIRO_INT_STATUS_NOTHING_TO_DO) {
	if (source->has_matrix) {
	    source->has_matrix = 0;
	    memcpy (&xtransform, &identity, sizeof (identity));
	    status = CAIRO_INT_STATUS_SUCCESS;
	}
    } else
	source->has_matrix = 1;
    if (status == CAIRO_INT_STATUS_SUCCESS)
	XRenderSetPictureTransform (dpy, source->picture, &xtransform);

    if (source->filter != pattern->base.filter) {
	picture_set_filter (dpy, source->picture, pattern->base.filter);
	source->filter = pattern->base.filter;
    }

    if (source->has_component_alpha != pattern->base.has_component_alpha) {
	pa.component_alpha = pattern->base.has_component_alpha;
	mask |= CPComponentAlpha;
	source->has_component_alpha = pattern->base.has_component_alpha;
    }

    if (source->extend != pattern->base.extend) {
	pa.repeat = extend_to_repeat (pattern->base.extend);
	mask |= CPRepeat;
	source->extend = pattern->base.extend;
    }

    if (mask)
	XRenderChangePicture (dpy, source->picture, mask, &pa);

    return &source->base;
}

static cairo_surface_t *
subsurface_source (cairo_xlib_surface_t *dst,
		   const cairo_surface_pattern_t *pattern,
		   cairo_bool_t is_mask,
		   const cairo_rectangle_int_t *extents,
		   const cairo_rectangle_int_t *sample,
		   int *src_x, int *src_y)
{
    cairo_surface_subsurface_t *sub;
    cairo_xlib_surface_t *src;
    cairo_xlib_source_t *source;
    Display *dpy = dst->display->display;
    cairo_int_status_t status;
    cairo_surface_pattern_t local_pattern;
    XTransform xtransform;
    XRenderPictureAttributes pa;
    unsigned mask = 0;

    sub = (cairo_surface_subsurface_t *) pattern->surface;

    if (sample->x >= 0 && sample->y >= 0 &&
	sample->x + sample->width  <= sub->extents.width &&
	sample->y + sample->height <= sub->extents.height)
    {
	src = (cairo_xlib_surface_t *) sub->target;
	status = _cairo_surface_flush (&src->base, 0);
	if (unlikely (status))
	    return _cairo_surface_create_in_error (status);

	if (pattern->base.filter == CAIRO_FILTER_NEAREST &&
	    _cairo_matrix_is_translation (&pattern->base.matrix))
	{
	    *src_x += pattern->base.matrix.x0 + sub->extents.x;
	    *src_y += pattern->base.matrix.y0 + sub->extents.y;

	    _cairo_xlib_surface_ensure_picture (src);
	    return cairo_surface_reference (&src->base);
	}
	else
	{
	    cairo_surface_pattern_t local_pattern = *pattern;
	    local_pattern.base.matrix.x0 += sub->extents.x;
	    local_pattern.base.matrix.y0 += sub->extents.y;
	    local_pattern.base.extend = CAIRO_EXTEND_NONE;
	    return embedded_source (dst, &local_pattern, extents,
				    src_x, src_y, init_source (dst, src));
	}
    }

    if (sub->snapshot && sub->snapshot->type == CAIRO_SURFACE_TYPE_XLIB) {
	src = (cairo_xlib_surface_t *) cairo_surface_reference (sub->snapshot);
	source = &src->embedded_source;
    } else {
	src = (cairo_xlib_surface_t *)
	    _cairo_surface_create_scratch (&dst->base,
					   sub->base.content,
					   sub->extents.width,
					   sub->extents.height,
					   NULL);
	if (src->base.type != CAIRO_SURFACE_TYPE_XLIB) {
	    cairo_surface_destroy (&src->base);
	    return _cairo_surface_create_in_error (CAIRO_STATUS_NO_MEMORY);
	}

	_cairo_pattern_init_for_surface (&local_pattern, sub->target);
	cairo_matrix_init_translate (&local_pattern.base.matrix,
				     sub->extents.x, sub->extents.y);
	local_pattern.base.filter = CAIRO_FILTER_NEAREST;
	status = _cairo_surface_paint (&src->base,
				       CAIRO_OPERATOR_SOURCE,
				       &local_pattern.base,
				       NULL);
	_cairo_pattern_fini (&local_pattern.base);

	if (unlikely (status)) {
	    cairo_surface_destroy (&src->base);
	    return _cairo_surface_create_in_error (status);
	}

	_cairo_xlib_surface_ensure_picture (src);
	_cairo_surface_subsurface_set_snapshot (&sub->base, &src->base);

	source = &src->embedded_source;
	source->has_component_alpha = 0;
	source->has_matrix = 0;
	source->filter = CAIRO_FILTER_NEAREST;
	source->extend = CAIRO_EXTEND_NONE;
    }

    status = _cairo_matrix_to_pixman_matrix_offset (&pattern->base.matrix,
						    pattern->base.filter,
						    extents->x + extents->width / 2,
						    extents->y + extents->height / 2,
						    (pixman_transform_t *)&xtransform,
						    src_x, src_y);
    if (status == CAIRO_INT_STATUS_NOTHING_TO_DO) {
	if (source->has_matrix) {
	    source->has_matrix = 0;
	    memcpy (&xtransform, &identity, sizeof (identity));
	    status = CAIRO_INT_STATUS_SUCCESS;
	}
    } else
	source->has_matrix = 1;
    if (status == CAIRO_INT_STATUS_SUCCESS)
	XRenderSetPictureTransform (dpy, src->picture, &xtransform);

    if (source->filter != pattern->base.filter) {
	picture_set_filter (dpy, src->picture, pattern->base.filter);
	source->filter = pattern->base.filter;
    }

    if (source->has_component_alpha != pattern->base.has_component_alpha) {
	pa.component_alpha = pattern->base.has_component_alpha;
	mask |= CPComponentAlpha;
	source->has_component_alpha = pattern->base.has_component_alpha;
    }

    if (source->extend != pattern->base.extend) {
	pa.repeat = extend_to_repeat (pattern->base.extend);
	mask |= CPRepeat;
	source->extend = pattern->base.extend;
    }

    if (mask)
	XRenderChangePicture (dpy, src->picture, mask, &pa);

    return &src->base;
}

static cairo_surface_t *
native_source (cairo_xlib_surface_t *dst,
	       const cairo_surface_pattern_t *pattern,
	       cairo_bool_t is_mask,
	       const cairo_rectangle_int_t *extents,
	       const cairo_rectangle_int_t *sample,
	       int *src_x, int *src_y)
{
    cairo_xlib_surface_t *src;
    cairo_int_status_t status;

    if (_cairo_surface_is_subsurface (pattern->surface))
	return subsurface_source (dst, pattern, is_mask,
				  extents, sample,
				  src_x, src_y);

    src = unwrap_source (pattern);
    status = _cairo_surface_flush (&src->base, 0);
    if (unlikely (status))
	return _cairo_surface_create_in_error (status);

    if (pattern->base.filter == CAIRO_FILTER_NEAREST &&
	sample->x >= 0 && sample->y >= 0 &&
	sample->x + sample->width  <= src->width &&
	sample->y + sample->height <= src->height &&
	_cairo_matrix_is_translation (&pattern->base.matrix))
    {
	*src_x += pattern->base.matrix.x0;
	*src_y += pattern->base.matrix.y0;
	_cairo_xlib_surface_ensure_picture (src);
	return cairo_surface_reference (&src->base);
    }

    return embedded_source (dst, pattern, extents, src_x, src_y,
			    init_source (dst, src));
}

static cairo_surface_t *
recording_pattern_get_surface (const cairo_pattern_t *pattern)
{
    cairo_surface_t *surface;

    surface = ((const cairo_surface_pattern_t *) pattern)->surface;

    if (_cairo_surface_is_paginated (surface))
	return cairo_surface_reference (_cairo_paginated_surface_get_recording (surface));

    if (_cairo_surface_is_snapshot (surface))
	return _cairo_surface_snapshot_get_target (surface);

    return cairo_surface_reference (surface);
}

static cairo_surface_t *
record_source (cairo_xlib_surface_t *dst,
	       const cairo_surface_pattern_t *pattern,
	       cairo_bool_t is_mask,
	       const cairo_rectangle_int_t *extents,
	       const cairo_rectangle_int_t *sample,
	       int *src_x, int *src_y)
{
    cairo_xlib_surface_t *src;
    cairo_surface_t *recording;
    cairo_matrix_t matrix, m;
    cairo_status_t status;
    cairo_rectangle_int_t upload, limit;

    upload = *sample;
    if (_cairo_surface_get_extents (pattern->surface, &limit) &&
	! _cairo_rectangle_intersect (&upload, &limit))
    {
	if (pattern->base.extend == CAIRO_EXTEND_NONE)
	    return alpha_source (dst, 0);

	upload = limit;
    }

    src = (cairo_xlib_surface_t *)
	_cairo_surface_create_scratch (&dst->base,
				       pattern->surface->content,
				       upload.width,
				       upload.height,
				       NULL);
    if (src->base.type != CAIRO_SURFACE_TYPE_XLIB) {
	cairo_surface_destroy (&src->base);
	return _cairo_surface_create_in_error (CAIRO_STATUS_NO_MEMORY);
    }

    cairo_matrix_init_translate (&matrix, upload.x, upload.y);
    recording = recording_pattern_get_surface (&pattern->base),
    status = _cairo_recording_surface_replay_with_clip (recording,
							&matrix, &src->base,
							NULL);
    cairo_surface_destroy (recording);
    if (unlikely (status)) {
	cairo_surface_destroy (&src->base);
	return _cairo_surface_create_in_error (status);
    }

    matrix = pattern->base.matrix;
    if (upload.x | upload.y) {
	cairo_matrix_init_translate (&m, -upload.x, -upload.y);
	cairo_matrix_multiply (&matrix, &matrix, &m);
    }

    _cairo_xlib_surface_ensure_picture (src);
    if (! picture_set_properties (src->display, src->picture,
				  &pattern->base, &matrix, extents,
				  src_x, src_y))
    {
	cairo_surface_destroy (&src->base);
	return render_pattern (dst, &pattern->base, is_mask,
			       extents, src_x, src_y);
    }

    return &src->base;
}

static cairo_surface_t *
surface_source (cairo_xlib_surface_t *dst,
		const cairo_surface_pattern_t *pattern,
		cairo_bool_t is_mask,
		const cairo_rectangle_int_t *extents,
		const cairo_rectangle_int_t *sample,
		int *src_x, int *src_y)
{
    cairo_surface_t *src;
    cairo_xlib_surface_t *xsrc;
    cairo_surface_pattern_t local_pattern;
    cairo_status_t status;
    cairo_rectangle_int_t upload, limit;

    src = pattern->surface;
    if (src->type == CAIRO_SURFACE_TYPE_IMAGE &&
	src->device == dst->base.device &&
	_cairo_xlib_shm_surface_get_pixmap (src)) {
	cairo_xlib_proxy_t *proxy;

	proxy = _cairo_malloc (sizeof(*proxy));
	if (unlikely (proxy == NULL))
	    return _cairo_surface_create_in_error (CAIRO_STATUS_NO_MEMORY);

	_cairo_surface_init (&proxy->source.base,
			     &cairo_xlib_proxy_backend,
			     dst->base.device,
			     src->content,
			     src->is_vector);

	proxy->source.dpy = dst->display->display;
	proxy->source.picture = XRenderCreatePicture (proxy->source.dpy,
						      _cairo_xlib_shm_surface_get_pixmap (src),
						      _cairo_xlib_shm_surface_get_xrender_format (src),
						      0, NULL);
	proxy->source.pixmap = None;

	proxy->source.has_component_alpha = 0;
	proxy->source.has_matrix = 0;
	proxy->source.filter = CAIRO_FILTER_NEAREST;
	proxy->source.extend = CAIRO_EXTEND_NONE;
	proxy->owner = cairo_surface_reference (src);

	return embedded_source (dst, pattern, extents, src_x, src_y,
				&proxy->source);
    }

    upload = *sample;
    if (_cairo_surface_get_extents (pattern->surface, &limit)) {
	if (pattern->base.extend == CAIRO_EXTEND_NONE) {
	    if (! _cairo_rectangle_intersect (&upload, &limit))
		return alpha_source (dst, 0);
	} else if (pattern->base.extend == CAIRO_EXTEND_PAD) {
	    if (! _cairo_rectangle_intersect (&upload, &limit))
		upload = limit;
	} else {
	    if (upload.x < limit.x ||
		upload.x + upload.width > limit.x + limit.width ||
		upload.y < limit.y ||
		upload.y + upload.height > limit.y + limit.height)
	    {
		upload = limit;
	    }
	}
    }

    xsrc = (cairo_xlib_surface_t *)
	    _cairo_surface_create_scratch (&dst->base,
					   src->content,
					   upload.width,
					   upload.height,
					   NULL);
    if (xsrc->base.type != CAIRO_SURFACE_TYPE_XLIB) {
	cairo_surface_destroy (src);
	cairo_surface_destroy (&xsrc->base);
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
    }

    if (_cairo_surface_is_image (src)) {
	status = _cairo_xlib_surface_draw_image (xsrc, (cairo_image_surface_t *)src,
						 upload.x, upload.y,
						 upload.width, upload.height,
						 0, 0);
    } else {
	cairo_image_surface_t *image;
	cairo_rectangle_int_t map_extents = { 0,0, upload.width,upload.height };

	image = _cairo_surface_map_to_image (&xsrc->base, &map_extents);

	_cairo_pattern_init_for_surface (&local_pattern, pattern->surface);
	cairo_matrix_init_translate (&local_pattern.base.matrix,
				     upload.x, upload.y);

	status = _cairo_surface_paint (&image->base,
				       CAIRO_OPERATOR_SOURCE,
				       &local_pattern.base,
				       NULL);
	_cairo_pattern_fini (&local_pattern.base);

	status = _cairo_surface_unmap_image (&xsrc->base, image);
	if (unlikely (status)) {
	    cairo_surface_destroy (&xsrc->base);
	    return _cairo_surface_create_in_error (status);
	}

	status = _cairo_xlib_surface_put_shm (xsrc);
	if (unlikely (status)) {
	    cairo_surface_destroy (&xsrc->base);
	    return _cairo_surface_create_in_error (status);
	}
    }

    _cairo_pattern_init_static_copy (&local_pattern.base, &pattern->base);
    if (upload.x | upload.y) {
	cairo_matrix_t m;
	cairo_matrix_init_translate (&m, -upload.x, -upload.y);
	cairo_matrix_multiply (&local_pattern.base.matrix,
			       &local_pattern.base.matrix,
			       &m);
    }

    *src_x = *src_y = 0;
    _cairo_xlib_surface_ensure_picture (xsrc);
    if (! picture_set_properties (xsrc->display,
				  xsrc->picture,
				  &local_pattern.base,
				  &local_pattern.base.matrix,
				  extents,
				  src_x, src_y))
    {
	cairo_surface_destroy (&xsrc->base);
	return render_pattern (dst, &pattern->base,
			       is_mask, extents,
			       src_x, src_y);
    }

    return &xsrc->base;
}

static cairo_bool_t
pattern_is_supported (cairo_xlib_display_t *display,
		      const cairo_pattern_t *pattern)
{
    if (pattern->type == CAIRO_PATTERN_TYPE_MESH)
	return FALSE;

    if (display->buggy_pad_reflect) {
	if (pattern->extend == CAIRO_EXTEND_REPEAT || pattern->extend == CAIRO_EXTEND_PAD)
	    return FALSE;
    }

    if (display->buggy_gradients) {
	if (pattern->type == CAIRO_PATTERN_TYPE_LINEAR || pattern->type == CAIRO_PATTERN_TYPE_RADIAL)
	    return FALSE;
    }

    switch (pattern->filter) {
    case CAIRO_FILTER_FAST:
    case CAIRO_FILTER_NEAREST:
	return CAIRO_RENDER_HAS_PICTURE_TRANSFORM (display) ||
	    _cairo_matrix_is_integer_translation (&pattern->matrix, NULL, NULL);
    case CAIRO_FILTER_GOOD:
	return CAIRO_RENDER_HAS_FILTER_GOOD (display);
    case CAIRO_FILTER_BEST:
	return CAIRO_RENDER_HAS_FILTER_BEST (display);
    case CAIRO_FILTER_BILINEAR:
    case CAIRO_FILTER_GAUSSIAN:
    default:
	return CAIRO_RENDER_HAS_FILTERS (display);
    }
}

cairo_surface_t *
_cairo_xlib_source_create_for_pattern (cairo_surface_t *_dst,
				       const cairo_pattern_t *pattern,
				       cairo_bool_t is_mask,
				       const cairo_rectangle_int_t *extents,
				       const cairo_rectangle_int_t *sample,
				       int *src_x, int *src_y)
{
    cairo_xlib_surface_t *dst = (cairo_xlib_surface_t *)_dst;

    *src_x = *src_y = 0;

    if (pattern == NULL || pattern->type == CAIRO_PATTERN_TYPE_SOLID) {
	if (pattern == NULL)
	    pattern = &_cairo_pattern_white.base;

	return solid_source (dst, &((cairo_solid_pattern_t *)pattern)->color);
    }

    if (pattern_is_supported (dst->display, pattern)) {
	if (pattern->type == CAIRO_PATTERN_TYPE_SURFACE) {
	    cairo_surface_pattern_t *spattern = (cairo_surface_pattern_t *)pattern;
	    if (spattern->surface->type == CAIRO_SURFACE_TYPE_XLIB &&
		_cairo_xlib_surface_same_screen (dst,
						 unwrap_source (spattern)))
		return native_source (dst, spattern, is_mask,
				      extents, sample,
				      src_x, src_y);

	    if (spattern->surface->type == CAIRO_SURFACE_TYPE_RECORDING)
		return record_source (dst, spattern, is_mask,
				      extents, sample,
				      src_x, src_y);

	    return surface_source (dst, spattern, is_mask,
				   extents, sample,
				   src_x, src_y);
	}

	if (pattern->type == CAIRO_PATTERN_TYPE_LINEAR ||
	    pattern->type == CAIRO_PATTERN_TYPE_RADIAL)
	{
	    cairo_gradient_pattern_t *gpattern = (cairo_gradient_pattern_t *)pattern;
	    return gradient_source (dst, gpattern, is_mask, extents, src_x, src_y);
	}
    }

    return render_pattern (dst, pattern, is_mask, extents, src_x, src_y);
}

#endif /* !CAIRO_HAS_XLIB_XCB_FUNCTIONS */
