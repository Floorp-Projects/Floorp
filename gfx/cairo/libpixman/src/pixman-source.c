/*
 *
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

#include <math.h>

#include "pixman-private.h"

typedef struct
{
    uint32_t        left_ag;
    uint32_t        left_rb;
    uint32_t        right_ag;
    uint32_t        right_rb;
    int32_t       left_x;
    int32_t       right_x;
    int32_t       stepper;

    pixman_gradient_stop_t	*stops;
    int                      num_stops;
    unsigned int             spread;

    int		  need_reset;
} GradientWalker;

static void
_gradient_walker_init (GradientWalker  *walker,
		       gradient_t      *gradient,
		       unsigned int     spread)
{
    walker->num_stops = gradient->n_stops;
    walker->stops     = gradient->stops;
    walker->left_x    = 0;
    walker->right_x   = 0x10000;
    walker->stepper   = 0;
    walker->left_ag   = 0;
    walker->left_rb   = 0;
    walker->right_ag  = 0;
    walker->right_rb  = 0;
    walker->spread    = spread;

    walker->need_reset = TRUE;
}

static void
_gradient_walker_reset (GradientWalker  *walker,
                        pixman_fixed_32_32_t     pos)
{
    int32_t                  x, left_x, right_x;
    pixman_color_t          *left_c, *right_c;
    int                      n, count = walker->num_stops;
    pixman_gradient_stop_t *      stops = walker->stops;

    static const pixman_color_t   transparent_black = { 0, 0, 0, 0 };

    switch (walker->spread)
    {
    case PIXMAN_REPEAT_NORMAL:
	x = (int32_t)pos & 0xFFFF;
	for (n = 0; n < count; n++)
	    if (x < stops[n].x)
		break;
	if (n == 0) {
	    left_x =  stops[count-1].x - 0x10000;
	    left_c = &stops[count-1].color;
	} else {
	    left_x =  stops[n-1].x;
	    left_c = &stops[n-1].color;
	}

	if (n == count) {
	    right_x =  stops[0].x + 0x10000;
	    right_c = &stops[0].color;
	} else {
	    right_x =  stops[n].x;
	    right_c = &stops[n].color;
	}
	left_x  += (pos - x);
	right_x += (pos - x);
	break;

    case PIXMAN_REPEAT_PAD:
	for (n = 0; n < count; n++)
	    if (pos < stops[n].x)
		break;

	if (n == 0) {
	    left_x =  INT32_MIN;
	    left_c = &stops[0].color;
	} else {
	    left_x =  stops[n-1].x;
	    left_c = &stops[n-1].color;
	}

	if (n == count) {
	    right_x =  INT32_MAX;
	    right_c = &stops[n-1].color;
	} else {
	    right_x =  stops[n].x;
	    right_c = &stops[n].color;
	}
	break;

    case PIXMAN_REPEAT_REFLECT:
	x = (int32_t)pos & 0xFFFF;
	if ((int32_t)pos & 0x10000)
	    x = 0x10000 - x;
	for (n = 0; n < count; n++)
	    if (x < stops[n].x)
		break;

	if (n == 0) {
	    left_x =  -stops[0].x;
	    left_c = &stops[0].color;
	} else {
	    left_x =  stops[n-1].x;
	    left_c = &stops[n-1].color;
	}

	if (n == count) {
	    right_x = 0x20000 - stops[n-1].x;
	    right_c = &stops[n-1].color;
	} else {
	    right_x =  stops[n].x;
	    right_c = &stops[n].color;
	}

	if ((int32_t)pos & 0x10000) {
	    pixman_color_t  *tmp_c;
	    int32_t          tmp_x;

	    tmp_x   = 0x10000 - right_x;
	    right_x = 0x10000 - left_x;
	    left_x  = tmp_x;

	    tmp_c   = right_c;
	    right_c = left_c;
	    left_c  = tmp_c;

	    x = 0x10000 - x;
	}
	left_x  += (pos - x);
	right_x += (pos - x);
	break;

    default:  /* RepeatNone */
	for (n = 0; n < count; n++)
	    if (pos < stops[n].x)
		break;

	if (n == 0)
	{
	    left_x  =  INT32_MIN;
	    right_x =  stops[0].x;
	    left_c  = right_c = (pixman_color_t*) &transparent_black;
	}
	else if (n == count)
	{
	    left_x  = stops[n-1].x;
	    right_x = INT32_MAX;
	    left_c  = right_c = (pixman_color_t*) &transparent_black;
	}
	else
	{
	    left_x  =  stops[n-1].x;
	    right_x =  stops[n].x;
	    left_c  = &stops[n-1].color;
	    right_c = &stops[n].color;
	}
    }

    walker->left_x   = left_x;
    walker->right_x  = right_x;
    walker->left_ag  = ((left_c->alpha >> 8) << 16)   | (left_c->green >> 8);
    walker->left_rb  = ((left_c->red & 0xff00) << 8)  | (left_c->blue >> 8);
    walker->right_ag = ((right_c->alpha >> 8) << 16)  | (right_c->green >> 8);
    walker->right_rb = ((right_c->red & 0xff00) << 8) | (right_c->blue >> 8);

    if ( walker->left_x == walker->right_x                ||
	 ( walker->left_ag == walker->right_ag &&
	   walker->left_rb == walker->right_rb )   )
    {
	walker->stepper = 0;
    }
    else
    {
	int32_t width = right_x - left_x;
	walker->stepper = ((1 << 24) + width/2)/width;
    }

    walker->need_reset = FALSE;
}

