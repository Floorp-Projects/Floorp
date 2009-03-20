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

#include "cairo-path-fixed-private.h"

/* private functions */
static cairo_status_t
_cairo_path_fixed_add (cairo_path_fixed_t *path,
		       cairo_path_op_t 	   op,
		       cairo_point_t	  *points,
		       int		   num_points);

static void
_cairo_path_fixed_add_buf (cairo_path_fixed_t *path,
			   cairo_path_buf_t   *buf);

static cairo_path_buf_t *
_cairo_path_buf_create (int buf_size);

static void
_cairo_path_buf_destroy (cairo_path_buf_t *buf);

static void
_cairo_path_buf_add_op (cairo_path_buf_t *buf,
			cairo_path_op_t   op);

static void
_cairo_path_buf_add_points (cairo_path_buf_t *buf,
			    cairo_point_t    *points,
			    int		      num_points);

void
_cairo_path_fixed_init (cairo_path_fixed_t *path)
{
    path->buf_head.base.next = NULL;
    path->buf_head.base.prev = NULL;
    path->buf_tail = &path->buf_head.base;

    path->buf_head.base.num_ops = 0;
    path->buf_head.base.num_points = 0;
    path->buf_head.base.buf_size = CAIRO_PATH_BUF_SIZE;
    path->buf_head.base.op = path->buf_head.op;
    path->buf_head.base.points = path->buf_head.points;

    path->current_point.x = 0;
    path->current_point.y = 0;
    path->has_current_point = FALSE;
    path->has_curve_to = FALSE;
    path->last_move_point = path->current_point;
}

