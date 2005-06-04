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

#ifndef CAIRO_H
#define CAIRO_H

#ifdef  __cplusplus
# define CAIRO_BEGIN_DECLS  extern "C" {
# define CAIRO_END_DECLS    }
#else
# define CAIRO_BEGIN_DECLS
# define CAIRO_END_DECLS
#endif

#include <cairo-features.h>

CAIRO_BEGIN_DECLS

/**
 * cairo_bool_t:
 *
 * #cairo_bool_t is used for boolean values. Returns of type
 * #cairo_bool_t will always be either 0 or 1, but testing against
 * these values explicitly is not encouraged; just use the
 * value as a boolean condition.
 *
 * <informalexample><programlisting>
 *  if (cairo_in_stroke (cr, x, y)) {
 *      /<!-- -->* do something *<!-- -->/
 *  }
 * </programlisting></informalexample>
 */
typedef int cairo_bool_t;

/**
 * cairo_t:
 *
 * A #cairo_t contains the current state of the rendering device,
 * including coordinates of yet to be drawn shapes.
 **/
typedef struct _cairo cairo_t;

/**
 * cairo_surface_t:
 *
 * A #cairo_surface_t represents an image, either as the destination
 * of a drawing operation or as source when drawing onto another
 * surface. There are different subtypes of cairo_surface_t for
 * different drawing backends; for example, cairo_image_surface_create()
 * creates a bitmap image in memory.
 *
 * Memory management of #cairo_surface_t is done with
 * cairo_surface_reference() and cairo_surface_destroy().
 */
typedef struct _cairo_surface cairo_surface_t;

/**
 * cairo_matrix_t:
 *
 * A #cairo_matrix_t holds an affine transformation, such as a scale,
 * rotation, or shear, or a combination of those.
 **/
typedef struct _cairo_matrix {
    double xx; double yx;
    double xy; double yy;
    double x0; double y0;
} cairo_matrix_t;

typedef struct _cairo_pattern cairo_pattern_t;

/**
 * cairo_destroy_func_t
 *
 * #cairo_destroy_func_t the type of function which is called when a
 * data element is destroyed. It is passed the pointer to the data
 * element and should free any memory and resources allocated for it.
 */
typedef void (*cairo_destroy_func_t) (void *data);

/**
 * cairo_user_data_key_t
 *
 * #cairo_user_data_key_t is used for attaching user data to cairo
 * data structures.  The actual contents of the struct is never used,
 * and there is no need to initialize the object; only the unique
 * address of a #cairo_data_key_t object is used.  Typically, you
 * would just use the address of a static #cairo_data_key_t object.
 */
typedef struct _cairo_user_data_key {
    int unused;
} cairo_user_data_key_t;

/**
 * cairo_status_t
 * @CAIRO_STATUS_SUCCESS: no error has occurred
 * @CAIRO_STATUS_NO_MEMORY: 
 * @CAIRO_STATUS_INVALID_RESTORE:
 * @CAIRO_STATUS_INVALID_POP_GROUP:
 * @CAIRO_STATUS_INVALID_MATRIX:
 * @CAIRO_STATUS_NO_TARGET_SURFACE:
 * @CAIRO_STATUS_NULL_POINTER:
 * @CAIRO_STATUS_INVALID_STRING:
 * @CAIRO_STATUS_INVALID_PATH_DATA:
 * @CAIRO_STATUS_READ_ERROR:
 * @CAIRO_STATUS_WRITE_ERROR:
 * @CAIRO_STATUS_SURFACE_FINISHED:
 * @CAIRO_STATUS_SURFACE_TYPE_MISMATCH:
 * @CAIRO_STATUS_BAD_NESTING: the same surface was used as the
 *  target surface for two different cairo contexts at once,
 *  and more drawing was done on the first context before the
 *  surface was unset as the target for the second context.
 *  See the documentation for cairo_create().
 *
 * #cairo_status_t is used to indicate errors that can occur when
 * using Cairo. In some cases it is returned directly by functions.
 * but when using #cairo_t, the last error, if any, is stored in
 * the context and can be retrieved with cairo_status().
 **/
typedef enum cairo_status {
    CAIRO_STATUS_SUCCESS = 0,
    CAIRO_STATUS_NO_MEMORY,
    CAIRO_STATUS_INVALID_RESTORE,
    CAIRO_STATUS_INVALID_POP_GROUP,
    CAIRO_STATUS_NO_CURRENT_POINT,
    CAIRO_STATUS_INVALID_MATRIX,
    CAIRO_STATUS_NO_TARGET_SURFACE,
    CAIRO_STATUS_NULL_POINTER,
    CAIRO_STATUS_INVALID_STRING,
    CAIRO_STATUS_INVALID_PATH_DATA,
    CAIRO_STATUS_READ_ERROR,
    CAIRO_STATUS_WRITE_ERROR,
    CAIRO_STATUS_SURFACE_FINISHED,
    CAIRO_STATUS_SURFACE_TYPE_MISMATCH,
    CAIRO_STATUS_BAD_NESTING
} cairo_status_t;

