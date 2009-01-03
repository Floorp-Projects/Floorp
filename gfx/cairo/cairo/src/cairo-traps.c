/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/*
 * Copyright © 2002 Keith Packard
 * Copyright © 2007 Red Hat, Inc.
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
 * The Initial Developer of the Original Code is Keith Packard
 *
 * Contributor(s):
 *	Keith R. Packard <keithp@keithp.com>
 *	Carl D. Worth <cworth@cworth.org>
 *
 * 2002-07-15: Converted from XRenderCompositeDoublePoly to #cairo_trap_t. Carl D. Worth
 */

#include "cairoint.h"

/* private functions */

static int
_compare_point_fixed_by_y (const void *av, const void *bv);

void
_cairo_traps_init (cairo_traps_t *traps)
{
    traps->status = CAIRO_STATUS_SUCCESS;

    traps->num_traps = 0;

    traps->traps_size = ARRAY_LENGTH (traps->traps_embedded);
    traps->traps = traps->traps_embedded;
    traps->extents.p1.x = traps->extents.p1.y = INT32_MAX;
    traps->extents.p2.x = traps->extents.p2.y = INT32_MIN;

    traps->has_limits = FALSE;
}

void
_cairo_traps_limit (cairo_traps_t	*traps,
		    cairo_box_t		*limits)
{
    traps->has_limits = TRUE;

    traps->limits = *limits;
}

cairo_bool_t
_cairo_traps_get_limit (cairo_traps_t *traps,
			cairo_box_t   *limits)
{
    *limits = traps->limits;
    return traps->has_limits;
}

void
_cairo_traps_clear (cairo_traps_t *traps)
{
    traps->status = CAIRO_STATUS_SUCCESS;

    traps->num_traps = 0;
    traps->extents.p1.x = traps->extents.p1.y = INT32_MAX;
    traps->extents.p2.x = traps->extents.p2.y = INT32_MIN;
}

void
_cairo_traps_fini (cairo_traps_t *traps)
{
    if (traps->traps != traps->traps_embedded)
	free (traps->traps);
}

/**
 * _cairo_traps_init_box:
 * @traps: a #cairo_traps_t
 * @box: a box that will be converted to a single trapezoid
 *       to store in @traps.
 *
 * Initializes a #cairo_traps_t to contain a single rectangular
 * trapezoid.
 **/
void
_cairo_traps_init_box (cairo_traps_t *traps,
		       const cairo_box_t   *box)
{
    _cairo_traps_init (traps);

    assert (traps->traps_size >= 1);

    traps->num_traps = 1;

    traps->traps[0].top = box->p1.y;
    traps->traps[0].bottom = box->p2.y;
    traps->traps[0].left.p1 = box->p1;
    traps->traps[0].left.p2.x = box->p1.x;
    traps->traps[0].left.p2.y = box->p2.y;
    traps->traps[0].right.p1.x = box->p2.x;
    traps->traps[0].right.p1.y = box->p1.y;
    traps->traps[0].right.p2 = box->p2;

    traps->extents = *box;
}

/* make room for at least one more trap */
static cairo_bool_t
_cairo_traps_grow (cairo_traps_t *traps)
{
    cairo_trapezoid_t *new_traps;
    int new_size = 2 * MAX (traps->traps_size, 16);

    if (traps->traps == traps->traps_embedded) {
	new_traps = _cairo_malloc_ab (new_size, sizeof (cairo_trapezoid_t));
	if (new_traps != NULL)
	    memcpy (new_traps, traps->traps, sizeof (traps->traps_embedded));
    } else {
	new_traps = _cairo_realloc_ab (traps->traps,
	                               new_size, sizeof (cairo_trapezoid_t));
    }

    if (new_traps == NULL) {
	traps->status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	return FALSE;
    }

    traps->traps = new_traps;
    traps->traps_size = new_size;
    return TRUE;
}