cairo_status_t
_cairo_path_fixed_init_copy (cairo_path_fixed_t *path,
			     cairo_path_fixed_t *other)
{
    cairo_path_buf_t *buf, *other_buf;
    unsigned int num_points, num_ops, buf_size;

    _cairo_path_fixed_init (path);

    path->current_point = other->current_point;
    path->has_current_point = other->has_current_point;
    path->has_curve_to = other->has_curve_to;
    path->last_move_point = other->last_move_point;

    path->buf_head.base.num_ops = other->buf_head.base.num_ops;
    path->buf_head.base.num_points = other->buf_head.base.num_points;
    path->buf_head.base.buf_size = other->buf_head.base.buf_size;
    memcpy (path->buf_head.op, other->buf_head.base.op,
	    other->buf_head.base.num_ops * sizeof (other->buf_head.op[0]));
    memcpy (path->buf_head.points, other->buf_head.points,
	    other->buf_head.base.num_points * sizeof (other->buf_head.points[0]));

    num_points = num_ops = 0;
    for (other_buf = other->buf_head.base.next;
	 other_buf != NULL;
	 other_buf = other_buf->next)
    {
	num_ops    += other_buf->num_ops;
	num_points += other_buf->num_points;
    }

    buf_size = MAX (num_ops, (num_points + 1) / 2);
    if (buf_size) {
	buf = _cairo_path_buf_create (buf_size);
	if (unlikely (buf == NULL)) {
	    _cairo_path_fixed_fini (path);
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
	}

	for (other_buf = other->buf_head.base.next;
	     other_buf != NULL;
	     other_buf = other_buf->next)
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
    unsigned long hash = 0;
    const cairo_path_buf_t *buf;
    int num_points, num_ops;

    hash = _cairo_hash_bytes (hash,
			 &path->current_point,
			 sizeof (path->current_point));
    hash = _cairo_hash_bytes (hash,
			 &path->last_move_point,
			 sizeof (path->last_move_point));

    num_ops = path->buf_head.base.num_ops;
    num_points = path->buf_head.base.num_points;
    for (buf = path->buf_head.base.next;
	 buf != NULL;
	 buf = buf->next)
    {
	hash = _cairo_hash_bytes (hash, buf->op,
			     buf->num_ops * sizeof (buf->op[0]));
	hash = _cairo_hash_bytes (hash, buf->points,
			     buf->num_points * sizeof (buf->points[0]));

	num_ops    += buf->num_ops;
	num_points += buf->num_points;
    }

    hash = _cairo_hash_bytes (hash, &num_ops, sizeof (num_ops));
    hash = _cairo_hash_bytes (hash, &num_points, sizeof (num_points));

    return hash;
}

unsigned long
_cairo_path_fixed_size (const cairo_path_fixed_t *path)
{
    const cairo_path_buf_t *buf;
    int num_points, num_ops;

    num_ops = path->buf_head.base.num_ops;
    num_points = path->buf_head.base.num_points;
    for (buf = path->buf_head.base.next;
	 buf != NULL;
	 buf = buf->next)
    {
	num_ops    += buf->num_ops;
	num_points += buf->num_points;
    }

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

    if (a != NULL) {
	num_ops_a = a->buf_head.base.num_ops;
	num_points_a = a->buf_head.base.num_points;
	for (buf_a = a->buf_head.base.next;
	     buf_a != NULL;
	     buf_a = buf_a->next)
	{
	    num_ops_a    += buf_a->num_ops;
	    num_points_a += buf_a->num_points;
	}
    } else
	num_ops_a = num_points_a = 0;

    if (b != NULL) {
	num_ops_b = b->buf_head.base.num_ops;
	num_points_b = b->buf_head.base.num_points;
	for (buf_b = b->buf_head.base.next;
	     buf_b != NULL;
	     buf_b = buf_b->next)
	{
	    num_ops_b    += buf_b->num_ops;
	    num_points_b += buf_b->num_points;
	}
    } else
	num_ops_b = num_points_b = 0;

    if (num_ops_a == 0 && num_ops_b == 0)
	return TRUE;

    if (num_ops_a != num_ops_b || num_points_a != num_points_b)
	return FALSE;

    assert (a != NULL && b != NULL);

    buf_a = &a->buf_head.base;
    num_points_a = buf_a->num_points;
    num_ops_a = buf_a->num_ops;
    ops_a = buf_a->op;
    points_a = buf_a->points;

    buf_b = &b->buf_head.base;
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

	    buf_a = buf_a->next;
	    if (buf_a == NULL)
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

	    buf_b = buf_b->next;
	    if (buf_b == NULL)
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

    buf = path->buf_head.base.next;
    while (buf) {
	cairo_path_buf_t *this = buf;
	buf = buf->next;
	_cairo_path_buf_destroy (this);
    }
    path->buf_head.base.next = NULL;
    path->buf_head.base.prev = NULL;
    path->buf_tail = &path->buf_head.base;
    path->buf_head.base.num_ops = 0;
    path->buf_head.base.num_points = 0;

    path->has_current_point = FALSE;
    path->has_curve_to = FALSE;
}

void
_cairo_path_fixed_destroy (cairo_path_fixed_t *path)
{
    _cairo_path_fixed_fini (path);
    free (path);
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
    if (path->buf_tail && path->buf_tail->num_ops &&
	path->buf_tail->op[path->buf_tail->num_ops - 1] == CAIRO_PATH_OP_MOVE_TO)
    {
	cairo_point_t *last_move_to_point;
	last_move_to_point = &path->buf_tail->points[path->buf_tail->num_points - 1];
	*last_move_to_point = point;
    } else {
	status = _cairo_path_fixed_add (path, CAIRO_PATH_OP_MOVE_TO, &point, 1);
	if (unlikely (status))
	    return status;
    }

    path->current_point = point;
    path->has_current_point = TRUE;
    path->last_move_point = path->current_point;

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
    cairo_fixed_t x, y;

    if (! path->has_current_point)
	return _cairo_error (CAIRO_STATUS_NO_CURRENT_POINT);

    x = path->current_point.x + dx;
    y = path->current_point.y + dy;

    return _cairo_path_fixed_move_to (path, x, y);
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
     * explicitly calling into _cairo_path_fixed_line_to to ensure
     * that the last_move_point state is updated properly.
     */
    if (! path->has_current_point)
	status = _cairo_path_fixed_move_to (path, point.x, point.y);
    else
	status = _cairo_path_fixed_add (path, CAIRO_PATH_OP_LINE_TO, &point, 1);

    if (unlikely (status))
	return status;

    path->current_point = point;
    path->has_current_point = TRUE;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_path_fixed_rel_line_to (cairo_path_fixed_t *path,
			       cairo_fixed_t	   dx,
			       cairo_fixed_t	   dy)
{
    cairo_fixed_t x, y;

    if (! path->has_current_point)
	return _cairo_error (CAIRO_STATUS_NO_CURRENT_POINT);

    x = path->current_point.x + dx;
    y = path->current_point.y + dy;

    return _cairo_path_fixed_line_to (path, x, y);
}

cairo_status_t
_cairo_path_fixed_curve_to (cairo_path_fixed_t	*path,
			    cairo_fixed_t x0, cairo_fixed_t y0,
			    cairo_fixed_t x1, cairo_fixed_t y1,
			    cairo_fixed_t x2, cairo_fixed_t y2)
{
    cairo_status_t status;
    cairo_point_t point[3];

    point[0].x = x0; point[0].y = y0;
    point[1].x = x1; point[1].y = y1;
    point[2].x = x2; point[2].y = y2;

    if (! path->has_current_point) {
	status = _cairo_path_fixed_add (path, CAIRO_PATH_OP_MOVE_TO,
					&point[0], 1);
	if (unlikely (status))
	    return status;
    }

    status = _cairo_path_fixed_add (path, CAIRO_PATH_OP_CURVE_TO, point, 3);
    if (unlikely (status))
	return status;

    path->current_point = point[2];
    path->has_current_point = TRUE;
    path->has_curve_to = TRUE;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_path_fixed_rel_curve_to (cairo_path_fixed_t *path,
				cairo_fixed_t dx0, cairo_fixed_t dy0,
				cairo_fixed_t dx1, cairo_fixed_t dy1,
				cairo_fixed_t dx2, cairo_fixed_t dy2)
{
    cairo_fixed_t x0, y0;
    cairo_fixed_t x1, y1;
    cairo_fixed_t x2, y2;

    if (! path->has_current_point)
	return _cairo_error (CAIRO_STATUS_NO_CURRENT_POINT);

    x0 = path->current_point.x + dx0;
    y0 = path->current_point.y + dy0;

    x1 = path->current_point.x + dx1;
    y1 = path->current_point.y + dy1;

    x2 = path->current_point.x + dx2;
    y2 = path->current_point.y + dy2;

    return _cairo_path_fixed_curve_to (path,
				       x0, y0,
				       x1, y1,
				       x2, y2);
}

cairo_status_t
_cairo_path_fixed_close_path (cairo_path_fixed_t *path)
{
    cairo_status_t status;

    if (! path->has_current_point)
	return CAIRO_STATUS_SUCCESS;

    status = _cairo_path_fixed_add (path, CAIRO_PATH_OP_CLOSE_PATH, NULL, 0);
    if (unlikely (status))
	return status;

    status = _cairo_path_fixed_move_to (path,
					path->last_move_point.x,
					path->last_move_point.y);
    if (unlikely (status))
	return status;

    return CAIRO_STATUS_SUCCESS;
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
_cairo_path_fixed_add (cairo_path_fixed_t *path,
		       cairo_path_op_t	   op,
		       cairo_point_t	  *points,
		       int		   num_points)
{
    cairo_path_buf_t *buf = path->buf_tail;

    if (buf->num_ops + 1 > buf->buf_size ||
	buf->num_points + num_points > 2 * buf->buf_size)
    {
	buf = _cairo_path_buf_create (buf->buf_size * 2);
	if (unlikely (buf == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	_cairo_path_fixed_add_buf (path, buf);
    }

    _cairo_path_buf_add_op (buf, op);
    _cairo_path_buf_add_points (buf, points, num_points);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_path_fixed_add_buf (cairo_path_fixed_t *path,
			   cairo_path_buf_t   *buf)
{
    buf->next = NULL;
    buf->prev = path->buf_tail;

    path->buf_tail->next = buf;
    path->buf_tail = buf;
}

static cairo_path_buf_t *
_cairo_path_buf_create (int buf_size)
{
    cairo_path_buf_t *buf;

    /* adjust buf_size to ensure that buf->points is naturally aligned */
    buf_size += sizeof (double)
	       - ((buf_size + sizeof (cairo_path_buf_t)) & (sizeof (double)-1));
    buf = _cairo_malloc_ab_plus_c (buf_size,
	                           sizeof (cairo_path_op_t) +
				   2 * sizeof (cairo_point_t),
				   sizeof (cairo_path_buf_t));
    if (buf) {
	buf->next = NULL;
	buf->prev = NULL;
	buf->num_ops = 0;
	buf->num_points = 0;
	buf->buf_size = buf_size;

	buf->op = (cairo_path_op_t *) (buf + 1);
	buf->points = (cairo_point_t *) (buf->op + buf_size);
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
_cairo_path_buf_add_points (cairo_path_buf_t *buf,
			    cairo_point_t    *points,
			    int		      num_points)
{
    int i;

    for (i=0; i < num_points; i++) {
	buf->points[buf->num_points++] = points[i];
    }
}

static int const num_args[] =
{
    1, /* cairo_path_move_to */
    1, /* cairo_path_op_line_to */
    3, /* cairo_path_op_curve_to */
    0, /* cairo_path_op_close_path */
};

cairo_status_t
_cairo_path_fixed_interpret (const cairo_path_fixed_t		*path,
			     cairo_direction_t			 dir,
			     cairo_path_fixed_move_to_func_t	*move_to,
			     cairo_path_fixed_line_to_func_t	*line_to,
			     cairo_path_fixed_curve_to_func_t	*curve_to,
			     cairo_path_fixed_close_path_func_t	*close_path,
			     void				*closure)
{
    cairo_status_t status;
    const cairo_path_buf_t *buf;
    cairo_path_op_t op;
    cairo_bool_t forward = (dir == CAIRO_DIRECTION_FORWARD);
    int step = forward ? 1 : -1;

    for (buf = forward ? &path->buf_head.base : path->buf_tail;
	 buf;
	 buf = forward ? buf->next : buf->prev)
    {
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

	for (i=start; i != stop; i += step) {
	    op = buf->op[i];

	    if (! forward) {
		points -= num_args[(int) op];
	    }

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
	    case CAIRO_PATH_OP_CLOSE_PATH:
	    default:
		status = (*close_path) (closure);
		break;
	    }
	    if (unlikely (status))
		return status;

	    if (forward) {
		points += num_args[(int) op];
	    }

	}
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_append_move_to (void		 *closure,
		 const cairo_point_t  *point)
{
    cairo_path_fixed_t *path = (cairo_path_fixed_t *) closure;
    return _cairo_path_fixed_move_to (path, point->x, point->y);
}

static cairo_status_t
_append_line_to (void		 *closure,
		 const cairo_point_t *point)
{
    cairo_path_fixed_t *path = (cairo_path_fixed_t *) closure;
    return _cairo_path_fixed_line_to (path, point->x, point->y);
}

static cairo_status_t
_append_curve_to (void	  *closure,
		  const cairo_point_t *p0,
		  const cairo_point_t *p1,
		  const cairo_point_t *p2)
{
    cairo_path_fixed_t *path = (cairo_path_fixed_t *) closure;
    return _cairo_path_fixed_curve_to (path, p0->x, p0->y, p1->x, p1->y, p2->x, p2->y);
}

static cairo_status_t
_append_close_path (void *closure)
{
    cairo_path_fixed_t *path = (cairo_path_fixed_t *) closure;
    return _cairo_path_fixed_close_path (path);
}

cairo_status_t
_cairo_path_fixed_append (cairo_path_fixed_t		  *path,
			  const cairo_path_fixed_t	  *other,
			  cairo_direction_t		   dir)
{
    return _cairo_path_fixed_interpret (other, dir,
					_append_move_to,
					_append_line_to,
					_append_curve_to,
					_append_close_path,
					path);
}

static void
_cairo_path_fixed_offset_and_scale (cairo_path_fixed_t *path,
				    cairo_fixed_t offx,
				    cairo_fixed_t offy,
				    cairo_fixed_t scalex,
				    cairo_fixed_t scaley)
{
    cairo_path_buf_t *buf = &path->buf_head.base;
    unsigned int i;

    while (buf) {
	 for (i = 0; i < buf->num_points; i++) {
	     if (scalex != CAIRO_FIXED_ONE)
		 buf->points[i].x = _cairo_fixed_mul (buf->points[i].x, scalex);
	     buf->points[i].x += offx;

	     if (scaley != CAIRO_FIXED_ONE)
		 buf->points[i].y = _cairo_fixed_mul (buf->points[i].y, scaley);
	     buf->points[i].y += offy;
	 }

	 buf = buf->next;
    }
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
			     cairo_matrix_t     *matrix)
{
    cairo_path_buf_t *buf;
    unsigned int i;
    double dx, dy;

    if (matrix->yx == 0.0 && matrix->xy == 0.0) {
	/* Fast path for the common case of scale+transform */
	_cairo_path_fixed_offset_and_scale (path,
					    _cairo_fixed_from_double (matrix->x0),
					    _cairo_fixed_from_double (matrix->y0),
					    _cairo_fixed_from_double (matrix->xx),
					    _cairo_fixed_from_double (matrix->yy));
	return;
    }

    buf = &path->buf_head.base;
    while (buf) {
	 for (i = 0; i < buf->num_points; i++) {
	    dx = _cairo_fixed_to_double (buf->points[i].x);
	    dy = _cairo_fixed_to_double (buf->points[i].y);

	    cairo_matrix_transform_point (matrix, &dx, &dy);

	    buf->points[i].x = _cairo_fixed_from_double (dx);
	    buf->points[i].y = _cairo_fixed_from_double (dy);
	 }

	 buf = buf->next;
    }
}

cairo_bool_t
_cairo_path_fixed_is_equal (cairo_path_fixed_t *path,
			    cairo_path_fixed_t *other)
{
    cairo_path_buf_t *path_buf, *other_buf;

    if (path->current_point.x != other->current_point.x ||
	path->current_point.y != other->current_point.y ||
	path->has_current_point != other->has_current_point ||
	path->has_curve_to != other->has_curve_to ||
	path->last_move_point.x != other->last_move_point.x ||
	path->last_move_point.y != other->last_move_point.y)
	return FALSE;

    other_buf = &other->buf_head.base;
    for (path_buf = &path->buf_head.base;
	 path_buf != NULL;
	 path_buf = path_buf->next)
    {
	if (other_buf == NULL ||
	    path_buf->num_ops != other_buf->num_ops ||
	    path_buf->num_points != other_buf->num_points ||
	    memcmp (path_buf->op, other_buf->op,
		    sizeof (cairo_path_op_t) * path_buf->num_ops) != 0 ||
	    memcmp (path_buf->points, other_buf->points,
		    sizeof (cairo_point_t) * path_buf->num_points) != 0)
	{
	    return FALSE;
	}
	other_buf = other_buf->next;
    }
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

    if (!path->has_curve_to) {
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

cairo_bool_t
_cairo_path_fixed_is_empty (cairo_path_fixed_t *path)
{
    if (path->buf_head.base.num_ops == 0)
	return TRUE;

    return FALSE;
}

/*
 * Check whether the given path contains a single rectangle.
 */
cairo_bool_t
_cairo_path_fixed_is_box (cairo_path_fixed_t *path,
			  cairo_box_t *box)
{
    cairo_path_buf_t *buf = &path->buf_head.base;

    /* We can't have more than one buf for this check */
    if (buf->next != NULL)
	return FALSE;

    /* Do we have the right number of ops? */
    if (buf->num_ops != 5 && buf->num_ops != 6)
	return FALSE;

    /* Check whether the ops are those that would be used for a rectangle */
    if (buf->op[0] != CAIRO_PATH_OP_MOVE_TO ||
	buf->op[1] != CAIRO_PATH_OP_LINE_TO ||
	buf->op[2] != CAIRO_PATH_OP_LINE_TO ||
	buf->op[3] != CAIRO_PATH_OP_LINE_TO)
    {
	return FALSE;
    }

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

    /* Ok, we may have a box, if the points line up */
    if (buf->points[0].y == buf->points[1].y &&
	buf->points[1].x == buf->points[2].x &&
	buf->points[2].y == buf->points[3].y &&
	buf->points[3].x == buf->points[0].x)
    {
	box->p1 = buf->points[0];
	box->p2 = buf->points[2];
	return TRUE;
    }

    if (buf->points[0].x == buf->points[1].x &&
	buf->points[1].y == buf->points[2].y &&
	buf->points[2].x == buf->points[3].x &&
	buf->points[3].y == buf->points[0].y)
    {
	box->p1 = buf->points[0];
	box->p2 = buf->points[2];
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
_cairo_path_fixed_is_rectangle (cairo_path_fixed_t *path,
				cairo_box_t        *box)
{
    cairo_path_buf_t *buf = &path->buf_head.base;

    if (!_cairo_path_fixed_is_box (path, box))
	return FALSE;

    if (buf->points[0].y == buf->points[1].y)
	return TRUE;

    return FALSE;
}

void
_cairo_path_fixed_iter_init (cairo_path_fixed_iter_t *iter,
			     cairo_path_fixed_t *path)
{
    iter->buf = &path->buf_head.base;
    iter->n_op = 0;
    iter->n_point = 0;
}

static cairo_bool_t
_cairo_path_fixed_iter_next_op (cairo_path_fixed_iter_t *iter)
{
    if (++iter->n_op >= iter->buf->num_ops) {
	iter->buf = iter->buf->next;
	iter->n_op = 0;
	iter->n_point = 0;
    }

    return iter->buf != NULL;
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
	box->p1 = points[0];
	box->p2 = points[2];
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

/* Closure for path region testing.  Every move_to must be to integer
 * coordinates, there must be no curves, and every line_to or
 * close_path must represent an axis aligned line to an integer point.
 * We're relying on the path interpreter always sending a single
 * move_to at the start of any subpath, not receiving having any
 * superfluous move_tos, and the path intepreter bailing with our
 * first non-successful error. */
typedef struct cairo_path_region_tester {
    cairo_point_t last_move_point;
    cairo_point_t current_point;
} cprt_t;

static cairo_status_t
_cprt_line_to (void          *closure,
	       const cairo_point_t *p2)
{
    cprt_t *self = closure;
    cairo_point_t *p1 = &self->current_point;

    if (p2->x == p1->x) {
	if (_cairo_fixed_is_integer (p2->y)) {
	    p1->y = p2->y;
	    return CAIRO_STATUS_SUCCESS;
	}
    } else if (p2->y == p1->y) {
	if (_cairo_fixed_is_integer (p2->x)) {
	    p1->x = p2->x;
	    return CAIRO_STATUS_SUCCESS;
	}
    }

    return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_status_t
_cprt_close_path (void *closure)
{
    cprt_t *self = closure;
    return _cprt_line_to (closure, &self->last_move_point);
}

static cairo_status_t
_cprt_move_to (void		    *closure,
	       const cairo_point_t  *p)
{
    cprt_t *self = closure;
    cairo_status_t status;

    status = _cprt_close_path (closure);
    if (status)
	return status;

    if (_cairo_fixed_is_integer (p->x) && _cairo_fixed_is_integer (p->y)) {
	self->current_point = *p;
	self->last_move_point = *p;
	return CAIRO_STATUS_SUCCESS;
    }

    return CAIRO_INT_STATUS_UNSUPPORTED;
}

/*
 * Check whether the given path is representable as a region.
 * That is, if the path contains only axis aligned lines between
 * integer coordinates in device space.
 */
cairo_bool_t
_cairo_path_fixed_is_region (cairo_path_fixed_t *path)
{
    cprt_t cprt;

    if (path->has_curve_to)
	return FALSE;

    cprt.current_point.x = 0;
    cprt.current_point.y = 0;
    cprt.last_move_point.x = 0;
    cprt.last_move_point.y = 0;

    return _cairo_path_fixed_interpret (path,
					CAIRO_DIRECTION_FORWARD,
					_cprt_move_to,
					_cprt_line_to,
					NULL,
					_cprt_close_path,
					&cprt) == CAIRO_STATUS_SUCCESS;
}
