/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
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
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <math.h>

#include "cairoint.h"

static cairo_matrix_t const CAIRO_MATRIX_IDENTITY = {
    {
	{1, 0},
	{0, 1},
	{0, 0}
    }
};

static void
_cairo_matrix_scalar_multiply (cairo_matrix_t *matrix, double scalar);

static void
_cairo_matrix_compute_adjoint (cairo_matrix_t *matrix);

/**
 * cairo_matrix_create:
 * 
 * Creates a new identity matrix.
 * 
 * Return value: a newly created matrix; free with cairo_matrix_destroy(),
 *  or %NULL if memory couldn't be allocated.
 **/
cairo_matrix_t *
cairo_matrix_create (void)
{
    cairo_matrix_t *matrix;

    matrix = malloc (sizeof (cairo_matrix_t));
    if (matrix == NULL)
	return NULL;

    _cairo_matrix_init (matrix);

    return matrix;
}

void
_cairo_matrix_init (cairo_matrix_t *matrix)
{
    cairo_matrix_set_identity (matrix);
}

void
_cairo_matrix_fini (cairo_matrix_t *matrix)
{
    /* nothing to do here */
}

/**
 * cairo_matrix_destroy:
 * @matrix: a #cairo_matrix_t
 * 
 * Frees a matrix created with cairo_matrix_create.
 **/
void
cairo_matrix_destroy (cairo_matrix_t *matrix)
{
    _cairo_matrix_fini (matrix);
    free (matrix);
}

/**
 * cairo_matrix_copy:
 * @matrix: a #cairo_matrix_t
 * @other: another #cairo_
 * 
 * Modifies @matrix to be identical to @other.
 * 
 * Return value: %CAIRO_STATUS_SUCCESS, always.
 **/
cairo_status_t
cairo_matrix_copy (cairo_matrix_t *matrix, const cairo_matrix_t *other)
{
    *matrix = *other;

    return CAIRO_STATUS_SUCCESS;
}
slim_hidden_def(cairo_matrix_copy);

/**
 * cairo_matrix_set_identity:
 * @matrix: a #cairo_matrix_t
 * 
 * Modifies @matrix to be an identity transformation.
 * 
 * Return value: %CAIRO_STATUS_SUCCESS, always.
 **/
cairo_status_t
cairo_matrix_set_identity (cairo_matrix_t *matrix)
{
    *matrix = CAIRO_MATRIX_IDENTITY;

    return CAIRO_STATUS_SUCCESS;
}
slim_hidden_def(cairo_matrix_set_identity);

/**
 * cairo_matrix_set_affine:
 * @matrix: a cairo_matrix_t
 * @a: a component of the affine transformation
 * @b: b component of the affine transformation
 * @c: c component of the affine transformation
 * @d: d component of the affine transformation
 * @tx: X translation component of the affine transformation
 * @ty: Y translation component of the affine transformation
 * 
 * Sets @matrix to be the affine transformation given by
 * @a, b, @c, @d, @tx, @ty. The transformation is given
 * by:
 * <programlisting>
 *  x_new = x * a + y * c + tx;
 *  y_new = x * b + y * d + ty;
 * </programlisting>
 *
 * Return value: %CAIRO_STATUS_SUCCESS, always.
 **/
cairo_status_t
cairo_matrix_set_affine (cairo_matrix_t *matrix,
			 double a, double b,
			 double c, double d,
			 double tx, double ty)
{
    matrix->m[0][0] =  a; matrix->m[0][1] =  b;
    matrix->m[1][0] =  c; matrix->m[1][1] =  d;
    matrix->m[2][0] = tx; matrix->m[2][1] = ty;

    return CAIRO_STATUS_SUCCESS;
}
slim_hidden_def(cairo_matrix_set_affine);

/**
 * cairo_matrix_get_affine:
 * @matrix: a @cairo_matrix_t
 * @a: location to store a component of affine transformation, or %NULL
 * @b: location to store b component of affine transformation, or %NULL
 * @c: location to store c component of affine transformation, or %NULL
 * @d: location to store d component of affine transformation, or %NULL
 * @tx: location to store X-translation component of affine transformation, or %NULL
 * @ty: location to store Y-translation component of affine transformation, or %NULL
 * 
 * Gets the matrix values for the affine tranformation that @matrix represents.
 * See cairo_matrix_set_affine().
 * 
 * Return value: %CAIRO_STATUS_SUCCESS, always.
 **/