/**
 * cairo_write_func_t
 *
 * #cairo_write_func_t is the type of function which is called when a
 * backend needs to write data to an output stream.  It is passed the
 * closure which was specified by the user at the time the write
 * function was registered, the data to write and the length of the
 * data in bytes.  The write function should return
 * CAIRO_STATUS_SUCCESS if all the data was successfully written,
 * CAIRO_STATUS_WRITE_ERROR otherwise.
 */
typedef cairo_status_t (*cairo_write_func_t) (void		  *closure,
					      const unsigned char *data,
					      unsigned int	   length);

/**
 * cairo_read_func_t
 *
 * #cairo_read_func_t is the type of function which is called when a
 * backend needs to read data from an intput stream.  It is passed the
 * closure which was specified by the user at the time the read
 * function was registered, the buffer to read the data into and the
 * length of the data in bytes.  The read function should return
 * CAIRO_STATUS_SUCCESS if all the data was successfully written,
 * CAIRO_STATUS_READ_ERROR otherwise.
 */
typedef cairo_status_t (*cairo_read_func_t) (void		*closure,
					     unsigned char	*data,
					     unsigned int	length);

/* Functions for manipulating state objects */
cairo_t *
cairo_create (cairo_surface_t *target);

void
cairo_reference (cairo_t *cr);

void
cairo_destroy (cairo_t *cr);

void
cairo_save (cairo_t *cr);

void
cairo_restore (cairo_t *cr);

/* XXX: I want to rethink this API
void
cairo_push_group (cairo_t *cr);

void
cairo_pop_group (cairo_t *cr);
*/

/* Modify state */

typedef enum cairo_operator {
    CAIRO_OPERATOR_CLEAR,

    CAIRO_OPERATOR_SOURCE,
    CAIRO_OPERATOR_OVER,
    CAIRO_OPERATOR_IN,
    CAIRO_OPERATOR_OUT,
    CAIRO_OPERATOR_ATOP,

    CAIRO_OPERATOR_DEST,
    CAIRO_OPERATOR_DEST_OVER,
    CAIRO_OPERATOR_DEST_IN,
    CAIRO_OPERATOR_DEST_OUT,
    CAIRO_OPERATOR_DEST_ATOP,

    CAIRO_OPERATOR_XOR,
    CAIRO_OPERATOR_ADD,
    CAIRO_OPERATOR_SATURATE
} cairo_operator_t;

void
cairo_set_operator (cairo_t *cr, cairo_operator_t op);

void
cairo_set_source (cairo_t *cr, cairo_pattern_t *source);

void
cairo_set_source_rgb (cairo_t *cr, double red, double green, double blue);

void
cairo_set_source_rgba (cairo_t *cr,
		       double red, double green, double blue,
		       double alpha);

void
cairo_set_source_surface (cairo_t	  *cr,
			  cairo_surface_t *surface,
			  double	   x,
			  double	   y);

void
cairo_set_tolerance (cairo_t *cr, double tolerance);

/**
 * cairo_fill_rule_t
 * @CAIRO_FILL_RULE_WINDING: If the path crosses the ray from
 * left-to-right, counts +1. If the path crosses the ray
 * from right to left, counts -1. (Left and right are determined
 * from the perspective of looking along the ray from the starting
 * point.) If the total count is non-zero, the point will be filled.
 * @CAIRO_FILL_RULE_EVEN_ODD: Counts the total number of
 * intersections, without regard to the orientation of the contour. If
 * the total number of intersections is odd, the point will be
 * filled.
 *
 * #cairo_fill_rule_t is used to select how paths are filled. For both
 * fill rules, whether or not a point is included in the fill is
 * determined by taking a ray from that point to infinity and looking
 * at intersections with the path. The ray can be in any direction,
 * as long as it doesn't pass through the end point of a segment
 * or have a tricky intersection such as intersecting tangent to the path.
 * (Note that filling is not actually implemented in this way. This
 * is just a description of the rule that is applied.)
 **/
typedef enum cairo_fill_rule {
    CAIRO_FILL_RULE_WINDING,
    CAIRO_FILL_RULE_EVEN_ODD
} cairo_fill_rule_t;

void
cairo_set_fill_rule (cairo_t *cr, cairo_fill_rule_t fill_rule);

void
cairo_set_line_width (cairo_t *cr, double width);


/**
 * cairo_line_cap_t
 * @CAIRO_LINE_CAP_BUTT: start(stop) the line exactly at the start(end) point
 * @CAIRO_LINE_CAP_ROUND: use a round ending, the center of the circle is the end point
 * @CAIRO_LINE_CAP_SQUARE: use squared ending, the center of the square is the end point
 *
 * enumeration for style of line-endings
 **/
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
cairo_transform (cairo_t	      *cr,
		 const cairo_matrix_t *matrix);

