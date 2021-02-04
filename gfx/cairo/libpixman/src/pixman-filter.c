/*
 * Copyright 2012, Red Hat, Inc.
 * Copyright 2012, Soren Sandmann
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Author: Soren Sandmann <soren.sandmann@gmail.com>
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "pixman-private.h"

typedef double (* kernel_func_t) (double x);

typedef struct
{
    pixman_kernel_t	kernel;
    kernel_func_t	func;
    double		width;
} filter_info_t;

static double
impulse_kernel (double x)
{
    return (x == 0.0)? 1.0 : 0.0;
}

static double
box_kernel (double x)
{
    return 1;
}

static double
linear_kernel (double x)
{
    return 1 - fabs (x);
}

static double
gaussian_kernel (double x)
{
#define SQRT2 (1.4142135623730950488016887242096980785696718753769480)
#define SIGMA (SQRT2 / 2.0)
    
    return exp (- x * x / (2 * SIGMA * SIGMA)) / (SIGMA * sqrt (2.0 * M_PI));
}

static double
sinc (double x)
{
    if (x == 0.0)
	return 1.0;
    else
	return sin (M_PI * x) / (M_PI * x);
}

static double
lanczos (double x, int n)
{
    return sinc (x) * sinc (x * (1.0 / n));
}

static double
lanczos2_kernel (double x)
{
    return lanczos (x, 2);
}

static double
lanczos3_kernel (double x)
{
    return lanczos (x, 3);
}

static double
nice_kernel (double x)
{
    return lanczos3_kernel (x * 0.75);
}

static double
general_cubic (double x, double B, double C)
{
    double ax = fabs(x);

    if (ax < 1)
    {
	return ((12 - 9 * B - 6 * C) * ax * ax * ax +
		(-18 + 12 * B + 6 * C) * ax * ax + (6 - 2 * B)) / 6;
    }
    else if (ax >= 1 && ax < 2)
    {
	return ((-B - 6 * C) * ax * ax * ax +
		(6 * B + 30 * C) * ax * ax + (-12 * B - 48 * C) *
		ax + (8 * B + 24 * C)) / 6;
    }
    else
    {
	return 0;
    }
}

static double
cubic_kernel (double x)
{
    /* This is the Mitchell-Netravali filter.
     *
     * (0.0, 0.5) would give us the Catmull-Rom spline,
     * but that one seems to be indistinguishable from Lanczos2.
     */
    return general_cubic (x, 1/3.0, 1/3.0);
}

static const filter_info_t filters[] =
{
    { PIXMAN_KERNEL_IMPULSE,	        impulse_kernel,   0.0 },
    { PIXMAN_KERNEL_BOX,	        box_kernel,       1.0 },
    { PIXMAN_KERNEL_LINEAR,	        linear_kernel,    2.0 },
    { PIXMAN_KERNEL_CUBIC,		cubic_kernel,     4.0 },
    { PIXMAN_KERNEL_GAUSSIAN,	        gaussian_kernel,  6 * SIGMA },
    { PIXMAN_KERNEL_LANCZOS2,	        lanczos2_kernel,  4.0 },
    { PIXMAN_KERNEL_LANCZOS3,	        lanczos3_kernel,  6.0 },
    { PIXMAN_KERNEL_LANCZOS3_STRETCHED, nice_kernel,      8.0 },
};

/* This function scales @kernel2 by @scale, then
 * aligns @x1 in @kernel1 with @x2 in @kernel2 and
 * and integrates the product of the kernels across @width.
 *
 * This function assumes that the intervals are within
 * the kernels in question. E.g., the caller must not
 * try to integrate a linear kernel ouside of [-1:1]
 */
static double
integral (pixman_kernel_t kernel1, double x1,
	  pixman_kernel_t kernel2, double scale, double x2,
	  double width)
{
    /* If the integration interval crosses zero, break it into
     * two separate integrals. This ensures that filters such
     * as LINEAR that are not differentiable at 0 will still
     * integrate properly.
     */
    if (x1 < 0 && x1 + width > 0)
    {
	return
	    integral (kernel1, x1, kernel2, scale, x2, - x1) +
	    integral (kernel1, 0, kernel2, scale, x2 - x1, width + x1);
    }
    else if (x2 < 0 && x2 + width > 0)
    {
	return
	    integral (kernel1, x1, kernel2, scale, x2, - x2) +
	    integral (kernel1, x1 - x2, kernel2, scale, 0, width + x2);
    }
    else if (kernel1 == PIXMAN_KERNEL_IMPULSE)
    {
	assert (width == 0.0);
	return filters[kernel2].func (x2 * scale);
    }
    else if (kernel2 == PIXMAN_KERNEL_IMPULSE)
    {
	assert (width == 0.0);
	return filters[kernel1].func (x1);
    }
    else
    {
	/* Integration via Simpson's rule */
#define N_SEGMENTS 128
#define SAMPLE(a1, a2)							\
	(filters[kernel1].func ((a1)) * filters[kernel2].func ((a2) * scale))
	
	double s = 0.0;
	double h = width / (double)N_SEGMENTS;
	int i;

	s = SAMPLE (x1, x2);

	for (i = 1; i < N_SEGMENTS; i += 2)
	{
	    double a1 = x1 + h * i;
	    double a2 = x2 + h * i;

	    s += 2 * SAMPLE (a1, a2);

	    if (i >= 2 && i < N_SEGMENTS - 1)
		s += 4 * SAMPLE (a1, a2);
	}

	s += SAMPLE (x1 + width, x2 + width);
	
	return h * s * (1.0 / 3.0);
    }
}

