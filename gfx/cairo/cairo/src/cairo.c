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


#include "cairoint.h"

#define CAIRO_TOLERANCE_MINIMUM	0.0002 /* We're limited by 16 bits of sub-pixel precision */

#ifdef CAIRO_DO_SANITY_CHECKING
#include <assert.h>
static int 
cairo_sane_state (cairo_t *cr)
{    
    if (cr == NULL)
	return 0;

    switch (cr->status) {
    case CAIRO_STATUS_SUCCESS:
    case CAIRO_STATUS_NO_MEMORY:
    case CAIRO_STATUS_INVALID_RESTORE:
    case CAIRO_STATUS_INVALID_POP_GROUP:
    case CAIRO_STATUS_NO_CURRENT_POINT:
    case CAIRO_STATUS_INVALID_MATRIX:
    case CAIRO_STATUS_NO_TARGET_SURFACE:
    case CAIRO_STATUS_NULL_POINTER:
	break;
    default:
	return 0;
    }
    return 1;
}
#define CAIRO_CHECK_SANITY(cr) assert(cairo_sane_state ((cr)))
#else
#define CAIRO_CHECK_SANITY(cr) 
#endif


/**
 * cairo_create:
 * 
 * Creates a new #cairo_t with default values. The target
 * surface must be set on the #cairo_t with cairo_set_target_surface(),
 * or a backend-specific function like cairo_set_target_image() before
 * drawing with the #cairo_t.
 * 
 * Return value: a newly allocated #cairo_t with a reference
 *  count of 1. The initial reference count should be released
 *  with cairo_destroy() when you are done using the #cairo_t.
 **/
cairo_t *
cairo_create (void)
{
    cairo_t *cr;

    cr = malloc (sizeof (cairo_t));
    if (cr == NULL)
	return NULL;

    cr->status = CAIRO_STATUS_SUCCESS;
    cr->ref_count = 1;

    cr->gstate = _cairo_gstate_create ();
    if (cr->gstate == NULL)
	cr->status = CAIRO_STATUS_NO_MEMORY;

    CAIRO_CHECK_SANITY (cr);
    return cr;
}

/**
 * cairo_reference:
 * @cr: a #cairo_t
 * 
 * Increases the reference count on @cr by one. This prevents
 * @cr from being destroyed until a matching call to cairo_destroy() 
 * is made.
 **/
void
cairo_reference (cairo_t *cr)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->ref_count++;
    CAIRO_CHECK_SANITY (cr);
}

/**
 * cairo_destroy:
 * @cr: a #cairo_t
 * 
 * Decreases the reference count on @cr by one. If the result
 * is zero, then @cr and all associated resources are freed.
 * See cairo_destroy().
 **/
void
cairo_destroy (cairo_t *cr)
{
    CAIRO_CHECK_SANITY (cr);
    cr->ref_count--;
    if (cr->ref_count)
	return;

    while (cr->gstate) {
	cairo_gstate_t *tmp = cr->gstate;
	cr->gstate = tmp->next;

	_cairo_gstate_destroy (tmp);
    }

    free (cr);
}

/**
 * cairo_save:
 * @cr: a #cairo_t
 * 
 * Makes a copy of the current state of @cr and saves it
 * on an internal stack of saved states for @cr. When
 * cairo_restore() is called, @cr will be restored to
 * the saved state. Multiple calls to cairo_save() and
 * cairo_restore() can be nested; each call to cairo_restore()
 * restores the state from the matching paired cairo_save().
 *
 * It isn't necessary to clear all saved states before
 * a #cairo_t is freed. If the reference count of a #cairo_t
 * drops to zero in response to a call to cairo_destroy(),
 * any saved states will be freed along with the #cairo_t.
 **/
void
cairo_save (cairo_t *cr)
{
    cairo_gstate_t *top;

    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    if (cr->gstate) {
	top = _cairo_gstate_clone (cr->gstate);
    } else {
	top = _cairo_gstate_create ();
    }

    if (top == NULL) {
	cr->status = CAIRO_STATUS_NO_MEMORY;
	CAIRO_CHECK_SANITY (cr);
	return;
    }

    top->next = cr->gstate;
    cr->gstate = top;
    CAIRO_CHECK_SANITY (cr);
}
slim_hidden_def(cairo_save);

/**
 * cairo_restore:
 * @cr: a #cairo_t
 * 
 * Restores @cr to the state saved by a preceding call to
 * cairo_save() and removes that state from the stack of
 * saved states.
 **/