void
cairo_set_matrix (cairo_t	       *cr,
		  const cairo_matrix_t *matrix);

void
cairo_identity_matrix (cairo_t *cr);

void
cairo_user_to_device (cairo_t *cr, double *x, double *y);

void
cairo_user_to_device_distance (cairo_t *cr, double *dx, double *dy);

void
cairo_device_to_user (cairo_t *cr, double *x, double *y);

void
cairo_device_to_user_distance (cairo_t *cr, double *dx, double *dy);

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

/* XXX: NYI
void
cairo_stroke_to_path (cairo_t *cr);
*/

void
cairo_close_path (cairo_t *cr);

/* Painting functions */
void
cairo_paint (cairo_t *cr);

void
cairo_paint_with_alpha (cairo_t *cr,
			double   alpha);

void
cairo_mask (cairo_t         *cr,
	    cairo_pattern_t *pattern);

void
cairo_mask_surface (cairo_t         *cr,
		    cairo_surface_t *surface,
		    double           surface_x,
		    double           surface_y);

void
cairo_stroke (cairo_t *cr);

void
cairo_stroke_preserve (cairo_t *cr);

void
cairo_fill (cairo_t *cr);

void
cairo_fill_preserve (cairo_t *cr);

void
cairo_copy_page (cairo_t *cr);

void
cairo_show_page (cairo_t *cr);

/* Insideness testing */
cairo_bool_t
cairo_in_stroke (cairo_t *cr, double x, double y);

cairo_bool_t
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
cairo_reset_clip (cairo_t *cr);

void
cairo_clip (cairo_t *cr);

void
cairo_clip_preserve (cairo_t *cr);

/* Font/Text functions */

/**
 * cairo_scaled_font_t:
 *
 * A #cairo_scaled_font_t is a font scaled to a particular size and device
 * resolution. A cairo_scaled_font_t is most useful for low-level font
 * usage where a library or application wants to cache a reference
 * to a scaled font to speed up the computation of metrics.
 */
typedef struct _cairo_scaled_font cairo_scaled_font_t;

/**
 * cairo_font_face_t:
 *
 * A #cairo_font_face_t specifies all aspects of a font other
 * than the size or font matrix (a font matrix is used to distort
 * a font by sheering it or scaling it unequally in the two
 * directions) . A font face can be set on a #cairo_t by using
 * cairo_set_font_face(); the size and font matrix are set with
 * cairo_set_font_size() and cairo_set_font_matrix().
 */
typedef struct _cairo_font_face cairo_font_face_t;

/**
 * cairo_glyph_t:
 * @index: glyph index in the font. The exact interpretation of the
 *      glyph index depends on the font technology being used.
 * @x: the offset in the X direction between the origin used for
 *     drawing or measuring the string and the origin of this glyph.
 * @y: the offset in the Y direction between the origin used for
 *     drawing or measuring the string and the origin of this glyph.
 *
 * The #cairo_glyph_t structure holds information about a single glyph
 * when drawing or measuring text. A font is (in simple terms) a
 * collection of shapes used to draw text. A glyph is one of these
 * shapes. There can be multiple glyphs for a single character
 * (alternates to be used in different contexts, for example), or a
 * glyph can be a <firstterm>ligature</firstterm> of multiple
 * characters. Cairo doesn't expose any way of converting input text
 * into glyphs, so in order to use the Cairo interfaces that take
 * arrays of glyphs, you must directly access the appropriate
 * underlying font system.
 *
 * Note that the offsets given by @x and @y are not cumulative. When
 * drawing or measuring text, each glyph is individually positioned
 * with respect to the overall origin
 **/
typedef struct {
  unsigned long        index;
  double               x;
  double               y;
} cairo_glyph_t;

/**
 * cairo_text_extents_t:
 * @x_bearing: the horizontal distance from the origin to the
 *   leftmost part of the glyphs as drawn. Positive if the
 *   glyphs lie entirely to the right of the origin.
 * @y_bearing: the vertical distance from the origin to the
 *   topmost part of the glyphs as drawn. Positive only if the
 *   glyphs lie completely below the origin; will usually be
 *   negative.
 * @width: width of the glyphs as drawn
 * @height: height of the glyphs as drawn
 * @x_advance:distance to advance in the X direction
 *    after drawing these glyphs
 * @y_advance: distance to advance in the Y direction
 *   after drawing these glyphs. Will typically be zero except
 *   for vertical text layout as found in East-Asian languages.
 *
 * The #cairo_text_extents_t structure stores the extents of a single
 * glyph or a string of glyphs in user-space coordinates. Because text
 * extents are in user-space coordinates, they are mostly, but not
 * entirely, independent of the current transformation matrix. If you call
 * <literal>cairo_scale(cr, 2.0, 2.0)</literal>, text will
 * be drawn twice as big, but the reported text extents will not be
 * doubled. They will change slightly due to hinting (so you can't
 * assume that metrics are independent of the transformation matrix),
 * but otherwise will remain unchanged.
 */