#define  GRADIENT_WALKER_NEED_RESET(w,x)				\
    ( (w)->need_reset || (x) < (w)->left_x || (x) >= (w)->right_x)


/* the following assumes that GRADIENT_WALKER_NEED_RESET(w,x) is FALSE */
static uint32_t
_gradient_walker_pixel (GradientWalker  *walker,
                        pixman_fixed_32_32_t     x)
{
    int  dist, idist;
    uint32_t  t1, t2, a, color;

    if (GRADIENT_WALKER_NEED_RESET (walker, x))
        _gradient_walker_reset (walker, x);

    dist  = ((int)(x - walker->left_x)*walker->stepper) >> 16;
    idist = 256 - dist;

    /* combined INTERPOLATE and premultiply */
    t1 = walker->left_rb*idist + walker->right_rb*dist;
    t1 = (t1 >> 8) & 0xff00ff;

    t2  = walker->left_ag*idist + walker->right_ag*dist;
    t2 &= 0xff00ff00;

    color = t2 & 0xff000000;
    a     = t2 >> 24;

    t1  = t1*a + 0x800080;
    t1  = (t1 + ((t1 >> 8) & 0xff00ff)) >> 8;

    t2  = (t2 >> 8)*a + 0x800080;
    t2  = (t2 + ((t2 >> 8) & 0xff00ff));

    return (color | (t1 & 0xff00ff) | (t2 & 0xff00));
}

