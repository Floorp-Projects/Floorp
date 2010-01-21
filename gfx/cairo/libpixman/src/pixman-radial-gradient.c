/*
 *
 * Copyright © 2000 Keith Packard, member of The XFree86 Project, Inc.
 * Copyright © 2000 SuSE, Inc.
 *             2005 Lars Knoll & Zack Rusin, Trolltech
 * Copyright © 2007 Red Hat, Inc.
 *
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

static void
radial_gradient_get_scanline_32 (pixman_image_t *image,
                                 int             x,
                                 int             y,
                                 int             width,
                                 uint32_t *      buffer,
                                 const uint32_t *mask,
                                 uint32_t        mask_bits)
{
    /*
     * In the radial gradient problem we are given two circles (c₁,r₁) and
     * (c₂,r₂) that define the gradient itself. Then, for any point p, we
     * must compute the value(s) of t within [0.0, 1.0] representing the
     * circle(s) that would color the point.
     *
     * There are potentially two values of t since the point p can be
     * colored by both sides of the circle, (which happens whenever one
     * circle is not entirely contained within the other).
     *
     * If we solve for a value of t that is outside of [0.0, 1.0] then we
     * use the extend mode (NONE, REPEAT, REFLECT, or PAD) to map to a
     * value within [0.0, 1.0].
     *
     * Here is an illustration of the problem:
     *
     *              p₂
     *           p  •
     *           •   ╲
     *        ·       ╲r₂
     *  p₁ ·           ╲
     *  •              θ╲
     *   ╲             ╌╌•
     *    ╲r₁        ·   c₂
     *    θ╲    ·
     *    ╌╌•
     *      c₁
     *
     * Given (c₁,r₁), (c₂,r₂) and p, we must find an angle θ such that two
     * points p₁ and p₂ on the two circles are collinear with p. Then, the
     * desired value of t is the ratio of the length of p₁p to the length
     * of p₁p₂.
     *
     * So, we have six unknown values: (p₁x, p₁y), (p₂x, p₂y), θ and t.
     * We can also write six equations that constrain the problem:
     *
     * Point p₁ is a distance r₁ from c₁ at an angle of θ:
     *
     *	1. p₁x = c₁x + r₁·cos θ
     *	2. p₁y = c₁y + r₁·sin θ
     *
     * Point p₂ is a distance r₂ from c₂ at an angle of θ:
     *
     *	3. p₂x = c₂x + r2·cos θ
     *	4. p₂y = c₂y + r2·sin θ
     *
     * Point p lies at a fraction t along the line segment p₁p₂:
     *
     *	5. px = t·p₂x + (1-t)·p₁x
     *	6. py = t·p₂y + (1-t)·p₁y
     *
     * To solve, first subtitute 1-4 into 5 and 6:
     *
     * px = t·(c₂x + r₂·cos θ) + (1-t)·(c₁x + r₁·cos θ)
     * py = t·(c₂y + r₂·sin θ) + (1-t)·(c₁y + r₁·sin θ)
     *
     * Then solve each for cos θ and sin θ expressed as a function of t:
     *
     * cos θ = (-(c₂x - c₁x)·t + (px - c₁x)) / ((r₂-r₁)·t + r₁)
     * sin θ = (-(c₂y - c₁y)·t + (py - c₁y)) / ((r₂-r₁)·t + r₁)
     *
     * To simplify this a bit, we define new variables for several of the
     * common terms as shown below:
     *
     *              p₂
     *           p  •
     *           •   ╲
     *        ·  ┆    ╲r₂
     *  p₁ ·     ┆     ╲
     *  •     pdy┆      ╲
     *   ╲       ┆       •c₂
     *    ╲r₁    ┆   ·   ┆
     *     ╲    ·┆       ┆cdy
     *      •╌╌╌╌┴╌╌╌╌╌╌╌┘
     *    c₁  pdx   cdx
     *
     * cdx = (c₂x - c₁x)
     * cdy = (c₂y - c₁y)
     *  dr =  r₂-r₁
     * pdx =  px - c₁x
     * pdy =  py - c₁y
     *
     * Note that cdx, cdy, and dr do not depend on point p at all, so can
     * be pre-computed for the entire gradient. The simplifed equations
     * are now:
     *
     * cos θ = (-cdx·t + pdx) / (dr·t + r₁)
     * sin θ = (-cdy·t + pdy) / (dr·t + r₁)
     *
     * Finally, to get a single function of t and eliminate the last
     * unknown θ, we use the identity sin²θ + cos²θ = 1. First, square
     * each equation, (we knew a quadratic was coming since it must be
     * possible to obtain two solutions in some cases):
     *
     * cos²θ = (cdx²t² - 2·cdx·pdx·t + pdx²) / (dr²·t² + 2·r₁·dr·t + r₁²)
     * sin²θ = (cdy²t² - 2·cdy·pdy·t + pdy²) / (dr²·t² + 2·r₁·dr·t + r₁²)
     *
     * Then add both together, set the result equal to 1, and express as a
     * standard quadratic equation in t of the form At² + Bt + C = 0
     *
     * (cdx² + cdy² - dr²)·t² - 2·(cdx·pdx + cdy·pdy + r₁·dr)·t + (pdx² + pdy² - r₁²) = 0
     *
     * In other words:
     *
     * A = cdx² + cdy² - dr²
     * B = -2·(pdx·cdx + pdy·cdy + r₁·dr)
     * C = pdx² + pdy² - r₁²
     *
     * And again, notice that A does not depend on p, so can be
     * precomputed. From here we just use the quadratic formula to solve
     * for t:
     *
     * t = (-2·B ± ⎷(B² - 4·A·C)) / 2·A
     */

    gradient_t *gradient = (gradient_t *)image;
    source_image_t *source = (source_image_t *)image;
    radial_gradient_t *radial = (radial_gradient_t *)image;
    uint32_t *end = buffer + width;
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
	/* When computing t over a scanline, we notice that some expressions
	 * are constant so we can compute them just once. Given:
	 *
	 * t = (-2·B ± ⎷(B² - 4·A·C)) / 2·A
	 *
	 * where
	 *
	 * A = cdx² + cdy² - dr² [precomputed as radial->A]
	 * B = -2·(pdx·cdx + pdy·cdy + r₁·dr)
	 * C = pdx² + pdy² - r₁²
	 *
	 * Since we have an affine transformation, we know that (pdx, pdy)
	 * increase linearly with each pixel,
	 *
	 * pdx = pdx₀ + n·cx,
	 * pdy = pdy₀ + n·cy,
	 *
	 * we can then express B in terms of an linear increment along
	 * the scanline:
	 *
	 * B = B₀ + n·cB, with
	 * B₀ = -2·(pdx₀·cdx + pdy₀·cdy + r₁·dr) and
	 * cB = -2·(cx·cdx + cy·cdy)
	 *
	 * Thus we can replace the full evaluation of B per-pixel (4 multiplies,
	 * 2 additions) with a single addition.
	 */
	double r1   = radial->c1.radius / 65536.;
	double r1sq = r1 * r1;
	double pdx  = rx - radial->c1.x / 65536.;
	double pdy  = ry - radial->c1.y / 65536.;
	double A = radial->A;
	double invA = -65536. / (2. * A);
	double A4 = -4. * A;
	double B  = -2. * (pdx*radial->cdx + pdy*radial->cdy + r1*radial->dr);
	double cB = -2. *  (cx*radial->cdx +  cy*radial->cdy);
	pixman_bool_t invert = A * radial->dr < 0;

	while (buffer < end)
	{
	    if (!mask || *mask++ & mask_bits)
	    {
		pixman_fixed_48_16_t t;
		double det = B * B + A4 * (pdx * pdx + pdy * pdy - r1sq);
		if (det <= 0.)
		    t = (pixman_fixed_48_16_t) (B * invA);
		else if (invert)
		    t = (pixman_fixed_48_16_t) ((B + sqrt (det)) * invA);
		else
		    t = (pixman_fixed_48_16_t) ((B - sqrt (det)) * invA);

		*buffer = _pixman_gradient_walker_pixel (&walker, t);
	    }
	    ++buffer;

	    pdx += cx;
	    pdy += cy;
	    B += cB;
	}
    }
    else
    {
	/* projective */
	while (buffer < end)
	{
	    if (!mask || *mask++ & mask_bits)
	    {
		double pdx, pdy;
		double B, C;
		double det;
		double c1x = radial->c1.x / 65536.0;
		double c1y = radial->c1.y / 65536.0;
		double r1  = radial->c1.radius / 65536.0;
		pixman_fixed_48_16_t t;
		double x, y;

		if (rz != 0)
		{
		    x = rx / rz;
		    y = ry / rz;
		}
		else
		{
		    x = y = 0.;
		}

		pdx = x - c1x;
		pdy = y - c1y;

		B = -2 * (pdx * radial->cdx +
			  pdy * radial->cdy +
			  r1 * radial->dr);
		C = (pdx * pdx + pdy * pdy - r1 * r1);

		det = (B * B) - (4 * radial->A * C);
		if (det < 0.0)
		    det = 0.0;

		if (radial->A * radial->dr < 0)
		    t = (pixman_fixed_48_16_t) ((-B - sqrt (det)) / (2.0 * radial->A) * 65536);
		else
		    t = (pixman_fixed_48_16_t) ((-B + sqrt (det)) / (2.0 * radial->A) * 65536);

		*buffer = _pixman_gradient_walker_pixel (&walker, t);
	    }
	    
	    ++buffer;

	    rx += cx;
	    ry += cy;
	    rz += cz;
	}
    }
}