typedef struct {
    double x_bearing;
    double y_bearing;
    double width;
    double height;
    double x_advance;
    double y_advance;
} cairo_text_extents_t;

/**
 * cairo_font_extents_t:
 * @ascent: the distance that the font extends above the baseline.
 *          Note that this is not always exactly equal to the maximum
 *          of the extents of all the glyphs in the font, but rather
 *          is picked to express the font designer's intent as to
 *          how the font should align with elements above it.
 * @descent: the distance that the font extends below the baseline.
 *           This value is positive for typical fonts that include
 *           portions below the baseline. Note that this is not always
 *           exactly equal to the maximum of the extents of all the
 *           glyphs in the font, but rather is picked to express the
 *           font designer's intent as to how the the font should
 *           align with elements below it.
 * @height: the recommended vertical distance between baselines when
 *          setting consecutive lines of text with the font. This
 *          is greater than @ascent+@descent by a
 *          quantity known as the <firstterm>line spacing</firstterm>
 *          or <firstterm>external leading</firstterm>. When space
 *          is at a premium, most fonts can be set with only
 *          a distance of @ascent+@descent between lines.
 * @max_x_advance: the maximum distance in the X direction that 
 *         the the origin is advanced for any glyph in the font.
 * @max_y_advance: the maximum distance in the Y direction that
 *         the the origin is advanced for any glyph in the font.
 *         this will be zero for normal fonts used for horizontal
 *         writing. (The scripts of East Asia are sometimes written
 *         vertically.)
 *
 * The #cairo_text_extents_t structure stores metric information for
 * a font. Values are given in the current user-space coordinate
 * system.
 *
 * Because font metrics are in user-space coordinates, they are
 * mostly, but not entirely, independent of the current transformation
 * matrix. If you call <literal>cairo_scale(cr, 2.0, 2.0)</literal>,
 * text will be drawn twice as big, but the reported text extents will
 * not be doubled. They will change slightly due to hinting (so you
 * can't assume that metrics are independent of the transformation
 * matrix), but otherwise will remain unchanged.
 */
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
cairo_select_font_face (cairo_t              *cr, 
			const char           *family, 
			cairo_font_slant_t   slant, 
			cairo_font_weight_t  weight);

void
cairo_set_font_size (cairo_t *cr, double size);

void
cairo_set_font_matrix (cairo_t		    *cr,
		       const cairo_matrix_t *matrix);

void
cairo_get_font_matrix (cairo_t *cr,
		       cairo_matrix_t *matrix);

void
cairo_show_text (cairo_t *cr, const char *utf8);

void
cairo_show_glyphs (cairo_t *cr, cairo_glyph_t *glyphs, int num_glyphs);

cairo_font_face_t *
cairo_get_font_face (cairo_t *cr);

void
cairo_font_extents (cairo_t              *cr, 
		    cairo_font_extents_t *extents);

void
cairo_set_font_face (cairo_t *cr, cairo_font_face_t *font_face);

void
cairo_text_extents (cairo_t              *cr,
		    const char    	 *utf8,
		    cairo_text_extents_t *extents);

void
cairo_glyph_extents (cairo_t               *cr,
		     cairo_glyph_t         *glyphs, 
		     int                   num_glyphs,
		     cairo_text_extents_t  *extents);

void
cairo_text_path  (cairo_t *cr, const char *utf8);

void
cairo_glyph_path (cairo_t *cr, cairo_glyph_t *glyphs, int num_glyphs);

/* Generic identifier for a font style */

void
cairo_font_face_reference (cairo_font_face_t *font_face);

void
cairo_font_face_destroy (cairo_font_face_t *font_face);

void *
cairo_font_face_get_user_data (cairo_font_face_t	   *font_face,
			       const cairo_user_data_key_t *key);

cairo_status_t
cairo_font_face_set_user_data (cairo_font_face_t	   *font_face,
			       const cairo_user_data_key_t *key,
			       void			   *user_data,
			       cairo_destroy_func_t	    destroy);

/* Portable interface to general font features. */

cairo_scaled_font_t *
cairo_scaled_font_create (cairo_font_face_t    *font_face,
			  const cairo_matrix_t *font_matrix,
			  const cairo_matrix_t *ctm);

void
cairo_scaled_font_reference (cairo_scaled_font_t *scaled_font);

void
cairo_scaled_font_destroy (cairo_scaled_font_t *scaled_font);

