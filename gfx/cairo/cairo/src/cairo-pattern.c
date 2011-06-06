/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2004 David Reveman
 * Copyright © 2005 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of David
 * Reveman not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. David Reveman makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * DAVID REVEMAN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL DAVID REVEMAN BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors: David Reveman <davidr@novell.com>
 *	    Keith Packard <keithp@keithp.com>
 *	    Carl Worth <cworth@cworth.org>
 */

#include "cairoint.h"
#include "cairo-error-private.h"
#include "cairo-freed-pool-private.h"

/**
 * SECTION:cairo-pattern
 * @Title: cairo_pattern_t
 * @Short_Description: Sources for drawing
 * @See_Also: #cairo_t, #cairo_surface_t
 *
 * #cairo_pattern_t is the paint with which cairo draws.
 * The primary use of patterns is as the source for all cairo drawing 
 * operations, although they can also be used as masks, that is, as the 
 * brush too.
 *
 * A cairo pattern is created by using one of the many constructors,
 * of the form cairo_pattern_create_<emphasis>type</emphasis>()
 * or implicitly through
 * cairo_set_source_<emphasis>type</emphasis>() functions.
 */

#if HAS_FREED_POOL
static freed_pool_t freed_pattern_pool[4];
#endif

static const cairo_solid_pattern_t _cairo_pattern_nil = {
    { CAIRO_PATTERN_TYPE_SOLID,		/* type */
      CAIRO_REFERENCE_COUNT_INVALID,	/* ref_count */
      CAIRO_STATUS_NO_MEMORY,		/* status */
      { 0, 0, 0, NULL },		/* user_data */
      { 1., 0., 0., 1., 0., 0., },	/* matrix */
      CAIRO_FILTER_DEFAULT,		/* filter */
      CAIRO_EXTEND_GRADIENT_DEFAULT },	/* extend */
};

static const cairo_solid_pattern_t _cairo_pattern_nil_null_pointer = {
    { CAIRO_PATTERN_TYPE_SOLID,		/* type */
      CAIRO_REFERENCE_COUNT_INVALID,	/* ref_count */
      CAIRO_STATUS_NULL_POINTER,	/* status */
      { 0, 0, 0, NULL },		/* user_data */
      { 1., 0., 0., 1., 0., 0., },	/* matrix */
      CAIRO_FILTER_DEFAULT,		/* filter */
      CAIRO_EXTEND_GRADIENT_DEFAULT },	/* extend */
};

const cairo_solid_pattern_t _cairo_pattern_black = {
    { CAIRO_PATTERN_TYPE_SOLID,		/* type */
      CAIRO_REFERENCE_COUNT_INVALID,	/* ref_count */
      CAIRO_STATUS_SUCCESS,		/* status */
      { 0, 0, 0, NULL },		/* user_data */
      { 1., 0., 0., 1., 0., 0., },	/* matrix */
      CAIRO_FILTER_DEFAULT,		/* filter */
      CAIRO_EXTEND_GRADIENT_DEFAULT},	/* extend */
    { 0., 0., 0., 1., 0, 0, 0, 0xffff },/* color (double rgba, short rgba) */
};

const cairo_solid_pattern_t _cairo_pattern_clear = {
    { CAIRO_PATTERN_TYPE_SOLID,		/* type */
      CAIRO_REFERENCE_COUNT_INVALID,	/* ref_count */
      CAIRO_STATUS_SUCCESS,		/* status */
      { 0, 0, 0, NULL },		/* user_data */
      { 1., 0., 0., 1., 0., 0., },	/* matrix */
      CAIRO_FILTER_DEFAULT,		/* filter */
      CAIRO_EXTEND_GRADIENT_DEFAULT},	/* extend */
    { 0., 0., 0., 0., 0, 0, 0, 0 },/* color (double rgba, short rgba) */
};

const cairo_solid_pattern_t _cairo_pattern_white = {
    { CAIRO_PATTERN_TYPE_SOLID,		/* type */
      CAIRO_REFERENCE_COUNT_INVALID,	/* ref_count */
      CAIRO_STATUS_SUCCESS,		/* status */
      { 0, 0, 0, NULL },		/* user_data */
      { 1., 0., 0., 1., 0., 0., },	/* matrix */
      CAIRO_FILTER_DEFAULT,		/* filter */
      CAIRO_EXTEND_GRADIENT_DEFAULT},	/* extend */
    { 1., 1., 1., 1., 0xffff, 0xffff, 0xffff, 0xffff },/* color (double rgba, short rgba) */
};

/**
 * _cairo_pattern_set_error:
 * @pattern: a pattern
 * @status: a status value indicating an error
 *
 * Atomically sets pattern->status to @status and calls _cairo_error;
 * Does nothing if status is %CAIRO_STATUS_SUCCESS.
 *
 * All assignments of an error status to pattern->status should happen
 * through _cairo_pattern_set_error(). Note that due to the nature of
 * the atomic operation, it is not safe to call this function on the nil
 * objects.
 *
 * The purpose of this function is to allow the user to set a
 * breakpoint in _cairo_error() to generate a stack trace for when the
 * user causes cairo to detect an error.
 **/
static cairo_status_t
_cairo_pattern_set_error (cairo_pattern_t *pattern,
			  cairo_status_t status)
{
    if (status == CAIRO_STATUS_SUCCESS)
	return status;

    /* Don't overwrite an existing error. This preserves the first
     * error, which is the most significant. */
    _cairo_status_set_error (&pattern->status, status);

    return _cairo_error (status);
}

static void
_cairo_pattern_init (cairo_pattern_t *pattern, cairo_pattern_type_t type)
{
#if HAVE_VALGRIND
    switch (type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	VALGRIND_MAKE_MEM_UNDEFINED (pattern, sizeof (cairo_solid_pattern_t));
	break;
    case CAIRO_PATTERN_TYPE_SURFACE:
	VALGRIND_MAKE_MEM_UNDEFINED (pattern, sizeof (cairo_surface_pattern_t));
	break;
    case CAIRO_PATTERN_TYPE_LINEAR:
	VALGRIND_MAKE_MEM_UNDEFINED (pattern, sizeof (cairo_linear_pattern_t));
	break;
    case CAIRO_PATTERN_TYPE_RADIAL:
	VALGRIND_MAKE_MEM_UNDEFINED (pattern, sizeof (cairo_radial_pattern_t));
	break;
    }
#endif

    pattern->type      = type;
    pattern->status    = CAIRO_STATUS_SUCCESS;

    /* Set the reference count to zero for on-stack patterns.
     * Callers needs to explicitly increment the count for heap allocations. */
    CAIRO_REFERENCE_COUNT_INIT (&pattern->ref_count, 0);

    _cairo_user_data_array_init (&pattern->user_data);

    if (type == CAIRO_PATTERN_TYPE_SURFACE)
	pattern->extend = CAIRO_EXTEND_SURFACE_DEFAULT;
    else
	pattern->extend = CAIRO_EXTEND_GRADIENT_DEFAULT;

    pattern->filter    = CAIRO_FILTER_DEFAULT;

    pattern->has_component_alpha = FALSE;

    cairo_matrix_init_identity (&pattern->matrix);
}