void
cairo_restore (cairo_t *cr)
{
    cairo_gstate_t *top;

    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    top = cr->gstate;
    cr->gstate = top->next;

    _cairo_gstate_destroy (top);

    if (cr->gstate == NULL)
	cr->status = CAIRO_STATUS_INVALID_RESTORE;
    
    if (cr->status)
	return;
   
    cr->status = _cairo_gstate_restore_external_state (cr->gstate);
    
    CAIRO_CHECK_SANITY (cr);
}
slim_hidden_def(cairo_restore);

/**
 * cairo_copy:
 * @dest: a #cairo_t
 * @src: another #cairo_t
 * 
 * This function copies all current state information from src to
 * dest. This includes the current point and path, the target surface,
 * the transformation matrix, and so forth.
 *
 * The stack of states saved with cairo_save() is <emphasis>not</emphasis>
 * not copied; nor are any saved states on @dest cleared. The
 * operation only copies the current state of @src to the current
 * state of @dest.
 **/
void
cairo_copy (cairo_t *dest, cairo_t *src)
{
    CAIRO_CHECK_SANITY (src);
    CAIRO_CHECK_SANITY (dest);
    if (dest->status)
	return;

    if (src->status) {
	dest->status = src->status;
	return;
    }

    dest->status = _cairo_gstate_copy (dest->gstate, src->gstate);
    CAIRO_CHECK_SANITY (src);
    CAIRO_CHECK_SANITY (dest);
}

/* XXX: I want to rethink this API
void
cairo_push_group (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = cairoPush (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_begin_group (cr->gstate);
}

void
cairo_pop_group (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_end_group (cr->gstate);
    if (cr->status)
	return;

    cr->status = cairoPop (cr);
}
*/

/**
 * cairo_set_target_surface:
 * @cr: a #cairo_t
 * @surface: a #cairo_surface_t
 * 
 * Directs output for a #cairo_t to a given surface. The surface
 * will be referenced by the #cairo_t, so you can immediately
 * call cairo_surface_destroy() on it if you don't need to
 * keep a reference to it around.
 **/
void
cairo_set_target_surface (cairo_t *cr, cairo_surface_t *surface)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_target_surface (cr->gstate, surface);
    CAIRO_CHECK_SANITY (cr);
}
slim_hidden_def(cairo_set_target_surface);

/**
 * cairo_set_target_image:
 * @cr: a #cairo_t
 * @data: a pointer to a buffer supplied by the application
 *    in which to write contents.
 * @format: the format of pixels in the buffer
 * @width: the width of the image to be stored in the buffer
 * @height: the eight of the image to be stored in the buffer
 * @stride: the number of bytes between the start of rows
 *   in the buffer. Having this be specified separate from @width
 *   allows for padding at the end of rows, or for writing
 *   to a subportion of a larger image.
 * 
 * Directs output for a #cairo_t to an in-memory image. The output
 * buffer must be kept around until the #cairo_t is destroyed or set
 * to to have a different target.  The initial contents of @buffer
 * will be used as the inital image contents; you must explicitely
 * clear the buffer, using, for example, cairo_rectangle() and
 * cairo_fill() if you want it cleared.
 **/
void
cairo_set_target_image (cairo_t		*cr,
			char		*data,
			cairo_format_t	format,
			int		width,
			int		height,
			int		stride)
{
    cairo_surface_t *surface;

    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    surface = cairo_surface_create_for_image (data,
					      format,
					      width, height, stride);
    if (surface == NULL) {
	cr->status = CAIRO_STATUS_NO_MEMORY;
	CAIRO_CHECK_SANITY (cr);
	return;
    }

    cairo_set_target_surface (cr, surface);

    cairo_surface_destroy (surface);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_set_operator (cairo_t *cr, cairo_operator_t op)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_operator (cr->gstate, op);
    CAIRO_CHECK_SANITY (cr);
}

/**
 * cairo_set_rgb_color:
 * @cr: a #cairo_t
 * @red: red component of color
 * @green: green component of color
 * @blue: blue component of color
 * 
 * Sets a constant color for filling and stroking. This replaces any
 * pattern set with cairo_set_pattern(). The color components are
 * floating point numbers in the range 0 to 1. If the values passed in
 * are outside that range, they will be clamped.
 **/