cairo_status_t
cairo_scaled_font_extents (cairo_scaled_font_t  *scaled_font,
			   cairo_font_extents_t *extents);

void
cairo_scaled_font_glyph_extents (cairo_scaled_font_t   *scaled_font,
				 cairo_glyph_t         *glyphs, 
				 int                   num_glyphs,
				 cairo_text_extents_t  *extents);

/* Query functions */

cairo_operator_t
cairo_get_operator (cairo_t *cr);

cairo_pattern_t *
cairo_get_source (cairo_t *cr);

double
cairo_get_tolerance (cairo_t *cr);

void
cairo_get_current_point (cairo_t *cr, double *x, double *y);

cairo_fill_rule_t
cairo_get_fill_rule (cairo_t *cr);

double
cairo_get_line_width (cairo_t *cr);

cairo_line_cap_t
cairo_get_line_cap (cairo_t *cr);

cairo_line_join_t
cairo_get_line_join (cairo_t *cr);

double
cairo_get_miter_limit (cairo_t *cr);

/* XXX: How to do cairo_get_dash??? Do we want to switch to a cairo_dash object? */

void
cairo_get_matrix (cairo_t *cr, cairo_matrix_t *matrix);

/* XXX: Need to decide the memory management semantics of this
   function. Should it reference the surface again? */
cairo_surface_t *
cairo_get_target (cairo_t *cr);

/**
 * cairo_path_data_t:
 *
 * A data structure for holding path data---appears within
 * #cairo_path_t.
 *
 * The data structure is designed to try to balance the demands of
 * efficiency and ease-of-use. A path is represented as an array of
 * cairo_path_data_t which is a union of headers and points.
 *
 * Each portion of the path is represented by one or more elements in
 * the array, (one header followed by 0 or more points). The length
 * value of the header is the number of array elements for the current
 * portion including the header, (ie. length == 1 + # of points), and
 * where the number of points for each element type must be as
 * follows:
 *
 *	CAIRO_PATH_MOVE_TO:	1 point
 *	CAIRO_PATH_LINE_TO:	1 point
 *	CAIRO_PATH_CURVE_TO:	3 points
 *	CAIRO_PATH_CLOSE_PATH:	0 points
 *
 * The semantics and ordering of the coordinate values are consistent
 * with cairo_move_to(), cairo_line_to(), cairo_curve_to(), and
 * cairo_close_path().
 *
 * Here is sample code for iterating through a #cairo_path_t:
 *
 * <informalexample><programlisting>
 *      int i;
 *	cairo_path_t *path;
 *      cairo_path_data_t *data;
 *
 *	path = cairo_copy_path (cr);
 *
 *      for (i=0; i < path->num_data; i += path->data[i].header.length) {
 *          data = &path->data[i];
 *	    switch (data->header.type) {
 *	    case CAIRO_PATH_MOVE_TO:
 *		do_move_to_things (data[1].point.x, data[1].point.y);
 *		break;
 *	    case CAIRO_PATH_LINE_TO:
 *		do_line_to_things (data[1].point.x, data[1].point.y);
 *		break;
 *	    case CAIRO_PATH_CURVE_TO:
 *		do_curve_to_things (data[1].point.x, data[1].point.y,
 *				    data[2].point.x, data[2].point.y,
 *				    data[3].point.x, data[3].point.y);
 *		break;
 *	    case CAIRO_PATH_CLOSE_PATH:
 *		do_close_path_things ();
 *		break;
 *	    }
 *	}
 *
 *	cairo_path_destroy (path);
 * </programlisting></informalexample>
 */
typedef union {
    struct {
	enum {
	    CAIRO_PATH_MOVE_TO,
	    CAIRO_PATH_LINE_TO,
	    CAIRO_PATH_CURVE_TO,
	    CAIRO_PATH_CLOSE_PATH
	} type;
	int length;
    } header;
    struct {
	double x, y;
    } point;
} cairo_path_data_t;

/**
 * cairo_path_t:
 *
 * A data structure for holding a path. This data structure serves as
 * the return value for cairo_copy_path_data() and
 * cairo_copy_path_data_flat() as well the input value for
 * cairo_append_path().
 *
 * See #cairo_path_data_t for hints on how to iterate over the
 * actual data within the path.
 *
 * The num_data member gives the number of elements in the data
 * array. This number is larger than the number of independent path
 * portions (MOVE_TO, LINE_TO, CURVE_TO, CLOSE_PATH), since the data
 * includes both headers and coordinates for each portion.
 **/
typedef struct cairo_path {
    cairo_path_data_t *data;
    int num_data;
} cairo_path_t;

cairo_path_t *
cairo_copy_path (cairo_t *cr);

cairo_path_t *
cairo_copy_path_flat (cairo_t *cr);

void
cairo_append_path (cairo_t	*cr,
		   cairo_path_t *path);

void
cairo_path_destroy (cairo_path_t *path);

