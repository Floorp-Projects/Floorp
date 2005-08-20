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

#include "cairo-clip-private.h"
#include "cairo-gstate-private.h"

static cairo_status_t
_cairo_gstate_init (cairo_gstate_t  *gstate,
		    cairo_surface_t *target);

static cairo_status_t
_cairo_gstate_init_copy (cairo_gstate_t *gstate, cairo_gstate_t *other);

static void
_cairo_gstate_fini (cairo_gstate_t *gstate);

static cairo_status_t
_cairo_gstate_clip_and_composite_trapezoids (cairo_gstate_t *gstate,
					     cairo_traps_t  *traps);

static cairo_status_t
_cairo_gstate_ensure_font_face (cairo_gstate_t *gstate);

static cairo_status_t
_cairo_gstate_ensure_scaled_font (cairo_gstate_t *gstate);

static void
_cairo_gstate_unset_scaled_font (cairo_gstate_t *gstate);

cairo_gstate_t *
_cairo_gstate_create (cairo_surface_t *target)
{
    cairo_status_t status;
    cairo_gstate_t *gstate;

    gstate = malloc (sizeof (cairo_gstate_t));

    if (gstate)
    {
	status = _cairo_gstate_init (gstate, target);
	if (status) {
	    free (gstate);
	    return NULL;		
	}
    }

    return gstate;
}

