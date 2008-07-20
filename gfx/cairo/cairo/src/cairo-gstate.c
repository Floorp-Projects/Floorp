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

#define _GNU_SOURCE

#include "cairoint.h"

#include "cairo-clip-private.h"
#include "cairo-gstate-private.h"

#if _XOPEN_SOURCE >= 600 || defined (_ISOC99_SOURCE)
#define ISFINITE(x) isfinite (x)
#else
#define ISFINITE(x) ((x) * (x) >= 0.) /* check for NaNs */
#endif

static cairo_status_t
_cairo_gstate_init_copy (cairo_gstate_t *gstate, cairo_gstate_t *other);

static cairo_status_t
_cairo_gstate_ensure_font_face (cairo_gstate_t *gstate);

static cairo_status_t
_cairo_gstate_ensure_scaled_font (cairo_gstate_t *gstate);

static void
_cairo_gstate_unset_scaled_font (cairo_gstate_t *gstate);

static void
_cairo_gstate_transform_glyphs_to_backend (cairo_gstate_t      *gstate,
                                           const cairo_glyph_t *glyphs,
                                           int                  num_glyphs,
                                           cairo_glyph_t       *transformed_glyphs);

cairo_status_t
_cairo_gstate_init (cairo_gstate_t  *gstate,
		    cairo_surface_t *target)
{
    cairo_status_t status;

    gstate->next = NULL;

    gstate->op = CAIRO_GSTATE_OPERATOR_DEFAULT;

    gstate->tolerance = CAIRO_GSTATE_TOLERANCE_DEFAULT;
    gstate->antialias = CAIRO_ANTIALIAS_DEFAULT;

    _cairo_stroke_style_init (&gstate->stroke_style);

    gstate->fill_rule = CAIRO_GSTATE_FILL_RULE_DEFAULT;

    gstate->font_face = NULL;
    gstate->scaled_font = NULL;

    cairo_matrix_init_scale (&gstate->font_matrix,
			     CAIRO_GSTATE_DEFAULT_FONT_SIZE,
			     CAIRO_GSTATE_DEFAULT_FONT_SIZE);

    _cairo_font_options_init_default (&gstate->font_options);

    _cairo_clip_init (&gstate->clip, target);

    gstate->target = cairo_surface_reference (target);
    gstate->parent_target = NULL;
    gstate->original_target = cairo_surface_reference (target);

    _cairo_gstate_identity_matrix (gstate);
    gstate->source_ctm_inverse = gstate->ctm_inverse;

    gstate->source = _cairo_pattern_create_solid (CAIRO_COLOR_BLACK,
						  CAIRO_CONTENT_COLOR);

    /* Now that the gstate is fully initialized and ready for the eventual
     * _cairo_gstate_fini(), we can check for errors (and not worry about
     * the resource deallocation). */

    if (target == NULL)
	return _cairo_error (CAIRO_STATUS_NULL_POINTER);

    status = target->status;
    if (status)
	return status;

    status = gstate->source->status;
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

/**
 * _cairo_gstate_init_copy:
 *
 * Initialize @gstate by performing a deep copy of state fields from
 * @other. Note that gstate->next is not copied but is set to %NULL by
 * this function.
 **/
static cairo_status_t
_cairo_gstate_init_copy (cairo_gstate_t *gstate, cairo_gstate_t *other)
{
    cairo_status_t status;

    gstate->op = other->op;

    gstate->tolerance = other->tolerance;
    gstate->antialias = other->antialias;

    status = _cairo_stroke_style_init_copy (&gstate->stroke_style,
					    &other->stroke_style);
    if (status)
	return status;

    gstate->fill_rule = other->fill_rule;

    gstate->font_face = cairo_font_face_reference (other->font_face);
    gstate->scaled_font = cairo_scaled_font_reference (other->scaled_font);

    gstate->font_matrix = other->font_matrix;

    _cairo_font_options_init_copy (&gstate->font_options , &other->font_options);

    status = _cairo_clip_init_copy (&gstate->clip, &other->clip);
    if (status) {
	_cairo_stroke_style_fini (&gstate->stroke_style);
	cairo_font_face_destroy (gstate->font_face);
	cairo_scaled_font_destroy (gstate->scaled_font);
	return status;
    }

    gstate->target = cairo_surface_reference (other->target);
    /* parent_target is always set to NULL; it's only ever set by redirect_target */
    gstate->parent_target = NULL;
    gstate->original_target = cairo_surface_reference (other->original_target);

    gstate->ctm = other->ctm;
    gstate->ctm_inverse = other->ctm_inverse;
    gstate->source_ctm_inverse = other->source_ctm_inverse;

    gstate->source = cairo_pattern_reference (other->source);

    gstate->next = NULL;

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_gstate_fini (cairo_gstate_t *gstate)
{
    _cairo_stroke_style_fini (&gstate->stroke_style);

    cairo_font_face_destroy (gstate->font_face);
    gstate->font_face = NULL;

    cairo_scaled_font_destroy (gstate->scaled_font);
    gstate->scaled_font = NULL;

    _cairo_clip_reset (&gstate->clip);

    cairo_surface_destroy (gstate->target);
    gstate->target = NULL;

    cairo_surface_destroy (gstate->parent_target);
    gstate->parent_target = NULL;

    cairo_surface_destroy (gstate->original_target);
    gstate->original_target = NULL;

    cairo_pattern_destroy (gstate->source);
    gstate->source = NULL;
}

static void
_cairo_gstate_destroy (cairo_gstate_t *gstate)
{
    _cairo_gstate_fini (gstate);
    free (gstate);
}

/**
 * _cairo_gstate_clone:
 * @other: a #cairo_gstate_t to be copied, not %NULL.
 *
 * Create a new #cairo_gstate_t setting all graphics state parameters
 * to the same values as contained in @other. gstate->next will be set
 * to %NULL and may be used by the caller to chain #cairo_gstate_t
 * objects together.
 *
 * Return value: a new #cairo_gstate_t or %NULL if there is insufficient
 * memory.
 **/
static cairo_status_t
_cairo_gstate_clone (cairo_gstate_t *other, cairo_gstate_t **out)
{
    cairo_status_t status;
    cairo_gstate_t *gstate;

    assert (other != NULL);

    gstate = malloc (sizeof (cairo_gstate_t));
    if (gstate == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    status = _cairo_gstate_init_copy (gstate, other);
    if (status) {
	free (gstate);
	return status;
    }

    *out = gstate;
    return CAIRO_STATUS_SUCCESS;
}

/**
 * _cairo_gstate_save:
 * @gstate: input/output gstate pointer
 *
 * Makes a copy of the current state of @gstate and saves it
 * to @gstate->next, then put the address of the newly allcated
 * copy into @gstate.  _cairo_gstate_restore() reverses this.
 **/
cairo_status_t
_cairo_gstate_save (cairo_gstate_t **gstate)
{
    cairo_gstate_t *top = NULL;
    cairo_status_t status;

    status = _cairo_gstate_clone (*gstate, &top);
    if (status)
	return status;

    top->next = *gstate;
    *gstate = top;

    return CAIRO_STATUS_SUCCESS;
}

/**
 * _cairo_gstate_restore:
 * @gstate: input/output gstate pointer
 *
 * Reverses the effects of one _cairo_gstate_save() call.
 **/
cairo_status_t
_cairo_gstate_restore (cairo_gstate_t **gstate)
{
    cairo_gstate_t *top;

    top = *gstate;
    if (top->next == NULL)
	return _cairo_error (CAIRO_STATUS_INVALID_RESTORE);

    *gstate = top->next;

    _cairo_gstate_destroy (top);

    return CAIRO_STATUS_SUCCESS;
}

/**
 * _cairo_gstate_redirect_target:
 * @gstate: a #cairo_gstate_t
 * @child: the new child target
 *
 * Redirect @gstate rendering to a "child" target. The original
 * "parent" target with which the gstate was created will not be
 * affected. See _cairo_gstate_get_target().
 *
 * Unless the redirected target has the same device offsets as the
 * original #cairo_t target, the clip will be INVALID after this call,
 * and the caller should either recreate or reset the clip.
 **/
cairo_status_t
_cairo_gstate_redirect_target (cairo_gstate_t *gstate, cairo_surface_t *child)
{
    cairo_status_t status;

    /* If this gstate is already redirected, this is an error; we need a
     * new gstate to be able to redirect */
    assert (gstate->parent_target == NULL);

    /* Set up our new parent_target based on our current target;
     * gstate->parent_target will take the ref that is held by gstate->target
     */
    cairo_surface_destroy (gstate->parent_target);
    gstate->parent_target = gstate->target;

    /* Now set up our new target; we overwrite gstate->target directly,
     * since its ref is now owned by gstate->parent_target */
    gstate->target = cairo_surface_reference (child);

    _cairo_clip_reset (&gstate->clip);
    status = _cairo_clip_init_deep_copy (&gstate->clip, &gstate->next->clip, child);
    if (status)
	return status;

    /* The clip is in surface backend coordinates for the previous target;
     * translate it into the child's backend coordinates. */
    _cairo_clip_translate (&gstate->clip,
                           _cairo_fixed_from_double (child->device_transform.x0 - gstate->parent_target->device_transform.x0),
                           _cairo_fixed_from_double (child->device_transform.y0 - gstate->parent_target->device_transform.y0));

    return CAIRO_STATUS_SUCCESS;
}

/**
 * _cairo_gstate_is_redirected
 * @gstate: a #cairo_gstate_t
 *
 * Return value: %TRUE if the gstate is redirected to a target
 * different than the original, %FALSE otherwise.
 **/
cairo_bool_t
_cairo_gstate_is_redirected (cairo_gstate_t *gstate)
{
    return (gstate->target != gstate->original_target);
}

/**
 * _cairo_gstate_get_target:
 * @gstate: a #cairo_gstate_t
 *
 * Return the current drawing target; if drawing is not redirected,
 * this will be the same as _cairo_gstate_get_original_target().
 *
 * Return value: the current target surface
 **/
cairo_surface_t *
_cairo_gstate_get_target (cairo_gstate_t *gstate)
{
    return gstate->target;
}

/**
 * _cairo_gstate_get_parent_target:
 * @gstate: a #cairo_gstate_t
 *
 * Return the parent surface of the current drawing target surface;
 * if this particular gstate isn't a redirect gstate, this will return %NULL.
 **/
cairo_surface_t *
_cairo_gstate_get_parent_target (cairo_gstate_t *gstate)
{
    return gstate->parent_target;
}

/**
 * _cairo_gstate_get_original_target:
 * @gstate: a #cairo_gstate_t
 *
 * Return the original target with which @gstate was created. This
 * function always returns the original target independent of any
 * child target that may have been set with
 * _cairo_gstate_redirect_target.
 *
 * Return value: the original target surface
 **/
cairo_surface_t *
_cairo_gstate_get_original_target (cairo_gstate_t *gstate)
{
    return gstate->original_target;
}

/**
 * _cairo_gstate_get_clip:
 * @gstate: a #cairo_gstate_t
 *
 * Return value: a pointer to the gstate's #cairo_clip_t structure.
 */
cairo_clip_t *
_cairo_gstate_get_clip (cairo_gstate_t *gstate)
{
    return &gstate->clip;
}

cairo_status_t
_cairo_gstate_set_source (cairo_gstate_t  *gstate,
			  cairo_pattern_t *source)
{
    if (source->status)
	return source->status;

    source = cairo_pattern_reference (source);
    cairo_pattern_destroy (gstate->source);
    gstate->source = source;
    gstate->source_ctm_inverse = gstate->ctm_inverse;

    return CAIRO_STATUS_SUCCESS;
}

cairo_pattern_t *
_cairo_gstate_get_source (cairo_gstate_t *gstate)
{
    return gstate->source;
}

cairo_status_t
_cairo_gstate_set_operator (cairo_gstate_t *gstate, cairo_operator_t op)
{
    gstate->op = op;

    return CAIRO_STATUS_SUCCESS;
}

cairo_operator_t
_cairo_gstate_get_operator (cairo_gstate_t *gstate)
{
    return gstate->op;
}

cairo_status_t
_cairo_gstate_set_tolerance (cairo_gstate_t *gstate, double tolerance)
{
    gstate->tolerance = tolerance;

    return CAIRO_STATUS_SUCCESS;
}

double
_cairo_gstate_get_tolerance (cairo_gstate_t *gstate)
{
    return gstate->tolerance;
}

cairo_status_t
_cairo_gstate_set_fill_rule (cairo_gstate_t *gstate, cairo_fill_rule_t fill_rule)
{
    gstate->fill_rule = fill_rule;

    return CAIRO_STATUS_SUCCESS;
}

cairo_fill_rule_t
_cairo_gstate_get_fill_rule (cairo_gstate_t *gstate)
{
    return gstate->fill_rule;
}

cairo_status_t
_cairo_gstate_set_line_width (cairo_gstate_t *gstate, double width)
{
    gstate->stroke_style.line_width = width;

    return CAIRO_STATUS_SUCCESS;
}

double
_cairo_gstate_get_line_width (cairo_gstate_t *gstate)
{
    return gstate->stroke_style.line_width;
}

cairo_status_t
_cairo_gstate_set_line_cap (cairo_gstate_t *gstate, cairo_line_cap_t line_cap)
{
    gstate->stroke_style.line_cap = line_cap;

    return CAIRO_STATUS_SUCCESS;
}

cairo_line_cap_t
_cairo_gstate_get_line_cap (cairo_gstate_t *gstate)
{
    return gstate->stroke_style.line_cap;
}

cairo_status_t
_cairo_gstate_set_line_join (cairo_gstate_t *gstate, cairo_line_join_t line_join)
{
    gstate->stroke_style.line_join = line_join;

    return CAIRO_STATUS_SUCCESS;
}

cairo_line_join_t
_cairo_gstate_get_line_join (cairo_gstate_t *gstate)
{
    return gstate->stroke_style.line_join;
}

cairo_status_t
_cairo_gstate_set_dash (cairo_gstate_t *gstate, const double *dash, int num_dashes, double offset)
{
    unsigned int i;
    double dash_total;

    if (gstate->stroke_style.dash)
	free (gstate->stroke_style.dash);

    gstate->stroke_style.num_dashes = num_dashes;

    if (gstate->stroke_style.num_dashes == 0) {
	gstate->stroke_style.dash = NULL;
	gstate->stroke_style.dash_offset = 0.0;
	return CAIRO_STATUS_SUCCESS;
    }

    gstate->stroke_style.dash = _cairo_malloc_ab (gstate->stroke_style.num_dashes, sizeof (double));
    if (gstate->stroke_style.dash == NULL) {
	gstate->stroke_style.num_dashes = 0;
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    memcpy (gstate->stroke_style.dash, dash, gstate->stroke_style.num_dashes * sizeof (double));

    dash_total = 0.0;
    for (i = 0; i < gstate->stroke_style.num_dashes; i++) {
	if (gstate->stroke_style.dash[i] < 0)
	    return _cairo_error (CAIRO_STATUS_INVALID_DASH);
	dash_total += gstate->stroke_style.dash[i];
    }

    if (dash_total == 0.0)
	return _cairo_error (CAIRO_STATUS_INVALID_DASH);

    /* A single dash value indicate symmetric repeating, so the total
     * is twice as long. */
    if (gstate->stroke_style.num_dashes == 1)
	dash_total *= 2;

    /* The dashing code doesn't like a negative offset, so we compute
     * the equivalent positive offset. */
    if (offset < 0)
	offset += ceil (-offset / dash_total + 0.5) * dash_total;

    gstate->stroke_style.dash_offset = offset;

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_gstate_get_dash (cairo_gstate_t *gstate,
			double         *dashes,
			int            *num_dashes,
			double         *offset)
{
    if (dashes)
	memcpy (dashes,
		gstate->stroke_style.dash,
		sizeof (double) * gstate->stroke_style.num_dashes);

    if (num_dashes)
	*num_dashes = gstate->stroke_style.num_dashes;

    if (offset)
	*offset = gstate->stroke_style.dash_offset;
}

cairo_status_t
_cairo_gstate_set_miter_limit (cairo_gstate_t *gstate, double limit)
{
    gstate->stroke_style.miter_limit = limit;

    return CAIRO_STATUS_SUCCESS;
}

double
_cairo_gstate_get_miter_limit (cairo_gstate_t *gstate)
{
    return gstate->stroke_style.miter_limit;
}

void
_cairo_gstate_get_matrix (cairo_gstate_t *gstate, cairo_matrix_t *matrix)
{
    *matrix = gstate->ctm;
}

cairo_status_t
_cairo_gstate_translate (cairo_gstate_t *gstate, double tx, double ty)
{
    cairo_matrix_t tmp;

    if (! ISFINITE (tx) || ! ISFINITE (ty))
	return _cairo_error (CAIRO_STATUS_INVALID_MATRIX);

    _cairo_gstate_unset_scaled_font (gstate);

    cairo_matrix_init_translate (&tmp, tx, ty);
    cairo_matrix_multiply (&gstate->ctm, &tmp, &gstate->ctm);

    /* paranoid check against gradual numerical instability */
    if (! _cairo_matrix_is_invertible (&gstate->ctm))
	return _cairo_error (CAIRO_STATUS_INVALID_MATRIX);

    cairo_matrix_init_translate (&tmp, -tx, -ty);
    cairo_matrix_multiply (&gstate->ctm_inverse, &gstate->ctm_inverse, &tmp);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_scale (cairo_gstate_t *gstate, double sx, double sy)
{
    cairo_matrix_t tmp;

    if (sx * sy == 0.) /* either sx or sy is 0, or det == 0 due to underflow */
	return _cairo_error (CAIRO_STATUS_INVALID_MATRIX);
    if (! ISFINITE (sx) || ! ISFINITE (sy))
	return _cairo_error (CAIRO_STATUS_INVALID_MATRIX);

    _cairo_gstate_unset_scaled_font (gstate);

    cairo_matrix_init_scale (&tmp, sx, sy);
    cairo_matrix_multiply (&gstate->ctm, &tmp, &gstate->ctm);

    /* paranoid check against gradual numerical instability */
    if (! _cairo_matrix_is_invertible (&gstate->ctm))
	return _cairo_error (CAIRO_STATUS_INVALID_MATRIX);

    cairo_matrix_init_scale (&tmp, 1/sx, 1/sy);
    cairo_matrix_multiply (&gstate->ctm_inverse, &gstate->ctm_inverse, &tmp);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_rotate (cairo_gstate_t *gstate, double angle)
{
    cairo_matrix_t tmp;

    if (angle == 0.)
	return CAIRO_STATUS_SUCCESS;

    if (! ISFINITE (angle))
	return _cairo_error (CAIRO_STATUS_INVALID_MATRIX);

    _cairo_gstate_unset_scaled_font (gstate);

    cairo_matrix_init_rotate (&tmp, angle);
    cairo_matrix_multiply (&gstate->ctm, &tmp, &gstate->ctm);

    /* paranoid check against gradual numerical instability */
    if (! _cairo_matrix_is_invertible (&gstate->ctm))
	return _cairo_error (CAIRO_STATUS_INVALID_MATRIX);

    cairo_matrix_init_rotate (&tmp, -angle);
    cairo_matrix_multiply (&gstate->ctm_inverse, &gstate->ctm_inverse, &tmp);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_transform (cairo_gstate_t	      *gstate,
			 const cairo_matrix_t *matrix)
{
    cairo_matrix_t tmp;
    cairo_status_t status;

    tmp = *matrix;
    status = cairo_matrix_invert (&tmp);
    if (status)
	return status;

    _cairo_gstate_unset_scaled_font (gstate);

    cairo_matrix_multiply (&gstate->ctm, matrix, &gstate->ctm);
    cairo_matrix_multiply (&gstate->ctm_inverse, &gstate->ctm_inverse, &tmp);

    /* paranoid check against gradual numerical instability */
    if (! _cairo_matrix_is_invertible (&gstate->ctm))
	return _cairo_error (CAIRO_STATUS_INVALID_MATRIX);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_set_matrix (cairo_gstate_t       *gstate,
			  const cairo_matrix_t *matrix)
{
    cairo_status_t status;

    if (! _cairo_matrix_is_invertible (matrix))
	return _cairo_error (CAIRO_STATUS_INVALID_MATRIX);

    _cairo_gstate_unset_scaled_font (gstate);

    gstate->ctm = *matrix;
    gstate->ctm_inverse = *matrix;
    status = cairo_matrix_invert (&gstate->ctm_inverse);
    assert (status == CAIRO_STATUS_SUCCESS);

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_gstate_identity_matrix (cairo_gstate_t *gstate)
{
    _cairo_gstate_unset_scaled_font (gstate);

    cairo_matrix_init_identity (&gstate->ctm);
    cairo_matrix_init_identity (&gstate->ctm_inverse);
}

void
_cairo_gstate_user_to_device (cairo_gstate_t *gstate, double *x, double *y)
{
    cairo_matrix_transform_point (&gstate->ctm, x, y);
}

void
_cairo_gstate_user_to_device_distance (cairo_gstate_t *gstate,
				       double *dx, double *dy)
{
    cairo_matrix_transform_distance (&gstate->ctm, dx, dy);
}

void
_cairo_gstate_device_to_user (cairo_gstate_t *gstate, double *x, double *y)
{
    cairo_matrix_transform_point (&gstate->ctm_inverse, x, y);
}

void
_cairo_gstate_device_to_user_distance (cairo_gstate_t *gstate,
				       double *dx, double *dy)
{
    cairo_matrix_transform_distance (&gstate->ctm_inverse, dx, dy);
}

void
_cairo_gstate_user_to_backend (cairo_gstate_t *gstate, double *x, double *y)
{
    cairo_matrix_transform_point (&gstate->ctm, x, y);
    cairo_matrix_transform_point (&gstate->target->device_transform, x, y);
}

void
_cairo_gstate_backend_to_user (cairo_gstate_t *gstate, double *x, double *y)
{
    cairo_matrix_transform_point (&gstate->target->device_transform_inverse, x, y);
    cairo_matrix_transform_point (&gstate->ctm_inverse, x, y);
}

void
_cairo_gstate_backend_to_user_rectangle (cairo_gstate_t *gstate,
                                         double *x1, double *y1,
                                         double *x2, double *y2,
                                         cairo_bool_t *is_tight)
{
    cairo_matrix_t matrix_inverse;

    cairo_matrix_multiply (&matrix_inverse,
                           &gstate->target->device_transform_inverse,
			   &gstate->ctm_inverse);
    _cairo_matrix_transform_bounding_box (&matrix_inverse,
					  x1, y1, x2, y2, is_tight);
}

/* XXX: NYI
cairo_status_t
_cairo_gstate_stroke_to_path (cairo_gstate_t *gstate)
{
    cairo_status_t status;

    _cairo_pen_init (&gstate);
    return CAIRO_STATUS_SUCCESS;
}
*/

cairo_status_t
_cairo_gstate_path_extents (cairo_gstate_t     *gstate,
			    cairo_path_fixed_t *path,
			    double *x1, double *y1,
			    double *x2, double *y2)
{
    cairo_status_t status;

    status = _cairo_path_fixed_bounds (path, x1, y1, x2, y2, gstate->tolerance);
    if (status)
	return status;

    _cairo_gstate_backend_to_user_rectangle (gstate, x1, y1, x2, y2, NULL);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gstate_copy_transformed_pattern (cairo_gstate_t  *gstate,
					cairo_pattern_t *pattern,
					cairo_pattern_t *original,
					cairo_matrix_t  *ctm_inverse)
{
    cairo_surface_pattern_t *surface_pattern;
    cairo_surface_t *surface;
    cairo_status_t status;

    status = _cairo_pattern_init_copy (pattern, original);
    if (status)
	return status;

    _cairo_pattern_transform (pattern, ctm_inverse);

    if (cairo_pattern_get_type (original) == CAIRO_PATTERN_TYPE_SURFACE) {
        surface_pattern = (cairo_surface_pattern_t *) original;
        surface = surface_pattern->surface;
        if (_cairo_surface_has_device_transform (surface))
            _cairo_pattern_transform (pattern, &surface->device_transform);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gstate_copy_transformed_source (cairo_gstate_t  *gstate,
				       cairo_pattern_t *pattern)
{
    return _cairo_gstate_copy_transformed_pattern (gstate, pattern,
					           gstate->source,
					           &gstate->source_ctm_inverse);
}

static cairo_status_t
_cairo_gstate_copy_transformed_mask (cairo_gstate_t  *gstate,
				     cairo_pattern_t *pattern,
				     cairo_pattern_t *mask)
{
    return _cairo_gstate_copy_transformed_pattern (gstate, pattern,
					           mask,
					           &gstate->ctm_inverse);
}

cairo_status_t
_cairo_gstate_paint (cairo_gstate_t *gstate)
{
    cairo_status_t status;
    cairo_pattern_union_t pattern;

    if (gstate->source->status)
	return gstate->source->status;

    status = _cairo_surface_set_clip (gstate->target, &gstate->clip);
    if (status)
	return status;

    status = _cairo_gstate_copy_transformed_source (gstate, &pattern.base);
    if (status)
	return status;

    status = _cairo_surface_paint (gstate->target,
				   gstate->op,
				   &pattern.base);

    _cairo_pattern_fini (&pattern.base);

    return status;
}

cairo_status_t
_cairo_gstate_mask (cairo_gstate_t  *gstate,
		    cairo_pattern_t *mask)
{
    cairo_status_t status;
    cairo_pattern_union_t source_pattern, mask_pattern;

    if (mask->status)
	return mask->status;

    if (gstate->source->status)
	return gstate->source->status;

    status = _cairo_surface_set_clip (gstate->target, &gstate->clip);
    if (status)
	return status;

    status = _cairo_gstate_copy_transformed_source (gstate, &source_pattern.base);
    if (status)
	return status;

    status = _cairo_gstate_copy_transformed_mask (gstate, &mask_pattern.base, mask);
    if (status)
	goto CLEANUP_SOURCE;

    status = _cairo_surface_mask (gstate->target,
				  gstate->op,
				  &source_pattern.base,
				  &mask_pattern.base);

    _cairo_pattern_fini (&mask_pattern.base);
CLEANUP_SOURCE:
    _cairo_pattern_fini (&source_pattern.base);

    return status;
}

cairo_status_t
_cairo_gstate_stroke (cairo_gstate_t *gstate, cairo_path_fixed_t *path)
{
    cairo_status_t status;
    cairo_pattern_union_t source_pattern;

    if (gstate->source->status)
	return gstate->source->status;

    if (gstate->stroke_style.line_width <= 0.0)
	return CAIRO_STATUS_SUCCESS;

    status = _cairo_surface_set_clip (gstate->target, &gstate->clip);
    if (status)
	return status;

    status = _cairo_gstate_copy_transformed_source (gstate,
	                                            &source_pattern.base);
    if (status)
	return status;

    status = _cairo_surface_stroke (gstate->target,
				    gstate->op,
				    &source_pattern.base,
				    path,
				    &gstate->stroke_style,
				    &gstate->ctm,
				    &gstate->ctm_inverse,
				    gstate->tolerance,
				    gstate->antialias);

    _cairo_pattern_fini (&source_pattern.base);

    return status;

}

cairo_status_t
_cairo_gstate_in_stroke (cairo_gstate_t	    *gstate,
			 cairo_path_fixed_t *path,
			 double		     x,
			 double		     y,
			 cairo_bool_t	    *inside_ret)
{
    cairo_status_t status;
    cairo_traps_t traps;

    if (gstate->stroke_style.line_width <= 0.0) {
	*inside_ret = FALSE;
	return CAIRO_STATUS_SUCCESS;
    }

    _cairo_gstate_user_to_backend (gstate, &x, &y);

    _cairo_traps_init (&traps);

    status = _cairo_path_fixed_stroke_to_traps (path,
						&gstate->stroke_style,
						&gstate->ctm,
						&gstate->ctm_inverse,
						gstate->tolerance,
						&traps);
    if (status)
	goto BAIL;

    *inside_ret = _cairo_traps_contain (&traps, x, y);

BAIL:
    _cairo_traps_fini (&traps);

    return status;
}

cairo_status_t
_cairo_gstate_fill (cairo_gstate_t *gstate, cairo_path_fixed_t *path)
{
    cairo_status_t status;
    cairo_pattern_union_t pattern;

    if (gstate->source->status)
	return gstate->source->status;

    status = _cairo_surface_set_clip (gstate->target, &gstate->clip);
    if (status)
	return status;

    status = _cairo_gstate_copy_transformed_source (gstate, &pattern.base);
    if (status)
	return status;

    status = _cairo_surface_fill (gstate->target,
				  gstate->op,
				  &pattern.base,
				  path,
				  gstate->fill_rule,
				  gstate->tolerance,
				  gstate->antialias);

    _cairo_pattern_fini (&pattern.base);

    return status;
}

cairo_status_t
_cairo_gstate_in_fill (cairo_gstate_t	  *gstate,
		       cairo_path_fixed_t *path,
		       double		   x,
		       double		   y,
		       cairo_bool_t	  *inside_ret)
{
    cairo_status_t status;
    cairo_traps_t traps;

    _cairo_gstate_user_to_backend (gstate, &x, &y);

    _cairo_traps_init (&traps);

    status = _cairo_path_fixed_fill_to_traps (path,
					      gstate->fill_rule,
					      gstate->tolerance,
					      &traps);
    if (status)
	goto BAIL;

    *inside_ret = _cairo_traps_contain (&traps, x, y);

BAIL:
    _cairo_traps_fini (&traps);

    return status;
}

cairo_status_t
_cairo_gstate_copy_page (cairo_gstate_t *gstate)
{
    cairo_surface_copy_page (gstate->target);
    return cairo_surface_status (gstate->target);
}

cairo_status_t
_cairo_gstate_show_page (cairo_gstate_t *gstate)
{
    cairo_surface_show_page (gstate->target);
    return cairo_surface_status (gstate->target);
}

static void
_cairo_gstate_traps_extents_to_user_rectangle (cairo_gstate_t	  *gstate,
                                               cairo_traps_t      *traps,
                                               double *x1, double *y1,
                                               double *x2, double *y2)
{
    cairo_box_t extents;

    if (traps->num_traps == 0) {
        /* no traps, so we actually won't draw anything */
	if (x1)
	    *x1 = 0.0;
	if (y1)
	    *y1 = 0.0;
	if (x2)
	    *x2 = 0.0;
	if (y2)
	    *y2 = 0.0;
    } else {
	_cairo_traps_extents (traps, &extents);

	if (x1)
	    *x1 = _cairo_fixed_to_double (extents.p1.x);
	if (y1)
	    *y1 = _cairo_fixed_to_double (extents.p1.y);
	if (x2)
	    *x2 = _cairo_fixed_to_double (extents.p2.x);
	if (y2)
	    *y2 = _cairo_fixed_to_double (extents.p2.y);

        _cairo_gstate_backend_to_user_rectangle (gstate, x1, y1, x2, y2, NULL);
    }
}

cairo_status_t
_cairo_gstate_stroke_extents (cairo_gstate_t	 *gstate,
			      cairo_path_fixed_t *path,
                              double *x1, double *y1,
			      double *x2, double *y2)
{
    cairo_status_t status;
    cairo_traps_t traps;

    if (gstate->stroke_style.line_width <= 0.0) {
	if (x1)
	    *x1 = 0.0;
	if (y1)
	    *y1 = 0.0;
	if (x2)
	    *x2 = 0.0;
	if (y2)
	    *y2 = 0.0;
	return CAIRO_STATUS_SUCCESS;
    }

    _cairo_traps_init (&traps);

    status = _cairo_path_fixed_stroke_to_traps (path,
						&gstate->stroke_style,
						&gstate->ctm,
						&gstate->ctm_inverse,
						gstate->tolerance,
						&traps);
    if (status == CAIRO_STATUS_SUCCESS) {
        _cairo_gstate_traps_extents_to_user_rectangle(gstate, &traps, x1, y1, x2, y2);
    }

    _cairo_traps_fini (&traps);

    return status;
}

cairo_status_t
_cairo_gstate_fill_extents (cairo_gstate_t     *gstate,
			    cairo_path_fixed_t *path,
                            double *x1, double *y1,
			    double *x2, double *y2)
{
    cairo_status_t status;
    cairo_traps_t traps;

    _cairo_traps_init (&traps);

    status = _cairo_path_fixed_fill_to_traps (path,
					      gstate->fill_rule,
					      gstate->tolerance,
					      &traps);
    if (status == CAIRO_STATUS_SUCCESS) {
        _cairo_gstate_traps_extents_to_user_rectangle(gstate, &traps, x1, y1, x2, y2);
    }

    _cairo_traps_fini (&traps);

    return status;
}

cairo_status_t
_cairo_gstate_reset_clip (cairo_gstate_t *gstate)
{
    _cairo_clip_reset (&gstate->clip);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_clip (cairo_gstate_t *gstate, cairo_path_fixed_t *path)
{
    return _cairo_clip_clip (&gstate->clip,
			     path, gstate->fill_rule, gstate->tolerance,
			     gstate->antialias, gstate->target);
}

cairo_status_t
_cairo_gstate_clip_extents (cairo_gstate_t *gstate,
		            double         *x1,
		            double         *y1,
        		    double         *x2,
        		    double         *y2)
{
    cairo_rectangle_int_t extents;
    cairo_status_t status;
    
    status = _cairo_surface_get_extents (gstate->target, &extents);
    if (status)
        return status;

    status = _cairo_clip_intersect_to_rectangle (&gstate->clip, &extents);
    if (status)
        return status;

    if (x1)
	*x1 = extents.x;
    if (y1)
	*y1 = extents.y;
    if (x2)
	*x2 = extents.x + extents.width;
    if (y2)
	*y2 = extents.y + extents.height;

    _cairo_gstate_backend_to_user_rectangle (gstate, x1, y1, x2, y2, NULL);

    return CAIRO_STATUS_SUCCESS;
}

cairo_rectangle_list_t*
_cairo_gstate_copy_clip_rectangle_list (cairo_gstate_t *gstate)
{
    return _cairo_clip_copy_rectangle_list (&gstate->clip, gstate);
}

static void
_cairo_gstate_unset_scaled_font (cairo_gstate_t *gstate)
{
    if (gstate->scaled_font) {
	cairo_scaled_font_destroy (gstate->scaled_font);
	gstate->scaled_font = NULL;
    }
}

cairo_status_t
_cairo_gstate_select_font_face (cairo_gstate_t       *gstate,
				const char           *family,
				cairo_font_slant_t    slant,
				cairo_font_weight_t   weight)
{
    cairo_font_face_t *font_face;
    cairo_status_t status;

    font_face = _cairo_toy_font_face_create (family, slant, weight);
    if (font_face->status)
	return font_face->status;

    status = _cairo_gstate_set_font_face (gstate, font_face);
    cairo_font_face_destroy (font_face);

    return status;
}

cairo_status_t
_cairo_gstate_set_font_size (cairo_gstate_t *gstate,
			     double          size)
{
    _cairo_gstate_unset_scaled_font (gstate);

    cairo_matrix_init_scale (&gstate->font_matrix, size, size);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_set_font_matrix (cairo_gstate_t	    *gstate,
			       const cairo_matrix_t *matrix)
{
    if (! _cairo_matrix_is_invertible (matrix))
	return _cairo_error (CAIRO_STATUS_INVALID_MATRIX);

    _cairo_gstate_unset_scaled_font (gstate);

    gstate->font_matrix = *matrix;

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_gstate_get_font_matrix (cairo_gstate_t *gstate,
			       cairo_matrix_t *matrix)
{
    *matrix = gstate->font_matrix;
}

void
_cairo_gstate_set_font_options (cairo_gstate_t             *gstate,
				const cairo_font_options_t *options)
{
    _cairo_gstate_unset_scaled_font (gstate);

    _cairo_font_options_init_copy (&gstate->font_options, options);
}

void
_cairo_gstate_get_font_options (cairo_gstate_t       *gstate,
				cairo_font_options_t *options)
{
    *options = gstate->font_options;
}

cairo_status_t
_cairo_gstate_get_font_face (cairo_gstate_t     *gstate,
			     cairo_font_face_t **font_face)
{
    cairo_status_t status;

    status = _cairo_gstate_ensure_font_face (gstate);
    if (status)
	return status;

    *font_face = gstate->font_face;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_get_scaled_font (cairo_gstate_t       *gstate,
			       cairo_scaled_font_t **scaled_font)
{
    cairo_status_t status;

    status = _cairo_gstate_ensure_scaled_font (gstate);
    if (status)
	return status;

    *scaled_font = gstate->scaled_font;

    return CAIRO_STATUS_SUCCESS;
}

/*
 * Like everything else in this file, fonts involve Too Many Coordinate Spaces;
 * it is easy to get confused about what's going on.
 *
 * The user's view
 * ---------------
 *
 * Users ask for things in user space. When cairo starts, a user space unit
 * is about 1/96 inch, which is similar to (but importantly different from)
 * the normal "point" units most users think in terms of. When a user
 * selects a font, its scale is set to "one user unit". The user can then
 * independently scale the user coordinate system *or* the font matrix, in
 * order to adjust the rendered size of the font.
 *
 * Metrics are returned in user space, whether they are obtained from
 * the currently selected font in a  #cairo_t or from a #cairo_scaled_font_t
 * which is a font specialized to a particular scale matrix, CTM, and target
 * surface.
 *
 * The font's view
 * ---------------
 *
 * Fonts are designed and stored (in say .ttf files) in "font space", which
 * describes an "EM Square" (a design tile) and has some abstract number
 * such as 1000, 1024, or 2048 units per "EM". This is basically an
 * uninteresting space for us, but we need to remember that it exists.
 *
 * Font resources (from libraries or operating systems) render themselves
 * to a particular device. Since they do not want to make most programmers
 * worry about the font design space, the scaling API is simplified to
 * involve just telling the font the required pixel size of the EM square
 * (that is, in device space).
 *
 *
 * Cairo's gstate view
 * -------------------
 *
 * In addition to the CTM and CTM inverse, we keep a matrix in the gstate
 * called the "font matrix" which describes the user's most recent
 * font-scaling or font-transforming request. This is kept in terms of an
 * abstract scale factor, composed with the CTM and used to set the font's
 * pixel size. So if the user asks to "scale the font by 12", the matrix
 * is:
 *
 *   [ 12.0, 0.0, 0.0, 12.0, 0.0, 0.0 ]
 *
 * It is an affine matrix, like all cairo matrices, where its tx and ty
 * components are used to "nudging" fonts around and are handled in gstate
 * and then ignored by the "scaled-font" layer.
 *
 * In order to perform any action on a font, we must build an object
 * called a #cairo_font_scale_t; this contains the central 2x2 matrix
 * resulting from "font matrix * CTM" (sans the font matrix translation
 * components as stated in the previous paragraph).
 *
 * We pass this to the font when making requests of it, which causes it to
 * reply for a particular [user request, device] combination, under the CTM
 * (to accommodate the "zoom in" == "bigger fonts" issue above).
 *
 * The other terms in our communication with the font are therefore in
 * device space. When we ask it to perform text->glyph conversion, it will
 * produce a glyph string in device space. Glyph vectors we pass to it for
 * measuring or rendering should be in device space. The metrics which we
 * get back from the font will be in device space. The contents of the
 * global glyph image cache will be in device space.
 *
 *
 * Cairo's public view
 * -------------------
 *
 * Since the values entering and leaving via public API calls are in user
 * space, the gstate functions typically need to multiply arguments by the
 * CTM (for user-input glyph vectors), and return values by the CTM inverse
 * (for font responses such as metrics or glyph vectors).
 *
 */

static cairo_status_t
_cairo_gstate_ensure_font_face (cairo_gstate_t *gstate)
{
    cairo_font_face_t *font_face;

    if (gstate->font_face != NULL)
	return gstate->font_face->status;


    font_face = _cairo_toy_font_face_create (CAIRO_FONT_FAMILY_DEFAULT,
					     CAIRO_FONT_SLANT_DEFAULT,
					     CAIRO_FONT_WEIGHT_DEFAULT);
    if (font_face->status)
	return font_face->status;

    gstate->font_face = font_face;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gstate_ensure_scaled_font (cairo_gstate_t *gstate)
{
    cairo_status_t status;
    cairo_font_options_t options;
    cairo_scaled_font_t *scaled_font;

    if (gstate->scaled_font != NULL)
	return gstate->scaled_font->status;

    status = _cairo_gstate_ensure_font_face (gstate);
    if (status)
	return status;

    cairo_surface_get_font_options (gstate->target, &options);
    cairo_font_options_merge (&options, &gstate->font_options);

    scaled_font = cairo_scaled_font_create (gstate->font_face,
				            &gstate->font_matrix,
					    &gstate->ctm,
					    &options);

    status = cairo_scaled_font_status (scaled_font);
    if (status)
	return status;

    gstate->scaled_font = scaled_font;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_get_font_extents (cairo_gstate_t *gstate,
				cairo_font_extents_t *extents)
{
    cairo_status_t status = _cairo_gstate_ensure_scaled_font (gstate);
    if (status)
	return status;

    cairo_scaled_font_extents (gstate->scaled_font, extents);

    return cairo_scaled_font_status (gstate->scaled_font);
}

cairo_status_t
_cairo_gstate_text_to_glyphs (cairo_gstate_t *gstate,
			      const char     *utf8,
			      double	      x,
			      double	      y,
			      cairo_glyph_t **glyphs,
			      int	     *num_glyphs)
{
    cairo_status_t status;

    status = _cairo_gstate_ensure_scaled_font (gstate);
    if (status)
	return status;

    return _cairo_scaled_font_text_to_glyphs (gstate->scaled_font, x, y,
					      utf8, glyphs, num_glyphs);
}

cairo_status_t
_cairo_gstate_set_font_face (cairo_gstate_t    *gstate,
			     cairo_font_face_t *font_face)
{
    if (font_face && font_face->status)
	return font_face->status;

    if (font_face != gstate->font_face) {
	cairo_font_face_destroy (gstate->font_face);
	gstate->font_face = cairo_font_face_reference (font_face);
    }

    _cairo_gstate_unset_scaled_font (gstate);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_glyph_extents (cairo_gstate_t *gstate,
			     const cairo_glyph_t *glyphs,
			     int num_glyphs,
			     cairo_text_extents_t *extents)
{
    cairo_status_t status;

    status = _cairo_gstate_ensure_scaled_font (gstate);
    if (status)
	return status;

    cairo_scaled_font_glyph_extents (gstate->scaled_font,
				     glyphs, num_glyphs,
				     extents);

    return cairo_scaled_font_status (gstate->scaled_font);
}

cairo_status_t
_cairo_gstate_show_glyphs (cairo_gstate_t *gstate,
			   const cairo_glyph_t *glyphs,
			   int num_glyphs)
{
    cairo_status_t status;
    cairo_pattern_union_t source_pattern;
    cairo_glyph_t *transformed_glyphs;
    cairo_glyph_t stack_transformed_glyphs[CAIRO_STACK_ARRAY_LENGTH (cairo_glyph_t)];

    if (gstate->source->status)
	return gstate->source->status;

    status = _cairo_surface_set_clip (gstate->target, &gstate->clip);
    if (status)
	return status;

    status = _cairo_gstate_ensure_scaled_font (gstate);
    if (status)
	return status;

    if (num_glyphs <= ARRAY_LENGTH (stack_transformed_glyphs)) {
	transformed_glyphs = stack_transformed_glyphs;
    } else {
	transformed_glyphs = _cairo_malloc_ab (num_glyphs, sizeof(cairo_glyph_t));
	if (transformed_glyphs == NULL)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    _cairo_gstate_transform_glyphs_to_backend (gstate, glyphs, num_glyphs,
                                               transformed_glyphs);

    status = _cairo_gstate_copy_transformed_source (gstate, &source_pattern.base);
    if (status)
	goto CLEANUP_GLYPHS;

    status = _cairo_surface_show_glyphs (gstate->target,
					 gstate->op,
					 &source_pattern.base,
					 transformed_glyphs,
					 num_glyphs,
					 gstate->scaled_font);

    _cairo_pattern_fini (&source_pattern.base);

CLEANUP_GLYPHS:
    if (transformed_glyphs != stack_transformed_glyphs)
      free (transformed_glyphs);

    return status;
}

cairo_status_t
_cairo_gstate_glyph_path (cairo_gstate_t      *gstate,
			  const cairo_glyph_t *glyphs,
			  int		       num_glyphs,
			  cairo_path_fixed_t  *path)
{
    cairo_status_t status;
    cairo_glyph_t *transformed_glyphs;
    cairo_glyph_t stack_transformed_glyphs[CAIRO_STACK_ARRAY_LENGTH (cairo_glyph_t)];

    status = _cairo_gstate_ensure_scaled_font (gstate);
    if (status)
	return status;

    if (num_glyphs < ARRAY_LENGTH (stack_transformed_glyphs))
      transformed_glyphs = stack_transformed_glyphs;
    else
      transformed_glyphs = _cairo_malloc_ab (num_glyphs, sizeof(cairo_glyph_t));
    if (transformed_glyphs == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    _cairo_gstate_transform_glyphs_to_backend (gstate, glyphs, num_glyphs,
                                               transformed_glyphs);

    CAIRO_MUTEX_LOCK (gstate->scaled_font->mutex);
    status = _cairo_scaled_font_glyph_path (gstate->scaled_font,
					    transformed_glyphs, num_glyphs,
					    path);
    CAIRO_MUTEX_UNLOCK (gstate->scaled_font->mutex);

    if (transformed_glyphs != stack_transformed_glyphs)
      free (transformed_glyphs);

    return status;
}

cairo_status_t
_cairo_gstate_set_antialias (cairo_gstate_t *gstate,
			     cairo_antialias_t antialias)
{
    gstate->antialias = antialias;

    return CAIRO_STATUS_SUCCESS;
}

cairo_antialias_t
_cairo_gstate_get_antialias (cairo_gstate_t *gstate)
{
    return gstate->antialias;
}

/**
 * _cairo_gstate_transform_glyphs_to_backend:
 * @gstate: a #cairo_gstate_t
 * @glyphs: the array of #cairo_glyph_t objects to be transformed
 * @num_glyphs: the number of elements in @glyphs
 * @transformed_glyphs: a pre-allocated array of at least @num_glyphs
 * #cairo_glyph_t objects
 *
 * Transform an array of glyphs to backend space by first adding the offset
 * of the font matrix, then transforming from user space to backend space.
 * The result of the transformation is placed in @transformed_glyphs.
 **/
static void
_cairo_gstate_transform_glyphs_to_backend (cairo_gstate_t      *gstate,
                                           const cairo_glyph_t *glyphs,
                                           int                  num_glyphs,
                                           cairo_glyph_t *transformed_glyphs)
{
    int i;
    cairo_matrix_t *ctm = &gstate->ctm;
    cairo_matrix_t *device_transform = &gstate->target->device_transform;

    if (_cairo_matrix_is_identity (ctm) &&
        _cairo_matrix_is_identity (device_transform) &&
	gstate->font_matrix.x0 == 0 && gstate->font_matrix.y0 == 0)
    {
        memcpy (transformed_glyphs, glyphs, num_glyphs * sizeof (cairo_glyph_t));
    }
    else if (_cairo_matrix_is_translation (ctm) &&
             _cairo_matrix_is_translation (device_transform))
    {
        double tx = gstate->font_matrix.x0 + ctm->x0 + device_transform->x0;
        double ty = gstate->font_matrix.y0 + ctm->y0 + device_transform->y0;

        for (i = 0; i < num_glyphs; i++)
        {
            transformed_glyphs[i].index = glyphs[i].index;
            transformed_glyphs[i].x = glyphs[i].x + tx;
            transformed_glyphs[i].y = glyphs[i].y + ty;
        }
    }
    else
    {
        cairo_matrix_t aggregate_transform;

        cairo_matrix_init_translate (&aggregate_transform,
                                     gstate->font_matrix.x0,
                                     gstate->font_matrix.y0);
        cairo_matrix_multiply (&aggregate_transform,
                               &aggregate_transform, ctm);
        cairo_matrix_multiply (&aggregate_transform,
                               &aggregate_transform, device_transform);

        for (i = 0; i < num_glyphs; i++)
        {
            transformed_glyphs[i] = glyphs[i];
            cairo_matrix_transform_point (&aggregate_transform,
                                          &transformed_glyphs[i].x,
                                          &transformed_glyphs[i].y);
        }
    }
}
