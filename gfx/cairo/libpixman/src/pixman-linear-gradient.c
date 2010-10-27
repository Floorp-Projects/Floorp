/*
 * Copyright © 2000 SuSE, Inc.
 * Copyright © 2007 Red Hat, Inc.
 * Copyright © 2000 Keith Packard, member of The XFree86 Project, Inc.
 *             2005 Lars Knoll & Zack Rusin, Trolltech
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdlib.h>
#include "pixman-private.h"

static source_image_class_t
linear_gradient_classify (pixman_image_t *image,
                          int             x,
                          int             y,
                          int             width,
                          int             height)
{
    linear_gradient_t *linear = (linear_gradient_t *)image;
    pixman_vector_t v;
    pixman_fixed_32_32_t l;
    pixman_fixed_48_16_t dx, dy, a, b, off;
    pixman_fixed_48_16_t factors[4];
    int i;

    image->source.class = SOURCE_IMAGE_CLASS_UNKNOWN;

    dx = linear->p2.x - linear->p1.x;
    dy = linear->p2.y - linear->p1.y;

    l = dx * dx + dy * dy;

    if (l)
    {
	a = (dx << 32) / l;
	b = (dy << 32) / l;
    }
    else
    {
	a = b = 0;
    }

    off = (-a * linear->p1.x
           -b * linear->p1.y) >> 16;

    for (i = 0; i < 3; i++)
    {
	v.vector[0] = pixman_int_to_fixed ((i % 2) * (width  - 1) + x);
	v.vector[1] = pixman_int_to_fixed ((i / 2) * (height - 1) + y);
	v.vector[2] = pixman_fixed_1;

	if (image->common.transform)
	{
	    if (!pixman_transform_point_3d (image->common.transform, &v))
	    {
		image->source.class = SOURCE_IMAGE_CLASS_UNKNOWN;

		return image->source.class;
	    }
	}

	factors[i] = ((a * v.vector[0] + b * v.vector[1]) >> 16) + off;
    }

    if (factors[2] == factors[0])
	image->source.class = SOURCE_IMAGE_CLASS_HORIZONTAL;
    else if (factors[1] == factors[0])
	image->source.class = SOURCE_IMAGE_CLASS_VERTICAL;

    return image->source.class;
}

static void
linear_gradient_get_scanline_32 (pixman_image_t *image,
                                 int             x,
                                 int             y,
                                 int             width,
                                 uint32_t *      buffer,
                                 const uint32_t *mask)
{
    pixman_vector_t v, unit;
    pixman_fixed_32_32_t l;
    pixman_fixed_48_16_t dx, dy, a, b, off;
    gradient_t *gradient = (gradient_t *)image;
    source_image_t *source = (source_image_t *)image;
    linear_gradient_t *linear = (linear_gradient_t *)image;
    uint32_t *end = buffer + width;
    pixman_gradient_walker_t walker;

    _pixman_gradient_walker_init (&walker, gradient, source->common.repeat);

    /* reference point is the center of the pixel */
    v.vector[0] = pixman_int_to_fixed (x) + pixman_fixed_1 / 2;
    v.vector[1] = pixman_int_to_fixed (y) + pixman_fixed_1 / 2;
    v.vector[2] = pixman_fixed_1;

    if (source->common.transform)
    {
	if (!pixman_transform_point_3d (source->common.transform, &v))
	    return;
	
	unit.vector[0] = source->common.transform->matrix[0][0];
	unit.vector[1] = source->common.transform->matrix[1][0];
	unit.vector[2] = source->common.transform->matrix[2][0];
    }
    else
    {
	unit.vector[0] = pixman_fixed_1;
	unit.vector[1] = 0;
	unit.vector[2] = 0;
    }

    dx = linear->p2.x - linear->p1.x;
    dy = linear->p2.y - linear->p1.y;

    l = dx * dx + dy * dy;

    if (l != 0)
    {
	a = (dx << 32) / l;
	b = (dy << 32) / l;
	off = (-a * linear->p1.x
	       -b * linear->p1.y) >> 16;
    }

    if (l == 0 || (unit.vector[2] == 0 && v.vector[2] == pixman_fixed_1))
    {
	pixman_fixed_48_16_t inc, t;

	/* affine transformation only */
	if (l == 0)
	{
	    t = 0;
	    inc = 0;
	}
	else
	{
	    t = ((a * v.vector[0] + b * v.vector[1]) >> 16) + off;
	    inc = (a * unit.vector[0] + b * unit.vector[1]) >> 16;
	}

	if (source->class == SOURCE_IMAGE_CLASS_VERTICAL)
	{
	    register uint32_t color;

	    color = _pixman_gradient_walker_pixel (&walker, t);
	    while (buffer < end)
		*buffer++ = color;
	}
	else
	{
	    if (!mask)
	    {
		while (buffer < end)
		{
		    *buffer++ = _pixman_gradient_walker_pixel (&walker, t);
		    
		    t += inc;
		}
	    }
	    else
	    {
		while (buffer < end)
		{
		    if (*mask++)
			*buffer = _pixman_gradient_walker_pixel (&walker, t);

		    buffer++;
		    t += inc;
		}
	    }
	}
    }
    else
    {
	/* projective transformation */
	pixman_fixed_48_16_t t;

	if (source->class == SOURCE_IMAGE_CLASS_VERTICAL)
	{
	    register uint32_t color;

	    if (v.vector[2] == 0)
	    {
		t = 0;
	    }
	    else
	    {
		pixman_fixed_48_16_t x, y;

		x = ((pixman_fixed_48_16_t) v.vector[0] << 16) / v.vector[2];
		y = ((pixman_fixed_48_16_t) v.vector[1] << 16) / v.vector[2];
		t = ((a * x + b * y) >> 16) + off;
	    }

	    color = _pixman_gradient_walker_pixel (&walker, t);
	    while (buffer < end)
		*buffer++ = color;
	}
	else
	{
	    while (buffer < end)
	    {
		if (!mask || *mask++)
		{
		    if (v.vector[2] == 0)
		    {
			t = 0;
		    }
		    else
		    {
			pixman_fixed_48_16_t x, y;
			x = ((pixman_fixed_48_16_t)v.vector[0] << 16) / v.vector[2];
			y = ((pixman_fixed_48_16_t)v.vector[1] << 16) / v.vector[2];
			t = ((a * x + b * y) >> 16) + off;
		    }

		    *buffer = _pixman_gradient_walker_pixel (&walker, t);
		}

		++buffer;

		v.vector[0] += unit.vector[0];
		v.vector[1] += unit.vector[1];
		v.vector[2] += unit.vector[2];
	    }
	}
    }
}

static void
linear_gradient_property_changed (pixman_image_t *image)
{
    image->common.get_scanline_32 = linear_gradient_get_scanline_32;
    image->common.get_scanline_64 = _pixman_image_get_scanline_generic_64;
}

PIXMAN_EXPORT pixman_image_t *
pixman_image_create_linear_gradient (pixman_point_fixed_t *        p1,
                                     pixman_point_fixed_t *        p2,
                                     const pixman_gradient_stop_t *stops,
                                     int                           n_stops)
{
    pixman_image_t *image;
    linear_gradient_t *linear;

    image = _pixman_image_allocate ();

    if (!image)
	return NULL;

    linear = &image->linear;

    if (!_pixman_init_gradient (&linear->common, stops, n_stops))
    {
	free (image);
	return NULL;
    }

    linear->p1 = *p1;
    linear->p2 = *p2;

    image->type = LINEAR;
    image->source.class = SOURCE_IMAGE_CLASS_UNKNOWN;
    image->common.classify = linear_gradient_classify;
    image->common.property_changed = linear_gradient_property_changed;

    return image;
}

