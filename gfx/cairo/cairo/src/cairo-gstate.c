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

#include "cairo-gstate-private.h"

static cairo_status_t
_cairo_gstate_set_target_surface (cairo_gstate_t  *gstate,
				  cairo_surface_t *surface);

static cairo_status_t
_cairo_gstate_clip_and_composite_trapezoids (cairo_gstate_t *gstate,
					     cairo_pattern_t *src,
					     cairo_operator_t operator,
					     cairo_surface_t *dst,
					     cairo_traps_t *traps);

static cairo_status_t
_cairo_gstate_ensure_font (cairo_gstate_t *gstate);

static cairo_status_t
_cairo_gstate_ensure_font_face (cairo_gstate_t *gstate);

static void
_cairo_gstate_unset_font (cairo_gstate_t *gstate);

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

cairo_status_t
_cairo_gstate_init (cairo_gstate_t  *gstate,
		    cairo_surface_t *target)
{
    cairo_status_t status;

    gstate->operator = CAIRO_GSTATE_OPERATOR_DEFAULT;

    gstate->tolerance = CAIRO_GSTATE_TOLERANCE_DEFAULT;

    gstate->line_width = CAIRO_GSTATE_LINE_WIDTH_DEFAULT;
    gstate->line_cap = CAIRO_GSTATE_LINE_CAP_DEFAULT;
    gstate->line_join = CAIRO_GSTATE_LINE_JOIN_DEFAULT;
    gstate->miter_limit = CAIRO_GSTATE_MITER_LIMIT_DEFAULT;

    gstate->fill_rule = CAIRO_GSTATE_FILL_RULE_DEFAULT;

    gstate->dash = NULL;
    gstate->num_dashes = 0;
    gstate->dash_offset = 0.0;
    gstate->max_dash_length = 0.0;
    gstate->fraction_dash_lit = 0.0;

    gstate->scaled_font = NULL;
    gstate->font_face = NULL;

    cairo_matrix_init_scale (&gstate->font_matrix,
			     CAIRO_GSTATE_DEFAULT_FONT_SIZE, 
			     CAIRO_GSTATE_DEFAULT_FONT_SIZE);
    
    gstate->surface = NULL;
    gstate->surface_level = 0;

    gstate->clip.region = NULL;
    gstate->clip.surface = NULL;

    gstate->source = _cairo_pattern_create_solid (CAIRO_COLOR_BLACK);
    if (!gstate->source)
	return CAIRO_STATUS_NO_MEMORY;    

    _cairo_gstate_identity_matrix (gstate);

    _cairo_pen_init_empty (&gstate->pen_regular);

    gstate->next = NULL;

    status = _cairo_gstate_set_target_surface (gstate, target);
    if (status)
	return status;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
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

    if (other->clip.region)
    {	
	gstate->clip.region = pixman_region_create ();
	pixman_region_copy (gstate->clip.region, other->clip.region);
    }

    if (gstate->font_face)
	cairo_font_face_reference (gstate->font_face);

    if (gstate->scaled_font)
	cairo_scaled_font_reference (gstate->scaled_font);
    
    cairo_surface_reference (gstate->surface);
    cairo_surface_reference (gstate->clip.surface);

    cairo_pattern_reference (gstate->source);
    
    status = _cairo_pen_init_copy (&gstate->pen_regular, &other->pen_regular);
    if (status)
	goto CLEANUP_FONT;

    status = _cairo_surface_begin (gstate->surface);
    if (status)
	goto CLEANUP_PEN;
    gstate->surface_level = gstate->surface->level;

    return status;

  CLEANUP_PEN:
    _cairo_pen_fini (&gstate->pen_regular);

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

    if (gstate->surface) {
	_cairo_surface_end (gstate->surface);
	cairo_surface_destroy (gstate->surface);
	gstate->surface = NULL;
    }

    if (gstate->clip.surface)
	cairo_surface_destroy (gstate->clip.surface);
    gstate->clip.surface = NULL;

    if (gstate->clip.region)
	pixman_region_destroy (gstate->clip.region);
    gstate->clip.region = NULL;

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

    gstate->parent_surface = gstate->surface;

    width = _cairo_surface_get_width (gstate->surface);
    height = _cairo_surface_get_height (gstate->surface);

    pix = XCreatePixmap (gstate->dpy,
			 _cairo_surface_get_drawable (gstate->surface),
			 width, height,
			 _cairo_surface_get_depth (gstate->surface));
    if (pix == 0)
	return CAIRO_STATUS_NO_MEMORY;

    gstate->surface = cairo_surface_create (gstate->dpy);
    if (gstate->surface == NULL)
	return CAIRO_STATUS_NO_MEMORY;

    _cairo_surface_set_drawableWH (gstate->surface, pix, width, height);

    status = _cairo_surface_fill_rectangle (gstate->surface,
                                   CAIRO_OPERATOR_SOURCE,
				   &CAIRO_COLOR_TRANSPARENT,
				   0, 0,
			           _cairo_surface_get_width (gstate->surface),
				   _cairo_surface_get_height (gstate->surface));
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
			      gstate->surface,
			      mask,
			      gstate->parent_surface,
			      0, 0,
			      0, 0,
			      0, 0,
			      _cairo_surface_get_width (gstate->surface),
			      _cairo_surface_get_height (gstate->surface));

    _cairo_surface_fini (&mask);

    pix = _cairo_surface_get_drawable (gstate->surface);
    XFreePixmap (gstate->dpy, pix);

    cairo_surface_destroy (gstate->surface);
    gstate->surface = gstate->parent_surface;
    gstate->parent_surface = NULL;

    return CAIRO_STATUS_SUCCESS;
}
*/

