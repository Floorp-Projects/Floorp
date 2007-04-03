/* cairo - a vector graphics library with display and print output
 *
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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@redhat.com>
 */

#include "cairo-path-private.h"
#include "cairo-path-fixed-private.h"
#include "cairo-gstate-private.h"

const cairo_path_t _cairo_path_nil = { CAIRO_STATUS_NO_MEMORY, NULL, 0 };

/* Closure for path interpretation. */
typedef struct cairo_path_count {
    int count;
    double tolerance;
    cairo_point_t current_point;
} cpc_t;

static cairo_status_t
_cpc_move_to (void *closure, cairo_point_t *point)
{
    cpc_t *cpc = closure;

    cpc->count += 2;

    cpc->current_point = *point;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cpc_line_to (void *closure, cairo_point_t *point)
{
    cpc_t *cpc = closure;

    cpc->count += 2;

    cpc->current_point = *point;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cpc_curve_to (void		*closure,
	       cairo_point_t	*p1,
	       cairo_point_t	*p2,
	       cairo_point_t	*p3)
{
    cpc_t *cpc = closure;

    cpc->count += 4;

    cpc->current_point = *p3;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cpc_curve_to_flatten (void		*closure,
		       cairo_point_t	*p1,
		       cairo_point_t	*p2,
		       cairo_point_t	*p3)
{
    cpc_t *cpc = closure;
    cairo_status_t status;
    cairo_spline_t spline;
    int i;

    cairo_point_t *p0 = &cpc->current_point;

    status = _cairo_spline_init (&spline, p0, p1, p2, p3);
    if (status == CAIRO_INT_STATUS_DEGENERATE)
	return CAIRO_STATUS_SUCCESS;

    status = _cairo_spline_decompose (&spline, cpc->tolerance);
    if (status)
      goto out;

    for (i=1; i < spline.num_points; i++)
	_cpc_line_to (cpc, &spline.points[i]);

    cpc->current_point = *p3;

    status = CAIRO_STATUS_SUCCESS;

 out:
    _cairo_spline_fini (&spline);
    return status;
}

static cairo_status_t
_cpc_close_path (void *closure)
{
    cpc_t *cpc = closure;

    cpc->count += 1;

    return CAIRO_STATUS_SUCCESS;
}

static int
_cairo_path_count (cairo_path_t		*path,
		   cairo_path_fixed_t	*path_fixed,
		   double		 tolerance,
		   cairo_bool_t		 flatten)
{
    cpc_t cpc;

    cpc.count = 0;
    cpc.tolerance = tolerance;
    cpc.current_point.x = 0;
    cpc.current_point.y = 0;

    _cairo_path_fixed_interpret (path_fixed,
				 CAIRO_DIRECTION_FORWARD,
				 _cpc_move_to,
				 _cpc_line_to,
				 flatten ?
				 _cpc_curve_to_flatten :
				 _cpc_curve_to,
				 _cpc_close_path,
				 &cpc);

    return cpc.count;
}

/* Closure for path interpretation. */
typedef struct cairo_path_populate {
    cairo_path_data_t *data;
    cairo_gstate_t    *gstate;
    cairo_point_t      current_point;
} cpp_t;

static cairo_status_t
_cpp_move_to (void *closure, cairo_point_t *point)
{
    cpp_t *cpp = closure;
    cairo_path_data_t *data = cpp->data;
    double x, y;

    x = _cairo_fixed_to_double (point->x);
    y = _cairo_fixed_to_double (point->y);

    _cairo_gstate_backend_to_user (cpp->gstate, &x, &y);

    data->header.type = CAIRO_PATH_MOVE_TO;
    data->header.length = 2;

    /* We index from 1 to leave room for data->header */
    data[1].point.x = x;
    data[1].point.y = y;

    cpp->data += data->header.length;

    cpp->current_point = *point;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cpp_line_to (void *closure, cairo_point_t *point)
{
    cpp_t *cpp = closure;
    cairo_path_data_t *data = cpp->data;
    double x, y;

    x = _cairo_fixed_to_double (point->x);
    y = _cairo_fixed_to_double (point->y);

    _cairo_gstate_backend_to_user (cpp->gstate, &x, &y);

    data->header.type = CAIRO_PATH_LINE_TO;
    data->header.length = 2;

    /* We index from 1 to leave room for data->header */
    data[1].point.x = x;
    data[1].point.y = y;

    cpp->data += data->header.length;

    cpp->current_point = *point;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cpp_curve_to (void		*closure,
	       cairo_point_t	*p1,
	       cairo_point_t	*p2,
	       cairo_point_t	*p3)
{
    cpp_t *cpp = closure;
    cairo_path_data_t *data = cpp->data;
    double x1, y1;
    double x2, y2;
    double x3, y3;

    x1 = _cairo_fixed_to_double (p1->x);
    y1 = _cairo_fixed_to_double (p1->y);
    _cairo_gstate_backend_to_user (cpp->gstate, &x1, &y1);

    x2 = _cairo_fixed_to_double (p2->x);
    y2 = _cairo_fixed_to_double (p2->y);
    _cairo_gstate_backend_to_user (cpp->gstate, &x2, &y2);

    x3 = _cairo_fixed_to_double (p3->x);
    y3 = _cairo_fixed_to_double (p3->y);
    _cairo_gstate_backend_to_user (cpp->gstate, &x3, &y3);

    data->header.type = CAIRO_PATH_CURVE_TO;
    data->header.length = 4;

    /* We index from 1 to leave room for data->header */
    data[1].point.x = x1;
    data[1].point.y = y1;

    data[2].point.x = x2;
    data[2].point.y = y2;

    data[3].point.x = x3;
    data[3].point.y = y3;

    cpp->data += data->header.length;

    cpp->current_point = *p3;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cpp_curve_to_flatten (void		*closure,
		       cairo_point_t	*p1,
		       cairo_point_t	*p2,
		       cairo_point_t	*p3)
{
    cpp_t *cpp = closure;
    cairo_status_t status;
    cairo_spline_t spline;
    int i;

    cairo_point_t *p0 = &cpp->current_point;

    status = _cairo_spline_init (&spline, p0, p1, p2, p3);
    if (status == CAIRO_INT_STATUS_DEGENERATE)
	return CAIRO_STATUS_SUCCESS;

    status = _cairo_spline_decompose (&spline, cpp->gstate->tolerance);
    if (status)
      goto out;

    for (i=1; i < spline.num_points; i++)
	_cpp_line_to (cpp, &spline.points[i]);

    cpp->current_point = *p3;

    status = CAIRO_STATUS_SUCCESS;

 out:
    _cairo_spline_fini (&spline);
    return status;
}

static cairo_status_t
_cpp_close_path (void *closure)
{
    cpp_t *cpp = closure;
    cairo_path_data_t *data = cpp->data;

    data->header.type = CAIRO_PATH_CLOSE_PATH;
    data->header.length = 1;

    cpp->data += data->header.length;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_path_populate (cairo_path_t		*path,
		      cairo_path_fixed_t	*path_fixed,
		      cairo_gstate_t		*gstate,
		      cairo_bool_t		 flatten)
{
    cpp_t cpp;

    cpp.data = path->data;
    cpp.gstate = gstate;
    cpp.current_point.x = 0;
    cpp.current_point.y = 0;

    _cairo_path_fixed_interpret (path_fixed,
				 CAIRO_DIRECTION_FORWARD,
				 _cpp_move_to,
				 _cpp_line_to,
				 flatten ?
				 _cpp_curve_to_flatten :
				 _cpp_curve_to,
				 _cpp_close_path,
				 &cpp);

    /* Sanity check the count */
    assert (cpp.data - path->data == path->num_data);
}

cairo_path_t *
_cairo_path_create_in_error (cairo_status_t status)
{
    cairo_path_t *path;

    path = malloc (sizeof (cairo_path_t));
    if (path == NULL)
	return (cairo_path_t*) &_cairo_path_nil;

    path->num_data = 0;
    path->data = NULL;
    path->status = status;

    return path;
}

static cairo_path_t *
_cairo_path_create_internal (cairo_path_fixed_t *path_fixed,
			     cairo_gstate_t     *gstate,
			     cairo_bool_t	 flatten)
{
    cairo_path_t *path;

    path = malloc (sizeof (cairo_path_t));
    if (path == NULL)
	return (cairo_path_t*) &_cairo_path_nil;

    path->num_data = _cairo_path_count (path, path_fixed,
					gstate->tolerance, flatten);

    path->data = malloc (path->num_data * sizeof (cairo_path_data_t));
    if (path->data == NULL) {
	free (path);
	return (cairo_path_t*) &_cairo_path_nil;
    }

    path->status = CAIRO_STATUS_SUCCESS;

    _cairo_path_populate (path, path_fixed,
			  gstate, flatten);

    return path;
}

/**
 * cairo_path_destroy:
 * @path: a path previously returned by either cairo_copy_path() or
 * cairo_copy_path_flat().
 *
 * Immediately releases all memory associated with @path. After a call
 * to cairo_path_destroy() the @path pointer is no longer valid and
 * should not be used further.
 *
 * NOTE: cairo_path_destroy function should only be called with a
 * pointer to a #cairo_path_t returned by a cairo function. Any path
 * that is created manually (ie. outside of cairo) should be destroyed
 * manually as well.
 **/
void
cairo_path_destroy (cairo_path_t *path)
{
    if (path == NULL || path == &_cairo_path_nil)
	return;

    free (path->data);
    path->num_data = 0;
    free (path);
}

/**
 * _cairo_path_create:
 * @path: a fixed-point, device-space path to be converted and copied
 * @gstate: the current graphics state
 *
 * Creates a user-space #cairo_path_t copy of the given device-space
 * @path. The @gstate parameter provides the inverse CTM for the
 * conversion.
 *
 * Return value: the new copy of the path. If there is insufficient
 * memory a pointer to a special static cairo_path_nil will be
 * returned instead with status==CAIRO_STATUS_NO_MEMORY and
 * data==NULL.
 **/
cairo_path_t *
_cairo_path_create (cairo_path_fixed_t *path,
		    cairo_gstate_t     *gstate)
{
    return _cairo_path_create_internal (path, gstate, FALSE);
}

/**
 * _cairo_path_create_flat:
 * @path: a fixed-point, device-space path to be flattened, converted and copied
 * @gstate: the current graphics state
 *
 * Creates a flattened, user-space #cairo_path_t copy of the given
 * device-space @path. The @gstate parameter provide the inverse CTM
 * for the conversion, as well as the tolerance value to control the
 * accuracy of the flattening.
 *
 * Return value: the flattened copy of the path. If there is insufficient
 * memory a pointer to a special static cairo_path_nil will be
 * returned instead with status==CAIRO_STATUS_NO_MEMORY and
 * data==NULL.
 **/
cairo_path_t *
_cairo_path_create_flat (cairo_path_fixed_t *path,
			 cairo_gstate_t     *gstate)
{
    return _cairo_path_create_internal (path, gstate, TRUE);
}

/**
 * _cairo_path_append_to_context:
 * @path: the path data to be appended
 * @cr: a cairo context
 *
 * Append @path to the current path within @cr.
 *
 * Return value: CAIRO_STATUS_INVALID_PATH_DATA if the data in @path
 * is invalid, and CAIRO_STATUS_SUCCESS otherwise.
 **/
cairo_status_t
_cairo_path_append_to_context (const cairo_path_t	*path,
			       cairo_t			*cr)
{
    int i;
    cairo_path_data_t *p;

    for (i=0; i < path->num_data; i += path->data[i].header.length) {
	p = &path->data[i];
	switch (p->header.type) {
	case CAIRO_PATH_MOVE_TO:
	    if (p->header.length < 2)
		return CAIRO_STATUS_INVALID_PATH_DATA;
	    cairo_move_to (cr,
			   p[1].point.x, p[1].point.y);
	    break;
	case CAIRO_PATH_LINE_TO:
	    if (p->header.length < 2)
		return CAIRO_STATUS_INVALID_PATH_DATA;
	    cairo_line_to (cr,
			   p[1].point.x, p[1].point.y);
	    break;
	case CAIRO_PATH_CURVE_TO:
	    if (p->header.length < 4)
		return CAIRO_STATUS_INVALID_PATH_DATA;
	    cairo_curve_to (cr,
			    p[1].point.x, p[1].point.y,
			    p[2].point.x, p[2].point.y,
			    p[3].point.x, p[3].point.y);
	    break;
	case CAIRO_PATH_CLOSE_PATH:
	    if (p->header.length < 1)
		return CAIRO_STATUS_INVALID_PATH_DATA;
	    cairo_close_path (cr);
	    break;
	default:
	    return CAIRO_STATUS_INVALID_PATH_DATA;
	}
    }

    return CAIRO_STATUS_SUCCESS;
}