void
cairo_set_rgb_color (cairo_t *cr, double red, double green, double blue)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    _cairo_restrict_value (&red, 0.0, 1.0);
    _cairo_restrict_value (&green, 0.0, 1.0);
    _cairo_restrict_value (&blue, 0.0, 1.0);

    cr->status = _cairo_gstate_set_rgb_color (cr->gstate, red, green, blue);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_set_pattern (cairo_t *cr, cairo_pattern_t *pattern)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_pattern (cr->gstate, pattern);
    CAIRO_CHECK_SANITY (cr);
}

cairo_pattern_t *
cairo_current_pattern (cairo_t *cr)
{
    CAIRO_CHECK_SANITY (cr);
    return _cairo_gstate_current_pattern (cr->gstate);
}

/**
 * cairo_set_tolerance:
 * @cr: a #cairo_t
 * @tolerance: the tolerance, in device units (typically pixels)
 * 
 * Sets the tolerance used when converting paths into trapezoids.
 * Curved segments of the path will be subdivided until the maximum
 * deviation between the original path and the polygonal approximation
 * is less than @tolerance. The default value is 0.1. A larger
 * value will give better performance, a smaller value, better
 * appearance. (Reducing the value from the default value of 0.1
 * is unlikely to improve appearance significantly.)
 **/
void
cairo_set_tolerance (cairo_t *cr, double tolerance)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    _cairo_restrict_value (&tolerance, CAIRO_TOLERANCE_MINIMUM, tolerance);

    cr->status = _cairo_gstate_set_tolerance (cr->gstate, tolerance);
    CAIRO_CHECK_SANITY (cr);
}

/**
 * cairo_set_alpha:
 * @cr: a #cairo_t
 * @alpha: the alpha value. 0 is transparent, 1 fully opaque.
 *  if the value is outside the range 0 to 1, it will be
 *  clamped to that range.
 *     
 * Sets an overall alpha value used for stroking and filling.  This
 * value is multiplied with any alpha value coming from a gradient or
 * image pattern.
 **/
void
cairo_set_alpha (cairo_t *cr, double alpha)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    _cairo_restrict_value (&alpha, 0.0, 1.0);

    cr->status = _cairo_gstate_set_alpha (cr->gstate, alpha);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_set_fill_rule (cairo_t *cr, cairo_fill_rule_t fill_rule)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_fill_rule (cr->gstate, fill_rule);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_set_line_width (cairo_t *cr, double width)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    _cairo_restrict_value (&width, 0.0, width);

    cr->status = _cairo_gstate_set_line_width (cr->gstate, width);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_set_line_cap (cairo_t *cr, cairo_line_cap_t line_cap)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_line_cap (cr->gstate, line_cap);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_set_line_join (cairo_t *cr, cairo_line_join_t line_join)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_line_join (cr->gstate, line_join);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_set_dash (cairo_t *cr, double *dashes, int ndash, double offset)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_dash (cr->gstate, dashes, ndash, offset);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_set_miter_limit (cairo_t *cr, double limit)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_miter_limit (cr->gstate, limit);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_translate (cairo_t *cr, double tx, double ty)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_translate (cr->gstate, tx, ty);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_scale (cairo_t *cr, double sx, double sy)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_scale (cr->gstate, sx, sy);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_rotate (cairo_t *cr, double angle)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_rotate (cr->gstate, angle);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_concat_matrix (cairo_t *cr,
	       cairo_matrix_t *matrix)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_concat_matrix (cr->gstate, matrix);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_set_matrix (cairo_t *cr,
		  cairo_matrix_t *matrix)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_matrix (cr->gstate, matrix);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_default_matrix (cairo_t *cr)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_default_matrix (cr->gstate);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_identity_matrix (cairo_t *cr)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_identity_matrix (cr->gstate);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_transform_point (cairo_t *cr, double *x, double *y)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_transform_point (cr->gstate, x, y);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_transform_distance (cairo_t *cr, double *dx, double *dy)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_transform_distance (cr->gstate, dx, dy);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_inverse_transform_point (cairo_t *cr, double *x, double *y)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_inverse_transform_point (cr->gstate, x, y);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_inverse_transform_distance (cairo_t *cr, double *dx, double *dy)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_inverse_transform_distance (cr->gstate, dx, dy);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_new_path (cairo_t *cr)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_new_path (cr->gstate);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_move_to (cairo_t *cr, double x, double y)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_move_to (cr->gstate, x, y);
    CAIRO_CHECK_SANITY (cr);
}
slim_hidden_def(cairo_move_to);

