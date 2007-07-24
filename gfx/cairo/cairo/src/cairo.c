/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
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
#include "cairo-private.h"

#include "cairo-arc-private.h"
#include "cairo-path-private.h"

#define CAIRO_TOLERANCE_MINIMUM	0.0002 /* We're limited by 16 bits of sub-pixel precision */

static const cairo_t cairo_nil = {
  CAIRO_REF_COUNT_INVALID,	/* ref_count */
  CAIRO_STATUS_NO_MEMORY,	/* status */
  { 0, 0, 0, NULL },		/* user_data */
  NULL,				/* gstate */
  {{				/* gstate_tail */
    0
  }},
  {{ 				/* path */
    { 0, 0 },			   /* last_move_point */
    { 0, 0 },			   /* current point */
    FALSE,			   /* has_current_point */
    FALSE,			   /* has_curve_to */
    NULL, {{0}}			   /* buf_tail, buf_head */
  }}
};

#include <assert.h>

/* This has to be updated whenever cairo_status_t is extended.  That's
 * a bit of a pain, but it should be easy to always catch as long as
 * one adds a new test case to test a trigger of the new status value.
 */
#define CAIRO_STATUS_LAST_STATUS CAIRO_STATUS_INVALID_INDEX

/**
 * _cairo_error:
 * @status: a status value indicating an error, (eg. not
 * CAIRO_STATUS_SUCCESS)
 *
 * Checks that status is an error status, but does nothing else.
 *
 * All assignments of an error status to any user-visible object
 * within the cairo application should result in a call to
 * _cairo_error().
 *
 * The purpose of this function is to allow the user to set a
 * breakpoint in _cairo_error() to generate a stack trace for when the
 * user causes cairo to detect an error.
 **/
void
_cairo_error (cairo_status_t status)
{
    assert (status > CAIRO_STATUS_SUCCESS &&
	    status <= CAIRO_STATUS_LAST_STATUS);
}

/**
 * _cairo_set_error:
 * @cr: a cairo context
 * @status: a status value indicating an error, (eg. not
 * CAIRO_STATUS_SUCCESS)
 *
 * Sets cr->status to @status and calls _cairo_error;
 *
 * All assignments of an error status to cr->status should happen
 * through _cairo_set_error() or else _cairo_error() should be
 * called immediately after the assignment.
 *
 * The purpose of this function is to allow the user to set a
 * breakpoint in _cairo_error() to generate a stack trace for when the
 * user causes cairo to detect an error.
 **/
static void
_cairo_set_error (cairo_t *cr, cairo_status_t status)
{
    /* Don't overwrite an existing error. This preserves the first
     * error, which is the most significant. It also avoids attempting
     * to write to read-only data (eg. from a nil cairo_t). */
    if (cr->status == CAIRO_STATUS_SUCCESS)
	cr->status = status;

    _cairo_error (status);
}

/**
 * cairo_version:
 *
 * Returns the version of the cairo library encoded in a single
 * integer as per CAIRO_VERSION_ENCODE. The encoding ensures that
 * later versions compare greater than earlier versions.
 *
 * A run-time comparison to check that cairo's version is greater than
 * or equal to version X.Y.Z could be performed as follows:
 *
 * <informalexample><programlisting>
 * if (cairo_version() >= CAIRO_VERSION_ENCODE(X,Y,Z)) {...}
 * </programlisting></informalexample>
 *
 * See also cairo_version_string() as well as the compile-time
 * equivalents %CAIRO_VERSION and %CAIRO_VERSION_STRING.
 *
 * Return value: the encoded version.
 **/
int
cairo_version (void)
{
    return CAIRO_VERSION;
}

/**
 * cairo_version_string:
 *
 * Returns the version of the cairo library as a human-readable string
 * of the form "X.Y.Z".
 *
 * See also cairo_version() as well as the compile-time equivalents
 * %CAIRO_VERSION_STRING and %CAIRO_VERSION.
 *
 * Return value: a string containing the version.
 **/
const char*
cairo_version_string (void)
{
    return CAIRO_VERSION_STRING;
}
slim_hidden_def (cairo_version_string);

/**
 * cairo_create:
 * @target: target surface for the context
 *
 * Creates a new #cairo_t with all graphics state parameters set to
 * default values and with @target as a target surface. The target
 * surface should be constructed with a backend-specific function such
 * as cairo_image_surface_create() (or any other
 * <literal>cairo_&lt;backend&gt;_surface_create</literal> variant).
 *
 * This function references @target, so you can immediately
 * call cairo_surface_destroy() on it if you don't need to
 * maintain a separate reference to it.
 *
 * Return value: a newly allocated #cairo_t with a reference
 *  count of 1. The initial reference count should be released
 *  with cairo_destroy() when you are done using the #cairo_t.
 *  This function never returns %NULL. If memory cannot be
 *  allocated, a special #cairo_t object will be returned on
 *  which cairo_status() returns %CAIRO_STATUS_NO_MEMORY.
 *  You can use this object normally, but no drawing will
 *  be done.
 **/
cairo_t *
cairo_create (cairo_surface_t *target)
{
    cairo_t *cr;

    cr = malloc (sizeof (cairo_t));
    if (cr == NULL)
	return (cairo_t *) &cairo_nil;

    cr->ref_count = 1;

    cr->status = CAIRO_STATUS_SUCCESS;

    _cairo_user_data_array_init (&cr->user_data);

    cr->gstate = cr->gstate_tail;
    _cairo_gstate_init (cr->gstate, target);

    _cairo_path_fixed_init (cr->path);

    if (target == NULL) {
	_cairo_set_error (cr, CAIRO_STATUS_NULL_POINTER);
    }

    return cr;
}
slim_hidden_def (cairo_create);

/**
 * cairo_reference:
 * @cr: a #cairo_t
 *
 * Increases the reference count on @cr by one. This prevents
 * @cr from being destroyed until a matching call to cairo_destroy()
 * is made.
 *
 * The number of references to a #cairo_t can be get using
 * cairo_get_reference_count().
 *
 * Return value: the referenced #cairo_t.
 **/
cairo_t *
cairo_reference (cairo_t *cr)
{
    if (cr == NULL || cr->ref_count == CAIRO_REF_COUNT_INVALID)
	return cr;

    assert (cr->ref_count > 0);

    cr->ref_count++;

    return cr;
}

/**
 * cairo_destroy:
 * @cr: a #cairo_t
 *
 * Decreases the reference count on @cr by one. If the result
 * is zero, then @cr and all associated resources are freed.
 * See cairo_reference().
 **/
void
cairo_destroy (cairo_t *cr)
{
    if (cr == NULL || cr->ref_count == CAIRO_REF_COUNT_INVALID)
	return;

    assert (cr->ref_count > 0);

    cr->ref_count--;
    if (cr->ref_count)
	return;

    while (cr->gstate != cr->gstate_tail) {
	cairo_gstate_t *tmp = cr->gstate;
	cr->gstate = tmp->next;

	_cairo_gstate_destroy (tmp);
    }

    _cairo_gstate_fini (cr->gstate);

    _cairo_path_fixed_fini (cr->path);

    _cairo_user_data_array_fini (&cr->user_data);

    free (cr);
}
slim_hidden_def (cairo_destroy);

/**
 * cairo_get_user_data:
 * @cr: a #cairo_t
 * @key: the address of the #cairo_user_data_key_t the user data was
 * attached to
 *
 * Return user data previously attached to @cr using the specified
 * key.  If no user data has been attached with the given key this
 * function returns %NULL.
 *
 * Return value: the user data previously attached or %NULL.
 *
 * Since: 1.4
 **/
void *
cairo_get_user_data (cairo_t			 *cr,
		     const cairo_user_data_key_t *key)
{
    return _cairo_user_data_array_get_data (&cr->user_data,
					    key);
}

/**
 * cairo_set_user_data:
 * @cr: a #cairo_t
 * @key: the address of a #cairo_user_data_key_t to attach the user data to
 * @user_data: the user data to attach to the #cairo_t
 * @destroy: a #cairo_destroy_func_t which will be called when the
 * #cairo_t is destroyed or when new user data is attached using the
 * same key.
 *
 * Attach user data to @cr.  To remove user data from a surface,
 * call this function with the key that was used to set it and %NULL
 * for @data.
 *
 * Return value: %CAIRO_STATUS_SUCCESS or %CAIRO_STATUS_NO_MEMORY if a
 * slot could not be allocated for the user data.
 *
 * Since: 1.4
 **/
cairo_status_t
cairo_set_user_data (cairo_t			 *cr,
		     const cairo_user_data_key_t *key,
		     void			 *user_data,
		     cairo_destroy_func_t	 destroy)
{
    if (cr->ref_count == CAIRO_REF_COUNT_INVALID)
	return CAIRO_STATUS_NO_MEMORY;

    return _cairo_user_data_array_set_data (&cr->user_data,
					    key, user_data, destroy);
}

/**
 * cairo_get_reference_count:
 * @cr: a #cairo_t
 *
 * Returns the current reference count of @cr.
 *
 * Return value: the current reference count of @cr.  If the
 * object is a nil object, 0 will be returned.
 *
 * Since: 1.4
 **/
unsigned int
cairo_get_reference_count (cairo_t *cr)
{
    if (cr == NULL || cr->ref_count == CAIRO_REF_COUNT_INVALID)
	return 0;

    return cr->ref_count;
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

    if (cr->status)
	return;

    top = _cairo_gstate_clone (cr->gstate);

    if (top == NULL) {
	_cairo_set_error (cr, CAIRO_STATUS_NO_MEMORY);
	return;
    }

    top->next = cr->gstate;
    cr->gstate = top;
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

    if (cr->status)
	return;

    if (cr->gstate == cr->gstate_tail) {
	_cairo_set_error (cr, CAIRO_STATUS_INVALID_RESTORE);
	return;
    }

    top = cr->gstate;
    cr->gstate = top->next;

    _cairo_gstate_destroy (top);
}
slim_hidden_def(cairo_restore);

/**
 * cairo_push_group:
 * @cr: a cairo context
 *
 * Temporarily redirects drawing to an intermediate surface known as a
 * group. The redirection lasts until the group is completed by a call
 * to cairo_pop_group() or cairo_pop_group_to_source(). These calls
 * provide the result of any drawing to the group as a pattern,
 * (either as an explicit object, or set as the source pattern).
 *
 * This group functionality can be convenient for performing
 * intermediate compositing. One common use of a group is to render
 * objects as opaque within the group, (so that they occlude each
 * other), and then blend the result with translucence onto the
 * destination.
 *
 * Groups can be nested arbitrarily deep by making balanced calls to
 * cairo_push_group()/cairo_pop_group(). Each call pushes/pops the new
 * target group onto/from a stack.
 *
 * The cairo_push_group() function calls cairo_save() so that any
 * changes to the graphics state will not be visible outside the
 * group, (the pop_group functions call cairo_restore()).
 *
 * By default the intermediate group will have a content type of
 * CAIRO_CONTENT_COLOR_ALPHA. Other content types can be chosen for
 * the group by using cairo_push_group_with_content() instead.
 *
 * As an example, here is how one might fill and stroke a path with
 * translucence, but without any portion of the fill being visible
 * under the stroke:
 *
 * <informalexample><programlisting>
 * cairo_push_group (cr);
 * cairo_set_source (cr, fill_pattern);
 * cairo_fill_preserve (cr);
 * cairo_set_source (cr, stroke_pattern);
 * cairo_stroke (cr);
 * cairo_pop_group_to_source (cr);
 * cairo_paint_with_alpha (cr, alpha);
 * </programlisting></informalexample>
 *
 * Since: 1.2
 */
void
cairo_push_group (cairo_t *cr)
{
    cairo_push_group_with_content (cr, CAIRO_CONTENT_COLOR_ALPHA);
}
slim_hidden_def(cairo_push_group);

/**
 * cairo_push_group_with_content:
 * @cr: a cairo context
 * @content: a %cairo_content_t indicating the type of group that
 *           will be created
 *
 * Temporarily redirects drawing to an intermediate surface known as a
 * group. The redirection lasts until the group is completed by a call
 * to cairo_pop_group() or cairo_pop_group_to_source(). These calls
 * provide the result of any drawing to the group as a pattern,
 * (either as an explicit object, or set as the source pattern).
 *
 * The group will have a content type of @content. The ability to
 * control this content type is the only distinction between this
 * function and cairo_push_group() which you should see for a more
 * detailed description of group rendering.
 *
 * Since: 1.2
 */