void pixmanFetchSourcePict(source_image_t * pict, int x, int y, int width,
                           uint32_t *buffer, uint32_t *mask, uint32_t maskBits)
{
#if 0
    SourcePictPtr   pGradient = pict->pSourcePict;
#endif
    GradientWalker  walker;
    uint32_t       *end = buffer + width;
    gradient_t	    *gradient;

    if (pict->common.type == SOLID)
    {
	register uint32_t color = ((solid_fill_t *)pict)->color;

	while (buffer < end)
	    *(buffer++) = color;

	return;
    }

    gradient = (gradient_t *)pict;

    _gradient_walker_init (&walker, gradient, pict->common.repeat);

    if (pict->common.type == LINEAR) {
	pixman_vector_t v, unit;
	pixman_fixed_32_32_t l;
	pixman_fixed_48_16_t dx, dy, a, b, off;
	linear_gradient_t *linear = (linear_gradient_t *)pict;

        /* reference point is the center of the pixel */
        v.vector[0] = pixman_int_to_fixed(x) + pixman_fixed_1/2;
        v.vector[1] = pixman_int_to_fixed(y) + pixman_fixed_1/2;
        v.vector[2] = pixman_fixed_1;
        if (pict->common.transform) {
            if (!pixman_transform_point_3d (pict->common.transform, &v))
                return;
            unit.vector[0] = pict->common.transform->matrix[0][0];
            unit.vector[1] = pict->common.transform->matrix[1][0];
            unit.vector[2] = pict->common.transform->matrix[2][0];
        } else {
            unit.vector[0] = pixman_fixed_1;
            unit.vector[1] = 0;
            unit.vector[2] = 0;
        }

        dx = linear->p2.x - linear->p1.x;
        dy = linear->p2.y - linear->p1.y;
        l = dx*dx + dy*dy;
        if (l != 0) {
            a = (dx << 32) / l;
            b = (dy << 32) / l;
            off = (-a*linear->p1.x - b*linear->p1.y)>>16;
        }
        if (l == 0  || (unit.vector[2] == 0 && v.vector[2] == pixman_fixed_1)) {
            pixman_fixed_48_16_t inc, t;
            /* affine transformation only */
            if (l == 0) {
                t = 0;
                inc = 0;
            } else {
                t = ((a*v.vector[0] + b*v.vector[1]) >> 16) + off;
                inc = (a * unit.vector[0] + b * unit.vector[1]) >> 16;
            }

	    if (pict->class == SOURCE_IMAGE_CLASS_VERTICAL)
	    {
		register uint32_t color;

		color = _gradient_walker_pixel( &walker, t );
		while (buffer < end)
		    *(buffer++) = color;
	    }
	    else
	    {
                if (!mask) {
                    while (buffer < end)
                    {
			*(buffer) = _gradient_walker_pixel (&walker, t);
                        buffer += 1;
                        t      += inc;
                    }
                } else {
                    while (buffer < end) {
                        if (*mask++ & maskBits)
                        {
			    *(buffer) = _gradient_walker_pixel (&walker, t);
                        }
                        buffer += 1;
                        t      += inc;
                    }
                }
	    }
	}
	else /* projective transformation */
	{
	    pixman_fixed_48_16_t t;

	    if (pict->class == SOURCE_IMAGE_CLASS_VERTICAL)
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

 		color = _gradient_walker_pixel( &walker, t );
		while (buffer < end)
		    *(buffer++) = color;
	    }
	    else
	    {
		while (buffer < end)
		{
		    if (!mask || *mask++ & maskBits)
		    {
			if (v.vector[2] == 0) {
			    t = 0;
			} else {
			    pixman_fixed_48_16_t x, y;
			    x = ((pixman_fixed_48_16_t)v.vector[0] << 16) / v.vector[2];
			    y = ((pixman_fixed_48_16_t)v.vector[1] << 16) / v.vector[2];
			    t = ((a*x + b*y) >> 16) + off;
			}
			*(buffer) = _gradient_walker_pixel (&walker, t);
		    }
		    ++buffer;
		    v.vector[0] += unit.vector[0];
		    v.vector[1] += unit.vector[1];
		    v.vector[2] += unit.vector[2];
		}
            }
        }
    } else {

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
        /* radial or conical */
        pixman_bool_t affine = TRUE;
        double cx = 1.;
        double cy = 0.;
        double cz = 0.;
	double rx = x + 0.5;
	double ry = y + 0.5;
        double rz = 1.;

        if (pict->common.transform) {
            pixman_vector_t v;
            /* reference point is the center of the pixel */
            v.vector[0] = pixman_int_to_fixed(x) + pixman_fixed_1/2;
            v.vector[1] = pixman_int_to_fixed(y) + pixman_fixed_1/2;
            v.vector[2] = pixman_fixed_1;
            if (!pixman_transform_point_3d (pict->common.transform, &v))
                return;

            cx = pict->common.transform->matrix[0][0]/65536.;
            cy = pict->common.transform->matrix[1][0]/65536.;
            cz = pict->common.transform->matrix[2][0]/65536.;
            rx = v.vector[0]/65536.;
            ry = v.vector[1]/65536.;
            rz = v.vector[2]/65536.;
            affine = pict->common.transform->matrix[2][0] == 0 && v.vector[2] == pixman_fixed_1;
        }

        if (pict->common.type == RADIAL) {
	    radial_gradient_t *radial = (radial_gradient_t *)pict;
            if (affine) {
                while (buffer < end) {
		    if (!mask || *mask++ & maskBits)
		    {
			double pdx, pdy;
			double B, C;
			double det;
			double c1x = radial->c1.x / 65536.0;
			double c1y = radial->c1.y / 65536.0;
			double r1  = radial->c1.radius / 65536.0;
                        pixman_fixed_48_16_t t;

			pdx = rx - c1x;
			pdy = ry - c1y;

			B = -2 * (  pdx * radial->cdx
				    + pdy * radial->cdy
				    + r1 * radial->dr);
			C = (pdx * pdx + pdy * pdy - r1 * r1);

                        det = (B * B) - (4 * radial->A * C);
			if (det < 0.0)
			    det = 0.0;

			if (radial->A < 0)
			    t = (pixman_fixed_48_16_t) ((- B - sqrt(det)) / (2.0 * radial->A) * 65536);
			else
			    t = (pixman_fixed_48_16_t) ((- B + sqrt(det)) / (2.0 * radial->A) * 65536);

			*(buffer) = _gradient_walker_pixel (&walker, t);
		    }
		    ++buffer;

                    rx += cx;
                    ry += cy;
                }
            } else {
		/* projective */
                while (buffer < end) {
		    if (!mask || *mask++ & maskBits)
		    {
			double pdx, pdy;
			double B, C;
			double det;
			double c1x = radial->c1.x / 65536.0;
			double c1y = radial->c1.y / 65536.0;
			double r1  = radial->c1.radius / 65536.0;
                        pixman_fixed_48_16_t t;
			double x, y;

			if (rz != 0) {
			    x = rx/rz;
			    y = ry/rz;
			} else {
			    x = y = 0.;
			}

			pdx = x - c1x;
			pdy = y - c1y;

			B = -2 * (  pdx * radial->cdx
				    + pdy * radial->cdy
				    + r1 * radial->dr);
			C = (pdx * pdx + pdy * pdy - r1 * r1);

                        det = (B * B) - (4 * radial->A * C);
			if (det < 0.0)
			    det = 0.0;

			if (radial->A < 0)
			    t = (pixman_fixed_48_16_t) ((- B - sqrt(det)) / (2.0 * radial->A) * 65536);
			else
			    t = (pixman_fixed_48_16_t) ((- B + sqrt(det)) / (2.0 * radial->A) * 65536);

			*(buffer) = _gradient_walker_pixel (&walker, t);
		    }
		    ++buffer;

                    rx += cx;
                    ry += cy;
		    rz += cz;
                }
            }
        } else /* SourcePictTypeConical */ {
	    conical_gradient_t *conical = (conical_gradient_t *)pict;
            double a = conical->angle/(180.*65536);
            if (affine) {
                rx -= conical->center.x/65536.;
                ry -= conical->center.y/65536.;

                while (buffer < end) {
		    double angle;

                    if (!mask || *mask++ & maskBits)
		    {
                        pixman_fixed_48_16_t   t;

                        angle = atan2(ry, rx) + a;
			t     = (pixman_fixed_48_16_t) (angle * (65536. / (2*M_PI)));

			*(buffer) = _gradient_walker_pixel (&walker, t);
		    }

                    ++buffer;
                    rx += cx;
                    ry += cy;
                }
            } else {
                while (buffer < end) {
                    double x, y;
                    double angle;

                    if (!mask || *mask++ & maskBits)
                    {
			pixman_fixed_48_16_t  t;

			if (rz != 0) {
			    x = rx/rz;
			    y = ry/rz;
			} else {
			    x = y = 0.;
			}
			x -= conical->center.x/65536.;
			y -= conical->center.y/65536.;
			angle = atan2(y, x) + a;
			t     = (pixman_fixed_48_16_t) (angle * (65536. / (2*M_PI)));

			*(buffer) = _gradient_walker_pixel (&walker, t);
		    }

                    ++buffer;
                    rx += cx;
                    ry += cy;
                    rz += cz;
                }
            }
        }
    }
}