static cairo_status_t
_cairo_gstate_init (cairo_gstate_t  *gstate,
		    cairo_surface_t *target)
{
    gstate->operator = CAIRO_GSTATE_OPERATOR_DEFAULT;

    gstate->tolerance = CAIRO_GSTATE_TOLERANCE_DEFAULT;
    gstate->antialias = CAIRO_ANTIALIAS_DEFAULT;

    gstate->line_width = CAIRO_GSTATE_LINE_WIDTH_DEFAULT;
    gstate->line_cap = CAIRO_GSTATE_LINE_CAP_DEFAULT;
    gstate->line_join = CAIRO_GSTATE_LINE_JOIN_DEFAULT;
    gstate->miter_limit = CAIRO_GSTATE_MITER_LIMIT_DEFAULT;

    gstate->fill_rule = CAIRO_GSTATE_FILL_RULE_DEFAULT;

    gstate->dash = NULL;
    gstate->num_dashes = 0;
    gstate->dash_offset = 0.0;

    gstate->font_face = NULL;
    gstate->scaled_font = NULL;

    cairo_matrix_init_scale (&gstate->font_matrix,
			     CAIRO_GSTATE_DEFAULT_FONT_SIZE, 
			     CAIRO_GSTATE_DEFAULT_FONT_SIZE);

    _cairo_font_options_init_default (&gstate->font_options);
    
    _cairo_clip_init (&gstate->clip, target);

    _cairo_gstate_identity_matrix (gstate);
    cairo_matrix_init_identity (&gstate->source_ctm_inverse);

    _cairo_pen_init_empty (&gstate->pen_regular);

    gstate->target = cairo_surface_reference (target);

    gstate->source = _cairo_pattern_create_solid (CAIRO_COLOR_BLACK);
    if (gstate->source->status)
	return CAIRO_STATUS_NO_MEMORY;

    gstate->next = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gstate_init_copy (cairo_gstate_t *gstate, cairo_gstate_t *other)
{
    cairo_status_t status;
    cairo_gstate_t *next;
    
    /* Copy all members, but don't smash the next pointer */
    next = gstate->next;
    *gstate = *other;
    gstate->next = next;

    /* Now fix up pointer data that needs to be cloned/referenced */
    if (other->dash) {
	gstate->dash = malloc (other->num_dashes * sizeof (double));
	if (gstate->dash == NULL)
	    return CAIRO_STATUS_NO_MEMORY;
	memcpy (gstate->dash, other->dash, other->num_dashes * sizeof (double));
    }

    _cairo_clip_init_copy (&gstate->clip, &other->clip);

    if (gstate->font_face)
	cairo_font_face_reference (gstate->font_face);

    if (gstate->scaled_font)
	cairo_scaled_font_reference (gstate->scaled_font);
    
    cairo_surface_reference (gstate->target);

    cairo_pattern_reference (gstate->source);
    
    status = _cairo_pen_init_copy (&gstate->pen_regular, &other->pen_regular);
    if (status)
	goto CLEANUP_FONT;

    return status;

  CLEANUP_FONT:
    cairo_scaled_font_destroy (gstate->scaled_font);
    gstate->scaled_font = NULL;
    
    free (gstate->dash);
    gstate->dash = NULL;

    return CAIRO_STATUS_NO_MEMORY;
}

void
_cairo_gstate_fini (cairo_gstate_t *gstate)
{
    if (gstate->font_face)
	cairo_font_face_destroy (gstate->font_face);

    if (gstate->scaled_font)
	cairo_scaled_font_destroy (gstate->scaled_font);

    if (gstate->target) {
	cairo_surface_destroy (gstate->target);
	gstate->target = NULL;
    }

    _cairo_clip_fini (&gstate->clip);

    cairo_pattern_destroy (gstate->source);

    _cairo_pen_fini (&gstate->pen_regular);

    if (gstate->dash) {
	free (gstate->dash);
	gstate->dash = NULL;
    }
}

void
_cairo_gstate_destroy (cairo_gstate_t *gstate)
{
    _cairo_gstate_fini (gstate);
    free (gstate);
}

cairo_gstate_t*
_cairo_gstate_clone (cairo_gstate_t *gstate)
{
    cairo_status_t status;
    cairo_gstate_t *clone;

    clone = malloc (sizeof (cairo_gstate_t));
    if (clone) {
	status = _cairo_gstate_init_copy (clone, gstate);
	if (status) {
	    free (clone);
	    return NULL;
	}
    }
    clone->next = NULL;

    return clone;
}

/* Push rendering off to an off-screen group. */
/* XXX: Rethinking this API
cairo_status_t
_cairo_gstate_begin_group (cairo_gstate_t *gstate)
{
    Pixmap pix;
    unsigned int width, height;

    gstate->parent_surface = gstate->target;

    width = _cairo_surface_get_width (gstate->target);
    height = _cairo_surface_get_height (gstate->target);

    pix = XCreatePixmap (gstate->dpy,
			 _cairo_surface_get_drawable (gstate->target),
			 width, height,
			 _cairo_surface_get_depth (gstate->target));
    if (pix == 0)
	return CAIRO_STATUS_NO_MEMORY;

    gstate->target = cairo_surface_create (gstate->dpy);
    if (gstate->target->status)
	return gstate->target->status;

    _cairo_surface_set_drawableWH (gstate->target, pix, width, height);

    status = _cairo_surface_fill_rectangle (gstate->target,
                                   CAIRO_OPERATOR_SOURCE,
				   &CAIRO_COLOR_TRANSPARENT,
				   0, 0,
			           _cairo_surface_get_width (gstate->target),
				   _cairo_surface_get_height (gstate->target));
    if (status)				 
        return status;

    return CAIRO_STATUS_SUCCESS;
}
*/

/* Complete the current offscreen group, composing its contents onto the parent surface. */
/* XXX: Rethinking this API
cairo_status_t
_cairo_gstate_end_group (cairo_gstate_t *gstate)
{
    Pixmap pix;
    cairo_color_t mask_color;
    cairo_surface_t mask;

    if (gstate->parent_surface == NULL)
	return CAIRO_STATUS_INVALID_POP_GROUP;

    _cairo_surface_init (&mask, gstate->dpy);
    _cairo_color_init (&mask_color);

    _cairo_surface_set_solid_color (&mask, &mask_color);

    * XXX: This could be made much more efficient by using
       _cairo_surface_get_damaged_width/Height if cairo_surface_t actually kept
       track of such informaton. *
    _cairo_surface_composite (gstate->operator,
			      gstate->target,
			      mask,
			      gstate->parent_surface,
			      0, 0,
			      0, 0,
			      0, 0,
			      _cairo_surface_get_width (gstate->target),
			      _cairo_surface_get_height (gstate->target));

    _cairo_surface_fini (&mask);

    pix = _cairo_surface_get_drawable (gstate->target);
    XFreePixmap (gstate->dpy, pix);

    cairo_surface_destroy (gstate->target);
    gstate->target = gstate->parent_surface;
    gstate->parent_surface = NULL;

    return CAIRO_STATUS_SUCCESS;
}
*/

cairo_surface_t *
_cairo_gstate_get_target (cairo_gstate_t *gstate)
{
    return gstate->target;
}

cairo_status_t
_cairo_gstate_set_source (cairo_gstate_t  *gstate,
			  cairo_pattern_t *source)
{
    if (source->status)
	return source->status;

    cairo_pattern_reference (source);
    cairo_pattern_destroy (gstate->source);
    gstate->source = source;
    gstate->source_ctm_inverse = gstate->ctm_inverse;
    
    return CAIRO_STATUS_SUCCESS;
}

cairo_pattern_t *
_cairo_gstate_get_source (cairo_gstate_t *gstate)
{
    if (gstate == NULL)
	return NULL;

    return gstate->source;
}

cairo_status_t
_cairo_gstate_set_operator (cairo_gstate_t *gstate, cairo_operator_t operator)
{
    gstate->operator = operator;

    return CAIRO_STATUS_SUCCESS;
}

cairo_operator_t
_cairo_gstate_get_operator (cairo_gstate_t *gstate)
{
    return gstate->operator;
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
    gstate->line_width = width;

    return CAIRO_STATUS_SUCCESS;
}

double
_cairo_gstate_get_line_width (cairo_gstate_t *gstate)
{
    return gstate->line_width;
}

cairo_status_t
_cairo_gstate_set_line_cap (cairo_gstate_t *gstate, cairo_line_cap_t line_cap)
{
    gstate->line_cap = line_cap;

    return CAIRO_STATUS_SUCCESS;
}

cairo_line_cap_t
_cairo_gstate_get_line_cap (cairo_gstate_t *gstate)
{
    return gstate->line_cap;
}

cairo_status_t
_cairo_gstate_set_line_join (cairo_gstate_t *gstate, cairo_line_join_t line_join)
{
    gstate->line_join = line_join;

    return CAIRO_STATUS_SUCCESS;
}

cairo_line_join_t
_cairo_gstate_get_line_join (cairo_gstate_t *gstate)
{
    return gstate->line_join;
}

cairo_status_t
_cairo_gstate_set_dash (cairo_gstate_t *gstate, double *dash, int num_dashes, double offset)
{
    if (gstate->dash) {
	free (gstate->dash);
	gstate->dash = NULL;
    }
    
    gstate->num_dashes = num_dashes;
    if (gstate->num_dashes) {
	gstate->dash = malloc (gstate->num_dashes * sizeof (double));
	if (gstate->dash == NULL) {
	    gstate->num_dashes = 0;
	    return CAIRO_STATUS_NO_MEMORY;
	}
    }

    memcpy (gstate->dash, dash, gstate->num_dashes * sizeof (double));
    gstate->dash_offset = offset;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_set_miter_limit (cairo_gstate_t *gstate, double limit)
{
    gstate->miter_limit = limit;

    return CAIRO_STATUS_SUCCESS;
}

double
_cairo_gstate_get_miter_limit (cairo_gstate_t *gstate)
{
    return gstate->miter_limit;
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

    _cairo_gstate_unset_scaled_font (gstate);
    
    cairo_matrix_init_translate (&tmp, tx, ty);
    cairo_matrix_multiply (&gstate->ctm, &tmp, &gstate->ctm);

    cairo_matrix_init_translate (&tmp, -tx, -ty);
    cairo_matrix_multiply (&gstate->ctm_inverse, &gstate->ctm_inverse, &tmp);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_scale (cairo_gstate_t *gstate, double sx, double sy)
{
    cairo_matrix_t tmp;

    if (sx == 0 || sy == 0)
	return CAIRO_STATUS_INVALID_MATRIX;

    _cairo_gstate_unset_scaled_font (gstate);
    
    cairo_matrix_init_scale (&tmp, sx, sy);
    cairo_matrix_multiply (&gstate->ctm, &tmp, &gstate->ctm);

    cairo_matrix_init_scale (&tmp, 1/sx, 1/sy);
    cairo_matrix_multiply (&gstate->ctm_inverse, &gstate->ctm_inverse, &tmp);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_rotate (cairo_gstate_t *gstate, double angle)
{
    cairo_matrix_t tmp;

    _cairo_gstate_unset_scaled_font (gstate);
    
    cairo_matrix_init_rotate (&tmp, angle);
    cairo_matrix_multiply (&gstate->ctm, &tmp, &gstate->ctm);

    cairo_matrix_init_rotate (&tmp, -angle);
    cairo_matrix_multiply (&gstate->ctm_inverse, &gstate->ctm_inverse, &tmp);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_transform (cairo_gstate_t	      *gstate,
			 const cairo_matrix_t *matrix)
{
    cairo_matrix_t tmp;

    _cairo_gstate_unset_scaled_font (gstate);
    
    tmp = *matrix;
    cairo_matrix_multiply (&gstate->ctm, &tmp, &gstate->ctm);

    cairo_matrix_invert (&tmp);
    cairo_matrix_multiply (&gstate->ctm_inverse, &gstate->ctm_inverse, &tmp);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_set_matrix (cairo_gstate_t       *gstate,
			  const cairo_matrix_t *matrix)
{
    cairo_status_t status;

    _cairo_gstate_unset_scaled_font (gstate);
    
    gstate->ctm = *matrix;

    gstate->ctm_inverse = *matrix;
    status = cairo_matrix_invert (&gstate->ctm_inverse);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_identity_matrix (cairo_gstate_t *gstate)
{
    _cairo_gstate_unset_scaled_font (gstate);
    
    cairo_matrix_init_identity (&gstate->ctm);
    cairo_matrix_init_identity (&gstate->ctm_inverse);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_user_to_device (cairo_gstate_t *gstate, double *x, double *y)
{
    cairo_matrix_transform_point (&gstate->ctm, x, y);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_user_to_device_distance (cairo_gstate_t *gstate,
				       double *dx, double *dy)
{
    cairo_matrix_transform_distance (&gstate->ctm, dx, dy);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_device_to_user (cairo_gstate_t *gstate, double *x, double *y)
{
    cairo_matrix_transform_point (&gstate->ctm_inverse, x, y);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_device_to_user_distance (cairo_gstate_t *gstate,
				       double *dx, double *dy)
{
    cairo_matrix_transform_distance (&gstate->ctm_inverse, dx, dy);

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_gstate_user_to_backend (cairo_gstate_t *gstate, double *x, double *y)
{
    cairo_matrix_transform_point (&gstate->ctm, x, y);
    if (gstate->target) {
	*x += gstate->target->device_x_offset;
	*y += gstate->target->device_y_offset;
    }
}

void
_cairo_gstate_backend_to_user (cairo_gstate_t *gstate, double *x, double *y)
{
    if (gstate->target) {
	*x -= gstate->target->device_x_offset;
	*y -= gstate->target->device_y_offset;
    }
    cairo_matrix_transform_point (&gstate->ctm_inverse, x, y);
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

static void
_cairo_gstate_copy_transformed_pattern (cairo_gstate_t  *gstate,
					cairo_pattern_t *pattern,
					cairo_pattern_t *original,
					cairo_matrix_t  *ctm_inverse)
{
    cairo_matrix_t tmp_matrix = *ctm_inverse;
  
    _cairo_pattern_init_copy (pattern, original);

    if (gstate->target)
	cairo_matrix_translate (&tmp_matrix,
				- gstate->target->device_x_offset,
				- gstate->target->device_y_offset);

    _cairo_pattern_transform (pattern, &tmp_matrix);
}

static void
_cairo_gstate_copy_transformed_source (cairo_gstate_t  *gstate,
				       cairo_pattern_t *pattern)
{
    _cairo_gstate_copy_transformed_pattern (gstate, pattern,
					    gstate->source,
					    &gstate->source_ctm_inverse);
}

static void
_cairo_gstate_copy_transformed_mask (cairo_gstate_t  *gstate,
				     cairo_pattern_t *pattern,
				     cairo_pattern_t *mask)
{
    _cairo_gstate_copy_transformed_pattern (gstate, pattern,
					    mask,
					    &gstate->ctm_inverse);
}

cairo_status_t
_cairo_gstate_paint (cairo_gstate_t *gstate)
{
    cairo_rectangle_t rectangle;
    cairo_status_t status;
    cairo_box_t box;
    cairo_traps_t traps;

    if (gstate->source->status)
	return gstate->source->status;

    status = _cairo_surface_set_clip (gstate->target, &gstate->clip);
    if (status)
	return status;

    status = _cairo_surface_get_extents (gstate->target, &rectangle);
    if (status)
	return status;
    status = _cairo_clip_intersect_to_rectangle (&gstate->clip, &rectangle);
    if (status)
	return status;

    box.p1.x = _cairo_fixed_from_int (rectangle.x);
    box.p1.y = _cairo_fixed_from_int (rectangle.y);
    box.p2.x = _cairo_fixed_from_int (rectangle.x + rectangle.width);
    box.p2.y = _cairo_fixed_from_int (rectangle.y + rectangle.height);
    status = _cairo_traps_init_box (&traps, &box);
    if (status)
	return status;
    
    _cairo_gstate_clip_and_composite_trapezoids (gstate, &traps);

    _cairo_traps_fini (&traps);

    return CAIRO_STATUS_SUCCESS;
}

/**
 * _cairo_operator_bounded:
 * @operator: a #cairo_operator_t
 * 
 * A bounded operator is one where a source or mask pixel
 * of zero results in no effect on the destination image.
 *
 * Unbounded operators often require special handling; if you, for
 * example, draw trapezoids with an unbounded operator, the effect
 * extends past the bounding box of the trapezoids.
 *
 * Return value: %TRUE if the operator is bounded
 **/
cairo_bool_t
_cairo_operator_bounded (cairo_operator_t operator)
{
    switch (operator) {
    case CAIRO_OPERATOR_OVER:
    case CAIRO_OPERATOR_ATOP:
    case CAIRO_OPERATOR_DEST:
    case CAIRO_OPERATOR_DEST_OVER:
    case CAIRO_OPERATOR_DEST_OUT:
    case CAIRO_OPERATOR_XOR:
    case CAIRO_OPERATOR_ADD:
    case CAIRO_OPERATOR_SATURATE:
	return TRUE;
    case CAIRO_OPERATOR_CLEAR:
    case CAIRO_OPERATOR_SOURCE:
    case CAIRO_OPERATOR_OUT:
    case CAIRO_OPERATOR_IN:
    case CAIRO_OPERATOR_DEST_IN:
    case CAIRO_OPERATOR_DEST_ATOP:
	return FALSE;
    }
    
    ASSERT_NOT_REACHED;
    return FALSE;
}

typedef cairo_status_t (*cairo_draw_func_t) (void                    *closure,
					     cairo_operator_t         operator,
					     cairo_pattern_t         *src,
					     cairo_surface_t         *dst,
					     int                      dst_x,
					     int                      dst_y,
					     const cairo_rectangle_t *extents);

/* Handles compositing with a clip surface when the operator allows
 * us to combine the clip with the mask
 */
static cairo_status_t
_cairo_gstate_clip_and_composite_with_mask (cairo_clip_t            *clip,
					    cairo_operator_t         operator,
					    cairo_pattern_t         *src,
					    cairo_draw_func_t        draw_func,
					    void                    *draw_closure,
					    cairo_surface_t         *dst,
					    const cairo_rectangle_t *extents)
{
    cairo_surface_t *intermediate;
    cairo_surface_pattern_t intermediate_pattern;
    cairo_status_t status;

    intermediate = cairo_surface_create_similar (clip->surface,
						 CAIRO_CONTENT_ALPHA,
						 extents->width,
						 extents->height);
    if (intermediate->status)
	return CAIRO_STATUS_NO_MEMORY;

    status = (*draw_func) (draw_closure, CAIRO_OPERATOR_SOURCE,
			   NULL, intermediate,
			   extents->x, extents->y,
			   extents);
    if (status)
	goto CLEANUP_SURFACE;
    
    status = _cairo_clip_combine_to_surface (clip, CAIRO_OPERATOR_IN,
					     intermediate,
					     extents->x, extents->y,
					     extents);
    if (status)
	goto CLEANUP_SURFACE;
    
    _cairo_pattern_init_for_surface (&intermediate_pattern, intermediate);
    
    status = _cairo_surface_composite (operator,
				       src, &intermediate_pattern.base, dst,
				       extents->x,     extents->y,
				       0,              0,
				       extents->x,     extents->y,
				       extents->width, extents->height);

    _cairo_pattern_fini (&intermediate_pattern.base);

 CLEANUP_SURFACE:
    cairo_surface_destroy (intermediate);

    return status;
}

/* Handles compositing with a clip surface when the operator allows
 * us to combine the clip with the mask
 */
static cairo_status_t
_cairo_gstate_clip_and_composite_combine (cairo_clip_t            *clip,
					  cairo_operator_t         operator,
					  cairo_pattern_t         *src,
					  cairo_draw_func_t        draw_func,
					  void                    *draw_closure,
					  cairo_surface_t         *dst,
					  const cairo_rectangle_t *extents)
{
    cairo_surface_t *intermediate;
    cairo_surface_pattern_t dst_pattern;
    cairo_surface_pattern_t intermediate_pattern;
    cairo_status_t status;

    /* We'd be better off here creating a surface identical in format
     * to dst, but we have no way of getting that information.
     * A CAIRO_CONTENT_CLONE or something might be useful.
     */
    intermediate = cairo_surface_create_similar (dst,
						 CAIRO_CONTENT_COLOR_ALPHA,
						 extents->width,
						 extents->height);
    if (intermediate->status)
	return CAIRO_STATUS_NO_MEMORY;

    /* Initialize the intermediate surface from the destination surface
     */
    _cairo_pattern_init_for_surface (&dst_pattern, dst);

    status = _cairo_surface_composite (CAIRO_OPERATOR_SOURCE,
				       &dst_pattern.base, NULL, intermediate,
				       extents->x,     extents->y,
				       0,              0,
				       0,              0,
				       extents->width, extents->height);

    _cairo_pattern_fini (&dst_pattern.base);

    if (status)
	goto CLEANUP_SURFACE;

    status = (*draw_func) (draw_closure, operator,
			   src, intermediate,
			   extents->x, extents->y,
			   extents);
    if (status)
	goto CLEANUP_SURFACE;

    /* Combine that with the clip
     */
    status = _cairo_clip_combine_to_surface (clip, CAIRO_OPERATOR_DEST_IN,
					     intermediate,
					     extents->x, extents->y,					     
					     extents);
    if (status)
	goto CLEANUP_SURFACE;

    /* Punch the clip out of the destination
     */
    status = _cairo_clip_combine_to_surface (clip, CAIRO_OPERATOR_DEST_OUT,
					     dst,
					     0, 0,
					     extents);
    if (status)
	goto CLEANUP_SURFACE;

    /* Now add the two results together
     */
    _cairo_pattern_init_for_surface (&intermediate_pattern, intermediate);
    
    status = _cairo_surface_composite (CAIRO_OPERATOR_ADD,
				       &intermediate_pattern.base, NULL, dst,
				       0,              0,
				       0,              0,
				       extents->x,     extents->y,
				       extents->width, extents->height);

    _cairo_pattern_fini (&intermediate_pattern.base);
    
 CLEANUP_SURFACE:
    cairo_surface_destroy (intermediate);

    return status;
}

static int
_cairo_rectangle_empty (const cairo_rectangle_t *rect)
{
    return rect->width == 0 || rect->height == 0;
}

/**
 * _cairo_gstate_clip_and_composite:
 * @gstate: a #cairo_gstate_t
 * @operator: the operator to draw with
 * @src: source pattern
 * @draw_func: function that can be called to draw with the mask onto a surface.
 * @draw_closure: data to pass to @draw_func.
 * @dst: destination surface
 * @extents: rectangle holding a bounding box for the operation; this
 *           rectangle will be used as the size for the temporary
 *           surface.
 *
 * When there is a surface clip, we typically need to create an intermediate
 * surface. This function handles the logic of creating a temporary surface
 * drawing to it, then compositing the result onto the target surface.
 *
 * @draw_func is to called to draw the mask; it will be called no more
 * than once.
 * 
 * Return value: %CAIRO_STATUS_SUCCESS if the drawing succeeded.
 **/
static cairo_status_t
_cairo_gstate_clip_and_composite (cairo_clip_t            *clip,
				  cairo_operator_t         operator,
				  cairo_pattern_t         *src,
				  cairo_draw_func_t        draw_func,
				  void                    *draw_closure,
				  cairo_surface_t         *dst,
				  const cairo_rectangle_t *extents)
{
    if (_cairo_rectangle_empty (extents))
	/* Nothing to do */
	return CAIRO_STATUS_SUCCESS;

    if (clip->surface)
    {
	if (_cairo_operator_bounded (operator))
	    return _cairo_gstate_clip_and_composite_with_mask (clip, operator,
							       src,
							       draw_func, draw_closure,
							       dst, extents);
	else
	    return _cairo_gstate_clip_and_composite_combine (clip, operator,
							     src,
							     draw_func, draw_closure,
							     dst, extents);
    }
    else
    {
	return (*draw_func) (draw_closure, operator,
			     src, dst,
			     0, 0,
			     extents);
    }
}
			       

static cairo_status_t
_get_mask_extents (cairo_gstate_t    *gstate,
		   cairo_pattern_t   *mask,
		   cairo_rectangle_t *extents)
{
    cairo_status_t status;

    /*
     * XXX should take mask extents into account, but
     * that involves checking the transform and
     * _cairo_operator_bounded (operator)...  For now,
     * be lazy and just use the destination extents
     */
    status = _cairo_surface_get_extents (gstate->target, extents);
    if (status)
	return status;

    return _cairo_clip_intersect_to_rectangle (&gstate->clip, extents);
}

static cairo_status_t
_cairo_gstate_mask_draw_func (void                    *closure,
			      cairo_operator_t         operator,
			      cairo_pattern_t         *src,
			      cairo_surface_t         *dst,
			      int                      dst_x,
			      int                      dst_y,
			      const cairo_rectangle_t *extents)
{
    cairo_pattern_t *mask = closure;

    if (src)
	return _cairo_surface_composite (operator,
					 src, mask, dst,
					 extents->x,         extents->y,
					 extents->x,         extents->y,
					 extents->x - dst_x, extents->y - dst_y,
					 extents->width,     extents->height);
    else
	return _cairo_surface_composite (operator,
					 mask, NULL, dst,
					 extents->x,         extents->y,
					 0,                  0, /* unused */
					 extents->x - dst_x, extents->y - dst_y,
					 extents->width,     extents->height);
}

cairo_status_t
_cairo_gstate_mask (cairo_gstate_t  *gstate,
		    cairo_pattern_t *mask)
{
    cairo_rectangle_t extents;
    cairo_pattern_union_t source_pattern, mask_pattern;
    cairo_status_t status;

    if (mask->status)
	return mask->status;

    if (gstate->source->status)
	return gstate->source->status;

    status = _cairo_surface_set_clip (gstate->target, &gstate->clip);
    if (status)
	return status;

    _cairo_gstate_copy_transformed_source (gstate, &source_pattern.base);
    _cairo_gstate_copy_transformed_mask (gstate, &mask_pattern.base, mask);
    
    _get_mask_extents (gstate, &mask_pattern.base, &extents);
    
    status = _cairo_gstate_clip_and_composite (&gstate->clip, gstate->operator,
					       &source_pattern.base, 
					       _cairo_gstate_mask_draw_func, &mask_pattern.base,
					       gstate->target,
					       &extents);

    _cairo_pattern_fini (&source_pattern.base);
    _cairo_pattern_fini (&mask_pattern.base);

    return status;
}

cairo_status_t
_cairo_gstate_stroke (cairo_gstate_t *gstate, cairo_path_fixed_t *path)
{
    cairo_status_t status;
    cairo_traps_t traps;

    if (gstate->source->status)
	return gstate->source->status;

    if (gstate->line_width <= 0.0)
	return CAIRO_STATUS_SUCCESS;

    status = _cairo_surface_set_clip (gstate->target, &gstate->clip);
    if (status)
	return status;

    _cairo_pen_init (&gstate->pen_regular, gstate->line_width / 2.0, gstate);

    _cairo_traps_init (&traps);

    status = _cairo_path_fixed_stroke_to_traps (path, gstate, &traps);
    if (status) {
	_cairo_traps_fini (&traps);
	return status;
    }

    _cairo_gstate_clip_and_composite_trapezoids (gstate, &traps);

    _cairo_traps_fini (&traps);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_in_stroke (cairo_gstate_t	    *gstate,
			 cairo_path_fixed_t *path,
			 double		     x,
			 double		     y,
			 cairo_bool_t	    *inside_ret)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    cairo_traps_t traps;

    _cairo_gstate_user_to_backend (gstate, &x, &y);

    _cairo_pen_init (&gstate->pen_regular, gstate->line_width / 2.0, gstate);

    _cairo_traps_init (&traps);

    status = _cairo_path_fixed_stroke_to_traps (path, gstate, &traps);
    if (status)
	goto BAIL;

    *inside_ret = _cairo_traps_contain (&traps, x, y);

BAIL:
    _cairo_traps_fini (&traps);

    return status;
}

/* XXX We currently have a confusing mix of boxes and rectangles as
 * exemplified by this function.  A cairo_box_t is a rectangular area
 * represented by the coordinates of the upper left and lower right
 * corners, expressed in fixed point numbers.  A cairo_rectangle_t is
 * also a rectangular area, but represented by the upper left corner
 * and the width and the height, as integer numbers.
 *
 * This function converts a cairo_box_t to a cairo_rectangle_t by
 * increasing the area to the nearest integer coordinates.  We should
 * standardize on cairo_rectangle_t and cairo_rectangle_fixed_t, and
 * this function could be renamed to the more reasonable
 * _cairo_rectangle_fixed_round.
 */

void
_cairo_box_round_to_rectangle (cairo_box_t *box, cairo_rectangle_t *rectangle)
{
    rectangle->x = _cairo_fixed_integer_floor (box->p1.x);
    rectangle->y = _cairo_fixed_integer_floor (box->p1.y);
    rectangle->width = _cairo_fixed_integer_ceil (box->p2.x) - rectangle->x;
    rectangle->height = _cairo_fixed_integer_ceil (box->p2.y) - rectangle->y;
}

void
_cairo_rectangle_intersect (cairo_rectangle_t *dest, cairo_rectangle_t *src)
{
    int x1, y1, x2, y2;

    x1 = MAX (dest->x, src->x);
    y1 = MAX (dest->y, src->y);
    x2 = MIN (dest->x + dest->width, src->x + src->width);
    y2 = MIN (dest->y + dest->height, src->y + src->height);

    if (x1 >= x2 || y1 >= y2) {
	dest->x = 0;
	dest->y = 0;
	dest->width = 0;
	dest->height = 0;
    } else {
	dest->x = x1;
	dest->y = y1;
	dest->width = x2 - x1;
	dest->height = y2 - y1;
    }	
}

/* Composites a region representing a set of trapezoids.
 */
static cairo_status_t
_composite_trap_region (cairo_clip_t      *clip,
			cairo_pattern_t   *src,
			cairo_operator_t   operator,
			cairo_surface_t   *dst,
			pixman_region16_t *trap_region,
			cairo_rectangle_t *extents)
{
    cairo_status_t status;
    cairo_pattern_union_t mask;
    int num_rects = pixman_region_num_rects (trap_region);
    unsigned int clip_serial;

    if (num_rects == 0)
	return CAIRO_STATUS_SUCCESS;
    
    if (num_rects > 1) {
	if (clip->mode != CAIRO_CLIP_MODE_REGION)
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	
	clip_serial = _cairo_surface_allocate_clip_serial (dst);
	status = _cairo_surface_set_clip_region (dst,
						 trap_region,
						 clip_serial);
	if (status)
	    return status;
    }
    
    if (clip->surface)
	_cairo_pattern_init_for_surface (&mask.surface, clip->surface);
	
    status = _cairo_surface_composite (operator,
				       src,
				       clip->surface ? &mask.base : NULL,
				       dst,
				       extents->x, extents->y,
				       extents->x - (clip->surface ? clip->surface_rect.x : 0),
				       extents->y - (clip->surface ? clip->surface_rect.y : 0),
				       extents->x, extents->y,
				       extents->width, extents->height);

    if (clip->surface)
      _cairo_pattern_fini (&mask.base);

    return status;
}

typedef struct {
    cairo_traps_t *traps;
    cairo_antialias_t antialias;
} cairo_composite_traps_info_t;

static cairo_status_t
_composite_traps_draw_func (void                    *closure,
			    cairo_operator_t         operator,
			    cairo_pattern_t         *src,
			    cairo_surface_t         *dst,
			    int                      dst_x,
			    int                      dst_y,
			    const cairo_rectangle_t *extents)
{
    cairo_composite_traps_info_t *info = closure;
    cairo_pattern_union_t pattern;
    cairo_status_t status;
    
    if (dst_x != 0 || dst_y != 0)
	_cairo_traps_translate (info->traps, - dst_x, - dst_y);

    _cairo_pattern_init_solid (&pattern.solid, CAIRO_COLOR_WHITE);
    if (!src)
	src = &pattern.base;
    
    status = _cairo_surface_composite_trapezoids (operator,
						  src, dst, info->antialias,
						  extents->x,         extents->y,
						  extents->x - dst_x, extents->y - dst_y,
						  extents->width,     extents->height,
						  info->traps->traps,
						  info->traps->num_traps);
    _cairo_pattern_fini (&pattern.base);

    return status;
}

/* Warning: This call modifies the coordinates of traps */
cairo_status_t
_cairo_surface_clip_and_composite_trapezoids (cairo_pattern_t *src,
					      cairo_operator_t operator,
					      cairo_surface_t *dst,
					      cairo_traps_t *traps,
					      cairo_clip_t *clip,
					      cairo_antialias_t antialias)
{
    cairo_status_t status;
    pixman_region16_t *trap_region;
    pixman_region16_t *clear_region = NULL;
    cairo_rectangle_t extents;
    cairo_composite_traps_info_t traps_info;
    
    if (traps->num_traps == 0)
	return CAIRO_STATUS_SUCCESS;

    status = _cairo_traps_extract_region (traps, &trap_region);
    if (status)
	return status;

    if (_cairo_operator_bounded (operator))
    {
	if (trap_region) {
	    status = _cairo_clip_intersect_to_region (clip, trap_region);
	    _cairo_region_extents_rectangle (trap_region, &extents);
	} else {
	    cairo_box_t trap_extents;
	    _cairo_traps_extents (traps, &trap_extents);
	    _cairo_box_round_to_rectangle (&trap_extents, &extents);
	    status = _cairo_clip_intersect_to_rectangle (clip, &extents);
	}
    }
    else
    {
	status = _cairo_surface_get_extents (dst, &extents);
	if (status)
	    return status;
	
	if (trap_region && !clip->surface) {
	    /* If we optimize drawing with an unbounded operator to
	     * _cairo_surface_fill_rectangles() or to drawing with a
	     * clip region, then we have an additional region to clear.
	     */
	    status = _cairo_surface_get_extents (dst, &extents);
	    if (status)
		return status;
	    
	    clear_region = _cairo_region_create_from_rectangle (&extents);
	    status = _cairo_clip_intersect_to_region (clip, clear_region);
	    if (status)
		return status;
	    
	    _cairo_region_extents_rectangle (clear_region,  &extents);
	    
	    if (pixman_region_subtract (clear_region, clear_region, trap_region) != PIXMAN_REGION_STATUS_SUCCESS)
		return CAIRO_STATUS_NO_MEMORY;
	    
	    if (!pixman_region_not_empty (clear_region)) {
		pixman_region_destroy (clear_region);
		clear_region = NULL;
	    }
	} else {
	    status = _cairo_clip_intersect_to_rectangle (clip, &extents);
	    if (status)
		return status;
	}
    }
	
    if (status)
	goto out;
    
    if (trap_region)
    {
	if (src->type == CAIRO_PATTERN_SOLID && !clip->surface)
	{
	    /* Solid rectangles special case */
	    status = _cairo_surface_fill_region (dst, operator,
						 &((cairo_solid_pattern_t *)src)->color,
						 trap_region);
	    if (!status && clear_region)
		status = _cairo_surface_fill_region (dst, operator,
						     CAIRO_COLOR_TRANSPARENT,
						     clear_region);

	    goto out;
	}

	if (_cairo_operator_bounded (operator) || !clip->surface)
	{
	    /* For a simple rectangle, we can just use composite(), for more
	     * rectangles, we have to set a clip region. The cost of rasterizing
	     * trapezoids is pretty high for most backends currently, so it's
	     * worthwhile even if a region is needed.
	     *
	     * If we have a clip surface, we set it as the mask; this only works
	     * for bounded operators; for unbounded operators, clip and mask
	     * cannot be interchanged.
	     *
	     * CAIRO_INT_STATUS_UNSUPPORTED will be returned if the region has
	     * more than rectangle and the destination doesn't support clip
	     * regions. In that case, we fall through.
	     */
	    status = _composite_trap_region (clip, src, operator, dst,
					     trap_region, &extents);
	    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    {
		if (!status && clear_region)
		    status = _cairo_surface_fill_region (dst, CAIRO_OPERATOR_CLEAR,
							 CAIRO_COLOR_TRANSPARENT,
							 clear_region);
		goto out;
	    }
	}
    }

    traps_info.traps = traps;
    traps_info.antialias = antialias;

    status = _cairo_gstate_clip_and_composite (clip, operator, src,
					       _composite_traps_draw_func, &traps_info,
					       dst, &extents);

 out:
    if (trap_region)
	pixman_region_destroy (trap_region);
    if (clear_region)
	pixman_region_destroy (clear_region);
    
    return status;
}

/* Warning: This call modifies the coordinates of traps */
static cairo_status_t
_cairo_gstate_clip_and_composite_trapezoids (cairo_gstate_t *gstate,
					     cairo_traps_t  *traps)
{
  cairo_pattern_union_t pattern;
  cairo_status_t status;
  
  _cairo_gstate_copy_transformed_source (gstate, &pattern.base);
  
  status = _cairo_surface_clip_and_composite_trapezoids (&pattern.base,
							 gstate->operator,
							 gstate->target,
							 traps,
							 &gstate->clip,
							 gstate->antialias);

  _cairo_pattern_fini (&pattern.base);

  return status;
}

cairo_status_t
_cairo_gstate_fill (cairo_gstate_t *gstate, cairo_path_fixed_t *path)
{
    cairo_status_t status;
    cairo_traps_t traps;

    if (gstate->source->status)
	return gstate->source->status;
    
    status = _cairo_surface_set_clip (gstate->target, &gstate->clip);
    if (status)
	return status;

    status = _cairo_surface_fill_path (gstate->operator,
				       gstate->source,
				       gstate->target,
				       path,
				       gstate->fill_rule,
				       gstate->tolerance);
    
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;

    _cairo_traps_init (&traps);

    status = _cairo_path_fixed_fill_to_traps (path,
					      gstate->fill_rule,
					      gstate->tolerance,
					      &traps);
    if (status) {
	_cairo_traps_fini (&traps);
	return status;
    }

    _cairo_gstate_clip_and_composite_trapezoids (gstate, &traps);

    _cairo_traps_fini (&traps);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_in_fill (cairo_gstate_t	  *gstate,
		       cairo_path_fixed_t *path,
		       double		   x,
		       double		   y,
		       cairo_bool_t	  *inside_ret)
{
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
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
    return _cairo_surface_copy_page (gstate->target);
}

cairo_status_t
_cairo_gstate_show_page (cairo_gstate_t *gstate)
{
    return _cairo_surface_show_page (gstate->target);
}

cairo_status_t
_cairo_gstate_stroke_extents (cairo_gstate_t	 *gstate,
			      cairo_path_fixed_t *path,
                              double *x1, double *y1,
			      double *x2, double *y2)
{
    cairo_status_t status;
    cairo_traps_t traps;
    cairo_box_t extents;
  
    _cairo_pen_init (&gstate->pen_regular, gstate->line_width / 2.0, gstate);

    _cairo_traps_init (&traps);
  
    status = _cairo_path_fixed_stroke_to_traps (path, gstate, &traps);
    if (status)
	goto BAIL;

    _cairo_traps_extents (&traps, &extents);

    *x1 = _cairo_fixed_to_double (extents.p1.x);
    *y1 = _cairo_fixed_to_double (extents.p1.y);
    *x2 = _cairo_fixed_to_double (extents.p2.x);
    *y2 = _cairo_fixed_to_double (extents.p2.y);

    _cairo_gstate_backend_to_user (gstate, x1, y1);
    _cairo_gstate_backend_to_user (gstate, x2, y2);
  
BAIL:
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
    cairo_box_t extents;
  
    _cairo_traps_init (&traps);
  
    status = _cairo_path_fixed_fill_to_traps (path,
					      gstate->fill_rule,
					      gstate->tolerance,
					      &traps);
    if (status)
	goto BAIL;
  
    _cairo_traps_extents (&traps, &extents);

    *x1 = _cairo_fixed_to_double (extents.p1.x);
    *y1 = _cairo_fixed_to_double (extents.p1.y);
    *x2 = _cairo_fixed_to_double (extents.p2.x);
    *y2 = _cairo_fixed_to_double (extents.p2.y);

    _cairo_gstate_backend_to_user (gstate, x1, y1);
    _cairo_gstate_backend_to_user (gstate, x2, y2);
  
BAIL:
    _cairo_traps_fini (&traps);
  
    return status;
}

cairo_status_t
_cairo_gstate_reset_clip (cairo_gstate_t *gstate)
{
    return _cairo_clip_reset (&gstate->clip);
}

cairo_status_t
_cairo_gstate_clip (cairo_gstate_t *gstate, cairo_path_fixed_t *path)
{
    return _cairo_clip_clip (&gstate->clip,
			     path, gstate->fill_rule, gstate->tolerance,
			     gstate->antialias, gstate->target);
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

    font_face = _cairo_toy_font_face_create (family, slant, weight);
    if (font_face->status)
	return font_face->status;

    _cairo_gstate_set_font_face (gstate, font_face);
    cairo_font_face_destroy (font_face);

    return CAIRO_STATUS_SUCCESS;
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

cairo_status_t
_cairo_gstate_set_font_options (cairo_gstate_t             *gstate,
				const cairo_font_options_t *options)
{
    _cairo_gstate_unset_scaled_font (gstate);

    gstate->font_options = *options;

    return CAIRO_STATUS_SUCCESS;
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
 * which is aa font specialized to a particular scale matrix, CTM, and target
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
 * It is an affine matrix, like all cairo matrices, but its tx and ty
 * components are always set to zero; we don't permit "nudging" fonts
 * around.
 *
 * In order to perform any action on a font, we must build an object
 * called a cairo_font_scale_t; this contains the central 2x2 matrix 
 * resulting from "font matrix * CTM".
 *  
 * We pass this to the font when making requests of it, which causes it to
 * reply for a particular [user request, device] combination, under the CTM
 * (to accomodate the "zoom in" == "bigger fonts" issue above).
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
 * space, the gstate functions typically need to multiply argumens by the
 * CTM (for user-input glyph vectors), and return values by the CTM inverse
 * (for font responses such as metrics or glyph vectors).
 *
 */

static cairo_status_t
_cairo_gstate_ensure_font_face (cairo_gstate_t *gstate)
{
    if (!gstate->font_face) {
	cairo_font_face_t *font_face;

	font_face = _cairo_toy_font_face_create (CAIRO_FONT_FAMILY_DEFAULT,
						 CAIRO_FONT_SLANT_DEFAULT,
						 CAIRO_FONT_WEIGHT_DEFAULT);
	if (font_face->status)
	    return font_face->status;
	else
	    gstate->font_face = font_face;
    }
    
    return CAIRO_STATUS_SUCCESS;
}
    
static cairo_status_t
_cairo_gstate_ensure_scaled_font (cairo_gstate_t *gstate)
{
    cairo_status_t status;
    cairo_font_options_t options;
    
    if (gstate->scaled_font)
	return CAIRO_STATUS_SUCCESS;
    
    status = _cairo_gstate_ensure_font_face (gstate);
    if (status)
	return status;

    cairo_surface_get_font_options (gstate->target, &options);
    cairo_font_options_merge (&options, &gstate->font_options);
    
    gstate->scaled_font = cairo_scaled_font_create (gstate->font_face,
						    &gstate->font_matrix,
						    &gstate->ctm,
						    &options);
    
    if (!gstate->scaled_font)
	return CAIRO_STATUS_NO_MEMORY;

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

    return CAIRO_STATUS_SUCCESS;
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
    int i;

    status = _cairo_gstate_ensure_scaled_font (gstate);
    if (status)
	return status;
    
    status = _cairo_scaled_font_text_to_glyphs (gstate->scaled_font, 
						utf8, glyphs, num_glyphs);

    if (status || !glyphs || !num_glyphs || !(*glyphs) || !(num_glyphs))
	return status;

    /* The font responded in glyph space, starting from (0,0).  Convert to
       user space by applying the font transform, then add any current point
       offset. */

    for (i = 0; i < *num_glyphs; ++i) {
	cairo_matrix_transform_point (&gstate->font_matrix, 
				      &((*glyphs)[i].x),
				      &((*glyphs)[i].y));
	(*glyphs)[i].x += x;
	(*glyphs)[i].y += y;
    }
    
    return CAIRO_STATUS_SUCCESS;
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
			     cairo_glyph_t *glyphs, 
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

    return CAIRO_STATUS_SUCCESS;
}

typedef struct {
    cairo_scaled_font_t *font;
    cairo_glyph_t *glyphs;
    int num_glyphs;
} cairo_show_glyphs_info_t;

static cairo_status_t
_cairo_gstate_show_glyphs_draw_func (void                    *closure,
				     cairo_operator_t         operator,
				     cairo_pattern_t         *src,
				     cairo_surface_t         *dst,
				     int                      dst_x,
				     int                      dst_y,
				     const cairo_rectangle_t *extents)
{
    cairo_show_glyphs_info_t *glyph_info = closure;
    cairo_pattern_union_t pattern;
    cairo_status_t status;

    /* Modifying the glyph array is fine because we know that this function
     * will be called only once, and we've already made a copy of the
     * glyphs in the wrapper.
     */
    if (dst_x != 0 || dst_y != 0) {
	int i;
	
	for (i = 0; i < glyph_info->num_glyphs; ++i)
	{
	    glyph_info->glyphs[i].x -= dst_x;
	    glyph_info->glyphs[i].y -= dst_y;
	}
    }

    _cairo_pattern_init_solid (&pattern.solid, CAIRO_COLOR_WHITE);
    if (!src)
	src = &pattern.base;
    
    status = _cairo_scaled_font_show_glyphs (glyph_info->font, 
					     operator, 
					     src, dst,
					     extents->x,         extents->y,
					     extents->x - dst_x, extents->y - dst_y,
					     extents->width,     extents->height,
					     glyph_info->glyphs,
					     glyph_info->num_glyphs);

    if (src == &pattern.base)
	_cairo_pattern_fini (&pattern.base);

    return status;
}

cairo_status_t
_cairo_gstate_show_glyphs (cairo_gstate_t *gstate, 
			   cairo_glyph_t *glyphs, 
			   int num_glyphs)
{
    cairo_status_t status;
    int i;
    cairo_glyph_t *transformed_glyphs = NULL;
    cairo_pattern_union_t pattern;
    cairo_box_t bbox;
    cairo_rectangle_t extents;
    cairo_show_glyphs_info_t glyph_info;

    if (gstate->source->status)
	return gstate->source->status;

    status = _cairo_surface_set_clip (gstate->target, &gstate->clip);
    if (status)
	return status;

    status = _cairo_gstate_ensure_scaled_font (gstate);
    if (status)
	return status;
    
    transformed_glyphs = malloc (num_glyphs * sizeof(cairo_glyph_t));
    if (transformed_glyphs == NULL)
	return CAIRO_STATUS_NO_MEMORY;
    
    for (i = 0; i < num_glyphs; ++i)
    {
	transformed_glyphs[i] = glyphs[i];
	_cairo_gstate_user_to_backend (gstate,
				       &transformed_glyphs[i].x, 
				       &transformed_glyphs[i].y);
    }

    if (_cairo_operator_bounded (gstate->operator))
    {
	status = _cairo_scaled_font_glyph_bbox (gstate->scaled_font,
						transformed_glyphs, num_glyphs, 
						&bbox);
	if (status)
	    goto CLEANUP_GLYPHS;
	
	_cairo_box_round_to_rectangle (&bbox, &extents);
    }
    else
    {
	status = _cairo_surface_get_extents (gstate->target, &extents);
	if (status)
	    goto CLEANUP_GLYPHS;
    }
    
    status = _cairo_clip_intersect_to_rectangle (&gstate->clip, &extents);
    if (status)
	goto CLEANUP_GLYPHS;
    
    _cairo_gstate_copy_transformed_source (gstate, &pattern.base);

    glyph_info.font = gstate->scaled_font;
    glyph_info.glyphs = transformed_glyphs;
    glyph_info.num_glyphs = num_glyphs;
    
    status = _cairo_gstate_clip_and_composite (&gstate->clip, gstate->operator,
					       &pattern.base,
					       _cairo_gstate_show_glyphs_draw_func, &glyph_info,
					       gstate->target,
					       &extents);

    _cairo_pattern_fini (&pattern.base);
    
 CLEANUP_GLYPHS:
    free (transformed_glyphs);
    
    return status;
}

cairo_status_t
_cairo_gstate_glyph_path (cairo_gstate_t     *gstate,
			  cairo_glyph_t	     *glyphs, 
			  int		      num_glyphs,
			  cairo_path_fixed_t *path)
{
    cairo_status_t status;
    int i;
    cairo_glyph_t *transformed_glyphs = NULL;

    status = _cairo_gstate_ensure_scaled_font (gstate);
    if (status)
	return status;
    
    transformed_glyphs = malloc (num_glyphs * sizeof(cairo_glyph_t));
    if (transformed_glyphs == NULL)
	return CAIRO_STATUS_NO_MEMORY;
    
    for (i = 0; i < num_glyphs; ++i)
    {
	transformed_glyphs[i] = glyphs[i];
	_cairo_gstate_user_to_backend (gstate,
				       &(transformed_glyphs[i].x), 
				       &(transformed_glyphs[i].y));
    }

    status = _cairo_scaled_font_glyph_path (gstate->scaled_font,
					    transformed_glyphs, num_glyphs,
					    path);

    free (transformed_glyphs);
    return status;
}

cairo_private cairo_status_t
_cairo_gstate_set_antialias (cairo_gstate_t *gstate,
			     cairo_antialias_t antialias)
{
    gstate->antialias = antialias;

    return CAIRO_STATUS_SUCCESS;
}

cairo_private cairo_antialias_t
_cairo_gstate_get_antialias (cairo_gstate_t *gstate)
{
    return gstate->antialias;
}

