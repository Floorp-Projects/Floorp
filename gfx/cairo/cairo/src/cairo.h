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
 *	Carl D. Worth <cworth@isi.edu>
 */

#ifndef CAIRO_H
#define CAIRO_H

#include <cairo-features.h>
#include <pixman.h>

typedef struct _cairo cairo_t;
typedef struct _cairo_surface cairo_surface_t;
typedef struct _cairo_matrix cairo_matrix_t;
typedef struct _cairo_pattern cairo_pattern_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Functions for manipulating state objects */
cairo_t *
cairo_create (void);

void
cairo_reference (cairo_t *cr);

void
cairo_destroy (cairo_t *cr);

void
cairo_save (cairo_t *cr);

void
cairo_restore (cairo_t *cr);

/* XXX: Replace with cairo_current_gstate/cairo_set_gstate */
void
cairo_copy (cairo_t *dest, cairo_t *src);

/* XXX: I want to rethink this API
void
cairo_push_group (cairo_t *cr);

void
cairo_pop_group (cairo_t *cr);
*/

/* Modify state */
void
cairo_set_target_surface (cairo_t *cr, cairo_surface_t *surface);

typedef enum cairo_format {
    CAIRO_FORMAT_ARGB32,
    CAIRO_FORMAT_RGB24,
    CAIRO_FORMAT_A8,
    CAIRO_FORMAT_A1
} cairo_format_t;

void
cairo_set_target_image (cairo_t	*cr,
			char		*data,
			cairo_format_t	format,
			int		width,
			int		height,
			int		stride);

typedef enum cairo_operator { 
    CAIRO_OPERATOR_CLEAR,
    CAIRO_OPERATOR_SRC,
    CAIRO_OPERATOR_DST,
    CAIRO_OPERATOR_OVER,
    CAIRO_OPERATOR_OVER_REVERSE,
    CAIRO_OPERATOR_IN,
    CAIRO_OPERATOR_IN_REVERSE,
    CAIRO_OPERATOR_OUT,
    CAIRO_OPERATOR_OUT_REVERSE,
    CAIRO_OPERATOR_ATOP,
    CAIRO_OPERATOR_ATOP_REVERSE,
    CAIRO_OPERATOR_XOR,
    CAIRO_OPERATOR_ADD,
    CAIRO_OPERATOR_SATURATE
} cairo_operator_t;

void
cairo_set_operator (cairo_t *cr, cairo_operator_t op);

/* XXX: Probably want to bite the bullet and expose a cairo_color_t object */

/* XXX: I've been trying to come up with a sane way to specify:

   cairo_set_color (cairo_t *cr, cairo_color_t *color);

   Keith wants to be able to support super-luminescent colors,
   (premultiplied colors with R/G/B greater than alpha). The current
   API does not allow that. Adding a premulitplied RGBA cairo_color_t
   would do the trick.

   One problem though is that alpha is currently orthogonal to
   color. For example, show_surface uses gstate->alpha but ignores the
   color. So, it doesn't seem be right to have cairo_set_color modify
   the behavior of cairo_show_surface.
*/

void
cairo_set_rgb_color (cairo_t *cr, double red, double green, double blue);

void
cairo_set_pattern (cairo_t *cr, cairo_pattern_t *pattern);

void
cairo_set_alpha (cairo_t *cr, double alpha);

/* XXX: Currently, the tolerance value is specified by the user in
   terms of device-space units. If I'm not mistaken, this is the only
   value in this API that is not expressed in user-space units. I
   should think whether this value should be user-space instead. */
void
cairo_set_tolerance (cairo_t *cr, double tolerance);

typedef enum cairo_fill_rule {
    CAIRO_FILL_RULE_WINDING,
    CAIRO_FILL_RULE_EVEN_ODD
} cairo_fill_rule_t;

void
cairo_set_fill_rule (cairo_t *cr, cairo_fill_rule_t fill_rule);

void
cairo_set_line_width (cairo_t *cr, double width);

typedef enum cairo_line_cap {
    CAIRO_LINE_CAP_BUTT,
    CAIRO_LINE_CAP_ROUND,
    CAIRO_LINE_CAP_SQUARE
} cairo_line_cap_t;

void
cairo_set_line_cap (cairo_t *cr, cairo_line_cap_t line_cap);

