/*
 * Copyright © 2008 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

/*
 * Matrix interfaces
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include <string.h>
#include "pixman-private.h"

#define F(x)    pixman_int_to_fixed (x)

PIXMAN_EXPORT void
pixman_transform_init_identity (struct pixman_transform *matrix)
{
    int i;

    memset (matrix, '\0', sizeof (struct pixman_transform));
    for (i = 0; i < 3; i++)
	matrix->matrix[i][i] = F (1);
}

typedef pixman_fixed_32_32_t pixman_fixed_34_30_t;

PIXMAN_EXPORT pixman_bool_t
pixman_transform_point_3d (const struct pixman_transform *transform,
                           struct pixman_vector *         vector)
{
    struct pixman_vector result;
    pixman_fixed_32_32_t partial;
    pixman_fixed_48_16_t v;
    int i, j;

    for (j = 0; j < 3; j++)
    {
	v = 0;
	for (i = 0; i < 3; i++)
	{
	    partial = ((pixman_fixed_48_16_t) transform->matrix[j][i] *
	               (pixman_fixed_48_16_t) vector->vector[i]);
	    v += partial >> 16;
	}
	
	if (v > pixman_max_fixed_48_16 || v < pixman_min_fixed_48_16)
	    return FALSE;
	
	result.vector[j] = (pixman_fixed_t) v;
    }
    
    *vector = result;

    if (!result.vector[2])
	return FALSE;

    return TRUE;
}

PIXMAN_EXPORT pixman_bool_t
pixman_transform_point (const struct pixman_transform *transform,
                        struct pixman_vector *         vector)
{
    pixman_fixed_32_32_t partial;
    pixman_fixed_34_30_t v[3];
    pixman_fixed_48_16_t quo;
    int i, j;

    for (j = 0; j < 3; j++)
    {
	v[j] = 0;
	
	for (i = 0; i < 3; i++)
	{
	    partial = ((pixman_fixed_32_32_t) transform->matrix[j][i] *
	               (pixman_fixed_32_32_t) vector->vector[i]);
	    v[j] += partial >> 2;
	}
    }
    
    if (!(v[2] >> 16))
	return FALSE;

    for (j = 0; j < 2; j++)
    {
	quo = v[j] / (v[2] >> 16);
	if (quo > pixman_max_fixed_48_16 || quo < pixman_min_fixed_48_16)
	    return FALSE;
	vector->vector[j] = (pixman_fixed_t) quo;
    }
    
    vector->vector[2] = pixman_fixed_1;
    return TRUE;
}

PIXMAN_EXPORT pixman_bool_t
pixman_transform_multiply (struct pixman_transform *      dst,
                           const struct pixman_transform *l,
                           const struct pixman_transform *r)
{
    struct pixman_transform d;
    int dx, dy;
    int o;

    for (dy = 0; dy < 3; dy++)
    {
	for (dx = 0; dx < 3; dx++)
	{
	    pixman_fixed_48_16_t v;
	    pixman_fixed_32_32_t partial;
	    
	    v = 0;
	    for (o = 0; o < 3; o++)
	    {
		partial =
		    (pixman_fixed_32_32_t) l->matrix[dy][o] *
		    (pixman_fixed_32_32_t) r->matrix[o][dx];

		v += partial >> 16;
	    }

	    if (v > pixman_max_fixed_48_16 || v < pixman_min_fixed_48_16)
		return FALSE;
	    
	    d.matrix[dy][dx] = (pixman_fixed_t) v;
	}
    }

    *dst = d;
    return TRUE;
}

PIXMAN_EXPORT void
pixman_transform_init_scale (struct pixman_transform *t,
                             pixman_fixed_t           sx,
                             pixman_fixed_t           sy)
{
    memset (t, '\0', sizeof (struct pixman_transform));

    t->matrix[0][0] = sx;
    t->matrix[1][1] = sy;
    t->matrix[2][2] = F (1);
}

static pixman_fixed_t
fixed_inverse (pixman_fixed_t x)
{
    return (pixman_fixed_t) ((((pixman_fixed_48_16_t) F (1)) * F (1)) / x);
}

PIXMAN_EXPORT pixman_bool_t
pixman_transform_scale (struct pixman_transform *forward,
                        struct pixman_transform *reverse,
                        pixman_fixed_t           sx,
                        pixman_fixed_t           sy)
{
    struct pixman_transform t;

    if (sx == 0 || sy == 0)
	return FALSE;

    if (forward)
    {
	pixman_transform_init_scale (&t, sx, sy);
	if (!pixman_transform_multiply (forward, &t, forward))
	    return FALSE;
    }
    
    if (reverse)
    {
	pixman_transform_init_scale (&t, fixed_inverse (sx),
	                             fixed_inverse (sy));
	if (!pixman_transform_multiply (reverse, reverse, &t))
	    return FALSE;
    }
    
    return TRUE;
}

PIXMAN_EXPORT void
pixman_transform_init_rotate (struct pixman_transform *t,
                              pixman_fixed_t           c,
                              pixman_fixed_t           s)
{
    memset (t, '\0', sizeof (struct pixman_transform));

    t->matrix[0][0] = c;
    t->matrix[0][1] = -s;
    t->matrix[1][0] = s;
    t->matrix[1][1] = c;
    t->matrix[2][2] = F (1);
}

PIXMAN_EXPORT pixman_bool_t
pixman_transform_rotate (struct pixman_transform *forward,
                         struct pixman_transform *reverse,
                         pixman_fixed_t           c,
                         pixman_fixed_t           s)
{
    struct pixman_transform t;

    if (forward)
    {
	pixman_transform_init_rotate (&t, c, s);
	if (!pixman_transform_multiply (forward, &t, forward))
	    return FALSE;
    }

    if (reverse)
    {
	pixman_transform_init_rotate (&t, c, -s);
	if (!pixman_transform_multiply (reverse, reverse, &t))
	    return FALSE;
    }
    
    return TRUE;
}

PIXMAN_EXPORT void
pixman_transform_init_translate (struct pixman_transform *t,
                                 pixman_fixed_t           tx,
                                 pixman_fixed_t           ty)
{
    memset (t, '\0', sizeof (struct pixman_transform));

    t->matrix[0][0] = F (1);
    t->matrix[0][2] = tx;
    t->matrix[1][1] = F (1);
    t->matrix[1][2] = ty;
    t->matrix[2][2] = F (1);
}

PIXMAN_EXPORT pixman_bool_t
pixman_transform_translate (struct pixman_transform *forward,
                            struct pixman_transform *reverse,
                            pixman_fixed_t           tx,
                            pixman_fixed_t           ty)
{
    struct pixman_transform t;

    if (forward)
    {
	pixman_transform_init_translate (&t, tx, ty);

	if (!pixman_transform_multiply (forward, &t, forward))
	    return FALSE;
    }

    if (reverse)
    {
	pixman_transform_init_translate (&t, -tx, -ty);

	if (!pixman_transform_multiply (reverse, reverse, &t))
	    return FALSE;
    }
    return TRUE;
}

PIXMAN_EXPORT pixman_bool_t
pixman_transform_bounds (const struct pixman_transform *matrix,
                         struct pixman_box16 *          b)

{
    struct pixman_vector v[4];
    int i;
    int x1, y1, x2, y2;

    v[0].vector[0] = F (b->x1);
    v[0].vector[1] = F (b->y1);
    v[0].vector[2] = F (1);

    v[1].vector[0] = F (b->x2);
    v[1].vector[1] = F (b->y1);
    v[1].vector[2] = F (1);

    v[2].vector[0] = F (b->x2);
    v[2].vector[1] = F (b->y2);
    v[2].vector[2] = F (1);

    v[3].vector[0] = F (b->x1);
    v[3].vector[1] = F (b->y2);
    v[3].vector[2] = F (1);

    for (i = 0; i < 4; i++)
    {
	if (!pixman_transform_point (matrix, &v[i]))
	    return FALSE;

	x1 = pixman_fixed_to_int (v[i].vector[0]);
	y1 = pixman_fixed_to_int (v[i].vector[1]);
	x2 = pixman_fixed_to_int (pixman_fixed_ceil (v[i].vector[0]));
	y2 = pixman_fixed_to_int (pixman_fixed_ceil (v[i].vector[1]));

	if (i == 0)
	{
	    b->x1 = x1;
	    b->y1 = y1;
	    b->x2 = x2;
	    b->y2 = y2;
	}
	else
	{
	    if (x1 < b->x1) b->x1 = x1;
	    if (y1 < b->y1) b->y1 = y1;
	    if (x2 > b->x2) b->x2 = x2;
	    if (y2 > b->y2) b->y2 = y2;
	}
    }

    return TRUE;
}

PIXMAN_EXPORT pixman_bool_t
pixman_transform_invert (struct pixman_transform *      dst,
                         const struct pixman_transform *src)
{
    struct pixman_f_transform m, r;

    pixman_f_transform_from_pixman_transform (&m, src);

    if (!pixman_f_transform_invert (&r, &m))
	return FALSE;

    if (!pixman_transform_from_pixman_f_transform (dst, &r))
	return FALSE;

    return TRUE;
}

static pixman_bool_t
within_epsilon (pixman_fixed_t a,
                pixman_fixed_t b,
                pixman_fixed_t epsilon)
{
    pixman_fixed_t t = a - b;

    if (t < 0)
	t = -t;

    return t <= epsilon;
}

#define EPSILON (pixman_fixed_t) (2)

#define IS_SAME(a, b) (within_epsilon (a, b, EPSILON))
#define IS_ZERO(a)    (within_epsilon (a, 0, EPSILON))
#define IS_ONE(a)     (within_epsilon (a, F (1), EPSILON))
#define IS_UNIT(a)			    \
    (within_epsilon (a, F (1), EPSILON) ||  \
     within_epsilon (a, F (-1), EPSILON) || \
     IS_ZERO (a))
#define IS_INT(a)    (IS_ZERO (pixman_fixed_frac (a)))

PIXMAN_EXPORT pixman_bool_t
pixman_transform_is_identity (const struct pixman_transform *t)
{
    return (IS_SAME (t->matrix[0][0], t->matrix[1][1]) &&
	    IS_SAME (t->matrix[0][0], t->matrix[2][2]) &&
	    !IS_ZERO (t->matrix[0][0]) &&
	    IS_ZERO (t->matrix[0][1]) &&
	    IS_ZERO (t->matrix[0][2]) &&
	    IS_ZERO (t->matrix[1][0]) &&
	    IS_ZERO (t->matrix[1][2]) &&
	    IS_ZERO (t->matrix[2][0]) &&
	    IS_ZERO (t->matrix[2][1]));
}

PIXMAN_EXPORT pixman_bool_t
pixman_transform_is_scale (const struct pixman_transform *t)
{
    return (!IS_ZERO (t->matrix[0][0]) &&
            IS_ZERO (t->matrix[0][1]) &&
            IS_ZERO (t->matrix[0][2]) &&

            IS_ZERO (t->matrix[1][0]) &&
            !IS_ZERO (t->matrix[1][1]) &&
            IS_ZERO (t->matrix[1][2]) &&

            IS_ZERO (t->matrix[2][0]) &&
            IS_ZERO (t->matrix[2][1]) &&
            !IS_ZERO (t->matrix[2][2]));
}

PIXMAN_EXPORT pixman_bool_t
pixman_transform_is_int_translate (const struct pixman_transform *t)
{
    return (IS_ONE (t->matrix[0][0]) &&
            IS_ZERO (t->matrix[0][1]) &&
            IS_INT (t->matrix[0][2]) &&

            IS_ZERO (t->matrix[1][0]) &&
            IS_ONE (t->matrix[1][1]) &&
            IS_INT (t->matrix[1][2]) &&

            IS_ZERO (t->matrix[2][0]) &&
            IS_ZERO (t->matrix[2][1]) &&
            IS_ONE (t->matrix[2][2]));
}

PIXMAN_EXPORT pixman_bool_t
pixman_transform_is_inverse (const struct pixman_transform *a,
                             const struct pixman_transform *b)
{
    struct pixman_transform t;

    pixman_transform_multiply (&t, a, b);

    return pixman_transform_is_identity (&t);
}

PIXMAN_EXPORT void
pixman_f_transform_from_pixman_transform (struct pixman_f_transform *    ft,
                                          const struct pixman_transform *t)
{
    int i, j;

    for (j = 0; j < 3; j++)
    {
	for (i = 0; i < 3; i++)
	    ft->m[j][i] = pixman_fixed_to_double (t->matrix[j][i]);
    }
}

PIXMAN_EXPORT pixman_bool_t
pixman_transform_from_pixman_f_transform (struct pixman_transform *        t,
                                          const struct pixman_f_transform *ft)
{
    int i, j;

    for (j = 0; j < 3; j++)
    {
	for (i = 0; i < 3; i++)
	{
	    double d = ft->m[j][i];
	    if (d < -32767.0 || d > 32767.0)
		return FALSE;
	    d = d * 65536.0 + 0.5;
	    t->matrix[j][i] = (pixman_fixed_t) floor (d);
	}
    }
    
    return TRUE;
}

static const int a[3] = { 3, 3, 2 };
static const int b[3] = { 2, 1, 1 };

PIXMAN_EXPORT pixman_bool_t
pixman_f_transform_invert (struct pixman_f_transform *      dst,
                           const struct pixman_f_transform *src)
{
    double det;
    int i, j;
    static int a[3] = { 2, 2, 1 };
    static int b[3] = { 1, 0, 0 };

    det = 0;
    for (i = 0; i < 3; i++)
    {
	double p;
	int ai = a[i];
	int bi = b[i];
	p = src->m[i][0] * (src->m[ai][2] * src->m[bi][1] -
	                    src->m[ai][1] * src->m[bi][2]);
	if (i == 1)
	    p = -p;
	det += p;
    }
    
    if (det == 0)
	return FALSE;
    
    det = 1 / det;
    for (j = 0; j < 3; j++)
    {
	for (i = 0; i < 3; i++)
	{
	    double p;
	    int ai = a[i];
	    int aj = a[j];
	    int bi = b[i];
	    int bj = b[j];

	    p = (src->m[ai][aj] * src->m[bi][bj] -
	         src->m[ai][bj] * src->m[bi][aj]);
	    
	    if (((i + j) & 1) != 0)
		p = -p;
	    
	    dst->m[j][i] = det * p;
	}
    }

    return TRUE;
}

PIXMAN_EXPORT pixman_bool_t
pixman_f_transform_point (const struct pixman_f_transform *t,
                          struct pixman_f_vector *         v)
{
    struct pixman_f_vector result;
    int i, j;
    double a;

    for (j = 0; j < 3; j++)
    {
	a = 0;
	for (i = 0; i < 3; i++)
	    a += t->m[j][i] * v->v[i];
	result.v[j] = a;
    }
    
    if (!result.v[2])
	return FALSE;

    for (j = 0; j < 2; j++)
	v->v[j] = result.v[j] / result.v[2];

    v->v[2] = 1;

    return TRUE;
}

PIXMAN_EXPORT void
pixman_f_transform_point_3d (const struct pixman_f_transform *t,
                             struct pixman_f_vector *         v)
{
    struct pixman_f_vector result;
    int i, j;
    double a;

    for (j = 0; j < 3; j++)
    {
	a = 0;
	for (i = 0; i < 3; i++)
	    a += t->m[j][i] * v->v[i];
	result.v[j] = a;
    }
    
    *v = result;
}

PIXMAN_EXPORT void
pixman_f_transform_multiply (struct pixman_f_transform *      dst,
                             const struct pixman_f_transform *l,
                             const struct pixman_f_transform *r)
{
    struct pixman_f_transform d;
    int dx, dy;
    int o;

    for (dy = 0; dy < 3; dy++)
    {
	for (dx = 0; dx < 3; dx++)
	{
	    double v = 0;
	    for (o = 0; o < 3; o++)
		v += l->m[dy][o] * r->m[o][dx];
	    d.m[dy][dx] = v;
	}
    }
    
    *dst = d;
}

PIXMAN_EXPORT void
pixman_f_transform_init_scale (struct pixman_f_transform *t,
                               double                     sx,
                               double                     sy)
{
    t->m[0][0] = sx;
    t->m[0][1] = 0;
    t->m[0][2] = 0;
    t->m[1][0] = 0;
    t->m[1][1] = sy;
    t->m[1][2] = 0;
    t->m[2][0] = 0;
    t->m[2][1] = 0;
    t->m[2][2] = 1;
}

PIXMAN_EXPORT pixman_bool_t
pixman_f_transform_scale (struct pixman_f_transform *forward,
                          struct pixman_f_transform *reverse,
                          double                     sx,
                          double                     sy)
{
    struct pixman_f_transform t;

    if (sx == 0 || sy == 0)
	return FALSE;

    if (forward)
    {
	pixman_f_transform_init_scale (&t, sx, sy);
	pixman_f_transform_multiply (forward, &t, forward);
    }
    
    if (reverse)
    {
	pixman_f_transform_init_scale (&t, 1 / sx, 1 / sy);
	pixman_f_transform_multiply (reverse, reverse, &t);
    }
    
    return TRUE;
}

PIXMAN_EXPORT void
pixman_f_transform_init_rotate (struct pixman_f_transform *t,
                                double                     c,
                                double                     s)
{
    t->m[0][0] = c;
    t->m[0][1] = -s;
    t->m[0][2] = 0;
    t->m[1][0] = s;
    t->m[1][1] = c;
    t->m[1][2] = 0;
    t->m[2][0] = 0;
    t->m[2][1] = 0;
    t->m[2][2] = 1;
}

PIXMAN_EXPORT pixman_bool_t
pixman_f_transform_rotate (struct pixman_f_transform *forward,
                           struct pixman_f_transform *reverse,
                           double                     c,
                           double                     s)
{
    struct pixman_f_transform t;

    if (forward)
    {
	pixman_f_transform_init_rotate (&t, c, s);
	pixman_f_transform_multiply (forward, &t, forward);
    }
    
    if (reverse)
    {
	pixman_f_transform_init_rotate (&t, c, -s);
	pixman_f_transform_multiply (reverse, reverse, &t);
    }

    return TRUE;
}

PIXMAN_EXPORT void
pixman_f_transform_init_translate (struct pixman_f_transform *t,
                                   double                     tx,
                                   double                     ty)
{
    t->m[0][0] = 1;
    t->m[0][1] = 0;
    t->m[0][2] = tx;
    t->m[1][0] = 0;
    t->m[1][1] = 1;
    t->m[1][2] = ty;
    t->m[2][0] = 0;
    t->m[2][1] = 0;
    t->m[2][2] = 1;
}

PIXMAN_EXPORT pixman_bool_t
pixman_f_transform_translate (struct pixman_f_transform *forward,
                              struct pixman_f_transform *reverse,
                              double                     tx,
                              double                     ty)
{
    struct pixman_f_transform t;

    if (forward)
    {
	pixman_f_transform_init_translate (&t, tx, ty);
	pixman_f_transform_multiply (forward, &t, forward);
    }

    if (reverse)
    {
	pixman_f_transform_init_translate (&t, -tx, -ty);
	pixman_f_transform_multiply (reverse, reverse, &t);
    }

    return TRUE;
}

PIXMAN_EXPORT pixman_bool_t
pixman_f_transform_bounds (const struct pixman_f_transform *t,
                           struct pixman_box16 *            b)
{
    struct pixman_f_vector v[4];
    int i;
    int x1, y1, x2, y2;

    v[0].v[0] = b->x1;
    v[0].v[1] = b->y1;
    v[0].v[2] = 1;
    v[1].v[0] = b->x2;
    v[1].v[1] = b->y1;
    v[1].v[2] = 1;
    v[2].v[0] = b->x2;
    v[2].v[1] = b->y2;
    v[2].v[2] = 1;
    v[3].v[0] = b->x1;
    v[3].v[1] = b->y2;
    v[3].v[2] = 1;

    for (i = 0; i < 4; i++)
    {
	if (!pixman_f_transform_point (t, &v[i]))
	    return FALSE;

	x1 = floor (v[i].v[0]);
	y1 = floor (v[i].v[1]);
	x2 = ceil (v[i].v[0]);
	y2 = ceil (v[i].v[1]);

	if (i == 0)
	{
	    b->x1 = x1;
	    b->y1 = y1;
	    b->x2 = x2;
	    b->y2 = y2;
	}
	else
	{
	    if (x1 < b->x1) b->x1 = x1;
	    if (y1 < b->y1) b->y1 = y1;
	    if (x2 > b->x2) b->x2 = x2;
	    if (y2 > b->y2) b->y2 = y2;
	}
    }

    return TRUE;
}

PIXMAN_EXPORT void
pixman_f_transform_init_identity (struct pixman_f_transform *t)
{
    int i, j;

    for (j = 0; j < 3; j++)
    {
	for (i = 0; i < 3; i++)
	    t->m[j][i] = i == j ? 1 : 0;
    }
}