void
_cairo_traps_add_trap (cairo_traps_t *traps,
		       cairo_fixed_t top, cairo_fixed_t bottom,
		       cairo_line_t *left, cairo_line_t *right)
{
    cairo_trapezoid_t *trap;

    /* Note: With the goofy trapezoid specification, (where an
     * arbitrary two points on the lines can specified for the left
     * and right edges), these limit checks would not work in
     * general. For example, one can imagine a trapezoid entirely
     * within the limits, but with two points used to specify the left
     * edge entirely to the right of the limits.  Fortunately, for our
     * purposes, cairo will never generate such a crazy
     * trapezoid. Instead, cairo always uses for its points the
     * extreme positions of the edge that are visible on at least some
     * trapezoid. With this constraint, it's impossible for both
     * points to be outside the limits while the relevant edge is
     * entirely inside the limits.
     */
    if (traps->has_limits) {
	/* Trivially reject if trapezoid is entirely to the right or
	 * to the left of the limits. */
	if (left->p1.x >= traps->limits.p2.x &&
	    left->p2.x >= traps->limits.p2.x)
	{
	    return;
	}

	if (right->p1.x <= traps->limits.p1.x &&
	    right->p2.x <= traps->limits.p1.x)
	{
	    return;
	}

	/* And reject if the trapezoid is entirely above or below */
	if (top > traps->limits.p2.y || bottom < traps->limits.p1.y)
	    return;

	/* Otherwise, clip the trapezoid to the limits. We only clip
	 * where an edge is entirely outside the limits. If we wanted
	 * to be more clever, we could handle cases where a trapezoid
	 * edge intersects the edge of the limits, but that would
	 * require slicing this trapezoid into multiple trapezoids,
	 * and I'm not sure the effort would be worth it. */
	if (top < traps->limits.p1.y)
	    top = traps->limits.p1.y;

	if (bottom > traps->limits.p2.y)
	    bottom = traps->limits.p2.y;

	if (left->p1.x <= traps->limits.p1.x &&
	    left->p2.x <= traps->limits.p1.x)
	{
	    left->p1.x = traps->limits.p1.x;
	    left->p2.x = traps->limits.p1.x;
	}

	if (right->p1.x >= traps->limits.p2.x &&
	    right->p2.x >= traps->limits.p2.x)
	{
	    right->p1.x = traps->limits.p2.x;
	    right->p2.x = traps->limits.p2.x;
	}
    }

    /* Trivial discards for empty trapezoids that are likely to be produced
     * by our tessellators (most notably convex_quad when given a simple
     * rectangle).
     */
    if (top >= bottom)
	return;
    /* cheap colinearity check */
    if (right->p1.x <= left->p1.x && right->p1.y == left->p1.y &&
	right->p2.x <= left->p2.x && right->p2.y == left->p2.y)
	return;

    if (traps->num_traps == traps->traps_size) {
	if (! _cairo_traps_grow (traps))
	    return;
    }

    trap = &traps->traps[traps->num_traps];
    trap->top = top;
    trap->bottom = bottom;
    trap->left = *left;
    trap->right = *right;

    if (top < traps->extents.p1.y)
	traps->extents.p1.y = top;
    if (bottom > traps->extents.p2.y)
	traps->extents.p2.y = bottom;
    /*
     * This isn't generally accurate, but it is close enough for
     * this purpose.  Assuming that the left and right segments always
     * contain the trapezoid vertical extents, these compares will
     * yield a containing box.  Assuming that the points all come from
     * the same figure which will eventually be completely drawn, then
     * the compares will yield the correct overall extents
     */
    if (left->p1.x < traps->extents.p1.x)
	traps->extents.p1.x = left->p1.x;
    if (left->p2.x < traps->extents.p1.x)
	traps->extents.p1.x = left->p2.x;

    if (right->p1.x > traps->extents.p2.x)
	traps->extents.p2.x = right->p1.x;
    if (right->p2.x > traps->extents.p2.x)
	traps->extents.p2.x = right->p2.x;

    traps->num_traps++;
}

static int
_compare_point_fixed_by_y (const void *av, const void *bv)
{
    const cairo_point_t	*a = av, *b = bv;

    int ret = a->y - b->y;
    if (ret == 0) {
	ret = a->x - b->x;
    }
    return ret;
}

void
_cairo_traps_translate (cairo_traps_t *traps, int x, int y)
{
    cairo_fixed_t xoff, yoff;
    cairo_trapezoid_t *t;
    int i;

    /* Ugh. The cairo_composite/(Render) interface doesn't allow
       an offset for the trapezoids. Need to manually shift all
       the coordinates to align with the offset origin of the
       intermediate surface. */

    xoff = _cairo_fixed_from_int (x);
    yoff = _cairo_fixed_from_int (y);

    for (i = 0, t = traps->traps; i < traps->num_traps; i++, t++) {
	t->top += yoff;
	t->bottom += yoff;
	t->left.p1.x += xoff;
	t->left.p1.y += yoff;
	t->left.p2.x += xoff;
	t->left.p2.y += yoff;
	t->right.p1.x += xoff;
	t->right.p1.y += yoff;
	t->right.p2.x += xoff;
	t->right.p2.y += yoff;
    }
}