typedef enum cairo_line_join {
    CAIRO_LINE_JOIN_MITER,
    CAIRO_LINE_JOIN_ROUND,
    CAIRO_LINE_JOIN_BEVEL
} cairo_line_join_t;

void
cairo_set_line_join (cairo_t *cr, cairo_line_join_t line_join);

void
cairo_set_dash (cairo_t *cr, double *dashes, int ndash, double offset);

void
cairo_set_miter_limit (cairo_t *cr, double limit);

void
cairo_translate (cairo_t *cr, double tx, double ty);

void
cairo_scale (cairo_t *cr, double sx, double sy);

void
cairo_rotate (cairo_t *cr, double angle);

void
cairo_concat_matrix (cairo_t *cr,
		     cairo_matrix_t *matrix);

void
cairo_set_matrix (cairo_t *cr,
		  cairo_matrix_t *matrix);

void
cairo_default_matrix (cairo_t *cr);

/* XXX: There's been a proposal to add cairo_default_matrix_exact */

void
cairo_identity_matrix (cairo_t *cr);

void
cairo_transform_point (cairo_t *cr, double *x, double *y);

void
cairo_transform_distance (cairo_t *cr, double *dx, double *dy);

void
cairo_inverse_transform_point (cairo_t *cr, double *x, double *y);

void
cairo_inverse_transform_distance (cairo_t *cr, double *dx, double *dy);

/* Path creation functions */
void
cairo_new_path (cairo_t *cr);

void
cairo_move_to (cairo_t *cr, double x, double y);

void
cairo_line_to (cairo_t *cr, double x, double y);

void
cairo_curve_to (cairo_t *cr,
		double x1, double y1,
		double x2, double y2,
		double x3, double y3);

void
cairo_arc (cairo_t *cr,
	   double xc, double yc,
	   double radius,
	   double angle1, double angle2);

void
cairo_arc_negative (cairo_t *cr,
		    double xc, double yc,
		    double radius,
		    double angle1, double angle2);

/* XXX: NYI
void
cairo_arc_to (cairo_t *cr,
	      double x1, double y1,
	      double x2, double y2,
	      double radius);
*/

void
cairo_rel_move_to (cairo_t *cr, double dx, double dy);

void
cairo_rel_line_to (cairo_t *cr, double dx, double dy);

void
cairo_rel_curve_to (cairo_t *cr,
		    double dx1, double dy1,
		    double dx2, double dy2,
		    double dx3, double dy3);

void
cairo_rectangle (cairo_t *cr,
		 double x, double y,
		 double width, double height);

/* XXX: This is the same name that PostScript uses, but to me the name
   suggests an actual drawing operation ala cairo_stroke --- especially
   since I want to add a cairo_path_t and with that it would be
   natural to have "cairo_stroke_path (cairo_t *, cairo_path_t *)"

   Maybe we could use something like "cairo_outline_path (cairo_t *)"? 
*/
/* XXX: NYI
void
cairo_stroke_path (cairo_t *cr);
*/

void
cairo_close_path (cairo_t *cr);

/* Painting functions */
void
cairo_stroke (cairo_t *cr);

void
cairo_fill (cairo_t *cr);

void
cairo_copy_page (cairo_t *cr);

void
cairo_show_page (cairo_t *cr);

/* Insideness testing */
int
cairo_in_stroke (cairo_t *cr, double x, double y);

int
cairo_in_fill (cairo_t *cr, double x, double y);

/* Rectangular extents */
void
cairo_stroke_extents (cairo_t *cr,
		      double *x1, double *y1,
		      double *x2, double *y2);

void
cairo_fill_extents (cairo_t *cr,
		    double *x1, double *y1,
		    double *x2, double *y2);

/* Clipping */
void
cairo_init_clip (cairo_t *cr);

/* Note: cairo_clip does not consume the current path */
void
cairo_clip (cairo_t *cr);

/* Font/Text functions */

typedef struct _cairo_font cairo_font_t;

typedef struct {
  unsigned long        index;
  double               x;
  double               y;
} cairo_glyph_t;

typedef struct {
    double x_bearing;
    double y_bearing;
    double width;
    double height;
    double x_advance;
    double y_advance;
} cairo_text_extents_t;

