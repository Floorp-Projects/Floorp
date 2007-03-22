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
 * exemplified by this function.  A cairo_box_t is a rectangular area
 * represented by the coordinates of the upper left and lower right
 * corners, expressed in fixed point numbers.  A cairo_rectangle_int16_t is
 * also a rectangular area, but represented by the upper left corner
 * and the width and the height, as integer numbers.
 *
 * This function converts a cairo_box_t to a cairo_rectangle_int16_t by
 * increasing the area to the nearest integer coordinates.  We should
 * standardize on cairo_rectangle_int16_t and cairo_rectangle_int16_t, and
 * this function could be renamed to the more reasonable
 * _cairo_rectangle_fixed_round.
 */

void
_cairo_box_round_to_rectangle (cairo_box_t *box, cairo_rectangle_int16_t *rectangle)
{
    rectangle->x = _cairo_fixed_integer_floor (box->p1.x);
    rectangle->y = _cairo_fixed_integer_floor (box->p1.y);
    rectangle->width = _cairo_fixed_integer_ceil (box->p2.x) - rectangle->x;
    rectangle->height = _cairo_fixed_integer_ceil (box->p2.y) - rectangle->y;
}

void
_cairo_rectangle_intersect (cairo_rectangle_int16_t *dest, cairo_rectangle_int16_t *src)
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