void
cairo_line_to (cairo_t *cr, double x, double y)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_line_to (cr->gstate, x, y);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_curve_to (cairo_t *cr,
		double x1, double y1,
		double x2, double y2,
		double x3, double y3)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_curve_to (cr->gstate,
					 x1, y1,
					 x2, y2,
					 x3, y3);
    CAIRO_CHECK_SANITY (cr);
}

/**
 * cairo_arc:
 * @cr: a Cairo context
 * @xc: X position of the center of the arc
 * @yc: Y position of the center of the arc
 * @radius: the radius of the arc
 * @angle1: the start angle, in radians
 * @angle2: the end angle, in radians
 * 
 * Adds an arc from @angle1 to @angle2 to the current path. If there
 * is a current point, that point is connected to the start of the arc
 * by a straight line segment. Angles are measured in radians with an
 * angle of 0 along the X axis and an angle of %M_PI radians (90
 * degrees) along the Y axis, so with the default transformation
 * matrix, positive angles are clockwise. (To convert from degrees to
 * radians, use <literal>degrees * (M_PI / 180.)</literal>.)  This
 * function gives the arc in the direction of increasing angle; see
 * cairo_arc_negative() to get the arc in the direction of decreasing
 * angle.
 *
 * A full arc is drawn as a circle. To make an oval arc, you can scale
 * the current transformation matrix by different amounts in the X and
 * Y directions. For example, to draw a full oval in the box given
 * by @x, @y, @width, @height:
 
 * <informalexample><programlisting>
 * cairo_save (cr);
 * cairo_translate (x + width / 2., y + height / 2.);
 * cairo_scale (1. / (height / 2.), 1. / (width / 2.));
 * cairo_arc (cr, 0., 0., 1., 0., 2 * M_PI);
 * cairo_restore (cr);
 * </programlisting></informalexample>
 **/
void
cairo_arc (cairo_t *cr,
	   double xc, double yc,
	   double radius,
	   double angle1, double angle2)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_arc (cr->gstate,
				    xc, yc,
				    radius,
				    angle1, angle2);
    CAIRO_CHECK_SANITY (cr);
}

/**
 * cairo_arc_negative:
 * @cr: a Cairo context
 * @xc: X position of the center of the arc
 * @yc: Y position of the center of the arc
 * @radius: the radius of the arc
 * @angle1: the start angle, in radians
 * @angle2: the end angle, in radians
 * 
 * Adds an arc from @angle1 to @angle2 to the current path. The
 * function behaves identically to cairo_arc() except that instead of
 * giving the arc in the direction of increasing angle, it gives
 * the arc in the direction of decreasing angle.
 **/
void
cairo_arc_negative (cairo_t *cr,
		    double xc, double yc,
		    double radius,
		    double angle1, double angle2)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_arc_negative (cr->gstate,
					     xc, yc,
					     radius,
					     angle1, angle2);
    CAIRO_CHECK_SANITY (cr);
}

/* XXX: NYI
void
cairo_arc_to (cairo_t *cr,
	      double x1, double y1,
	      double x2, double y2,
	      double radius)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_arc_to (cr->gstate,
				       x1, y1,
				       x2, y2,
				       radius);
}
*/

void
cairo_rel_move_to (cairo_t *cr, double dx, double dy)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_rel_move_to (cr->gstate, dx, dy);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_rel_line_to (cairo_t *cr, double dx, double dy)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_rel_line_to (cr->gstate, dx, dy);
    CAIRO_CHECK_SANITY (cr);
}
slim_hidden_def(cairo_rel_line_to);

void
cairo_rel_curve_to (cairo_t *cr,
		    double dx1, double dy1,
		    double dx2, double dy2,
		    double dx3, double dy3)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_rel_curve_to (cr->gstate,
					     dx1, dy1,
					     dx2, dy2,
					     dx3, dy3);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_rectangle (cairo_t *cr,
		 double x, double y,
		 double width, double height)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cairo_move_to (cr, x, y);
    cairo_rel_line_to (cr, width, 0);
    cairo_rel_line_to (cr, 0, height);
    cairo_rel_line_to (cr, -width, 0);
    cairo_close_path (cr);
    CAIRO_CHECK_SANITY (cr);
}

/* XXX: NYI
void
cairo_stroke_path (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_stroke_path (cr->gstate);
}
*/