/* Error status queries */

cairo_status_t
cairo_status (cairo_t *cr);

const char *
cairo_status_string (cairo_t *cr);

/* Surface manipulation */

/**
 * cairo_format_t
 * @CAIRO_FORMAT_ARGB32: each pixel is a 32-bit quantity, with
 *   alpha in the upper 8 bits, then red, then green, then blue.
 *   The 32-bit quantities are stored native-endian. Pre-multiplied
 *   alpha is used. (That is, 50% transparent red is 0x80800000,
 *   not 0x80ff0000.)
 * @CAIRO_FORMAT_RGB24: each pixel is a 32-bit quantity, with
 *   the upper 8 bits unused. Red, Green, and Blue are stored
 *   in the remaining 24 bits in that order.
 * @CAIRO_FORMAT_A8: each pixel is a 8-bit quantity holding
 *   an alpha value.
 * @CAIRO_FORMAT_A1: each pixel is a 1-bit quantity holding
 *   an alpha value. Pixels are packed together into 32-bit
 *   quantities. The ordering of the bits matches the
 *   endianess of the platform. On a big-endian machine, the
 *   first pixel is in the uppermost bit, on a little-endian
 *   machine the first pixel is in the least-significant bit.
 *
 * #cairo_format_t is used to identify the memory format of
 * image data.
 */
typedef enum cairo_format {
    CAIRO_FORMAT_ARGB32,
    CAIRO_FORMAT_RGB24,
    CAIRO_FORMAT_A8,
    CAIRO_FORMAT_A1
} cairo_format_t;

/* XXX: I want to remove this function, (replace with
   cairo_begin_group and friends). */
cairo_surface_t *
cairo_surface_create_similar (cairo_surface_t	*other,
			      cairo_format_t	format,
			      int		width,
			      int		height);

void
cairo_surface_reference (cairo_surface_t *surface);

void
cairo_surface_destroy (cairo_surface_t *surface);

cairo_status_t
cairo_surface_finish (cairo_surface_t *surface);

#ifdef CAIRO_HAS_PNG_FUNCTIONS

cairo_status_t
cairo_surface_write_to_png (cairo_surface_t	*surface,
			    const char		*filename);

cairo_status_t
cairo_surface_write_to_png_stream (cairo_surface_t	*surface,
				   cairo_write_func_t	write_func,
				   void			*closure);

#endif

void *
cairo_surface_get_user_data (cairo_surface_t		 *surface,
			     const cairo_user_data_key_t *key);

cairo_status_t
cairo_surface_set_user_data (cairo_surface_t		 *surface,
			     const cairo_user_data_key_t *key,
			     void			 *user_data,
			     cairo_destroy_func_t	 destroy);

void
cairo_surface_set_device_offset (cairo_surface_t *surface,
				 double           x_offset,
				 double           y_offset);

/* Image-surface functions */

cairo_surface_t *
cairo_image_surface_create (cairo_format_t	format,
			    int			width,
			    int			height);

cairo_surface_t *
cairo_image_surface_create_for_data (unsigned char	       *data,
				     cairo_format_t		format,
				     int			width,
				     int			height,
				     int			stride);

int
cairo_image_surface_get_width (cairo_surface_t *surface);

int
cairo_image_surface_get_height (cairo_surface_t *surface);

#ifdef CAIRO_HAS_PNG_FUNCTIONS

cairo_surface_t *
cairo_image_surface_create_from_png (const char	*filename);

cairo_surface_t *
cairo_image_surface_create_from_png_stream (cairo_read_func_t	read_func,
					    void		*closure);

#endif

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
cairo_pattern_add_color_stop_rgb (cairo_pattern_t *pattern,
				  double offset,
				  double red, double green, double blue);

cairo_status_t
cairo_pattern_add_color_stop_rgba (cairo_pattern_t *pattern,
				   double offset,
				   double red, double green, double blue,
				   double alpha);

cairo_status_t
cairo_pattern_set_matrix (cairo_pattern_t      *pattern,
			  const cairo_matrix_t *matrix);

cairo_status_t
cairo_pattern_get_matrix (cairo_pattern_t *pattern,
			  cairo_matrix_t  *matrix);

typedef enum {
    CAIRO_EXTEND_NONE,
    CAIRO_EXTEND_REPEAT,
    CAIRO_EXTEND_REFLECT
} cairo_extend_t;

cairo_status_t
cairo_pattern_set_extend (cairo_pattern_t *pattern, cairo_extend_t extend);

cairo_extend_t
cairo_pattern_get_extend (cairo_pattern_t *pattern);

typedef enum {
    CAIRO_FILTER_FAST,
    CAIRO_FILTER_GOOD,
    CAIRO_FILTER_BEST,
    CAIRO_FILTER_NEAREST,
    CAIRO_FILTER_BILINEAR,
    CAIRO_FILTER_GAUSSIAN
} cairo_filter_t;
  