static pixman_fixed_t *
create_1d_filter (int             *width,
		  pixman_kernel_t  reconstruct,
		  pixman_kernel_t  sample,
		  double           scale,
		  int              n_phases)
{
    pixman_fixed_t *params, *p;
    double step;
    double size;
    int i;

    size = scale * filters[sample].width + filters[reconstruct].width;
    *width = ceil (size);

    p = params = malloc (*width * n_phases * sizeof (pixman_fixed_t));
    if (!params)
        return NULL;

    step = 1.0 / n_phases;

    for (i = 0; i < n_phases; ++i)
    {
        double frac = step / 2.0 + i * step;
	pixman_fixed_t new_total;
        int x, x1, x2;
	double total;

	/* Sample convolution of reconstruction and sampling
	 * filter. See rounding.txt regarding the rounding
	 * and sample positions.
	 */

	x1 = ceil (frac - *width / 2.0 - 0.5);
        x2 = x1 + *width;

	total = 0;
        for (x = x1; x < x2; ++x)
        {
	    double pos = x + 0.5 - frac;
	    double rlow = - filters[reconstruct].width / 2.0;
	    double rhigh = rlow + filters[reconstruct].width;
	    double slow = pos - scale * filters[sample].width / 2.0;
	    double shigh = slow + scale * filters[sample].width;
	    double c = 0.0;
	    double ilow, ihigh;

	    if (rhigh >= slow && rlow <= shigh)
	    {
		ilow = MAX (slow, rlow);
		ihigh = MIN (shigh, rhigh);

		c = integral (reconstruct, ilow,
			      sample, 1.0 / scale, ilow - pos,
			      ihigh - ilow);
	    }

	    total += c;
            *p++ = (pixman_fixed_t)(c * 65535.0 + 0.5);
        }

	/* Normalize */
	p -= *width;
        total = 1 / total;
        new_total = 0;
	for (x = x1; x < x2; ++x)
	{
	    pixman_fixed_t t = (*p) * total + 0.5;

	    new_total += t;
	    *p++ = t;
	}

	if (new_total != pixman_fixed_1)
	    *(p - *width / 2) += (pixman_fixed_1 - new_total);
    }

    return params;
}

/* Create the parameter list for a SEPARABLE_CONVOLUTION filter
 * with the given kernels and scale parameters
 */
PIXMAN_EXPORT pixman_fixed_t *
pixman_filter_create_separable_convolution (int             *n_values,
					    pixman_fixed_t   scale_x,
					    pixman_fixed_t   scale_y,
					    pixman_kernel_t  reconstruct_x,
					    pixman_kernel_t  reconstruct_y,
					    pixman_kernel_t  sample_x,
					    pixman_kernel_t  sample_y,
					    int              subsample_bits_x,
					    int	             subsample_bits_y)
{
    double sx = fabs (pixman_fixed_to_double (scale_x));
    double sy = fabs (pixman_fixed_to_double (scale_y));
    pixman_fixed_t *horz = NULL, *vert = NULL, *params = NULL;
    int subsample_x, subsample_y;
    int width, height;

    subsample_x = (1 << subsample_bits_x);
    subsample_y = (1 << subsample_bits_y);

    horz = create_1d_filter (&width, reconstruct_x, sample_x, sx, subsample_x);
    vert = create_1d_filter (&height, reconstruct_y, sample_y, sy, subsample_y);

    if (!horz || !vert)
        goto out;
    
    *n_values = 4 + width * subsample_x + height * subsample_y;
    
    params = malloc (*n_values * sizeof (pixman_fixed_t));
    if (!params)
        goto out;

    params[0] = pixman_int_to_fixed (width);
    params[1] = pixman_int_to_fixed (height);
    params[2] = pixman_int_to_fixed (subsample_bits_x);
    params[3] = pixman_int_to_fixed (subsample_bits_y);

    memcpy (params + 4, horz,
	    width * subsample_x * sizeof (pixman_fixed_t));
    memcpy (params + 4 + width * subsample_x, vert,
	    height * subsample_y * sizeof (pixman_fixed_t));

out:
    free (horz);
    free (vert);

    return params;
}