static void
radial_gradient_property_changed (pixman_image_t *image)
{
    image->common.get_scanline_32 = radial_gradient_get_scanline_32;
    image->common.get_scanline_64 = _pixman_image_get_scanline_generic_64;
}

PIXMAN_EXPORT pixman_image_t *
pixman_image_create_radial_gradient (pixman_point_fixed_t *        inner,
                                     pixman_point_fixed_t *        outer,
                                     pixman_fixed_t                inner_radius,
                                     pixman_fixed_t                outer_radius,
                                     const pixman_gradient_stop_t *stops,
                                     int                           n_stops)
{
    pixman_image_t *image;
    radial_gradient_t *radial;

    return_val_if_fail (n_stops >= 2, NULL);

    image = _pixman_image_allocate ();

    if (!image)
	return NULL;

    radial = &image->radial;

    if (!_pixman_init_gradient (&radial->common, stops, n_stops))
    {
	free (image);
	return NULL;
    }

    image->type = RADIAL;

    radial->c1.x = inner->x;
    radial->c1.y = inner->y;
    radial->c1.radius = inner_radius;
    radial->c2.x = outer->x;
    radial->c2.y = outer->y;
    radial->c2.radius = outer_radius;
    radial->cdx = pixman_fixed_to_double (radial->c2.x - radial->c1.x);
    radial->cdy = pixman_fixed_to_double (radial->c2.y - radial->c1.y);
    radial->dr = pixman_fixed_to_double (radial->c2.radius - radial->c1.radius);
    radial->A = (radial->cdx * radial->cdx +
		 radial->cdy * radial->cdy -
		 radial->dr  * radial->dr);

    image->common.property_changed = radial_gradient_property_changed;

    return image;
}