cairo_status_t
cairo_pattern_set_filter (cairo_pattern_t *pattern, cairo_filter_t filter);

cairo_filter_t
cairo_pattern_get_filter (cairo_pattern_t *pattern);

/* Matrix functions */

void
cairo_matrix_init (cairo_matrix_t *matrix,
		   double  xx, double  yx,
		   double  xy, double  yy,
		   double  x0, double  y0);

void
cairo_matrix_init_identity (cairo_matrix_t *matrix);

void
cairo_matrix_init_translate (cairo_matrix_t *matrix,
			     double tx, double ty);

void
cairo_matrix_init_scale (cairo_matrix_t *matrix,
			 double sx, double sy);

void
cairo_matrix_init_rotate (cairo_matrix_t *matrix,
			  double radians);

void
cairo_matrix_translate (cairo_matrix_t *matrix, double tx, double ty);

void
cairo_matrix_scale (cairo_matrix_t *matrix, double sx, double sy);

void
cairo_matrix_rotate (cairo_matrix_t *matrix, double radians);

cairo_status_t
cairo_matrix_invert (cairo_matrix_t *matrix);

void
cairo_matrix_multiply (cairo_matrix_t	    *result,
		       const cairo_matrix_t *a,
		       const cairo_matrix_t *b);

/* XXX: Need a new name here perhaps. */
void
cairo_matrix_transform_distance (const cairo_matrix_t *matrix,
				 double *dx, double *dy);

/* XXX: Need a new name here perhaps. */
void
cairo_matrix_transform_point (const cairo_matrix_t *matrix,
			      double *x, double *y);

#ifndef _CAIROINT_H_

/* Obsolete functions. These definitions exist to coerce the compiler
 * into providing a little bit of guidance with its error
 * messages. The idea is to help users port their old code without
 * having to dig through lots of documentation.
 *
 * The first set of REPLACED_BY functions is for functions whose names
 * have just been changed. So fixing these up is mechanical, (and
 * automated by means of the cairo/util/cairo-api-update script.
 *
 * The second set of DEPRECATED_BY functions is for functions where
 * the replacement is used in a different way, (ie. different
 * arguments, multiple functions instead of one, etc). Fixing these up
 * will require a bit more work on the user's part, (and hopefully we
 * can get cairo-api-update to find these and print some guiding
 * information).
 */
#define cairo_current_font_extents   cairo_current_font_extents_REPLACED_BY_cairo_font_extents
#define cairo_get_font_extents       cairo_get_font_extents_REPLACED_BY_cairo_font_extents
#define cairo_current_operator       cairo_current_operator_REPLACED_BY_cairo_get_operator
#define cairo_current_tolerance	     cairo_current_tolerance_REPLACED_BY_cairo_get_tolerance
#define cairo_current_point	     cairo_current_point_REPLACED_BY_cairo_get_current_point
#define cairo_current_fill_rule	     cairo_current_fill_rule_REPLACED_BY_cairo_get_fill_rule
#define cairo_current_line_width     cairo_current_line_width_REPLACED_BY_cairo_get_line_width
#define cairo_current_line_cap       cairo_current_line_cap_REPLACED_BY_cairo_get_line_cap
#define cairo_current_line_join      cairo_current_line_join_REPLACED_BY_cairo_get_line_join
#define cairo_current_miter_limit    cairo_current_miter_limit_REPLACED_BY_cairo_get_miter_limit
#define cairo_current_matrix         cairo_current_matrix_REPLACED_BY_cairo_get_matrix
#define cairo_current_target_surface cairo_current_target_surface_REPLACED_BY_cairo_get_target
#define cairo_get_status             cairo_get_status_REPLACED_BY_cairo_status
#define cairo_get_status_string	     cairo_get_status_string_REPLACED_BY_cairo_status_string
#define cairo_concat_matrix		 cairo_concat_matrix_REPLACED_BY_cairo_transform
#define cairo_scale_font                 cairo_scale_font_REPLACED_BY_cairo_set_font_size
#define cairo_select_font                cairo_select_font_REPLACED_BY_cairo_select_font_face
#define cairo_transform_font             cairo_transform_font_REPLACED_BY_cairo_set_font_matrix
#define cairo_transform_point		 cairo_transform_point_REPLACED_BY_cairo_user_to_device
#define cairo_transform_distance	 cairo_transform_distance_REPLACED_BY_cairo_user_to_device_distance
#define cairo_inverse_transform_point	 cairo_inverse_transform_point_REPLACED_BY_cairo_device_to_user
#define cairo_inverse_transform_distance cairo_inverse_transform_distance_REPLACED_BY_cairo_device_to_user_distance
#define cairo_init_clip			 cairo_init_clip_REPLACED_BY_cairo_reset_clip
#define cairo_surface_create_for_image	 cairo_surface_create_for_image_REPLACED_BY_cairo_image_surface_create_for_data
#define cairo_default_matrix		 cairo_default_matrix_REPLACED_BY_cairo_identity_matrix
#define cairo_matrix_set_affine		 cairo_matrix_set_affine_REPLACED_BY_cairo_matrix_init
#define cairo_matrix_set_identity	 cairo_matrix_set_identity_REPLACED_BY_cairo_matrix_init_identity
#define cairo_pattern_add_color_stop	 cairo_pattern_add_color_stop_REPLACED_BY_cairo_pattern_add_color_stop_rgba
#define cairo_set_rgb_color		 cairo_set_rgb_color_REPLACED_BY_cairo_set_source_rgb
#define cairo_set_pattern		 cairo_set_pattern_REPLACED_BY_cairo_set_source
#define cairo_xlib_surface_create_for_pixmap_with_visual	cairo_xlib_surface_create_for_pixmap_with_visual_REPLACED_BY_cairo_xlib_surface_create
#define cairo_xlib_surface_create_for_window_with_visual	cairo_xlib_surface_create_for_window_with_visual_REPLACED_BY_cairo_xlib_surface_create
#define cairo_xcb_surface_create_for_pixmap_with_visual	cairo_xcb_surface_create_for_pixmap_with_visual_REPLACED_BY_cairo_xcb_surface_create
#define cairo_xcb_surface_create_for_window_with_visual	cairo_xcb_surface_create_for_window_with_visual_REPLACED_BY_cairo_xcb_surface_create