static cairo_status_t
_cairo_gradient_pattern_init_copy (cairo_gradient_pattern_t	  *pattern,
				   const cairo_gradient_pattern_t *other)
{
    if (CAIRO_INJECT_FAULT ())
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    if (other->base.type == CAIRO_PATTERN_TYPE_LINEAR)
    {
	cairo_linear_pattern_t *dst = (cairo_linear_pattern_t *) pattern;
	cairo_linear_pattern_t *src = (cairo_linear_pattern_t *) other;

	*dst = *src;
    }
    else
    {
	cairo_radial_pattern_t *dst = (cairo_radial_pattern_t *) pattern;
	cairo_radial_pattern_t *src = (cairo_radial_pattern_t *) other;

	*dst = *src;
    }

    if (other->stops == other->stops_embedded)
	pattern->stops = pattern->stops_embedded;
    else if (other->stops)
    {
	pattern->stops = _cairo_malloc_ab (other->stops_size,
					   sizeof (cairo_gradient_stop_t));
	if (unlikely (pattern->stops == NULL)) {
	    pattern->stops_size = 0;
	    pattern->n_stops = 0;
	    return _cairo_pattern_set_error (&pattern->base, CAIRO_STATUS_NO_MEMORY);
	}

	memcpy (pattern->stops, other->stops,
		other->n_stops * sizeof (cairo_gradient_stop_t));
    }

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_pattern_init_copy (cairo_pattern_t	*pattern,
			  const cairo_pattern_t *other)
{
    if (other->status)
	return _cairo_pattern_set_error (pattern, other->status);

    switch (other->type) {
    case CAIRO_PATTERN_TYPE_SOLID: {
	cairo_solid_pattern_t *dst = (cairo_solid_pattern_t *) pattern;
	cairo_solid_pattern_t *src = (cairo_solid_pattern_t *) other;

	VG (VALGRIND_MAKE_MEM_UNDEFINED (pattern, sizeof (cairo_solid_pattern_t)));

	*dst = *src;
    } break;
    case CAIRO_PATTERN_TYPE_SURFACE: {
	cairo_surface_pattern_t *dst = (cairo_surface_pattern_t *) pattern;
	cairo_surface_pattern_t *src = (cairo_surface_pattern_t *) other;

	VG (VALGRIND_MAKE_MEM_UNDEFINED (pattern, sizeof (cairo_surface_pattern_t)));

	*dst = *src;
	cairo_surface_reference (dst->surface);
    } break;
    case CAIRO_PATTERN_TYPE_LINEAR:
    case CAIRO_PATTERN_TYPE_RADIAL: {
	cairo_gradient_pattern_t *dst = (cairo_gradient_pattern_t *) pattern;
	cairo_gradient_pattern_t *src = (cairo_gradient_pattern_t *) other;
	cairo_status_t status;

	if (other->type == CAIRO_PATTERN_TYPE_LINEAR) {
	    VG (VALGRIND_MAKE_MEM_UNDEFINED (pattern, sizeof (cairo_linear_pattern_t)));
	} else {
	    VG (VALGRIND_MAKE_MEM_UNDEFINED (pattern, sizeof (cairo_radial_pattern_t)));
	}

	status = _cairo_gradient_pattern_init_copy (dst, src);
	if (unlikely (status))
	    return status;

    } break;
    }

    /* The reference count and user_data array are unique to the copy. */
    CAIRO_REFERENCE_COUNT_INIT (&pattern->ref_count, 0);
    _cairo_user_data_array_init (&pattern->user_data);

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_pattern_init_static_copy (cairo_pattern_t	*pattern,
				 const cairo_pattern_t *other)
{
    int size;

    assert (other->status == CAIRO_STATUS_SUCCESS);

    switch (other->type) {
    default:
	ASSERT_NOT_REACHED;
    case CAIRO_PATTERN_TYPE_SOLID:
	size = sizeof (cairo_solid_pattern_t);
	break;
    case CAIRO_PATTERN_TYPE_SURFACE:
	size = sizeof (cairo_surface_pattern_t);
	break;
    case CAIRO_PATTERN_TYPE_LINEAR:
	size = sizeof (cairo_linear_pattern_t);
	break;
    case CAIRO_PATTERN_TYPE_RADIAL:
	size = sizeof (cairo_radial_pattern_t);
	break;
    }

    memcpy (pattern, other, size);

    CAIRO_REFERENCE_COUNT_INIT (&pattern->ref_count, 0);
    _cairo_user_data_array_init (&pattern->user_data);
}

cairo_status_t
_cairo_pattern_init_snapshot (cairo_pattern_t       *pattern,
			      const cairo_pattern_t *other)
{
    cairo_status_t status;

    /* We don't bother doing any fancy copy-on-write implementation
     * for the pattern's data. It's generally quite tiny. */
    status = _cairo_pattern_init_copy (pattern, other);
    if (unlikely (status))
	return status;

    /* But we do let the surface snapshot stuff be as fancy as it
     * would like to be. */
    if (pattern->type == CAIRO_PATTERN_TYPE_SURFACE) {
	cairo_surface_pattern_t *surface_pattern =
	    (cairo_surface_pattern_t *) pattern;
	cairo_surface_t *surface = surface_pattern->surface;

	surface_pattern->surface = _cairo_surface_snapshot (surface);

	cairo_surface_destroy (surface);

	if (surface_pattern->surface->status)
	    return surface_pattern->surface->status;
    }

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_pattern_fini (cairo_pattern_t *pattern)
{
    _cairo_user_data_array_fini (&pattern->user_data);

    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	break;
    case CAIRO_PATTERN_TYPE_SURFACE: {
	cairo_surface_pattern_t *surface_pattern =
	    (cairo_surface_pattern_t *) pattern;

	cairo_surface_destroy (surface_pattern->surface);
    } break;
    case CAIRO_PATTERN_TYPE_LINEAR:
    case CAIRO_PATTERN_TYPE_RADIAL: {
	cairo_gradient_pattern_t *gradient =
	    (cairo_gradient_pattern_t *) pattern;

	if (gradient->stops && gradient->stops != gradient->stops_embedded)
	    free (gradient->stops);
    } break;
    }

#if HAVE_VALGRIND
    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	VALGRIND_MAKE_MEM_NOACCESS (pattern, sizeof (cairo_solid_pattern_t));
	break;
    case CAIRO_PATTERN_TYPE_SURFACE:
	VALGRIND_MAKE_MEM_NOACCESS (pattern, sizeof (cairo_surface_pattern_t));
	break;
    case CAIRO_PATTERN_TYPE_LINEAR:
	VALGRIND_MAKE_MEM_NOACCESS (pattern, sizeof (cairo_linear_pattern_t));
	break;
    case CAIRO_PATTERN_TYPE_RADIAL:
	VALGRIND_MAKE_MEM_NOACCESS (pattern, sizeof (cairo_radial_pattern_t));
	break;
    }
#endif
}

cairo_status_t
_cairo_pattern_create_copy (cairo_pattern_t	  **pattern_out,
			    const cairo_pattern_t  *other)
{
    cairo_pattern_t *pattern;
    cairo_status_t status;

    if (other->status)
	return other->status;

    switch (other->type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	pattern = malloc (sizeof (cairo_solid_pattern_t));
	break;
    case CAIRO_PATTERN_TYPE_SURFACE:
	pattern = malloc (sizeof (cairo_surface_pattern_t));
	break;
    case CAIRO_PATTERN_TYPE_LINEAR:
	pattern = malloc (sizeof (cairo_linear_pattern_t));
	break;
    case CAIRO_PATTERN_TYPE_RADIAL:
	pattern = malloc (sizeof (cairo_radial_pattern_t));
	break;
    default:
	ASSERT_NOT_REACHED;
	return _cairo_error (CAIRO_STATUS_PATTERN_TYPE_MISMATCH);
    }
    if (unlikely (pattern == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    status = _cairo_pattern_init_copy (pattern, other);
    if (unlikely (status)) {
	free (pattern);
	return status;
    }

    CAIRO_REFERENCE_COUNT_INIT (&pattern->ref_count, 1);
    *pattern_out = pattern;
    return CAIRO_STATUS_SUCCESS;
}


void
_cairo_pattern_init_solid (cairo_solid_pattern_t *pattern,
			   const cairo_color_t	 *color)
{
    _cairo_pattern_init (&pattern->base, CAIRO_PATTERN_TYPE_SOLID);
    pattern->color = *color;
}

void
_cairo_pattern_init_for_surface (cairo_surface_pattern_t *pattern,
				 cairo_surface_t	 *surface)
{
    if (surface->status) {
	/* Force to solid to simplify the pattern_fini process. */
	_cairo_pattern_init (&pattern->base, CAIRO_PATTERN_TYPE_SOLID);
	_cairo_pattern_set_error (&pattern->base, surface->status);
	return;
    }

    _cairo_pattern_init (&pattern->base, CAIRO_PATTERN_TYPE_SURFACE);

    pattern->surface = cairo_surface_reference (surface);
}

static void
_cairo_pattern_init_gradient (cairo_gradient_pattern_t *pattern,
			      cairo_pattern_type_t     type)
{
    _cairo_pattern_init (&pattern->base, type);

    pattern->n_stops    = 0;
    pattern->stops_size = 0;
    pattern->stops      = NULL;
}

void
_cairo_pattern_init_linear (cairo_linear_pattern_t *pattern,
			    double x0, double y0, double x1, double y1)
{
    _cairo_pattern_init_gradient (&pattern->base, CAIRO_PATTERN_TYPE_LINEAR);

    pattern->p1.x = _cairo_fixed_from_double (x0);
    pattern->p1.y = _cairo_fixed_from_double (y0);
    pattern->p2.x = _cairo_fixed_from_double (x1);
    pattern->p2.y = _cairo_fixed_from_double (y1);
}

void
_cairo_pattern_init_radial (cairo_radial_pattern_t *pattern,
			    double cx0, double cy0, double radius0,
			    double cx1, double cy1, double radius1)
{
    _cairo_pattern_init_gradient (&pattern->base, CAIRO_PATTERN_TYPE_RADIAL);

    pattern->c1.x = _cairo_fixed_from_double (cx0);
    pattern->c1.y = _cairo_fixed_from_double (cy0);
    pattern->r1   = _cairo_fixed_from_double (fabs (radius0));
    pattern->c2.x = _cairo_fixed_from_double (cx1);
    pattern->c2.y = _cairo_fixed_from_double (cy1);
    pattern->r2   = _cairo_fixed_from_double (fabs (radius1));
}

cairo_pattern_t *
_cairo_pattern_create_solid (const cairo_color_t *color)
{
    cairo_solid_pattern_t *pattern;

    pattern =
	_freed_pool_get (&freed_pattern_pool[CAIRO_PATTERN_TYPE_SOLID]);
    if (unlikely (pattern == NULL)) {
	/* None cached, need to create a new pattern. */
	pattern = malloc (sizeof (cairo_solid_pattern_t));
	if (unlikely (pattern == NULL)) {
	    _cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	    return (cairo_pattern_t *) &_cairo_pattern_nil;
	}
    }

    _cairo_pattern_init_solid (pattern, color);
    CAIRO_REFERENCE_COUNT_INIT (&pattern->base.ref_count, 1);

    return &pattern->base;
}

cairo_pattern_t *
_cairo_pattern_create_in_error (cairo_status_t status)
{
    cairo_pattern_t *pattern;

    if (status == CAIRO_STATUS_NO_MEMORY)
	return (cairo_pattern_t *)&_cairo_pattern_nil.base;

    CAIRO_MUTEX_INITIALIZE ();

    pattern = _cairo_pattern_create_solid (CAIRO_COLOR_BLACK);
    if (pattern->status == CAIRO_STATUS_SUCCESS)
	status = _cairo_pattern_set_error (pattern, status);

    return pattern;
}

/**
 * cairo_pattern_create_rgb:
 * @red: red component of the color
 * @green: green component of the color
 * @blue: blue component of the color
 *
 * Creates a new #cairo_pattern_t corresponding to an opaque color.  The
 * color components are floating point numbers in the range 0 to 1.
 * If the values passed in are outside that range, they will be
 * clamped.
 *
 * Return value: the newly created #cairo_pattern_t if successful, or
 * an error pattern in case of no memory.  The caller owns the
 * returned object and should call cairo_pattern_destroy() when
 * finished with it.
 *
 * This function will always return a valid pointer, but if an error
 * occurred the pattern status will be set to an error.  To inspect
 * the status of a pattern use cairo_pattern_status().
 **/
cairo_pattern_t *
cairo_pattern_create_rgb (double red, double green, double blue)
{
    cairo_color_t color;

    red   = _cairo_restrict_value (red,   0.0, 1.0);
    green = _cairo_restrict_value (green, 0.0, 1.0);
    blue  = _cairo_restrict_value (blue,  0.0, 1.0);

    _cairo_color_init_rgb (&color, red, green, blue);

    CAIRO_MUTEX_INITIALIZE ();

    return _cairo_pattern_create_solid (&color);
}
slim_hidden_def (cairo_pattern_create_rgb);

/**
 * cairo_pattern_create_rgba:
 * @red: red component of the color
 * @green: green component of the color
 * @blue: blue component of the color
 * @alpha: alpha component of the color
 *
 * Creates a new #cairo_pattern_t corresponding to a translucent color.
 * The color components are floating point numbers in the range 0 to
 * 1.  If the values passed in are outside that range, they will be
 * clamped.
 *
 * Return value: the newly created #cairo_pattern_t if successful, or
 * an error pattern in case of no memory.  The caller owns the
 * returned object and should call cairo_pattern_destroy() when
 * finished with it.
 *
 * This function will always return a valid pointer, but if an error
 * occurred the pattern status will be set to an error.  To inspect
 * the status of a pattern use cairo_pattern_status().
 **/
cairo_pattern_t *
cairo_pattern_create_rgba (double red, double green, double blue,
			   double alpha)
{
    cairo_color_t color;

    red   = _cairo_restrict_value (red,   0.0, 1.0);
    green = _cairo_restrict_value (green, 0.0, 1.0);
    blue  = _cairo_restrict_value (blue,  0.0, 1.0);
    alpha = _cairo_restrict_value (alpha, 0.0, 1.0);

    _cairo_color_init_rgba (&color, red, green, blue, alpha);

    CAIRO_MUTEX_INITIALIZE ();

    return _cairo_pattern_create_solid (&color);
}
slim_hidden_def (cairo_pattern_create_rgba);

/**
 * cairo_pattern_create_for_surface:
 * @surface: the surface
 *
 * Create a new #cairo_pattern_t for the given surface.
 *
 * Return value: the newly created #cairo_pattern_t if successful, or
 * an error pattern in case of no memory.  The caller owns the
 * returned object and should call cairo_pattern_destroy() when
 * finished with it.
 *
 * This function will always return a valid pointer, but if an error
 * occurred the pattern status will be set to an error.  To inspect
 * the status of a pattern use cairo_pattern_status().
 **/
cairo_pattern_t *
cairo_pattern_create_for_surface (cairo_surface_t *surface)
{
    cairo_surface_pattern_t *pattern;

    if (surface == NULL) {
	_cairo_error_throw (CAIRO_STATUS_NULL_POINTER);
	return (cairo_pattern_t*) &_cairo_pattern_nil_null_pointer;
    }

    if (surface->status)
	return _cairo_pattern_create_in_error (surface->status);

    pattern =
	_freed_pool_get (&freed_pattern_pool[CAIRO_PATTERN_TYPE_SURFACE]);
    if (unlikely (pattern == NULL)) {
	pattern = malloc (sizeof (cairo_surface_pattern_t));
	if (unlikely (pattern == NULL)) {
	    _cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	    return (cairo_pattern_t *)&_cairo_pattern_nil.base;
	}
    }

    CAIRO_MUTEX_INITIALIZE ();

    _cairo_pattern_init_for_surface (pattern, surface);
    CAIRO_REFERENCE_COUNT_INIT (&pattern->base.ref_count, 1);

    return &pattern->base;
}
slim_hidden_def (cairo_pattern_create_for_surface);

/**
 * cairo_pattern_create_linear:
 * @x0: x coordinate of the start point
 * @y0: y coordinate of the start point
 * @x1: x coordinate of the end point
 * @y1: y coordinate of the end point
 *
 * Create a new linear gradient #cairo_pattern_t along the line defined
 * by (x0, y0) and (x1, y1).  Before using the gradient pattern, a
 * number of color stops should be defined using
 * cairo_pattern_add_color_stop_rgb() or
 * cairo_pattern_add_color_stop_rgba().
 *
 * Note: The coordinates here are in pattern space. For a new pattern,
 * pattern space is identical to user space, but the relationship
 * between the spaces can be changed with cairo_pattern_set_matrix().
 *
 * Return value: the newly created #cairo_pattern_t if successful, or
 * an error pattern in case of no memory.  The caller owns the
 * returned object and should call cairo_pattern_destroy() when
 * finished with it.
 *
 * This function will always return a valid pointer, but if an error
 * occurred the pattern status will be set to an error.  To inspect
 * the status of a pattern use cairo_pattern_status().
 **/
cairo_pattern_t *
cairo_pattern_create_linear (double x0, double y0, double x1, double y1)
{
    cairo_linear_pattern_t *pattern;

    pattern =
	_freed_pool_get (&freed_pattern_pool[CAIRO_PATTERN_TYPE_LINEAR]);
    if (unlikely (pattern == NULL)) {
	pattern = malloc (sizeof (cairo_linear_pattern_t));
	if (unlikely (pattern == NULL)) {
	    _cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	    return (cairo_pattern_t *) &_cairo_pattern_nil.base;
	}
    }

    CAIRO_MUTEX_INITIALIZE ();

    _cairo_pattern_init_linear (pattern, x0, y0, x1, y1);
    CAIRO_REFERENCE_COUNT_INIT (&pattern->base.base.ref_count, 1);

    return &pattern->base.base;
}

/**
 * cairo_pattern_create_radial:
 * @cx0: x coordinate for the center of the start circle
 * @cy0: y coordinate for the center of the start circle
 * @radius0: radius of the start circle
 * @cx1: x coordinate for the center of the end circle
 * @cy1: y coordinate for the center of the end circle
 * @radius1: radius of the end circle
 *
 * Creates a new radial gradient #cairo_pattern_t between the two
 * circles defined by (cx0, cy0, radius0) and (cx1, cy1, radius1).  Before using the
 * gradient pattern, a number of color stops should be defined using
 * cairo_pattern_add_color_stop_rgb() or
 * cairo_pattern_add_color_stop_rgba().
 *
 * Note: The coordinates here are in pattern space. For a new pattern,
 * pattern space is identical to user space, but the relationship
 * between the spaces can be changed with cairo_pattern_set_matrix().
 *
 * Return value: the newly created #cairo_pattern_t if successful, or
 * an error pattern in case of no memory.  The caller owns the
 * returned object and should call cairo_pattern_destroy() when
 * finished with it.
 *
 * This function will always return a valid pointer, but if an error
 * occurred the pattern status will be set to an error.  To inspect
 * the status of a pattern use cairo_pattern_status().
 **/
cairo_pattern_t *
cairo_pattern_create_radial (double cx0, double cy0, double radius0,
			     double cx1, double cy1, double radius1)
{
    cairo_radial_pattern_t *pattern;

    pattern =
	_freed_pool_get (&freed_pattern_pool[CAIRO_PATTERN_TYPE_RADIAL]);
    if (unlikely (pattern == NULL)) {
	pattern = malloc (sizeof (cairo_radial_pattern_t));
	if (unlikely (pattern == NULL)) {
	    _cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	    return (cairo_pattern_t *) &_cairo_pattern_nil.base;
	}
    }

    CAIRO_MUTEX_INITIALIZE ();

    _cairo_pattern_init_radial (pattern, cx0, cy0, radius0, cx1, cy1, radius1);
    CAIRO_REFERENCE_COUNT_INIT (&pattern->base.base.ref_count, 1);

    return &pattern->base.base;
}

/**
 * cairo_pattern_reference:
 * @pattern: a #cairo_pattern_t
 *
 * Increases the reference count on @pattern by one. This prevents
 * @pattern from being destroyed until a matching call to
 * cairo_pattern_destroy() is made.
 *
 * The number of references to a #cairo_pattern_t can be get using
 * cairo_pattern_get_reference_count().
 *
 * Return value: the referenced #cairo_pattern_t.
 **/
cairo_pattern_t *
cairo_pattern_reference (cairo_pattern_t *pattern)
{
    if (pattern == NULL ||
	    CAIRO_REFERENCE_COUNT_IS_INVALID (&pattern->ref_count))
	return pattern;

    assert (CAIRO_REFERENCE_COUNT_HAS_REFERENCE (&pattern->ref_count));

    _cairo_reference_count_inc (&pattern->ref_count);

    return pattern;
}
slim_hidden_def (cairo_pattern_reference);

/**
 * cairo_pattern_get_type:
 * @pattern: a #cairo_pattern_t
 *
 * This function returns the type a pattern.
 * See #cairo_pattern_type_t for available types.
 *
 * Return value: The type of @pattern.
 *
 * Since: 1.2
 **/
cairo_pattern_type_t
cairo_pattern_get_type (cairo_pattern_t *pattern)
{
    return pattern->type;
}

/**
 * cairo_pattern_status:
 * @pattern: a #cairo_pattern_t
 *
 * Checks whether an error has previously occurred for this
 * pattern.
 *
 * Return value: %CAIRO_STATUS_SUCCESS, %CAIRO_STATUS_NO_MEMORY, or
 * %CAIRO_STATUS_PATTERN_TYPE_MISMATCH.
 **/
cairo_status_t
cairo_pattern_status (cairo_pattern_t *pattern)
{
    return pattern->status;
}

/**
 * cairo_pattern_destroy:
 * @pattern: a #cairo_pattern_t
 *
 * Decreases the reference count on @pattern by one. If the result is
 * zero, then @pattern and all associated resources are freed.  See
 * cairo_pattern_reference().
 **/
void
cairo_pattern_destroy (cairo_pattern_t *pattern)
{
    cairo_pattern_type_t type;

    if (pattern == NULL ||
	    CAIRO_REFERENCE_COUNT_IS_INVALID (&pattern->ref_count))
	return;

    assert (CAIRO_REFERENCE_COUNT_HAS_REFERENCE (&pattern->ref_count));

    if (! _cairo_reference_count_dec_and_test (&pattern->ref_count))
	return;

    type = pattern->type;
    _cairo_pattern_fini (pattern);

    /* maintain a small cache of freed patterns */
    _freed_pool_put (&freed_pattern_pool[type], pattern);
}
slim_hidden_def (cairo_pattern_destroy);

/**
 * cairo_pattern_get_reference_count:
 * @pattern: a #cairo_pattern_t
 *
 * Returns the current reference count of @pattern.
 *
 * Return value: the current reference count of @pattern.  If the
 * object is a nil object, 0 will be returned.
 *
 * Since: 1.4
 **/
unsigned int
cairo_pattern_get_reference_count (cairo_pattern_t *pattern)
{
    if (pattern == NULL ||
	    CAIRO_REFERENCE_COUNT_IS_INVALID (&pattern->ref_count))
	return 0;

    return CAIRO_REFERENCE_COUNT_GET_VALUE (&pattern->ref_count);
}

/**
 * cairo_pattern_get_user_data:
 * @pattern: a #cairo_pattern_t
 * @key: the address of the #cairo_user_data_key_t the user data was
 * attached to
 *
 * Return user data previously attached to @pattern using the
 * specified key.  If no user data has been attached with the given
 * key this function returns %NULL.
 *
 * Return value: the user data previously attached or %NULL.
 *
 * Since: 1.4
 **/
void *
cairo_pattern_get_user_data (cairo_pattern_t		 *pattern,
			     const cairo_user_data_key_t *key)
{
    return _cairo_user_data_array_get_data (&pattern->user_data,
					    key);
}

/**
 * cairo_pattern_set_user_data:
 * @pattern: a #cairo_pattern_t
 * @key: the address of a #cairo_user_data_key_t to attach the user data to
 * @user_data: the user data to attach to the #cairo_pattern_t
 * @destroy: a #cairo_destroy_func_t which will be called when the
 * #cairo_t is destroyed or when new user data is attached using the
 * same key.
 *
 * Attach user data to @pattern.  To remove user data from a surface,
 * call this function with the key that was used to set it and %NULL
 * for @data.
 *
 * Return value: %CAIRO_STATUS_SUCCESS or %CAIRO_STATUS_NO_MEMORY if a
 * slot could not be allocated for the user data.
 *
 * Since: 1.4
 **/
cairo_status_t
cairo_pattern_set_user_data (cairo_pattern_t		 *pattern,
			     const cairo_user_data_key_t *key,
			     void			 *user_data,
			     cairo_destroy_func_t	  destroy)
{
    if (CAIRO_REFERENCE_COUNT_IS_INVALID (&pattern->ref_count))
	return pattern->status;

    return _cairo_user_data_array_set_data (&pattern->user_data,
					    key, user_data, destroy);
}

/* make room for at least one more color stop */
static cairo_status_t
_cairo_pattern_gradient_grow (cairo_gradient_pattern_t *pattern)
{
    cairo_gradient_stop_t *new_stops;
    int old_size = pattern->stops_size;
    int embedded_size = ARRAY_LENGTH (pattern->stops_embedded);
    int new_size = 2 * MAX (old_size, 4);

    /* we have a local buffer at pattern->stops_embedded.  try to fulfill the request
     * from there. */
    if (old_size < embedded_size) {
	pattern->stops = pattern->stops_embedded;
	pattern->stops_size = embedded_size;
	return CAIRO_STATUS_SUCCESS;
    }

    if (CAIRO_INJECT_FAULT ())
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    assert (pattern->n_stops <= pattern->stops_size);

    if (pattern->stops == pattern->stops_embedded) {
	new_stops = _cairo_malloc_ab (new_size, sizeof (cairo_gradient_stop_t));
	if (new_stops)
	    memcpy (new_stops, pattern->stops, old_size * sizeof (cairo_gradient_stop_t));
    } else {
	new_stops = _cairo_realloc_ab (pattern->stops,
				       new_size,
				       sizeof (cairo_gradient_stop_t));
    }

    if (unlikely (new_stops == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    pattern->stops = new_stops;
    pattern->stops_size = new_size;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_pattern_add_color_stop (cairo_gradient_pattern_t *pattern,
			       double			 offset,
			       double			 red,
			       double			 green,
			       double			 blue,
			       double			 alpha)
{
    cairo_gradient_stop_t *stops;
    unsigned int	   i;

    if (pattern->n_stops >= pattern->stops_size) {
        cairo_status_t status = _cairo_pattern_gradient_grow (pattern);
	if (unlikely (status)) {
	    status = _cairo_pattern_set_error (&pattern->base, status);
	    return;
	}
    }

    stops = pattern->stops;

    for (i = 0; i < pattern->n_stops; i++)
    {
	if (offset < stops[i].offset)
	{
	    memmove (&stops[i + 1], &stops[i],
		     sizeof (cairo_gradient_stop_t) * (pattern->n_stops - i));

	    break;
	}
    }

    stops[i].offset = offset;

    stops[i].color.red   = red;
    stops[i].color.green = green;
    stops[i].color.blue  = blue;
    stops[i].color.alpha = alpha;

    stops[i].color.red_short   = _cairo_color_double_to_short (red);
    stops[i].color.green_short = _cairo_color_double_to_short (green);
    stops[i].color.blue_short  = _cairo_color_double_to_short (blue);
    stops[i].color.alpha_short = _cairo_color_double_to_short (alpha);

    pattern->n_stops++;
}

/**
 * cairo_pattern_add_color_stop_rgb:
 * @pattern: a #cairo_pattern_t
 * @offset: an offset in the range [0.0 .. 1.0]
 * @red: red component of color
 * @green: green component of color
 * @blue: blue component of color
 *
 * Adds an opaque color stop to a gradient pattern. The offset
 * specifies the location along the gradient's control vector. For
 * example, a linear gradient's control vector is from (x0,y0) to
 * (x1,y1) while a radial gradient's control vector is from any point
 * on the start circle to the corresponding point on the end circle.
 *
 * The color is specified in the same way as in cairo_set_source_rgb().
 *
 * If two (or more) stops are specified with identical offset values,
 * they will be sorted according to the order in which the stops are
 * added, (stops added earlier will compare less than stops added
 * later). This can be useful for reliably making sharp color
 * transitions instead of the typical blend.
 *
 *
 * Note: If the pattern is not a gradient pattern, (eg. a linear or
 * radial pattern), then the pattern will be put into an error status
 * with a status of %CAIRO_STATUS_PATTERN_TYPE_MISMATCH.
 **/
void
cairo_pattern_add_color_stop_rgb (cairo_pattern_t *pattern,
				  double	   offset,
				  double	   red,
				  double	   green,
				  double	   blue)
{
    if (pattern->status)
	return;

    if (pattern->type != CAIRO_PATTERN_TYPE_LINEAR &&
	pattern->type != CAIRO_PATTERN_TYPE_RADIAL)
    {
	_cairo_pattern_set_error (pattern, CAIRO_STATUS_PATTERN_TYPE_MISMATCH);
	return;
    }

    offset = _cairo_restrict_value (offset, 0.0, 1.0);
    red    = _cairo_restrict_value (red,    0.0, 1.0);
    green  = _cairo_restrict_value (green,  0.0, 1.0);
    blue   = _cairo_restrict_value (blue,   0.0, 1.0);

    _cairo_pattern_add_color_stop ((cairo_gradient_pattern_t *) pattern,
				   offset, red, green, blue, 1.0);
}

/**
 * cairo_pattern_add_color_stop_rgba:
 * @pattern: a #cairo_pattern_t
 * @offset: an offset in the range [0.0 .. 1.0]
 * @red: red component of color
 * @green: green component of color
 * @blue: blue component of color
 * @alpha: alpha component of color
 *
 * Adds a translucent color stop to a gradient pattern. The offset
 * specifies the location along the gradient's control vector. For
 * example, a linear gradient's control vector is from (x0,y0) to
 * (x1,y1) while a radial gradient's control vector is from any point
 * on the start circle to the corresponding point on the end circle.
 *
 * The color is specified in the same way as in cairo_set_source_rgba().
 *
 * If two (or more) stops are specified with identical offset values,
 * they will be sorted according to the order in which the stops are
 * added, (stops added earlier will compare less than stops added
 * later). This can be useful for reliably making sharp color
 * transitions instead of the typical blend.
 *
 * Note: If the pattern is not a gradient pattern, (eg. a linear or
 * radial pattern), then the pattern will be put into an error status
 * with a status of %CAIRO_STATUS_PATTERN_TYPE_MISMATCH.
 */
void
cairo_pattern_add_color_stop_rgba (cairo_pattern_t *pattern,
				   double	   offset,
				   double	   red,
				   double	   green,
				   double	   blue,
				   double	   alpha)
{
    if (pattern->status)
	return;

    if (pattern->type != CAIRO_PATTERN_TYPE_LINEAR &&
	pattern->type != CAIRO_PATTERN_TYPE_RADIAL)
    {
	_cairo_pattern_set_error (pattern, CAIRO_STATUS_PATTERN_TYPE_MISMATCH);
	return;
    }

    offset = _cairo_restrict_value (offset, 0.0, 1.0);
    red    = _cairo_restrict_value (red,    0.0, 1.0);
    green  = _cairo_restrict_value (green,  0.0, 1.0);
    blue   = _cairo_restrict_value (blue,   0.0, 1.0);
    alpha  = _cairo_restrict_value (alpha,  0.0, 1.0);

    _cairo_pattern_add_color_stop ((cairo_gradient_pattern_t *) pattern,
				   offset, red, green, blue, alpha);
}

/**
 * cairo_pattern_set_matrix:
 * @pattern: a #cairo_pattern_t
 * @matrix: a #cairo_matrix_t
 *
 * Sets the pattern's transformation matrix to @matrix. This matrix is
 * a transformation from user space to pattern space.
 *
 * When a pattern is first created it always has the identity matrix
 * for its transformation matrix, which means that pattern space is
 * initially identical to user space.
 *
 * Important: Please note that the direction of this transformation
 * matrix is from user space to pattern space. This means that if you
 * imagine the flow from a pattern to user space (and on to device
 * space), then coordinates in that flow will be transformed by the
 * inverse of the pattern matrix.
 *
 * For example, if you want to make a pattern appear twice as large as
 * it does by default the correct code to use is:
 *
 * <informalexample><programlisting>
 * cairo_matrix_init_scale (&amp;matrix, 0.5, 0.5);
 * cairo_pattern_set_matrix (pattern, &amp;matrix);
 * </programlisting></informalexample>
 *
 * Meanwhile, using values of 2.0 rather than 0.5 in the code above
 * would cause the pattern to appear at half of its default size.
 *
 * Also, please note the discussion of the user-space locking
 * semantics of cairo_set_source().
 **/
void
cairo_pattern_set_matrix (cairo_pattern_t      *pattern,
			  const cairo_matrix_t *matrix)
{
    cairo_matrix_t inverse;
    cairo_status_t status;

    if (pattern->status)
	return;

    if (memcmp (&pattern->matrix, matrix, sizeof (cairo_matrix_t)) == 0)
	return;

    pattern->matrix = *matrix;

    inverse = *matrix;
    status = cairo_matrix_invert (&inverse);
    if (unlikely (status))
	status = _cairo_pattern_set_error (pattern, status);
}
slim_hidden_def (cairo_pattern_set_matrix);

/**
 * cairo_pattern_get_matrix:
 * @pattern: a #cairo_pattern_t
 * @matrix: return value for the matrix
 *
 * Stores the pattern's transformation matrix into @matrix.
 **/
void
cairo_pattern_get_matrix (cairo_pattern_t *pattern, cairo_matrix_t *matrix)
{
    *matrix = pattern->matrix;
}

/**
 * cairo_pattern_set_filter:
 * @pattern: a #cairo_pattern_t
 * @filter: a #cairo_filter_t describing the filter to use for resizing
 * the pattern
 *
 * Sets the filter to be used for resizing when using this pattern.
 * See #cairo_filter_t for details on each filter.
 *
 * * Note that you might want to control filtering even when you do not
 * have an explicit #cairo_pattern_t object, (for example when using
 * cairo_set_source_surface()). In these cases, it is convenient to
 * use cairo_get_source() to get access to the pattern that cairo
 * creates implicitly. For example:
 *
 * <informalexample><programlisting>
 * cairo_set_source_surface (cr, image, x, y);
 * cairo_pattern_set_filter (cairo_get_source (cr), CAIRO_FILTER_NEAREST);
 * </programlisting></informalexample>
 **/
void
cairo_pattern_set_filter (cairo_pattern_t *pattern, cairo_filter_t filter)
{
    if (pattern->status)
	return;

    pattern->filter = filter;
}

/**
 * cairo_pattern_get_filter:
 * @pattern: a #cairo_pattern_t
 *
 * Gets the current filter for a pattern.  See #cairo_filter_t
 * for details on each filter.
 *
 * Return value: the current filter used for resizing the pattern.
 **/
cairo_filter_t
cairo_pattern_get_filter (cairo_pattern_t *pattern)
{
    return pattern->filter;
}

/**
 * cairo_pattern_set_extend:
 * @pattern: a #cairo_pattern_t
 * @extend: a #cairo_extend_t describing how the area outside of the
 * pattern will be drawn
 *
 * Sets the mode to be used for drawing outside the area of a pattern.
 * See #cairo_extend_t for details on the semantics of each extend
 * strategy.
 *
 * The default extend mode is %CAIRO_EXTEND_NONE for surface patterns
 * and %CAIRO_EXTEND_PAD for gradient patterns.
 **/
void
cairo_pattern_set_extend (cairo_pattern_t *pattern, cairo_extend_t extend)
{
    if (pattern->status)
	return;

    pattern->extend = extend;
}

/**
 * cairo_pattern_get_extend:
 * @pattern: a #cairo_pattern_t
 *
 * Gets the current extend mode for a pattern.  See #cairo_extend_t
 * for details on the semantics of each extend strategy.
 *
 * Return value: the current extend strategy used for drawing the
 * pattern.
 **/
cairo_extend_t
cairo_pattern_get_extend (cairo_pattern_t *pattern)
{
    return pattern->extend;
}
slim_hidden_def (cairo_pattern_get_extend);

void
_cairo_pattern_transform (cairo_pattern_t	*pattern,
			  const cairo_matrix_t  *ctm_inverse)
{
    if (pattern->status)
	return;

    cairo_matrix_multiply (&pattern->matrix, ctm_inverse, &pattern->matrix);
}

static void
_cairo_linear_pattern_classify (cairo_linear_pattern_t *pattern,
				double		       offset_x,
				double		       offset_y,
				int		       width,
				int		       height,
				cairo_bool_t           *is_horizontal,
				cairo_bool_t           *is_vertical)
{
    cairo_point_double_t point0, point1;
    double a, b, c, d, tx, ty;
    double scale, start, dx, dy;
    cairo_fixed_t factors[3];
    int i;

    /* To classify a pattern as horizontal or vertical, we first
     * compute the (fixed point) factors at the corners of the
     * pattern. We actually only need 3/4 corners, so we skip the
     * fourth.
     */
    point0.x = _cairo_fixed_to_double (pattern->p1.x);
    point0.y = _cairo_fixed_to_double (pattern->p1.y);
    point1.x = _cairo_fixed_to_double (pattern->p2.x);
    point1.y = _cairo_fixed_to_double (pattern->p2.y);

    _cairo_matrix_get_affine (&pattern->base.base.matrix,
			      &a, &b, &c, &d, &tx, &ty);

    dx = point1.x - point0.x;
    dy = point1.y - point0.y;
    scale = dx * dx + dy * dy;
    scale = (scale) ? 1.0 / scale : 1.0;

    start = dx * point0.x + dy * point0.y;

    for (i = 0; i < 3; i++) {
	double qx_device = (i % 2) * (width - 1) + offset_x;
	double qy_device = (i / 2) * (height - 1) + offset_y;

	/* transform fragment into pattern space */
	double qx = a * qx_device + c * qy_device + tx;
	double qy = b * qx_device + d * qy_device + ty;

	factors[i] = _cairo_fixed_from_double (((dx * qx + dy * qy) - start) * scale);
    }

    /* We consider a pattern to be vertical if the fixed point factor
     * at the two upper corners is the same. We could accept a small
     * change, but determining what change is acceptable would require
     * sorting the stops in the pattern and looking at the differences.
     *
     * Horizontal works the same way with the two left corners.
     */

    *is_vertical = factors[1] == factors[0];
    *is_horizontal = factors[2] == factors[0];
}

static cairo_int_status_t
_cairo_pattern_acquire_surface_for_gradient (const cairo_gradient_pattern_t *pattern,
					     cairo_surface_t	        *dst,
					     int			x,
					     int			y,
					     unsigned int		width,
					     unsigned int	        height,
					     cairo_surface_t	        **out,
					     cairo_surface_attributes_t *attr)
{
    cairo_image_surface_t *image;
    pixman_image_t	  *pixman_image;
    pixman_transform_t	  pixman_transform;
    cairo_status_t	  status;
    cairo_bool_t	  repeat = FALSE;
    cairo_bool_t          opaque = TRUE;

    pixman_gradient_stop_t pixman_stops_static[2];
    pixman_gradient_stop_t *pixman_stops = pixman_stops_static;
    unsigned int i;
    int clone_offset_x, clone_offset_y;
    cairo_matrix_t matrix = pattern->base.matrix;

    if (CAIRO_INJECT_FAULT ())
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    if (pattern->n_stops > ARRAY_LENGTH(pixman_stops_static)) {
	pixman_stops = _cairo_malloc_ab (pattern->n_stops,
					 sizeof(pixman_gradient_stop_t));
	if (unlikely (pixman_stops == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    for (i = 0; i < pattern->n_stops; i++) {
	pixman_stops[i].x = _cairo_fixed_16_16_from_double (pattern->stops[i].offset);
	pixman_stops[i].color.red = pattern->stops[i].color.red_short;
	pixman_stops[i].color.green = pattern->stops[i].color.green_short;
	pixman_stops[i].color.blue = pattern->stops[i].color.blue_short;
	pixman_stops[i].color.alpha = pattern->stops[i].color.alpha_short;
	if (! CAIRO_ALPHA_SHORT_IS_OPAQUE (pixman_stops[i].color.alpha))
	    opaque = FALSE;
    }

    if (pattern->base.type == CAIRO_PATTERN_TYPE_LINEAR)
    {
	cairo_linear_pattern_t *linear = (cairo_linear_pattern_t *) pattern;
	pixman_point_fixed_t p1, p2;
	cairo_fixed_t xdim, ydim;

	xdim = linear->p2.x - linear->p1.x;
	ydim = linear->p2.y - linear->p1.y;

	/*
	 * Transform the matrix to avoid overflow when converting between
	 * cairo_fixed_t and pixman_fixed_t (without incurring performance
	 * loss when the transformation is unnecessary).
	 *
	 * XXX: Consider converting out-of-range co-ordinates and transforms.
	 * Having a function to compute the required transformation to
	 * "normalize" a given bounding box would be generally useful -
	 * cf linear patterns, gradient patterns, surface patterns...
	 */
#define PIXMAN_MAX_INT ((pixman_fixed_1 >> 1) - pixman_fixed_e) /* need to ensure deltas also fit */
	if (_cairo_fixed_integer_ceil (xdim) > PIXMAN_MAX_INT ||
	    _cairo_fixed_integer_ceil (ydim) > PIXMAN_MAX_INT)
	{
	    double sf;

	    if (xdim > ydim)
		sf = PIXMAN_MAX_INT / _cairo_fixed_to_double (xdim);
	    else
		sf = PIXMAN_MAX_INT / _cairo_fixed_to_double (ydim);

	    p1.x = _cairo_fixed_16_16_from_double (_cairo_fixed_to_double (linear->p1.x) * sf);
	    p1.y = _cairo_fixed_16_16_from_double (_cairo_fixed_to_double (linear->p1.y) * sf);
	    p2.x = _cairo_fixed_16_16_from_double (_cairo_fixed_to_double (linear->p2.x) * sf);
	    p2.y = _cairo_fixed_16_16_from_double (_cairo_fixed_to_double (linear->p2.y) * sf);

	    cairo_matrix_scale (&matrix, sf, sf);
	}
	else
	{
	    p1.x = _cairo_fixed_to_16_16 (linear->p1.x);
	    p1.y = _cairo_fixed_to_16_16 (linear->p1.y);
	    p2.x = _cairo_fixed_to_16_16 (linear->p2.x);
	    p2.y = _cairo_fixed_to_16_16 (linear->p2.y);
	}

	pixman_image = pixman_image_create_linear_gradient (&p1, &p2,
							    pixman_stops,
							    pattern->n_stops);
    }
    else
    {
	cairo_radial_pattern_t *radial = (cairo_radial_pattern_t *) pattern;
	pixman_point_fixed_t c1, c2;
	pixman_fixed_t r1, r2;

	c1.x = _cairo_fixed_to_16_16 (radial->c1.x);
	c1.y = _cairo_fixed_to_16_16 (radial->c1.y);
	r1   = _cairo_fixed_to_16_16 (radial->r1);

	c2.x = _cairo_fixed_to_16_16 (radial->c2.x);
	c2.y = _cairo_fixed_to_16_16 (radial->c2.y);
	r2   = _cairo_fixed_to_16_16 (radial->r2);

	pixman_image = pixman_image_create_radial_gradient (&c1, &c2,
							    r1, r2,
							    pixman_stops,
							    pattern->n_stops);
    }

    if (pixman_stops != pixman_stops_static)
	free (pixman_stops);

    if (unlikely (pixman_image == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    if (_cairo_surface_is_image (dst))
    {
	image = (cairo_image_surface_t *)
	    _cairo_image_surface_create_for_pixman_image (pixman_image,
							  PIXMAN_a8r8g8b8);
	if (image->base.status)
	{
	    pixman_image_unref (pixman_image);
	    return image->base.status;
	}

	attr->x_offset = attr->y_offset = 0;
	attr->matrix = matrix;
	attr->extend = pattern->base.extend;
	attr->filter = CAIRO_FILTER_NEAREST;
	attr->has_component_alpha = pattern->base.has_component_alpha;

	*out = &image->base;

	return CAIRO_STATUS_SUCCESS;
    }

    if (pattern->base.type == CAIRO_PATTERN_TYPE_LINEAR) {
	cairo_bool_t is_horizontal;
	cairo_bool_t is_vertical;

	_cairo_linear_pattern_classify ((cairo_linear_pattern_t *)pattern,
					x, y, width, height,
					&is_horizontal, &is_vertical);
	if (is_horizontal) {
	    height = 1;
	    repeat = TRUE;
	}
	/* width-1 repeating patterns are quite slow with scan-line based
	 * compositing code, so we use a wider strip and spend some extra
	 * expense in computing the gradient. It's possible that for narrow
	 * gradients we'd be better off using a 2 or 4 pixel strip; the
	 * wider the gradient, the more it's worth spending extra time
	 * computing a sample.
	 */
	if (is_vertical && width > 8) {
	    width = 8;
	    repeat = TRUE;
	}
    }

    if (! pixman_image_set_filter (pixman_image, PIXMAN_FILTER_BILINEAR,
				   NULL, 0))
    {
	pixman_image_unref (pixman_image);
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    image = (cairo_image_surface_t *)
	cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
    if (image->base.status) {
	pixman_image_unref (pixman_image);
	return image->base.status;
    }

    _cairo_matrix_to_pixman_matrix (&matrix, &pixman_transform,
				    width/2., height/2.);
    if (!pixman_image_set_transform (pixman_image, &pixman_transform)) {
	cairo_surface_destroy (&image->base);
	pixman_image_unref (pixman_image);
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    switch (pattern->base.extend) {
    case CAIRO_EXTEND_NONE:
	pixman_image_set_repeat (pixman_image, PIXMAN_REPEAT_NONE);
	break;
    case CAIRO_EXTEND_REPEAT:
	pixman_image_set_repeat (pixman_image, PIXMAN_REPEAT_NORMAL);
	break;
    case CAIRO_EXTEND_REFLECT:
	pixman_image_set_repeat (pixman_image, PIXMAN_REPEAT_REFLECT);
	break;
    case CAIRO_EXTEND_PAD:
	pixman_image_set_repeat (pixman_image, PIXMAN_REPEAT_PAD);
	break;
    }

    pixman_image_composite32 (PIXMAN_OP_SRC,
                              pixman_image,
                              NULL,
                              image->pixman_image,
                              x, y,
                              0, 0,
                              0, 0,
                              width, height);

    pixman_image_unref (pixman_image);

    _cairo_debug_check_image_surface_is_defined (&image->base);

    status = _cairo_surface_clone_similar (dst, &image->base,
					   0, 0, width, height,
					   &clone_offset_x,
					   &clone_offset_y,
					   out);

    cairo_surface_destroy (&image->base);

    attr->x_offset = -x;
    attr->y_offset = -y;
    cairo_matrix_init_identity (&attr->matrix);
    attr->extend = repeat ? CAIRO_EXTEND_REPEAT : CAIRO_EXTEND_NONE;
    attr->filter = CAIRO_FILTER_NEAREST;
    attr->has_component_alpha = pattern->base.has_component_alpha;

    return status;
}

/* We maintain a small cache here, because we don't want to constantly
 * recreate surfaces for simple solid colors. */
#define MAX_SURFACE_CACHE_SIZE 16
static struct {
    struct _cairo_pattern_solid_surface_cache{
	cairo_color_t    color;
	cairo_surface_t *surface;
    } cache[MAX_SURFACE_CACHE_SIZE];
    int size;
} solid_surface_cache;

static cairo_bool_t
_cairo_pattern_solid_surface_matches (
	const struct _cairo_pattern_solid_surface_cache	    *cache,
	const cairo_solid_pattern_t			    *pattern,
	cairo_surface_t					    *dst)
{
    if (cairo_surface_get_content (cache->surface) != _cairo_color_get_content (&pattern->color))
	return FALSE;

    if (CAIRO_REFERENCE_COUNT_GET_VALUE (&cache->surface->ref_count) != 1)
	return FALSE;

    if (! _cairo_surface_is_similar (cache->surface, dst))
	return FALSE;

    return TRUE;
}

static cairo_bool_t
_cairo_pattern_solid_surface_matches_color (
	const struct _cairo_pattern_solid_surface_cache	    *cache,
	const cairo_solid_pattern_t			    *pattern,
	cairo_surface_t					    *dst)
{
    if (! _cairo_color_equal (&cache->color, &pattern->color))
	return FALSE;

    return _cairo_pattern_solid_surface_matches (cache, pattern, dst);
}

static cairo_int_status_t
_cairo_pattern_acquire_surface_for_solid (const cairo_solid_pattern_t	     *pattern,
					  cairo_surface_t	     *dst,
					  int			     x,
					  int			     y,
					  unsigned int		     width,
					  unsigned int		     height,
					  cairo_surface_t	     **out,
					  cairo_surface_attributes_t *attribs)
{
    static int i;

    cairo_surface_t *surface, *to_destroy = NULL;
    cairo_status_t   status;

    CAIRO_MUTEX_LOCK (_cairo_pattern_solid_surface_cache_lock);

    /* Check cache first */
    if (i < solid_surface_cache.size &&
	_cairo_pattern_solid_surface_matches_color (&solid_surface_cache.cache[i],
						    pattern,
						    dst))
    {
	goto DONE;
    }

    for (i = 0 ; i < solid_surface_cache.size; i++) {
	if (_cairo_pattern_solid_surface_matches_color (&solid_surface_cache.cache[i],
							pattern,
							dst))
	{
	    goto DONE;
	}
    }

    /* Choose a surface to repaint/evict */
    surface = NULL;
    if (solid_surface_cache.size == MAX_SURFACE_CACHE_SIZE) {
	i = rand () % MAX_SURFACE_CACHE_SIZE;
	surface = solid_surface_cache.cache[i].surface;

	if (_cairo_pattern_solid_surface_matches (&solid_surface_cache.cache[i],
						  pattern,
						  dst))
	{
	    /* Reuse the surface instead of evicting */
	    status = _cairo_surface_repaint_solid_pattern_surface (dst, surface, pattern);
	    if (unlikely (status))
		goto EVICT;

	    cairo_surface_reference (surface);
	}
	else
	{
	  EVICT:
	    surface = NULL;
	}
    }

    if (surface == NULL) {
	/* Not cached, need to create new */
	surface = _cairo_surface_create_solid_pattern_surface (dst, pattern);
	if (surface == NULL) {
	    status = CAIRO_INT_STATUS_UNSUPPORTED;
	    goto UNLOCK;
	}
	if (unlikely (surface->status)) {
	    status = surface->status;
	    goto UNLOCK;
	}

	if (unlikely (! _cairo_surface_is_similar (surface, dst)))
	{
	    /* In the rare event of a substitute surface being returned,
	     * don't cache the fallback.
	     */
	    *out = surface;
	    goto NOCACHE;
	}
    }

    if (i == solid_surface_cache.size)
	solid_surface_cache.size++;

    to_destroy = solid_surface_cache.cache[i].surface;
    solid_surface_cache.cache[i].surface = surface;
    solid_surface_cache.cache[i].color   = pattern->color;

DONE:
    *out = cairo_surface_reference (solid_surface_cache.cache[i].surface);

NOCACHE:
    attribs->x_offset = attribs->y_offset = 0;
    cairo_matrix_init_identity (&attribs->matrix);
    attribs->extend = CAIRO_EXTEND_REPEAT;
    attribs->filter = CAIRO_FILTER_NEAREST;
    attribs->has_component_alpha = pattern->base.has_component_alpha;

    status = CAIRO_STATUS_SUCCESS;

UNLOCK:
    CAIRO_MUTEX_UNLOCK (_cairo_pattern_solid_surface_cache_lock);

    if (to_destroy)
      cairo_surface_destroy (to_destroy);

    return status;
}

static void
_cairo_pattern_reset_solid_surface_cache (void)
{
    CAIRO_MUTEX_LOCK (_cairo_pattern_solid_surface_cache_lock);

    /* remove surfaces starting from the end so that solid_surface_cache.cache
     * is always in a consistent state when we release the mutex. */
    while (solid_surface_cache.size) {
	cairo_surface_t *surface;

	solid_surface_cache.size--;
	surface = solid_surface_cache.cache[solid_surface_cache.size].surface;
	solid_surface_cache.cache[solid_surface_cache.size].surface = NULL;

	/* release the lock to avoid the possibility of a recursive
	 * deadlock when the surface destroy closure gets called */
	CAIRO_MUTEX_UNLOCK (_cairo_pattern_solid_surface_cache_lock);
	cairo_surface_destroy (surface);
	CAIRO_MUTEX_LOCK (_cairo_pattern_solid_surface_cache_lock);
    }

    CAIRO_MUTEX_UNLOCK (_cairo_pattern_solid_surface_cache_lock);
}

static void
_extents_to_linear_parameter (const cairo_linear_pattern_t *linear,
			      const cairo_rectangle_int_t *extents,
			      double t[2])
{
    double t0, tdx, tdy;
    double p1x, p1y, pdx, pdy, invsqnorm;

    p1x = _cairo_fixed_to_double (linear->p1.x);
    p1y = _cairo_fixed_to_double (linear->p1.y);
    pdx = _cairo_fixed_to_double (linear->p2.x) - p1x;
    pdy = _cairo_fixed_to_double (linear->p2.y) - p1y;
    invsqnorm = 1.0 / (pdx * pdx + pdy * pdy);
    pdx *= invsqnorm;
    pdy *= invsqnorm;

    t0 = (extents->x - p1x) * pdx + (extents->y - p1y) * pdy;
    tdx = extents->width * pdx;
    tdy = extents->height * pdy;

    t[0] = t[1] = t0;
    if (tdx < 0)
	t[0] += tdx;
    else
	t[1] += tdx;

    if (tdy < 0)
	t[0] += tdy;
    else
	t[1] += tdy;
}

static cairo_bool_t
_linear_pattern_is_degenerate (const cairo_linear_pattern_t *linear)
{
    return linear->p1.x == linear->p2.x && linear->p1.y == linear->p2.y;
}

static cairo_bool_t
_radial_pattern_is_degenerate (const cairo_radial_pattern_t *radial)
{
    return radial->r1 == radial->r2 &&
	(radial->r1 == 0 /* && radial->r2 == 0 */ ||
	 (radial->c1.x == radial->c2.x && radial->c1.y == radial->c2.y));
}

static cairo_bool_t
_gradient_is_clear (const cairo_gradient_pattern_t *gradient,
		    const cairo_rectangle_int_t *extents)
{
    unsigned int i;

    assert (gradient->base.type == CAIRO_PATTERN_TYPE_LINEAR ||
	    gradient->base.type == CAIRO_PATTERN_TYPE_RADIAL);

    if (gradient->n_stops == 0 ||
	(gradient->base.extend == CAIRO_EXTEND_NONE &&
	 gradient->stops[0].offset == gradient->stops[gradient->n_stops - 1].offset))
	return TRUE;

    /* Check if the extents intersect the drawn part of the pattern. */
    if (gradient->base.type == CAIRO_PATTERN_TYPE_LINEAR) {
	if (gradient->base.extend == CAIRO_EXTEND_NONE) {
	    cairo_linear_pattern_t *linear = (cairo_linear_pattern_t *) gradient;
	    /* EXTEND_NONE degenerate linear gradients are clear */
	    if (_linear_pattern_is_degenerate (linear))
		return TRUE;

	    if (extents != NULL) {
		double t[2];
		_extents_to_linear_parameter (linear, extents, t);
		if ((t[0] <= 0.0 && t[1] <= 0.0) || (t[0] >= 1.0 && t[1] >= 1.0))
		    return TRUE;
	    }
	}
    } else {
	cairo_radial_pattern_t *radial = (cairo_radial_pattern_t *) gradient;
	/* degenerate radial gradients are clear */
	if (_radial_pattern_is_degenerate (radial) && FALSE)
	    return TRUE;
	/* TODO: check actual intersection */
    }

    for (i = 0; i < gradient->n_stops; i++)
	if (! CAIRO_COLOR_IS_CLEAR (&gradient->stops[i].color))
	    return FALSE;

    return TRUE;
}

/**
 * _cairo_gradient_pattern_is_solid
 *
 * Convenience function to determine whether a gradient pattern is
 * a solid color within the given extents. In this case the color
 * argument is initialized to the color the pattern represents.
 * This functions doesn't handle completely transparent gradients,
 * thus it should be called only after _cairo_pattern_is_clear has
 * returned FALSE.
 *
 * Return value: %TRUE if the pattern is a solid color.
 **/
cairo_bool_t
_cairo_gradient_pattern_is_solid (const cairo_gradient_pattern_t *gradient,
				  const cairo_rectangle_int_t *extents,
				  cairo_color_t *color)
{
    unsigned int i;

    assert (gradient->base.type == CAIRO_PATTERN_TYPE_LINEAR ||
	    gradient->base.type == CAIRO_PATTERN_TYPE_RADIAL);

    /* TODO: radial, degenerate linear */
    if (gradient->base.type == CAIRO_PATTERN_TYPE_LINEAR) {
	if (gradient->base.extend == CAIRO_EXTEND_NONE) {
	    cairo_linear_pattern_t *linear = (cairo_linear_pattern_t *) gradient;
	    double t[2];

	    /* We already know that the pattern is not clear, thus if some
	     * part of it is clear, the whole is not solid.
	     */

	    if (extents == NULL)
		return FALSE;

	    _extents_to_linear_parameter (linear, extents, t);
	    if (t[0] < 0.0 || t[1] > 1.0)
		return FALSE;
	}
    }

    for (i = 1; i < gradient->n_stops; i++)
	if (! _cairo_color_stop_equal (&gradient->stops[0].color,
				       &gradient->stops[i].color))
	    return FALSE;

    _cairo_color_init_rgba (color,
			    gradient->stops[0].color.red,
			    gradient->stops[0].color.green,
			    gradient->stops[0].color.blue,
			    gradient->stops[0].color.alpha);

    return TRUE;
}

/**
 * _cairo_pattern_is_opaque_solid
 *
 * Convenience function to determine whether a pattern is an opaque
 * (alpha==1.0) solid color pattern. This is done by testing whether
 * the pattern's alpha value when converted to a byte is 255, so if a
 * backend actually supported deep alpha channels this function might
 * not do the right thing.
 *
 * Return value: %TRUE if the pattern is an opaque, solid color.
 **/
cairo_bool_t
_cairo_pattern_is_opaque_solid (const cairo_pattern_t *pattern)
{
    cairo_solid_pattern_t *solid;

    if (pattern->type != CAIRO_PATTERN_TYPE_SOLID)
	return FALSE;

    solid = (cairo_solid_pattern_t *) pattern;

    return CAIRO_COLOR_IS_OPAQUE (&solid->color);
}

static cairo_bool_t
_surface_is_opaque (const cairo_surface_pattern_t *pattern,
		    const cairo_rectangle_int_t *r)
{
    if (pattern->surface->content & CAIRO_CONTENT_ALPHA)
	return FALSE;

    if (pattern->base.extend != CAIRO_EXTEND_NONE)
	return TRUE;

    if (r != NULL) {
	cairo_rectangle_int_t extents;

	if (! _cairo_surface_get_extents (pattern->surface, &extents))
	    return TRUE;

	if (r->x >= extents.x &&
	    r->y >= extents.y &&
	    r->x + r->width <= extents.x + extents.width &&
	    r->y + r->height <= extents.y + extents.height)
	{
	    return TRUE;
	}
    }

    return FALSE;
}

static cairo_bool_t
_surface_is_clear (const cairo_surface_pattern_t *pattern)
{
    cairo_rectangle_int_t extents;

    if (_cairo_surface_get_extents (pattern->surface, &extents) &&
	(extents.width == 0 || extents.height == 0))
	return TRUE;

    return pattern->surface->is_clear &&
	pattern->surface->content & CAIRO_CONTENT_ALPHA;
}

static cairo_bool_t
_gradient_is_opaque (const cairo_gradient_pattern_t *gradient,
		     const cairo_rectangle_int_t *extents)
{
    unsigned int i;

    assert (gradient->base.type == CAIRO_PATTERN_TYPE_LINEAR ||
	    gradient->base.type == CAIRO_PATTERN_TYPE_RADIAL);

    if (gradient->n_stops == 0 ||
	(gradient->base.extend == CAIRO_EXTEND_NONE &&
	 gradient->stops[0].offset == gradient->stops[gradient->n_stops - 1].offset))
	return FALSE;

    if (gradient->base.type == CAIRO_PATTERN_TYPE_LINEAR) {
	if (gradient->base.extend == CAIRO_EXTEND_NONE) {
	    double t[2];
	    cairo_linear_pattern_t *linear = (cairo_linear_pattern_t *) gradient;

	    /* EXTEND_NONE degenerate radial gradients are clear */
	    if (_linear_pattern_is_degenerate (linear))
		return FALSE;

	    if (extents == NULL)
		return FALSE;

	    _extents_to_linear_parameter (linear, extents, t);
	    if (t[0] < 0.0 || t[1] > 1.0)
		return FALSE;
	}
    }

    for (i = 0; i < gradient->n_stops; i++)
	if (! CAIRO_COLOR_IS_OPAQUE (&gradient->stops[i].color))
	    return FALSE;

    return TRUE;
}

/**
 * _cairo_pattern_is_opaque
 *
 * Convenience function to determine whether a pattern is an opaque
 * pattern (of any type). The same caveats that apply to
 * _cairo_pattern_is_opaque_solid apply here as well.
 *
 * Return value: %TRUE if the pattern is a opaque.
 **/
cairo_bool_t
_cairo_pattern_is_opaque (const cairo_pattern_t *abstract_pattern,
			  const cairo_rectangle_int_t *extents)
{
    const cairo_pattern_union_t *pattern;

    if (abstract_pattern->has_component_alpha)
	return FALSE;

    pattern = (cairo_pattern_union_t *) abstract_pattern;
    switch (pattern->base.type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	return _cairo_pattern_is_opaque_solid (abstract_pattern);
    case CAIRO_PATTERN_TYPE_SURFACE:
	return _surface_is_opaque (&pattern->surface, extents);
    case CAIRO_PATTERN_TYPE_LINEAR:
    case CAIRO_PATTERN_TYPE_RADIAL:
	return _gradient_is_opaque (&pattern->gradient.base, extents);
    }

    ASSERT_NOT_REACHED;
    return FALSE;
}

cairo_bool_t
_cairo_pattern_is_clear (const cairo_pattern_t *abstract_pattern)
{
    const cairo_pattern_union_t *pattern;

    if (abstract_pattern->has_component_alpha)
	return FALSE;

    pattern = (cairo_pattern_union_t *) abstract_pattern;
    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	return CAIRO_COLOR_IS_CLEAR (&pattern->solid.color);
    case CAIRO_PATTERN_TYPE_SURFACE:
	return _surface_is_clear (&pattern->surface);
    case CAIRO_PATTERN_TYPE_LINEAR:
    case CAIRO_PATTERN_TYPE_RADIAL:
	return _gradient_is_clear (&pattern->gradient.base, NULL);
    }

    ASSERT_NOT_REACHED;
    return FALSE;
}

/**
 * _cairo_pattern_analyze_filter:
 * @pattern: surface pattern
 * @pad_out: location to store necessary padding in the source image, or %NULL
 * Returns: the optimized #cairo_filter_t to use with @pattern.
 *
 * Analyze the filter to determine how much extra needs to be sampled
 * from the source image to account for the filter radius and whether
 * we can optimize the filter to a simpler value.
 *
 * XXX: We don't actually have any way of querying the backend for
 *      the filter radius, so we just guess base on what we know that
 *      backends do currently (see bug #10508)
 */
cairo_filter_t
_cairo_pattern_analyze_filter (const cairo_pattern_t	*pattern,
			       double			*pad_out)
{
    double pad;
    cairo_filter_t optimized_filter;

    switch (pattern->filter) {
    case CAIRO_FILTER_GOOD:
    case CAIRO_FILTER_BEST:
    case CAIRO_FILTER_BILINEAR:
	/* If source pixels map 1:1 onto destination pixels, we do
	 * not need to filter (and do not want to filter, since it
	 * will cause blurriness)
	 */
	if (_cairo_matrix_is_pixel_exact (&pattern->matrix)) {
	    pad = 0.;
	    optimized_filter = CAIRO_FILTER_NEAREST;
	} else {
	    /* 0.5 is enough for a bilinear filter. It's possible we
	     * should defensively use more for CAIRO_FILTER_BEST, but
	     * without a single example, it's hard to know how much
	     * more would be defensive...
	     */
	    pad = 0.5;
	    optimized_filter = pattern->filter;
	}
	break;

    case CAIRO_FILTER_FAST:
    case CAIRO_FILTER_NEAREST:
    case CAIRO_FILTER_GAUSSIAN:
    default:
	pad = 0.;
	optimized_filter = pattern->filter;
	break;
    }

    if (pad_out)
	*pad_out = pad;

    return optimized_filter;
}


static double
_pixman_nearest_sample (double d)
{
    return ceil (d - .5);
}

static cairo_int_status_t
_cairo_pattern_acquire_surface_for_surface (const cairo_surface_pattern_t   *pattern,
					    cairo_surface_t	       *dst,
					    int			       x,
					    int			       y,
					    unsigned int	       width,
					    unsigned int	       height,
					    unsigned int	       flags,
					    cairo_surface_t	       **out,
					    cairo_surface_attributes_t *attr)
{
    cairo_surface_t *surface;
    cairo_rectangle_int_t extents;
    cairo_rectangle_int_t sampled_area;
    double x1, y1, x2, y2;
    int tx, ty;
    double pad;
    cairo_bool_t is_identity;
    cairo_bool_t is_empty;
    cairo_bool_t is_bounded;
    cairo_int_status_t status;

    surface = cairo_surface_reference (pattern->surface);

    is_identity = FALSE;
    attr->matrix = pattern->base.matrix;
    attr->extend = pattern->base.extend;
    attr->filter = _cairo_pattern_analyze_filter (&pattern->base, &pad);
    attr->has_component_alpha = pattern->base.has_component_alpha;

    attr->x_offset = attr->y_offset = tx = ty = 0;
    if (_cairo_matrix_is_integer_translation (&attr->matrix, &tx, &ty)) {
	cairo_matrix_init_identity (&attr->matrix);
	attr->x_offset = tx;
	attr->y_offset = ty;
	is_identity = TRUE;
    } else if (attr->filter == CAIRO_FILTER_NEAREST) {
	/*
	 * For NEAREST, we can remove the fractional translation component
	 * from the transformation - this ensures that the pattern will always
	 * hit fast-paths in the backends for simple transformations that
	 * become (almost) identity, without loss of quality.
	 */
	attr->matrix.x0 = 0;
	attr->matrix.y0 = 0;
	if (_cairo_matrix_is_pixel_exact (&attr->matrix)) {
	    /* The rounding here is rather peculiar as it needs to match the
	     * rounding performed on the sample coordinate used by pixman.
	     */
	    attr->matrix.x0 = _pixman_nearest_sample (pattern->base.matrix.x0);
	    attr->matrix.y0 = _pixman_nearest_sample (pattern->base.matrix.y0);
	} else {
	    attr->matrix.x0 = pattern->base.matrix.x0;
	    attr->matrix.y0 = pattern->base.matrix.y0;
	}

	if (_cairo_matrix_is_integer_translation (&attr->matrix, &tx, &ty)) {
	    cairo_matrix_init_identity (&attr->matrix);
	    attr->x_offset = tx;
	    attr->y_offset = ty;
	    is_identity = TRUE;
	}
    }

    /* XXX: Hack:
     *
     * The way we currently support CAIRO_EXTEND_REFLECT is to create
     * an image twice bigger on each side, and create a pattern of four
     * images such that the new image, when repeated, has the same effect
     * of reflecting the original pattern.
     */
    if (flags & CAIRO_PATTERN_ACQUIRE_NO_REFLECT &&
	attr->extend == CAIRO_EXTEND_REFLECT)
    {
	cairo_t *cr;
	cairo_surface_t *src;
	int w, h;

	is_bounded = _cairo_surface_get_extents (surface, &extents);
	assert (is_bounded);

	status = _cairo_surface_clone_similar (dst, surface,
					       extents.x, extents.y,
					       extents.width, extents.height,
					       &extents.x, &extents.y, &src);
	if (unlikely (status))
	    goto BAIL;

	w = 2 * extents.width;
	h = 2 * extents.height;

	if (is_identity) {
	    attr->x_offset = -x;
	    x += tx;
	    while (x <= -w)
		x += w;
	    while (x >= w)
		x -= w;
	    extents.x += x;
	    tx = x = 0;

	    attr->y_offset = -y;
	    y += ty;
	    while (y <= -h)
		y += h;
	    while (y >= h)
		y -= h;
	    extents.y += y;
	    ty = y = 0;
	}

	cairo_surface_destroy (surface);
	surface = _cairo_surface_create_similar_solid (dst,
						       dst->content, w, h,
						       CAIRO_COLOR_TRANSPARENT,
						       FALSE);
	if (surface == NULL)
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	if (unlikely (surface->status)) {
	    cairo_surface_destroy (src);
	    return surface->status;
	}

	surface->device_transform = pattern->surface->device_transform;
	surface->device_transform_inverse = pattern->surface->device_transform_inverse;

	cr = cairo_create (surface);

	cairo_set_source_surface (cr, src, -extents.x, -extents.y);
	cairo_paint (cr);

	cairo_scale (cr, -1, +1);
	cairo_set_source_surface (cr, src, extents.x-w, -extents.y);
	cairo_paint (cr);
	cairo_set_source_surface (cr, src, extents.x, -extents.y);
	cairo_paint (cr);

	cairo_scale (cr, +1, -1);
	cairo_set_source_surface (cr, src, extents.x-w, extents.y-h);
	cairo_paint (cr);
	cairo_set_source_surface (cr, src, extents.x, extents.y-h);
	cairo_paint (cr);
	cairo_set_source_surface (cr, src, extents.x-w, extents.y);
	cairo_paint (cr);
	cairo_set_source_surface (cr, src, extents.x, extents.y);
	cairo_paint (cr);

	cairo_scale (cr, -1, +1);
	cairo_set_source_surface (cr, src, -extents.x, extents.y-h);
	cairo_paint (cr);
	cairo_set_source_surface (cr, src, -extents.x, extents.y);
	cairo_paint (cr);

	status = cairo_status (cr);
	cairo_destroy (cr);

	cairo_surface_destroy (src);

	if (unlikely (status))
	    goto BAIL;

	attr->extend = CAIRO_EXTEND_REPEAT;
    }

    /* We first transform the rectangle to the coordinate space of the
     * source surface so that we only need to clone that portion of the
     * surface that will be read.
     */
    x1 = x;
    y1 = y;
    x2 = x + (int) width;
    y2 = y + (int) height;
    if (! is_identity) {
	_cairo_matrix_transform_bounding_box (&attr->matrix,
					      &x1, &y1, &x2, &y2,
					      NULL);
    }

    sampled_area.x = floor (x1 - pad);
    sampled_area.y = floor (y1 - pad);
    sampled_area.width  = ceil (x2 + pad) - sampled_area.x;
    sampled_area.height = ceil (y2 + pad) - sampled_area.y;

    sampled_area.x += tx;
    sampled_area.y += ty;

    if ( _cairo_surface_get_extents (surface, &extents)) {
	if (attr->extend == CAIRO_EXTEND_NONE) {
	    /* Never acquire a larger area than the source itself */
	    is_empty = _cairo_rectangle_intersect (&extents, &sampled_area);
	} else {
	    int trim = 0;

	    if (sampled_area.x >= extents.x &&
		sampled_area.x + (int) sampled_area.width <= extents.x + (int) extents.width)
	    {
		/* source is horizontally contained within extents, trim */
		extents.x = sampled_area.x;
		extents.width = sampled_area.width;
		trim |= 0x1;
	    }

	    if (sampled_area.y >= extents.y &&
		sampled_area.y + (int) sampled_area.height <= extents.y + (int) extents.height)
	    {
		/* source is vertically contained within extents, trim */
		extents.y = sampled_area.y;
		extents.height = sampled_area.height;
		trim |= 0x2;
	    }

	    if (trim == 0x3) {
		/* source is wholly contained within extents, drop the REPEAT */
		attr->extend = CAIRO_EXTEND_NONE;
	    }

	    is_empty = extents.width == 0 || extents.height == 0;
	}
    }

    /* XXX can we use is_empty? */

    status = _cairo_surface_clone_similar (dst, surface,
					   extents.x, extents.y,
					   extents.width, extents.height,
					   &x, &y, out);
    if (unlikely (status))
	goto BAIL;

    if (x != 0 || y != 0) {
	if (is_identity) {
	    attr->x_offset -= x;
	    attr->y_offset -= y;
	} else {
	    cairo_matrix_t m;

	    x -= attr->x_offset;
	    y -= attr->y_offset;
	    attr->x_offset = 0;
	    attr->y_offset = 0;

	    cairo_matrix_init_translate (&m, -x, -y);
	    cairo_matrix_multiply (&attr->matrix, &attr->matrix, &m);
	}
    }

    /* reduce likelihood of range overflow with large downscaling */
    if (! is_identity) {
	cairo_matrix_t m;
	cairo_status_t invert_status;

	m = attr->matrix;
	invert_status = cairo_matrix_invert (&m);
	assert (invert_status == CAIRO_STATUS_SUCCESS);

	if (m.x0 != 0. || m.y0 != 0.) {
	    /* pixman also limits the [xy]_offset to 16 bits so evenly
	     * spread the bits between the two.
	     */
	    x = floor (m.x0 / 2);
	    y = floor (m.y0 / 2);
	    attr->x_offset -= x;
	    attr->y_offset -= y;
	    cairo_matrix_init_translate (&m, x, y);
	    cairo_matrix_multiply (&attr->matrix, &m, &attr->matrix);
	}
    }

  BAIL:
    cairo_surface_destroy (surface);
    return status;
}

/**
 * _cairo_pattern_acquire_surface:
 * @pattern: a #cairo_pattern_t
 * @dst: destination surface
 * @x: X coordinate in source corresponding to left side of destination area
 * @y: Y coordinate in source corresponding to top side of destination area
 * @width: width of destination area
 * @height: height of destination area
 * @surface_out: location to store a pointer to a surface
 * @attributes: surface attributes that destination backend should apply to
 * the returned surface
 *
 * A convenience function to obtain a surface to use as the source for
 * drawing on @dst.
 *
 * Note that this function is only suitable for use when the destination
 * surface is pixel based and 1 device unit maps to one pixel.
 *
 * Return value: %CAIRO_STATUS_SUCCESS if a surface was stored in @surface_out.
 **/
cairo_int_status_t
_cairo_pattern_acquire_surface (const cairo_pattern_t	   *pattern,
				cairo_surface_t		   *dst,
				int			   x,
				int			   y,
				unsigned int		   width,
				unsigned int		   height,
				unsigned int		   flags,
				cairo_surface_t		   **surface_out,
				cairo_surface_attributes_t *attributes)
{
    if (unlikely (pattern->status)) {
	*surface_out = NULL;
	return pattern->status;
    }

    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	return _cairo_pattern_acquire_surface_for_solid ((cairo_solid_pattern_t *) pattern,
							 dst, x, y, width, height,
							 surface_out,
							 attributes);

    case CAIRO_PATTERN_TYPE_LINEAR:
    case CAIRO_PATTERN_TYPE_RADIAL:
	return _cairo_pattern_acquire_surface_for_gradient ((cairo_gradient_pattern_t *) pattern,
							    dst, x, y, width, height,
							    surface_out,
							    attributes);

    case CAIRO_PATTERN_TYPE_SURFACE:
	return _cairo_pattern_acquire_surface_for_surface ((cairo_surface_pattern_t *) pattern,
							   dst, x, y, width, height,
							   flags,
							   surface_out,
							   attributes);

    default:
	ASSERT_NOT_REACHED;
	return _cairo_error (CAIRO_STATUS_PATTERN_TYPE_MISMATCH);
    }
}

/**
 * _cairo_pattern_release_surface:
 * @pattern: a #cairo_pattern_t
 * @surface: a surface obtained by _cairo_pattern_acquire_surface
 * @attributes: attributes obtained by _cairo_pattern_acquire_surface
 *
 * Releases resources obtained by _cairo_pattern_acquire_surface.
 **/
void
_cairo_pattern_release_surface (const cairo_pattern_t *pattern,
				cairo_surface_t		   *surface,
				cairo_surface_attributes_t *attributes)
{
    cairo_surface_destroy (surface);
}

cairo_int_status_t
_cairo_pattern_acquire_surfaces (const cairo_pattern_t	    *src,
				 const cairo_pattern_t	    *mask,
				 cairo_surface_t	    *dst,
				 int			    src_x,
				 int			    src_y,
				 int			    mask_x,
				 int			    mask_y,
				 unsigned int		    width,
				 unsigned int		    height,
				 unsigned int		    flags,
				 cairo_surface_t	    **src_out,
				 cairo_surface_t	    **mask_out,
				 cairo_surface_attributes_t *src_attributes,
				 cairo_surface_attributes_t *mask_attributes)
{
    cairo_int_status_t	  status;
    cairo_pattern_union_t src_tmp;

    if (unlikely (src->status))
	return src->status;
    if (unlikely (mask != NULL && mask->status))
	return mask->status;

    /* If src and mask are both solid, then the mask alpha can be
     * combined into src and mask can be ignored. */

    if (src->type == CAIRO_PATTERN_TYPE_SOLID &&
	mask &&
	! mask->has_component_alpha &&
	mask->type == CAIRO_PATTERN_TYPE_SOLID)
    {
	cairo_color_t combined;
	cairo_solid_pattern_t *src_solid = (cairo_solid_pattern_t *) src;
	cairo_solid_pattern_t *mask_solid = (cairo_solid_pattern_t *) mask;

	combined = src_solid->color;
	_cairo_color_multiply_alpha (&combined, mask_solid->color.alpha);

	_cairo_pattern_init_solid (&src_tmp.solid, &combined);

	src = &src_tmp.base;
	mask = NULL;
    }

    status = _cairo_pattern_acquire_surface (src, dst,
					     src_x, src_y,
					     width, height,
					     flags,
					     src_out, src_attributes);
    if (unlikely (status))
	goto BAIL;

    if (mask == NULL) {
	*mask_out = NULL;
	goto BAIL;
    }

    status = _cairo_pattern_acquire_surface (mask, dst,
					     mask_x, mask_y,
					     width, height,
					     flags,
					     mask_out, mask_attributes);
    if (unlikely (status))
	_cairo_pattern_release_surface (src, *src_out, src_attributes);

  BAIL:
    if (src == &src_tmp.base)
	_cairo_pattern_fini (&src_tmp.base);

    return status;
}

/**
 * _cairo_pattern_get_extents:
 *
 * Return the "target-space" extents of @pattern in @extents.
 *
 * For unbounded patterns, the @extents will be initialized with
 * "infinite" extents, (minimum and maximum fixed-point values).
 *
 * XXX: Currently, bounded gradient patterns will also return
 * "infinite" extents, though it would be possible to optimize these
 * with a little more work.
 **/
void
_cairo_pattern_get_extents (const cairo_pattern_t         *pattern,
			    cairo_rectangle_int_t         *extents)
{
    double x1, y1, x2, y2;
    cairo_status_t status;

    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	goto UNBOUNDED;

    case CAIRO_PATTERN_TYPE_SURFACE:
	{
	    cairo_rectangle_int_t surface_extents;
	    const cairo_surface_pattern_t *surface_pattern =
		(const cairo_surface_pattern_t *) pattern;
	    cairo_surface_t *surface = surface_pattern->surface;
	    double pad;

	    if (! _cairo_surface_get_extents (surface, &surface_extents))
		goto UNBOUNDED;

	    if (surface_extents.width == 0 || surface_extents.height == 0)
		goto EMPTY;

	    if (pattern->extend != CAIRO_EXTEND_NONE)
		goto UNBOUNDED;

	    /* The filter can effectively enlarge the extents of the
	     * pattern, so extend as necessary.
	     */
	    _cairo_pattern_analyze_filter (&surface_pattern->base, &pad);
	    x1 = surface_extents.x - pad;
	    y1 = surface_extents.y - pad;
	    x2 = surface_extents.x + (int) surface_extents.width  + pad;
	    y2 = surface_extents.y + (int) surface_extents.height + pad;
	}
	break;

    case CAIRO_PATTERN_TYPE_RADIAL:
	{
	    const cairo_radial_pattern_t *radial =
		(const cairo_radial_pattern_t *) pattern;
	    double cx1, cy1;
	    double cx2, cy2;
	    double r, D;

	    if (radial->r1 == 0 && radial->r2 == 0)
		goto EMPTY;

	    cx1 = _cairo_fixed_to_double (radial->c1.x);
	    cy1 = _cairo_fixed_to_double (radial->c1.y);
	    r = _cairo_fixed_to_double (radial->r1);
	    x1 = cx1 - r; x2 = cx1 + r;
	    y1 = cy1 - r; y2 = cy1 + r;

	    cx2 = _cairo_fixed_to_double (radial->c2.x);
	    cy2 = _cairo_fixed_to_double (radial->c2.y);
	    r = fabs (_cairo_fixed_to_double (radial->r2));

	    if (pattern->extend != CAIRO_EXTEND_NONE)
		goto UNBOUNDED;

	    /* We need to be careful, as if the circles are not
	     * self-contained, then the solution is actually unbounded.
	     */
	    D = (cx1-cx2)*(cx1-cx2) + (cy1-cy2)*(cy1-cy2);
	    if (D > r*r - 1e-5)
		goto UNBOUNDED;

	    if (cx2 - r < x1)
		x1 = cx2 - r;
	    if (cx2 + r > x2)
		x2 = cx2 + r;

	    if (cy2 - r < y1)
		y1 = cy2 - r;
	    if (cy2 + r > y2)
		y2 = cy2 + r;
	}
	break;

    case CAIRO_PATTERN_TYPE_LINEAR:
	{
	    const cairo_linear_pattern_t *linear =
		(const cairo_linear_pattern_t *) pattern;

	    if (pattern->extend != CAIRO_EXTEND_NONE)
		goto UNBOUNDED;

	    if (linear->p1.x == linear->p2.x && linear->p1.y == linear->p2.y)
		goto EMPTY;

	    if (pattern->matrix.xy != 0. || pattern->matrix.yx != 0.)
		goto UNBOUNDED;

	    if (linear->p1.x == linear->p2.x) {
		x1 = -HUGE_VAL;
		x2 = HUGE_VAL;
		y1 = _cairo_fixed_to_double (MIN (linear->p1.y, linear->p2.y));
		y2 = _cairo_fixed_to_double (MAX (linear->p1.y, linear->p2.y));
	    } else if (linear->p1.y == linear->p2.y) {
		x1 = _cairo_fixed_to_double (MIN (linear->p1.x, linear->p2.x));
		x2 = _cairo_fixed_to_double (MAX (linear->p1.x, linear->p2.x));
		y1 = -HUGE_VAL;
		y2 = HUGE_VAL;
	    } else {
		goto  UNBOUNDED;
	    }
	}
	break;

    default:
	ASSERT_NOT_REACHED;
    }

    if (_cairo_matrix_is_translation (&pattern->matrix)) {
	x1 -= pattern->matrix.x0; x2 -= pattern->matrix.x0;
	y1 -= pattern->matrix.y0; y2 -= pattern->matrix.y0;
    } else {
	cairo_matrix_t imatrix;

	imatrix = pattern->matrix;
	status = cairo_matrix_invert (&imatrix);
	/* cairo_pattern_set_matrix ensures the matrix is invertible */
	assert (status == CAIRO_STATUS_SUCCESS);

	_cairo_matrix_transform_bounding_box (&imatrix,
					      &x1, &y1, &x2, &y2,
					      NULL);
    }

    x1 = floor (x1);
    if (x1 < CAIRO_RECT_INT_MIN)
	x1 = CAIRO_RECT_INT_MIN;
    y1 = floor (y1);
    if (y1 < CAIRO_RECT_INT_MIN)
	y1 = CAIRO_RECT_INT_MIN;

    x2 = ceil (x2);
    if (x2 > CAIRO_RECT_INT_MAX)
	x2 = CAIRO_RECT_INT_MAX;
    y2 = ceil (y2);
    if (y2 > CAIRO_RECT_INT_MAX)
	y2 = CAIRO_RECT_INT_MAX;

    extents->x = x1; extents->width  = x2 - x1;
    extents->y = y1; extents->height = y2 - y1;
    return;

  UNBOUNDED:
    /* unbounded patterns -> 'infinite' extents */
    _cairo_unbounded_rectangle_init (extents);
    return;

  EMPTY:
    extents->x = extents->y = 0;
    extents->width = extents->height = 0;
    return;
}


static unsigned long
_cairo_solid_pattern_hash (unsigned long hash,
			   const cairo_pattern_t *pattern)
{
    const cairo_solid_pattern_t *solid = (cairo_solid_pattern_t *) pattern;

    hash = _cairo_hash_bytes (hash, &solid->color, sizeof (solid->color));

    return hash;
}

static unsigned long
_cairo_gradient_color_stops_hash (unsigned long hash,
				  const cairo_gradient_pattern_t *gradient)
{
    unsigned int n;

    hash = _cairo_hash_bytes (hash,
			      &gradient->n_stops,
			      sizeof (gradient->n_stops));

    for (n = 0; n < gradient->n_stops; n++) {
	hash = _cairo_hash_bytes (hash,
				  &gradient->stops[n].offset,
				  sizeof (double));
	hash = _cairo_hash_bytes (hash,
				  &gradient->stops[n].color,
				  sizeof (cairo_color_t));
    }

    return hash;
}

unsigned long
_cairo_linear_pattern_hash (unsigned long hash,
			    const cairo_linear_pattern_t *linear)
{
    hash = _cairo_hash_bytes (hash, &linear->p1, sizeof (linear->p1));
    hash = _cairo_hash_bytes (hash, &linear->p2, sizeof (linear->p2));

    return _cairo_gradient_color_stops_hash (hash, &linear->base);
}

unsigned long
_cairo_radial_pattern_hash (unsigned long hash,
			    const cairo_radial_pattern_t *radial)
{
    hash = _cairo_hash_bytes (hash, &radial->c1, sizeof (radial->c1));
    hash = _cairo_hash_bytes (hash, &radial->r1, sizeof (radial->r1));
    hash = _cairo_hash_bytes (hash, &radial->c2, sizeof (radial->c2));
    hash = _cairo_hash_bytes (hash, &radial->r2, sizeof (radial->r2));

    return _cairo_gradient_color_stops_hash (hash, &radial->base);
}

static unsigned long
_cairo_surface_pattern_hash (unsigned long hash,
			     const cairo_pattern_t *pattern)
{
    const cairo_surface_pattern_t *surface = (cairo_surface_pattern_t *) pattern;

    hash ^= surface->surface->unique_id;

    return hash;
}

unsigned long
_cairo_pattern_hash (const cairo_pattern_t *pattern)
{
    unsigned long hash = _CAIRO_HASH_INIT_VALUE;

    if (pattern->status)
	return 0;

    hash = _cairo_hash_bytes (hash, &pattern->type, sizeof (pattern->type));
    if (pattern->type != CAIRO_PATTERN_TYPE_SOLID) {
	hash = _cairo_hash_bytes (hash,
				  &pattern->matrix, sizeof (pattern->matrix));
	hash = _cairo_hash_bytes (hash,
				  &pattern->filter, sizeof (pattern->filter));
	hash = _cairo_hash_bytes (hash,
				  &pattern->extend, sizeof (pattern->extend));
	hash = _cairo_hash_bytes (hash,
				  &pattern->has_component_alpha,
				  sizeof (pattern->has_component_alpha));
    }

    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	return _cairo_solid_pattern_hash (hash, pattern);
    case CAIRO_PATTERN_TYPE_LINEAR:
	return _cairo_linear_pattern_hash (hash, (cairo_linear_pattern_t *) pattern);
    case CAIRO_PATTERN_TYPE_RADIAL:
	return _cairo_radial_pattern_hash (hash, (cairo_radial_pattern_t *) pattern);
    case CAIRO_PATTERN_TYPE_SURFACE:
	return _cairo_surface_pattern_hash (hash, pattern);
    default:
	ASSERT_NOT_REACHED;
	return FALSE;
    }
}

static unsigned long
_cairo_gradient_pattern_color_stops_size (const cairo_pattern_t *pattern)
{
    cairo_gradient_pattern_t *gradient = (cairo_gradient_pattern_t *) pattern;

    return gradient->n_stops * (sizeof (double) + sizeof (cairo_color_t));
}

unsigned long
_cairo_pattern_size (const cairo_pattern_t *pattern)
{
    if (pattern->status)
	return 0;

    /* XXX */
    switch (pattern->type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	return sizeof (cairo_solid_pattern_t);
	break;
    case CAIRO_PATTERN_TYPE_SURFACE:
	return sizeof (cairo_surface_pattern_t);
	break;
    case CAIRO_PATTERN_TYPE_LINEAR:
	return sizeof (cairo_linear_pattern_t) +
	    _cairo_gradient_pattern_color_stops_size (pattern);
	break;
    case CAIRO_PATTERN_TYPE_RADIAL:
	return sizeof (cairo_radial_pattern_t) +
	    _cairo_gradient_pattern_color_stops_size (pattern);
    default:
	ASSERT_NOT_REACHED;
	return 0;
    }
}


static cairo_bool_t
_cairo_solid_pattern_equal (const cairo_pattern_t *A,
			    const cairo_pattern_t *B)
{
    const cairo_solid_pattern_t *a = (cairo_solid_pattern_t *) A;
    const cairo_solid_pattern_t *b = (cairo_solid_pattern_t *) B;

    return _cairo_color_equal (&a->color, &b->color);
}

static cairo_bool_t
_cairo_gradient_color_stops_equal (const cairo_gradient_pattern_t *a,
				   const cairo_gradient_pattern_t *b)
{
    unsigned int n;

    if (a->n_stops != b->n_stops)
	return FALSE;

    for (n = 0; n < a->n_stops; n++) {
	if (a->stops[n].offset != b->stops[n].offset)
	    return FALSE;
	if (! _cairo_color_stop_equal (&a->stops[n].color, &b->stops[n].color))
	    return FALSE;
    }

    return TRUE;
}

cairo_bool_t
_cairo_linear_pattern_equal (const cairo_linear_pattern_t *a,
			     const cairo_linear_pattern_t *b)
{
    if (a->p1.x != b->p1.x)
	return FALSE;

    if (a->p1.y != b->p1.y)
	return FALSE;

    if (a->p2.x != b->p2.x)
	return FALSE;

    if (a->p2.y != b->p2.y)
	return FALSE;

    return _cairo_gradient_color_stops_equal (&a->base, &b->base);
}

cairo_bool_t
_cairo_radial_pattern_equal (const cairo_radial_pattern_t *a,
			     const cairo_radial_pattern_t *b)
{
    if (a->c1.x != b->c1.x)
	return FALSE;

    if (a->c1.y != b->c1.y)
	return FALSE;

    if (a->r1 != b->r1)
	return FALSE;

    if (a->c2.x != b->c2.x)
	return FALSE;

    if (a->c2.y != b->c2.y)
	return FALSE;

    if (a->r2 != b->r2)
	return FALSE;

    return _cairo_gradient_color_stops_equal (&a->base, &b->base);
}

static cairo_bool_t
_cairo_surface_pattern_equal (const cairo_pattern_t *A,
			      const cairo_pattern_t *B)
{
    const cairo_surface_pattern_t *a = (cairo_surface_pattern_t *) A;
    const cairo_surface_pattern_t *b = (cairo_surface_pattern_t *) B;

    return a->surface->unique_id == b->surface->unique_id;
}

cairo_bool_t
_cairo_pattern_equal (const cairo_pattern_t *a, const cairo_pattern_t *b)
{
    if (a->status || b->status)
	return FALSE;

    if (a == b)
	return TRUE;

    if (a->type != b->type)
	return FALSE;

    if (a->has_component_alpha != b->has_component_alpha)
	return FALSE;

    if (a->type != CAIRO_PATTERN_TYPE_SOLID) {
	if (memcmp (&a->matrix, &b->matrix, sizeof (cairo_matrix_t)))
	    return FALSE;

	if (a->filter != b->filter)
	    return FALSE;

	if (a->extend != b->extend)
	    return FALSE;
    }

    switch (a->type) {
    case CAIRO_PATTERN_TYPE_SOLID:
	return _cairo_solid_pattern_equal (a, b);
    case CAIRO_PATTERN_TYPE_LINEAR:
	return _cairo_linear_pattern_equal ((cairo_linear_pattern_t *) a,
					    (cairo_linear_pattern_t *) b);
    case CAIRO_PATTERN_TYPE_RADIAL:
	return _cairo_radial_pattern_equal ((cairo_radial_pattern_t *) a,
					    (cairo_radial_pattern_t *) b);
    case CAIRO_PATTERN_TYPE_SURFACE:
	return _cairo_surface_pattern_equal (a, b);
    default:
	ASSERT_NOT_REACHED;
	return FALSE;
    }
}

/**
 * cairo_pattern_get_rgba
 * @pattern: a #cairo_pattern_t
 * @red: return value for red component of color, or %NULL
 * @green: return value for green component of color, or %NULL
 * @blue: return value for blue component of color, or %NULL
 * @alpha: return value for alpha component of color, or %NULL
 *
 * Gets the solid color for a solid color pattern.
 *
 * Return value: %CAIRO_STATUS_SUCCESS, or
 * %CAIRO_STATUS_PATTERN_TYPE_MISMATCH if the pattern is not a solid
 * color pattern.
 *
 * Since: 1.4
 **/
cairo_status_t
cairo_pattern_get_rgba (cairo_pattern_t *pattern,
			double *red, double *green,
			double *blue, double *alpha)
{
    cairo_solid_pattern_t *solid = (cairo_solid_pattern_t*) pattern;
    double r0, g0, b0, a0;

    if (pattern->status)
	return pattern->status;

    if (pattern->type != CAIRO_PATTERN_TYPE_SOLID)
	return _cairo_error (CAIRO_STATUS_PATTERN_TYPE_MISMATCH);

    _cairo_color_get_rgba (&solid->color, &r0, &g0, &b0, &a0);

    if (red)
	*red = r0;
    if (green)
	*green = g0;
    if (blue)
	*blue = b0;
    if (alpha)
	*alpha = a0;

    return CAIRO_STATUS_SUCCESS;
}

/**
 * cairo_pattern_get_surface
 * @pattern: a #cairo_pattern_t
 * @surface: return value for surface of pattern, or %NULL
 * 
 * Gets the surface of a surface pattern.  The reference returned in
 * @surface is owned by the pattern; the caller should call
 * cairo_surface_reference() if the surface is to be retained.
 *
 * Return value: %CAIRO_STATUS_SUCCESS, or
 * %CAIRO_STATUS_PATTERN_TYPE_MISMATCH if the pattern is not a surface
 * pattern.
 *
 * Since: 1.4
 **/
cairo_status_t
cairo_pattern_get_surface (cairo_pattern_t *pattern,
			   cairo_surface_t **surface)
{
    cairo_surface_pattern_t *spat = (cairo_surface_pattern_t*) pattern;

    if (pattern->status)
	return pattern->status;

    if (pattern->type != CAIRO_PATTERN_TYPE_SURFACE)
	return CAIRO_STATUS_PATTERN_TYPE_MISMATCH;

    if (surface)
	*surface = spat->surface;

    return CAIRO_STATUS_SUCCESS;
}

/**
 * cairo_pattern_get_color_stop_rgba
 * @pattern: a #cairo_pattern_t
 * @index: index of the stop to return data for
 * @offset: return value for the offset of the stop, or %NULL
 * @red: return value for red component of color, or %NULL
 * @green: return value for green component of color, or %NULL
 * @blue: return value for blue component of color, or %NULL
 * @alpha: return value for alpha component of color, or %NULL
 *
 * Gets the color and offset information at the given @index for a
 * gradient pattern.  Values of @index are 0 to 1 less than the number
 * returned by cairo_pattern_get_color_stop_count().
 *
 * Return value: %CAIRO_STATUS_SUCCESS, or %CAIRO_STATUS_INVALID_INDEX
 * if @index is not valid for the given pattern.  If the pattern is
 * not a gradient pattern, %CAIRO_STATUS_PATTERN_TYPE_MISMATCH is
 * returned.
 *
 * Since: 1.4
 **/
cairo_status_t
cairo_pattern_get_color_stop_rgba (cairo_pattern_t *pattern,
				   int index, double *offset,
				   double *red, double *green,
				   double *blue, double *alpha)
{
    cairo_gradient_pattern_t *gradient = (cairo_gradient_pattern_t*) pattern;

    if (pattern->status)
	return pattern->status;

    if (pattern->type != CAIRO_PATTERN_TYPE_LINEAR &&
	pattern->type != CAIRO_PATTERN_TYPE_RADIAL)
	return _cairo_error (CAIRO_STATUS_PATTERN_TYPE_MISMATCH);

    if (index < 0 || (unsigned int) index >= gradient->n_stops)
	return _cairo_error (CAIRO_STATUS_INVALID_INDEX);

    if (offset)
	*offset = gradient->stops[index].offset;
    if (red)
	*red = gradient->stops[index].color.red;
    if (green)
	*green = gradient->stops[index].color.green;
    if (blue)
	*blue = gradient->stops[index].color.blue;
    if (alpha)
	*alpha = gradient->stops[index].color.alpha;

    return CAIRO_STATUS_SUCCESS;
}

/**
 * cairo_pattern_get_color_stop_count
 * @pattern: a #cairo_pattern_t
 * @count: return value for the number of color stops, or %NULL
 *
 * Gets the number of color stops specified in the given gradient
 * pattern.
 *
 * Return value: %CAIRO_STATUS_SUCCESS, or
 * %CAIRO_STATUS_PATTERN_TYPE_MISMATCH if @pattern is not a gradient
 * pattern.
 *
 * Since: 1.4
 */
cairo_status_t
cairo_pattern_get_color_stop_count (cairo_pattern_t *pattern,
				    int *count)
{
    cairo_gradient_pattern_t *gradient = (cairo_gradient_pattern_t*) pattern;

    if (pattern->status)
	return pattern->status;

    if (pattern->type != CAIRO_PATTERN_TYPE_LINEAR &&
	pattern->type != CAIRO_PATTERN_TYPE_RADIAL)
	return _cairo_error (CAIRO_STATUS_PATTERN_TYPE_MISMATCH);

    if (count)
	*count = gradient->n_stops;

    return CAIRO_STATUS_SUCCESS;
}

/**
 * cairo_pattern_get_linear_points
 * @pattern: a #cairo_pattern_t
 * @x0: return value for the x coordinate of the first point, or %NULL
 * @y0: return value for the y coordinate of the first point, or %NULL
 * @x1: return value for the x coordinate of the second point, or %NULL
 * @y1: return value for the y coordinate of the second point, or %NULL
 *
 * Gets the gradient endpoints for a linear gradient.
 *
 * Return value: %CAIRO_STATUS_SUCCESS, or
 * %CAIRO_STATUS_PATTERN_TYPE_MISMATCH if @pattern is not a linear
 * gradient pattern.
 *
 * Since: 1.4
 **/
cairo_status_t
cairo_pattern_get_linear_points (cairo_pattern_t *pattern,
				 double *x0, double *y0,
				 double *x1, double *y1)
{
    cairo_linear_pattern_t *linear = (cairo_linear_pattern_t*) pattern;

    if (pattern->status)
	return pattern->status;

    if (pattern->type != CAIRO_PATTERN_TYPE_LINEAR)
	return _cairo_error (CAIRO_STATUS_PATTERN_TYPE_MISMATCH);

    if (x0)
	*x0 = _cairo_fixed_to_double (linear->p1.x);
    if (y0)
	*y0 = _cairo_fixed_to_double (linear->p1.y);
    if (x1)
	*x1 = _cairo_fixed_to_double (linear->p2.x);
    if (y1)
	*y1 = _cairo_fixed_to_double (linear->p2.y);

    return CAIRO_STATUS_SUCCESS;
}

/**
 * cairo_pattern_get_radial_circles
 * @pattern: a #cairo_pattern_t
 * @x0: return value for the x coordinate of the center of the first circle, or %NULL
 * @y0: return value for the y coordinate of the center of the first circle, or %NULL
 * @r0: return value for the radius of the first circle, or %NULL
 * @x1: return value for the x coordinate of the center of the second circle, or %NULL
 * @y1: return value for the y coordinate of the center of the second circle, or %NULL
 * @r1: return value for the radius of the second circle, or %NULL
 *
 * Gets the gradient endpoint circles for a radial gradient, each
 * specified as a center coordinate and a radius.
 *
 * Return value: %CAIRO_STATUS_SUCCESS, or
 * %CAIRO_STATUS_PATTERN_TYPE_MISMATCH if @pattern is not a radial
 * gradient pattern.
 *
 * Since: 1.4
 **/
cairo_status_t
cairo_pattern_get_radial_circles (cairo_pattern_t *pattern,
				  double *x0, double *y0, double *r0,
				  double *x1, double *y1, double *r1)
{
    cairo_radial_pattern_t *radial = (cairo_radial_pattern_t*) pattern;

    if (pattern->status)
	return pattern->status;

    if (pattern->type != CAIRO_PATTERN_TYPE_RADIAL)
	return _cairo_error (CAIRO_STATUS_PATTERN_TYPE_MISMATCH);

    if (x0)
	*x0 = _cairo_fixed_to_double (radial->c1.x);
    if (y0)
	*y0 = _cairo_fixed_to_double (radial->c1.y);
    if (r0)
	*r0 = _cairo_fixed_to_double (radial->r1);
    if (x1)
	*x1 = _cairo_fixed_to_double (radial->c2.x);
    if (y1)
	*y1 = _cairo_fixed_to_double (radial->c2.y);
    if (r1)
	*r1 = _cairo_fixed_to_double (radial->r2);

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_pattern_reset_static_data (void)
{
#if HAS_FREED_POOL
    int i;

    for (i = 0; i < ARRAY_LENGTH (freed_pattern_pool); i++)
	_freed_pool_reset (&freed_pattern_pool[i]);
#endif

    _cairo_pattern_reset_solid_surface_cache ();
}
