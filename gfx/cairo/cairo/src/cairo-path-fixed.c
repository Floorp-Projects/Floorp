/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
 * Copyright © 2005 Red Hat, Inc.
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
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA
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

#include "cairo-error-private.h"
#include "cairo-path-fixed-private.h"
#include "cairo-slope-private.h"

static cairo_status_t
_cairo_path_fixed_add (cairo_path_fixed_t  *path,
		       cairo_path_op_t	    op,
		       const cairo_point_t *points,
		       int		    num_points);

static void
_cairo_path_fixed_add_buf (cairo_path_fixed_t *path,
			   cairo_path_buf_t   *buf);

static cairo_path_buf_t *
_cairo_path_buf_create (int size_ops, int size_points);

static void
_cairo_path_buf_destroy (cairo_path_buf_t *buf);

static void
_cairo_path_buf_add_op (cairo_path_buf_t *buf,
			cairo_path_op_t   op);

static void
_cairo_path_buf_add_points (cairo_path_buf_t       *buf,
			    const cairo_point_t    *points,
			    int		            num_points);

#define cairo_path_head(path__) (&(path__)->buf.base)
#define cairo_path_tail(path__) cairo_path_buf_prev (cairo_path_head (path__))

#define cairo_path_buf_next(pos__) \
    cairo_list_entry ((pos__)->link.next, cairo_path_buf_t, link)
#define cairo_path_buf_prev(pos__) \
    cairo_list_entry ((pos__)->link.prev, cairo_path_buf_t, link)

#define cairo_path_foreach_buf_start(pos__, path__) \
    pos__ = cairo_path_head (path__); do
#define cairo_path_foreach_buf_end(pos__, path__) \
    while ((pos__ = cairo_path_buf_next (pos__)) !=  cairo_path_head (path__))

void
_cairo_path_fixed_init (cairo_path_fixed_t *path)
{
    VG (VALGRIND_MAKE_MEM_UNDEFINED (path, sizeof (cairo_path_fixed_t)));

    cairo_list_init (&path->buf.base.link);

    path->buf.base.num_ops = 0;
    path->buf.base.num_points = 0;
    path->buf.base.size_ops = ARRAY_LENGTH (path->buf.op);
    path->buf.base.size_points = ARRAY_LENGTH (path->buf.points);
    path->buf.base.op = path->buf.op;
    path->buf.base.points = path->buf.points;

    path->current_point.x = 0;
    path->current_point.y = 0;
    path->last_move_point = path->current_point;
    path->has_last_move_point = FALSE;
    path->has_current_point = FALSE;
    path->has_curve_to = FALSE;
    path->is_rectilinear = TRUE;
    path->maybe_fill_region = TRUE;
    path->is_empty_fill = TRUE;

    path->extents.p1.x = path->extents.p1.y = INT_MAX;
    path->extents.p2.x = path->extents.p2.y = INT_MIN;
}

cairo_status_t
_cairo_path_fixed_init_copy (cairo_path_fixed_t *path,
			     const cairo_path_fixed_t *other)
{
    cairo_path_buf_t *buf, *other_buf;
    unsigned int num_points, num_ops;

    VG (VALGRIND_MAKE_MEM_UNDEFINED (path, sizeof (cairo_path_fixed_t)));

    cairo_list_init (&path->buf.base.link);

    path->buf.base.op = path->buf.op;
    path->buf.base.points = path->buf.points;
    path->buf.base.size_ops = ARRAY_LENGTH (path->buf.op);
    path->buf.base.size_points = ARRAY_LENGTH (path->buf.points);

    path->current_point = other->current_point;
    path->last_move_point = other->last_move_point;
    path->has_last_move_point = other->has_last_move_point;
    path->has_current_point = other->has_current_point;
    path->has_curve_to = other->has_curve_to;
    path->is_rectilinear = other->is_rectilinear;
    path->maybe_fill_region = other->maybe_fill_region;
    path->is_empty_fill = other->is_empty_fill;

    path->extents = other->extents;

    path->buf.base.num_ops = other->buf.base.num_ops;
    path->buf.base.num_points = other->buf.base.num_points;
    memcpy (path->buf.op, other->buf.base.op,
	    other->buf.base.num_ops * sizeof (other->buf.op[0]));
    memcpy (path->buf.points, other->buf.points,
	    other->buf.base.num_points * sizeof (other->buf.points[0]));

    num_points = num_ops = 0;
    for (other_buf = cairo_path_buf_next (cairo_path_head (other));
	 other_buf != cairo_path_head (other);
	 other_buf = cairo_path_buf_next (other_buf))
    {
	num_ops    += other_buf->num_ops;
	num_points += other_buf->num_points;
    }

    if (num_ops) {
	buf = _cairo_path_buf_create (num_ops, num_points);
	if (unlikely (buf == NULL)) {
	    _cairo_path_fixed_fini (path);
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
	}

	for (other_buf = cairo_path_buf_next (cairo_path_head (other));
	     other_buf != cairo_path_head (other);
	     other_buf = cairo_path_buf_next (other_buf))
	{
	    memcpy (buf->op + buf->num_ops, other_buf->op,
		    other_buf->num_ops * sizeof (buf->op[0]));
	    buf->num_ops += other_buf->num_ops;

	    memcpy (buf->points + buf->num_points, other_buf->points,
		    other_buf->num_points * sizeof (buf->points[0]));
	    buf->num_points += other_buf->num_points;
	}

	_cairo_path_fixed_add_buf (path, buf);
    }

    return CAIRO_STATUS_SUCCESS;
}