typedef struct {
    double ascent;
    double descent;
    double height;
    double max_x_advance;
    double max_y_advance;
} cairo_font_extents_t;

typedef enum cairo_font_slant {
  CAIRO_FONT_SLANT_NORMAL,
  CAIRO_FONT_SLANT_ITALIC,
  CAIRO_FONT_SLANT_OBLIQUE
} cairo_font_slant_t;
  
typedef enum cairo_font_weight {
  CAIRO_FONT_WEIGHT_NORMAL,
  CAIRO_FONT_WEIGHT_BOLD
} cairo_font_weight_t;
  
/* This interface is for dealing with text as text, not caring about the
   font object inside the the cairo_t. */

void
cairo_select_font (cairo_t              *cr, 
		   const char           *family, 
		   cairo_font_slant_t   slant, 
		   cairo_font_weight_t  weight);

void
cairo_scale_font (cairo_t *cr, double scale);

void
cairo_transform_font (cairo_t *cr, cairo_matrix_t *matrix);

void
cairo_show_text (cairo_t *cr, const unsigned char *utf8);

void
cairo_show_glyphs (cairo_t *cr, cairo_glyph_t *glyphs, int num_glyphs);

cairo_font_t *
cairo_current_font (cairo_t *cr);

void
cairo_current_font_extents (cairo_t *cr, 
			    cairo_font_extents_t *extents);

void
cairo_set_font (cairo_t *cr, cairo_font_t *font);

void
cairo_text_extents (cairo_t                *cr,
		    const unsigned char    *utf8,
		    cairo_text_extents_t   *extents);

void
cairo_glyph_extents (cairo_t               *cr,
		     cairo_glyph_t         *glyphs, 
		     int                   num_glyphs,
		     cairo_text_extents_t  *extents);

void
cairo_text_path  (cairo_t *cr, const unsigned char *utf8);

void
cairo_glyph_path (cairo_t *cr, cairo_glyph_t *glyphs, int num_glyphs);

/* Portable interface to general font features. */
  
void
cairo_font_reference (cairo_font_t *font);

void
cairo_font_destroy (cairo_font_t *font);

void
cairo_font_set_transform (cairo_font_t *font, 
			  cairo_matrix_t *matrix);

void
cairo_font_current_transform (cairo_font_t *font, 
			      cairo_matrix_t *matrix);

/* Image functions */

/* XXX: Eliminate width/height here */
void
cairo_show_surface (cairo_t		*cr,
		    cairo_surface_t	*surface,
		    int		width,
		    int		height);

/* Query functions */

/* XXX: It would be nice if I could find a simpler way to make the
   definitions for the deprecated functions. Ideally, I would just
   have to put DEPRECATE (cairo_get_operator, cairo_current_operator)
   into one file and be done with it. For now, I've got a little more
   typing than that. */

cairo_operator_t
cairo_current_operator (cairo_t *cr);

void
cairo_current_rgb_color (cairo_t *cr, double *red, double *green, double *blue);
cairo_pattern_t *
cairo_current_pattern (cairo_t *cr);

double
cairo_current_alpha (cairo_t *cr);

/* XXX: Do we want cairo_current_pattern as well? */

double
cairo_current_tolerance (cairo_t *cr);

void
cairo_current_point (cairo_t *cr, double *x, double *y);

cairo_fill_rule_t
cairo_current_fill_rule (cairo_t *cr);

double
cairo_current_line_width (cairo_t *cr);

cairo_line_cap_t
cairo_current_line_cap (cairo_t *cr);

cairo_line_join_t
cairo_current_line_join (cairo_t *cr);

double
cairo_current_miter_limit (cairo_t *cr);

/* XXX: How to do cairo_current_dash??? Do we want to switch to a cairo_dash object? */

void
cairo_current_matrix (cairo_t *cr, cairo_matrix_t *matrix);

/* XXX: Need to decide the memory mangement semantics of this
   function. Should it reference the surface again? */
cairo_surface_t *
cairo_current_target_surface (cairo_t *cr);

typedef void (cairo_move_to_func_t) (void *closure,
				     double x, double y);

typedef void (cairo_line_to_func_t) (void *closure,
				     double x, double y);

typedef void (cairo_curve_to_func_t) (void *closure,
				      double x1, double y1,
				      double x2, double y2,
				      double x3, double y3);