void
cairo_push_group_with_content (cairo_t *cr, cairo_content_t content)
{
    cairo_status_t status;
    cairo_rectangle_int16_t extents;
    cairo_surface_t *group_surface = NULL;

    /* Get the extents that we'll use in creating our new group surface */
    _cairo_surface_get_extents (_cairo_gstate_get_target (cr->gstate), &extents);
    status = _cairo_clip_intersect_to_rectangle (_cairo_gstate_get_clip (cr->gstate), &extents);
    if (status != CAIRO_STATUS_SUCCESS)
	goto bail;

    group_surface = cairo_surface_create_similar (_cairo_gstate_get_target (cr->gstate),
						  content,
						  extents.width,
						  extents.height);
    status = cairo_surface_status (group_surface);
    if (status)
	goto bail;

    /* Set device offsets on the new surface so that logically it appears at
     * the same location on the parent surface -- when we pop_group this,
     * the source pattern will get fixed up for the appropriate target surface
     * device offsets, so we want to set our own surface offsets from /that/,
     * and not from the device origin. */
    cairo_surface_set_device_offset (group_surface,
                                     cr->gstate->target->device_transform.x0 - extents.x,
                                     cr->gstate->target->device_transform.y0 - extents.y);

    /* create a new gstate for the redirect */
    cairo_save (cr);
    if (cr->status)
	goto bail;

    _cairo_gstate_redirect_target (cr->gstate, group_surface);

bail:
    cairo_surface_destroy (group_surface);
    if (status)
	_cairo_set_error (cr, status);
}
slim_hidden_def(cairo_push_group_with_content);

/**
 * cairo_pop_group:
 * @cr: a cairo context
 *
 * Terminates the redirection begun by a call to cairo_push_group() or
 * cairo_push_group_with_content() and returns a new pattern
 * containing the results of all drawing operations performed to the
 * group.
 *
 * The cairo_pop_group() function calls cairo_restore(), (balancing a
 * call to cairo_save() by the push_group function), so that any
 * changes to the graphics state will not be visible outside the
 * group.
 *
 * Return value: a newly created (surface) pattern containing the
 * results of all drawing operations performed to the group. The
 * caller owns the returned object and should call
 * cairo_pattern_destroy() when finished with it.
 *
 * Since: 1.2
 **/
cairo_pattern_t *
cairo_pop_group (cairo_t *cr)
{
    cairo_surface_t *group_surface, *parent_target;
    cairo_pattern_t *group_pattern = NULL;
    cairo_matrix_t group_matrix;

    /* Grab the active surfaces */
    group_surface = _cairo_gstate_get_target (cr->gstate);
    parent_target = _cairo_gstate_get_parent_target (cr->gstate);

    /* Verify that we are at the right nesting level */
    if (parent_target == NULL) {
	_cairo_set_error (cr, CAIRO_STATUS_INVALID_POP_GROUP);
	return NULL;
    }

    /* We need to save group_surface before we restore; we don't need
     * to reference parent_target and original_target, since the
     * gstate will still hold refs to them once we restore. */
    cairo_surface_reference (group_surface);

    cairo_restore (cr);

    if (cr->status)
	goto done;

    group_pattern = cairo_pattern_create_for_surface (group_surface);
    if (!group_pattern) {
        cr->status = CAIRO_STATUS_NO_MEMORY;
        goto done;
    }

    _cairo_gstate_get_matrix (cr->gstate, &group_matrix);
    cairo_pattern_set_matrix (group_pattern, &group_matrix);
done:
    cairo_surface_destroy (group_surface);

    return group_pattern;
}
slim_hidden_def(cairo_pop_group);

/**
 * cairo_pop_group_to_source:
 * @cr: a cairo context
 *
 * Terminates the redirection begun by a call to cairo_push_group() or
 * cairo_push_group_with_content() and installs the resulting pattern
 * as the source pattern in the given cairo context.
 *
 * The behavior of this function is equivalent to the sequence of
 * operations:
 *
 * <informalexample><programlisting>
 * cairo_pattern_t *group = cairo_pop_group (cr);
 * cairo_set_source (cr, group);
 * cairo_pattern_destroy (group);
 * </programlisting></informalexample>
 *
 * but is more convenient as their is no need for a variable to store
 * the short-lived pointer to the pattern.
 *
 * The cairo_pop_group() function calls cairo_restore(), (balancing a
 * call to cairo_save() by the push_group function), so that any
 * changes to the graphics state will not be visible outside the
 * group.
 *
 * Since: 1.2
 **/
void
cairo_pop_group_to_source (cairo_t *cr)
{
    cairo_pattern_t *group_pattern;

    group_pattern = cairo_pop_group (cr);
    if (!group_pattern)
        return;

    cairo_set_source (cr, group_pattern);
    cairo_pattern_destroy (group_pattern);
}
slim_hidden_def(cairo_pop_group_to_source);

/**
 * cairo_set_operator:
 * @cr: a #cairo_t
 * @op: a compositing operator, specified as a #cairo_operator_t
 *
 * Sets the compositing operator to be used for all drawing
 * operations. See #cairo_operator_t for details on the semantics of
 * each available compositing operator.
 *
 * XXX: I'd also like to direct the reader's attention to some
 * (not-yet-written) section on cairo's imaging model. How would I do
 * that if such a section existed? (cworth).
 **/
void
cairo_set_operator (cairo_t *cr, cairo_operator_t op)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_operator (cr->gstate, op);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}
slim_hidden_def (cairo_set_operator);

/**
 * cairo_set_source_rgb
 * @cr: a cairo context
 * @red: red component of color
 * @green: green component of color
 * @blue: blue component of color
 *
 * Sets the source pattern within @cr to an opaque color. This opaque
 * color will then be used for any subsequent drawing operation until
 * a new source pattern is set.
 *
 * The color components are floating point numbers in the range 0 to
 * 1. If the values passed in are outside that range, they will be
 * clamped.
 **/
void
cairo_set_source_rgb (cairo_t *cr, double red, double green, double blue)
{
    cairo_pattern_t *pattern;

    if (cr->status)
	return;

    pattern = cairo_pattern_create_rgb (red, green, blue);
    cairo_set_source (cr, pattern);
    cairo_pattern_destroy (pattern);
}

/**
 * cairo_set_source_rgba:
 * @cr: a cairo context
 * @red: red component of color
 * @green: green component of color
 * @blue: blue component of color
 * @alpha: alpha component of color
 *
 * Sets the source pattern within @cr to a translucent color. This
 * color will then be used for any subsequent drawing operation until
 * a new source pattern is set.
 *
 * The color and alpha components are floating point numbers in the
 * range 0 to 1. If the values passed in are outside that range, they
 * will be clamped.
 **/
void
cairo_set_source_rgba (cairo_t *cr,
		       double red, double green, double blue,
		       double alpha)
{
    cairo_pattern_t *pattern;

    if (cr->status)
	return;

    pattern = cairo_pattern_create_rgba (red, green, blue, alpha);
    cairo_set_source (cr, pattern);
    cairo_pattern_destroy (pattern);
}

/**
 * cairo_set_source_surface:
 * @cr: a cairo context
 * @surface: a surface to be used to set the source pattern
 * @x: User-space X coordinate for surface origin
 * @y: User-space Y coordinate for surface origin
 *
 * This is a convenience function for creating a pattern from @surface
 * and setting it as the source in @cr with cairo_set_source().
 *
 * The @x and @y parameters give the user-space coordinate at which
 * the surface origin should appear. (The surface origin is its
 * upper-left corner before any transformation has been applied.) The
 * @x and @y patterns are negated and then set as translation values
 * in the pattern matrix.
 *
 * Other than the initial translation pattern matrix, as described
 * above, all other pattern attributes, (such as its extend mode), are
 * set to the default values as in cairo_pattern_create_for_surface().
 * The resulting pattern can be queried with cairo_get_source() so
 * that these attributes can be modified if desired, (eg. to create a
 * repeating pattern with cairo_pattern_set_extend()).
 **/
void
cairo_set_source_surface (cairo_t	  *cr,
			  cairo_surface_t *surface,
			  double	   x,
			  double	   y)
{
    cairo_pattern_t *pattern;
    cairo_matrix_t matrix;

    if (cr->status)
	return;

    pattern = cairo_pattern_create_for_surface (surface);

    cairo_matrix_init_translate (&matrix, -x, -y);
    cairo_pattern_set_matrix (pattern, &matrix);

    cairo_set_source (cr, pattern);
    cairo_pattern_destroy (pattern);
}
slim_hidden_def (cairo_set_source_surface);

/**
 * cairo_set_source
 * @cr: a cairo context
 * @source: a #cairo_pattern_t to be used as the source for
 * subsequent drawing operations.
 *
 * Sets the source pattern within @cr to @source. This pattern
 * will then be used for any subsequent drawing operation until a new
 * source pattern is set.
 *
 * Note: The pattern's transformation matrix will be locked to the
 * user space in effect at the time of cairo_set_source(). This means
 * that further modifications of the current transformation matrix
 * will not affect the source pattern. See cairo_pattern_set_matrix().
 *
 * XXX: I'd also like to direct the reader's attention to some
 * (not-yet-written) section on cairo's imaging model. How would I do
 * that if such a section existed? (cworth).
 **/
void
cairo_set_source (cairo_t *cr, cairo_pattern_t *source)
{
    if (cr->status)
	return;

    if (source == NULL) {
	_cairo_set_error (cr, CAIRO_STATUS_NULL_POINTER);
	return;
    }

    if (source->status) {
	_cairo_set_error (cr, source->status);
	return;
    }

    cr->status = _cairo_gstate_set_source (cr->gstate, source);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}
slim_hidden_def (cairo_set_source);

/**
 * cairo_get_source:
 * @cr: a cairo context
 *
 * Gets the current source pattern for @cr.
 *
 * Return value: the current source pattern. This object is owned by
 * cairo. To keep a reference to it, you must call
 * cairo_pattern_reference().
 **/