unsigned long
_cairo_path_fixed_hash (const cairo_path_fixed_t *path)
{
    unsigned long hash = _CAIRO_HASH_INIT_VALUE;
    const cairo_path_buf_t *buf;
    int num_points, num_ops;

    hash = _cairo_hash_bytes (hash, &path->extents, sizeof (path->extents));

    num_ops = num_points = 0;
    cairo_path_foreach_buf_start (buf, path) {
	hash = _cairo_hash_bytes (hash, buf->op,
			          buf->num_ops * sizeof (buf->op[0]));
	hash = _cairo_hash_bytes (hash, buf->points,
			          buf->num_points * sizeof (buf->points[0]));

	num_ops    += buf->num_ops;
	num_points += buf->num_points;
    } cairo_path_foreach_buf_end (buf, path);

    hash = _cairo_hash_bytes (hash, &num_ops, sizeof (num_ops));
    hash = _cairo_hash_bytes (hash, &num_points, sizeof (num_points));

    return hash;
}

unsigned long
_cairo_path_fixed_size (const cairo_path_fixed_t *path)
{
    const cairo_path_buf_t *buf;
    int num_points, num_ops;

    num_ops = num_points = 0;
    cairo_path_foreach_buf_start (buf, path) {
	num_ops    += buf->num_ops;
	num_points += buf->num_points;
    } cairo_path_foreach_buf_end (buf, path);

    return num_ops * sizeof (buf->op[0]) +
	   num_points * sizeof (buf->points[0]);
}

cairo_bool_t
_cairo_path_fixed_equal (const cairo_path_fixed_t *a,
			 const cairo_path_fixed_t *b)
{
    const cairo_path_buf_t *buf_a, *buf_b;
    const cairo_path_op_t *ops_a, *ops_b;
    const cairo_point_t *points_a, *points_b;
    int num_points_a, num_ops_a;
    int num_points_b, num_ops_b;

    if (a == b)
	return TRUE;

    /* use the flags to quickly differentiate based on contents */
    if (a->is_empty_fill != b->is_empty_fill ||
	a->has_curve_to != b->has_curve_to ||
	a->maybe_fill_region != b->maybe_fill_region ||
	a->is_rectilinear != b->is_rectilinear)
    {
	return FALSE;
    }

    if (a->extents.p1.x != b->extents.p1.x ||
	a->extents.p1.y != b->extents.p1.y ||
	a->extents.p2.x != b->extents.p2.x ||
	a->extents.p2.y != b->extents.p2.y)
    {
	return FALSE;
    }

    num_ops_a = num_points_a = 0;
    cairo_path_foreach_buf_start (buf_a, a) {
	num_ops_a    += buf_a->num_ops;
	num_points_a += buf_a->num_points;
    } cairo_path_foreach_buf_end (buf_a, a);

    num_ops_b = num_points_b = 0;
    cairo_path_foreach_buf_start (buf_b, b) {
	num_ops_b    += buf_b->num_ops;
	num_points_b += buf_b->num_points;
    } cairo_path_foreach_buf_end (buf_b, b);

    if (num_ops_a == 0 && num_ops_b == 0)
	return TRUE;

    if (num_ops_a != num_ops_b || num_points_a != num_points_b)
	return FALSE;

    buf_a = cairo_path_head (a);
    num_points_a = buf_a->num_points;
    num_ops_a = buf_a->num_ops;
    ops_a = buf_a->op;
    points_a = buf_a->points;

    buf_b = cairo_path_head (b);
    num_points_b = buf_b->num_points;
    num_ops_b = buf_b->num_ops;
    ops_b = buf_b->op;
    points_b = buf_b->points;

    while (TRUE) {
	int num_ops = MIN (num_ops_a, num_ops_b);
	int num_points = MIN (num_points_a, num_points_b);

	if (memcmp (ops_a, ops_b, num_ops * sizeof (cairo_path_op_t)))
	    return FALSE;
	if (memcmp (points_a, points_b, num_points * sizeof (cairo_point_t)))
	    return FALSE;

	num_ops_a -= num_ops;
	ops_a += num_ops;
	num_points_a -= num_points;
	points_a += num_points;
	if (num_ops_a == 0 || num_points_a == 0) {
	    if (num_ops_a || num_points_a)
		return FALSE;

	    buf_a = cairo_path_buf_next (buf_a);
	    if (buf_a == cairo_path_head (a))
		break;

	    num_points_a = buf_a->num_points;
	    num_ops_a = buf_a->num_ops;
	    ops_a = buf_a->op;
	    points_a = buf_a->points;
	}

	num_ops_b -= num_ops;
	ops_b += num_ops;
	num_points_b -= num_points;
	points_b += num_points;
	if (num_ops_b == 0 || num_points_b == 0) {
	    if (num_ops_b || num_points_b)
		return FALSE;

	    buf_b = cairo_path_buf_next (buf_b);
	    if (buf_b == cairo_path_head (b))
		break;

	    num_points_b = buf_b->num_points;
	    num_ops_b = buf_b->num_ops;
	    ops_b = buf_b->op;
	    points_b = buf_b->points;
	}
    }

    return TRUE;
}

cairo_path_fixed_t *
_cairo_path_fixed_create (void)
{
    cairo_path_fixed_t	*path;

    path = malloc (sizeof (cairo_path_fixed_t));
    if (!path) {
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	return NULL;
    }

    _cairo_path_fixed_init (path);
    return path;
}

void
_cairo_path_fixed_fini (cairo_path_fixed_t *path)
{
    cairo_path_buf_t *buf;

    buf = cairo_path_buf_next (cairo_path_head (path));
    while (buf != cairo_path_head (path)) {
	cairo_path_buf_t *this = buf;
	buf = cairo_path_buf_next (buf);
	_cairo_path_buf_destroy (this);
    }

    VG (VALGRIND_MAKE_MEM_NOACCESS (path, sizeof (cairo_path_fixed_t)));
}

void
_cairo_path_fixed_destroy (cairo_path_fixed_t *path)
{
    _cairo_path_fixed_fini (path);
    free (path);
}

