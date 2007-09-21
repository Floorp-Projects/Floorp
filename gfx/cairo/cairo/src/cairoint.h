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

/*
 * These definitions are solely for use by the implementation of cairo
 * and constitute no kind of standard.  If you need any of these
 * functions, please drop me a note.  Either the library needs new
 * functionality, or there's a way to do what you need using the
 * existing published interfaces. cworth@cworth.org
 */

#ifndef _CAIROINT_H_
#define _CAIROINT_H_

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>

#ifdef _MSC_VER
#define _USE_MATH_DEFINES
#endif
#include <math.h>
#include <limits.h>
#include <stdio.h>

#include "cairo.h"
#include <pixman.h>

#ifdef _MSC_VER
#define snprintf _snprintf
#undef inline
#define inline __inline
#endif

CAIRO_BEGIN_DECLS

#if __GNUC__ >= 3 && defined(__ELF__) && !defined(__sun)
# define slim_hidden_proto(name)		slim_hidden_proto1(name, slim_hidden_int_name(name)) cairo_private
# define slim_hidden_proto_no_warn(name)	slim_hidden_proto1(name, slim_hidden_int_name(name)) cairo_private_no_warn
# define slim_hidden_def(name)			slim_hidden_def1(name, slim_hidden_int_name(name))
# define slim_hidden_int_name(name) INT_##name
# define slim_hidden_proto1(name, internal)				\
  extern __typeof (name) name						\
	__asm__ (slim_hidden_asmname (internal))
# define slim_hidden_def1(name, internal)				\
  extern __typeof (name) EXT_##name __asm__(slim_hidden_asmname(name))	\
	__attribute__((__alias__(slim_hidden_asmname(internal))))
# define slim_hidden_ulp		slim_hidden_ulp1(__USER_LABEL_PREFIX__)
# define slim_hidden_ulp1(x)		slim_hidden_ulp2(x)
# define slim_hidden_ulp2(x)		#x
# define slim_hidden_asmname(name)	slim_hidden_asmname1(name)
# define slim_hidden_asmname1(name)	slim_hidden_ulp #name
#else
# define slim_hidden_proto(name)		int _cairo_dummy_prototype(void)
# define slim_hidden_proto_no_warn(name)	int _cairo_dummy_prototype(void)
# define slim_hidden_def(name)			int _cairo_dummy_prototype(void)
#endif

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
#define CAIRO_PRINTF_FORMAT(fmt_index, va_index) \
	__attribute__((__format__(__printf__, fmt_index, va_index)))
#else
#define CAIRO_PRINTF_FORMAT(fmt_index, va_index)
#endif

/* slim_internal.h */
#if (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3)) && defined(__ELF__) && !defined(__sun)
#define cairo_private_no_warn	__attribute__((__visibility__("hidden")))
#elif defined(__SUNPRO_C) && (__SUNPRO_C >= 0x550)
#define cairo_private_no_warn	__hidden
#else /* not gcc >= 3.3 and not Sun Studio >= 8 */
#define cairo_private_no_warn
#endif

#ifndef WARN_UNUSED_RESULT
#define WARN_UNUSED_RESULT
#endif
/* Add attribute(warn_unused_result) if supported */
#define cairo_warn	    WARN_UNUSED_RESULT
#define cairo_private	    cairo_private_no_warn cairo_warn