cairo_pattern_t *
cairo_get_source (cairo_t *cr)
{
    if (cr->status)
	return (cairo_pattern_t*) &cairo_pattern_nil.base;

    return _cairo_gstate_get_source (cr->gstate);
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
    if (cr->status)
	return;

    _cairo_restrict_value (&tolerance, CAIRO_TOLERANCE_MINIMUM, tolerance);

    cr->status = _cairo_gstate_set_tolerance (cr->gstate, tolerance);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_set_antialias:
 * @cr: a #cairo_t
 * @antialias: the new antialiasing mode
 *
 * Set the antialiasing mode of the rasterizer used for drawing shapes.
 * This value is a hint, and a particular backend may or may not support
 * a particular value.  At the current time, no backend supports
 * %CAIRO_ANTIALIAS_SUBPIXEL when drawing shapes.
 *
 * Note that this option does not affect text rendering, instead see
 * cairo_font_options_set_antialias().
 **/
void
cairo_set_antialias (cairo_t *cr, cairo_antialias_t antialias)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_antialias (cr->gstate, antialias);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_set_fill_rule:
 * @cr: a #cairo_t
 * @fill_rule: a fill rule, specified as a #cairo_fill_rule_t
 *
 * Set the current fill rule within the cairo context. The fill rule
 * is used to determine which regions are inside or outside a complex
 * (potentially self-intersecting) path. The current fill rule affects
 * both cairo_fill and cairo_clip. See #cairo_fill_rule_t for details
 * on the semantics of each available fill rule.
 **/
void
cairo_set_fill_rule (cairo_t *cr, cairo_fill_rule_t fill_rule)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_fill_rule (cr->gstate, fill_rule);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_set_line_width:
 * @cr: a #cairo_t
 * @width: a line width
 *
 * Sets the current line width within the cairo context. The line
 * width value specifies the diameter of a pen that is circular in
 * user space, (though device-space pen may be an ellipse in general
 * due to scaling/shear/rotation of the CTM).
 *
 * Note: When the description above refers to user space and CTM it
 * refers to the user space and CTM in effect at the time of the
 * stroking operation, not the user space and CTM in effect at the
 * time of the call to cairo_set_line_width(). The simplest usage
 * makes both of these spaces identical. That is, if there is no
 * change to the CTM between a call to cairo_set_line_with() and the
 * stroking operation, then one can just pass user-space values to
 * cairo_set_line_width() and ignore this note.
 *
 * As with the other stroke parameters, the current line width is
 * examined by cairo_stroke(), cairo_stroke_extents(), and
 * cairo_stroke_to_path(), but does not have any effect during path
 * construction.
 *
 * The default line width value is 2.0.
 **/
void
cairo_set_line_width (cairo_t *cr, double width)
{
    if (cr->status)
	return;

    _cairo_restrict_value (&width, 0.0, width);

    cr->status = _cairo_gstate_set_line_width (cr->gstate, width);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_set_line_cap:
 * @cr: a cairo context
 * @line_cap: a line cap style
 *
 * Sets the current line cap style within the cairo context. See
 * #cairo_line_cap_t for details about how the available line cap
 * styles are drawn.
 *
 * As with the other stroke parameters, the current line cap style is
 * examined by cairo_stroke(), cairo_stroke_extents(), and
 * cairo_stroke_to_path(), but does not have any effect during path
 * construction.
 **/
void
cairo_set_line_cap (cairo_t *cr, cairo_line_cap_t line_cap)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_line_cap (cr->gstate, line_cap);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_set_line_join:
 * @cr: a cairo context
 * @line_join: a line joint style
 *
 * Sets the current line join style within the cairo context. See
 * #cairo_line_join_t for details about how the available line join
 * styles are drawn.
 *
 * As with the other stroke parameters, the current line join style is
 * examined by cairo_stroke(), cairo_stroke_extents(), and
 * cairo_stroke_to_path(), but does not have any effect during path
 * construction.
 **/
void
cairo_set_line_join (cairo_t *cr, cairo_line_join_t line_join)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_line_join (cr->gstate, line_join);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_set_dash:
 * @cr: a cairo context
 * @dashes: an array specifying alternate lengths of on and off stroke portions
 * @num_dashes: the length of the dashes array
 * @offset: an offset into the dash pattern at which the stroke should start
 *
 * Sets the dash pattern to be used by cairo_stroke(). A dash pattern
 * is specified by @dashes, an array of positive values. Each value
 * provides the length of alternate "on" and "off" portions of the
 * stroke. The @offset specifies an offset into the pattern at which
 * the stroke begins.
 *
 * Each "on" segment will have caps applied as if the segment were a
 * separate sub-path. In particular, it is valid to use an "on" length
 * of 0.0 with CAIRO_LINE_CAP_ROUND or CAIRO_LINE_CAP_SQUARE in order
 * to distributed dots or squares along a path.
 *
 * Note: The length values are in user-space units as evaluated at the
 * time of stroking. This is not necessarily the same as the user
 * space at the time of cairo_set_dash().
 *
 * If @num_dashes is 0 dashing is disabled.
 *
 * If @num_dashes is 1 a symmetric pattern is assumed with alternating
 * on and off portions of the size specified by the single value in
 * @dashes.
 *
 * If any value in @dashes is negative, or if all values are 0, then
 * @cairo_t will be put into an error state with a status of
 * #CAIRO_STATUS_INVALID_DASH.
 **/
void
cairo_set_dash (cairo_t	     *cr,
		const double *dashes,
		int	      num_dashes,
		double	      offset)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_dash (cr->gstate,
					 dashes, num_dashes, offset);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_get_dash_count:
 * @cr: a #cairo_t
 *
 * This function returns the length of the dash array in @cr (0 if dashing
 * is not currently in effect).
 *
 * See also cairo_set_dash() and cairo_get_dash().
 *
 * Return value: the length of the dash array, or 0 if no dash array set.
 *
 * Since: 1.4
 */
int
cairo_get_dash_count (cairo_t *cr)
{
    return cr->gstate->stroke_style.num_dashes;
}

/**
 * cairo_get_dash:
 * @cr: a #cairo_t
 * @dashes: return value for the dash array, or %NULL
 * @offset: return value for the current dash offset, or %NULL
 *
 * Gets the current dash array.  If not %NULL, @dashes should be big
 * enough to hold at least the number of values returned by
 * cairo_get_dash_count().
 *
 * Since: 1.4
 **/
void
cairo_get_dash (cairo_t *cr,
		double  *dashes,
		double  *offset)
{
    if (dashes)
	memcpy (dashes,
		cr->gstate->stroke_style.dash,
		sizeof (double) * cr->gstate->stroke_style.num_dashes);

    if (offset)
	*offset = cr->gstate->stroke_style.dash_offset;
}

/**
 * cairo_set_miter_limit:
 * @cr: a cairo context
 * @limit: miter limit to set
 *
 * Sets the current miter limit within the cairo context.
 *
 * If the current line join style is set to %CAIRO_LINE_JOIN_MITER
 * (see cairo_set_line_join()), the miter limit is used to determine
 * whether the lines should be joined with a bevel instead of a miter.
 * Cairo divides the length of the miter by the line width.
 * If the result is greater than the miter limit, the style is
 * converted to a bevel.
 *
 * As with the other stroke parameters, the current line miter limit is
 * examined by cairo_stroke(), cairo_stroke_extents(), and
 * cairo_stroke_to_path(), but does not have any effect during path
 * construction.
 **/
void
cairo_set_miter_limit (cairo_t *cr, double limit)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_miter_limit (cr->gstate, limit);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_translate:
 * @cr: a cairo context
 * @tx: amount to translate in the X direction
 * @ty: amount to translate in the Y direction
 *
 * Modifies the current transformation matrix (CTM) by translating the
 * user-space origin by (@tx, @ty). This offset is interpreted as a
 * user-space coordinate according to the CTM in place before the new
 * call to cairo_translate. In other words, the translation of the
 * user-space origin takes place after any existing transformation.
 **/
void
cairo_translate (cairo_t *cr, double tx, double ty)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_translate (cr->gstate, tx, ty);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_scale:
 * @cr: a cairo context
 * @sx: scale factor for the X dimension
 * @sy: scale factor for the Y dimension
 *
 * Modifies the current transformation matrix (CTM) by scaling the X
 * and Y user-space axes by @sx and @sy respectively. The scaling of
 * the axes takes place after any existing transformation of user
 * space.
 **/
void
cairo_scale (cairo_t *cr, double sx, double sy)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_scale (cr->gstate, sx, sy);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}
slim_hidden_def (cairo_scale);

/**
 * cairo_rotate:
 * @cr: a cairo context
 * @angle: angle (in radians) by which the user-space axes will be
 * rotated
 *
 * Modifies the current transformation matrix (CTM) by rotating the
 * user-space axes by @angle radians. The rotation of the axes takes
 * places after any existing transformation of user space. The
 * rotation direction for positive angles is from the positive X axis
 * toward the positive Y axis.
 **/
void
cairo_rotate (cairo_t *cr, double angle)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_rotate (cr->gstate, angle);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_transform:
 * @cr: a cairo context
 * @matrix: a transformation to be applied to the user-space axes
 *
 * Modifies the current transformation matrix (CTM) by applying
 * @matrix as an additional transformation. The new transformation of
 * user space takes place after any existing transformation.
 **/
void
cairo_transform (cairo_t	      *cr,
		 const cairo_matrix_t *matrix)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_transform (cr->gstate, matrix);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_set_matrix:
 * @cr: a cairo context
 * @matrix: a transformation matrix from user space to device space
 *
 * Modifies the current transformation matrix (CTM) by setting it
 * equal to @matrix.
 **/
void
cairo_set_matrix (cairo_t	       *cr,
		  const cairo_matrix_t *matrix)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_matrix (cr->gstate, matrix);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_identity_matrix:
 * @cr: a cairo context
 *
 * Resets the current transformation matrix (CTM) by setting it equal
 * to the identity matrix. That is, the user-space and device-space
 * axes will be aligned and one user-space unit will transform to one
 * device-space unit.
 **/
void
cairo_identity_matrix (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_identity_matrix (cr->gstate);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_user_to_device:
 * @cr: a cairo context
 * @x: X value of coordinate (in/out parameter)
 * @y: Y value of coordinate (in/out parameter)
 *
 * Transform a coordinate from user space to device space by
 * multiplying the given point by the current transformation matrix
 * (CTM).
 **/
void
cairo_user_to_device (cairo_t *cr, double *x, double *y)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_user_to_device (cr->gstate, x, y);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_user_to_device_distance:
 * @cr: a cairo context
 * @dx: X component of a distance vector (in/out parameter)
 * @dy: Y component of a distance vector (in/out parameter)
 *
 * Transform a distance vector from user space to device space. This
 * function is similar to cairo_user_to_device() except that the
 * translation components of the CTM will be ignored when transforming
 * (@dx,@dy).
 **/
void
cairo_user_to_device_distance (cairo_t *cr, double *dx, double *dy)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_user_to_device_distance (cr->gstate, dx, dy);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_device_to_user:
 * @cr: a cairo
 * @x: X value of coordinate (in/out parameter)
 * @y: Y value of coordinate (in/out parameter)
 *
 * Transform a coordinate from device space to user space by
 * multiplying the given point by the inverse of the current
 * transformation matrix (CTM).
 **/
void
cairo_device_to_user (cairo_t *cr, double *x, double *y)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_device_to_user (cr->gstate, x, y);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_device_to_user_distance:
 * @cr: a cairo context
 * @dx: X component of a distance vector (in/out parameter)
 * @dy: Y component of a distance vector (in/out parameter)
 *
 * Transform a distance vector from device space to user space. This
 * function is similar to cairo_device_to_user() except that the
 * translation components of the inverse CTM will be ignored when
 * transforming (@dx,@dy).
 **/
void
cairo_device_to_user_distance (cairo_t *cr, double *dx, double *dy)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_device_to_user_distance (cr->gstate, dx, dy);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_new_path:
 * @cr: a cairo context
 *
 * Clears the current path. After this call there will be no path and
 * no current point.
 **/
void
cairo_new_path (cairo_t *cr)
{
    if (cr->status)
	return;

    _cairo_path_fixed_fini (cr->path);
}
slim_hidden_def(cairo_new_path);

/**
 * cairo_move_to:
 * @cr: a cairo context
 * @x: the X coordinate of the new position
 * @y: the Y coordinate of the new position
 *
 * Begin a new sub-path. After this call the current point will be (@x,
 * @y).
 **/
void
cairo_move_to (cairo_t *cr, double x, double y)
{
    cairo_fixed_t x_fixed, y_fixed;

    if (cr->status)
	return;

    _cairo_gstate_user_to_backend (cr->gstate, &x, &y);
    x_fixed = _cairo_fixed_from_double (x);
    y_fixed = _cairo_fixed_from_double (y);

    cr->status = _cairo_path_fixed_move_to (cr->path, x_fixed, y_fixed);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}
slim_hidden_def(cairo_move_to);

/**
 * cairo_new_sub_path:
 * @cr: a cairo context
 *
 * Begin a new sub-path. Note that the existing path is not
 * affected. After this call there will be no current point.
 *
 * In many cases, this call is not needed since new sub-paths are
 * frequently started with cairo_move_to().
 *
 * A call to cairo_new_sub_path() is particularly useful when
 * beginning a new sub-path with one of the cairo_arc() calls. This
 * makes things easier as it is no longer necessary to manually
 * compute the arc's initial coordinates for a call to
 * cairo_move_to().
 *
 * Since: 1.2
 **/
void
cairo_new_sub_path (cairo_t *cr)
{
    if (cr->status)
	return;

    _cairo_path_fixed_new_sub_path (cr->path);
}

/**
 * cairo_line_to:
 * @cr: a cairo context
 * @x: the X coordinate of the end of the new line
 * @y: the Y coordinate of the end of the new line
 *
 * Adds a line to the path from the current point to position (@x, @y)
 * in user-space coordinates. After this call the current point
 * will be (@x, @y).
 *
 * If there is no current point before the call to cairo_line_to()
 * this function will behave as cairo_move_to (@cr, @x, @y).
 **/
void
cairo_line_to (cairo_t *cr, double x, double y)
{
    cairo_fixed_t x_fixed, y_fixed;

    if (cr->status)
	return;

    _cairo_gstate_user_to_backend (cr->gstate, &x, &y);
    x_fixed = _cairo_fixed_from_double (x);
    y_fixed = _cairo_fixed_from_double (y);

    cr->status = _cairo_path_fixed_line_to (cr->path, x_fixed, y_fixed);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}
slim_hidden_def (cairo_line_to);

/**
 * cairo_curve_to:
 * @cr: a cairo context
 * @x1: the X coordinate of the first control point
 * @y1: the Y coordinate of the first control point
 * @x2: the X coordinate of the second control point
 * @y2: the Y coordinate of the second control point
 * @x3: the X coordinate of the end of the curve
 * @y3: the Y coordinate of the end of the curve
 *
 * Adds a cubic Bézier spline to the path from the current point to
 * position (@x3, @y3) in user-space coordinates, using (@x1, @y1) and
 * (@x2, @y2) as the control points. After this call the current point
 * will be (@x3, @y3).
 *
 * If there is no current point before the call to cairo_curve_to()
 * this function will behave as if preceded by a call to
 * cairo_move_to (@cr, @x1, @y1).
 **/
void
cairo_curve_to (cairo_t *cr,
		double x1, double y1,
		double x2, double y2,
		double x3, double y3)
{
    cairo_fixed_t x1_fixed, y1_fixed;
    cairo_fixed_t x2_fixed, y2_fixed;
    cairo_fixed_t x3_fixed, y3_fixed;

    if (cr->status)
	return;

    _cairo_gstate_user_to_backend (cr->gstate, &x1, &y1);
    _cairo_gstate_user_to_backend (cr->gstate, &x2, &y2);
    _cairo_gstate_user_to_backend (cr->gstate, &x3, &y3);

    x1_fixed = _cairo_fixed_from_double (x1);
    y1_fixed = _cairo_fixed_from_double (y1);

    x2_fixed = _cairo_fixed_from_double (x2);
    y2_fixed = _cairo_fixed_from_double (y2);

    x3_fixed = _cairo_fixed_from_double (x3);
    y3_fixed = _cairo_fixed_from_double (y3);

    cr->status = _cairo_path_fixed_curve_to (cr->path,
					     x1_fixed, y1_fixed,
					     x2_fixed, y2_fixed,
					     x3_fixed, y3_fixed);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}
slim_hidden_def (cairo_curve_to);

/**
 * cairo_arc:
 * @cr: a cairo context
 * @xc: X position of the center of the arc
 * @yc: Y position of the center of the arc
 * @radius: the radius of the arc
 * @angle1: the start angle, in radians
 * @angle2: the end angle, in radians
 *
 * Adds a circular arc of the given @radius to the current path.  The
 * arc is centered at (@xc, @yc), begins at @angle1 and proceeds in
 * the direction of increasing angles to end at @angle2. If @angle2 is
 * less than @angle1 it will be progressively increased by 2*M_PI
 * until it is greater than @angle1.
 *
 * If there is a current point, an initial line segment will be added
 * to the path to connect the current point to the beginning of the
 * arc.
 *
 * Angles are measured in radians. An angle of 0.0 is in the direction
 * of the positive X axis (in user space). An angle of %M_PI/2.0 radians
 * (90 degrees) is in the direction of the positive Y axis (in
 * user space). Angles increase in the direction from the positive X
 * axis toward the positive Y axis. So with the default transformation
 * matrix, angles increase in a clockwise direction.
 *
 * (To convert from degrees to radians, use <literal>degrees * (M_PI /
 * 180.)</literal>.)
 *
 * This function gives the arc in the direction of increasing angles;
 * see cairo_arc_negative() to get the arc in the direction of
 * decreasing angles.
 *
 * The arc is circular in user space. To achieve an elliptical arc,
 * you can scale the current transformation matrix by different
 * amounts in the X and Y directions. For example, to draw an ellipse
 * in the box given by @x, @y, @width, @height:
 *
 * <informalexample><programlisting>
 * cairo_save (cr);
 * cairo_translate (cr, x + width / 2., y + height / 2.);
 * cairo_scale (cr, 1. / (height / 2.), 1. / (width / 2.));
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
    if (cr->status)
	return;

    /* Do nothing, successfully, if radius is <= 0 */
    if (radius <= 0.0)
	return;

    while (angle2 < angle1)
	angle2 += 2 * M_PI;

    cairo_line_to (cr,
		   xc + radius * cos (angle1),
		   yc + radius * sin (angle1));

    _cairo_arc_path (cr, xc, yc, radius,
		     angle1, angle2);
}

/**
 * cairo_arc_negative:
 * @cr: a cairo context
 * @xc: X position of the center of the arc
 * @yc: Y position of the center of the arc
 * @radius: the radius of the arc
 * @angle1: the start angle, in radians
 * @angle2: the end angle, in radians
 *
 * Adds a circular arc of the given @radius to the current path.  The
 * arc is centered at (@xc, @yc), begins at @angle1 and proceeds in
 * the direction of decreasing angles to end at @angle2. If @angle2 is
 * greater than @angle1 it will be progressively decreased by 2*M_PI
 * until it is greater than @angle1.
 *
 * See cairo_arc() for more details. This function differs only in the
 * direction of the arc between the two angles.
 **/
void
cairo_arc_negative (cairo_t *cr,
		    double xc, double yc,
		    double radius,
		    double angle1, double angle2)
{
    if (cr->status)
	return;

    /* Do nothing, successfully, if radius is <= 0 */
    if (radius <= 0.0)
	return;

    while (angle2 > angle1)
	angle2 -= 2 * M_PI;

    cairo_line_to (cr,
		   xc + radius * cos (angle1),
		   yc + radius * sin (angle1));

     _cairo_arc_path_negative (cr, xc, yc, radius,
			       angle1, angle2);
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

/**
 * cairo_rel_move_to:
 * @cr: a cairo context
 * @dx: the X offset
 * @dy: the Y offset
 *
 * Begin a new sub-path. After this call the current point will offset
 * by (@x, @y).
 *
 * Given a current point of (x, y), cairo_rel_move_to(@cr, @dx, @dy)
 * is logically equivalent to cairo_move_to (@cr, x + @dx, y + @dy).
 *
 * It is an error to call this function with no current point. Doing
 * so will cause @cr to shutdown with a status of
 * CAIRO_STATUS_NO_CURRENT_POINT.
 **/
void
cairo_rel_move_to (cairo_t *cr, double dx, double dy)
{
    cairo_fixed_t dx_fixed, dy_fixed;

    if (cr->status)
	return;

    _cairo_gstate_user_to_device_distance (cr->gstate, &dx, &dy);
    dx_fixed = _cairo_fixed_from_double (dx);
    dy_fixed = _cairo_fixed_from_double (dy);

    cr->status = _cairo_path_fixed_rel_move_to (cr->path, dx_fixed, dy_fixed);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_rel_line_to:
 * @cr: a cairo context
 * @dx: the X offset to the end of the new line
 * @dy: the Y offset to the end of the new line
 *
 * Relative-coordinate version of cairo_line_to(). Adds a line to the
 * path from the current point to a point that is offset from the
 * current point by (@dx, @dy) in user space. After this call the
 * current point will be offset by (@dx, @dy).
 *
 * Given a current point of (x, y), cairo_rel_line_to(@cr, @dx, @dy)
 * is logically equivalent to cairo_line_to (@cr, x + @dx, y + @dy).
 *
 * It is an error to call this function with no current point. Doing
 * so will cause @cr to shutdown with a status of
 * CAIRO_STATUS_NO_CURRENT_POINT.
 **/
void
cairo_rel_line_to (cairo_t *cr, double dx, double dy)
{
    cairo_fixed_t dx_fixed, dy_fixed;

    if (cr->status)
	return;

    _cairo_gstate_user_to_device_distance (cr->gstate, &dx, &dy);
    dx_fixed = _cairo_fixed_from_double (dx);
    dy_fixed = _cairo_fixed_from_double (dy);

    cr->status = _cairo_path_fixed_rel_line_to (cr->path, dx_fixed, dy_fixed);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}
slim_hidden_def(cairo_rel_line_to);

/**
 * cairo_rel_curve_to:
 * @cr: a cairo context
 * @dx1: the X offset to the first control point
 * @dy1: the Y offset to the first control point
 * @dx2: the X offset to the second control point
 * @dy2: the Y offset to the second control point
 * @dx3: the X offset to the end of the curve
 * @dy3: the Y offset to the end of the curve
 *
 * Relative-coordinate version of cairo_curve_to(). All offsets are
 * relative to the current point. Adds a cubic Bézier spline to the
 * path from the current point to a point offset from the current
 * point by (@dx3, @dy3), using points offset by (@dx1, @dy1) and
 * (@dx2, @dy2) as the control points. After this call the current
 * point will be offset by (@dx3, @dy3).
 *
 * Given a current point of (x, y), cairo_rel_curve_to (@cr, @dx1,
 * @dy1, @dx2, @dy2, @dx3, @dy3) is logically equivalent to
 * cairo_curve_to (@cr, x + @dx1, y + @dy1, x + @dx2, y + @dy2, x +
 * @dx3, y + @dy3).
 *
 * It is an error to call this function with no current point. Doing
 * so will cause @cr to shutdown with a status of
 * CAIRO_STATUS_NO_CURRENT_POINT.
 **/
void
cairo_rel_curve_to (cairo_t *cr,
		    double dx1, double dy1,
		    double dx2, double dy2,
		    double dx3, double dy3)
{
    cairo_fixed_t dx1_fixed, dy1_fixed;
    cairo_fixed_t dx2_fixed, dy2_fixed;
    cairo_fixed_t dx3_fixed, dy3_fixed;

    if (cr->status)
	return;

    _cairo_gstate_user_to_device_distance (cr->gstate, &dx1, &dy1);
    _cairo_gstate_user_to_device_distance (cr->gstate, &dx2, &dy2);
    _cairo_gstate_user_to_device_distance (cr->gstate, &dx3, &dy3);

    dx1_fixed = _cairo_fixed_from_double (dx1);
    dy1_fixed = _cairo_fixed_from_double (dy1);

    dx2_fixed = _cairo_fixed_from_double (dx2);
    dy2_fixed = _cairo_fixed_from_double (dy2);

    dx3_fixed = _cairo_fixed_from_double (dx3);
    dy3_fixed = _cairo_fixed_from_double (dy3);

    cr->status = _cairo_path_fixed_rel_curve_to (cr->path,
						 dx1_fixed, dy1_fixed,
						 dx2_fixed, dy2_fixed,
						 dx3_fixed, dy3_fixed);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_rectangle:
 * @cr: a cairo context
 * @x: the X coordinate of the top left corner of the rectangle
 * @y: the Y coordinate to the top left corner of the rectangle
 * @width: the width of the rectangle
 * @height: the height of the rectangle
 *
 * Adds a closed sub-path rectangle of the given size to the current
 * path at position (@x, @y) in user-space coordinates.
 *
 * This function is logically equivalent to:
 * <informalexample><programlisting>
 * cairo_move_to (cr, x, y);
 * cairo_rel_line_to (cr, width, 0);
 * cairo_rel_line_to (cr, 0, height);
 * cairo_rel_line_to (cr, -width, 0);
 * cairo_close_path (cr);
 * </programlisting></informalexample>
 **/
void
cairo_rectangle (cairo_t *cr,
		 double x, double y,
		 double width, double height)
{
    if (cr->status)
	return;

    cairo_move_to (cr, x, y);
    cairo_rel_line_to (cr, width, 0);
    cairo_rel_line_to (cr, 0, height);
    cairo_rel_line_to (cr, -width, 0);
    cairo_close_path (cr);
}

/* XXX: NYI
void
cairo_stroke_to_path (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_stroke_path (cr->gstate);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}
*/

/**
 * cairo_close_path:
 * @cr: a cairo context
 *
 * Adds a line segment to the path from the current point to the
 * beginning of the current sub-path, (the most recent point passed to
 * cairo_move_to()), and closes this sub-path. After this call the
 * current point will be at the joined endpoint of the sub-path.
 *
 * The behavior of cairo_close_path() is distinct from simply calling
 * cairo_line_to() with the equivalent coordinate in the case of
 * stroking. When a closed sub-path is stroked, there are no caps on
 * the ends of the sub-path. Instead, there is a line join connecting
 * the final and initial segments of the sub-path.
 *
 * If there is no current point before the call to cairo_close_path,
 * this function will have no effect.
 *
 * Note: As of cairo version 1.2.4 any call to cairo_close_path will
 * place an explicit MOVE_TO element into the path immediately after
 * the CLOSE_PATH element, (which can be seen in cairo_copy_path() for
 * example). This can simplify path processing in some cases as it may
 * not be necessary to save the "last move_to point" during processing
 * as the MOVE_TO immediately after the CLOSE_PATH will provide that
 * point.
 **/
void
cairo_close_path (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_path_fixed_close_path (cr->path);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}
slim_hidden_def(cairo_close_path);

/**
 * cairo_paint:
 * @cr: a cairo context
 *
 * A drawing operator that paints the current source everywhere within
 * the current clip region.
 **/
void
cairo_paint (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_paint (cr->gstate);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}
slim_hidden_def (cairo_paint);

/**
 * cairo_paint_with_alpha:
 * @cr: a cairo context
 * @alpha: alpha value, between 0 (transparent) and 1 (opaque)
 *
 * A drawing operator that paints the current source everywhere within
 * the current clip region using a mask of constant alpha value
 * @alpha. The effect is similar to cairo_paint(), but the drawing
 * is faded out using the alpha value.
 **/
void
cairo_paint_with_alpha (cairo_t *cr,
			double   alpha)
{
    cairo_color_t color;
    cairo_pattern_union_t pattern;

    if (cr->status)
	return;

    if (CAIRO_ALPHA_IS_OPAQUE (alpha)) {
	cairo_paint (cr);
	return;
    }

    if (CAIRO_ALPHA_IS_ZERO (alpha)) {
	return;
    }

    _cairo_color_init_rgba (&color, 1., 1., 1., alpha);
    _cairo_pattern_init_solid (&pattern.solid, &color);

    cr->status = _cairo_gstate_mask (cr->gstate, &pattern.base);
    if (cr->status)
	_cairo_set_error (cr, cr->status);

    _cairo_pattern_fini (&pattern.base);
}

/**
 * cairo_mask:
 * @cr: a cairo context
 * @pattern: a #cairo_pattern_t
 *
 * A drawing operator that paints the current source
 * using the alpha channel of @pattern as a mask. (Opaque
 * areas of @pattern are painted with the source, transparent
 * areas are not painted.)
 */
void
cairo_mask (cairo_t         *cr,
	    cairo_pattern_t *pattern)
{
    if (cr->status)
	return;

    if (pattern == NULL) {
	_cairo_set_error (cr, CAIRO_STATUS_NULL_POINTER);
	return;
    }

    if (pattern->status) {
	_cairo_set_error (cr, pattern->status);
	return;
    }

    cr->status = _cairo_gstate_mask (cr->gstate, pattern);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}
slim_hidden_def (cairo_mask);

/**
 * cairo_mask_surface:
 * @cr: a cairo context
 * @surface: a #cairo_surface_t
 * @surface_x: X coordinate at which to place the origin of @surface
 * @surface_y: Y coordinate at which to place the origin of @surface
 *
 * A drawing operator that paints the current source
 * using the alpha channel of @surface as a mask. (Opaque
 * areas of @surface are painted with the source, transparent
 * areas are not painted.)
 */
void
cairo_mask_surface (cairo_t         *cr,
		    cairo_surface_t *surface,
		    double           surface_x,
		    double           surface_y)
{
    cairo_pattern_t *pattern;
    cairo_matrix_t matrix;

    if (cr->status)
	return;

    pattern = cairo_pattern_create_for_surface (surface);

    cairo_matrix_init_translate (&matrix, - surface_x, - surface_y);
    cairo_pattern_set_matrix (pattern, &matrix);

    cairo_mask (cr, pattern);

    cairo_pattern_destroy (pattern);
}

/**
 * cairo_stroke:
 * @cr: a cairo context
 *
 * A drawing operator that strokes the current path according to the
 * current line width, line join, line cap, and dash settings. After
 * cairo_stroke, the current path will be cleared from the cairo
 * context. See cairo_set_line_width(), cairo_set_line_join(),
 * cairo_set_line_cap(), cairo_set_dash(), and
 * cairo_stroke_preserve().
 *
 * Note: Degenerate segments and sub-paths are treated specially and
 * provide a useful result. These can result in two different
 * situations:
 *
 * 1. Zero-length "on" segments set in cairo_set_dash(). If the cap
 * style is CAIRO_LINE_CAP_ROUND or CAIRO_LINE_CAP_SQUARE then these
 * segments will be drawn as circular dots or squares respectively. In
 * the case of CAIRO_LINE_CAP_SQUARE, the orientation of the squares
 * is determined by the direction of the underlying path.
 *
 * 2. A sub-path created by cairo_move_to() followed by either a
 * cairo_close_path() or one or more calls to cairo_line_to() to the
 * same coordinate as the cairo_move_to(). If the cap style is
 * CAIRO_LINE_CAP_ROUND then these sub-paths will be drawn as circular
 * dots. Note that in the case of CAIRO_LINE_CAP_SQUARE a degenerate
 * sub-path will not be drawn at all, (since the correct orientation
 * is indeterminate).
 *
 * In no case will a cap style of CAIRO_LINE_CAP_BUTT cause anything
 * to be drawn in the case of either degenerate segments or sub-paths.
 **/
void
cairo_stroke (cairo_t *cr)
{
    cairo_stroke_preserve (cr);

    cairo_new_path (cr);
}

/**
 * cairo_stroke_preserve:
 * @cr: a cairo context
 *
 * A drawing operator that strokes the current path according to the
 * current line width, line join, line cap, and dash settings. Unlike
 * cairo_stroke(), cairo_stroke_preserve preserves the path within the
 * cairo context.
 *
 * See cairo_set_line_width(), cairo_set_line_join(),
 * cairo_set_line_cap(), cairo_set_dash(), and
 * cairo_stroke_preserve().
 **/
void
cairo_stroke_preserve (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_stroke (cr->gstate, cr->path);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}
slim_hidden_def(cairo_stroke_preserve);

/**
 * cairo_fill:
 * @cr: a cairo context
 *
 * A drawing operator that fills the current path according to the
 * current fill rule, (each sub-path is implicitly closed before being
 * filled). After cairo_fill, the current path will be cleared from
 * the cairo context. See cairo_set_fill_rule() and
 * cairo_fill_preserve().
 **/
void
cairo_fill (cairo_t *cr)
{
    cairo_fill_preserve (cr);

    cairo_new_path (cr);
}

/**
 * cairo_fill_preserve:
 * @cr: a cairo context
 *
 * A drawing operator that fills the current path according to the
 * current fill rule, (each sub-path is implicitly closed before being
 * filled). Unlike cairo_fill(), cairo_fill_preserve preserves the
 * path within the cairo context.
 *
 * See cairo_set_fill_rule() and cairo_fill().
 **/
void
cairo_fill_preserve (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_fill (cr->gstate, cr->path);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}
slim_hidden_def(cairo_fill_preserve);

/**
 * cairo_copy_page:
 * @cr: a cairo context
 *
 * Emits the current page for backends that support multiple pages, but
 * doesn't clear it, so, the contents of the current page will be retained
 * for the next page too.  Use cairo_show_page() if you want to get an
 * empty page after the emission.
 **/
void
cairo_copy_page (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_copy_page (cr->gstate);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_show_page:
 * @cr: a cairo context
 *
 * Emits and clears the current page for backends that support multiple
 * pages.  Use cairo_copy_page() if you don't want to clear the page.
 **/
void
cairo_show_page (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_show_page (cr->gstate);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_in_stroke:
 * @cr: a cairo context
 * @x: X coordinate of the point to test
 * @y: Y coordinate of the point to test
 *
 * Tests whether the given point is inside the area that would be
 * affected by a cairo_stroke() operation given the current path and
 * stroking parameters.
 *
 * See cairo_stroke(), cairo_set_line_width(), cairo_set_line_join(),
 * cairo_set_line_cap(), cairo_set_dash(), and
 * cairo_stroke_preserve().
 *
 * Return value: A non-zero value if the point is inside, or zero if
 * outside.
 **/
cairo_bool_t
cairo_in_stroke (cairo_t *cr, double x, double y)
{
    cairo_bool_t inside;

    if (cr->status)
	return 0;

    cr->status = _cairo_gstate_in_stroke (cr->gstate,
					  cr->path,
					  x, y, &inside);
    if (cr->status)
	return 0;

    return inside;
}

/**
 * cairo_in_fill:
 * @cr: a cairo context
 * @x: X coordinate of the point to test
 * @y: Y coordinate of the point to test
 *
 * Tests whether the given point is inside the area that would be
 * affected by a cairo_fill() operation given the current path and
 * filling parameters.
 *
 * See cairo_fill(), cairo_set_fill_rule() and cairo_fill_preserve().
 *
 * Return value: A non-zero value if the point is inside, or zero if
 * outside.
 **/
cairo_bool_t
cairo_in_fill (cairo_t *cr, double x, double y)
{
    cairo_bool_t inside;

    if (cr->status)
	return 0;

    cr->status = _cairo_gstate_in_fill (cr->gstate,
					cr->path,
					x, y, &inside);
    if (cr->status) {
	_cairo_set_error (cr, cr->status);
	return 0;
    }

    return inside;
}

/**
 * cairo_stroke_extents:
 * @cr: a cairo context
 * @x1: left of the resulting extents
 * @y1: top of the resulting extents
 * @x2: right of the resulting extents
 * @y2: bottom of the resulting extents
 *
 * Computes a bounding box in user coordinates covering the area that
 * would be affected by a cairo_stroke() operation operation given the
 * current path and stroke parameters. If the current path is empty,
 * returns an empty rectangle (0,0, 0,0). Surface dimensions and
 * clipping are not taken into account.
 *
 * See cairo_stroke(), cairo_set_line_width(), cairo_set_line_join(),
 * cairo_set_line_cap(), cairo_set_dash(), and
 * cairo_stroke_preserve().
 **/
void
cairo_stroke_extents (cairo_t *cr,
                      double *x1, double *y1, double *x2, double *y2)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_stroke_extents (cr->gstate,
					       cr->path,
					       x1, y1, x2, y2);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_fill_extents:
 * @cr: a cairo context
 * @x1: left of the resulting extents
 * @y1: top of the resulting extents
 * @x2: right of the resulting extents
 * @y2: bottom of the resulting extents
 *
 * Computes a bounding box in user coordinates covering the area that
 * would be affected by a cairo_fill() operation given the current path
 * and fill parameters. If the current path is empty, returns an empty
 * rectangle (0,0, 0,0). Surface dimensions and clipping are not taken
 * into account.
 *
 * See cairo_fill(), cairo_set_fill_rule() and cairo_fill_preserve().
 **/
void
cairo_fill_extents (cairo_t *cr,
                    double *x1, double *y1, double *x2, double *y2)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_fill_extents (cr->gstate,
					     cr->path,
					     x1, y1, x2, y2);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_clip:
 * @cr: a cairo context
 *
 * Establishes a new clip region by intersecting the current clip
 * region with the current path as it would be filled by cairo_fill()
 * and according to the current fill rule (see cairo_set_fill_rule()).
 *
 * After cairo_clip, the current path will be cleared from the cairo
 * context.
 *
 * The current clip region affects all drawing operations by
 * effectively masking out any changes to the surface that are outside
 * the current clip region.
 *
 * Calling cairo_clip() can only make the clip region smaller, never
 * larger. But the current clip is part of the graphics state, so a
 * temporary restriction of the clip region can be achieved by
 * calling cairo_clip() within a cairo_save()/cairo_restore()
 * pair. The only other means of increasing the size of the clip
 * region is cairo_reset_clip().
 **/
void
cairo_clip (cairo_t *cr)
{
    cairo_clip_preserve (cr);

    cairo_new_path (cr);
}

/**
 * cairo_clip_preserve:
 * @cr: a cairo context
 *
 * Establishes a new clip region by intersecting the current clip
 * region with the current path as it would be filled by cairo_fill()
 * and according to the current fill rule (see cairo_set_fill_rule()).
 *
 * Unlike cairo_clip(), cairo_clip_preserve preserves the path within
 * the cairo context.
 *
 * The current clip region affects all drawing operations by
 * effectively masking out any changes to the surface that are outside
 * the current clip region.
 *
 * Calling cairo_clip() can only make the clip region smaller, never
 * larger. But the current clip is part of the graphics state, so a
 * temporary restriction of the clip region can be achieved by
 * calling cairo_clip() within a cairo_save()/cairo_restore()
 * pair. The only other means of increasing the size of the clip
 * region is cairo_reset_clip().
 **/
void
cairo_clip_preserve (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_clip (cr->gstate, cr->path);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}
slim_hidden_def(cairo_clip_preserve);

/**
 * cairo_reset_clip:
 * @cr: a cairo context
 *
 * Reset the current clip region to its original, unrestricted
 * state. That is, set the clip region to an infinitely large shape
 * containing the target surface. Equivalently, if infinity is too
 * hard to grasp, one can imagine the clip region being reset to the
 * exact bounds of the target surface.
 *
 * Note that code meant to be reusable should not call
 * cairo_reset_clip() as it will cause results unexpected by
 * higher-level code which calls cairo_clip(). Consider using
 * cairo_save() and cairo_restore() around cairo_clip() as a more
 * robust means of temporarily restricting the clip region.
 **/
void
cairo_reset_clip (cairo_t *cr)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_reset_clip (cr->gstate);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_clip_extents:
 * @cr: a cairo context
 * @x1: left of the resulting extents
 * @y1: top of the resulting extents
 * @x2: right of the resulting extents
 * @y2: bottom of the resulting extents
 *
 * Computes a bounding box in user coordinates covering the area inside the
 * current clip.
 *
 * Since: 1.4
 **/
void
cairo_clip_extents (cairo_t *cr,
		    double *x1, double *y1,
		    double *x2, double *y2)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_clip_extents (cr->gstate, x1, y1, x2, y2);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

static cairo_rectangle_list_t *
_cairo_rectangle_list_create_in_error (cairo_status_t status)
{
    cairo_rectangle_list_t *list;

    list = malloc (sizeof (cairo_rectangle_list_t));
    if (list == NULL)
        return (cairo_rectangle_list_t*) &_cairo_rectangles_nil;
    list->status = status;
    list->rectangles = NULL;
    list->num_rectangles = 0;
    return list;
}

/**
 * cairo_copy_clip_rectangle_list:
 * @cr: a cairo context
 *
 * Gets the current clip region as a list of rectangles in user coordinates.
 * Never returns %NULL.
 *
 * The status in the list may be CAIRO_STATUS_CLIP_NOT_REPRESENTABLE to
 * indicate that the clip region cannot be represented as a list of
 * user-space rectangles. The status may have other values to indicate
 * other errors.
 *
 * The caller must always call cairo_rectangle_list_destroy on the result of
 * this function.
 *
 * Returns: the current clip region as a list of rectangles in user coordinates.
 *
 * Since: 1.4
 **/
cairo_rectangle_list_t *
cairo_copy_clip_rectangle_list (cairo_t *cr)
{
    if (cr->status)
        return _cairo_rectangle_list_create_in_error (cr->status);

    return _cairo_gstate_copy_clip_rectangle_list (cr->gstate);
}

/**
 * cairo_select_font_face:
 * @cr: a #cairo_t
 * @family: a font family name, encoded in UTF-8
 * @slant: the slant for the font
 * @weight: the weight for the font
 *
 * Selects a family and style of font from a simplified description as
 * a family name, slant and weight. This function is meant to be used
 * only for applications with simple font needs: Cairo doesn't provide
 * for operations such as listing all available fonts on the system,
 * and it is expected that most applications will need to use a more
 * comprehensive font handling and text layout library in addition to
 * cairo.
 **/
void
cairo_select_font_face (cairo_t              *cr,
			const char           *family,
			cairo_font_slant_t    slant,
			cairo_font_weight_t   weight)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_select_font_face (cr->gstate, family, slant, weight);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_font_extents:
 * @cr: a #cairo_t
 * @extents: a #cairo_font_extents_t object into which the results
 * will be stored.
 *
 * Gets the font extents for the currently selected font.
 **/
void
cairo_font_extents (cairo_t              *cr,
		    cairo_font_extents_t *extents)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_get_font_extents (cr->gstate, extents);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_set_font_face:
 * @cr: a #cairo_t
 * @font_face: a #cairo_font_face_t, or %NULL to restore to the default font
 *
 * Replaces the current #cairo_font_face_t object in the #cairo_t with
 * @font_face. The replaced font face in the #cairo_t will be
 * destroyed if there are no other references to it.
 **/
void
cairo_set_font_face (cairo_t           *cr,
		     cairo_font_face_t *font_face)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_font_face (cr->gstate, font_face);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_get_font_face:
 * @cr: a #cairo_t
 *
 * Gets the current font face for a #cairo_t.
 *
 * Return value: the current font face.  This object is owned by
 * cairo. To keep a reference to it, you must call
 * cairo_font_face_reference.
 *
 * This function never returns %NULL. If memory cannot be allocated, a
 * special "nil" #cairo_font_face_t object will be returned on which
 * cairo_font_face_status() returns %CAIRO_STATUS_NO_MEMORY. Using
 * this nil object will cause its error state to propagate to other
 * objects it is passed to, (for example, calling
 * cairo_set_font_face() with a nil font will trigger an error that
 * will shutdown the cairo_t object).
 **/
cairo_font_face_t *
cairo_get_font_face (cairo_t *cr)
{
    cairo_font_face_t *font_face;

    if (cr->status)
	return (cairo_font_face_t*) &_cairo_font_face_nil;

    cr->status = _cairo_gstate_get_font_face (cr->gstate, &font_face);
    if (cr->status) {
	_cairo_set_error (cr, cr->status);
	return (cairo_font_face_t*) &_cairo_font_face_nil;
    }

    return font_face;
}

/**
 * cairo_set_font_size:
 * @cr: a #cairo_t
 * @size: the new font size, in user space units
 *
 * Sets the current font matrix to a scale by a factor of @size, replacing
 * any font matrix previously set with cairo_set_font_size() or
 * cairo_set_font_matrix(). This results in a font size of @size user space
 * units. (More precisely, this matrix will result in the font's
 * em-square being a @size by @size square in user space.)
 **/
void
cairo_set_font_size (cairo_t *cr, double size)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_font_size (cr->gstate, size);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_set_font_matrix
 * @cr: a #cairo_t
 * @matrix: a #cairo_matrix_t describing a transform to be applied to
 * the current font.
 *
 * Sets the current font matrix to @matrix. The font matrix gives a
 * transformation from the design space of the font (in this space,
 * the em-square is 1 unit by 1 unit) to user space. Normally, a
 * simple scale is used (see cairo_set_font_size()), but a more
 * complex font matrix can be used to shear the font
 * or stretch it unequally along the two axes
 **/
void
cairo_set_font_matrix (cairo_t		    *cr,
		       const cairo_matrix_t *matrix)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_font_matrix (cr->gstate, matrix);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_get_font_matrix
 * @cr: a #cairo_t
 * @matrix: return value for the matrix
 *
 * Stores the current font matrix into @matrix. See
 * cairo_set_font_matrix().
 **/
void
cairo_get_font_matrix (cairo_t *cr, cairo_matrix_t *matrix)
{
    _cairo_gstate_get_font_matrix (cr->gstate, matrix);
}

/**
 * cairo_set_font_options:
 * @cr: a #cairo_t
 * @options: font options to use
 *
 * Sets a set of custom font rendering options for the #cairo_t.
 * Rendering options are derived by merging these options with the
 * options derived from underlying surface; if the value in @options
 * has a default value (like %CAIRO_ANTIALIAS_DEFAULT), then the value
 * from the surface is used.
 **/
void
cairo_set_font_options (cairo_t                    *cr,
			const cairo_font_options_t *options)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_set_font_options (cr->gstate, options);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_get_font_options:
 * @cr: a #cairo_t
 * @options: a #cairo_font_options_t object into which to store
 *   the retrieved options. All existing values are overwritten
 *
 * Retrieves font rendering options set via #cairo_set_font_options.
 * Note that the returned options do not include any options derived
 * from the underlying surface; they are literally the options
 * passed to cairo_set_font_options().
 **/
void
cairo_get_font_options (cairo_t              *cr,
			cairo_font_options_t *options)
{
    _cairo_gstate_get_font_options (cr->gstate, options);
}

/**
 * cairo_set_scaled_font:
 * @cr: a #cairo_t
 * @scaled_font: a #cairo_scaled_font_t
 *
 * Replaces the current font face, font matrix, and font options in
 * the #cairo_t with those of the #cairo_scaled_font_t.  Except for
 * some translation, the current CTM of the #cairo_t should be the
 * same as that of the #cairo_scaled_font_t, which can be accessed
 * using cairo_scaled_font_get_ctm().
 *
 * Since: 1.2
 **/
void
cairo_set_scaled_font (cairo_t                   *cr,
		       const cairo_scaled_font_t *scaled_font)
{
    if (cr->status)
	return;

    cr->status = scaled_font->status;
    if (cr->status)
        goto BAIL;

    cr->status = _cairo_gstate_set_font_face (cr->gstate, scaled_font->font_face);
    if (cr->status)
        goto BAIL;

    cr->status = _cairo_gstate_set_font_matrix (cr->gstate, &scaled_font->font_matrix);
    if (cr->status)
        goto BAIL;

    cr->status = _cairo_gstate_set_font_options (cr->gstate, &scaled_font->options);
    if (cr->status)
        goto BAIL;

    return;

BAIL:
    _cairo_set_error (cr, cr->status);
}

/**
 * cairo_get_scaled_font:
 * @cr: a #cairo_t
 *
 * Gets the current scaled font for a #cairo_t.
 *
 * Return value: the current scaled font. This object is owned by
 * cairo. To keep a reference to it, you must call
 * cairo_scaled_font_reference().
 *
 * This function never returns %NULL. If memory cannot be allocated, a
 * special "nil" #cairo_scaled_font_t object will be returned on which
 * cairo_scaled_font_status() returns %CAIRO_STATUS_NO_MEMORY. Using
 * this nil object will cause its error state to propagate to other
 * objects it is passed to, (for example, calling
 * cairo_set_scaled_font() with a nil font will trigger an error that
 * will shutdown the cairo_t object).
 *
 * Since: 1.4
 **/
cairo_scaled_font_t *
cairo_get_scaled_font (cairo_t *cr)
{
    cairo_scaled_font_t *scaled_font;

    if (cr->status)
	return (cairo_scaled_font_t *)&_cairo_scaled_font_nil;

    cr->status = _cairo_gstate_get_scaled_font (cr->gstate, &scaled_font);
    if (cr->status) {
	_cairo_set_error (cr, cr->status);
	return (cairo_scaled_font_t *)&_cairo_scaled_font_nil;
    }

    return scaled_font;
}

/**
 * cairo_text_extents:
 * @cr: a #cairo_t
 * @utf8: a string of text, encoded in UTF-8
 * @extents: a #cairo_text_extents_t object into which the results
 * will be stored
 *
 * Gets the extents for a string of text. The extents describe a
 * user-space rectangle that encloses the "inked" portion of the text,
 * (as it would be drawn by cairo_show_text()). Additionally, the
 * x_advance and y_advance values indicate the amount by which the
 * current point would be advanced by cairo_show_text().
 *
 * Note that whitespace characters do not directly contribute to the
 * size of the rectangle (extents.width and extents.height). They do
 * contribute indirectly by changing the position of non-whitespace
 * characters. In particular, trailing whitespace characters are
 * likely to not affect the size of the rectangle, though they will
 * affect the x_advance and y_advance values.
 **/
void
cairo_text_extents (cairo_t              *cr,
		    const char		 *utf8,
		    cairo_text_extents_t *extents)
{
    cairo_glyph_t *glyphs = NULL;
    int num_glyphs;
    double x, y;

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

    cairo_get_current_point (cr, &x, &y);

    cr->status = _cairo_gstate_text_to_glyphs (cr->gstate, utf8,
					       x, y,
					       &glyphs, &num_glyphs);

    if (cr->status) {
	if (glyphs)
	    free (glyphs);
	_cairo_set_error (cr, cr->status);
	return;
    }

    cr->status = _cairo_gstate_glyph_extents (cr->gstate, glyphs, num_glyphs, extents);
    if (glyphs)
	free (glyphs);

    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_glyph_extents:
 * @cr: a #cairo_t
 * @glyphs: an array of #cairo_glyph_t objects
 * @num_glyphs: the number of elements in @glyphs
 * @extents: a #cairo_text_extents_t object into which the results
 * will be stored
 *
 * Gets the extents for an array of glyphs. The extents describe a
 * user-space rectangle that encloses the "inked" portion of the
 * glyphs, (as they would be drawn by cairo_show_glyphs()).
 * Additionally, the x_advance and y_advance values indicate the
 * amount by which the current point would be advanced by
 * cairo_show_glyphs.
 *
 * Note that whitespace glyphs do not contribute to the size of the
 * rectangle (extents.width and extents.height).
 **/
void
cairo_glyph_extents (cairo_t                *cr,
		     const cairo_glyph_t    *glyphs,
		     int                    num_glyphs,
		     cairo_text_extents_t   *extents)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_glyph_extents (cr->gstate, glyphs, num_glyphs,
					      extents);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_show_text:
 * @cr: a cairo context
 * @utf8: a string of text encoded in UTF-8
 *
 * A drawing operator that generates the shape from a string of UTF-8
 * characters, rendered according to the current font_face, font_size
 * (font_matrix), and font_options.
 *
 * This function first computes a set of glyphs for the string of
 * text. The first glyph is placed so that its origin is at the
 * current point. The origin of each subsequent glyph is offset from
 * that of the previous glyph by the advance values of the previous
 * glyph.
 *
 * After this call the current point is moved to the origin of where
 * the next glyph would be placed in this same progression. That is,
 * the current point will be at the origin of the final glyph offset
 * by its advance values. This allows for easy display of a single
 * logical string with multiple calls to cairo_show_text().
 *
 * NOTE: The cairo_show_text() function call is part of what the cairo
 * designers call the "toy" text API. It is convenient for short demos
 * and simple programs, but it is not expected to be adequate for
 * serious text-using applications. See cairo_show_glyphs() for the
 * "real" text display API in cairo.
 **/
void
cairo_show_text (cairo_t *cr, const char *utf8)
{
    cairo_text_extents_t extents;
    cairo_glyph_t *glyphs = NULL, *last_glyph;
    int num_glyphs;
    double x, y;

    if (cr->status)
	return;

    if (utf8 == NULL)
	return;

    cairo_get_current_point (cr, &x, &y);

    cr->status = _cairo_gstate_text_to_glyphs (cr->gstate, utf8,
					       x, y,
					       &glyphs, &num_glyphs);
    if (cr->status)
	goto BAIL;

    if (num_glyphs == 0)
	return;

    cr->status = _cairo_gstate_show_glyphs (cr->gstate, glyphs, num_glyphs);
    if (cr->status)
	goto BAIL;

    last_glyph = &glyphs[num_glyphs - 1];
    cr->status = _cairo_gstate_glyph_extents (cr->gstate,
					      last_glyph, 1,
					      &extents);
    if (cr->status)
	goto BAIL;

    x = last_glyph->x + extents.x_advance;
    y = last_glyph->y + extents.y_advance;
    cairo_move_to (cr, x, y);

 BAIL:
    if (glyphs)
	free (glyphs);

    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_show_glyphs:
 * @cr: a cairo context
 * @glyphs: array of glyphs to show
 * @num_glyphs: number of glyphs to show
 *
 * A drawing operator that generates the shape from an array  of glyphs,
 * rendered according to the current font_face, font_size
 * (font_matrix), and font_options.
 **/
void
cairo_show_glyphs (cairo_t *cr, const cairo_glyph_t *glyphs, int num_glyphs)
{
    if (cr->status)
	return;

    if (num_glyphs == 0)
	return;

    cr->status = _cairo_gstate_show_glyphs (cr->gstate, glyphs, num_glyphs);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_text_path:
 * @cr: a cairo context
 * @utf8: a string of text encoded in UTF-8
 *
 * Adds closed paths for text to the current path.  The generated
 * path if filled, achieves an effect similar to that of
 * cairo_show_text().
 *
 * Text conversion and positioning is done similar to cairo_show_text().
 *
 * Like cairo_show_text(), After this call the current point is
 * moved to the origin of where the next glyph would be placed in
 * this same progression.  That is, the current point will be at
 * the origin of the final glyph offset by its advance values.
 * This allows for chaining multiple calls to to cairo_text_path()
 * without having to set current point in between.
 *
 * NOTE: The cairo_text_path() function call is part of what the cairo
 * designers call the "toy" text API. It is convenient for short demos
 * and simple programs, but it is not expected to be adequate for
 * serious text-using applications. See cairo_glyph_path() for the
 * "real" text path API in cairo.
 **/
void
cairo_text_path  (cairo_t *cr, const char *utf8)
{
    cairo_text_extents_t extents;
    cairo_glyph_t *glyphs = NULL, *last_glyph;
    int num_glyphs;
    double x, y;

    if (cr->status)
	return;

    cairo_get_current_point (cr, &x, &y);

    cr->status = _cairo_gstate_text_to_glyphs (cr->gstate, utf8,
					       x, y,
					       &glyphs, &num_glyphs);

    if (cr->status)
	goto BAIL;

    if (num_glyphs == 0)
	return;

    cr->status = _cairo_gstate_glyph_path (cr->gstate,
					   glyphs, num_glyphs,
					   cr->path);

    if (cr->status)
	goto BAIL;

    last_glyph = &glyphs[num_glyphs - 1];
    cr->status = _cairo_gstate_glyph_extents (cr->gstate,
					      last_glyph, 1,
					      &extents);

    if (cr->status)
	goto BAIL;

    x = last_glyph->x + extents.x_advance;
    y = last_glyph->y + extents.y_advance;
    cairo_move_to (cr, x, y);

 BAIL:
    if (glyphs)
	free (glyphs);

    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_glyph_path:
 * @cr: a cairo context
 * @glyphs: array of glyphs to show
 * @num_glyphs: number of glyphs to show
 *
 * Adds closed paths for the glyphs to the current path.  The generated
 * path if filled, achieves an effect similar to that of
 * cairo_show_glyphs().
 **/
void
cairo_glyph_path (cairo_t *cr, const cairo_glyph_t *glyphs, int num_glyphs)
{
    if (cr->status)
	return;

    cr->status = _cairo_gstate_glyph_path (cr->gstate,
					   glyphs, num_glyphs,
					   cr->path);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_get_operator:
 * @cr: a cairo context
 *
 * Gets the current compositing operator for a cairo context.
 *
 * Return value: the current compositing operator.
 **/
cairo_operator_t
cairo_get_operator (cairo_t *cr)
{
    return _cairo_gstate_get_operator (cr->gstate);
}

/**
 * cairo_get_tolerance:
 * @cr: a cairo context
 *
 * Gets the current tolerance value, as set by cairo_set_tolerance().
 *
 * Return value: the current tolerance value.
 **/
double
cairo_get_tolerance (cairo_t *cr)
{
    return _cairo_gstate_get_tolerance (cr->gstate);
}
slim_hidden_def (cairo_get_tolerance);

/**
 * cairo_get_antialias:
 * @cr: a cairo context
 *
 * Gets the current shape antialiasing mode, as set by cairo_set_shape_antialias().
 *
 * Return value: the current shape antialiasing mode.
 **/
cairo_antialias_t
cairo_get_antialias (cairo_t *cr)
{
    return _cairo_gstate_get_antialias (cr->gstate);
}

/**
 * cairo_get_current_point:
 * @cr: a cairo context
 * @x: return value for X coordinate of the current point
 * @y: return value for Y coordinate of the current point
 *
 * Gets the current point of the current path, which is
 * conceptually the final point reached by the path so far.
 *
 * The current point is returned in the user-space coordinate
 * system. If there is no defined current point then @x and @y will
 * both be set to 0.0.
 *
 * Most path construction functions alter the current point. See the
 * following for details on how they affect the current point:
 *
 * cairo_new_path(), cairo_move_to(), cairo_line_to(),
 * cairo_curve_to(), cairo_arc(), cairo_rel_move_to(),
 * cairo_rel_line_to(), cairo_rel_curve_to(), cairo_arc(),
 * cairo_text_path(), cairo_stroke_to_path()
 **/
void
cairo_get_current_point (cairo_t *cr, double *x_ret, double *y_ret)
{
    cairo_status_t status;
    cairo_fixed_t x_fixed, y_fixed;
    double x, y;

    status = _cairo_path_fixed_get_current_point (cr->path, &x_fixed, &y_fixed);
    if (status == CAIRO_STATUS_NO_CURRENT_POINT) {
	x = 0.0;
	y = 0.0;
    } else {
	x = _cairo_fixed_to_double (x_fixed);
	y = _cairo_fixed_to_double (y_fixed);
	_cairo_gstate_backend_to_user (cr->gstate, &x, &y);
    }

    if (x_ret)
	*x_ret = x;
    if (y_ret)
	*y_ret = y;
}
slim_hidden_def(cairo_get_current_point);

/**
 * cairo_get_fill_rule:
 * @cr: a cairo context
 *
 * Gets the current fill rule, as set by cairo_set_fill_rule().
 *
 * Return value: the current fill rule.
 **/
cairo_fill_rule_t
cairo_get_fill_rule (cairo_t *cr)
{
    return _cairo_gstate_get_fill_rule (cr->gstate);
}

/**
 * cairo_get_line_width:
 * @cr: a cairo context
 *
 * This function returns the current line width value exactly as set by
 * cairo_set_line_width(). Note that the value is unchanged even if
 * the CTM has changed between the calls to cairo_set_line_width() and
 * cairo_get_line_width().
 *
 * Return value: the current line width.
 **/
double
cairo_get_line_width (cairo_t *cr)
{
    return _cairo_gstate_get_line_width (cr->gstate);
}

/**
 * cairo_get_line_cap:
 * @cr: a cairo context
 *
 * Gets the current line cap style, as set by cairo_set_line_cap().
 *
 * Return value: the current line cap style.
 **/
cairo_line_cap_t
cairo_get_line_cap (cairo_t *cr)
{
    return _cairo_gstate_get_line_cap (cr->gstate);
}

/**
 * cairo_get_line_join:
 * @cr: a cairo context
 *
 * Gets the current line join style, as set by cairo_set_line_join().
 *
 * Return value: the current line join style.
 **/
cairo_line_join_t
cairo_get_line_join (cairo_t *cr)
{
    return _cairo_gstate_get_line_join (cr->gstate);
}

/**
 * cairo_get_miter_limit:
 * @cr: a cairo context
 *
 * Gets the current miter limit, as set by cairo_set_miter_limit().
 *
 * Return value: the current miter limit.
 **/
double
cairo_get_miter_limit (cairo_t *cr)
{
    return _cairo_gstate_get_miter_limit (cr->gstate);
}

/**
 * cairo_get_matrix:
 * @cr: a cairo context
 * @matrix: return value for the matrix
 *
 * Stores the current transformation matrix (CTM) into @matrix.
 **/
void
cairo_get_matrix (cairo_t *cr, cairo_matrix_t *matrix)
{
    _cairo_gstate_get_matrix (cr->gstate, matrix);
}
slim_hidden_def (cairo_get_matrix);

/**
 * cairo_get_target:
 * @cr: a cairo context
 *
 * Gets the target surface for the cairo context as passed to
 * cairo_create().
 *
 * This function will always return a valid pointer, but the result
 * can be a "nil" surface if @cr is already in an error state,
 * (ie. cairo_status() <literal>!=</literal> %CAIRO_STATUS_SUCCESS).
 * A nil surface is indicated by cairo_surface_status()
 * <literal>!=</literal> %CAIRO_STATUS_SUCCESS.
 *
 * Return value: the target surface. This object is owned by cairo. To
 * keep a reference to it, you must call cairo_surface_reference().
 **/
cairo_surface_t *
cairo_get_target (cairo_t *cr)
{
    if (cr->status)
	return (cairo_surface_t*) &_cairo_surface_nil;

    return _cairo_gstate_get_original_target (cr->gstate);
}

/**
 * cairo_get_group_target:
 * @cr: a cairo context
 *
 * Gets the target surface for the current group as started by the
 * most recent call to cairo_push_group() or
 * cairo_push_group_with_content().
 *
 * This function will return NULL if called "outside" of any group
 * rendering blocks, (that is, after the last balancing call to
 * cairo_pop_group() or cairo_pop_group_to_source()).
 *
 * Return value: the target group surface, or NULL if none.  This
 * object is owned by cairo. To keep a reference to it, you must call
 * cairo_surface_reference().
 *
 * Since: 1.2
 **/
cairo_surface_t *
cairo_get_group_target (cairo_t *cr)
{
    if (cr->status)
	return (cairo_surface_t*) &_cairo_surface_nil;

    return _cairo_gstate_get_target (cr->gstate);
}

/**
 * cairo_copy_path:
 * @cr: a cairo context
 *
 * Creates a copy of the current path and returns it to the user as a
 * #cairo_path_t. See #cairo_path_data_t for hints on how to iterate
 * over the returned data structure.
 *
 * This function will always return a valid pointer, but the result
 * will have no data (<literal>data==NULL</literal> and
 * <literal>num_data==0</literal>), if either of the following
 * conditions hold:
 *
 * <orderedlist>
 * <listitem>If there is insufficient memory to copy the path. In this
 *     case <literal>path->status</literal> will be set to
 *     %CAIRO_STATUS_NO_MEMORY.</listitem>
 * <listitem>If @cr is already in an error state. In this case
 *    <literal>path->status</literal> will contain the same status that
 *    would be returned by cairo_status().</listitem>
 * </orderedlist>
 *
 * In either case, <literal>path->status</literal> will be set to
 * %CAIRO_STATUS_NO_MEMORY (regardless of what the error status in
 * @cr might have been).
 *
 * Return value: the copy of the current path. The caller owns the
 * returned object and should call cairo_path_destroy() when finished
 * with it.
 **/
cairo_path_t *
cairo_copy_path (cairo_t *cr)
{
    if (cr->status)
	return _cairo_path_create_in_error (cr->status);

    return _cairo_path_create (cr->path, cr->gstate);
}

/**
 * cairo_copy_path_flat:
 * @cr: a cairo context
 *
 * Gets a flattened copy of the current path and returns it to the
 * user as a #cairo_path_t. See #cairo_path_data_t for hints on
 * how to iterate over the returned data structure.
 *
 * This function is like cairo_copy_path() except that any curves
 * in the path will be approximated with piecewise-linear
 * approximations, (accurate to within the current tolerance
 * value). That is, the result is guaranteed to not have any elements
 * of type %CAIRO_PATH_CURVE_TO which will instead be replaced by a
 * series of %CAIRO_PATH_LINE_TO elements.
 *
 * This function will always return a valid pointer, but the result
 * will have no data (<literal>data==NULL</literal> and
 * <literal>num_data==0</literal>), if either of the following
 * conditions hold:
 *
 * <orderedlist>
 * <listitem>If there is insufficient memory to copy the path. In this
 *     case <literal>path->status</literal> will be set to
 *     %CAIRO_STATUS_NO_MEMORY.</listitem>
 * <listitem>If @cr is already in an error state. In this case
 *    <literal>path->status</literal> will contain the same status that
 *    would be returned by cairo_status().</listitem>
 * </orderedlist>
 *
 * Return value: the copy of the current path. The caller owns the
 * returned object and should call cairo_path_destroy() when finished
 * with it.
 **/
cairo_path_t *
cairo_copy_path_flat (cairo_t *cr)
{
    if (cr->status)
	return _cairo_path_create_in_error (cr->status);

    return _cairo_path_create_flat (cr->path, cr->gstate);
}

/**
 * cairo_append_path:
 * @cr: a cairo context
 * @path: path to be appended
 *
 * Append the @path onto the current path. The @path may be either the
 * return value from one of cairo_copy_path() or
 * cairo_copy_path_flat() or it may be constructed manually.  See
 * #cairo_path_t for details on how the path data structure should be
 * initialized, and note that <literal>path->status</literal> must be
 * initialized to %CAIRO_STATUS_SUCCESS.
 **/
void
cairo_append_path (cairo_t		*cr,
		   const cairo_path_t	*path)
{
    if (cr->status)
	return;

    if (path == NULL) {
	_cairo_set_error (cr, CAIRO_STATUS_NULL_POINTER);
	return;
    }

    if (path->status) {
	if (path->status > CAIRO_STATUS_SUCCESS &&
	    path->status <= CAIRO_STATUS_LAST_STATUS)
	    _cairo_set_error (cr, path->status);
	else
	    _cairo_set_error (cr, CAIRO_STATUS_INVALID_STATUS);
	return;
    }

    if (path->data == NULL) {
	_cairo_set_error (cr, CAIRO_STATUS_NULL_POINTER);
	return;
    }

    cr->status = _cairo_path_append_to_context (path, cr);
    if (cr->status)
	_cairo_set_error (cr, cr->status);
}

/**
 * cairo_status:
 * @cr: a cairo context
 *
 * Checks whether an error has previously occurred for this context.
 *
 * Returns the current status of this context, see #cairo_status_t
 **/
cairo_status_t
cairo_status (cairo_t *cr)
{
    return cr->status;
}
slim_hidden_def (cairo_status);

/**
 * cairo_status_to_string:
 * @status: a cairo status
 *
 * Provides a human-readable description of a #cairo_status_t.
 *
 * Returns a string representation of the status
 */
const char *
cairo_status_to_string (cairo_status_t status)
{
    switch (status) {
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
    case CAIRO_STATUS_INVALID_STATUS:
	return "invalid value for an input cairo_status_t";
    case CAIRO_STATUS_NULL_POINTER:
	return "NULL pointer";
    case CAIRO_STATUS_INVALID_STRING:
	return "input string not valid UTF-8";
    case CAIRO_STATUS_INVALID_PATH_DATA:
	return "input path data not valid";
    case CAIRO_STATUS_READ_ERROR:
	return "error while reading from input stream";
    case CAIRO_STATUS_WRITE_ERROR:
	return "error while writing to output stream";
    case CAIRO_STATUS_SURFACE_FINISHED:
	return "the target surface has been finished";
    case CAIRO_STATUS_SURFACE_TYPE_MISMATCH:
	return "the surface type is not appropriate for the operation";
    case CAIRO_STATUS_PATTERN_TYPE_MISMATCH:
	return "the pattern type is not appropriate for the operation";
    case CAIRO_STATUS_INVALID_CONTENT:
	return "invalid value for an input cairo_content_t";
    case CAIRO_STATUS_INVALID_FORMAT:
	return "invalid value for an input cairo_format_t";
    case CAIRO_STATUS_INVALID_VISUAL:
	return "invalid value for an input Visual*";
    case CAIRO_STATUS_FILE_NOT_FOUND:
	return "file not found";
    case CAIRO_STATUS_INVALID_DASH:
	return "invalid value for a dash setting";
    case CAIRO_STATUS_INVALID_DSC_COMMENT:
	return "invalid value for a DSC comment";
    case CAIRO_STATUS_INVALID_INDEX:
	return "invalid index passed to getter";
    case CAIRO_STATUS_CLIP_NOT_REPRESENTABLE:
        return "clip region not representable in desired format";
    }

    return "<unknown error status>";
}

void
_cairo_restrict_value (double *value, double min, double max)
{
    if (*value < min)
	*value = min;
    else if (*value > max)
	*value = max;
}

/* This function is identical to the C99 function lround(), except that it
 * performs arithmetic rounding (instead of away-from-zero rounding) and
 * has a valid input range of (INT_MIN, INT_MAX] instead of
 * [INT_MIN, INT_MAX]. It is much faster on both x86 and FPU-less systems
 * than other commonly used methods for rounding (lround, round, rint, lrint
 * or float (d + 0.5)).
 *
 * The reason why this function is much faster on x86 than other
 * methods is due to the fact that it avoids the fldcw instruction.
 * This instruction incurs a large performance penalty on modern Intel
 * processors due to how it prevents efficient instruction pipelining.
 *
 * The reason why this function is much faster on FPU-less systems is for
 * an entirely different reason. All common rounding methods involve multiple
 * floating-point operations. Each one of these operations has to be
 * emulated in software, which adds up to be a large performance penalty.
 * This function doesn't perform any floating-point calculations, and thus
 * avoids this penalty.
  */
int
_cairo_lround (double d)
{
    uint32_t top, shift_amount, output;
    union {
        double d;
        uint64_t ui64;
        uint32_t ui32[2];
    } u;

    u.d = d;

    /* If the integer word order doesn't match the float word order, we swap
     * the words of the input double. This is needed because we will be
     * treating the whole double as a 64-bit unsigned integer. Notice that we
     * use WORDS_BIGENDIAN to detect the integer word order, which isn't
     * exactly correct because WORDS_BIGENDIAN refers to byte order, not word
     * order. Thus, we are making the assumption that the byte order is the
     * same as the integer word order which, on the modern machines that we
     * care about, is OK.
     */
#if ( defined(FLOAT_WORDS_BIGENDIAN) && !defined(WORDS_BIGENDIAN)) || \
    (!defined(FLOAT_WORDS_BIGENDIAN) &&  defined(WORDS_BIGENDIAN))
    {
        uint32_t temp = u.ui32[0];
        u.ui32[0] = u.ui32[1];
        u.ui32[1] = temp;
    }
#endif

#ifdef WORDS_BIGENDIAN
    #define MSW (0) /* Most Significant Word */
    #define LSW (1) /* Least Significant Word */
#else
    #define MSW (1)
    #define LSW (0)
#endif

    /* By shifting the most significant word of the input double to the
     * right 20 places, we get the very "top" of the double where the exponent
     * and sign bit lie.
     */
    top = u.ui32[MSW] >> 20;

    /* Here, we calculate how much we have to shift the mantissa to normalize
     * it to an integer value. We extract the exponent "top" by masking out the
     * sign bit, then we calculate the shift amount by subtracting the exponent
     * from the bias. Notice that the correct bias for 64-bit doubles is
     * actually 1075, but we use 1053 instead for two reasons:
     *
     *  1) To perform rounding later on, we will first need the target
     *     value in a 31.1 fixed-point format. Thus, the bias needs to be one
     *     less: (1075 - 1: 1074).
     *
     *  2) To avoid shifting the mantissa as a full 64-bit integer (which is
     *     costly on certain architectures), we break the shift into two parts.
     *     First, the upper and lower parts of the mantissa are shifted
     *     individually by a constant amount that all valid inputs will require
     *     at the very least. This amount is chosen to be 21, because this will
     *     allow the two parts of the mantissa to later be combined into a
     *     single 32-bit representation, on which the remainder of the shift
     *     will be performed. Thus, we decrease the bias by an additional 21:
     *     (1074 - 21: 1053).
     */
    shift_amount = 1053 - (top & 0x7FF);

    /* We are done with the exponent portion in "top", so here we shift it off
     * the end.
     */
    top >>= 11;

    /* Before we perform any operations on the mantissa, we need to OR in
     * the implicit 1 at the top (see the IEEE-754 spec). We needn't mask
     * off the sign bit nor the exponent bits because these higher bits won't
     * make a bit of difference in the rest of our calculations.
     */
    u.ui32[MSW] |= 0x100000;

    /* If the input double is negative, we have to decrease the mantissa
     * by a hair. This is an important part of performing arithmetic rounding,
     * as negative numbers must round towards positive infinity in the
     * halfwase case of -x.5. Since "top" contains only the sign bit at this
     * point, we can just decrease the mantissa by the value of "top".
     */
    u.ui64 -= top;

    /* By decrementing "top", we create a bitmask with a value of either
     * 0x0 (if the input was negative) or 0xFFFFFFFF (if the input was positive
     * and thus the unsigned subtraction underflowed) that we'll use later.
     */
    top--;

    /* Here, we shift the mantissa by the constant value as described above.
     * We can emulate a 64-bit shift right by 21 through shifting the top 32
     * bits left 11 places and ORing in the bottom 32 bits shifted 21 places
     * to the right. Both parts of the mantissa are now packed into a single
     * 32-bit integer. Although we severely truncate the lower part in the
     * process, we still have enough significant bits to perform the conversion
     * without error (for all valid inputs).
     */
    output = (u.ui32[MSW] << 11) | (u.ui32[LSW] >> 21);

    /* Next, we perform the shift that converts the X.Y fixed-point number
     * currently found in "output" to the desired 31.1 fixed-point format
     * needed for the following rounding step. It is important to consider
     * all possible values for "shift_amount" at this point:
     *
     * - {shift_amount < 0} Since shift_amount is an unsigned integer, it
     *   really can't have a value less than zero. But, if the shift_amount
     *   calculation above caused underflow (which would happen with
     *   input > INT_MAX or input <= INT_MIN) then shift_amount will now be
     *   a very large number, and so this shift will result in complete
     *   garbage. But that's OK, as the input was out of our range, so our
     *   output is undefined.
     *
     * - {shift_amount > 31} If the magnitude of the input was very small
     *   (i.e. |input| << 1.0), shift_amount will have a value greater than
     *   31. Thus, this shift will also result in garbage. After performing
     *   the shift, we will zero-out "output" if this is the case.
     *
     * - {0 <= shift_amount < 32} In this case, the shift will properly convert
     *   the mantissa into a 31.1 fixed-point number.
     */
    output >>= shift_amount;

    /* This is where we perform rounding with the 31.1 fixed-point number.
     * Since what we're after is arithmetic rounding, we simply add the single
     * fractional bit into the integer part of "output", and just keep the
     * integer part.
     */
    output = (output >> 1) + (output & 1);

    /* Here, we zero-out the result if the magnitude if the input was very small
     * (as explained in the section above). Notice that all input out of the
     * valid range is also caught by this condition, which means we produce 0
     * for all invalid input, which is a nice side effect.
     *
     * The most straightforward way to do this would be:
     *
     *      if (shift_amount > 31)
     *          output = 0;
     *
     * But we can use a little trick to avoid the potential branch. The
     * expression (shift_amount > 31) will be either 1 or 0, which when
     * decremented will be either 0x0 or 0xFFFFFFFF (unsigned underflow),
     * which can be used to conditionally mask away all the bits in "output"
     * (in the 0x0 case), effectively zeroing it out. Certain, compilers would
     * have done this for us automatically.
     */
    output &= ((shift_amount > 31) - 1);

    /* If the input double was a negative number, then we have to negate our
     * output. The most straightforward way to do this would be:
     *
     *      if (!top)
     *          output = -output;
     *
     * as "top" at this point is either 0x0 (if the input was negative) or
     * 0xFFFFFFFF (if the input was positive). But, we can use a trick to
     * avoid the branch. Observe that the following snippet of code has the
     * same effect as the reference snippet above:
     *
     *      if (!top)
     *          output = 0 - output;
     *      else
     *          output = output - 0;
     *
     * Armed with the bitmask found in "top", we can condense the two statements
     * into the following:
     *
     *      output = (output & top) - (output & ~top);
     *
     * where, in the case that the input double was negative, "top" will be 0,
     * and the statement will be equivalent to:
     *
     *      output = (0) - (output);
     *
     * and if the input double was positive, "top" will be 0xFFFFFFFF, and the
     * statement will be equivalent to:
     *
     *      output = (output) - (0);
     *
     * Which, as pointed out earlier, is equivalent to the original reference
     * snippet.
     */
    output = (output & top) - (output & ~top);

    return output;
#undef MSW
#undef LSW
}