void
_cairo_trapezoid_array_translate_and_scale (cairo_trapezoid_t *offset_traps,
                                            cairo_trapezoid_t *src_traps,
                                            int num_traps,
                                            double tx, double ty,
                                            double sx, double sy)
{
    int i;
    cairo_fixed_t xoff = _cairo_fixed_from_double (tx);
    cairo_fixed_t yoff = _cairo_fixed_from_double (ty);

    if (sx == 1.0 && sy == 1.0) {
        for (i = 0; i < num_traps; i++) {
            offset_traps[i].top = src_traps[i].top + yoff;
            offset_traps[i].bottom = src_traps[i].bottom + yoff;
            offset_traps[i].left.p1.x = src_traps[i].left.p1.x + xoff;
            offset_traps[i].left.p1.y = src_traps[i].left.p1.y + yoff;
            offset_traps[i].left.p2.x = src_traps[i].left.p2.x + xoff;
            offset_traps[i].left.p2.y = src_traps[i].left.p2.y + yoff;
            offset_traps[i].right.p1.x = src_traps[i].right.p1.x + xoff;
            offset_traps[i].right.p1.y = src_traps[i].right.p1.y + yoff;
            offset_traps[i].right.p2.x = src_traps[i].right.p2.x + xoff;
            offset_traps[i].right.p2.y = src_traps[i].right.p2.y + yoff;
        }
    } else {
        cairo_fixed_t xsc = _cairo_fixed_from_double (sx);
        cairo_fixed_t ysc = _cairo_fixed_from_double (sy);

        for (i = 0; i < num_traps; i++) {
            offset_traps[i].top = _cairo_fixed_mul (src_traps[i].top + yoff, ysc);
            offset_traps[i].bottom = _cairo_fixed_mul (src_traps[i].bottom + yoff, ysc);
            offset_traps[i].left.p1.x = _cairo_fixed_mul (src_traps[i].left.p1.x + xoff, xsc);
            offset_traps[i].left.p1.y = _cairo_fixed_mul (src_traps[i].left.p1.y + yoff, ysc);
            offset_traps[i].left.p2.x = _cairo_fixed_mul (src_traps[i].left.p2.x + xoff, xsc);
            offset_traps[i].left.p2.y = _cairo_fixed_mul (src_traps[i].left.p2.y + yoff, ysc);
            offset_traps[i].right.p1.x = _cairo_fixed_mul (src_traps[i].right.p1.x + xoff, xsc);
            offset_traps[i].right.p1.y = _cairo_fixed_mul (src_traps[i].right.p1.y + yoff, ysc);
            offset_traps[i].right.p2.x = _cairo_fixed_mul (src_traps[i].right.p2.x + xoff, xsc);
            offset_traps[i].right.p2.y = _cairo_fixed_mul (src_traps[i].right.p2.y + yoff, ysc);
        }
    }
}

/* A triangle is simply a degenerate case of a convex
 * quadrilateral. We would not benefit from having any distinct
 * implementation of triangle vs. quadrilateral tessellation here. */
cairo_status_t
_cairo_traps_tessellate_triangle (cairo_traps_t *traps,
				  const cairo_point_t t[3])
{
    cairo_point_t quad[4];

    quad[0] = t[0];
    quad[1] = t[0];
    quad[2] = t[1];
    quad[3] = t[2];

    return _cairo_traps_tessellate_convex_quad (traps, quad);
}

cairo_status_t
_cairo_traps_tessellate_rectangle (cairo_traps_t *traps,
				   const cairo_point_t *top_left,
				   const cairo_point_t *bottom_right)
{
    cairo_line_t left;
    cairo_line_t right;

     left.p1.x =  left.p2.x = top_left->x;
     left.p1.y = right.p1.y = top_left->y;
    right.p1.x = right.p2.x = bottom_right->x;
     left.p2.y = right.p2.y = bottom_right->y;

    _cairo_traps_add_trap (traps, top_left->y, bottom_right->y, &left, &right);

    return traps->status;
}