typedef void (cairo_close_path_func_t) (void *closure);

extern void
cairo_current_path (cairo_t			*cr,
		    cairo_move_to_func_t	*move_to,
		    cairo_line_to_func_t	*line_to,
		    cairo_curve_to_func_t	*curve_to,
		    cairo_close_path_func_t	*close_path,
		    void			*closure);

extern void
cairo_current_path_flat (cairo_t			*cr,
			 cairo_move_to_func_t		*move_to,
			 cairo_line_to_func_t		*line_to,
			 cairo_close_path_func_t	*close_path,
			 void				*closure);

/* Error status queries */

typedef enum cairo_status {
    CAIRO_STATUS_SUCCESS = 0,
    CAIRO_STATUS_NO_MEMORY,
    CAIRO_STATUS_INVALID_RESTORE,
    CAIRO_STATUS_INVALID_POP_GROUP,
    CAIRO_STATUS_NO_CURRENT_POINT,
    CAIRO_STATUS_INVALID_MATRIX,
    CAIRO_STATUS_NO_TARGET_SURFACE,
    CAIRO_STATUS_NULL_POINTER
} cairo_status_t;

cairo_status_t
cairo_status (cairo_t *cr);

const char *
cairo_status_string (cairo_t *cr);

/* Surface manipulation */
/* XXX: We may want to rename this function in light of the new
   virtualized surface backends... */
cairo_surface_t *
cairo_surface_create_for_image (char           *data,
				cairo_format_t  format,
				int             width,
				int             height,
				int             stride);

/* XXX: I want to remove this function, (replace with
   cairo_set_target_scratch or similar). */
cairo_surface_t *
cairo_surface_create_similar (cairo_surface_t	*other,
			      cairo_format_t	format,
			      int		width,
			      int		height);

void
cairo_surface_reference (cairo_surface_t *surface);

void
cairo_surface_destroy (cairo_surface_t *surface);

/* XXX: Note: The current Render/Ic implementations don't do the right
   thing with repeat when the surface has a non-identity matrix. */
/* XXX: Rework this as a cairo function with an enum: cairo_set_pattern_extend */
cairo_status_t
cairo_surface_set_repeat (cairo_surface_t *surface, int repeat);

/* XXX: Rework this as a cairo function: cairo_set_pattern_transform */
cairo_status_t
cairo_surface_set_matrix (cairo_surface_t *surface, cairo_matrix_t *matrix);

/* XXX: Rework this as a cairo function: cairo_current_pattern_transform */
cairo_status_t
cairo_surface_get_matrix (cairo_surface_t *surface, cairo_matrix_t *matrix);

typedef enum {
    CAIRO_FILTER_FAST,
    CAIRO_FILTER_GOOD,
    CAIRO_FILTER_BEST,
    CAIRO_FILTER_NEAREST,
    CAIRO_FILTER_BILINEAR,
    CAIRO_FILTER_GAUSSIAN
} cairo_filter_t;
  
/* XXX: Rework this as a cairo function: cairo_set_pattern_filter */
cairo_status_t
cairo_surface_set_filter (cairo_surface_t *surface, cairo_filter_t filter);

cairo_filter_t 
cairo_surface_get_filter (cairo_surface_t *surface);

/* Image-surface functions */

cairo_surface_t *
cairo_image_surface_create (cairo_format_t	format,
			    int			width,
			    int			height);

cairo_surface_t *
cairo_image_surface_create_for_data (char			*data,
				     cairo_format_t		format,
				     int			width,
				     int			height,
				     int			stride);

/* Pattern creation functions */
cairo_pattern_t *
cairo_pattern_create_for_surface (cairo_surface_t *surface);

cairo_pattern_t *
cairo_pattern_create_linear (double x0, double y0,
			     double x1, double y1);

cairo_pattern_t *
cairo_pattern_create_radial (double cx0, double cy0, double radius0,
			     double cx1, double cy1, double radius1);

void
cairo_pattern_reference (cairo_pattern_t *pattern);

void
cairo_pattern_destroy (cairo_pattern_t *pattern);
  
cairo_status_t
cairo_pattern_add_color_stop (cairo_pattern_t *pattern,
			      double offset,
			      double red, double green, double blue,
			      double alpha);
  
