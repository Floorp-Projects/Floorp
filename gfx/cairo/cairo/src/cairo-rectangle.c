/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
 * Copyright © 2005 Red Hat, Inc.
 * Copyright © 2006 Red Hat, Inc.
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

#include "cairoint.h"

/* XXX We currently have a confusing mix of boxes and rectangles as
 * exemplified by this function.  A #cairo_box_t is a rectangular area
 * represented by the coordinates of the upper left and lower right
 * corners, expressed in fixed point numbers.  A #cairo_rectangle_int_t is
 * also a rectangular area, but represented by the upper left corner
 * and the width and the height, as integer numbers.
 *
 * This function converts a #cairo_box_t to a #cairo_rectangle_int_t by
 * increasing the area to the nearest integer coordinates.  We should
 * standardize on #cairo_rectangle_fixed_t and #cairo_rectangle_int_t, and
 * this function could be renamed to the more reasonable
 * _cairo_rectangle_fixed_round.
 */

void
_cairo_box_round_to_rectangle (cairo_box_t *box, cairo_rectangle_int_t *rectangle)
{
    rectangle->x = _cairo_fixed_integer_floor (box->p1.x);
    rectangle->y = _cairo_fixed_integer_floor (box->p1.y);
    rectangle->width = _cairo_fixed_integer_ceil (box->p2.x) - rectangle->x;
    rectangle->height = _cairo_fixed_integer_ceil (box->p2.y) - rectangle->y;
}

void
_cairo_rectangle_intersect (cairo_rectangle_int_t *dest, cairo_rectangle_int_t *src)
{
    int x1, y1, x2, y2;

    x1 = MAX (dest->x, src->x);
    y1 = MAX (dest->y, src->y);
    x2 = MIN (dest->x + dest->width, src->x + src->width);
    y2 = MIN (dest->y + dest->height, src->y + src->height);

    if (x1 >= x2 || y1 >= y2) {
	dest->x = 0;
	dest->y = 0;
	dest->width = 0;
	dest->height = 0;
    } else {
	dest->x = x1;
	dest->y = y1;
	dest->width = x2 - x1;
	dest->height = y2 - y1;
    }
}


#define P1x (line->p1.x)
#define P1y (line->p1.y)
#define P2x (line->p2.x)
#define P2y (line->p2.y)
#define B1x (box->p1.x)
#define B1y (box->p1.y)
#define B2x (box->p2.x)
#define B2y (box->p2.y)

/*
 * Check whether any part of line intersects box.  This function essentially
 * computes whether the ray starting at line->p1 in the direction of line->p2
 * intersects the box before it reaches p2.  Normally, this is done
 * by dividing by the lengths of the line projected onto each axis.  Because
 * we're in fixed point, this function does a bit more work to avoid having to
 * do the division -- we don't care about the actual intersection point, so
 * it's of no interest to us.
 */

cairo_bool_t
_cairo_box_intersects_line_segment (cairo_box_t *box, cairo_line_t *line)
{
    cairo_fixed_t t1=0, t2=0, t3=0, t4=0;
    cairo_int64_t t1y, t2y, t3x, t4x;

    cairo_fixed_t xlen, ylen;

    if (_cairo_box_contains_point(box, &line->p1) ||
	_cairo_box_contains_point(box, &line->p2))
	return TRUE;

    xlen = P2x - P1x;
    ylen = P2y - P1y;

    if (xlen) {
	if (xlen > 0) {
	    t1 = B1x - P1x;
	    t2 = B2x - P1x;
	} else {
	    t1 = P1x - B2x;
	    t2 = P1x - B1x;
	    xlen = - xlen;
	}

	if ((t1 < 0 || t1 > xlen) &&
	    (t2 < 0 || t2 > xlen))
	    return FALSE;
    } else {
	/* Fully vertical line -- check that X is in bounds */
	if (P1x < B1x || P1x > B2x)
	    return FALSE;
    }

    if (ylen) {
	if (ylen > 0) {
	    t3 = B1y - P1y;
	    t4 = B2y - P1y;
	} else {
	    t3 = P1y - B2y;
	    t4 = P1y - B1y;
	    ylen = - ylen;
	}

	if ((t3 < 0 || t3 > ylen) &&
	    (t4 < 0 || t4 > ylen))
	    return FALSE;
    } else {
	/* Fully horizontal line -- check Y */
	if (P1y < B1y || P1y > B2y)
	    return FALSE;
    }

    /* If we had a horizontal or vertical line, then it's already been checked */
    if (P1x == P2x || P1y == P2y)
	return TRUE;

    /* Check overlap.  Note that t1 < t2 and t3 < t4 here. */
    t1y = _cairo_int32x32_64_mul (t1, ylen);
    t2y = _cairo_int32x32_64_mul (t2, ylen);
    t3x = _cairo_int32x32_64_mul (t3, xlen);
    t4x = _cairo_int32x32_64_mul (t4, xlen);

    if (_cairo_int64_lt(t1y, t4x) &&
	_cairo_int64_lt(t3x, t2y))
	return TRUE;

    return FALSE;
}

cairo_bool_t
_cairo_box_contains_point (cairo_box_t *box, cairo_point_t *point)
{
    if (point->x < box->p1.x || point->x > box->p2.x ||
	point->y < box->p1.y || point->y > box->p2.y)
	return FALSE;
    return TRUE;
}