cairo_status_t
_cairo_traps_tessellate_convex_quad (cairo_traps_t *traps,
				     const cairo_point_t q[4])
{
    int a, b, c, d;
    int i;
    cairo_slope_t ab, ad;
    cairo_bool_t b_left_of_d;
    cairo_line_t left;
    cairo_line_t right;

    /* Choose a as a point with minimal y */
    a = 0;
    for (i = 1; i < 4; i++)
	if (_compare_point_fixed_by_y (&q[i], &q[a]) < 0)
	    a = i;

    /* b and d are adjacent to a, while c is opposite */
    b = (a + 1) % 4;
    c = (a + 2) % 4;
    d = (a + 3) % 4;

    /* Choose between b and d so that b.y is less than d.y */
    if (_compare_point_fixed_by_y (&q[d], &q[b]) < 0) {
	b = (a + 3) % 4;
	d = (a + 1) % 4;
    }

    /* Without freedom left to choose anything else, we have four
     * cases to tessellate.
     *
     * First, we have to determine the Y-axis sort of the four
     * vertices, (either abcd or abdc). After that we need to detemine
     * which edges will be "left" and which will be "right" in the
     * resulting trapezoids. This can be determined by computing a
     * slope comparison of ab and ad to determine if b is left of d or
     * not.
     *
     * Note that "left of" here is in the sense of which edges should
     * be the left vs. right edges of the trapezoid. In particular, b
     * left of d does *not* mean that b.x is less than d.x.
     *
     * This should hopefully be made clear in the lame ASCII art
     * below. Since the same slope comparison is used in all cases, we
     * compute it before testing for the Y-value sort. */

    /* Note: If a == b then the ab slope doesn't give us any
     * information. In that case, we can replace it with the ac (or
     * equivalenly the bc) slope which gives us exactly the same
     * information we need. At worst the names of the identifiers ab
     * and b_left_of_d are inaccurate in this case, (would be ac, and
     * c_left_of_d). */
    if (q[a].x == q[b].x && q[a].y == q[b].y)
	_cairo_slope_init (&ab, &q[a], &q[c]);
    else
	_cairo_slope_init (&ab, &q[a], &q[b]);

    _cairo_slope_init (&ad, &q[a], &q[d]);

    b_left_of_d = (_cairo_slope_compare (&ab, &ad) > 0);

    if (q[c].y <= q[d].y) {
	if (b_left_of_d) {
	    /* Y-sort is abcd and b is left of d, (slope(ab) > slope (ad))
	     *
	     *                      top bot left right
	     *        _a  a  a
	     *      / /  /|  |\      a.y b.y  ab   ad
	     *     b /  b |  b \
	     *    / /   | |   \ \    b.y c.y  bc   ad
	     *   c /    c |    c \
	     *  | /      \|     \ \  c.y d.y  cd   ad
	     *  d         d       d
	     */
	    left.p1  = q[a]; left.p2  = q[b];
	    right.p1 = q[a]; right.p2 = q[d];
	    _cairo_traps_add_trap (traps, q[a].y, q[b].y, &left, &right);
	    left.p1  = q[b]; left.p2  = q[c];
	    _cairo_traps_add_trap (traps, q[b].y, q[c].y, &left, &right);
	    left.p1  = q[c]; left.p2  = q[d];
	    _cairo_traps_add_trap (traps, q[c].y, q[d].y, &left, &right);
	} else {
	    /* Y-sort is abcd and b is right of d, (slope(ab) <= slope (ad))
	     *
	     *       a  a  a_
	     *      /|  |\  \ \     a.y b.y  ad  ab
	     *     / b  | b  \ b
	     *    / /   | |   \ \   b.y c.y  ad  bc
	     *   / c    | c    \ c
	     *  / /     |/      \ | c.y d.y  ad  cd
	     *  d       d         d
	     */
	    left.p1  = q[a]; left.p2  = q[d];
	    right.p1 = q[a]; right.p2 = q[b];
	    _cairo_traps_add_trap (traps, q[a].y, q[b].y, &left, &right);
	    right.p1 = q[b]; right.p2 = q[c];
	    _cairo_traps_add_trap (traps, q[b].y, q[c].y, &left, &right);
	    right.p1 = q[c]; right.p2 = q[d];
	    _cairo_traps_add_trap (traps, q[c].y, q[d].y, &left, &right);
	}
    } else {
	if (b_left_of_d) {
	    /* Y-sort is abdc and b is left of d, (slope (ab) > slope (ad))
	     *
	     *        a   a     a
	     *       //  / \    |\     a.y b.y  ab  ad
	     *     /b/  b   \   b \
	     *    / /    \   \   \ \   b.y d.y  bc  ad
	     *   /d/      \   d   \ d
	     *  //         \ /     \|  d.y c.y  bc  dc
	     *  c           c       c
	     */
	    left.p1  = q[a]; left.p2  = q[b];
	    right.p1 = q[a]; right.p2 = q[d];
	    _cairo_traps_add_trap (traps, q[a].y, q[b].y, &left, &right);
	    left.p1  = q[b]; left.p2  = q[c];
	    _cairo_traps_add_trap (traps, q[b].y, q[d].y, &left, &right);
	    right.p1 = q[d]; right.p2 = q[c];
	    _cairo_traps_add_trap (traps, q[d].y, q[c].y, &left, &right);
	} else {
	    /* Y-sort is abdc and b is right of d, (slope (ab) <= slope (ad))
	     *
	     *      a     a   a
	     *     /|    / \  \\       a.y b.y  ad  ab
	     *    / b   /   b  \b\
	     *   / /   /   /    \ \    b.y d.y  ad  bc
	     *  d /   d   /	 \d\
	     *  |/     \ /         \\  d.y c.y  dc  bc
	     *  c       c	   c
	     */
	    left.p1  = q[a]; left.p2  = q[d];
	    right.p1 = q[a]; right.p2 = q[b];
	    _cairo_traps_add_trap (traps, q[a].y, q[b].y, &left, &right);
	    right.p1 = q[b]; right.p2 = q[c];
	    _cairo_traps_add_trap (traps, q[b].y, q[d].y, &left, &right);
	    left.p1  = q[d]; left.p2  = q[c];
	    _cairo_traps_add_trap (traps, q[d].y, q[c].y, &left, &right);
	}
    }

    return traps->status;
}