#define cairo_current_path	     cairo_current_path_DEPRECATED_BY_cairo_copy_path
#define cairo_current_path_flat	     cairo_current_path_flat_DEPRECATED_BY_cairo_copy_path_flat
#define cairo_get_path		     cairo_get_path_DEPRECATED_BY_cairo_copy_path
#define cairo_get_path_flat	     cairo_get_path_flat_DEPRECATED_BY_cairo_get_path_flat
#define cairo_set_alpha		     cairo_set_alpha_DEPRECATED_BY_cairo_set_source_rgba_OR_cairo_paint_with_alpha
#define cairo_show_surface	     cairo_show_surface_DEPRECATED_BY_cairo_set_source_surface_AND_cairo_paint
#define cairo_copy		     cairo_copy_DEPRECATED_BY_cairo_create_AND_MANY_INDIVIDUAL_FUNCTIONS
#define cairo_surface_set_repeat	cairo_surface_set_repeat_DEPRECATED_BY_cairo_pattern_set_extend
#define cairo_surface_set_matrix	cairo_surface_set_matrix_DEPRECATED_BY_cairo_pattern_set_matrix
#define cairo_surface_get_matrix	cairo_surface_get_matrix_DEPRECATED_BY_cairo_pattern_get_matrix
#define cairo_surface_set_filter	cairo_surface_set_filter_DEPRECATED_BY_cairo_pattern_set_filter
#define cairo_surface_get_filter	cairo_surface_get_filter_DEPRECATED_BY_cairo_pattern_get_filter
#define cairo_matrix_create		cairo_matrix_create_DEPRECATED_BY_cairo_matrix_t
#define cairo_matrix_destroy		cairo_matrix_destroy_DEPRECATED_BY_cairo_matrix_t
#define cairo_matrix_copy		cairo_matrix_copy_DEPRECATED_BY_cairo_matrix_t
#define cairo_matrix_get_affine		cairo_matrix_get_affine_DEPRECATED_BY_cairo_matrix_t
#define cairo_set_target_surface	cairo_set_target_surface_DEPRECATED_BY_cairo_create
#define cairo_set_target_glitz		cairo_set_target_glitz_DEPRECATED_BY_cairo_glitz_surface_create
#define cairo_set_target_image		cairo_set_target_image_DEPRECATED_BY_cairo_image_surface_create_for_data
#define cairo_set_target_pdf		cairo_set_target_pdf_DEPRECATED_BY_cairo_pdf_surface_create
#define cairo_set_target_png		cairo_set_target_png_DEPRECATED_BY_cairo_surface_write_to_png
#define cairo_set_target_ps		cairo_set_target_ps_DEPRECATED_BY_cairo_ps_surface_create
#define cairo_set_target_quartz		cairo_set_target_quartz_DEPRECATED_BY_cairo_quartz_surface_create
#define cairo_set_target_win32		cairo_set_target_win32_DEPRECATED_BY_cairo_win32_surface_create
#define cairo_set_target_xcb		cairo_set_target_xcb_DEPRECATED_BY_cairo_xcb_surface_create
#define cairo_set_target_drawable	cairo_set_target_drawable_DEPRECATED_BY_cairo_xlib_surface_create

#endif

CAIRO_END_DECLS

#endif /* CAIRO_H */