static cairo_status_t
_cairo_gstate_set_target_surface (cairo_gstate_t *gstate, cairo_surface_t *surface)
{
    cairo_status_t status;
    
    if (gstate->surface == surface)
	return CAIRO_STATUS_SUCCESS;
    
    if (surface) {
	status = _cairo_surface_begin_reset_clip (surface);
	if (!CAIRO_OK (status))
	    return status;
    }

    _cairo_gstate_unset_font (gstate);

    if (gstate->surface) {
	_cairo_surface_end (gstate->surface);
	cairo_surface_destroy (gstate->surface);
    }

    gstate->surface = surface;

    /* Sometimes the user wants to return to having no target surface,
     * (just like after cairo_create). This can be useful for forcing
     * the old surface to be destroyed. */
    if (surface == NULL) {
	gstate->surface_level = 0;
	return CAIRO_STATUS_SUCCESS;
    }

    cairo_surface_reference (gstate->surface);
    gstate->surface_level = surface->level;

    _cairo_gstate_identity_matrix (gstate);

    return CAIRO_STATUS_SUCCESS;
}

cairo_surface_t *
_cairo_gstate_get_target (cairo_gstate_t *gstate)
{
    if (gstate == NULL)
	return NULL;

    return gstate->surface;
}

cairo_status_t
_cairo_gstate_set_source (cairo_gstate_t  *gstate,
			  cairo_pattern_t *source)
{
    if (source == NULL)
	return CAIRO_STATUS_NULL_POINTER;

    cairo_pattern_reference (source);
    cairo_pattern_destroy (gstate->source);
    gstate->source = source;
    
    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_set_source_solid (cairo_gstate_t	    *gstate,
				const cairo_color_t *color)
{
    cairo_status_t status;
    cairo_pattern_t *source;

    source = _cairo_pattern_create_solid (color);
    if (!source)
	return CAIRO_STATUS_NO_MEMORY;

    status = _cairo_gstate_set_source (gstate, source);

    cairo_pattern_destroy (source);
    
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
    double length = 0.0, lit = 0.0;
    int i;

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

	gstate->max_dash_length = 0.0;
	for (i = 0; i < num_dashes; i++) {
	    gstate->max_dash_length = MAX(dash[i], gstate->max_dash_length);

	    if (!(i & 1))
		lit += dash[i];
	    length += dash[i];
	}
	gstate->fraction_dash_lit = lit/length;
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

    _cairo_gstate_unset_font (gstate);
    
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

    _cairo_gstate_unset_font (gstate);
    
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

    _cairo_gstate_unset_font (gstate);
    
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

    _cairo_gstate_unset_font (gstate);
    
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

    _cairo_gstate_unset_font (gstate);
    
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
    _cairo_gstate_unset_font (gstate);
    
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
    if (gstate->surface) {
	*x += gstate->surface->device_x_offset;
	*y += gstate->surface->device_y_offset;
    }
}

void
_cairo_gstate_backend_to_user (cairo_gstate_t *gstate, double *x, double *y)
{
    if (gstate->surface) {
	*x -= gstate->surface->device_x_offset;
	*y -= gstate->surface->device_y_offset;
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
_cairo_gstate_pattern_transform (cairo_gstate_t  *gstate,
				 cairo_pattern_t *pattern)
{
    cairo_matrix_t tmp_matrix = gstate->ctm_inverse;
    
    if (gstate->surface)
	cairo_matrix_translate (&tmp_matrix,
				- gstate->surface->device_x_offset,
				- gstate->surface->device_y_offset);

    _cairo_pattern_transform (pattern, &tmp_matrix);
}

cairo_status_t
_cairo_gstate_paint (cairo_gstate_t *gstate)
{
    cairo_rectangle_t rectangle;
    cairo_status_t status;
    cairo_box_t box;
    cairo_traps_t traps;

    if (gstate->surface->level != gstate->surface_level)
	return CAIRO_STATUS_BAD_NESTING;
    
    status = _cairo_surface_get_clip_extents (gstate->surface, &rectangle);
    if (!CAIRO_OK (status))
	return status;

    box.p1.x = _cairo_fixed_from_int (rectangle.x);
    box.p1.y = _cairo_fixed_from_int (rectangle.y);
    box.p2.x = _cairo_fixed_from_int (rectangle.x + rectangle.width);
    box.p2.y = _cairo_fixed_from_int (rectangle.y + rectangle.height);
    status = _cairo_traps_init_box (&traps, &box);
    if (!CAIRO_OK (status))
	return status;
    
    _cairo_gstate_clip_and_composite_trapezoids (gstate,
                                                 gstate->source,
                                                 gstate->operator,
                                                 gstate->surface,
                                                 &traps);

    _cairo_traps_fini (&traps);

    return CAIRO_STATUS_SUCCESS;
}

/* Combines @gstate->clip_surface using the IN operator with
 * the given intermediate surface, which corresponds to the
 * rectangle of the destination space given by @extents.
 */
static cairo_status_t
_cairo_gstate_combine_clip_surface (cairo_gstate_t    *gstate,
				    cairo_surface_t   *intermediate,
				    cairo_rectangle_t *extents)
{
    cairo_pattern_union_t pattern;
    cairo_status_t status;

    _cairo_pattern_init_for_surface (&pattern.surface,
				     gstate->clip.surface);
    
    status = _cairo_surface_composite (CAIRO_OPERATOR_IN,
				       &pattern.base,
				       NULL,
				       intermediate,
				       extents->x - gstate->clip.rect.x,
				       extents->y - gstate->clip.rect.y, 
				       0, 0,
				       0, 0,
				       extents->width, extents->height);
    
    _cairo_pattern_fini (&pattern.base);

    return status;
}

/* Creates a region from a cairo_rectangle_t */
static cairo_status_t
_region_new_from_rect (cairo_rectangle_t  *rect,
		       pixman_region16_t **region)
{
    *region = pixman_region_create ();
    if (pixman_region_union_rect (*region, *region,
				  rect->x, rect->y,
				  rect->width, rect->height) != PIXMAN_REGION_STATUS_SUCCESS) {
	pixman_region_destroy (*region);
	return CAIRO_STATUS_NO_MEMORY;
    }

    return CAIRO_STATUS_SUCCESS;
}

/* Gets the bounding box of a region as a cairo_rectangle_t */
static void
_region_rect_extents (pixman_region16_t *region,
		      cairo_rectangle_t *rect)
{
    pixman_box16_t *region_extents = pixman_region_extents (region);

    rect->x = region_extents->x1;
    rect->y = region_extents->y1;
    rect->width = region_extents->x2 - region_extents->x1;
    rect->height = region_extents->y2 - region_extents->y1;
}

/* Intersects @region with the clipping bounds (both region
 * and surface) of @gstate
 */
static cairo_status_t
_cairo_gstate_intersect_clip (cairo_gstate_t    *gstate,
			      pixman_region16_t *region)
{
    if (gstate->clip.region)
	pixman_region_intersect (region, gstate->clip.region, region);

    if (gstate->clip.surface) {
	pixman_region16_t *clip_rect;
	cairo_status_t status;
    
	status = _region_new_from_rect (&gstate->clip.rect, &clip_rect);
	if (!CAIRO_OK (status))
	    return status;
	
	if (pixman_region_intersect (region,
				     clip_rect,
				     region) != PIXMAN_REGION_STATUS_SUCCESS)
	    status = CAIRO_STATUS_NO_MEMORY;

	pixman_region_destroy (clip_rect);

	if (!CAIRO_OK (status))
	    return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_get_mask_extents (cairo_gstate_t    *gstate,
		   cairo_pattern_t   *mask,
		   cairo_rectangle_t *extents)
{
    cairo_rectangle_t clip_rect;
    pixman_region16_t *clip_region;
    cairo_status_t status;
    
    status = _cairo_surface_get_clip_extents (gstate->surface, &clip_rect);
    if (!CAIRO_OK (status))
	return status;

    status = _region_new_from_rect (&clip_rect, &clip_region);
    if (!CAIRO_OK (status))
	return status;

    status = _cairo_gstate_intersect_clip (gstate, clip_region);
    if (!CAIRO_OK (status))
	return status;

    _region_rect_extents (clip_region, extents);

    pixman_region_destroy (clip_region);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_mask (cairo_gstate_t  *gstate,
		    cairo_pattern_t *mask)
{
    cairo_rectangle_t extents;
    cairo_pattern_union_t pattern;
    cairo_surface_pattern_t intermediate_pattern;
    cairo_pattern_t *effective_mask;
    cairo_status_t status;
    int mask_x, mask_y;

    if (gstate->surface->level != gstate->surface_level)
	return CAIRO_STATUS_BAD_NESTING;
    
    _get_mask_extents (gstate, mask, &extents);
    
    if (gstate->clip.surface) {
	/* When there is clip surface, we'll need to create a
	 * temporary surface that combines the clip and mask
	 */
	cairo_surface_t *intermediate;

	intermediate = cairo_surface_create_similar (gstate->clip.surface,
						     CAIRO_FORMAT_A8,
						     extents.width,
						     extents.height);
	if (intermediate == NULL)
	    return CAIRO_STATUS_NO_MEMORY;

	status = _cairo_surface_composite (CAIRO_OPERATOR_SOURCE,
					   mask, NULL, intermediate,
					   extents.x,     extents.y,
					   0,             0,
					   0,             0,
					   extents.width, extents.height);
	if (!CAIRO_OK (status)) {
	    cairo_surface_destroy (intermediate);
	    return status;
	}
	
	status = _cairo_gstate_combine_clip_surface (gstate, intermediate, &extents);
	if (!CAIRO_OK (status)) {
	    cairo_surface_destroy (intermediate);
	    return status;
	}
	
	_cairo_pattern_init_for_surface (&intermediate_pattern, intermediate);
	cairo_surface_destroy (intermediate);

	effective_mask = &intermediate_pattern.base;
	mask_x = extents.x;
	mask_y = extents.y;
	
    } else {
	effective_mask = mask;
	mask_x = mask_y = 0;
    }

    _cairo_pattern_init_copy (&pattern.base, gstate->source);
    _cairo_gstate_pattern_transform (gstate, &pattern.base);

    status = _cairo_surface_composite (gstate->operator,
				       &pattern.base,
				       effective_mask,
				       gstate->surface,
				       extents.x,          extents.y,
				       extents.x - mask_x, extents.y - mask_y,
				       extents.x,          extents.y,
				       extents.width, extents.height);

    if (gstate->clip.surface)
	_cairo_pattern_fini (&intermediate_pattern.base);

    return status;
}

static cairo_bool_t
_dashes_invisible (cairo_gstate_t *gstate)
{
    if (gstate->dash) {
	double min, max;

	_cairo_matrix_compute_expansion_factors (&gstate->ctm, &min, &max);

	/* Quick and dirty applicaton of Nyquist sampling limit */

	if (min * gstate->max_dash_length < 0.5f)
	    return TRUE;
    }

    return FALSE;
}

cairo_status_t
_cairo_gstate_stroke (cairo_gstate_t *gstate, cairo_path_fixed_t *path)
{
    cairo_status_t status;
    cairo_traps_t traps;
    double *dash = NULL;
    double alpha = 0.0;

    if (gstate->surface->level != gstate->surface_level)
	return CAIRO_STATUS_BAD_NESTING;
    
    if (gstate->line_width <= 0.0)
	return CAIRO_STATUS_SUCCESS;

    if (_dashes_invisible(gstate)) {
	dash = gstate->dash;
	gstate->dash = NULL;

	/* alpha = gstate->alpha; */
	/* gstate->alpha *= gstate->fraction_dash_lit; */
    }

    _cairo_pen_init (&gstate->pen_regular, gstate->line_width / 2.0, gstate);

    _cairo_traps_init (&traps);

    status = _cairo_path_fixed_stroke_to_traps (path, gstate, &traps);
    if (status)
	goto BAIL;

    _cairo_gstate_clip_and_composite_trapezoids (gstate,
                                                 gstate->source,
                                                 gstate->operator,
                                                 gstate->surface,
                                                 &traps);

BAIL:
    _cairo_traps_fini (&traps);

    if (dash) {
	gstate->dash = dash;
	/* gstate->alpha = alpha; */
    }

    return status;
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

static void
_cairo_box_round_to_rectangle (cairo_box_t *box, cairo_rectangle_t *rectangle)
{
    rectangle->x = _cairo_fixed_integer_floor (box->p1.x);
    rectangle->y = _cairo_fixed_integer_floor (box->p1.y);
    rectangle->width = _cairo_fixed_integer_ceil (box->p2.x) - rectangle->x;
    rectangle->height = _cairo_fixed_integer_ceil (box->p2.y) - rectangle->y;
}

static void
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

static int
_cairo_rectangle_empty (cairo_rectangle_t *rect)
{
    return rect->width == 0 || rect->height == 0;
}

/* Given a region representing a set of trapezoids that will be
 * drawn, clip the region according to the gstate and compute
 * the overall extents.
 */
static cairo_status_t
_clip_and_compute_extents_region (cairo_gstate_t    *gstate,
				  pixman_region16_t *trap_region,
				  cairo_rectangle_t *extents)
{
    cairo_status_t status;
    
    status = _cairo_gstate_intersect_clip (gstate, trap_region);
    if (!CAIRO_OK (status))
	return status;

    _region_rect_extents (trap_region, extents);

    return CAIRO_STATUS_SUCCESS;
}

/* Given a a set of trapezoids to draw, find a bounding box (non-exact)
 * of the trapezoids clipped by the gstate
 */
static cairo_status_t
_clip_and_compute_extents_arbitrary (cairo_gstate_t    *gstate,
				     cairo_traps_t     *traps,
				     cairo_rectangle_t *extents)
{
    cairo_box_t trap_extents;
	
    _cairo_traps_extents (traps, &trap_extents);
    _cairo_box_round_to_rectangle (&trap_extents, extents);
    
    if (gstate->clip.region) {
	pixman_region16_t *intersection;
	cairo_status_t status;
	
	status = _region_new_from_rect (extents, &intersection);
	if (!CAIRO_OK (status))
	    return status;
	
	if (pixman_region_intersect (intersection,
				     gstate->clip.region,
				     intersection) == PIXMAN_REGION_STATUS_SUCCESS) 
	    _region_rect_extents (intersection, extents);
	else
	    status = CAIRO_STATUS_NO_MEMORY;

	pixman_region_destroy (intersection);

	if (!CAIRO_OK (status))
	    return status;
    }

    if (gstate->clip.surface)
	_cairo_rectangle_intersect (extents, &gstate->clip.rect);

    return CAIRO_STATUS_SUCCESS;
}

/* Composites a region representing a set of trapezoids.
 */
static cairo_status_t
_composite_trap_region (cairo_gstate_t    *gstate,
			cairo_pattern_t   *src,
			cairo_operator_t   operator,
			cairo_surface_t   *dst,
			pixman_region16_t *trap_region,
			cairo_rectangle_t *extents)
{
    cairo_status_t status, tmp_status;
    cairo_pattern_union_t pattern;
    cairo_pattern_union_t mask;
    int num_rects = pixman_region_num_rects (trap_region);

    if (num_rects == 0)
	return CAIRO_STATUS_SUCCESS;
    
    if (num_rects > 1) {
	status = _cairo_surface_set_clip_region (dst, trap_region);
	if (!CAIRO_OK (status))
	    return status;
    }
    
    _cairo_pattern_init_copy (&pattern.base, src);
    _cairo_gstate_pattern_transform (gstate, &pattern.base);

    if (gstate->clip.surface)
	_cairo_pattern_init_for_surface (&mask.surface, gstate->clip.surface);
	
    status = _cairo_surface_composite (gstate->operator,
				       &pattern.base,
				       gstate->clip.surface ? &mask.base : NULL,
				       dst,
				       extents->x, extents->y,
				       extents->x - (gstate->clip.surface ? gstate->clip.rect.x : 0),
				       extents->y - (gstate->clip.surface ? gstate->clip.rect.y : 0),
				       extents->x, extents->y,
				       extents->width, extents->height);

    _cairo_pattern_fini (&pattern.base);
    if (gstate->clip.surface)
      _cairo_pattern_fini (&mask.base);

    if (num_rects > 1) {
	tmp_status = _cairo_surface_set_clip_region (dst, gstate->clip.region);
	if (!CAIRO_OK (tmp_status))
	    status = tmp_status;
    }
				      
    return status;
}

static void
translate_traps (cairo_traps_t *traps, int x, int y)
{
    cairo_fixed_t xoff, yoff;
    cairo_trapezoid_t *t;
    int i;

    /* Ugh. The cairo_composite/(Render) interface doesn't allow
       an offset for the trapezoids. Need to manually shift all
       the coordinates to align with the offset origin of the
       intermediate surface. */

    xoff = _cairo_fixed_from_int (x);
    yoff = _cairo_fixed_from_int (y);

    for (i = 0, t = traps->traps; i < traps->num_traps; i++, t++) {
	t->top += yoff;
	t->bottom += yoff;
	t->left.p1.x += xoff;
	t->left.p1.y += yoff;
	t->left.p2.x += xoff;
	t->left.p2.y += yoff;
	t->right.p1.x += xoff;
	t->right.p1.y += yoff;
	t->right.p2.x += xoff;
	t->right.p2.y += yoff;
    }
}

/* Composites a set of trapezoids in the case where we need to create
 * an intermediate surface to handle gstate->clip.surface
 * 
 * Warning: This call modifies the coordinates of traps
 */
static cairo_status_t
_composite_traps_intermediate_surface (cairo_gstate_t    *gstate,
				       cairo_pattern_t   *src,
				       cairo_operator_t   operator,
				       cairo_surface_t   *dst,
				       cairo_traps_t     *traps,
				       cairo_rectangle_t *extents)
{
    cairo_pattern_union_t pattern;
    cairo_surface_t *intermediate;
    cairo_surface_pattern_t intermediate_pattern;
    cairo_status_t status;

    translate_traps (traps, -extents->x, -extents->y);

    intermediate = _cairo_surface_create_similar_solid (gstate->clip.surface,
							CAIRO_FORMAT_A8,
							extents->width,
							extents->height,
							CAIRO_COLOR_TRANSPARENT);
    if (intermediate == NULL)
	return CAIRO_STATUS_NO_MEMORY;
    
    _cairo_pattern_init_solid (&pattern.solid, CAIRO_COLOR_WHITE);
    
    status = _cairo_surface_composite_trapezoids (CAIRO_OPERATOR_ADD,
						  &pattern.base,
						  intermediate,
						  extents->x, extents->y,
						  0, 0,
						  extents->width,
						  extents->height,
						  traps->traps,
						  traps->num_traps);
    _cairo_pattern_fini (&pattern.base);
    
    if (!CAIRO_OK (status))
	goto out;

    status = _cairo_gstate_combine_clip_surface (gstate, intermediate, extents);
    if (!CAIRO_OK (status))
	goto out;
    
    _cairo_pattern_init_for_surface (&intermediate_pattern, intermediate);
    _cairo_pattern_init_copy (&pattern.base, src);
    _cairo_gstate_pattern_transform (gstate, &pattern.base);
    
    status = _cairo_surface_composite (operator,
				       &pattern.base,
				       &intermediate_pattern.base,
				       dst,
				       extents->x, extents->y,
				       0, 0,
				       extents->x, extents->y,
				       extents->width, extents->height);
    
    _cairo_pattern_fini (&pattern.base);
    _cairo_pattern_fini (&intermediate_pattern.base);
    
 out:
    cairo_surface_destroy (intermediate);
    
    return status;
}

/* Composites a region representing a set of trapezoids in the
 * case of a solid source (so we can use
 * _cairo_surface_fill_rectangles).
 */
static cairo_status_t
_composite_trap_region_solid (cairo_gstate_t        *gstate,
			      cairo_solid_pattern_t *src,
			      cairo_operator_t       operator,
			      cairo_surface_t       *dst,
			      pixman_region16_t     *region)
{
    int num_rects = pixman_region_num_rects (region);
    pixman_box16_t *boxes = pixman_region_rects (region);
    cairo_rectangle_t *rects;
    cairo_status_t status;
    int i;

    if (!num_rects)
	return CAIRO_STATUS_SUCCESS;
    
    rects = malloc (sizeof (pixman_rectangle_t) * num_rects);
    if (!rects)
	return CAIRO_STATUS_NO_MEMORY;

    for (i = 0; i < num_rects; i++) {
	rects[i].x = boxes[i].x1;
	rects[i].y = boxes[i].y1;
	rects[i].width = boxes[i].x2 - boxes[i].x1;
	rects[i].height = boxes[i].y2 - boxes[i].y1;
    }

    status =  _cairo_surface_fill_rectangles (dst, operator,
					      &src->color, rects, num_rects);
    
    free (rects);

    return status;
}

/* Composites a set of trapezoids in the general case where
   gstate->clip.surface == NULL
 */
static cairo_status_t
_composite_traps (cairo_gstate_t    *gstate,
		  cairo_pattern_t   *src,
		  cairo_operator_t   operator,
		  cairo_surface_t   *dst,
		  cairo_traps_t     *traps,
		  cairo_rectangle_t *extents)
{
    cairo_pattern_union_t pattern;
    cairo_status_t status;

    _cairo_pattern_init_copy (&pattern.base, src);
    _cairo_gstate_pattern_transform (gstate, &pattern.base);
    
    status = _cairo_surface_composite_trapezoids (gstate->operator,
						  &pattern.base, dst,
						  extents->x, extents->y,
						  extents->x, extents->y,
						  extents->width,
						  extents->height,
						  traps->traps,
						  traps->num_traps);

    _cairo_pattern_fini (&pattern.base);

    return status;
}

/* Warning: This call modifies the coordinates of traps */
static cairo_status_t
_cairo_gstate_clip_and_composite_trapezoids (cairo_gstate_t *gstate,
					     cairo_pattern_t *src,
					     cairo_operator_t operator,
					     cairo_surface_t *dst,
					     cairo_traps_t *traps)
{
    cairo_status_t status;
    pixman_region16_t *trap_region;
    cairo_rectangle_t extents;
    
    if (traps->num_traps == 0)
	return CAIRO_STATUS_SUCCESS;

    if (gstate->surface == NULL)
	return CAIRO_STATUS_NO_TARGET_SURFACE;

    status = _cairo_traps_extract_region (traps, &trap_region);
    if (!CAIRO_OK (status))
	return status;

    if (trap_region)
	status = _clip_and_compute_extents_region (gstate, trap_region, &extents);
    else
	status = _clip_and_compute_extents_arbitrary (gstate, traps, &extents);
	
    if (!CAIRO_OK (status))
	goto out;
    
    if (_cairo_rectangle_empty (&extents))
	/* Nothing to do */
	goto out;

    if (gstate->clip.surface) {
	if (trap_region) {
	    /* If we are compositing a set of rectangles, we can set them as the
	     * clip region for the destination surface and use the clip surface
	     * as the mask. A clip region might not be supported, in which case
	     * we fall through to the next method
	     */
	    status = _composite_trap_region (gstate, src, operator, dst,
					     trap_region, &extents);
	    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
		goto out;
	}
	
	/* Handle a clip surface by creating an intermediate surface. */
	status = _composite_traps_intermediate_surface (gstate, src, operator,
							dst, traps, &extents);
    } else {
        /* No clip surface */
	if (trap_region && src->type == CAIRO_PATTERN_SOLID) {
	    /* Solid rectangles are handled specially */
	    status = _composite_trap_region_solid (gstate, (cairo_solid_pattern_t *)src,
						   operator, dst, trap_region);
	} else if (trap_region && pixman_region_num_rects (trap_region) <= 1) {
	    /* For a simple rectangle, we can just use composite(), for more
	     * rectangles, we'd have to set a clip region. That might still
	     * be a win, but it's less obvious. (Depends on the backend)
	     */
	    status = _composite_trap_region (gstate, src, operator, dst,
					     trap_region, &extents);
	} else {
	    status = _composite_traps (gstate, src, operator,
				       dst, traps, &extents);
	}
    }

 out:
    if (trap_region)
	pixman_region_destroy (trap_region);
    
    return status;
}

cairo_status_t
_cairo_gstate_fill (cairo_gstate_t *gstate, cairo_path_fixed_t *path)
{
    cairo_status_t status;
    cairo_traps_t traps;

    if (gstate->surface->level != gstate->surface_level)
	return CAIRO_STATUS_BAD_NESTING;

    status = _cairo_surface_fill_path (gstate->operator,
				       gstate->source,
				       gstate->surface,
				       path);
    
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;

    _cairo_traps_init (&traps);

    status = _cairo_path_fixed_fill_to_traps (path, gstate, &traps);
    if (status) {
	_cairo_traps_fini (&traps);
	return status;
    }

    _cairo_gstate_clip_and_composite_trapezoids (gstate,
                                                 gstate->source,
                                                 gstate->operator,
                                                 gstate->surface,
                                                 &traps);

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

    status = _cairo_path_fixed_fill_to_traps (path, gstate, &traps);
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
    if (gstate->surface == NULL)
	return CAIRO_STATUS_NO_TARGET_SURFACE;

    return _cairo_surface_copy_page (gstate->surface);
}

cairo_status_t
_cairo_gstate_show_page (cairo_gstate_t *gstate)
{
    if (gstate->surface == NULL)
	return CAIRO_STATUS_NO_TARGET_SURFACE;

    return _cairo_surface_show_page (gstate->surface);
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
    double *dash = NULL;

    if (_dashes_invisible(gstate)) {
	dash = gstate->dash;
	gstate->dash = NULL;
    }

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

    if (dash)
	gstate->dash = dash;
  
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
  
    status = _cairo_path_fixed_fill_to_traps (path, gstate, &traps);
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
    if (gstate->surface->level != gstate->surface_level)
	return CAIRO_STATUS_BAD_NESTING;
    
    /* destroy any existing clip-region artifacts */
    if (gstate->clip.surface)
	cairo_surface_destroy (gstate->clip.surface);
    gstate->clip.surface = NULL;

    if (gstate->clip.region)
	pixman_region_destroy (gstate->clip.region);
    gstate->clip.region = NULL;

    /* reset the surface's clip to the whole surface */
    if (gstate->surface)
	_cairo_surface_set_clip_region (gstate->surface, 
					gstate->clip.region);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_clip (cairo_gstate_t *gstate, cairo_path_fixed_t *path)
{
    cairo_status_t status;
    cairo_pattern_union_t pattern;
    cairo_traps_t traps;
    cairo_box_t extents;
    pixman_region16_t *region;

    if (gstate->surface->level != gstate->surface_level)
	return CAIRO_STATUS_BAD_NESTING;
    
    /* Fill the clip region as traps. */

    _cairo_traps_init (&traps);
    status = _cairo_path_fixed_fill_to_traps (path, gstate, &traps);
    if (!CAIRO_OK (status)) {
	_cairo_traps_fini (&traps);
	return status;
    }

    /* Check to see if we can represent these traps as a PixRegion. */

    status = _cairo_traps_extract_region (&traps, &region);
    if (!CAIRO_OK (status)) {
	_cairo_traps_fini (&traps);
	return status;
    }
    
    if (region) {
	status = CAIRO_STATUS_SUCCESS;
	
	if (gstate->clip.region == NULL) {
	    gstate->clip.region = region;
	} else {
	    pixman_region16_t *intersection = pixman_region_create();

	    if (pixman_region_intersect (intersection, 
					 gstate->clip.region, region)
		== PIXMAN_REGION_STATUS_SUCCESS) {
		pixman_region_destroy (gstate->clip.region);
		gstate->clip.region = intersection;
	    } else {		
		status = CAIRO_STATUS_NO_MEMORY;
	    }
	    pixman_region_destroy (region);
	}
	    
	if (CAIRO_OK (status))
	    status = _cairo_surface_set_clip_region (gstate->surface, 
						     gstate->clip.region);
	
	if (status != CAIRO_INT_STATUS_UNSUPPORTED) {
	    _cairo_traps_fini (&traps);
	    return status;
	}

	/* Fall through as status == CAIRO_INT_STATUS_UNSUPPORTED
	   means that backend doesn't support clipping regions and
	   mask surface clipping should be used instead. */
    }

    /* Otherwise represent the clip as a mask surface. */

    if (gstate->clip.surface == NULL) {
	_cairo_traps_extents (&traps, &extents);
	_cairo_box_round_to_rectangle (&extents, &gstate->clip.rect);
	gstate->clip.surface =
	    _cairo_surface_create_similar_solid (gstate->surface,
						 CAIRO_FORMAT_A8,
						 gstate->clip.rect.width,
						 gstate->clip.rect.height,
						 CAIRO_COLOR_WHITE);
	if (gstate->clip.surface == NULL)
	    return CAIRO_STATUS_NO_MEMORY;
    }

    translate_traps (&traps, -gstate->clip.rect.x, -gstate->clip.rect.y);
    _cairo_pattern_init_solid (&pattern.solid, CAIRO_COLOR_WHITE);
    
    status = _cairo_surface_composite_trapezoids (CAIRO_OPERATOR_IN,
						  &pattern.base,
						  gstate->clip.surface,
						  0, 0,
						  0, 0,
						  gstate->clip.rect.width,
						  gstate->clip.rect.height,
						  traps.traps,
						  traps.num_traps);

    _cairo_pattern_fini (&pattern.base);
    
    _cairo_traps_fini (&traps);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_gstate_unset_font (cairo_gstate_t *gstate)
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

    font_face = _cairo_simple_font_face_create (family, slant, weight);
    if (!font_face)
	return CAIRO_STATUS_NO_MEMORY;

    _cairo_gstate_set_font_face (gstate, font_face);
    cairo_font_face_destroy (font_face);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_set_font_size (cairo_gstate_t *gstate, 
			     double          size)
{
    _cairo_gstate_unset_font (gstate);

    cairo_matrix_init_scale (&gstate->font_matrix, size, size);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_set_font_matrix (cairo_gstate_t	    *gstate, 
			       const cairo_matrix_t *matrix)
{
    _cairo_gstate_unset_font (gstate);

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
	gstate->font_face = _cairo_simple_font_face_create (CAIRO_FONT_FAMILY_DEFAULT,
							    CAIRO_FONT_SLANT_DEFAULT,
							    CAIRO_FONT_WEIGHT_DEFAULT);
	if (!gstate->font_face)
	    return CAIRO_STATUS_NO_MEMORY;
    }
    
    return CAIRO_STATUS_SUCCESS;
}
    
static cairo_status_t
_cairo_gstate_ensure_font (cairo_gstate_t *gstate)
{
    cairo_status_t status;
    
    if (gstate->scaled_font)
	return CAIRO_STATUS_SUCCESS;
    
    status = _cairo_gstate_ensure_font_face (gstate);
    if (status)
	return status;

    gstate->scaled_font = cairo_scaled_font_create (gstate->font_face,
						    &gstate->font_matrix,
						    &gstate->ctm);
    
    if (!gstate->scaled_font)
	return CAIRO_STATUS_NO_MEMORY;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_get_font_extents (cairo_gstate_t *gstate, 
				cairo_font_extents_t *extents)
{
    cairo_status_t status = _cairo_gstate_ensure_font (gstate);
    if (status)
	return status;

    return cairo_scaled_font_extents (gstate->scaled_font, extents);
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

    status = _cairo_gstate_ensure_font (gstate);
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
    if (font_face != gstate->font_face) {
	if (gstate->font_face)
	    cairo_font_face_destroy (gstate->font_face);
	gstate->font_face = font_face;
	if (gstate->font_face)
	    cairo_font_face_reference (gstate->font_face);
    }

    _cairo_gstate_unset_font (gstate);
    
    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gstate_glyph_extents (cairo_gstate_t *gstate,
			     cairo_glyph_t *glyphs, 
			     int num_glyphs,
			     cairo_text_extents_t *extents)
{
    cairo_status_t status;

    status = _cairo_gstate_ensure_font (gstate);
    if (status)
	return status;

    cairo_scaled_font_glyph_extents (gstate->scaled_font,
				     glyphs, num_glyphs,
				     extents);

    return CAIRO_STATUS_SUCCESS;
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

    if (gstate->surface->level != gstate->surface_level)
	return CAIRO_STATUS_BAD_NESTING;
    
    status = _cairo_gstate_ensure_font (gstate);
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
    
    status = _cairo_scaled_font_glyph_bbox (gstate->scaled_font,
					    transformed_glyphs, num_glyphs, 
					    &bbox);
    _cairo_box_round_to_rectangle (&bbox, &extents);

    if (status)
	goto CLEANUP_GLYPHS;

    if (gstate->clip.surface)
    {
	cairo_surface_t *intermediate;
	cairo_surface_pattern_t intermediate_pattern;
	
	_cairo_rectangle_intersect (&extents, &gstate->clip.rect);

	/* Shortcut if empty */
	if (_cairo_rectangle_empty (&extents)) {
	    status = CAIRO_STATUS_SUCCESS;
	    goto BAIL1;
	}
	
	intermediate = _cairo_surface_create_similar_solid (gstate->clip.surface,
							    CAIRO_FORMAT_A8,
							    extents.width,
							    extents.height,
							    CAIRO_COLOR_TRANSPARENT);
	if (intermediate == NULL) {
	    status = CAIRO_STATUS_NO_MEMORY;
	    goto BAIL1;
	}

	/* move the glyphs again, from dev space to intermediate space */
	for (i = 0; i < num_glyphs; ++i)
	{
	    transformed_glyphs[i].x -= extents.x;
	    transformed_glyphs[i].y -= extents.y;
	}

	_cairo_pattern_init_solid (&pattern.solid, CAIRO_COLOR_WHITE);
    
	status = _cairo_scaled_font_show_glyphs (gstate->scaled_font, 
						 CAIRO_OPERATOR_ADD, 
						 &pattern.base, intermediate,
						 extents.x, extents.y,
						 0, 0,
						 extents.width, extents.height,
						 transformed_glyphs, num_glyphs);
	
	_cairo_pattern_fini (&pattern.base);

	if (status)
	    goto BAIL2;

	_cairo_pattern_init_for_surface (&pattern.surface,
					 gstate->clip.surface);
    
	status = _cairo_surface_composite (CAIRO_OPERATOR_IN,
					   &pattern.base,
					   NULL,
					   intermediate,
					   extents.x - gstate->clip.rect.x,
					   extents.y - gstate->clip.rect.y, 
					   0, 0,
					   0, 0,
					   extents.width, extents.height);

	_cairo_pattern_fini (&pattern.base);

	if (status)
	    goto BAIL2;

	_cairo_pattern_init_for_surface (&intermediate_pattern, intermediate);
	_cairo_pattern_init_copy (&pattern.base, gstate->source);
	_cairo_gstate_pattern_transform (gstate, &pattern.base);
    
	status = _cairo_surface_composite (gstate->operator,
					   &pattern.base,
					   &intermediate_pattern.base,
					   gstate->surface,
					   extents.x, extents.y, 
					   0, 0,
					   extents.x, extents.y,
					   extents.width, extents.height);
	_cairo_pattern_fini (&pattern.base);
	_cairo_pattern_fini (&intermediate_pattern.base);

    BAIL2:
	cairo_surface_destroy (intermediate);
    BAIL1:
	;
    }
    else
    {
	_cairo_pattern_init_copy (&pattern.base, gstate->source);
	_cairo_gstate_pattern_transform (gstate, &pattern.base);

	status = _cairo_scaled_font_show_glyphs (gstate->scaled_font, 
						 gstate->operator, &pattern.base,
						 gstate->surface,
						 extents.x, extents.y,
						 extents.x, extents.y,
						 extents.width, extents.height,
						 transformed_glyphs, num_glyphs);

	_cairo_pattern_fini (&pattern.base);
    }
    
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

    status = _cairo_gstate_ensure_font (gstate);
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