static cairo_bool_t
_cairo_trap_contains (cairo_trapezoid_t *t, cairo_point_t *pt)
{
    cairo_slope_t slope_left, slope_pt, slope_right;

    if (t->top > pt->y)
	return FALSE;
    if (t->bottom < pt->y)
	return FALSE;

    _cairo_slope_init (&slope_left, &t->left.p1, &t->left.p2);
    _cairo_slope_init (&slope_pt, &t->left.p1, pt);

    if (_cairo_slope_compare (&slope_left, &slope_pt) < 0)
	return FALSE;

    _cairo_slope_init (&slope_right, &t->right.p1, &t->right.p2);
    _cairo_slope_init (&slope_pt, &t->right.p1, pt);

    if (_cairo_slope_compare (&slope_pt, &slope_right) < 0)
	return FALSE;

    return TRUE;
}

cairo_bool_t
_cairo_traps_contain (const cairo_traps_t *traps,
		      double x, double y)
{
    int i;
    cairo_point_t point;

    point.x = _cairo_fixed_from_double (x);
    point.y = _cairo_fixed_from_double (y);

    for (i = 0; i < traps->num_traps; i++) {
	if (_cairo_trap_contains (&traps->traps[i], &point))
	    return TRUE;
    }

    return FALSE;
}

void
_cairo_traps_extents (const cairo_traps_t *traps,
		      cairo_box_t         *extents)
{
    if (traps->num_traps == 0) {
	extents->p1.x = extents->p1.y = _cairo_fixed_from_int (0);
	extents->p2.x = extents->p2.y = _cairo_fixed_from_int (0);
    } else {
	*extents = traps->extents;
	if (traps->has_limits) {
	    /* clip the traps to the imposed limits */
	    if (extents->p1.x < traps->limits.p1.x)
		extents->p1.x = traps->limits.p1.x;
	    if (extents->p2.x > traps->limits.p2.x)
		extents->p2.x = traps->limits.p2.x;

	    if (extents->p1.y < traps->limits.p1.y)
		extents->p1.y = traps->limits.p1.y;
	    if (extents->p2.y > traps->limits.p2.y)
		extents->p2.y = traps->limits.p2.y;
	}
    }
}

