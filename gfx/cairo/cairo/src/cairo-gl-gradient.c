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
#include <stdint.h>
#include "cairo-error-private.h"
#include "cairo-gl-gradient-private.h"
#include "cairo-gl-private.h"


static int
_cairo_gl_gradient_sample_width (unsigned int                 n_stops,
				 const cairo_gradient_stop_t *stops)
{
    unsigned int n;
    int width;

    width = 8;
    for (n = 1; n < n_stops; n++) {
	double dx = stops[n].offset - stops[n-1].offset;
	double delta, max;
	int ramp;

	if (dx == 0)
	    return 1024; /* we need to emulate an infinitely sharp step */

	max = fabs (stops[n].color.red - stops[n-1].color.red);

	delta = fabs (stops[n].color.green - stops[n-1].color.green);
	if (delta > max)
	    max = delta;

	delta = fabs (stops[n].color.blue - stops[n-1].color.blue);
	if (delta > max)
	    max = delta;

	delta = fabs (stops[n].color.alpha - stops[n-1].color.alpha);
	if (delta > max)
	    max = delta;

	ramp = 128 * max / dx;
	if (ramp > width)
	    width = ramp;
    }

    return (width + 7) & -8;
}

static uint8_t premultiply(double c, double a)
{
    int v = c * a * 256;
    return v - (v >> 8);
}

static uint32_t color_stop_to_pixel(const cairo_gradient_stop_t *stop)
{
    uint8_t a, r, g, b;

    a = stop->color.alpha_short >> 8;
    r = premultiply(stop->color.red,   stop->color.alpha);
    g = premultiply(stop->color.green, stop->color.alpha);
    b = premultiply(stop->color.blue,  stop->color.alpha);

    if (_cairo_is_little_endian ())
	return (uint32_t)a << 24 | r << 16 | g << 8 | b << 0;
    else
	return a << 0 | r << 8 | g << 16 | (uint32_t)b << 24;
}

