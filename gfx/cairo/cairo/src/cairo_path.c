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

#include <stdlib.h>
#include "cairoint.h"

/* private functions */
static cairo_status_t
_cairo_path_add (cairo_path_t *path, cairo_path_op_t op, cairo_point_t *points, int num_pts);

static void
_cairo_path_add_op_buf (cairo_path_t *path, cairo_path_op_buf_t *op);

static cairo_status_t
_cairo_path_new_op_buf (cairo_path_t *path);

static void
_cairo_path_add_arg_buf (cairo_path_t *path, cairo_path_arg_buf_t *arg);

static cairo_status_t
_cairo_path_new_arg_buf (cairo_path_t *path);

static cairo_path_op_buf_t *
_cairo_path_op_buf_create (void);

static void
_cairo_path_op_buf_destroy (cairo_path_op_buf_t *buf);

static void
_cairo_path_op_buf_add (cairo_path_op_buf_t *op_buf, cairo_path_op_t op);

static cairo_path_arg_buf_t *
_cairo_path_arg_buf_create (void);

static void
_cairo_path_arg_buf_destroy (cairo_path_arg_buf_t *buf);

static void
_cairo_path_arg_buf_add (cairo_path_arg_buf_t *arg, cairo_point_t *points, int num_points);

void
_cairo_path_init (cairo_path_t *path)
{
    path->op_head = NULL;
    path->op_tail = NULL;

    path->arg_head = NULL;
    path->arg_tail = NULL;

    path->current_point.x = 0;
    path->current_point.y = 0;
    path->has_current_point = 0;
    path->last_move_point = path->current_point;
}