cairo_status_t
cairo_matrix_get_affine (cairo_matrix_t *matrix,
			 double *a, double *b,
			 double *c, double *d,
			 double *tx, double *ty)
{
    if (a)
	*a  = matrix->m[0][0];
    if (b)
	*b  = matrix->m[0][1];

    if (c)
	*c  = matrix->m[1][0];
    if (d)
	*d  = matrix->m[1][1];

    if (tx)
	*tx = matrix->m[2][0];
    if (ty)
	*ty = matrix->m[2][1];

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_matrix_set_translate (cairo_matrix_t *matrix,
			     double tx, double ty)
{
    return cairo_matrix_set_affine (matrix,
				    1, 0,
				    0, 1,
				    tx, ty);
}

/**
 * cairo_matrix_translate:
 * @matrix: a cairo_matrix_t
 * @tx: amount to rotate in the X direction
 * @ty: amount to rotate in the Y direction
 * 
 * Applies a translation by @tx, @ty to the transformation in
 * @matrix. The new transformation is given by first translating by
 * @tx, @ty then applying the original transformation
 * 
 * Return value: %CAIRO_STATUS_SUCCESS, always.
 **/
cairo_status_t
cairo_matrix_translate (cairo_matrix_t *matrix, double tx, double ty)
{
    cairo_matrix_t tmp;

    _cairo_matrix_set_translate (&tmp, tx, ty);

    return cairo_matrix_multiply (matrix, &tmp, matrix);
}

cairo_status_t
_cairo_matrix_set_scale (cairo_matrix_t *matrix,
			 double sx, double sy)
{
    return cairo_matrix_set_affine (matrix,
				    sx,  0,
				    0, sy,
				    0, 0);
}

/**
 * cairo_matrix_scale:
 * @matrix: a #cairo_matrix_t
 * @sx: Scale factor in the X direction
 * @sy: Scale factor in the Y direction
 * 
 * Applies scaling by @tx, @ty to the transformation in
 * @matrix. The new transformation is given by first scaling by @sx
 * and @sy then applying the original transformation
 * 
 * Return value: %CAIRO_STATUS_SUCCESS, always.
 **/
cairo_status_t
cairo_matrix_scale (cairo_matrix_t *matrix, double sx, double sy)
{
    cairo_matrix_t tmp;

    _cairo_matrix_set_scale (&tmp, sx, sy);

    return cairo_matrix_multiply (matrix, &tmp, matrix);
}
slim_hidden_def(cairo_matrix_scale);

cairo_status_t
_cairo_matrix_set_rotate (cairo_matrix_t *matrix,
		   double radians)
{
    double  s;
    double  c;
#if HAVE_SINCOS
    sincos (radians, &s, &c);
#else
    s = sin (radians);
    c = cos (radians);
#endif
    return cairo_matrix_set_affine (matrix,
				    c, s,
				    -s, c,
				    0, 0);
}

/**
 * cairo_matrix_rotate:
 * @matrix: a @cairo_matrix_t
 * @radians: angle of rotation, in radians. Angles are defined
 *  so that an angle of 90 degrees (%M_PI radians) rotates the
 *  positive X axis into the positive Y axis. With the default
 *  Cairo choice of axis orientation, positive rotations are
 *  clockwise.
 * 
 * Applies rotation by @radians to the transformation in
 * @matrix. The new transformation is given by first rotating by
 * @radians then applying the original transformation
 * 
 * Return value: %CAIRO_STATUS_SUCCESS, always.
 **/
cairo_status_t
cairo_matrix_rotate (cairo_matrix_t *matrix, double radians)
{
    cairo_matrix_t tmp;

    _cairo_matrix_set_rotate (&tmp, radians);

    return cairo_matrix_multiply (matrix, &tmp, matrix);
}

/**
 * cairo_matrix_multiply:
 * @result: a @cairo_matrix_t in which to store the result
 * @a: a @cairo_matrix_t
 * @b: a @cairo_matrix_t
 * 
 * Multiplies the affine transformations in @a and @b together
 * and stores the result in @result. The resulting transformation
 * is given by first applying the transformation in @b then
 * applying the transformation in @a.
 * 
 * Return value: %CAIRO_STATUS_SUCCESS, always.
 **/
cairo_status_t
cairo_matrix_multiply (cairo_matrix_t *result, const cairo_matrix_t *a, const cairo_matrix_t *b)
{
    cairo_matrix_t r;
    int	    row, col, n;
    double  t;

    for (row = 0; row < 3; row++) {
	for (col = 0; col < 2; col++) {
	    if (row == 2)
		t = b->m[2][col];
	    else
		t = 0;
	    for (n = 0; n < 2; n++) {
		t += a->m[row][n] * b->m[n][col];
	    }
	    r.m[row][col] = t;
	}
    }

    *result = r;

    return CAIRO_STATUS_SUCCESS;
}
slim_hidden_def(cairo_matrix_multiply);

/**
 * cairo_matrix_transform_distance:
 * @matrix: a @cairo_matrix_t
 * @dx: a distance in the X direction. An in/out parameter
 * @dy: a distance in the Y direction. An in/out parameter
 * 
 * Transforms the vector (@dx,@dy) by @matrix.  Translation is
 * ignored. In terms of the components of the affine transformation:
 *
 * <programlisting>
 * dx2 = dx1 * a + dy1 * c;
 * dy2 = dx1 * b + dy1 * d;
 * </programlisting>
 *
 * Affine transformations are position invariant, so the same vector
 * always transforms to the same vector. If (@x1,@y1) transforms
 * to (@x2,@y2) then (@x1+@dx1,@y1+@dy1) will transform to
 * (@x1+@dx2,@y1+@dy2) for all values of @x1 and @x2.
 * 
 * Return value: %CAIRO_STATUS_SUCCESS, always.
 **/
cairo_status_t
cairo_matrix_transform_distance (cairo_matrix_t *matrix, double *dx, double *dy)
{
    double new_x, new_y;

    new_x = (matrix->m[0][0] * *dx
	     + matrix->m[1][0] * *dy);
    new_y = (matrix->m[0][1] * *dx
	     + matrix->m[1][1] * *dy);

    *dx = new_x;
    *dy = new_y;

    return CAIRO_STATUS_SUCCESS;
}
slim_hidden_def(cairo_matrix_transform_distance);

/**
 * cairo_matrix_transform_point:
 * @matrix: a @cairo_matrix_t
 * @x: X position. An in/out parameter
 * @y: Y position. An in/out parameter
 * 
 * Transforms the point (@x, @y) by @matrix.
 * 
 * Return value: %CAIRO_STATUS_SUCCESS, always.
 **/
cairo_status_t
cairo_matrix_transform_point (cairo_matrix_t *matrix, double *x, double *y)
{
    cairo_matrix_transform_distance (matrix, x, y);

    *x += matrix->m[2][0];
    *y += matrix->m[2][1];

    return CAIRO_STATUS_SUCCESS;
}
slim_hidden_def(cairo_matrix_transform_point);

cairo_status_t
_cairo_matrix_transform_bounding_box (cairo_matrix_t *matrix,
				      double *x, double *y,
				      double *width, double *height)
{
    int i;
    double quad_x[4], quad_y[4];
    double dx1, dy1;
    double dx2, dy2;
    double min_x, max_x;
    double min_y, max_y;

    quad_x[0] = *x;
    quad_y[0] = *y;
    cairo_matrix_transform_point (matrix, &quad_x[0], &quad_y[0]);

    dx1 = *width;
    dy1 = 0;
    cairo_matrix_transform_distance (matrix, &dx1, &dy1);
    quad_x[1] = quad_x[0] + dx1;
    quad_y[1] = quad_y[0] + dy1;

    dx2 = 0;
    dy2 = *height;
    cairo_matrix_transform_distance (matrix, &dx2, &dy2);
    quad_x[2] = quad_x[0] + dx2;
    quad_y[2] = quad_y[0] + dy2;

    quad_x[3] = quad_x[0] + dx1 + dx2;
    quad_y[3] = quad_y[0] + dy1 + dy2;

    min_x = max_x = quad_x[0];
    min_y = max_y = quad_y[0];

    for (i=1; i < 4; i++) {
	if (quad_x[i] < min_x)
	    min_x = quad_x[i];
	if (quad_x[i] > max_x)
	    max_x = quad_x[i];

	if (quad_y[i] < min_y)
	    min_y = quad_y[i];
	if (quad_y[i] > max_y)
	    max_y = quad_y[i];
    }

    *x = min_x;
    *y = min_y;
    *width = max_x - min_x;
    *height = max_y - min_y;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_matrix_scalar_multiply (cairo_matrix_t *matrix, double scalar)
{
    int row, col;

    for (row = 0; row < 3; row++)
	for (col = 0; col < 2; col++)
	    matrix->m[row][col] *= scalar;
}

/* This function isn't a correct adjoint in that the implicit 1 in the
   homogeneous result should actually be ad-bc instead. But, since this
   adjoint is only used in the computation of the inverse, which
   divides by det (A)=ad-bc anyway, everything works out in the end. */
static void
_cairo_matrix_compute_adjoint (cairo_matrix_t *matrix)
{
    /* adj (A) = transpose (C:cofactor (A,i,j)) */
    double a, b, c, d, tx, ty;

    a  = matrix->m[0][0]; b  = matrix->m[0][1];
    c  = matrix->m[1][0]; d  = matrix->m[1][1];
    tx = matrix->m[2][0]; ty = matrix->m[2][1];

    cairo_matrix_set_affine (matrix,
			     d, -b,
			     -c, a,
			     c*ty - d*tx, b*tx - a*ty);
}

/**
 * cairo_matrix_invert:
 * @matrix: a @cairo_matrix_t
 * 
 * Changes @matrix to be the inverse of it's original value. Not
 * all transformation matrices have inverses; if the matrix
 * collapses points together (it is <firstterm>degenerate</firstterm>),
 * then it has no inverse and this function will fail.
 * 
 * Returns: If @matrix has an inverse, modifies @matrix to
 *  be the inverse matrix and returns %CAIRO_STATUS_SUCCESS. Otherwise,
 *  returns %CAIRO_STATUS_INVALID_MATRIX.
 **/
cairo_status_t
cairo_matrix_invert (cairo_matrix_t *matrix)
{
    /* inv (A) = 1/det (A) * adj (A) */
    double det;

    _cairo_matrix_compute_determinant (matrix, &det);
    
    if (det == 0)
	return CAIRO_STATUS_INVALID_MATRIX;

    _cairo_matrix_compute_adjoint (matrix);
    _cairo_matrix_scalar_multiply (matrix, 1 / det);

    return CAIRO_STATUS_SUCCESS;
}
slim_hidden_def(cairo_matrix_invert);

cairo_status_t
_cairo_matrix_compute_determinant (cairo_matrix_t *matrix, double *det)
{
    double a, b, c, d;

    a = matrix->m[0][0]; b = matrix->m[0][1];
    c = matrix->m[1][0]; d = matrix->m[1][1];

    *det = a*d - b*c;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_matrix_compute_eigen_values (cairo_matrix_t *matrix, double *lambda1, double *lambda2)
{
    /* The eigenvalues of an NxN matrix M are found by solving the polynomial:

       det (M - lI) = 0

       The zeros in our homogeneous 3x3 matrix make this equation equal
       to that formed by the sub-matrix:

       M = a b 
           c d

       by which:

       l^2 - (a+d)l + (ad - bc) = 0

       l = (a+d +/- sqrt (a^2 + 2ad + d^2 - 4 (ad-bc))) / 2;
    */

    double a, b, c, d, rad;

    a = matrix->m[0][0];
    b = matrix->m[0][1];
    c = matrix->m[1][0];
    d = matrix->m[1][1];

    rad = sqrt (a*a + 2*a*d + d*d - 4*(a*d - b*c));
    *lambda1 = (a + d + rad) / 2.0;
    *lambda2 = (a + d - rad) / 2.0;

    return CAIRO_STATUS_SUCCESS;
}

/* Compute the amount that each basis vector is scaled by. */
cairo_status_t
_cairo_matrix_compute_scale_factors (cairo_matrix_t *matrix, double *sx, double *sy, int x_major)
{
    double det;

    _cairo_matrix_compute_determinant (matrix, &det);

    if (det == 0)
	*sx = *sy = 0;
    else
    {
	double x = x_major != 0;
	double y = x == 0;
	double major, minor;
	
	cairo_matrix_transform_distance (matrix, &x, &y);
	major = sqrt(x*x + y*y);
	/*
	 * ignore mirroring
	 */
	if (det < 0)
	    det = -det;
	if (major)
	    minor = det / major;
	else 
	    minor = 0.0;
	if (x_major)
	{
	    *sx = major;
	    *sy = minor;
	}
	else
	{
	    *sx = minor;
	    *sy = major;
	}
    }

    return CAIRO_STATUS_SUCCESS;
}

cairo_bool_t 
_cairo_matrix_is_integer_translation(cairo_matrix_t *mat, 
				     int *itx, int *ity)
{
    double a, b, c, d, tx, ty;
    int ttx, tty;
    int ok = 0;
    cairo_matrix_get_affine (mat, &a, &b, &c, &d, &tx, &ty);
    ttx = _cairo_fixed_from_double (tx);
    tty = _cairo_fixed_from_double (ty);
    ok = ((a == 1.0)
	  && (b == 0.0)
	  && (c == 0.0)
	  && (d == 1.0)
	  && (_cairo_fixed_is_integer(ttx))
	  && (_cairo_fixed_is_integer(tty)));
    if (ok) {
	*itx = _cairo_fixed_integer_part(ttx);
	*ity = _cairo_fixed_integer_part(tty);
	return TRUE;
    } 
    return FALSE;
}