static cairo_status_t
_cairo_gl_gradient_render (const cairo_gl_context_t    *ctx,
			   unsigned int                 n_stops,
			   const cairo_gradient_stop_t *stops,
			   void                        *bytes,
			   int                          width)
{
    pixman_image_t *gradient, *image;
    pixman_gradient_stop_t pixman_stops_stack[32];
    pixman_gradient_stop_t *pixman_stops;
    pixman_point_fixed_t p1, p2;
    unsigned int i;
    pixman_format_code_t gradient_pixman_format;

    /*
     * Ensure that the order of the gradient's components in memory is BGRA.
     * This is done so that the gradient's pixel data is always suitable for
     * texture upload using format=GL_BGRA and type=GL_UNSIGNED_BYTE.
     */
    if (_cairo_is_little_endian ())
	gradient_pixman_format = PIXMAN_a8r8g8b8;
    else
	gradient_pixman_format = PIXMAN_b8g8r8a8;

    pixman_stops = pixman_stops_stack;
    if (unlikely (n_stops > ARRAY_LENGTH (pixman_stops_stack))) {
	pixman_stops = _cairo_malloc_ab (n_stops,
					 sizeof (pixman_gradient_stop_t));
	if (unlikely (pixman_stops == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    for (i = 0; i < n_stops; i++) {
	pixman_stops[i].x = _cairo_fixed_16_16_from_double (stops[i].offset);
	pixman_stops[i].color.red   = stops[i].color.red_short;
	pixman_stops[i].color.green = stops[i].color.green_short;
	pixman_stops[i].color.blue  = stops[i].color.blue_short;
	pixman_stops[i].color.alpha = stops[i].color.alpha_short;
    }

    p1.x = _cairo_fixed_16_16_from_double (0.5);
    p1.y = 0;
    p2.x = _cairo_fixed_16_16_from_double (width - 0.5);
    p2.y = 0;

    gradient = pixman_image_create_linear_gradient (&p1, &p2,
						    pixman_stops,
						    n_stops);
    if (pixman_stops != pixman_stops_stack)
	free (pixman_stops);

    if (unlikely (gradient == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    pixman_image_set_filter (gradient, PIXMAN_FILTER_BILINEAR, NULL, 0);
    pixman_image_set_repeat (gradient, PIXMAN_REPEAT_PAD);

    image = pixman_image_create_bits (gradient_pixman_format, width, 1,
				      bytes, sizeof(uint32_t)*width);
    if (unlikely (image == NULL)) {
	pixman_image_unref (gradient);
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    pixman_image_composite32 (PIXMAN_OP_SRC,
			      gradient, NULL, image,
			      0, 0,
			      0, 0,
			      0, 0,
			      width, 1);

    pixman_image_unref (gradient);
    pixman_image_unref (image);

    /* We need to fudge pixel 0 to hold the left-most color stop and not
     * the neareset stop to the zeroth pixel centre in order to correctly
     * populate the border color. For completeness, do both edges.
     */
    ((uint32_t*)bytes)[0] = color_stop_to_pixel(&stops[0]);
    ((uint32_t*)bytes)[width-1] = color_stop_to_pixel(&stops[n_stops-1]);

    return CAIRO_STATUS_SUCCESS;
}

static unsigned long
_cairo_gl_gradient_hash (unsigned int                  n_stops,
			 const cairo_gradient_stop_t  *stops)
{
    return _cairo_hash_bytes (n_stops,
			      stops,
			      sizeof (cairo_gradient_stop_t) * n_stops);
}

static cairo_gl_gradient_t *
_cairo_gl_gradient_lookup (cairo_gl_context_t           *ctx,
			   unsigned long                 hash,
			   unsigned int                  n_stops,
			   const cairo_gradient_stop_t  *stops)
{
    cairo_gl_gradient_t lookup;

    lookup.cache_entry.hash = hash,
    lookup.n_stops = n_stops;
    lookup.stops = stops;

    return _cairo_cache_lookup (&ctx->gradients, &lookup.cache_entry);
}

cairo_bool_t
_cairo_gl_gradient_equal (const void *key_a, const void *key_b)
{
    const cairo_gl_gradient_t *a = key_a;
    const cairo_gl_gradient_t *b = key_b;

    if (a->n_stops != b->n_stops)
	return FALSE;

    return memcmp (a->stops, b->stops, a->n_stops * sizeof (cairo_gradient_stop_t)) == 0;
}

cairo_int_status_t
_cairo_gl_gradient_create (cairo_gl_context_t           *ctx,
			   unsigned int                  n_stops,
			   const cairo_gradient_stop_t  *stops,
			   cairo_gl_gradient_t         **gradient_out)
{
    unsigned long hash;
    cairo_gl_gradient_t *gradient;
    cairo_status_t status;
    int tex_width;
    GLint internal_format;
    void *data;

    if ((unsigned int) ctx->max_texture_size / 2 <= n_stops)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    hash = _cairo_gl_gradient_hash (n_stops, stops);

    gradient = _cairo_gl_gradient_lookup (ctx, hash, n_stops, stops);
    if (gradient) {
	*gradient_out = _cairo_gl_gradient_reference (gradient);
	return CAIRO_STATUS_SUCCESS;
    }

    gradient = _cairo_malloc (sizeof (cairo_gl_gradient_t) + sizeof (cairo_gradient_stop_t) * (n_stops - 1));
    if (gradient == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    tex_width = _cairo_gl_gradient_sample_width (n_stops, stops);
    if (tex_width > ctx->max_texture_size)
	tex_width = ctx->max_texture_size;

    CAIRO_REFERENCE_COUNT_INIT (&gradient->ref_count, 2);
    gradient->cache_entry.hash = hash;
    gradient->cache_entry.size = tex_width;
    gradient->device = &ctx->base;
    gradient->n_stops = n_stops;
    gradient->stops = gradient->stops_embedded;
    memcpy (gradient->stops_embedded, stops, n_stops * sizeof (cairo_gradient_stop_t));

    glGenTextures (1, &gradient->tex);
    _cairo_gl_context_activate (ctx, CAIRO_GL_TEX_TEMP);
    glBindTexture (ctx->tex_target, gradient->tex);

    data = _cairo_malloc_ab (tex_width, sizeof (uint32_t));
    if (unlikely (data == NULL)) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto cleanup_gradient;
    }

    status = _cairo_gl_gradient_render (ctx, n_stops, stops, data, tex_width);
    if (unlikely (status))
	goto cleanup_data;

    /*
     * In OpenGL ES 2.0 no format conversion is allowed i.e. 'internalFormat'
     * must match 'format' in glTexImage2D.
     */
    if (_cairo_gl_get_flavor () == CAIRO_GL_FLAVOR_ES3 ||
	_cairo_gl_get_flavor () == CAIRO_GL_FLAVOR_ES2)
	internal_format = GL_BGRA;
    else
	internal_format = GL_RGBA;

    glTexImage2D (ctx->tex_target, 0, internal_format, tex_width, 1, 0,
		  GL_BGRA, GL_UNSIGNED_BYTE, data);

    free (data);

    /* we ignore errors here and just return an uncached gradient */
    if (unlikely (_cairo_cache_insert (&ctx->gradients, &gradient->cache_entry)))
	CAIRO_REFERENCE_COUNT_INIT (&gradient->ref_count, 1);

    *gradient_out = gradient;
    return CAIRO_STATUS_SUCCESS;

cleanup_data:
    free (data);
cleanup_gradient:
    free (gradient);
    return status;
}

cairo_gl_gradient_t *
_cairo_gl_gradient_reference (cairo_gl_gradient_t *gradient)
{
    assert (CAIRO_REFERENCE_COUNT_HAS_REFERENCE (&gradient->ref_count));

    _cairo_reference_count_inc (&gradient->ref_count);

    return gradient;
}

void
_cairo_gl_gradient_destroy (cairo_gl_gradient_t *gradient)
{
    cairo_gl_context_t *ctx;
    cairo_status_t ignore;

    assert (CAIRO_REFERENCE_COUNT_HAS_REFERENCE (&gradient->ref_count));

    if (! _cairo_reference_count_dec_and_test (&gradient->ref_count))
	return;

    if (_cairo_gl_context_acquire (gradient->device, &ctx) == CAIRO_STATUS_SUCCESS) {
	/* The gradient my still be active in the last operation, so flush */
	_cairo_gl_composite_flush (ctx);
	glDeleteTextures (1, &gradient->tex);
	ignore = _cairo_gl_context_release (ctx, CAIRO_STATUS_SUCCESS);
    }

    free (gradient);
}