cairo_status_t
_cairo_path_init_copy (cairo_path_t *path, cairo_path_t *other)
{
    cairo_path_op_buf_t *op, *other_op;
    cairo_path_arg_buf_t *arg, *other_arg;

    _cairo_path_init (path);
    path->current_point = other->current_point;
    path->has_current_point = other->has_current_point;
    path->last_move_point = other->last_move_point;

    for (other_op = other->op_head; other_op; other_op = other_op->next) {
	op = _cairo_path_op_buf_create ();
	if (op == NULL) {
	    _cairo_path_fini(path);
	    return CAIRO_STATUS_NO_MEMORY;
	}
	*op = *other_op;
	_cairo_path_add_op_buf (path, op);
    }

    for (other_arg = other->arg_head; other_arg; other_arg = other_arg->next) {
	arg = _cairo_path_arg_buf_create ();
	if (arg == NULL) {
	    _cairo_path_fini(path);
	    return CAIRO_STATUS_NO_MEMORY;
	}
	*arg = *other_arg;
	_cairo_path_add_arg_buf (path, arg);
    }

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_path_fini (cairo_path_t *path)
{
    cairo_path_op_buf_t *op;
    cairo_path_arg_buf_t *arg;

    while (path->op_head) {
	op = path->op_head;
	path->op_head = op->next;
	_cairo_path_op_buf_destroy (op);
    }
    path->op_tail = NULL;

    while (path->arg_head) {
	arg = path->arg_head;
	path->arg_head = arg->next;
	_cairo_path_arg_buf_destroy (arg);
    }
    path->arg_tail = NULL;

    path->has_current_point = 0;
}

cairo_status_t
_cairo_path_move_to (cairo_path_t *path, cairo_point_t *point)
{
    cairo_status_t status;

    status = _cairo_path_add (path, CAIRO_PATH_OP_MOVE_TO, point, 1);
    if (status)
	return status;

    path->current_point = *point;
    path->has_current_point = 1;
    path->last_move_point = path->current_point;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_path_rel_move_to (cairo_path_t *path, cairo_distance_t *distance)
{
    cairo_point_t point;

    point.x = path->current_point.x + distance->dx;
    point.y = path->current_point.y + distance->dy;

    return _cairo_path_move_to (path, &point);
}

cairo_status_t
_cairo_path_line_to (cairo_path_t *path, cairo_point_t *point)
{
    cairo_status_t status;

    status = _cairo_path_add (path, CAIRO_PATH_OP_LINE_TO, point, 1);
    if (status)
	return status;

    path->current_point = *point;
    path->has_current_point = 1;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_path_rel_line_to (cairo_path_t *path, cairo_distance_t *distance)
{
    cairo_point_t point;

    point.x = path->current_point.x + distance->dx;
    point.y = path->current_point.y + distance->dy;

    return _cairo_path_line_to (path, &point);
}

cairo_status_t
_cairo_path_curve_to (cairo_path_t *path,
		      cairo_point_t *p0,
		      cairo_point_t *p1,
		      cairo_point_t *p2)
{
    cairo_status_t status;
    cairo_point_t point[3];

    point[0] = *p0;
    point[1] = *p1;
    point[2] = *p2;

    status = _cairo_path_add (path, CAIRO_PATH_OP_CURVE_TO, point, 3);
    if (status)
	return status;

    path->current_point = *p2;
    path->has_current_point = 1;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_path_rel_curve_to (cairo_path_t *path,
			  cairo_distance_t *d0,
			  cairo_distance_t *d1,
			  cairo_distance_t *d2)
{
    cairo_point_t p0, p1, p2;

    p0.x = path->current_point.x + d0->dx;
    p0.y = path->current_point.y + d0->dy;

    p1.x = path->current_point.x + d1->dx;
    p1.y = path->current_point.y + d1->dy;

    p2.x = path->current_point.x + d2->dx;
    p2.y = path->current_point.y + d2->dy;

    return _cairo_path_curve_to (path, &p0, &p1, &p2);
}

cairo_status_t
_cairo_path_close_path (cairo_path_t *path)
{
    cairo_status_t status;

    status = _cairo_path_add (path, CAIRO_PATH_OP_CLOSE_PATH, NULL, 0);
    if (status)
	return status;

    path->current_point.x = path->last_move_point.x;
    path->current_point.y = path->last_move_point.y;
    path->has_current_point = 1;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_path_current_point (cairo_path_t *path, cairo_point_t *point)
{
    if (! path->has_current_point)
	return CAIRO_STATUS_NO_CURRENT_POINT;

    *point = path->current_point;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_add (cairo_path_t *path, cairo_path_op_t op, cairo_point_t *points, int num_points)
{
    cairo_status_t status;

    if (path->op_tail == NULL || path->op_tail->num_ops + 1 > CAIRO_PATH_BUF_SZ) {
	status = _cairo_path_new_op_buf (path);
	if (status)
	    return status;
    }
    _cairo_path_op_buf_add (path->op_tail, op);

    if (path->arg_tail == NULL || path->arg_tail->num_points + num_points > CAIRO_PATH_BUF_SZ) {
	status = _cairo_path_new_arg_buf (path);
	if (status)
	    return status;
    }
    _cairo_path_arg_buf_add (path->arg_tail, points, num_points);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_path_add_op_buf (cairo_path_t *path, cairo_path_op_buf_t *op)
{
    op->next = NULL;
    op->prev = path->op_tail;

    if (path->op_tail) {
	path->op_tail->next = op;
    } else {
	path->op_head = op;
    }

    path->op_tail = op;
}

static cairo_status_t
_cairo_path_new_op_buf (cairo_path_t *path)
{
    cairo_path_op_buf_t *op;

    op = _cairo_path_op_buf_create ();
    if (op == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    _cairo_path_add_op_buf (path, op);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_path_add_arg_buf (cairo_path_t *path, cairo_path_arg_buf_t *arg)
{
    arg->next = NULL;
    arg->prev = path->arg_tail;

    if (path->arg_tail) {
	path->arg_tail->next = arg;
    } else {
	path->arg_head = arg;
    }

    path->arg_tail = arg;
}

static cairo_status_t
_cairo_path_new_arg_buf (cairo_path_t *path)
{
    cairo_path_arg_buf_t *arg;

    arg = _cairo_path_arg_buf_create ();

    if (arg == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    _cairo_path_add_arg_buf (path, arg);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_path_op_buf_t *
_cairo_path_op_buf_create (void)
{
    cairo_path_op_buf_t *op;

    op = malloc (sizeof (cairo_path_op_buf_t));

    if (op) {
	op->num_ops = 0;
	op->next = NULL;
    }

    return op;
}

static void
_cairo_path_op_buf_destroy (cairo_path_op_buf_t *op)
{
    free (op);
}

static void
_cairo_path_op_buf_add (cairo_path_op_buf_t *op_buf, cairo_path_op_t op)
{
    op_buf->op[op_buf->num_ops++] = op;
}

static cairo_path_arg_buf_t *
_cairo_path_arg_buf_create (void)
{
    cairo_path_arg_buf_t *arg;

    arg = malloc (sizeof (cairo_path_arg_buf_t));

    if (arg) {
	arg->num_points = 0;
	arg->next = NULL;
    }

    return arg;
}

static void
_cairo_path_arg_buf_destroy (cairo_path_arg_buf_t *arg)
{
    free (arg);
}

static void
_cairo_path_arg_buf_add (cairo_path_arg_buf_t *arg, cairo_point_t *points, int num_points)
{
    int i;

    for (i=0; i < num_points; i++) {
	arg->points[arg->num_points++] = points[i];
    }
}

#define CAIRO_PATH_OP_MAX_ARGS 3

static int const num_args[] = 
{
    1, /* cairo_path_move_to */
    1, /* cairo_path_op_line_to */
    3, /* cairo_path_op_curve_to */
    0, /* cairo_path_op_close_path */
};

cairo_status_t
_cairo_path_interpret (cairo_path_t			*path,
		       cairo_direction_t		dir,
		       cairo_path_move_to_func_t	*move_to,
		       cairo_path_line_to_func_t	*line_to,
		       cairo_path_curve_to_func_t	*curve_to,
		       cairo_path_close_path_func_t	*close_path,
		       void				*closure)
{
    cairo_status_t status;
    int i, arg;
    cairo_path_op_buf_t *op_buf;
    cairo_path_op_t op;
    cairo_path_arg_buf_t *arg_buf = path->arg_head;
    int buf_i = 0;
    cairo_point_t point[CAIRO_PATH_OP_MAX_ARGS];
    int step = (dir == CAIRO_DIRECTION_FORWARD) ? 1 : -1;

    for (op_buf = (dir == CAIRO_DIRECTION_FORWARD) ? path->op_head : path->op_tail;
	 op_buf;
	 op_buf = (dir == CAIRO_DIRECTION_FORWARD) ? op_buf->next : op_buf->prev)
    {
	int start, stop;
	if (dir == CAIRO_DIRECTION_FORWARD) {
	    start = 0;
	    stop = op_buf->num_ops;
	} else {
	    start = op_buf->num_ops - 1;
	    stop = -1;
	}

	for (i=start; i != stop; i += step) {
	    op = op_buf->op[i];

	    if (dir == CAIRO_DIRECTION_REVERSE) {
		if (buf_i == 0) {
		    arg_buf = arg_buf->prev;
		    buf_i = arg_buf->num_points;
		}
		buf_i -= num_args[op];
	    }

	    for (arg = 0; arg < num_args[op]; arg++) {
		point[arg] = arg_buf->points[buf_i];
		buf_i++;
		if (buf_i >= arg_buf->num_points) {
		    arg_buf = arg_buf->next;
		    buf_i = 0;
		}
	    }

	    if (dir == CAIRO_DIRECTION_REVERSE) {
		buf_i -= num_args[op];
	    }

	    switch (op) {
	    case CAIRO_PATH_OP_MOVE_TO:
		status = (*move_to) (closure, &point[0]);
		break;
	    case CAIRO_PATH_OP_LINE_TO:
		status = (*line_to) (closure, &point[0]);
		break;
	    case CAIRO_PATH_OP_CURVE_TO:
		status = (*curve_to) (closure, &point[0], &point[1], &point[2]);
		break;
	    case CAIRO_PATH_OP_CLOSE_PATH:
	    default:
		status = (*close_path) (closure);
		break;
	    }
	    if (status)
		return status;
	}
    }

    return CAIRO_STATUS_SUCCESS;
}