void
cairo_close_path (cairo_t *cr)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_close_path (cr->gstate);
    CAIRO_CHECK_SANITY (cr);
}
slim_hidden_def(cairo_close_path);

void
cairo_stroke (cairo_t *cr)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_stroke (cr->gstate);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_fill (cairo_t *cr)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_fill (cr->gstate);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_copy_page (cairo_t *cr)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_copy_page (cr->gstate);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_show_page (cairo_t *cr)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_show_page (cr->gstate);
    CAIRO_CHECK_SANITY (cr);
}

cairo_bool_t
cairo_in_stroke (cairo_t *cr, double x, double y)
{
    int inside;

    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return 0;

    cr->status = _cairo_gstate_in_stroke (cr->gstate, x, y, &inside);

    CAIRO_CHECK_SANITY (cr);

    if (cr->status)
	return 0;

    return inside;
}

int
cairo_in_fill (cairo_t *cr, double x, double y)
{
    int inside;

    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return 0;

    cr->status = _cairo_gstate_in_fill (cr->gstate, x, y, &inside);

    CAIRO_CHECK_SANITY (cr);

    if (cr->status)
	return 0;

    return inside;
}

void
cairo_stroke_extents (cairo_t *cr,
                      double *x1, double *y1, double *x2, double *y2)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;
    
    cr->status = _cairo_gstate_stroke_extents (cr->gstate, x1, y1, x2, y2);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_fill_extents (cairo_t *cr,
                    double *x1, double *y1, double *x2, double *y2)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;
    
    cr->status = _cairo_gstate_fill_extents (cr->gstate, x1, y1, x2, y2);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_init_clip (cairo_t *cr)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_init_clip (cr->gstate);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_clip (cairo_t *cr)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_clip (cr->gstate);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_select_font (cairo_t              *cr, 
		   const char           *family, 
		   cairo_font_slant_t   slant, 
		   cairo_font_weight_t  weight)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_select_font (cr->gstate, family, slant, weight);
    CAIRO_CHECK_SANITY (cr);
}

/**
 * cairo_current_font:
 * @cr: a #cairo_t
 * 
 * Gets the current font object for a #cairo_t. If there is no current
 * font object, because the font parameters, transform, or target
 * surface has been changed since a font was last used, a font object
 * will be created and stored in in the #cairo_t.
 *
 * Return value: the current font object. Can return %NULL
 *   on out-of-memory or if the context is already in
 *   an error state. This object is owned by Cairo. To keep
 *   a reference to it, you must call cairo_font_reference().
 **/
cairo_font_t *
cairo_current_font (cairo_t *cr)
{
    cairo_font_t *ret;

    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return NULL;

    cr->status = _cairo_gstate_current_font (cr->gstate, &ret);  
    CAIRO_CHECK_SANITY (cr);
    return ret;
}

void
cairo_current_font_extents (cairo_t *cr, 
			    cairo_font_extents_t *extents)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_current_font_extents (cr->gstate, extents);
    CAIRO_CHECK_SANITY (cr);
}


/**
 * cairo_set_font:
 * @cr: a #cairo_t
 * @font: a #cairo_font_t, or %NULL to unset any previously set font.
 * 
 * Replaces the current #cairo_font_t object in the #cairo_t with
 * @font. The replaced font in the #cairo_t will be destroyed if there
 * are no other references to it. Since a #cairo_font_t is specific to
 * a particular output device and size, changing the transformation,
 * font transformation, or target surfaces of a #cairo_t will clear
 * any previously set font. Setting the font using cairo_set_font() is
 * exclusive with the simple font selection API provided by
 * cairo_select_font(). The size and transformation set by
 * cairo_scale_font() and cairo_transform_font() are ignored unless
 * they were taken into account when creating @font.
 **/
