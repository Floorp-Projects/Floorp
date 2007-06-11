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

#include <stdlib.h>
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
_cairo_path_buf_create (void);

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
    path->buf_head->next = NULL;
    path->buf_head->prev = NULL;
    path->buf_tail = path->buf_head;

    path->buf_head->num_ops = 0;
    path->buf_head->num_points = 0;

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

    _cairo_path_fixed_init (path);
    path->current_point = other->current_point;
    path->has_current_point = other->has_current_point;
    path->has_curve_to = other->has_curve_to;
    path->last_move_point = other->last_move_point;

    path->buf_head->num_ops = other->buf_head->num_ops;
    path->buf_head->num_points = other->buf_head->num_points;
    memcpy (path->buf_head->op, other->buf_head->op,
	    other->buf_head->num_ops * sizeof (other->buf_head->op[0]));
    memcpy (path->buf_head->points, other->buf_head->points,
	    other->buf_head->num_points * sizeof (other->buf_head->points[0]));
    for (other_buf = other->buf_head->next;
	 other_buf;
	 other_buf = other_buf->next)
    {
	buf = _cairo_path_buf_create ();
	if (buf == NULL) {
	    _cairo_path_fixed_fini (path);
	    return CAIRO_STATUS_NO_MEMORY;
	}
	memcpy (buf, other_buf, sizeof (cairo_path_buf_t));
	_cairo_path_fixed_add_buf (path, buf);
    }

    return CAIRO_STATUS_SUCCESS;
}

cairo_path_fixed_t *
_cairo_path_fixed_create (void)
{
    cairo_path_fixed_t	*path = malloc (sizeof (cairo_path_fixed_t));

    if (!path)
	return NULL;
    _cairo_path_fixed_init (path);
    return path;
}

void
_cairo_path_fixed_fini (cairo_path_fixed_t *path)
{
    cairo_path_buf_t *buf;

    buf = path->buf_head->next;
    while (buf) {
	cairo_path_buf_t *this = buf;
	buf = buf->next;
	_cairo_path_buf_destroy (this);
    }
    path->buf_head->next = NULL;
    path->buf_head->prev = NULL;
    path->buf_tail = path->buf_head;
    path->buf_head->num_ops = 0;
    path->buf_head->num_points = 0;

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
	if (status)
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
	return CAIRO_STATUS_NO_CURRENT_POINT;

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

    if (status)
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
	return CAIRO_STATUS_NO_CURRENT_POINT;

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
	if (status)
	    return status;
    }

    status = _cairo_path_fixed_add (path, CAIRO_PATH_OP_CURVE_TO, point, 3);
    if (status)
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
	return CAIRO_STATUS_NO_CURRENT_POINT;

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
    if (status)
	return status;

    status = _cairo_path_fixed_move_to (path,
					path->last_move_point.x,
					path->last_move_point.y);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_path_fixed_get_current_point (cairo_path_fixed_t *path,
				     cairo_fixed_t	*x,
				     cairo_fixed_t	*y)
{
    if (! path->has_current_point)
	return CAIRO_STATUS_NO_CURRENT_POINT;

    *x = path->current_point.x;
    *y = path->current_point.y;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_fixed_add (cairo_path_fixed_t *path,
		       cairo_path_op_t	   op,
		       cairo_point_t	  *points,
		       int		   num_points)
{
    if (path->buf_tail->num_ops + 1 > CAIRO_PATH_BUF_SIZE ||
	path->buf_tail->num_points + num_points > CAIRO_PATH_BUF_SIZE)
    {
	cairo_path_buf_t *buf;

	buf = _cairo_path_buf_create ();
	if (buf == NULL)
	    return CAIRO_STATUS_NO_MEMORY;

	_cairo_path_fixed_add_buf (path, buf);
    }

    _cairo_path_buf_add_op (path->buf_tail, op);
    _cairo_path_buf_add_points (path->buf_tail, points, num_points);

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
_cairo_path_buf_create (void)
{
    cairo_path_buf_t *buf;

    buf = malloc (sizeof (cairo_path_buf_t));

    if (buf) {
	buf->next = NULL;
	buf->prev = NULL;
	buf->num_ops = 0;
	buf->num_points = 0;
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
_cairo_path_fixed_interpret (cairo_path_fixed_t			*path,
			     cairo_direction_t			 dir,
			     cairo_path_fixed_move_to_func_t	*move_to,
			     cairo_path_fixed_line_to_func_t	*line_to,
			     cairo_path_fixed_curve_to_func_t	*curve_to,
			     cairo_path_fixed_close_path_func_t	*close_path,
			     void				*closure)
{
    cairo_status_t status;
    cairo_path_buf_t *buf;
    cairo_path_op_t op;
    cairo_bool_t forward = (dir == CAIRO_DIRECTION_FORWARD);
    int step = forward ? 1 : -1;

    for (buf = forward ? path->buf_head : path->buf_tail;
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
		points -= num_args[op];
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
	    if (status)
		return status;

	    if (forward) {
		points += num_args[op];
	    }

	}
    }

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_path_fixed_offset_and_scale (cairo_path_fixed_t *path,
				    cairo_fixed_t offx,
				    cairo_fixed_t offy,
				    cairo_fixed_t scalex,
				    cairo_fixed_t scaley)
{
    cairo_path_buf_t *buf = path->buf_head;
    int i;
    cairo_int64_t i64temp;
    cairo_fixed_t fixedtemp;

    while (buf) {
	 for (i = 0; i < buf->num_points; i++) {
	     if (scalex == CAIRO_FIXED_ONE) {
		 buf->points[i].x += offx;
	     } else {
		 fixedtemp = buf->points[i].x + offx;
		 i64temp = _cairo_int32x32_64_mul (fixedtemp, scalex);
		 buf->points[i].x = _cairo_int64_to_int32(_cairo_int64_rsl (i64temp, 16));
	     }

	     if (scaley == CAIRO_FIXED_ONE) {
		 buf->points[i].y += offy;
	     } else {
		 fixedtemp = buf->points[i].y + offy;
		 i64temp = _cairo_int32x32_64_mul (fixedtemp, scaley);
		 buf->points[i].y = _cairo_int64_to_int32(_cairo_int64_rsl (i64temp, 16));
	     }
	 }

	 buf = buf->next;
    }
}


/**
 * _cairo_path_fixed_device_transform:
 * @path: a #cairo_path_fixed_t to be transformed
 * @device_transform: a matrix with only scaling/translation (no rotation or shear)
 *
 * Transform the fixed-point path according to the scaling and
 * translation of the given matrix. This function assert()s that the
 * given matrix has no rotation or shear elements, (that is, xy and yx
 * are 0.0).
 **/
void
_cairo_path_fixed_device_transform (cairo_path_fixed_t	*path,
				    cairo_matrix_t	*device_transform)
{
    assert (device_transform->yx == 0.0 && device_transform->xy == 0.0);
    /* XXX: FRAGILE: I'm not really sure whether we're doing the
     * "right" thing here if there is both scaling and translation in
     * the matrix. But for now, the internals guarantee that we won't
     * really ever have both going on. */
    _cairo_path_fixed_offset_and_scale (path,
					_cairo_fixed_from_double (device_transform->x0),
					_cairo_fixed_from_double (device_transform->y0),
					_cairo_fixed_from_double (device_transform->xx),
					_cairo_fixed_from_double (device_transform->yy));
}