static cairo_path_op_t
_cairo_path_last_op (cairo_path_fixed_t *path)
{
    cairo_path_buf_t *buf;

    buf = cairo_path_tail (path);
    if (buf->num_ops == 0)
	return -1;

    return buf->op[buf->num_ops - 1];
}

static inline void
_cairo_path_fixed_extents_add (cairo_path_fixed_t *path,
			       const cairo_point_t *point)
{
    if (point->x < path->extents.p1.x)
	path->extents.p1.x = point->x;
    if (point->y < path->extents.p1.y)
	path->extents.p1.y = point->y;

    if (point->x > path->extents.p2.x)
	path->extents.p2.x = point->x;
    if (point->y > path->extents.p2.y)
	path->extents.p2.y = point->y;
}

cairo_status_t
_cairo_path_fixed_move_to (cairo_path_fixed_t  *path,
			   cairo_fixed_t	x,
			   cairo_fixed_t	y)
{
    cairo_status_t status;
    cairo_point_t point;

    point.x = x;
    point.y = y;

    /* If the previous op was also a MOVE_TO, then just change its
     * point rather than adding a new op. */
    if (_cairo_path_last_op (path) == CAIRO_PATH_OP_MOVE_TO) {
	cairo_path_buf_t *buf;

	buf = cairo_path_tail (path);
	buf->points[buf->num_points - 1] = point;
    } else {
	status = _cairo_path_fixed_add (path, CAIRO_PATH_OP_MOVE_TO, &point, 1);
	if (unlikely (status))
	    return status;

	if (path->has_current_point && path->is_rectilinear) {
	    /* a move-to is first an implicit close */
	    path->is_rectilinear = path->current_point.x == path->last_move_point.x ||
				   path->current_point.y == path->last_move_point.y;
	    path->maybe_fill_region &= path->is_rectilinear;
	}
	if (path->maybe_fill_region) {
	    path->maybe_fill_region =
		_cairo_fixed_is_integer (path->last_move_point.x) &&
		_cairo_fixed_is_integer (path->last_move_point.y);
	}
    }

    path->current_point = point;
    path->last_move_point = point;
    path->has_last_move_point = TRUE;
    path->has_current_point = TRUE;

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_path_fixed_new_sub_path (cairo_path_fixed_t *path)
{
    path->has_current_point = FALSE;
}

cairo_status_t
_cairo_path_fixed_rel_move_to (cairo_path_fixed_t *path,
			       cairo_fixed_t	   dx,
			       cairo_fixed_t	   dy)
{
    if (unlikely (! path->has_current_point))
	return _cairo_error (CAIRO_STATUS_NO_CURRENT_POINT);

    return _cairo_path_fixed_move_to (path,
				      path->current_point.x + dx,
				      path->current_point.y + dy);

}

cairo_status_t
_cairo_path_fixed_line_to (cairo_path_fixed_t *path,
			   cairo_fixed_t	x,
			   cairo_fixed_t	y)
{
    cairo_status_t status;
    cairo_point_t point;

    point.x = x;
    point.y = y;

    /* When there is not yet a current point, the line_to operation
     * becomes a move_to instead. Note: We have to do this by
     * explicitly calling into _cairo_path_fixed_move_to to ensure
     * that the last_move_point state is updated properly.
     */
    if (! path->has_current_point)
	return _cairo_path_fixed_move_to (path, point.x, point.y);

    /* If the previous op was but the initial MOVE_TO and this segment
     * is degenerate, then we can simply skip this point. Note that
     * a move-to followed by a degenerate line-to is a valid path for
     * stroking, but at all other times is simply a degenerate segment.
     */
    if (_cairo_path_last_op (path) != CAIRO_PATH_OP_MOVE_TO) {
	if (x == path->current_point.x && y == path->current_point.y)
	    return CAIRO_STATUS_SUCCESS;
    }

    /* If the previous op was also a LINE_TO with the same gradient,
     * then just change its end-point rather than adding a new op.
     */
    if (_cairo_path_last_op (path) == CAIRO_PATH_OP_LINE_TO) {
	cairo_path_buf_t *buf;
	const cairo_point_t *p;

	buf = cairo_path_tail (path);
	if (likely (buf->num_points >= 2)) {
	    p = &buf->points[buf->num_points-2];
	} else {
	    cairo_path_buf_t *prev_buf = cairo_path_buf_prev (buf);
	    p = &prev_buf->points[prev_buf->num_points - (2 - buf->num_points)];
	}

	if (p->x == path->current_point.x && p->y == path->current_point.y) {
	    /* previous line element was degenerate, replace */
	    buf->points[buf->num_points - 1] = point;
	    goto FLAGS;
	} else {
	    cairo_slope_t prev, self;

	    _cairo_slope_init (&prev, p, &path->current_point);
	    _cairo_slope_init (&self, &path->current_point, &point);
	    if (_cairo_slope_equal (&prev, &self) &&
		/* cannot trim anti-parallel segments whilst stroking */
		! _cairo_slope_backwards (&prev, &self))
	    {
		buf->points[buf->num_points - 1] = point;
		goto FLAGS;
	    }
	}
    }

    status = _cairo_path_fixed_add (path, CAIRO_PATH_OP_LINE_TO, &point, 1);
    if (unlikely (status))
	return status;

  FLAGS:
    if (path->is_rectilinear) {
	path->is_rectilinear = path->current_point.x == x ||
			       path->current_point.y == y;
	path->maybe_fill_region &= path->is_rectilinear;
    }
    if (path->maybe_fill_region) {
	path->maybe_fill_region = _cairo_fixed_is_integer (x) &&
				  _cairo_fixed_is_integer (y);
    }
    if (path->is_empty_fill) {
	path->is_empty_fill = path->current_point.x == x &&
			      path->current_point.y == y;
    }

    path->current_point = point;
    if (path->has_last_move_point) {
	_cairo_path_fixed_extents_add (path, &path->last_move_point);
	path->has_last_move_point = FALSE;
    }
    _cairo_path_fixed_extents_add (path, &point);
    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_path_fixed_rel_line_to (cairo_path_fixed_t *path,
			       cairo_fixed_t	   dx,
			       cairo_fixed_t	   dy)
{
    if (unlikely (! path->has_current_point))
	return _cairo_error (CAIRO_STATUS_NO_CURRENT_POINT);

    return _cairo_path_fixed_line_to (path,
				      path->current_point.x + dx,
				      path->current_point.y + dy);
}

cairo_status_t
_cairo_path_fixed_curve_to (cairo_path_fixed_t	*path,
			    cairo_fixed_t x0, cairo_fixed_t y0,
			    cairo_fixed_t x1, cairo_fixed_t y1,
			    cairo_fixed_t x2, cairo_fixed_t y2)
{
    cairo_status_t status;
    cairo_point_t point[3];

    /* make sure subpaths are started properly */
    if (! path->has_current_point) {
	status = _cairo_path_fixed_move_to (path, x0, y0);
	if (unlikely (status))
	    return status;
    }

    point[0].x = x0; point[0].y = y0;
    point[1].x = x1; point[1].y = y1;
    point[2].x = x2; point[2].y = y2;
    status = _cairo_path_fixed_add (path, CAIRO_PATH_OP_CURVE_TO, point, 3);
    if (unlikely (status))
	return status;

    path->current_point = point[2];
    path->has_current_point = TRUE;
    path->is_empty_fill = FALSE;
    path->has_curve_to = TRUE;
    path->is_rectilinear = FALSE;
    path->maybe_fill_region = FALSE;

    /* coarse bounds */
    if (path->has_last_move_point) {
	_cairo_path_fixed_extents_add (path, &path->last_move_point);
	path->has_last_move_point = FALSE;
    }
    _cairo_path_fixed_extents_add (path, &point[0]);
    _cairo_path_fixed_extents_add (path, &point[1]);
    _cairo_path_fixed_extents_add (path, &point[2]);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_path_fixed_rel_curve_to (cairo_path_fixed_t *path,
				cairo_fixed_t dx0, cairo_fixed_t dy0,
				cairo_fixed_t dx1, cairo_fixed_t dy1,
				cairo_fixed_t dx2, cairo_fixed_t dy2)
{
    if (unlikely (! path->has_current_point))
	return _cairo_error (CAIRO_STATUS_NO_CURRENT_POINT);

    return _cairo_path_fixed_curve_to (path,
				       path->current_point.x + dx0,
				       path->current_point.y + dy0,

				       path->current_point.x + dx1,
				       path->current_point.y + dy1,

				       path->current_point.x + dx2,
				       path->current_point.y + dy2);
}

cairo_status_t
_cairo_path_fixed_close_path (cairo_path_fixed_t *path)
{
    cairo_status_t status;

    if (! path->has_current_point)
	return CAIRO_STATUS_SUCCESS;

    /* If the previous op was also a LINE_TO back to the start, discard it */
    if (_cairo_path_last_op (path) == CAIRO_PATH_OP_LINE_TO) {
	if (path->current_point.x == path->last_move_point.x &&
	    path->current_point.y == path->last_move_point.y)
	{
	    cairo_path_buf_t *buf;
	    cairo_point_t *p;

	    buf = cairo_path_tail (path);
	    if (likely (buf->num_points >= 2)) {
		p = &buf->points[buf->num_points-2];
	    } else {
		cairo_path_buf_t *prev_buf = cairo_path_buf_prev (buf);
		p = &prev_buf->points[prev_buf->num_points - (2 - buf->num_points)];
	    }

	    path->current_point = *p;
	    buf->num_ops--;
	    buf->num_points--;
	}
    }

    status = _cairo_path_fixed_add (path, CAIRO_PATH_OP_CLOSE_PATH, NULL, 0);
    if (unlikely (status))
	return status;

    return _cairo_path_fixed_move_to (path,
				      path->last_move_point.x,
				      path->last_move_point.y);
}

cairo_bool_t
_cairo_path_fixed_get_current_point (cairo_path_fixed_t *path,
				     cairo_fixed_t	*x,
				     cairo_fixed_t	*y)
{
    if (! path->has_current_point)
	return FALSE;

    *x = path->current_point.x;
    *y = path->current_point.y;

    return TRUE;
}

static cairo_status_t
_cairo_path_fixed_add (cairo_path_fixed_t   *path,
		       cairo_path_op_t	     op,
		       const cairo_point_t  *points,
		       int		     num_points)
{
    cairo_path_buf_t *buf = cairo_path_tail (path);

    if (buf->num_ops + 1 > buf->size_ops ||
	buf->num_points + num_points > buf->size_points)
    {
	buf = _cairo_path_buf_create (buf->num_ops * 2, buf->num_points * 2);
	if (unlikely (buf == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	_cairo_path_fixed_add_buf (path, buf);
    }

    if (WATCH_PATH) {
	const char *op_str[] = {
	    "move-to",
	    "line-to",
	    "curve-to",
	    "close-path",
	};
	char buf[1024];
	int len = 0;
	int i;

	len += snprintf (buf + len, sizeof (buf), "[");
	for (i = 0; i < num_points; i++) {
	    if (i != 0)
		len += snprintf (buf + len, sizeof (buf), " ");
	    len += snprintf (buf + len, sizeof (buf), "(%f, %f)",
			     _cairo_fixed_to_double (points[i].x),
			     _cairo_fixed_to_double (points[i].y));
	}
	len += snprintf (buf + len, sizeof (buf), "]");

	fprintf (stderr,
		 "_cairo_path_fixed_add (%s, %s)\n",
		 op_str[(int) op], buf);
    }

    _cairo_path_buf_add_op (buf, op);
    _cairo_path_buf_add_points (buf, points, num_points);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_path_fixed_add_buf (cairo_path_fixed_t *path,
			   cairo_path_buf_t   *buf)
{
    cairo_list_add_tail (&buf->link, &cairo_path_head (path)->link);
}

COMPILE_TIME_ASSERT (sizeof (cairo_path_op_t) == 1);
static cairo_path_buf_t *
_cairo_path_buf_create (int size_ops, int size_points)
{
    cairo_path_buf_t *buf;

    /* adjust size_ops to ensure that buf->points is naturally aligned */
    size_ops += sizeof (double) - ((sizeof (cairo_path_buf_t) + size_ops) % sizeof (double));
    buf = _cairo_malloc_ab_plus_c (size_points, sizeof (cairo_point_t), size_ops + sizeof (cairo_path_buf_t));
    if (buf) {
	buf->num_ops = 0;
	buf->num_points = 0;
	buf->size_ops = size_ops;
	buf->size_points = size_points;

	buf->op = (cairo_path_op_t *) (buf + 1);
	buf->points = (cairo_point_t *) (buf->op + size_ops);
    }

    return buf;
}

static void
_cairo_path_buf_destroy (cairo_path_buf_t *buf)
{
    free (buf);
}

static void
_cairo_path_buf_add_op (cairo_path_buf_t *buf,
			cairo_path_op_t	  op)
{
    buf->op[buf->num_ops++] = op;
}

static void
_cairo_path_buf_add_points (cairo_path_buf_t       *buf,
			    const cairo_point_t    *points,
			    int		            num_points)
{
    memcpy (buf->points + buf->num_points,
	    points,
	    sizeof (points[0]) * num_points);
    buf->num_points += num_points;
}

cairo_status_t
_cairo_path_fixed_interpret (const cairo_path_fixed_t		*path,
			     cairo_direction_t			 dir,
			     cairo_path_fixed_move_to_func_t	*move_to,
			     cairo_path_fixed_line_to_func_t	*line_to,
			     cairo_path_fixed_curve_to_func_t	*curve_to,
			     cairo_path_fixed_close_path_func_t	*close_path,
			     void				*closure)
{
    const uint8_t num_args[] = {
	1, /* cairo_path_move_to */
	1, /* cairo_path_op_line_to */
	3, /* cairo_path_op_curve_to */
	0, /* cairo_path_op_close_path */
    };
    cairo_status_t status;
    const cairo_path_buf_t *buf, *first;
    cairo_bool_t forward = (dir == CAIRO_DIRECTION_FORWARD);
    int step = forward ? 1 : -1;

    buf = first = forward ? cairo_path_head (path) : cairo_path_tail (path);
    do {
	cairo_point_t *points;
	int start, stop, i;

	if (forward) {
	    start = 0;
	    stop = buf->num_ops;
	    points = buf->points;
	} else {
	    start = buf->num_ops - 1;
	    stop = -1;
	    points = buf->points + buf->num_points;
	}

	for (i = start; i != stop; i += step) {
	    cairo_path_op_t op = buf->op[i];

	    if (! forward)
		points -= num_args[(int) op];

	    switch (op) {
	    case CAIRO_PATH_OP_MOVE_TO:
		status = (*move_to) (closure, &points[0]);
		break;
	    case CAIRO_PATH_OP_LINE_TO:
		status = (*line_to) (closure, &points[0]);
		break;
	    case CAIRO_PATH_OP_CURVE_TO:
		status = (*curve_to) (closure, &points[0], &points[1], &points[2]);
		break;
	    default:
		ASSERT_NOT_REACHED;
	    case CAIRO_PATH_OP_CLOSE_PATH:
		status = (*close_path) (closure);
		break;
	    }
	    if (unlikely (status))
		return status;

	    if (forward)
		points += num_args[(int) op];
	}
    } while ((buf = forward ? cairo_path_buf_next (buf) : cairo_path_buf_prev (buf)) != first);

    return CAIRO_STATUS_SUCCESS;
}

typedef struct _cairo_path_fixed_append_closure {
    cairo_point_t	    offset;
    cairo_path_fixed_t	    *path;
} cairo_path_fixed_append_closure_t;

static cairo_status_t
_append_move_to (void		 *abstract_closure,
		 const cairo_point_t  *point)
{
    cairo_path_fixed_append_closure_t	*closure = abstract_closure;

    return _cairo_path_fixed_move_to (closure->path,
				      point->x + closure->offset.x,
				      point->y + closure->offset.y);
}

static cairo_status_t
_append_line_to (void		 *abstract_closure,
		 const cairo_point_t *point)
{
    cairo_path_fixed_append_closure_t	*closure = abstract_closure;

    return _cairo_path_fixed_line_to (closure->path,
				      point->x + closure->offset.x,
				      point->y + closure->offset.y);
}

static cairo_status_t
_append_curve_to (void	  *abstract_closure,
		  const cairo_point_t *p0,
		  const cairo_point_t *p1,
		  const cairo_point_t *p2)
{
    cairo_path_fixed_append_closure_t	*closure = abstract_closure;

    return _cairo_path_fixed_curve_to (closure->path,
				       p0->x + closure->offset.x,
				       p0->y + closure->offset.y,
				       p1->x + closure->offset.x,
				       p1->y + closure->offset.y,
				       p2->x + closure->offset.x,
				       p2->y + closure->offset.y);
}

static cairo_status_t
_append_close_path (void *abstract_closure)
{
    cairo_path_fixed_append_closure_t	*closure = abstract_closure;

    return _cairo_path_fixed_close_path (closure->path);
}

cairo_status_t
_cairo_path_fixed_append (cairo_path_fixed_t		    *path,
			  const cairo_path_fixed_t	    *other,
			  cairo_direction_t		     dir,
			  cairo_fixed_t			     tx,
			  cairo_fixed_t			     ty)
{
    cairo_path_fixed_append_closure_t closure;

    closure.path = path;
    closure.offset.x = tx;
    closure.offset.y = ty;

    return _cairo_path_fixed_interpret (other, dir,
					_append_move_to,
					_append_line_to,
					_append_curve_to,
					_append_close_path,
					&closure);
}

static void
_cairo_path_fixed_offset_and_scale (cairo_path_fixed_t *path,
				    cairo_fixed_t offx,
				    cairo_fixed_t offy,
				    cairo_fixed_t scalex,
				    cairo_fixed_t scaley)
{
    cairo_path_buf_t *buf;
    unsigned int i;

    if (path->maybe_fill_region) {
	path->maybe_fill_region = _cairo_fixed_is_integer (offx) &&
	                          _cairo_fixed_is_integer (offy) &&
			          _cairo_fixed_is_integer (scalex) &&
			          _cairo_fixed_is_integer (scaley);
    }

    cairo_path_foreach_buf_start (buf, path) {
	 for (i = 0; i < buf->num_points; i++) {
	     if (scalex != CAIRO_FIXED_ONE)
		 buf->points[i].x = _cairo_fixed_mul (buf->points[i].x, scalex);
	     buf->points[i].x += offx;

	     if (scaley != CAIRO_FIXED_ONE)
		 buf->points[i].y = _cairo_fixed_mul (buf->points[i].y, scaley);
	     buf->points[i].y += offy;
	 }
    } cairo_path_foreach_buf_end (buf, path);

    path->extents.p1.x = _cairo_fixed_mul (scalex, path->extents.p1.x) + offx;
    path->extents.p2.x = _cairo_fixed_mul (scalex, path->extents.p2.x) + offx;

    path->extents.p1.y = _cairo_fixed_mul (scaley, path->extents.p1.y) + offy;
    path->extents.p2.y = _cairo_fixed_mul (scaley, path->extents.p2.y) + offy;
}

void
_cairo_path_fixed_translate (cairo_path_fixed_t *path,
			     cairo_fixed_t offx,
			     cairo_fixed_t offy)
{
    cairo_path_buf_t *buf;
    unsigned int i;

    if (offx == 0 && offy == 0)
	return;

    if (path->maybe_fill_region &&
	! (_cairo_fixed_is_integer (offx) && _cairo_fixed_is_integer (offy)))
    {
	path->maybe_fill_region = FALSE;
    }

    path->last_move_point.x += offx;
    path->last_move_point.y += offy;
    path->current_point.x += offx;
    path->current_point.y += offy;

    cairo_path_foreach_buf_start (buf, path) {
	 for (i = 0; i < buf->num_points; i++) {
	     buf->points[i].x += offx;
	     buf->points[i].y += offy;
	 }
    } cairo_path_foreach_buf_end (buf, path);

    path->extents.p1.x += offx;
    path->extents.p1.y += offy;
    path->extents.p2.x += offx;
    path->extents.p2.y += offy;
}

/**
 * _cairo_path_fixed_transform:
 * @path: a #cairo_path_fixed_t to be transformed
 * @matrix: a #cairo_matrix_t
 *
 * Transform the fixed-point path according to the given matrix.
 * There is a fast path for the case where @matrix has no rotation
 * or shear.
 **/
void
_cairo_path_fixed_transform (cairo_path_fixed_t	*path,
			     const cairo_matrix_t     *matrix)
{
    cairo_path_buf_t *buf;
    unsigned int i;
    double dx, dy;

    /* XXX current_point, last_move_to */

    if (matrix->yx == 0.0 && matrix->xy == 0.0) {
	/* Fast path for the common case of scale+transform */
	 if (matrix->xx == 1. && matrix->yy == 1.) {
	     _cairo_path_fixed_translate (path,
					  _cairo_fixed_from_double (matrix->x0),
					  _cairo_fixed_from_double (matrix->y0));
	 } else {
	     _cairo_path_fixed_offset_and_scale (path,
						 _cairo_fixed_from_double (matrix->x0),
						 _cairo_fixed_from_double (matrix->y0),
						 _cairo_fixed_from_double (matrix->xx),
						 _cairo_fixed_from_double (matrix->yy));
	 }
	return;
    }

    path->extents.p1.x = path->extents.p1.y = INT_MAX;
    path->extents.p2.x = path->extents.p2.y = INT_MIN;
    path->maybe_fill_region = FALSE;
    cairo_path_foreach_buf_start (buf, path) {
	 for (i = 0; i < buf->num_points; i++) {
	    dx = _cairo_fixed_to_double (buf->points[i].x);
	    dy = _cairo_fixed_to_double (buf->points[i].y);

	    cairo_matrix_transform_point (matrix, &dx, &dy);

	    buf->points[i].x = _cairo_fixed_from_double (dx);
	    buf->points[i].y = _cairo_fixed_from_double (dy);

	    /* XXX need to eliminate surplus move-to's? */
	    _cairo_path_fixed_extents_add (path, &buf->points[i]);
	 }
    } cairo_path_foreach_buf_end (buf, path);
}

cairo_bool_t
_cairo_path_fixed_is_equal (const cairo_path_fixed_t *path,
			    const cairo_path_fixed_t *other)
{
    const cairo_path_buf_t *path_buf, *other_buf;

    if (path->current_point.x != other->current_point.x ||
	path->current_point.y != other->current_point.y ||
	path->has_current_point != other->has_current_point ||
	path->has_curve_to != other->has_curve_to ||
	path->is_rectilinear != other->is_rectilinear ||
	path->maybe_fill_region != other->maybe_fill_region ||
	path->last_move_point.x != other->last_move_point.x ||
	path->last_move_point.y != other->last_move_point.y)
    {
	return FALSE;
    }

    other_buf = cairo_path_head (other);
    cairo_path_foreach_buf_start (path_buf, path) {
	if (path_buf->num_ops != other_buf->num_ops ||
	    path_buf->num_points != other_buf->num_points ||
	    memcmp (path_buf->op, other_buf->op,
		    sizeof (cairo_path_op_t) * path_buf->num_ops) != 0 ||
	    memcmp (path_buf->points, other_buf->points,
		    sizeof (cairo_point_t) * path_buf->num_points) != 0)
	{
	    return FALSE;
	}
	other_buf = cairo_path_buf_next (other_buf);
    } cairo_path_foreach_buf_end (path_buf, path);

    return TRUE;
}

/* Closure for path flattening */
typedef struct cairo_path_flattener {
    double tolerance;
    cairo_point_t current_point;
    cairo_path_fixed_move_to_func_t	*move_to;
    cairo_path_fixed_line_to_func_t	*line_to;
    cairo_path_fixed_close_path_func_t	*close_path;
    void *closure;
} cpf_t;

static cairo_status_t
_cpf_move_to (void *closure,
	      const cairo_point_t *point)
{
    cpf_t *cpf = closure;

    cpf->current_point = *point;

    return cpf->move_to (cpf->closure, point);
}

static cairo_status_t
_cpf_line_to (void *closure,
	      const cairo_point_t *point)
{
    cpf_t *cpf = closure;

    cpf->current_point = *point;

    return cpf->line_to (cpf->closure, point);
}

static cairo_status_t
_cpf_curve_to (void		*closure,
	       const cairo_point_t	*p1,
	       const cairo_point_t	*p2,
	       const cairo_point_t	*p3)
{
    cpf_t *cpf = closure;
    cairo_spline_t spline;

    cairo_point_t *p0 = &cpf->current_point;

    if (! _cairo_spline_init (&spline,
			      cpf->line_to,
			      cpf->closure,
			      p0, p1, p2, p3))
    {
	return _cpf_line_to (closure, p3);
    }

    cpf->current_point = *p3;

    return _cairo_spline_decompose (&spline, cpf->tolerance);
}

static cairo_status_t
_cpf_close_path (void *closure)
{
    cpf_t *cpf = closure;

    return cpf->close_path (cpf->closure);
}

cairo_status_t
_cairo_path_fixed_interpret_flat (const cairo_path_fixed_t		*path,
				  cairo_direction_t			dir,
				  cairo_path_fixed_move_to_func_t	*move_to,
				  cairo_path_fixed_line_to_func_t	*line_to,
				  cairo_path_fixed_close_path_func_t	*close_path,
				  void					*closure,
				  double				tolerance)
{
    cpf_t flattener;

    if (! path->has_curve_to) {
	return _cairo_path_fixed_interpret (path, dir,
					    move_to,
					    line_to,
					    NULL,
					    close_path,
					    closure);
    }

    flattener.tolerance = tolerance;
    flattener.move_to = move_to;
    flattener.line_to = line_to;
    flattener.close_path = close_path;
    flattener.closure = closure;
    return _cairo_path_fixed_interpret (path, dir,
					_cpf_move_to,
					_cpf_line_to,
					_cpf_curve_to,
					_cpf_close_path,
					&flattener);
}

static inline void
_canonical_box (cairo_box_t *box,
		const cairo_point_t *p1,
		const cairo_point_t *p2)
{
    if (p1->x <= p2->x) {
	box->p1.x = p1->x;
	box->p2.x = p2->x;
    } else {
	box->p1.x = p2->x;
	box->p2.x = p1->x;
    }

    if (p1->y <= p2->y) {
	box->p1.y = p1->y;
	box->p2.y = p2->y;
    } else {
	box->p1.y = p2->y;
	box->p2.y = p1->y;
    }
}

/*
 * Check whether the given path contains a single rectangle.
 */
cairo_bool_t
_cairo_path_fixed_is_box (const cairo_path_fixed_t *path,
			  cairo_box_t *box)
{
    const cairo_path_buf_t *buf = cairo_path_head (path);

    if (! path->is_rectilinear)
	return FALSE;

    /* Do we have the right number of ops? */
    if (buf->num_ops < 4 || buf->num_ops > 6)
	return FALSE;

    /* Check whether the ops are those that would be used for a rectangle */
    if (buf->op[0] != CAIRO_PATH_OP_MOVE_TO ||
	buf->op[1] != CAIRO_PATH_OP_LINE_TO ||
	buf->op[2] != CAIRO_PATH_OP_LINE_TO ||
	buf->op[3] != CAIRO_PATH_OP_LINE_TO)
    {
	return FALSE;
    }

    /* we accept an implicit close for filled paths */
    if (buf->num_ops > 4) {
	/* Now, there are choices. The rectangle might end with a LINE_TO
	 * (to the original point), but this isn't required. If it
	 * doesn't, then it must end with a CLOSE_PATH. */
	if (buf->op[4] == CAIRO_PATH_OP_LINE_TO) {
	    if (buf->points[4].x != buf->points[0].x ||
		buf->points[4].y != buf->points[0].y)
		return FALSE;
	} else if (buf->op[4] != CAIRO_PATH_OP_CLOSE_PATH) {
	    return FALSE;
	}

	if (buf->num_ops == 6) {
	    /* A trailing CLOSE_PATH or MOVE_TO is ok */
	    if (buf->op[5] != CAIRO_PATH_OP_MOVE_TO &&
		buf->op[5] != CAIRO_PATH_OP_CLOSE_PATH)
		return FALSE;
	}
    }

    /* Ok, we may have a box, if the points line up */
    if (buf->points[0].y == buf->points[1].y &&
	buf->points[1].x == buf->points[2].x &&
	buf->points[2].y == buf->points[3].y &&
	buf->points[3].x == buf->points[0].x)
    {
	_canonical_box (box, &buf->points[0], &buf->points[2]);
	return TRUE;
    }

    if (buf->points[0].x == buf->points[1].x &&
	buf->points[1].y == buf->points[2].y &&
	buf->points[2].x == buf->points[3].x &&
	buf->points[3].y == buf->points[0].y)
    {
	_canonical_box (box, &buf->points[0], &buf->points[2]);
	return TRUE;
    }

    return FALSE;
}

/*
 * Check whether the given path contains a single rectangle
 * that is logically equivalent to:
 * <informalexample><programlisting>
 *   cairo_move_to (cr, x, y);
 *   cairo_rel_line_to (cr, width, 0);
 *   cairo_rel_line_to (cr, 0, height);
 *   cairo_rel_line_to (cr, -width, 0);
 *   cairo_close_path (cr);
 * </programlisting></informalexample>
 */
cairo_bool_t
_cairo_path_fixed_is_rectangle (const cairo_path_fixed_t *path,
				cairo_box_t        *box)
{
    const cairo_path_buf_t *buf;

    if (! _cairo_path_fixed_is_box (path, box))
	return FALSE;

    buf = cairo_path_head (path);
    if (buf->points[0].y == buf->points[1].y)
	return TRUE;

    return FALSE;
}

void
_cairo_path_fixed_iter_init (cairo_path_fixed_iter_t *iter,
			     const cairo_path_fixed_t *path)
{
    iter->first = iter->buf = cairo_path_head (path);
    iter->n_op = 0;
    iter->n_point = 0;
}

static cairo_bool_t
_cairo_path_fixed_iter_next_op (cairo_path_fixed_iter_t *iter)
{
    if (++iter->n_op >= iter->buf->num_ops) {
	iter->buf = cairo_path_buf_next (iter->buf);
	if (iter->buf == iter->first) {
	    iter->buf = NULL;
	    return FALSE;
	}

	iter->n_op = 0;
	iter->n_point = 0;
    }

    return TRUE;
}

cairo_bool_t
_cairo_path_fixed_iter_is_fill_box (cairo_path_fixed_iter_t *_iter,
				    cairo_box_t *box)
{
    cairo_point_t points[5];
    cairo_path_fixed_iter_t iter;

    if (_iter->buf == NULL)
	return FALSE;

    iter = *_iter;

    if (iter.n_op == iter.buf->num_ops &&
	! _cairo_path_fixed_iter_next_op (&iter))
    {
	return FALSE;
    }

    /* Check whether the ops are those that would be used for a rectangle */
    if (iter.buf->op[iter.n_op] != CAIRO_PATH_OP_MOVE_TO)
	return FALSE;
    points[0] = iter.buf->points[iter.n_point++];
    if (! _cairo_path_fixed_iter_next_op (&iter))
	return FALSE;

    if (iter.buf->op[iter.n_op] != CAIRO_PATH_OP_LINE_TO)
	return FALSE;
    points[1] = iter.buf->points[iter.n_point++];
    if (! _cairo_path_fixed_iter_next_op (&iter))
	return FALSE;

    if (iter.buf->op[iter.n_op] != CAIRO_PATH_OP_LINE_TO)
	return FALSE;
    points[2] = iter.buf->points[iter.n_point++];
    if (! _cairo_path_fixed_iter_next_op (&iter))
	return FALSE;

    if (iter.buf->op[iter.n_op] != CAIRO_PATH_OP_LINE_TO)
	return FALSE;
    points[3] = iter.buf->points[iter.n_point++];
    if (! _cairo_path_fixed_iter_next_op (&iter))
	return FALSE;

    /* Now, there are choices. The rectangle might end with a LINE_TO
     * (to the original point), but this isn't required. If it
     * doesn't, then it must end with a CLOSE_PATH (which may be implicit). */
    if (iter.buf->op[iter.n_op] == CAIRO_PATH_OP_LINE_TO)
    {
	points[4] = iter.buf->points[iter.n_point++];
	if (points[4].x != points[0].x || points[4].y != points[0].y)
	    return FALSE;
    }
    else if (! (iter.buf->op[iter.n_op] == CAIRO_PATH_OP_CLOSE_PATH ||
		iter.buf->op[iter.n_op] == CAIRO_PATH_OP_MOVE_TO))
    {
	return FALSE;
    }
    if (! _cairo_path_fixed_iter_next_op (&iter))
	return FALSE;

    /* Ok, we may have a box, if the points line up */
    if (points[0].y == points[1].y &&
	points[1].x == points[2].x &&
	points[2].y == points[3].y &&
	points[3].x == points[0].x)
    {
	box->p1 = points[0];
	box->p2 = points[2];
	*_iter = iter;
	return TRUE;
    }

    if (points[0].x == points[1].x &&
	points[1].y == points[2].y &&
	points[2].x == points[3].x &&
	points[3].y == points[0].y)
    {
	box->p1 = points[1];
	box->p2 = points[3];
	*_iter = iter;
	return TRUE;
    }

    return FALSE;
}

cairo_bool_t
_cairo_path_fixed_iter_at_end (const cairo_path_fixed_iter_t *iter)
{
    if (iter->buf == NULL)
	return TRUE;

    if (iter->n_op == iter->buf->num_ops)
	return TRUE;

    if (iter->buf->op[iter->n_op] == CAIRO_PATH_OP_MOVE_TO &&
	iter->buf->num_ops == iter->n_op + 1)
    {
	return TRUE;
    }

    return FALSE;
}
