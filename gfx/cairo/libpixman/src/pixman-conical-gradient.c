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
#include <math.h>
#include "pixman-private.h"

static force_inline double
coordinates_to_parameter (double x, double y, double angle)
{
    double t;

    t = atan2 (y, x) + angle;

    while (t < 0)
	t += 2 * M_PI;

    while (t >= 2 * M_PI)
	t -= 2 * M_PI;

    return 1 - t * (1 / (2 * M_PI)); /* Scale t to [0, 1] and
				      * make rotation CCW
				      */
}

static void
conical_gradient_get_scanline_32 (pixman_image_t *image,
                                  int             x,
                                  int             y,
                                  int             width,
                                  uint32_t *      buffer,
                                  const uint32_t *mask)
{
    source_image_t *source = (source_image_t *)image;
    gradient_t *gradient = (gradient_t *)source;
    conical_gradient_t *conical = (conical_gradient_t *)image;
    uint32_t       *end = buffer + width;
    pixman_gradient_walker_t walker;
    pixman_bool_t affine = TRUE;
    double cx = 1.;
    double cy = 0.;
    double cz = 0.;
    double rx = x + 0.5;
    double ry = y + 0.5;
    double rz = 1.;

    _pixman_gradient_walker_init (&walker, gradient, source->common.repeat);

    if (source->common.transform)
    {
	pixman_vector_t v;

	/* reference point is the center of the pixel */
	v.vector[0] = pixman_int_to_fixed (x) + pixman_fixed_1 / 2;
	v.vector[1] = pixman_int_to_fixed (y) + pixman_fixed_1 / 2;
	v.vector[2] = pixman_fixed_1;

	if (!pixman_transform_point_3d (source->common.transform, &v))
	    return;

	cx = source->common.transform->matrix[0][0] / 65536.;
	cy = source->common.transform->matrix[1][0] / 65536.;
	cz = source->common.transform->matrix[2][0] / 65536.;

	rx = v.vector[0] / 65536.;
	ry = v.vector[1] / 65536.;
	rz = v.vector[2] / 65536.;

	affine =
	    source->common.transform->matrix[2][0] == 0 &&
	    v.vector[2] == pixman_fixed_1;
    }

    if (affine)
    {
	rx -= conical->center.x / 65536.;
	ry -= conical->center.y / 65536.;

	while (buffer < end)
	{
	    if (!mask || *mask++)
	    {
		double t = coordinates_to_parameter (rx, ry, conical->angle);

		*buffer = _pixman_gradient_walker_pixel (
		    &walker, (pixman_fixed_48_16_t)pixman_double_to_fixed (t));
	    }

	    ++buffer;

	    rx += cx;
	    ry += cy;
	}
    }
    else
    {
	while (buffer < end)
	{
	    double x, y;

	    if (!mask || *mask++)
	    {
		double t;

		if (rz != 0)
		{
		    x = rx / rz;
		    y = ry / rz;
		}
		else
		{
		    x = y = 0.;
		}

		x -= conical->center.x / 65536.;
		y -= conical->center.y / 65536.;

		t = coordinates_to_parameter (x, y, conical->angle);

		*buffer = _pixman_gradient_walker_pixel (
		    &walker, (pixman_fixed_48_16_t)pixman_double_to_fixed (t));
	    }

	    ++buffer;

	    rx += cx;
	    ry += cy;
	    rz += cz;
	}
    }
}

static void
conical_gradient_property_changed (pixman_image_t *image)
{
    image->common.get_scanline_32 = conical_gradient_get_scanline_32;
    image->common.get_scanline_64 = _pixman_image_get_scanline_generic_64;
}

PIXMAN_EXPORT pixman_image_t *
pixman_image_create_conical_gradient (pixman_point_fixed_t *        center,
                                      pixman_fixed_t                angle,
                                      const pixman_gradient_stop_t *stops,
                                      int                           n_stops)
{
    pixman_image_t *image = _pixman_image_allocate ();
    conical_gradient_t *conical;

    if (!image)
	return NULL;

    conical = &image->conical;

    if (!_pixman_init_gradient (&conical->common, stops, n_stops))
    {
	free (image);
	return NULL;
    }

    angle = MOD (angle, pixman_int_to_fixed (360));

    image->type = CONICAL;

    conical->center = *center;
    conical->angle = (pixman_fixed_to_double (angle) / 180.0) * M_PI;

    image->common.property_changed = conical_gradient_property_changed;

    return image;
}