/* This macro allow us to deprecate a function by providing an alias
   for the old function name to the new function name. With this
   macro, binary compatibility is preserved. The macro only works on
   some platforms --- tough.

   Meanwhile, new definitions in the public header file break the
   source code so that it will no longer link against the old
   symbols. Instead it will give a descriptive error message
   indicating that the old function has been deprecated by the new
   function.
*/
#if __GNUC__ >= 2 && defined(__ELF__)
# define CAIRO_FUNCTION_ALIAS(old, new)		\
	extern __typeof (new) old		\
	__asm__ ("" #old)			\
	__attribute__((__alias__("" #new)))
#else
# define CAIRO_FUNCTION_ALIAS(old, new)
#endif

#ifndef __GNUC__
#define __attribute__(x)
#endif

#undef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#undef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef NDEBUG
#undef assert
#define assert(expr) \
    do { if (!(expr)) fprintf(stderr, "Assertion failed at %s:%d: %s\n", \
          __FILE__, __LINE__, #expr); } while (0)
#endif

#undef  ARRAY_LENGTH
#define ARRAY_LENGTH(__array) ((int) (sizeof (__array) / sizeof (__array[0])))

/* Size in bytes of buffer to use off the stack per functions.
 * Mostly used by text functions.  For larger allocations, they'll
 * malloc(). */
#ifndef CAIRO_STACK_BUFFER_SIZE
#define CAIRO_STACK_BUFFER_SIZE (512 * sizeof (int))
#endif

#define ASSERT_NOT_REACHED		\
do {					\
    static const int NOT_REACHED = 0;	\
    assert (NOT_REACHED);		\
} while (0)

#define CAIRO_REF_COUNT_INVALID ((unsigned int) -1)

#define CAIRO_ALPHA_IS_OPAQUE(alpha) ((alpha) >= ((double)0xff00 / (double)0xffff))
#define CAIRO_ALPHA_SHORT_IS_OPAQUE(alpha) ((alpha) >= 0xff00)
#define CAIRO_ALPHA_IS_ZERO(alpha) ((alpha) <= 0.0)

#define CAIRO_COLOR_IS_OPAQUE(color) CAIRO_ALPHA_SHORT_IS_OPAQUE ((color)->alpha_short)

/* Reverse the bits in a byte with 7 operations (no 64-bit):
 * Devised by Sean Anderson, July 13, 2001.
 * Source: http://graphics.stanford.edu/~seander/bithacks.html#ReverseByteWith32Bits
 */
#define CAIRO_BITSWAP8(c) ((((c) * 0x0802LU & 0x22110LU) | ((c) * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16)

#ifdef WORDS_BIGENDIAN
#define CAIRO_BITSWAP8_IF_LITTLE_ENDIAN(c) (c)
#else
#define CAIRO_BITSWAP8_IF_LITTLE_ENDIAN(c) CAIRO_BITSWAP8(c)
#endif

#ifdef WORDS_BIGENDIAN

#define cpu_to_be16(v) (v)
#define be16_to_cpu(v) (v)
#define cpu_to_be32(v) (v)
#define be32_to_cpu(v) (v)

#else

static inline uint16_t
cpu_to_be16(uint16_t v)
{
    return (v << 8) | (v >> 8);
}

static inline uint16_t
be16_to_cpu(uint16_t v)
{
    return cpu_to_be16 (v);
}

static inline uint32_t
cpu_to_be32(uint32_t v)
{
    return (cpu_to_be16 (v) << 16) | cpu_to_be16 (v >> 16);
}

static inline uint32_t
be32_to_cpu(uint32_t v)
{
    return cpu_to_be32 (v);
}

#endif

#include "cairo-types-private.h"
#include "cairo-cache-private.h"
#include "cairo-fixed-private.h"

typedef struct _cairo_region cairo_region_t;

typedef struct _cairo_point {
    cairo_fixed_t x;
    cairo_fixed_t y;
} cairo_point_t;

typedef struct _cairo_slope
{
    cairo_fixed_t dx;
    cairo_fixed_t dy;
} cairo_slope_t, cairo_distance_t;

typedef struct _cairo_point_double {
    double x;
    double y;
} cairo_point_double_t;

typedef struct _cairo_distance_double {
    double dx;
    double dy;
} cairo_distance_double_t;

typedef struct _cairo_line {
    cairo_point_t p1;
    cairo_point_t p2;
} cairo_line_t, cairo_box_t;

typedef struct _cairo_trapezoid {
    cairo_fixed_t top, bottom;
    cairo_line_t left, right;
} cairo_trapezoid_t;

typedef struct _cairo_rectangle_int16 {
    int16_t x, y;
    uint16_t width, height;
} cairo_rectangle_int16_t, cairo_glyph_size_t;

typedef struct _cairo_rectangle_int32 {
    int32_t x, y;
    uint32_t width, height;
} cairo_rectangle_int32_t;

typedef struct _cairo_point_int16 {
    int16_t x, y;
} cairo_point_int16_t;

typedef struct _cairo_point_int32 {
    int16_t x, y;
} cairo_point_int32_t;

typedef struct _cairo_box_int16 {
    cairo_point_int16_t p1;
    cairo_point_int16_t p2;
} cairo_box_int16_t;

typedef struct _cairo_box_int32 {
    cairo_point_int32_t p1;
    cairo_point_int32_t p2;
} cairo_box_int32_t;

#if CAIRO_FIXED_BITS == 32 && CAIRO_FIXED_FRAC_BITS >= 16
typedef cairo_rectangle_int16_t cairo_rectangle_int_t;
typedef cairo_point_int16_t cairo_point_int_t;
typedef cairo_box_int16_t cairo_box_int_t;
#define CAIRO_RECT_INT_MIN INT16_MIN
#define CAIRO_RECT_INT_MAX INT16_MAX
#elif CAIRO_FIXED_BITS == 32
typedef cairo_rectangle_int32_t cairo_rectangle_int_t;
typedef cairo_point_int32_t cairo_point_int_t;
typedef cairo_box_int32_t cairo_box_int_t;
#define CAIRO_RECT_INT_MIN INT32_MIN
#define CAIRO_RECT_INT_MAX INT32_MAX
#else
#error Not sure how to pick a cairo_rectangle_int_t for your CAIRO_FIXED_BITS!
#endif

typedef enum _cairo_direction {
    CAIRO_DIRECTION_FORWARD,
    CAIRO_DIRECTION_REVERSE
} cairo_direction_t;

typedef struct _cairo_path_fixed cairo_path_fixed_t;
typedef enum _cairo_clip_mode {
    CAIRO_CLIP_MODE_PATH,
    CAIRO_CLIP_MODE_REGION,
    CAIRO_CLIP_MODE_MASK
} cairo_clip_mode_t;
typedef struct _cairo_clip_path cairo_clip_path_t;

typedef struct _cairo_edge {
    cairo_line_t edge;
    int clockWise;

    cairo_fixed_t current_x;
} cairo_edge_t;

typedef struct _cairo_polygon {
    cairo_status_t status;

    cairo_point_t first_point;
    cairo_point_t current_point;
    cairo_bool_t has_current_point;

    int num_edges;
    int edges_size;
    cairo_edge_t *edges;
    cairo_edge_t  edges_embedded[8];
} cairo_polygon_t;

typedef struct _cairo_spline {
    cairo_point_t a, b, c, d;

    cairo_slope_t initial_slope;
    cairo_slope_t final_slope;

    int num_points;
    int points_size;
    cairo_point_t *points;
    cairo_point_t  points_embedded[8];
} cairo_spline_t;

typedef struct _cairo_pen_vertex {
    cairo_point_t point;

    cairo_slope_t slope_ccw;
    cairo_slope_t slope_cw;
} cairo_pen_vertex_t;

typedef struct _cairo_pen {
    double radius;
    double tolerance;

    cairo_pen_vertex_t *vertices;
    int num_vertices;
} cairo_pen_t;

typedef struct _cairo_color cairo_color_t;
typedef struct _cairo_image_surface cairo_image_surface_t;

cairo_private void
_cairo_box_round_to_rectangle (cairo_box_t *box, cairo_rectangle_int_t *rectangle);

cairo_private void
_cairo_rectangle_intersect (cairo_rectangle_int_t *dest, cairo_rectangle_int_t *src);

/* cairo_array.c structures and functions */

cairo_private void
_cairo_array_init (cairo_array_t *array, int element_size);

cairo_private void
_cairo_array_init_snapshot (cairo_array_t	*array,
			    const cairo_array_t *other);

cairo_private void
_cairo_array_fini (cairo_array_t *array);

cairo_private cairo_status_t
_cairo_array_grow_by (cairo_array_t *array, int additional);

cairo_private void
_cairo_array_truncate (cairo_array_t *array, unsigned int num_elements);

cairo_private cairo_status_t
_cairo_array_append (cairo_array_t *array, const void *element);

cairo_private cairo_status_t
_cairo_array_append_multiple (cairo_array_t	*array,
			      const void	*elements,
			      int		 num_elements);

cairo_private cairo_status_t
_cairo_array_allocate (cairo_array_t	 *array,
		       unsigned int	  num_elements,
		       void		**elements);

cairo_private void *
_cairo_array_index (cairo_array_t *array, unsigned int index);

cairo_private void
_cairo_array_copy_element (cairo_array_t *array, int index, void *dst);

cairo_private int
_cairo_array_num_elements (cairo_array_t *array);

cairo_private int
_cairo_array_size (cairo_array_t *array);

cairo_private void
_cairo_user_data_array_init (cairo_user_data_array_t *array);

cairo_private void
_cairo_user_data_array_fini (cairo_user_data_array_t *array);

cairo_private void *
_cairo_user_data_array_get_data (cairo_user_data_array_t     *array,
				 const cairo_user_data_key_t *key);

cairo_private cairo_status_t
_cairo_user_data_array_set_data (cairo_user_data_array_t     *array,
				 const cairo_user_data_key_t *key,
				 void			     *user_data,
				 cairo_destroy_func_t	      destroy);

cairo_private unsigned long
_cairo_hash_string (const char *c);

typedef struct _cairo_unscaled_font_backend cairo_unscaled_font_backend_t;

/*
 * A cairo_unscaled_font_t is just an opaque handle we use in the
 * glyph cache.
 */
typedef struct _cairo_unscaled_font {
    cairo_hash_entry_t hash_entry;
    unsigned int ref_count;
    const cairo_unscaled_font_backend_t *backend;
} cairo_unscaled_font_t;

typedef struct _cairo_scaled_glyph {
    cairo_cache_entry_t	    cache_entry;	/* hash is glyph index */
    cairo_scaled_font_t	    *scaled_font;	/* font the glyph lives in */
    cairo_text_extents_t    metrics;		/* user-space metrics */
    cairo_box_t		    bbox;		/* device-space bounds */
    int16_t                 x_advance;		/* device-space rounded X advance */
    int16_t                 y_advance;		/* device-space rounded Y advance */
    cairo_image_surface_t   *surface;		/* device-space image */
    cairo_path_fixed_t	    *path;		/* device-space outline */
    void		    *surface_private;	/* for the surface backend */
} cairo_scaled_glyph_t;

#define _cairo_scaled_glyph_index(g) ((g)->cache_entry.hash)
#define _cairo_scaled_glyph_set_index(g,i)  ((g)->cache_entry.hash = (i))

#include "cairo-scaled-font-private.h"

struct _cairo_font_face {
    /* hash_entry must be first */
    cairo_hash_entry_t hash_entry;
    cairo_status_t status;
    unsigned int ref_count;
    cairo_user_data_array_t user_data;
    const cairo_font_face_backend_t *backend;
};

cairo_private void
_cairo_font_reset_static_data (void);

cairo_private void
_cairo_ft_font_reset_static_data (void);

/* the font backend interface */

struct _cairo_unscaled_font_backend {
    void (*destroy)     	    (void		             *unscaled_font);
};

/* cairo_toy_font_face_t - simple family/slant/weight font faces used for
 * the built-in font API
 */

typedef struct _cairo_toy_font_face {
    cairo_font_face_t base;
    const char *family;
    cairo_bool_t owns_family;
    cairo_font_slant_t slant;
    cairo_font_weight_t weight;
} cairo_toy_font_face_t;

typedef enum _cairo_scaled_glyph_info {
    CAIRO_SCALED_GLYPH_INFO_METRICS	= (1 << 0),
    CAIRO_SCALED_GLYPH_INFO_SURFACE	= (1 << 1),
    CAIRO_SCALED_GLYPH_INFO_PATH	= (1 << 2)
} cairo_scaled_glyph_info_t;

typedef struct _cairo_scaled_font_subset {
    cairo_scaled_font_t *scaled_font;
    unsigned int font_id;
    unsigned int subset_id;

    /* Index of glyphs array is subset_glyph_index.
     * Value of glyphs array is scaled_font_glyph_index.
     */
    unsigned long *glyphs;
    unsigned long *to_unicode;
    unsigned int num_glyphs;
    cairo_bool_t is_composite;
} cairo_scaled_font_subset_t;

struct _cairo_scaled_font_backend {
    cairo_font_type_t type;

    cairo_warn cairo_status_t
    (*create_toy)  (cairo_toy_font_face_t	*toy_face,
		    const cairo_matrix_t	*font_matrix,
		    const cairo_matrix_t	*ctm,
		    const cairo_font_options_t	*options,
		    cairo_scaled_font_t	       **scaled_font);

    void
    (*fini)		(void			*scaled_font);

    cairo_warn cairo_int_status_t
    (*scaled_glyph_init)	(void			     *scaled_font,
				 cairo_scaled_glyph_t	     *scaled_glyph,
				 cairo_scaled_glyph_info_t    info);

    /* A backend only needs to implement this or ucs4_to_index(), not
     * both. This allows the backend to do something more sophisticated
     * then just converting characters one by one.
     */
    cairo_warn cairo_int_status_t
    (*text_to_glyphs) (void                *scaled_font,
		       double		    x,
		       double		    y,
		       const char          *utf8,
		       cairo_glyph_t      **glyphs,
		       int 		   *num_glyphs);

    unsigned long
    (*ucs4_to_index)		(void			     *scaled_font,
				 uint32_t		      ucs4);
    cairo_warn cairo_int_status_t
    (*show_glyphs)	(void			*scaled_font,
			 cairo_operator_t	 op,
			 cairo_pattern_t	*pattern,
			 cairo_surface_t	*surface,
			 int			 source_x,
			 int			 source_y,
			 int			 dest_x,
			 int			 dest_y,
			 unsigned int		 width,
			 unsigned int		 height,
			 cairo_glyph_t		*glyphs,
			 int			 num_glyphs);

    cairo_warn cairo_int_status_t
    (*load_truetype_table)(void		        *scaled_font,
                           unsigned long         tag,
                           long                  offset,
                           unsigned char        *buffer,
                           unsigned long        *length);

    void
    (*map_glyphs_to_unicode)(void                       *scaled_font,
                                   cairo_scaled_font_subset_t *font_subset);

};

struct _cairo_font_face_backend {
    cairo_font_type_t	type;

    /* The destroy() function is allowed to resurrect the font face
     * by re-referencing. This is needed for the FreeType backend.
     */
    void
    (*destroy)     (void			*font_face);

    cairo_warn cairo_status_t
    (*scaled_font_create) (void				*font_face,
			   const cairo_matrix_t		*font_matrix,
			   const cairo_matrix_t		*ctm,
			   const cairo_font_options_t	*options,
			   cairo_scaled_font_t	       **scaled_font);
};

/* concrete font backends */
#if CAIRO_HAS_FT_FONT

extern const cairo_private struct _cairo_scaled_font_backend cairo_ft_scaled_font_backend;

#endif

#if CAIRO_HAS_WIN32_FONT

extern const cairo_private struct _cairo_scaled_font_backend cairo_win32_scaled_font_backend;

#endif

#if CAIRO_HAS_ATSUI_FONT

extern const cairo_private struct _cairo_scaled_font_backend cairo_atsui_scaled_font_backend;

#endif

typedef struct _cairo_stroke_style {
    double		 line_width;
    cairo_line_cap_t	 line_cap;
    cairo_line_join_t	 line_join;
    double		 miter_limit;
    double		*dash;
    unsigned int	 num_dashes;
    double		 dash_offset;
} cairo_stroke_style_t;

struct _cairo_surface_backend {
    cairo_surface_type_t type;

    cairo_surface_t *
    (*create_similar)		(void			*surface,
				 cairo_content_t	 content,
				 int			 width,
				 int			 height);

    cairo_warn cairo_status_t
    (*finish)			(void			*surface);

    cairo_warn cairo_status_t
    (*acquire_source_image)	(void                    *abstract_surface,
				 cairo_image_surface_t  **image_out,
				 void                   **image_extra);

    void
    (*release_source_image)	(void                   *abstract_surface,
				 cairo_image_surface_t  *image,
				 void                   *image_extra);

    cairo_warn cairo_status_t
    (*acquire_dest_image)       (void                    *abstract_surface,
				 cairo_rectangle_int_t   *interest_rect,
				 cairo_image_surface_t  **image_out,
				 cairo_rectangle_int_t   *image_rect,
				 void                   **image_extra);

    void
    (*release_dest_image)       (void                    *abstract_surface,
				 cairo_rectangle_int_t   *interest_rect,
				 cairo_image_surface_t   *image,
				 cairo_rectangle_int_t   *image_rect,
				 void                    *image_extra);

    /* Create a new surface (@clone_out) with the following
     * characteristics:
     *
     * 1. It is as compatible as possible with @surface (in terms of
     *    efficiency)
     *
     * 2. It has the same size as @src
     *
     * 3. It has the same contents as @src within the given rectangle.
     */
    cairo_warn cairo_status_t
    (*clone_similar)            (void                   *surface,
				 cairo_surface_t        *src,
				 int                     src_x,
				 int                     src_y,
				 int                     width,
				 int                     height,
				 cairo_surface_t       **clone_out);

    /* XXX: dst should be the first argument for consistency */
    cairo_warn cairo_int_status_t
    (*composite)		(cairo_operator_t	 op,
				 cairo_pattern_t       	*src,
				 cairo_pattern_t	*mask,
				 void			*dst,
				 int			 src_x,
				 int			 src_y,
				 int			 mask_x,
				 int			 mask_y,
				 int			 dst_x,
				 int			 dst_y,
				 unsigned int		 width,
				 unsigned int		 height);

    cairo_warn cairo_int_status_t
    (*fill_rectangles)		(void			 *surface,
				 cairo_operator_t	  op,
				 const cairo_color_t     *color,
				 cairo_rectangle_int_t   *rects,
				 int			  num_rects);

    /* XXX: dst should be the first argument for consistency */
    cairo_warn cairo_int_status_t
    (*composite_trapezoids)	(cairo_operator_t	 op,
				 cairo_pattern_t	*pattern,
				 void			*dst,
				 cairo_antialias_t	 antialias,
				 int			 src_x,
				 int			 src_y,
				 int			 dst_x,
				 int			 dst_y,
				 unsigned int		 width,
				 unsigned int		 height,
				 cairo_trapezoid_t	*traps,
				 int			 num_traps);

    cairo_warn cairo_int_status_t
    (*copy_page)		(void			*surface);

    cairo_warn cairo_int_status_t
    (*show_page)		(void			*surface);

    /* Set given region as the clip region for the surface, replacing
     * any previously set clip region.  Passing in a NULL region will
     * clear the surface clip region.
     *
     * The surface is expected to store the clip region and clip all
     * following drawing operations against it until the clip region
     * is cleared of replaced by another clip region.
     *
     * Cairo will call this function whenever a clip path can be
     * represented as a device pixel aligned set of rectangles.  When
     * this is not possible, cairo will use mask surfaces for
     * clipping.
     */
    cairo_warn cairo_int_status_t
    (*set_clip_region)		(void			*surface,
				 cairo_region_t		*region);

    /* Intersect the given path against the clip path currently set in
     * the surface, using the given fill_rule and tolerance, and set
     * the result as the new clipping path for the surface.  Passing
     * in a NULL path will clear the surface clipping path.
     *
     * The surface is expected to store the resulting clip path and
     * clip all following drawing operations against it until the clip
     * path cleared or intersected with a new path.
     *
     * If a surface implements this function, set_clip_region() will
     * never be called and should not be implemented.  If this
     * function is not implemented cairo will use set_clip_region()
     * (if available) and mask surfaces for clipping.
     */
    cairo_warn cairo_int_status_t
    (*intersect_clip_path)	(void			*dst,
				 cairo_path_fixed_t	*path,
				 cairo_fill_rule_t	fill_rule,
				 double			tolerance,
				 cairo_antialias_t	antialias);

    /* Get the extents of the current surface. For many surface types
     * this will be as simple as { x=0, y=0, width=surface->width,
     * height=surface->height}.
     *
     * This function need not take account of any clipping from
     * set_clip_region since the generic version of set_clip_region
     * saves those, and the generic get_clip_extents will only call
     * into the specific surface->get_extents if there is no current
     * clip.
     */
    cairo_warn cairo_int_status_t
    (*get_extents)		(void			 *surface,
				 cairo_rectangle_int_t   *rectangle);

    /*
     * This is an optional entry to let the surface manage its own glyph
     * resources. If null, render against this surface, using image
     * surfaces as glyphs.
     */
    cairo_warn cairo_int_status_t
    (*old_show_glyphs)		(cairo_scaled_font_t	        *font,
				 cairo_operator_t		 op,
				 cairo_pattern_t		*pattern,
				 void				*surface,
				 int				 source_x,
				 int				 source_y,
				 int				 dest_x,
				 int				 dest_y,
				 unsigned int			 width,
				 unsigned int			 height,
				 cairo_glyph_t			*glyphs,
				 int				 num_glyphs);

    void
    (*get_font_options)         (void                  *surface,
				 cairo_font_options_t  *options);

    cairo_warn cairo_status_t
    (*flush)                    (void                  *surface);

    cairo_warn cairo_status_t
    (*mark_dirty_rectangle)     (void                  *surface,
				 int                    x,
				 int                    y,
				 int                    width,
				 int                    height);

    void
    (*scaled_font_fini)		(cairo_scaled_font_t   *scaled_font);

    void
    (*scaled_glyph_fini)	(cairo_scaled_glyph_t	*scaled_glyph,
				 cairo_scaled_font_t	*scaled_font);

    /* OK, I'm starting over somewhat by defining the 5 top-level
     * drawing operators for the surface backend here with consistent
     * naming and argument-order conventions. */
    cairo_warn cairo_int_status_t
    (*paint)			(void			*surface,
				 cairo_operator_t	 op,
				 cairo_pattern_t	*source);

    cairo_warn cairo_int_status_t
    (*mask)			(void			*surface,
				 cairo_operator_t	 op,
				 cairo_pattern_t	*source,
				 cairo_pattern_t	*mask);

    cairo_warn cairo_int_status_t
    (*stroke)			(void			*surface,
				 cairo_operator_t	 op,
				 cairo_pattern_t	*source,
				 cairo_path_fixed_t	*path,
				 cairo_stroke_style_t	*style,
				 cairo_matrix_t		*ctm,
				 cairo_matrix_t		*ctm_inverse,
				 double			 tolerance,
				 cairo_antialias_t	 antialias);

    cairo_warn cairo_int_status_t
    (*fill)			(void			*surface,
				 cairo_operator_t	 op,
				 cairo_pattern_t	*source,
				 cairo_path_fixed_t	*path,
				 cairo_fill_rule_t	 fill_rule,
				 double			 tolerance,
				 cairo_antialias_t	 antialias);

    cairo_warn cairo_int_status_t
    (*show_glyphs)		(void			*surface,
				 cairo_operator_t	 op,
				 cairo_pattern_t	*source,
				 cairo_glyph_t		*glyphs,
				 int			 num_glyphs,
				 cairo_scaled_font_t	*scaled_font);

    cairo_surface_t *
    (*snapshot)			(void			*surface);

    cairo_bool_t
    (*is_similar)		(void			*surface_a,
	                         void			*surface_b,
				 cairo_content_t         content);

    cairo_warn cairo_status_t
    (*reset)			(void			*surface);

    cairo_warn cairo_int_status_t
    (*fill_stroke)		(void			*surface,
				 cairo_operator_t	 fill_op,
				 cairo_pattern_t	*fill_source,
				 cairo_fill_rule_t	 fill_rule,
				 double			 fill_tolerance,
				 cairo_antialias_t	 fill_antialias,
				 cairo_path_fixed_t	*path,
				 cairo_operator_t	 stroke_op,
				 cairo_pattern_t	*stroke_source,
				 cairo_stroke_style_t	*stroke_style,
				 cairo_matrix_t		*stroke_ctm,
				 cairo_matrix_t		*stroke_ctm_inverse,
				 double			 stroke_tolerance,
				 cairo_antialias_t	 stroke_antialias);
};

typedef struct _cairo_format_masks {
    int bpp;
    unsigned long alpha_mask;
    unsigned long red_mask;
    unsigned long green_mask;
    unsigned long blue_mask;
} cairo_format_masks_t;

#include "cairo-surface-private.h"

struct _cairo_image_surface {
    cairo_surface_t base;

    pixman_format_code_t pixman_format;
    cairo_format_t format;
    unsigned char *data;
    cairo_bool_t owns_data;
    cairo_bool_t has_clip;

    int width;
    int height;
    int stride;
    int depth;

    pixman_image_t *pixman_image;
};

extern const cairo_private cairo_surface_backend_t cairo_image_surface_backend;

/* XXX: Right now, the cairo_color structure puts unpremultiplied
   color in the doubles and premultiplied color in the shorts. Yes,
   this is crazy insane, (but at least we don't export this
   madness). I'm still working on a cleaner API, but in the meantime,
   at least this does prevent precision loss in color when changing
   alpha. */
struct _cairo_color {
    double red;
    double green;
    double blue;
    double alpha;

    unsigned short red_short;
    unsigned short green_short;
    unsigned short blue_short;
    unsigned short alpha_short;
};

typedef enum {
    CAIRO_STOCK_WHITE,
    CAIRO_STOCK_BLACK,
    CAIRO_STOCK_TRANSPARENT
} cairo_stock_t;

#define CAIRO_EXTEND_SURFACE_DEFAULT CAIRO_EXTEND_NONE
#define CAIRO_EXTEND_GRADIENT_DEFAULT CAIRO_EXTEND_PAD
#define CAIRO_FILTER_DEFAULT CAIRO_FILTER_BEST

struct _cairo_pattern {
    cairo_pattern_type_t    type;
    unsigned int	    ref_count;
    cairo_status_t          status;
    cairo_user_data_array_t user_data;

    cairo_matrix_t	    matrix;
    cairo_filter_t	    filter;
    cairo_extend_t	    extend;
};

typedef struct _cairo_solid_pattern {
    cairo_pattern_t base;
    cairo_color_t color;
    cairo_content_t content;
} cairo_solid_pattern_t;

extern const cairo_private cairo_solid_pattern_t _cairo_pattern_nil;
extern const cairo_private cairo_solid_pattern_t cairo_pattern_none;

typedef struct _cairo_surface_pattern {
    cairo_pattern_t base;

    cairo_surface_t *surface;
} cairo_surface_pattern_t;

typedef struct _cairo_gradient_stop {
    cairo_fixed_t x;
    cairo_color_t color;
} cairo_gradient_stop_t;

typedef struct _cairo_gradient_pattern {
    cairo_pattern_t base;

    unsigned int	    n_stops;
    unsigned int	    stops_size;
    cairo_gradient_stop_t  *stops;
    cairo_gradient_stop_t   stops_embedded[2];
} cairo_gradient_pattern_t;

typedef struct _cairo_linear_pattern {
    cairo_gradient_pattern_t base;

    cairo_point_t p1;
    cairo_point_t p2;
} cairo_linear_pattern_t;

typedef struct _cairo_radial_pattern {
    cairo_gradient_pattern_t base;

    cairo_point_t c1;
    cairo_fixed_t r1;
    cairo_point_t c2;
    cairo_fixed_t r2;
} cairo_radial_pattern_t;

typedef union {
    cairo_gradient_pattern_t base;

    cairo_linear_pattern_t linear;
    cairo_radial_pattern_t radial;
} cairo_gradient_pattern_union_t;

typedef union {
    cairo_pattern_t base;

    cairo_solid_pattern_t	   solid;
    cairo_surface_pattern_t	   surface;
    cairo_gradient_pattern_union_t gradient;
} cairo_pattern_union_t;

typedef struct _cairo_surface_attributes {
    cairo_matrix_t matrix;
    cairo_extend_t extend;
    cairo_filter_t filter;
    int		   x_offset;
    int		   y_offset;
    cairo_bool_t   acquired;
    void	   *extra;
} cairo_surface_attributes_t;

typedef struct _cairo_traps {
    cairo_status_t status;

    cairo_box_t extents;

    int num_traps;
    int traps_size;
    cairo_trapezoid_t *traps;
    cairo_trapezoid_t  traps_embedded[1];

    cairo_bool_t has_limits;
    cairo_box_t limits;
} cairo_traps_t;

#define CAIRO_FONT_SLANT_DEFAULT   CAIRO_FONT_SLANT_NORMAL
#define CAIRO_FONT_WEIGHT_DEFAULT  CAIRO_FONT_WEIGHT_NORMAL

#define CAIRO_WIN32_FONT_FAMILY_DEFAULT "Arial"
#define CAIRO_ATSUI_FONT_FAMILY_DEFAULT  "Monaco"
#define CAIRO_FT_FONT_FAMILY_DEFAULT     ""

#if   CAIRO_HAS_WIN32_FONT

#define CAIRO_FONT_FAMILY_DEFAULT CAIRO_WIN32_FONT_FAMILY_DEFAULT
#define CAIRO_SCALED_FONT_BACKEND_DEFAULT &cairo_win32_scaled_font_backend

#elif CAIRO_HAS_ATSUI_FONT

#define CAIRO_FONT_FAMILY_DEFAULT CAIRO_ATSUI_FONT_FAMILY_DEFAULT
#define CAIRO_SCALED_FONT_BACKEND_DEFAULT &cairo_atsui_scaled_font_backend

#elif CAIRO_HAS_FT_FONT

#define CAIRO_FONT_FAMILY_DEFAULT CAIRO_FT_FONT_FAMILY_DEFAULT
#define CAIRO_SCALED_FONT_BACKEND_DEFAULT &cairo_ft_scaled_font_backend

#endif

#define CAIRO_GSTATE_OPERATOR_DEFAULT	CAIRO_OPERATOR_OVER
#define CAIRO_GSTATE_TOLERANCE_DEFAULT	0.1
#define CAIRO_GSTATE_FILL_RULE_DEFAULT	CAIRO_FILL_RULE_WINDING
#define CAIRO_GSTATE_LINE_WIDTH_DEFAULT	2.0
#define CAIRO_GSTATE_LINE_CAP_DEFAULT	CAIRO_LINE_CAP_BUTT
#define CAIRO_GSTATE_LINE_JOIN_DEFAULT	CAIRO_LINE_JOIN_MITER
#define CAIRO_GSTATE_MITER_LIMIT_DEFAULT	10.0
#define CAIRO_GSTATE_DEFAULT_FONT_SIZE  10.0

#define CAIRO_SURFACE_RESOLUTION_DEFAULT 72.0
#define CAIRO_SURFACE_FALLBACK_RESOLUTION_DEFAULT 300.0

typedef struct _cairo_gstate cairo_gstate_t;

typedef struct _cairo_stroke_face {
    cairo_point_t ccw;
    cairo_point_t point;
    cairo_point_t cw;
    cairo_slope_t dev_vector;
    cairo_point_double_t usr_vector;
} cairo_stroke_face_t;

/* cairo.c */
cairo_private void
_cairo_restrict_value (double *value, double min, double max);

cairo_private int
_cairo_lround (double d);

/* cairo_gstate.c */
cairo_private cairo_status_t
_cairo_gstate_init (cairo_gstate_t  *gstate,
		    cairo_surface_t *target);

cairo_private void
_cairo_gstate_fini (cairo_gstate_t *gstate);

cairo_private cairo_status_t
_cairo_gstate_save (cairo_gstate_t **gstate);

cairo_private cairo_status_t
_cairo_gstate_restore (cairo_gstate_t **gstate);

cairo_private cairo_bool_t
_cairo_gstate_is_redirected (cairo_gstate_t *gstate);

cairo_private cairo_status_t
_cairo_gstate_redirect_target (cairo_gstate_t *gstate, cairo_surface_t *child);

cairo_private cairo_surface_t *
_cairo_gstate_get_target (cairo_gstate_t *gstate);

cairo_private cairo_surface_t *
_cairo_gstate_get_parent_target (cairo_gstate_t *gstate);

cairo_private cairo_surface_t *
_cairo_gstate_get_original_target (cairo_gstate_t *gstate);

cairo_private cairo_clip_t *
_cairo_gstate_get_clip (cairo_gstate_t *gstate);

cairo_private cairo_status_t
_cairo_gstate_set_source (cairo_gstate_t *gstate, cairo_pattern_t *source);

cairo_private cairo_pattern_t *
_cairo_gstate_get_source (cairo_gstate_t *gstate);

cairo_private cairo_status_t
_cairo_gstate_set_operator (cairo_gstate_t *gstate, cairo_operator_t op);

cairo_private cairo_operator_t
_cairo_gstate_get_operator (cairo_gstate_t *gstate);

cairo_private cairo_status_t
_cairo_gstate_set_tolerance (cairo_gstate_t *gstate, double tolerance);

cairo_private double
_cairo_gstate_get_tolerance (cairo_gstate_t *gstate);

cairo_private cairo_status_t
_cairo_gstate_set_fill_rule (cairo_gstate_t *gstate, cairo_fill_rule_t fill_rule);

cairo_private cairo_fill_rule_t
_cairo_gstate_get_fill_rule (cairo_gstate_t *gstate);

cairo_private cairo_status_t
_cairo_gstate_set_line_width (cairo_gstate_t *gstate, double width);

cairo_private double
_cairo_gstate_get_line_width (cairo_gstate_t *gstate);

cairo_private cairo_status_t
_cairo_gstate_set_line_cap (cairo_gstate_t *gstate, cairo_line_cap_t line_cap);

cairo_private cairo_line_cap_t
_cairo_gstate_get_line_cap (cairo_gstate_t *gstate);

cairo_private cairo_status_t
_cairo_gstate_set_line_join (cairo_gstate_t *gstate, cairo_line_join_t line_join);

cairo_private cairo_line_join_t
_cairo_gstate_get_line_join (cairo_gstate_t *gstate);

cairo_private cairo_status_t
_cairo_gstate_set_dash (cairo_gstate_t *gstate, const double *dash, int num_dashes, double offset);

cairo_private void
_cairo_gstate_get_dash (cairo_gstate_t *gstate, double *dash, int *num_dashes, double *offset);

cairo_private cairo_status_t
_cairo_gstate_set_miter_limit (cairo_gstate_t *gstate, double limit);

cairo_private double
_cairo_gstate_get_miter_limit (cairo_gstate_t *gstate);

cairo_private void
_cairo_gstate_get_matrix (cairo_gstate_t *gstate, cairo_matrix_t *matrix);

cairo_private cairo_status_t
_cairo_gstate_translate (cairo_gstate_t *gstate, double tx, double ty);

cairo_private cairo_status_t
_cairo_gstate_scale (cairo_gstate_t *gstate, double sx, double sy);

cairo_private cairo_status_t
_cairo_gstate_rotate (cairo_gstate_t *gstate, double angle);

cairo_private cairo_status_t
_cairo_gstate_transform (cairo_gstate_t	      *gstate,
			 const cairo_matrix_t *matrix);

cairo_private cairo_status_t
_cairo_gstate_set_matrix (cairo_gstate_t       *gstate,
			  const cairo_matrix_t *matrix);

cairo_private void
_cairo_gstate_identity_matrix (cairo_gstate_t *gstate);

cairo_private void
_cairo_gstate_user_to_device (cairo_gstate_t *gstate, double *x, double *y);

cairo_private void
_cairo_gstate_user_to_device_distance (cairo_gstate_t *gstate, double *dx, double *dy);

cairo_private void
_cairo_gstate_device_to_user (cairo_gstate_t *gstate, double *x, double *y);

cairo_private void
_cairo_gstate_device_to_user_distance (cairo_gstate_t *gstate, double *dx, double *dy);

cairo_private void
_cairo_gstate_user_to_backend (cairo_gstate_t *gstate, double *x, double *y);

cairo_private void
_cairo_gstate_backend_to_user (cairo_gstate_t *gstate, double *x, double *y);

cairo_private void
_cairo_gstate_backend_to_user_rectangle (cairo_gstate_t *gstate,
                                         double *x1, double *y1,
                                         double *x2, double *y2,
                                         cairo_bool_t *is_tight);

cairo_private cairo_status_t
_cairo_gstate_paint (cairo_gstate_t *gstate);

cairo_private cairo_status_t
_cairo_gstate_mask (cairo_gstate_t  *gstate,
		    cairo_pattern_t *mask);

cairo_private cairo_status_t
_cairo_gstate_stroke (cairo_gstate_t *gstate, cairo_path_fixed_t *path);

cairo_private cairo_status_t
_cairo_gstate_fill (cairo_gstate_t *gstate, cairo_path_fixed_t *path);

cairo_private cairo_status_t
_cairo_gstate_copy_page (cairo_gstate_t *gstate);

cairo_private cairo_status_t
_cairo_gstate_show_page (cairo_gstate_t *gstate);

cairo_private cairo_status_t
_cairo_gstate_stroke_extents (cairo_gstate_t	 *gstate,
			      cairo_path_fixed_t *path,
                              double *x1, double *y1,
			      double *x2, double *y2);

cairo_private cairo_status_t
_cairo_gstate_fill_extents (cairo_gstate_t     *gstate,
			    cairo_path_fixed_t *path,
                            double *x1, double *y1,
			    double *x2, double *y2);

cairo_private cairo_status_t
_cairo_gstate_in_stroke (cairo_gstate_t	    *gstate,
			 cairo_path_fixed_t *path,
			 double		     x,
			 double		     y,
			 cairo_bool_t	    *inside_ret);

cairo_private cairo_status_t
_cairo_gstate_in_fill (cairo_gstate_t	  *gstate,
		       cairo_path_fixed_t *path,
		       double		   x,
		       double		   y,
		       cairo_bool_t	  *inside_ret);

cairo_private cairo_status_t
_cairo_gstate_clip (cairo_gstate_t *gstate, cairo_path_fixed_t *path);

cairo_private cairo_status_t
_cairo_gstate_reset_clip (cairo_gstate_t *gstate);

cairo_private cairo_status_t
_cairo_gstate_clip_extents (cairo_gstate_t *gstate,
		            double         *x1,
		            double         *y1,
        		    double         *x2,
        		    double         *y2);

cairo_private cairo_rectangle_list_t*
_cairo_gstate_copy_clip_rectangle_list (cairo_gstate_t *gstate);

cairo_private cairo_status_t
_cairo_gstate_show_surface (cairo_gstate_t	*gstate,
			    cairo_surface_t	*surface,
			    double		 x,
			    double		 y,
			    double		width,
			    double		height);

cairo_private cairo_status_t
_cairo_gstate_select_font_face (cairo_gstate_t *gstate,
				const char *family,
				cairo_font_slant_t slant,
				cairo_font_weight_t weight);

cairo_private cairo_status_t
_cairo_gstate_set_font_size (cairo_gstate_t *gstate,
			     double          size);

cairo_private void
_cairo_gstate_get_font_matrix (cairo_gstate_t *gstate,
			       cairo_matrix_t *matrix);

cairo_private cairo_status_t
_cairo_gstate_set_font_matrix (cairo_gstate_t	    *gstate,
			       const cairo_matrix_t *matrix);

cairo_private void
_cairo_gstate_get_font_options (cairo_gstate_t       *gstate,
				cairo_font_options_t *options);

cairo_private void
_cairo_gstate_set_font_options (cairo_gstate_t	           *gstate,
				const cairo_font_options_t *options);

cairo_private cairo_status_t
_cairo_gstate_get_font_face (cairo_gstate_t     *gstate,
			     cairo_font_face_t **font_face);

cairo_private cairo_status_t
_cairo_gstate_get_scaled_font (cairo_gstate_t       *gstate,
			       cairo_scaled_font_t **scaled_font);

cairo_private cairo_status_t
_cairo_gstate_get_font_extents (cairo_gstate_t *gstate,
				cairo_font_extents_t *extents);

cairo_private cairo_status_t
_cairo_gstate_set_font_face (cairo_gstate_t    *gstate,
			     cairo_font_face_t *font_face);

cairo_private cairo_status_t
_cairo_gstate_text_to_glyphs (cairo_gstate_t *font,
			      const char     *utf8,
			      double	      x,
			      double	      y,
			      cairo_glyph_t **glyphs,
			      int	     *num_glyphs);

cairo_private cairo_status_t
_cairo_gstate_glyph_extents (cairo_gstate_t *gstate,
			     const cairo_glyph_t *glyphs,
			     int num_glyphs,
			     cairo_text_extents_t *extents);

cairo_private cairo_status_t
_cairo_gstate_show_glyphs (cairo_gstate_t *gstate,
			   const cairo_glyph_t *glyphs,
			   int num_glyphs);

cairo_private cairo_status_t
_cairo_gstate_glyph_path (cairo_gstate_t      *gstate,
			  const cairo_glyph_t *glyphs,
			  int		       num_glyphs,
			  cairo_path_fixed_t  *path);

cairo_private cairo_bool_t
_cairo_operator_bounded_by_mask (cairo_operator_t op);

cairo_private cairo_bool_t
_cairo_operator_bounded_by_source (cairo_operator_t op);

/* cairo_color.c */
cairo_private const cairo_color_t *
_cairo_stock_color (cairo_stock_t stock);

#define CAIRO_COLOR_WHITE       _cairo_stock_color (CAIRO_STOCK_WHITE)
#define CAIRO_COLOR_BLACK       _cairo_stock_color (CAIRO_STOCK_BLACK)
#define CAIRO_COLOR_TRANSPARENT _cairo_stock_color (CAIRO_STOCK_TRANSPARENT)

cairo_private uint16_t
_cairo_color_double_to_short (double d);

cairo_private void
_cairo_color_init (cairo_color_t *color);

cairo_private void
_cairo_color_init_rgb (cairo_color_t *color,
		       double red, double green, double blue);

cairo_private void
_cairo_color_init_rgba (cairo_color_t *color,
			double red, double green, double blue,
			double alpha);

cairo_private void
_cairo_color_multiply_alpha (cairo_color_t *color,
			     double	    alpha);

cairo_private void
_cairo_color_get_rgba (cairo_color_t *color,
		       double	     *red,
		       double	     *green,
		       double	     *blue,
		       double	     *alpha);

cairo_private void
_cairo_color_get_rgba_premultiplied (cairo_color_t *color,
				     double	   *red,
				     double	   *green,
				     double	   *blue,
				     double	   *alpha);

cairo_private cairo_bool_t
_cairo_color_equal (const cairo_color_t *color_a,
                    const cairo_color_t *color_b);

/* cairo-font-face.c */

cairo_private void
_cairo_scaled_font_freeze_cache (cairo_scaled_font_t *scaled_font);

cairo_private void
_cairo_scaled_font_thaw_cache (cairo_scaled_font_t *scaled_font);

cairo_private void
_cairo_scaled_font_reset_cache (cairo_scaled_font_t *scaled_font);

cairo_private void
_cairo_scaled_font_set_error (cairo_scaled_font_t *scaled_font,
			      cairo_status_t status);

extern const cairo_private cairo_font_face_t _cairo_font_face_nil;
extern const cairo_private cairo_scaled_font_t _cairo_scaled_font_nil;

cairo_private void
_cairo_font_face_init (cairo_font_face_t               *font_face,
		       const cairo_font_face_backend_t *backend);

cairo_private cairo_font_face_t *
_cairo_toy_font_face_create (const char           *family,
			     cairo_font_slant_t    slant,
			     cairo_font_weight_t   weight);

cairo_private void
_cairo_unscaled_font_init (cairo_unscaled_font_t               *font,
			   const cairo_unscaled_font_backend_t *backend);

cairo_private_no_warn cairo_unscaled_font_t *
_cairo_unscaled_font_reference (cairo_unscaled_font_t *font);

cairo_private void
_cairo_unscaled_font_destroy (cairo_unscaled_font_t *font);

/* cairo-font-options.c */

cairo_private void
_cairo_font_options_init_default (cairo_font_options_t *options);

cairo_private void
_cairo_font_options_init_copy (cairo_font_options_t		*options,
			       const cairo_font_options_t	*other);

/* cairo_hull.c */
cairo_private cairo_status_t
_cairo_hull_compute (cairo_pen_vertex_t *vertices, int *num_vertices);

/* cairo-lzw.c */
cairo_private unsigned char *
_cairo_lzw_compress (unsigned char *data, unsigned long *size_in_out);

/* cairo_operator.c */
cairo_private cairo_bool_t
_cairo_operator_always_opaque (cairo_operator_t op);

cairo_private cairo_bool_t
_cairo_operator_always_translucent (cairo_operator_t op);

/* cairo_path.c */
cairo_private void
_cairo_path_fixed_init (cairo_path_fixed_t *path);

cairo_private cairo_status_t
_cairo_path_fixed_init_copy (cairo_path_fixed_t *path,
			     cairo_path_fixed_t *other);

cairo_private cairo_bool_t
_cairo_path_fixed_is_equal (cairo_path_fixed_t *path,
			    cairo_path_fixed_t *other);

cairo_private cairo_path_fixed_t *
_cairo_path_fixed_create (void);

cairo_private void
_cairo_path_fixed_fini (cairo_path_fixed_t *path);

cairo_private void
_cairo_path_fixed_destroy (cairo_path_fixed_t *path);

cairo_private cairo_status_t
_cairo_path_fixed_move_to (cairo_path_fixed_t  *path,
			   cairo_fixed_t	x,
			   cairo_fixed_t	y);

cairo_private void
_cairo_path_fixed_new_sub_path (cairo_path_fixed_t *path);

cairo_private cairo_status_t
_cairo_path_fixed_rel_move_to (cairo_path_fixed_t *path,
			       cairo_fixed_t	   dx,
			       cairo_fixed_t	   dy);

cairo_private cairo_status_t
_cairo_path_fixed_line_to (cairo_path_fixed_t *path,
			   cairo_fixed_t	x,
			   cairo_fixed_t	y);

cairo_private cairo_status_t
_cairo_path_fixed_rel_line_to (cairo_path_fixed_t *path,
			       cairo_fixed_t	   dx,
			       cairo_fixed_t	   dy);

cairo_private cairo_status_t
_cairo_path_fixed_curve_to (cairo_path_fixed_t	*path,
			    cairo_fixed_t x0, cairo_fixed_t y0,
			    cairo_fixed_t x1, cairo_fixed_t y1,
			    cairo_fixed_t x2, cairo_fixed_t y2);

cairo_private cairo_status_t
_cairo_path_fixed_rel_curve_to (cairo_path_fixed_t *path,
				cairo_fixed_t dx0, cairo_fixed_t dy0,
				cairo_fixed_t dx1, cairo_fixed_t dy1,
				cairo_fixed_t dx2, cairo_fixed_t dy2);

cairo_private cairo_status_t
_cairo_path_fixed_close_path (cairo_path_fixed_t *path);

cairo_private cairo_status_t
_cairo_path_fixed_get_current_point (cairo_path_fixed_t *path,
				     cairo_fixed_t	*x,
				     cairo_fixed_t	*y);

typedef cairo_status_t
(cairo_path_fixed_move_to_func_t) (void		 *closure,
				   cairo_point_t *point);

typedef cairo_status_t
(cairo_path_fixed_line_to_func_t) (void		 *closure,
				   cairo_point_t *point);

typedef cairo_status_t
(cairo_path_fixed_curve_to_func_t) (void	  *closure,
				    cairo_point_t *p0,
				    cairo_point_t *p1,
				    cairo_point_t *p2);

typedef cairo_status_t
(cairo_path_fixed_close_path_func_t) (void *closure);

cairo_private cairo_status_t
_cairo_path_fixed_interpret (cairo_path_fixed_t		  *path,
		       cairo_direction_t		   dir,
		       cairo_path_fixed_move_to_func_t	  *move_to,
		       cairo_path_fixed_line_to_func_t	  *line_to,
		       cairo_path_fixed_curve_to_func_t	  *curve_to,
		       cairo_path_fixed_close_path_func_t *close_path,
		       void				  *closure);

cairo_private cairo_status_t
_cairo_path_fixed_bounds (cairo_path_fixed_t *path,
			  double *x1, double *y1,
			  double *x2, double *y2);

cairo_private void
_cairo_path_fixed_device_transform (cairo_path_fixed_t	*path,
				    cairo_matrix_t	*device_transform);

/* cairo_path_fill.c */
cairo_private cairo_status_t
_cairo_path_fixed_fill_to_traps (cairo_path_fixed_t *path,
				 cairo_fill_rule_t   fill_rule,
				 double              tolerance,
				 cairo_traps_t      *traps);

/* cairo_path_stroke.c */
cairo_private cairo_status_t
_cairo_path_fixed_stroke_to_traps (cairo_path_fixed_t	*path,
				   cairo_stroke_style_t	*stroke_style,
				   cairo_matrix_t	*ctm,
				   cairo_matrix_t	*ctm_inverse,
				   double		 tolerance,
				   cairo_traps_t	*traps);

/* cairo-scaled-font.c */

cairo_private cairo_status_t
_cairo_scaled_font_init (cairo_scaled_font_t               *scaled_font,
			 cairo_font_face_t		   *font_face,
			 const cairo_matrix_t              *font_matrix,
			 const cairo_matrix_t              *ctm,
			 const cairo_font_options_t	   *options,
			 const cairo_scaled_font_backend_t *backend);

cairo_private void
_cairo_scaled_font_set_metrics (cairo_scaled_font_t	    *scaled_font,
				cairo_font_extents_t	    *fs_metrics);

cairo_private void
_cairo_scaled_font_fini (cairo_scaled_font_t *scaled_font);

cairo_private cairo_status_t
_cairo_scaled_font_font_extents (cairo_scaled_font_t  *scaled_font,
				 cairo_font_extents_t *extents);

cairo_private cairo_status_t
_cairo_scaled_font_text_to_glyphs (cairo_scaled_font_t	*scaled_font,
				   double		x,
				   double		y,
				   const char           *utf8,
				   cairo_glyph_t       **glyphs,
				   int 		        *num_glyphs);

cairo_private cairo_status_t
_cairo_scaled_font_glyph_device_extents (cairo_scaled_font_t	 *scaled_font,
					 const cairo_glyph_t	 *glyphs,
					 int                      num_glyphs,
					 cairo_rectangle_int16_t *extents);

cairo_private cairo_status_t
_cairo_scaled_font_show_glyphs (cairo_scaled_font_t *scaled_font,
				cairo_operator_t     op,
				cairo_pattern_t	    *source,
				cairo_surface_t	    *surface,
				int		     source_x,
				int		     source_y,
				int		     dest_x,
				int		     dest_y,
				unsigned int	     width,
				unsigned int	     height,
				cairo_glyph_t	    *glyphs,
				int		     num_glyphs);

cairo_private cairo_status_t
_cairo_scaled_font_glyph_path (cairo_scaled_font_t *scaled_font,
			       const cairo_glyph_t *glyphs,
			       int                  num_glyphs,
			       cairo_path_fixed_t  *path);

cairo_private void
_cairo_scaled_glyph_set_metrics (cairo_scaled_glyph_t *scaled_glyph,
				 cairo_scaled_font_t *scaled_font,
				 cairo_text_extents_t *fs_metrics);

cairo_private void
_cairo_scaled_glyph_set_surface (cairo_scaled_glyph_t *scaled_glyph,
				 cairo_scaled_font_t *scaled_font,
				 cairo_image_surface_t *surface);

cairo_private void
_cairo_scaled_glyph_set_path (cairo_scaled_glyph_t *scaled_glyph,
			      cairo_scaled_font_t *scaled_font,
			      cairo_path_fixed_t *path);

cairo_private cairo_int_status_t
_cairo_scaled_glyph_lookup (cairo_scaled_font_t *scaled_font,
			    unsigned long index,
			    cairo_scaled_glyph_info_t info,
			    cairo_scaled_glyph_t **scaled_glyph_ret);

cairo_private void
_cairo_scaled_font_map_destroy (void);

/* cairo-stroke-style.c */

cairo_private void
_cairo_stroke_style_init (cairo_stroke_style_t *style);

cairo_private cairo_status_t
_cairo_stroke_style_init_copy (cairo_stroke_style_t *style,
			       cairo_stroke_style_t *other);

cairo_private void
_cairo_stroke_style_fini (cairo_stroke_style_t *style);

/* cairo-surface.c */

extern const cairo_private cairo_surface_t _cairo_surface_nil;
extern const cairo_private cairo_surface_t _cairo_surface_nil_read_error;
extern const cairo_private cairo_surface_t _cairo_surface_nil_write_error;
extern const cairo_private cairo_surface_t _cairo_surface_nil_file_not_found;

cairo_private void
_cairo_surface_set_error (cairo_surface_t	*surface,
			  cairo_status_t	 status);

cairo_private void
_cairo_surface_set_resolution (cairo_surface_t *surface,
                               double x_res,
                               double y_res);

cairo_private cairo_surface_t *
_cairo_surface_create_similar_scratch (cairo_surface_t *other,
				       cairo_content_t	content,
				       int		width,
				       int		height);

/* Note: the color_pattern argument is optional - if provided it will reuse
 * that pattern instead of creating a very short-lived fresh solid pattern
 */
cairo_private cairo_surface_t *
_cairo_surface_create_similar_solid (cairo_surface_t	 *other,
				     cairo_content_t	  content,
				     int		  width,
				     int		  height,
				     const cairo_color_t *color,
				     cairo_pattern_t     *color_pattern);

cairo_private void
_cairo_surface_init (cairo_surface_t			*surface,
		     const cairo_surface_backend_t	*backend,
		     cairo_content_t			 content);

cairo_private void
_cairo_surface_set_font_options (cairo_surface_t       *surface,
				 cairo_font_options_t  *options);

cairo_private cairo_clip_mode_t
_cairo_surface_get_clip_mode (cairo_surface_t *surface);

cairo_private cairo_status_t
_cairo_surface_composite (cairo_operator_t	op,
			  cairo_pattern_t	*src,
			  cairo_pattern_t	*mask,
			  cairo_surface_t	*dst,
			  int			src_x,
			  int			src_y,
			  int			mask_x,
			  int			mask_y,
			  int			dst_x,
			  int			dst_y,
			  unsigned int		width,
			  unsigned int		height);

cairo_private cairo_status_t
_cairo_surface_fill_rectangle (cairo_surface_t	   *surface,
			       cairo_operator_t	    op,
			       const cairo_color_t *color,
			       int		    x,
			       int		    y,
			       int		    width,
			       int		    height);

cairo_private cairo_status_t
_cairo_surface_fill_region (cairo_surface_t	   *surface,
			    cairo_operator_t	    op,
			    const cairo_color_t    *color,
			    cairo_region_t         *region);

cairo_private cairo_status_t
_cairo_surface_fill_rectangles (cairo_surface_t		*surface,
				cairo_operator_t         op,
				const cairo_color_t	*color,
				cairo_rectangle_int_t	*rects,
				int			 num_rects);

cairo_private cairo_status_t
_cairo_surface_paint (cairo_surface_t	*surface,
		      cairo_operator_t	 op,
		      cairo_pattern_t	*source);

cairo_private cairo_status_t
_cairo_surface_mask (cairo_surface_t	*surface,
		     cairo_operator_t	 op,
		     cairo_pattern_t	*source,
		     cairo_pattern_t	*mask);

cairo_private cairo_status_t
_cairo_surface_fill_stroke (cairo_surface_t	    *surface,
			    cairo_operator_t	     fill_op,
			    cairo_pattern_t	    *fill_source,
			    cairo_fill_rule_t	     fill_rule,
			    double		     fill_tolerance,
			    cairo_antialias_t	     fill_antialias,
			    cairo_path_fixed_t	    *path,
			    cairo_operator_t	     stroke_op,
			    cairo_pattern_t	    *stroke_source,
			    cairo_stroke_style_t    *stroke_style,
			    cairo_matrix_t	    *stroke_ctm,
			    cairo_matrix_t	    *stroke_ctm_inverse,
			    double		     stroke_tolerance,
			    cairo_antialias_t	     stroke_antialias);

cairo_private cairo_status_t
_cairo_surface_stroke (cairo_surface_t		*surface,
		       cairo_operator_t		 op,
		       cairo_pattern_t		*source,
		       cairo_path_fixed_t	*path,
		       cairo_stroke_style_t	*style,
		       cairo_matrix_t		*ctm,
		       cairo_matrix_t		*ctm_inverse,
		       double			 tolerance,
		       cairo_antialias_t	 antialias);

cairo_private cairo_status_t
_cairo_surface_fill (cairo_surface_t	*surface,
		     cairo_operator_t	 op,
		     cairo_pattern_t	*source,
		     cairo_path_fixed_t	*path,
		     cairo_fill_rule_t	 fill_rule,
		     double		 tolerance,
		     cairo_antialias_t	 antialias);

cairo_private cairo_status_t
_cairo_surface_show_glyphs (cairo_surface_t	*surface,
			    cairo_operator_t	 op,
			    cairo_pattern_t	*source,
			    cairo_glyph_t	*glyphs,
			    int			 num_glyphs,
			    cairo_scaled_font_t	*scaled_font);

cairo_private cairo_status_t
_cairo_surface_composite_trapezoids (cairo_operator_t	op,
				     cairo_pattern_t	*pattern,
				     cairo_surface_t	*dst,
				     cairo_antialias_t	antialias,
				     int		src_x,
				     int		src_y,
				     int		dst_x,
				     int		dst_y,
				     unsigned int	width,
				     unsigned int	height,
				     cairo_trapezoid_t	*traps,
				     int		ntraps);

cairo_private cairo_status_t
_cairo_surface_acquire_source_image (cairo_surface_t         *surface,
				     cairo_image_surface_t  **image_out,
				     void                   **image_extra);

cairo_private void
_cairo_surface_release_source_image (cairo_surface_t        *surface,
				     cairo_image_surface_t  *image,
				     void                   *image_extra);

cairo_private cairo_status_t
_cairo_surface_acquire_dest_image (cairo_surface_t         *surface,
				   cairo_rectangle_int_t   *interest_rect,
				   cairo_image_surface_t  **image_out,
				   cairo_rectangle_int_t   *image_rect,
				   void                   **image_extra);

cairo_private void
_cairo_surface_release_dest_image (cairo_surface_t        *surface,
				   cairo_rectangle_int_t  *interest_rect,
				   cairo_image_surface_t  *image,
				   cairo_rectangle_int_t  *image_rect,
				   void                   *image_extra);

cairo_private cairo_status_t
_cairo_surface_clone_similar (cairo_surface_t  *surface,
			      cairo_surface_t  *src,
			      int               src_x,
			      int               src_y,
			      int               width,
			      int               height,
			      cairo_surface_t **clone_out);

cairo_private cairo_surface_t *
_cairo_surface_snapshot (cairo_surface_t *surface);

cairo_private cairo_bool_t
_cairo_surface_is_similar (cairo_surface_t *surface_a,
	                   cairo_surface_t *surface_b,
			   cairo_content_t  content);

cairo_private cairo_status_t
_cairo_surface_reset (cairo_surface_t *surface);

cairo_private unsigned int
_cairo_surface_get_current_clip_serial (cairo_surface_t *surface);

cairo_private unsigned int
_cairo_surface_allocate_clip_serial (cairo_surface_t *surface);

cairo_private cairo_status_t
_cairo_surface_reset_clip (cairo_surface_t *surface);

cairo_private cairo_status_t
_cairo_surface_set_clip_region (cairo_surface_t	    *surface,
				cairo_region_t      *region,
				unsigned int	    serial);

cairo_private cairo_int_status_t
_cairo_surface_intersect_clip_path (cairo_surface_t    *surface,
				    cairo_path_fixed_t *path,
				    cairo_fill_rule_t   fill_rule,
				    double		tolerance,
				    cairo_antialias_t	antialias);

cairo_private cairo_status_t
_cairo_surface_set_clip (cairo_surface_t *surface, cairo_clip_t *clip);

cairo_private cairo_status_t
_cairo_surface_get_extents (cairo_surface_t         *surface,
			    cairo_rectangle_int_t   *rectangle);

cairo_private cairo_status_t
_cairo_surface_old_show_glyphs (cairo_scaled_font_t	*scaled_font,
				cairo_operator_t	 op,
				cairo_pattern_t		*pattern,
				cairo_surface_t		*surface,
				int			 source_x,
				int			 source_y,
				int			 dest_x,
				int			 dest_y,
				unsigned int		 width,
				unsigned int		 height,
				cairo_glyph_t		*glyphs,
				int			 num_glyphs);

cairo_private cairo_status_t
_cairo_surface_composite_fixup_unbounded (cairo_surface_t            *dst,
					  cairo_surface_attributes_t *src_attr,
					  int                         src_width,
					  int                         src_height,
					  cairo_surface_attributes_t *mask_attr,
					  int                         mask_width,
					  int                         mask_height,
					  int			      src_x,
					  int			      src_y,
					  int			      mask_x,
					  int			      mask_y,
					  int			      dst_x,
					  int			      dst_y,
					  unsigned int		      width,
					  unsigned int		      height);

cairo_private cairo_status_t
_cairo_surface_composite_shape_fixup_unbounded (cairo_surface_t            *dst,
						cairo_surface_attributes_t *src_attr,
						int                         src_width,
						int                         src_height,
						int                         mask_width,
						int                         mask_height,
						int			    src_x,
						int			    src_y,
						int			    mask_x,
						int			    mask_y,
						int			    dst_x,
						int			    dst_y,
						unsigned int		    width,
						unsigned int		    height);

cairo_private cairo_bool_t
_cairo_surface_is_opaque (const cairo_surface_t *surface);

cairo_private void
_cairo_surface_set_device_scale (cairo_surface_t *surface,
				 double		  sx,
				 double		  sy);

cairo_private cairo_bool_t
_cairo_surface_has_device_transform (cairo_surface_t *surface);

/* cairo_image_surface.c */

/* XXX: In cairo 1.2.0 we added a new CAIRO_FORMAT_RGB16_565 but
 * neglected to adjust this macro. The net effect is that it's
 * impossible to externally create an image surface with this
 * format. This is perhaps a good thing since we also neglected to fix
 * up things like cairo_surface_write_to_png for the new format
 * (-Wswitch-enum will tell you where). Is it obvious that format was
 * added in haste?
 *
 * The reason for the new format was to allow the xlib backend to be
 * used on X servers with a 565 visual. So the new format did its job
 * for that, even without being considered "valid" for the sake of
 * things like cairo_image_surface_create.
 *
 * Since 1.2.0 we ran into the same situtation with X servers with BGR
 * visuals. This time we invented cairo_internal_format_t instead,
 * (see it for more discussion).
 *
 * The punchline is that CAIRO_FORMAT_VALID must not conside any
 * internal format to be valid. Also we need to decide if the
 * RGB16_565 should be moved to instead be an internal format. If so,
 * this macro need not change for it. (We probably will need to leave
 * an RGB16_565 value in the header files for the sake of code that
 * might have that value in it.)
 *
 * If we do decide to start fully supporting RGB16_565 as an external
 * format, then CAIRO_FORMAT_VALID needs to be adjusted to include
 * it. But that should not happen before all necessary code is fixed
 * to support it (at least cairo_surface_write_to_png and a few spots
 * in cairo-xlib-surface.c--again see -Wswitch-enum).
 */
#define CAIRO_FORMAT_INVALID ((unsigned int) -1)
#define CAIRO_FORMAT_VALID(format) ((format) <= CAIRO_FORMAT_A1)

#define CAIRO_CONTENT_VALID(content) ((content) && 			         \
				      (((content) & ~(CAIRO_CONTENT_COLOR |      \
						      CAIRO_CONTENT_ALPHA |      \
						      CAIRO_CONTENT_COLOR_ALPHA))\
				       == 0))

cairo_private cairo_format_t
_cairo_format_from_content (cairo_content_t content);

cairo_private cairo_content_t
_cairo_content_from_format (cairo_format_t format);

cairo_private cairo_surface_t *
_cairo_image_surface_create_for_pixman_image (pixman_image_t		*pixman_image,
					      pixman_format_code_t	 pixman_format);

cairo_private pixman_format_code_t
_pixman_format_from_masks (cairo_format_masks_t *masks);

void
_pixman_format_to_masks (pixman_format_code_t	 pixman_format,
			 uint32_t		*bpp,
			 uint32_t		*red,
			 uint32_t		*green,
			 uint32_t		*blue);

cairo_private cairo_surface_t *
_cairo_image_surface_create_with_pixman_format (unsigned char		*data,
						pixman_format_code_t	 pixman_format,
						int			 width,
						int			 height,
						int			 stride);

cairo_private cairo_surface_t *
_cairo_image_surface_create_with_masks (unsigned char	       *data,
					cairo_format_masks_t   *format,
					int			width,
					int			height,
					int			stride);

cairo_private cairo_surface_t *
_cairo_image_surface_create_with_content (cairo_content_t	content,
					  int			width,
					  int			height);

cairo_private cairo_surface_t *
_cairo_image_surface_create_for_data_with_content (unsigned char	*data,
						   cairo_content_t	 content,
						   int			 width,
						   int			 height,
						   int			 stride);

cairo_private void
_cairo_image_surface_assume_ownership_of_data (cairo_image_surface_t *surface);

/* XXX: It's a nasty kludge that this appears here. Backend functions
 * like this should really be static. But we're doing this to work
 * around some general defects in the backend clipping interfaces,
 * (see some notes in test-paginated-surface.c).
 *
 * I want to fix the real defects, but it's "hard" as they touch many
 * backends, so doing that will require synchronizing several backend
 * maintainers.
 */
cairo_private cairo_int_status_t
_cairo_image_surface_set_clip_region (void *abstract_surface,
				      cairo_region_t *region);

cairo_private cairo_image_surface_t *
_cairo_image_surface_clone (cairo_image_surface_t	*surface,
			    cairo_format_t		 format);

cairo_private cairo_bool_t
_cairo_surface_is_image (const cairo_surface_t *surface);

cairo_private cairo_bool_t
_cairo_surface_is_meta (const cairo_surface_t *surface);

/* cairo_pen.c */
cairo_private cairo_status_t
_cairo_pen_init (cairo_pen_t	*pen,
		 double		 radius,
		 double		 tolerance,
		 cairo_matrix_t	*ctm);

cairo_private void
_cairo_pen_init_empty (cairo_pen_t *pen);

cairo_private cairo_status_t
_cairo_pen_init_copy (cairo_pen_t *pen, cairo_pen_t *other);

cairo_private void
_cairo_pen_fini (cairo_pen_t *pen);

cairo_private cairo_status_t
_cairo_pen_add_points (cairo_pen_t *pen, cairo_point_t *point, int num_points);

cairo_private cairo_status_t
_cairo_pen_add_points_for_slopes (cairo_pen_t *pen,
				  cairo_point_t *a,
				  cairo_point_t *b,
				  cairo_point_t *c,
				  cairo_point_t *d);

cairo_private void
_cairo_pen_find_active_cw_vertex_index (cairo_pen_t *pen,
					cairo_slope_t *slope,
					int *active);

cairo_private void
_cairo_pen_find_active_ccw_vertex_index (cairo_pen_t *pen,
					 cairo_slope_t *slope,
					 int *active);

cairo_private cairo_status_t
_cairo_pen_stroke_spline (cairo_pen_t *pen,
			  cairo_spline_t *spline,
			  double tolerance,
			  cairo_traps_t *traps);

/* cairo_polygon.c */
cairo_private void
_cairo_polygon_init (cairo_polygon_t *polygon);

cairo_private void
_cairo_polygon_fini (cairo_polygon_t *polygon);

cairo_private cairo_status_t
_cairo_polygon_status (cairo_polygon_t *polygon);

cairo_private void
_cairo_polygon_add_edge (cairo_polygon_t *polygon, cairo_point_t *p1, cairo_point_t *p2);

cairo_private void
_cairo_polygon_move_to (cairo_polygon_t *polygon, cairo_point_t *point);

cairo_private void
_cairo_polygon_line_to (cairo_polygon_t *polygon, cairo_point_t *point);

cairo_private void
_cairo_polygon_close (cairo_polygon_t *polygon);

/* cairo_spline.c */
cairo_private cairo_int_status_t
_cairo_spline_init (cairo_spline_t *spline,
		    cairo_point_t *a,
		    cairo_point_t *b,
		    cairo_point_t *c,
		    cairo_point_t *d);

cairo_private cairo_status_t
_cairo_spline_decompose (cairo_spline_t *spline, double tolerance);

cairo_private void
_cairo_spline_fini (cairo_spline_t *spline);

/* cairo_matrix.c */
cairo_private void
_cairo_matrix_get_affine (const cairo_matrix_t *matrix,
			  double *xx, double *yx,
			  double *xy, double *yy,
			  double *x0, double *y0);

cairo_private void
_cairo_matrix_transform_bounding_box (const cairo_matrix_t *matrix,
				      double *x1, double *y1,
				      double *x2, double *y2,
				      cairo_bool_t *is_tight);

cairo_private void
_cairo_matrix_compute_determinant (const cairo_matrix_t *matrix, double *det);

cairo_private void
_cairo_matrix_compute_scale_factors (const cairo_matrix_t *matrix,
				     double *sx, double *sy, int x_major);

cairo_private cairo_bool_t
_cairo_matrix_is_identity (const cairo_matrix_t *matrix);

cairo_private cairo_bool_t
_cairo_matrix_is_translation (const cairo_matrix_t *matrix);

cairo_private cairo_bool_t
_cairo_matrix_is_integer_translation(const cairo_matrix_t *matrix,
				     int *itx, int *ity);

cairo_private double
_cairo_matrix_transformed_circle_major_axis(cairo_matrix_t *matrix, double radius);

cairo_private void
_cairo_matrix_to_pixman_matrix (const cairo_matrix_t	*matrix,
				pixman_transform_t	*pixman_transform);

/* cairo_traps.c */
cairo_private void
_cairo_traps_init (cairo_traps_t *traps);

cairo_private void
_cairo_traps_limit (cairo_traps_t	*traps,
		    cairo_box_t		*limits);

cairo_private cairo_status_t
_cairo_traps_init_box (cairo_traps_t *traps,
		       cairo_box_t   *box);

cairo_private void
_cairo_traps_fini (cairo_traps_t *traps);

cairo_private cairo_status_t
_cairo_traps_status (cairo_traps_t *traps);

cairo_private void
_cairo_traps_translate (cairo_traps_t *traps, int x, int y);

cairo_private cairo_status_t
_cairo_traps_tessellate_triangle (cairo_traps_t *traps, cairo_point_t t[3]);

cairo_private cairo_status_t
_cairo_traps_tessellate_convex_quad (cairo_traps_t *traps, cairo_point_t q[4]);

cairo_private cairo_status_t
_cairo_traps_tessellate_polygon (cairo_traps_t *traps,
				 cairo_polygon_t *poly,
				 cairo_fill_rule_t fill_rule);

cairo_private void
_cairo_traps_add_trap_from_points (cairo_traps_t *traps, cairo_fixed_t top, cairo_fixed_t bottom,
				   cairo_point_t left_p1, cairo_point_t left_p2,
				   cairo_point_t right_p1, cairo_point_t right_p2);

cairo_private cairo_status_t
_cairo_bentley_ottmann_tessellate_polygon (cairo_traps_t      *traps,
					   cairo_polygon_t      *polygon,
					   cairo_fill_rule_t     fill_rule);

cairo_private int
_cairo_traps_contain (cairo_traps_t *traps, double x, double y);

cairo_private void
_cairo_traps_extents (cairo_traps_t *traps, cairo_box_t *extents);

cairo_private cairo_int_status_t
_cairo_traps_extract_region (cairo_traps_t     *tr,
			     cairo_region_t *region);

cairo_private void
_cairo_trapezoid_array_translate_and_scale (cairo_trapezoid_t *offset_traps,
					    cairo_trapezoid_t *src_traps,
					    int num_traps,
					    double tx, double ty,
					    double sx, double sy);

/* cairo_slope.c */
cairo_private void
_cairo_slope_init (cairo_slope_t *slope, cairo_point_t *a, cairo_point_t *b);

cairo_private int
_cairo_slope_compare (cairo_slope_t *a, cairo_slope_t *b);

cairo_private int
_cairo_slope_clockwise (cairo_slope_t *a, cairo_slope_t *b);

cairo_private int
_cairo_slope_counter_clockwise (cairo_slope_t *a, cairo_slope_t *b);

/* cairo_pattern.c */

cairo_private cairo_status_t
_cairo_pattern_init_copy (cairo_pattern_t	*pattern,
			  const cairo_pattern_t *other);

cairo_private void
_cairo_pattern_init_solid (cairo_solid_pattern_t	*pattern,
			   const cairo_color_t		*color,
			   cairo_content_t		 content);

cairo_private void
_cairo_pattern_init_for_surface (cairo_surface_pattern_t *pattern,
				 cairo_surface_t *surface);

cairo_private void
_cairo_pattern_init_linear (cairo_linear_pattern_t *pattern,
			    double x0, double y0, double x1, double y1);

cairo_private void
_cairo_pattern_init_radial (cairo_radial_pattern_t *pattern,
			    double cx0, double cy0, double radius0,
			    double cx1, double cy1, double radius1);

cairo_private void
_cairo_pattern_fini (cairo_pattern_t *pattern);

cairo_private cairo_pattern_t *
_cairo_pattern_create_solid (const cairo_color_t	*color,
			     cairo_content_t		 content);

cairo_private void
_cairo_pattern_transform (cairo_pattern_t      *pattern,
			  const cairo_matrix_t *ctm_inverse);

cairo_private cairo_bool_t
_cairo_pattern_is_opaque_solid (const cairo_pattern_t *pattern);

cairo_private cairo_bool_t
_cairo_pattern_is_opaque (const cairo_pattern_t *abstract_pattern);

cairo_private cairo_int_status_t
_cairo_pattern_acquire_surface (cairo_pattern_t		   *pattern,
				cairo_surface_t		   *dst,
				int			   x,
				int			   y,
				unsigned int		   width,
				unsigned int		   height,
				cairo_surface_t		   **surface_out,
				cairo_surface_attributes_t *attributes);

cairo_private void
_cairo_pattern_release_surface (cairo_pattern_t		   *pattern,
				cairo_surface_t		   *surface,
				cairo_surface_attributes_t *attributes);

cairo_private cairo_int_status_t
_cairo_pattern_acquire_surfaces (cairo_pattern_t	    *src,
				 cairo_pattern_t	    *mask,
				 cairo_surface_t	    *dst,
				 int			    src_x,
				 int			    src_y,
				 int			    mask_x,
				 int			    mask_y,
				 unsigned int		    width,
				 unsigned int		    height,
				 cairo_surface_t	    **src_out,
				 cairo_surface_t	    **mask_out,
				 cairo_surface_attributes_t *src_attributes,
				 cairo_surface_attributes_t *mask_attributes);

cairo_private cairo_status_t
_cairo_pattern_get_extents (cairo_pattern_t	    *pattern,
			    cairo_rectangle_int_t   *extents);

cairo_private void
_cairo_pattern_reset_static_data (void);

cairo_private cairo_status_t
_cairo_gstate_set_antialias (cairo_gstate_t *gstate,
			     cairo_antialias_t antialias);

cairo_private cairo_antialias_t
_cairo_gstate_get_antialias (cairo_gstate_t *gstate);

/* cairo-region.c */

#include "cairo-region-private.h"

/* cairo_unicode.c */

cairo_private cairo_status_t
_cairo_utf8_to_ucs4 (const unsigned char *str,
		     int		  len,
		     uint32_t	        **result,
		     int		 *items_written);

cairo_private cairo_status_t
_cairo_utf8_to_utf16 (const unsigned char *str,
		      int		   len,
		      uint16_t		 **result,
		      int		  *items_written);

cairo_private void
_cairo_error (cairo_status_t status);

/* Avoid unnecessary PLT entries.  */
slim_hidden_proto (cairo_clip_preserve);
slim_hidden_proto (cairo_close_path);
slim_hidden_proto (cairo_create);
slim_hidden_proto (cairo_curve_to);
slim_hidden_proto (cairo_destroy);
slim_hidden_proto (cairo_fill_preserve);
slim_hidden_proto (cairo_font_face_destroy);
slim_hidden_proto_no_warn (cairo_font_face_reference);
slim_hidden_proto (cairo_font_options_create);
slim_hidden_proto (cairo_font_options_destroy);
slim_hidden_proto (cairo_font_options_equal);
slim_hidden_proto (cairo_font_options_hash);
slim_hidden_proto (cairo_font_options_merge);
slim_hidden_proto (cairo_font_options_set_antialias);
slim_hidden_proto (cairo_font_options_set_hint_metrics);
slim_hidden_proto (cairo_font_options_set_hint_style);
slim_hidden_proto (cairo_font_options_set_subpixel_order);
slim_hidden_proto (cairo_font_options_status);
slim_hidden_proto (cairo_get_current_point);
slim_hidden_proto (cairo_get_matrix);
slim_hidden_proto (cairo_get_tolerance);
slim_hidden_proto (cairo_image_surface_create);
slim_hidden_proto (cairo_image_surface_create_for_data);
slim_hidden_proto (cairo_image_surface_get_height);
slim_hidden_proto (cairo_image_surface_get_width);
slim_hidden_proto (cairo_line_to);
slim_hidden_proto (cairo_mask);
slim_hidden_proto (cairo_matrix_init);
slim_hidden_proto (cairo_matrix_init_identity);
slim_hidden_proto (cairo_matrix_init_rotate);
slim_hidden_proto (cairo_matrix_init_scale);
slim_hidden_proto (cairo_matrix_init_translate);
slim_hidden_proto (cairo_matrix_invert);
slim_hidden_proto (cairo_matrix_multiply);
slim_hidden_proto (cairo_matrix_scale);
slim_hidden_proto (cairo_matrix_transform_distance);
slim_hidden_proto (cairo_matrix_transform_point);
slim_hidden_proto (cairo_matrix_translate);
slim_hidden_proto (cairo_move_to);
slim_hidden_proto (cairo_new_path);
slim_hidden_proto (cairo_paint);
slim_hidden_proto (cairo_pattern_create_for_surface);
slim_hidden_proto (cairo_pattern_create_rgb);
slim_hidden_proto (cairo_pattern_create_rgba);
slim_hidden_proto (cairo_pattern_destroy);
slim_hidden_proto (cairo_pattern_get_extend);
slim_hidden_proto (cairo_pattern_get_type);
slim_hidden_proto_no_warn (cairo_pattern_reference);
slim_hidden_proto (cairo_pattern_set_matrix);
slim_hidden_proto (cairo_pattern_status);
slim_hidden_proto (cairo_pop_group);
slim_hidden_proto (cairo_pop_group_to_source);
slim_hidden_proto (cairo_push_group);
slim_hidden_proto (cairo_push_group_with_content);
slim_hidden_proto (cairo_rel_line_to);
slim_hidden_proto (cairo_restore);
slim_hidden_proto (cairo_save);
slim_hidden_proto (cairo_scale);
slim_hidden_proto (cairo_scaled_font_create);
slim_hidden_proto (cairo_scaled_font_destroy);
slim_hidden_proto (cairo_scaled_font_extents);
slim_hidden_proto (cairo_scaled_font_get_ctm);
slim_hidden_proto (cairo_scaled_font_get_font_face);
slim_hidden_proto (cairo_scaled_font_get_font_matrix);
slim_hidden_proto (cairo_scaled_font_get_font_options);
slim_hidden_proto (cairo_scaled_font_glyph_extents);
slim_hidden_proto_no_warn (cairo_scaled_font_reference);
slim_hidden_proto (cairo_scaled_font_status);
slim_hidden_proto (cairo_set_operator);
slim_hidden_proto (cairo_set_source);
slim_hidden_proto (cairo_set_source_surface);
slim_hidden_proto (cairo_status);
slim_hidden_proto (cairo_stroke_preserve);
slim_hidden_proto (cairo_surface_create_similar);
slim_hidden_proto (cairo_surface_destroy);
slim_hidden_proto (cairo_surface_finish);
slim_hidden_proto (cairo_surface_get_content);
slim_hidden_proto (cairo_surface_get_device_offset);
slim_hidden_proto (cairo_surface_get_font_options);
slim_hidden_proto (cairo_surface_get_type);
slim_hidden_proto (cairo_surface_mark_dirty_rectangle);
slim_hidden_proto_no_warn (cairo_surface_reference);
slim_hidden_proto (cairo_surface_set_device_offset);
slim_hidden_proto (cairo_surface_set_fallback_resolution);
slim_hidden_proto (cairo_surface_copy_page);
slim_hidden_proto (cairo_surface_show_page);
slim_hidden_proto (cairo_surface_status);
slim_hidden_proto (cairo_version_string);

#if CAIRO_HAS_PNG_FUNCTIONS

slim_hidden_proto (cairo_surface_write_to_png_stream);

#endif

CAIRO_END_DECLS

#include "cairo-mutex-private.h"
#include "cairo-wideint-private.h"
#include "cairo-malloc-private.h"
#include "cairo-hash-private.h"

#endif