void
cairo_set_font (cairo_t *cr, cairo_font_t *font)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_font (cr->gstate, font);  
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_scale_font (cairo_t *cr, double scale)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_scale_font (cr->gstate, scale);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_transform_font (cairo_t *cr, cairo_matrix_t *matrix)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_transform_font (cr->gstate, matrix);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_text_extents (cairo_t                *cr,
		    const unsigned char    *utf8,
		    cairo_text_extents_t   *extents)
{
    cairo_glyph_t *glyphs = NULL;
    int nglyphs;

    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    if (utf8 == NULL) {
	extents->x_bearing = 0.0;
	extents->y_bearing = 0.0;
	extents->width = 0.0;
	extents->height = 0.0;
	extents->x_advance = 0.0;
	extents->y_advance = 0.0;
	return;
    }

    cr->status = _cairo_gstate_text_to_glyphs (cr->gstate, utf8, &glyphs, &nglyphs);
    CAIRO_CHECK_SANITY (cr);

    if (cr->status) {
	if (glyphs)
	    free (glyphs);
	return;
    }
	
    cr->status = _cairo_gstate_glyph_extents (cr->gstate, glyphs, nglyphs, extents);
    CAIRO_CHECK_SANITY (cr);
    
    if (glyphs)
	free (glyphs);
}

void
cairo_glyph_extents (cairo_t                *cr,
		     cairo_glyph_t          *glyphs, 
		     int                    num_glyphs,
		     cairo_text_extents_t   *extents)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_glyph_extents (cr->gstate, glyphs, num_glyphs,
					      extents);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_show_text (cairo_t *cr, const unsigned char *utf8)
{
    cairo_glyph_t *glyphs = NULL;
    int nglyphs;

    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    if (utf8 == NULL)
	return;

    cr->status = _cairo_gstate_text_to_glyphs (cr->gstate, utf8, &glyphs, &nglyphs);
    CAIRO_CHECK_SANITY (cr);

    if (cr->status) {
	if (glyphs)
	    free (glyphs);
	return;
    }

    cr->status = _cairo_gstate_show_glyphs (cr->gstate, glyphs, nglyphs);
    CAIRO_CHECK_SANITY (cr);

    if (glyphs)
	free (glyphs);
}

void
cairo_show_glyphs (cairo_t *cr, cairo_glyph_t *glyphs, int num_glyphs)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_show_glyphs (cr->gstate, glyphs, num_glyphs);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_text_path  (cairo_t *cr, const unsigned char *utf8)
{
    cairo_glyph_t *glyphs = NULL;
    int nglyphs;

    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_text_to_glyphs (cr->gstate, utf8, &glyphs, &nglyphs);
    CAIRO_CHECK_SANITY (cr);

    if (cr->status) {
	if (glyphs)
	    free (glyphs);
	return;
    }

    cr->status = _cairo_gstate_glyph_path (cr->gstate, glyphs, nglyphs);
    CAIRO_CHECK_SANITY (cr);

    if (glyphs)
	free (glyphs);
}

void
cairo_glyph_path (cairo_t *cr, cairo_glyph_t *glyphs, int num_glyphs)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_glyph_path (cr->gstate, glyphs, num_glyphs);  
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_show_surface (cairo_t		*cr,
		    cairo_surface_t	*surface,
		    int			width,
		    int			height)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_show_surface (cr->gstate,
					     surface, width, height);
    CAIRO_CHECK_SANITY (cr);
}

cairo_operator_t
cairo_current_operator (cairo_t *cr)
{
    CAIRO_CHECK_SANITY (cr);
    return _cairo_gstate_current_operator (cr->gstate);
}
DEPRECATE (cairo_get_operator, cairo_current_operator);

void
cairo_current_rgb_color (cairo_t *cr, double *red, double *green, double *blue)
{
    CAIRO_CHECK_SANITY (cr);
    _cairo_gstate_current_rgb_color (cr->gstate, red, green, blue);
    CAIRO_CHECK_SANITY (cr);
}
DEPRECATE (cairo_get_rgb_color, cairo_current_rgb_color);

double
cairo_current_alpha (cairo_t *cr)
{
    CAIRO_CHECK_SANITY (cr);
    return _cairo_gstate_current_alpha (cr->gstate);
}
DEPRECATE (cairo_get_alpha, cairo_current_alpha);

double
cairo_current_tolerance (cairo_t *cr)
{
    CAIRO_CHECK_SANITY (cr);
    return _cairo_gstate_current_tolerance (cr->gstate);
}
DEPRECATE (cairo_get_tolerance, cairo_current_tolerance);

void
cairo_current_point (cairo_t *cr, double *x, double *y)
{
    CAIRO_CHECK_SANITY (cr);
    _cairo_gstate_current_point (cr->gstate, x, y);
    CAIRO_CHECK_SANITY (cr);
}
DEPRECATE (cairo_get_current_point, cairo_current_point);