/**
 * _cairo_traps_extract_region:
 * @traps: a #cairo_traps_t
 * @region: a #cairo_region_t
 *
 * Determines if a set of trapezoids are exactly representable as a
 * cairo region.  If so, the passed-in region is initialized to
 * the area representing the given traps.  It should be finalized
 * with _cairo_region_fini().  If not, %CAIRO_INT_STATUS_UNSUPPORTED
 * is returned.
 *
 * Return value: %CAIRO_STATUS_SUCCESS, %CAIRO_INT_STATUS_UNSUPPORTED
 * or %CAIRO_STATUS_NO_MEMORY
 **/
cairo_int_status_t
_cairo_traps_extract_region (const cairo_traps_t  *traps,
			     cairo_region_t       *region)
{
    cairo_box_int_t stack_boxes[CAIRO_STACK_ARRAY_LENGTH (cairo_box_int_t)];
    cairo_box_int_t *boxes = stack_boxes;
    int i, box_count;
    cairo_int_status_t status;

    for (i = 0; i < traps->num_traps; i++)
	if (!(traps->traps[i].left.p1.x == traps->traps[i].left.p2.x
	      && traps->traps[i].right.p1.x == traps->traps[i].right.p2.x
	      && _cairo_fixed_is_integer(traps->traps[i].top)
	      && _cairo_fixed_is_integer(traps->traps[i].bottom)
	      && _cairo_fixed_is_integer(traps->traps[i].left.p1.x)
	      && _cairo_fixed_is_integer(traps->traps[i].right.p1.x))) {
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}

    if (traps->num_traps > ARRAY_LENGTH(stack_boxes)) {
	boxes = _cairo_malloc_ab (traps->num_traps, sizeof(cairo_box_int_t));

	if (boxes == NULL)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    box_count = 0;

    for (i = 0; i < traps->num_traps; i++) {
	int x1 = _cairo_fixed_integer_part(traps->traps[i].left.p1.x);
	int y1 = _cairo_fixed_integer_part(traps->traps[i].top);
	int x2 = _cairo_fixed_integer_part(traps->traps[i].right.p1.x);
	int y2 = _cairo_fixed_integer_part(traps->traps[i].bottom);

	/* XXX: Sometimes we get degenerate trapezoids from the tesellator;
	 * skip these.
	 */
	if (x1 == x2 || y1 == y2)
	    continue;

	boxes[box_count].p1.x = x1;
	boxes[box_count].p1.y = y1;
	boxes[box_count].p2.x = x2;
	boxes[box_count].p2.y = y2;

	box_count++;
    }

    status = _cairo_region_init_boxes (region, boxes, box_count);

    if (boxes != stack_boxes)
	free (boxes);

    if (status)
	_cairo_region_fini (region);

    return status;
}

/* moves trap points such that they become the actual corners of the trapezoid */
static void
_sanitize_trap (cairo_trapezoid_t *t)
{
    cairo_trapezoid_t s = *t;

#define FIX(lr, tb, p) \
    if (t->lr.p.y != t->tb) { \
        t->lr.p.x = s.lr.p2.x + _cairo_fixed_mul_div (s.lr.p1.x - s.lr.p2.x, s.tb - s.lr.p2.y, s.lr.p1.y - s.lr.p2.y); \
        t->lr.p.y = s.tb; \
    }
    FIX (left,  top,    p1);
    FIX (left,  bottom, p2);
    FIX (right, top,    p1);
    FIX (right, bottom, p2);
}

cairo_private cairo_status_t
_cairo_traps_path (const cairo_traps_t *traps,
		   cairo_path_fixed_t  *path)
{
    int i;

    for (i = 0; i < traps->num_traps; i++) {
	cairo_status_t status;
	cairo_trapezoid_t trap = traps->traps[i];

	if (trap.top == trap.bottom)
	    continue;

	_sanitize_trap (&trap);

	status = _cairo_path_fixed_move_to (path, trap.left.p1.x, trap.top);
	if (status) return status;
	status = _cairo_path_fixed_line_to (path, trap.right.p1.x, trap.top);
	if (status) return status;
	status = _cairo_path_fixed_line_to (path, trap.right.p2.x, trap.bottom);
	if (status) return status;
	status = _cairo_path_fixed_line_to (path, trap.left.p2.x, trap.bottom);
	if (status) return status;
	status = _cairo_path_fixed_close_path (path);
	if (status) return status;
    }

    return CAIRO_STATUS_SUCCESS;
}