cairo_status_t
cairo_pattern_set_matrix (cairo_pattern_t *pattern, cairo_matrix_t *matrix);

cairo_status_t
cairo_pattern_get_matrix (cairo_pattern_t *pattern, cairo_matrix_t *matrix);

typedef enum {
    CAIRO_EXTEND_NONE,
    CAIRO_EXTEND_REPEAT,
    CAIRO_EXTEND_REFLECT
} cairo_extend_t;

cairo_status_t
cairo_pattern_set_extend (cairo_pattern_t *pattern, cairo_extend_t extend);

cairo_extend_t
cairo_pattern_get_extend (cairo_pattern_t *pattern);

cairo_status_t
cairo_pattern_set_filter (cairo_pattern_t *pattern, cairo_filter_t filter);

cairo_filter_t
cairo_pattern_get_filter (cairo_pattern_t *pattern);

/* Matrix functions */

/* XXX: Rename all of these to cairo_transform_t */

cairo_matrix_t *
cairo_matrix_create (void);

void
cairo_matrix_destroy (cairo_matrix_t *matrix);

cairo_status_t
cairo_matrix_copy (cairo_matrix_t *matrix, const cairo_matrix_t *other);

cairo_status_t
cairo_matrix_set_identity (cairo_matrix_t *matrix);

cairo_status_t
cairo_matrix_set_affine (cairo_matrix_t *cr,
			 double a, double b,
			 double c, double d,
			 double tx, double ty);

cairo_status_t
cairo_matrix_get_affine (cairo_matrix_t *matrix,
			 double *a, double *b,
			 double *c, double *d,
			 double *tx, double *ty);

cairo_status_t
cairo_matrix_translate (cairo_matrix_t *matrix, double tx, double ty);

cairo_status_t
cairo_matrix_scale (cairo_matrix_t *matrix, double sx, double sy);

cairo_status_t
cairo_matrix_rotate (cairo_matrix_t *matrix, double radians);

cairo_status_t
cairo_matrix_invert (cairo_matrix_t *matrix);

cairo_status_t
cairo_matrix_multiply (cairo_matrix_t *result, const cairo_matrix_t *a, const cairo_matrix_t *b);

cairo_status_t
cairo_matrix_transform_distance (cairo_matrix_t *matrix, double *dx, double *dy);

cairo_status_t
cairo_matrix_transform_point (cairo_matrix_t *matrix, double *x, double *y);

/* Deprecated functions. We've made some effort to allow the
   deprecated functions to continue to work for now, (with useful
   warnings). But the deprecated functions will not appear in the next
   release. */
#ifndef _CAIROINT_H_
#define cairo_get_operator	cairo_get_operator_DEPRECATED_BY_cairo_current_operator
#define cairo_get_rgb_color	cairo_get_rgb_color_DEPRECATED_BY_cairo_current_rgb_color
#define cairo_get_alpha	 	cairo_get_alpha_DEPRECATED_BY_cairo_current_alpha
#define cairo_get_tolerance	cairo_get_tolerance_DEPRECATED_BY_cairo_current_tolerance
#define cairo_get_current_point	cairo_get_current_point_DEPRECATED_BY_cairo_current_point
#define cairo_get_fill_rule	cairo_get_fill_rule_DEPRECATED_BY_cairo_current_fill_rule
#define cairo_get_line_width	cairo_get_line_width_DEPRECATED_BY_cairo_current_line_width
#define cairo_get_line_cap	cairo_get_line_cap_DEPRECATED_BY_cairo_current_line_cap
#define cairo_get_line_join	cairo_get_line_join_DEPRECATED_BY_cairo_current_line_join
#define cairo_get_miter_limit	cairo_get_miter_limit_DEPRECATED_BY_cairo_current_miter_limit
#define cairo_get_matrix	cairo_get_matrix_DEPRECATED_BY_cairo_current_matrix
#define cairo_get_target_surface	cairo_get_target_surface_DEPRECATED_BY_cairo_current_target_surface
#define cairo_get_status	 	cairo_get_status_DEPRECATED_BY_cairo_status
#define cairo_get_status_string		cairo_get_status_string_DEPRECATED_BY_cairo_status_string
#endif

#ifdef __cplusplus
}
#endif

#endif /* CAIRO_H */