cairo_fill_rule_t
cairo_current_fill_rule (cairo_t *cr)
{
    CAIRO_CHECK_SANITY (cr);
    return _cairo_gstate_current_fill_rule (cr->gstate);
}
DEPRECATE (cairo_get_fill_rule, cairo_current_fill_rule);

double
cairo_current_line_width (cairo_t *cr)
{
    CAIRO_CHECK_SANITY (cr);
    return _cairo_gstate_current_line_width (cr->gstate);
}
DEPRECATE (cairo_get_line_width, cairo_current_line_width);

cairo_line_cap_t
cairo_current_line_cap (cairo_t *cr)
{
    CAIRO_CHECK_SANITY (cr);
    return _cairo_gstate_current_line_cap (cr->gstate);
}
DEPRECATE (cairo_get_line_cap, cairo_current_line_cap);

cairo_line_join_t
cairo_current_line_join (cairo_t *cr)
{
    CAIRO_CHECK_SANITY (cr);
    return _cairo_gstate_current_line_join (cr->gstate);
}
DEPRECATE (cairo_get_line_join, cairo_current_line_join);

double
cairo_current_miter_limit (cairo_t *cr)
{
    CAIRO_CHECK_SANITY (cr);
    return _cairo_gstate_current_miter_limit (cr->gstate);
}
DEPRECATE (cairo_get_miter_limit, cairo_current_miter_limit);

void
cairo_current_matrix (cairo_t *cr, cairo_matrix_t *matrix)
{
    CAIRO_CHECK_SANITY (cr);
    _cairo_gstate_current_matrix (cr->gstate, matrix);
    CAIRO_CHECK_SANITY (cr);
}
DEPRECATE (cairo_get_matrix, cairo_current_matrix);

cairo_surface_t *
cairo_current_target_surface (cairo_t *cr)
{
    CAIRO_CHECK_SANITY (cr);
    return _cairo_gstate_current_target_surface (cr->gstate);
}
DEPRECATE (cairo_get_target_surface, cairo_current_target_surface);

void
cairo_current_path (cairo_t			*cr,
		    cairo_move_to_func_t	*move_to,
		    cairo_line_to_func_t	*line_to,
		    cairo_curve_to_func_t	*curve_to,
		    cairo_close_path_func_t	*close_path,
		    void			*closure)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;
	
    cr->status = _cairo_gstate_interpret_path (cr->gstate,
					       move_to,
					       line_to,
					       curve_to,
					       close_path,
					       closure);
    CAIRO_CHECK_SANITY (cr);
}

void
cairo_current_path_flat (cairo_t			*cr,
			 cairo_move_to_func_t		*move_to,
			 cairo_line_to_func_t		*line_to,
			 cairo_close_path_func_t	*close_path,
			 void				*closure)
{
    CAIRO_CHECK_SANITY (cr);
    if (cr->status)
	return;

    cr->status = _cairo_gstate_interpret_path (cr->gstate,
					       move_to,
					       line_to,
					       NULL,
					       close_path,
					       closure);
    CAIRO_CHECK_SANITY (cr);
}

cairo_status_t
cairo_status (cairo_t *cr)
{
    CAIRO_CHECK_SANITY (cr);
    return cr->status;
}
DEPRECATE (cairo_get_status, cairo_status);

const char *
cairo_status_string (cairo_t *cr)
{
    switch (cr->status) {
    case CAIRO_STATUS_SUCCESS:
	return "success";
    case CAIRO_STATUS_NO_MEMORY:
	return "out of memory";
    case CAIRO_STATUS_INVALID_RESTORE:
	return "cairo_restore without matching cairo_save";
    case CAIRO_STATUS_INVALID_POP_GROUP:
	return "cairo_pop_group without matching cairo_push_group";
    case CAIRO_STATUS_NO_CURRENT_POINT:
	return "no current point defined";
    case CAIRO_STATUS_INVALID_MATRIX:
	return "invalid matrix (not invertible)";
    case CAIRO_STATUS_NO_TARGET_SURFACE:
	return "no target surface has been set";
    case CAIRO_STATUS_NULL_POINTER:
	return "NULL pointer";
    case CAIRO_STATUS_INVALID_STRING:
	return "input string not valid UTF-8";
    }

    return "<unknown error status>";
}
DEPRECATE (cairo_get_status_string, cairo_status_string);

void
_cairo_restrict_value (double *value, double min, double max)
{
    if (*value < min)
	*value = min;
    else if (*value > max)
	*value = max;
}
