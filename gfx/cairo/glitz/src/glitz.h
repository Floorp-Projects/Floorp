/*
 * Copyright © 2004 David Reveman
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * David Reveman not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * David Reveman makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * DAVID REVEMAN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL DAVID REVEMAN BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: David Reveman <davidr@novell.com>
 */

#ifndef GLITZ_H_INCLUDED
#define GLITZ_H_INCLUDED

#if defined(__SVR4) && defined(__sun)
#  include <sys/int_types.h>
#else
#  if defined(__OpenBSD__)
#    include <inttypes.h>
#  else
#    include <stdint.h>
#  endif
#endif

#define GLITZ_MAJOR    0
#define GLITZ_MINOR    5
#define GLITZ_REVISION 0

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

typedef int glitz_bool_t;
typedef short glitz_short_t;
typedef int glitz_int_t;
typedef float glitz_float_t;
typedef double glitz_double_t;
typedef int glitz_fixed16_16_t;

typedef struct _glitz_drawable glitz_drawable_t;
typedef struct _glitz_surface glitz_surface_t;

typedef struct _glitz_rectangle_t {
  short          x, y;
  unsigned short width, height;
} glitz_rectangle_t;

typedef struct _glitz_box_t {
  short x1, y1, x2, y2;
} glitz_box_t;

typedef struct _glitz_point_fixed_t {
  glitz_fixed16_16_t x;
  glitz_fixed16_16_t y;
} glitz_point_fixed_t;

typedef struct _glitz_line_fixed_t {
  glitz_point_fixed_t p1;
  glitz_point_fixed_t p2;
} glitz_line_fixed_t;

typedef struct _glitz_trapezoid_t {
  glitz_fixed16_16_t top, bottom;
  glitz_line_fixed_t left, right;
} glitz_trapezoid_t;

typedef struct _glitz_span_fixed_t {
  glitz_fixed16_16_t left, right, y;
} glitz_span_fixed_t;

typedef struct _glitz_trap_t {
  glitz_span_fixed_t top, bottom;
} glitz_trap_t;

typedef struct _glitz_transform_t {
  glitz_fixed16_16_t matrix[3][3];
} glitz_transform_t;

typedef struct {
  unsigned short red;
  unsigned short green;
  unsigned short blue;
  unsigned short alpha;
} glitz_color_t;

typedef enum {
  GLITZ_FILTER_NEAREST,
  GLITZ_FILTER_BILINEAR,
  GLITZ_FILTER_CONVOLUTION,
  GLITZ_FILTER_GAUSSIAN,
  GLITZ_FILTER_LINEAR_GRADIENT,
  GLITZ_FILTER_RADIAL_GRADIENT
} glitz_filter_t;

typedef enum {
  GLITZ_OPERATOR_CLEAR,
  GLITZ_OPERATOR_SRC,
  GLITZ_OPERATOR_DST,
  GLITZ_OPERATOR_OVER,
  GLITZ_OPERATOR_OVER_REVERSE,
  GLITZ_OPERATOR_IN,
  GLITZ_OPERATOR_IN_REVERSE,
  GLITZ_OPERATOR_OUT,
  GLITZ_OPERATOR_OUT_REVERSE,
  GLITZ_OPERATOR_ATOP,
  GLITZ_OPERATOR_ATOP_REVERSE,
  GLITZ_OPERATOR_XOR,
  GLITZ_OPERATOR_ADD
} glitz_operator_t;

#define GLITZ_FEATURE_TEXTURE_RECTANGLE_MASK        (1L <<  0)
#define GLITZ_FEATURE_TEXTURE_NON_POWER_OF_TWO_MASK (1L <<  1)
#define GLITZ_FEATURE_TEXTURE_MIRRORED_REPEAT_MASK  (1L <<  2)
#define GLITZ_FEATURE_TEXTURE_BORDER_CLAMP_MASK     (1L <<  3)
#define GLITZ_FEATURE_MULTISAMPLE_MASK              (1L <<  4)
#define GLITZ_FEATURE_MULTISAMPLE_FILTER_HINT_MASK  (1L <<  5)
#define GLITZ_FEATURE_MULTITEXTURE_MASK             (1L <<  6)
#define GLITZ_FEATURE_TEXTURE_ENV_COMBINE_MASK      (1L <<  7)
#define GLITZ_FEATURE_TEXTURE_ENV_DOT3_MASK         (1L <<  8)
#define GLITZ_FEATURE_FRAGMENT_PROGRAM_MASK         (1L <<  9)
#define GLITZ_FEATURE_VERTEX_BUFFER_OBJECT_MASK     (1L << 10)
#define GLITZ_FEATURE_PIXEL_BUFFER_OBJECT_MASK      (1L << 11)
#define GLITZ_FEATURE_PER_COMPONENT_RENDERING_MASK  (1L << 12)
#define GLITZ_FEATURE_BLEND_COLOR_MASK              (1L << 13)
#define GLITZ_FEATURE_PACKED_PIXELS_MASK            (1L << 14)
#define GLITZ_FEATURE_MULTI_DRAW_ARRAYS_MASK        (1L << 15)
#define GLITZ_FEATURE_FRAMEBUFFER_OBJECT_MASK       (1L << 16)


/* glitz_format.c */

typedef enum {
  GLITZ_STANDARD_ARGB32,
  GLITZ_STANDARD_RGB24,
  GLITZ_STANDARD_A8,
  GLITZ_STANDARD_A1
} glitz_format_name_t;

#define GLITZ_FORMAT_ID_MASK         (1L <<  0)
#define GLITZ_FORMAT_RED_SIZE_MASK   (1L <<  1)
#define GLITZ_FORMAT_GREEN_SIZE_MASK (1L <<  2)
#define GLITZ_FORMAT_BLUE_SIZE_MASK  (1L <<  3)
#define GLITZ_FORMAT_ALPHA_SIZE_MASK (1L <<  4)

typedef unsigned long glitz_format_id_t;

typedef struct _glitz_color_format_t {
  unsigned short red_size;
  unsigned short green_size;
  unsigned short blue_size;
  unsigned short alpha_size;
} glitz_color_format_t;

#define GLITZ_FORMAT_DEPTH_SIZE_MASK   (1L <<  5)
#define GLITZ_FORMAT_STENCIL_SIZE_MASK (1L <<  6)
#define GLITZ_FORMAT_DOUBLEBUFFER_MASK (1L <<  7)
#define GLITZ_FORMAT_SAMPLES_MASK      (1L <<  8)

typedef struct _glitz_drawable_format_t {
  glitz_format_id_t    id;
  glitz_color_format_t color;
  unsigned short       depth_size;
  unsigned short       stencil_size;
  unsigned short       samples;
  glitz_bool_t         doublebuffer;
} glitz_drawable_format_t;

#define GLITZ_FORMAT_TYPE_MASK (1L << 5)

typedef enum {
  GLITZ_FORMAT_TYPE_COLOR
} glitz_format_type_t;

typedef struct _glitz_format_t {
  glitz_format_id_t    id;
  glitz_format_type_t  type;
  glitz_color_format_t color;
} glitz_format_t;

glitz_format_t *
glitz_find_standard_format (glitz_drawable_t    *drawable,
			    glitz_format_name_t format_name);

glitz_format_t *
glitz_find_format (glitz_drawable_t     *drawable,
		   unsigned long        mask,
		   const glitz_format_t *templ,
		   int                  count);

glitz_drawable_format_t *
glitz_find_drawable_format (glitz_drawable_t              *other,
			    unsigned long                 mask,
			    const glitz_drawable_format_t *templ,
			    int                           count);

glitz_drawable_format_t *
glitz_find_pbuffer_format (glitz_drawable_t              *other,
			   unsigned long                 mask,
			   const glitz_drawable_format_t *templ,
			   int                           count);


/* glitz_status.c */

typedef enum {
  GLITZ_STATUS_SUCCESS = 0,
  GLITZ_STATUS_NO_MEMORY,
  GLITZ_STATUS_BAD_COORDINATE,
  GLITZ_STATUS_NOT_SUPPORTED,
  GLITZ_STATUS_CONTENT_DESTROYED
} glitz_status_t;

const char *
glitz_status_string (glitz_status_t status);


/* glitz_drawable.c */

typedef enum {
  GLITZ_DRAWABLE_BUFFER_FRONT_COLOR,
  GLITZ_DRAWABLE_BUFFER_BACK_COLOR
} glitz_drawable_buffer_t;

glitz_drawable_t *
glitz_create_drawable (glitz_drawable_t        *other,
		       glitz_drawable_format_t *format,
		       unsigned int            width,
		       unsigned int            height);

glitz_drawable_t *
glitz_create_pbuffer_drawable (glitz_drawable_t        *other,
			       glitz_drawable_format_t *format,
			       unsigned int            width,
			       unsigned int            height);

void
glitz_drawable_destroy (glitz_drawable_t *drawable);

void
glitz_drawable_reference (glitz_drawable_t *drawable);

void
glitz_drawable_update_size (glitz_drawable_t *drawable,
			    unsigned int     width,
			    unsigned int     height);

unsigned int
glitz_drawable_get_width (glitz_drawable_t *drawable);

unsigned int
glitz_drawable_get_height (glitz_drawable_t *drawable);

void
glitz_drawable_swap_buffer_region (glitz_drawable_t *drawable,
				   int              x_origin,
				   int              y_origin,
				   glitz_box_t      *box,
				   int              n_box);

void
glitz_drawable_swap_buffers (glitz_drawable_t *drawable);

void
glitz_drawable_flush (glitz_drawable_t *drawable);

void
glitz_drawable_finish (glitz_drawable_t *drawable);

unsigned long
glitz_drawable_get_features (glitz_drawable_t *drawable);

glitz_drawable_format_t *
glitz_drawable_get_format (glitz_drawable_t *drawable);


/* glitz_surface.c */

#define GLITZ_SURFACE_UNNORMALIZED_MASK (1L << 0)

typedef struct _glitz_surface_attributes_t {
  glitz_bool_t unnormalized;
} glitz_surface_attributes_t;

glitz_surface_t *
glitz_surface_create (glitz_drawable_t           *drawable,
		      glitz_format_t             *format,
		      unsigned int               width,
		      unsigned int               height,
		      unsigned long              mask,
		      glitz_surface_attributes_t *attributes);

void
glitz_surface_destroy (glitz_surface_t *surface);

void
glitz_surface_reference (glitz_surface_t *surface);

void
glitz_surface_attach (glitz_surface_t         *surface,
		      glitz_drawable_t        *drawable,
		      glitz_drawable_buffer_t buffer);

void
glitz_surface_detach (glitz_surface_t *surface);

void
glitz_surface_flush (glitz_surface_t *surface);

glitz_drawable_t *
glitz_surface_get_drawable (glitz_surface_t *surface);

glitz_drawable_t *
glitz_surface_get_attached_drawable (glitz_surface_t *surface);

void
glitz_surface_set_transform (glitz_surface_t   *surface,
			     glitz_transform_t *transform);

typedef enum {
  GLITZ_FILL_TRANSPARENT,
  GLITZ_FILL_NEAREST,
  GLITZ_FILL_REPEAT,
  GLITZ_FILL_REFLECT
} glitz_fill_t;

void
glitz_surface_set_fill (glitz_surface_t *surface,
			glitz_fill_t    fill);

void
glitz_surface_set_component_alpha (glitz_surface_t *surface,
				   glitz_bool_t    component_alpha);

void
glitz_surface_set_filter (glitz_surface_t    *surface,
			  glitz_filter_t     filter,
			  glitz_fixed16_16_t *params,
			  int                n_params);

void
glitz_surface_set_dither (glitz_surface_t *surface,
			  glitz_bool_t    dither);

unsigned int
glitz_surface_get_width (glitz_surface_t *surface);

unsigned int
glitz_surface_get_height (glitz_surface_t *surface);

glitz_status_t
glitz_surface_get_status (glitz_surface_t *surface);

glitz_format_t *
glitz_surface_get_format (glitz_surface_t *surface);

void
glitz_surface_translate_point (glitz_surface_t     *surface,
			       glitz_point_fixed_t *src,
			       glitz_point_fixed_t *dst);

void
glitz_surface_set_clip_region (glitz_surface_t *surface,
			       int             x_origin,
			       int             y_origin,
			       glitz_box_t     *box,
			       int             n_box);

glitz_bool_t
glitz_surface_valid_target (glitz_surface_t *surface);


/* glitz_texture.c */

typedef struct _glitz_texture_object glitz_texture_object_t;

glitz_texture_object_t *
glitz_texture_object_create (glitz_surface_t *surface);

void
glitz_texture_object_destroy (glitz_texture_object_t *texture);

void
glitz_texture_object_reference (glitz_texture_object_t *texture);

typedef enum {
  GLITZ_TEXTURE_FILTER_TYPE_MAG = 0,
  GLITZ_TEXTURE_FILTER_TYPE_MIN = 1
} glitz_texture_filter_type_t;

typedef enum {
  GLITZ_TEXTURE_FILTER_NEAREST = 0,
  GLITZ_TEXTURE_FILTER_LINEAR  = 1
} glitz_texture_filter_t;

void
glitz_texture_object_set_filter (glitz_texture_object_t      *texture,
				 glitz_texture_filter_type_t type,
				 glitz_texture_filter_t      filter);

typedef enum {
  GLITZ_TEXTURE_WRAP_TYPE_S = 0,
  GLITZ_TEXTURE_WRAP_TYPE_T = 1
} glitz_texture_wrap_type_t;

typedef enum {
  GLITZ_TEXTURE_WRAP_CLAMP	     = 0,
  GLITZ_TEXTURE_WRAP_CLAMP_TO_EDGE   = 1,
  GLITZ_TEXTURE_WRAP_CLAMP_TO_BORDER = 2,
  GLITZ_TEXTURE_WRAP_REPEAT	     = 3,
  GLITZ_TEXTURE_WRAP_MIRRORED_REPEAT = 4
} glitz_texture_wrap_t;

void
glitz_texture_object_set_wrap (glitz_texture_object_t    *texture,
			       glitz_texture_wrap_type_t type,
			       glitz_texture_wrap_t      wrap);

void
glitz_texture_object_set_border_color (glitz_texture_object_t *texture,
				       glitz_color_t          *color);

typedef enum {
  GLITZ_TEXTURE_TARGET_2D   = 0,
  GLITZ_TEXTURE_TARGET_RECT = 1
} glitz_texture_target_t;

glitz_texture_target_t
glitz_texture_object_get_target (glitz_texture_object_t *texture);


/* glitz_context.c */

typedef struct _glitz_context glitz_context_t;

glitz_context_t *
glitz_context_create (glitz_drawable_t        *drawable,
		      glitz_drawable_format_t *format);

void
glitz_context_destroy (glitz_context_t *context);

void
glitz_context_reference (glitz_context_t *context);

void
glitz_context_copy (glitz_context_t *src,
		    glitz_context_t *dst,
		    unsigned long   mask);

typedef void (*glitz_lose_current_function_t) (void *closure);

void
glitz_context_set_user_data (glitz_context_t               *context,
			     void                          *closure,
			     glitz_lose_current_function_t lose_current);

typedef void (*glitz_function_pointer_t) (void);

glitz_function_pointer_t
glitz_context_get_proc_address (glitz_context_t *context,
				const char      *name);

void
glitz_context_make_current (glitz_context_t  *context,
			    glitz_drawable_t *drawable);

void
glitz_context_bind_texture (glitz_context_t        *context,
			    glitz_texture_object_t *texture);


/* glitz_rect.c */

void
glitz_set_rectangle (glitz_surface_t     *dst,
		     const glitz_color_t *color,
		     int                 x,
		     int                 y,
		     unsigned int        width,
		     unsigned int        height);

void
glitz_set_rectangles (glitz_surface_t         *dst,
		      const glitz_color_t     *color,
		      const glitz_rectangle_t *rects,
		      int                     n_rects);


/* glitz_buffer.c */

typedef struct _glitz_buffer glitz_buffer_t;

typedef enum {
  GLITZ_BUFFER_HINT_STREAM_DRAW,
  GLITZ_BUFFER_HINT_STREAM_READ,
  GLITZ_BUFFER_HINT_STREAM_COPY,
  GLITZ_BUFFER_HINT_STATIC_DRAW,
  GLITZ_BUFFER_HINT_STATIC_READ,
  GLITZ_BUFFER_HINT_STATIC_COPY,
  GLITZ_BUFFER_HINT_DYNAMIC_DRAW,
  GLITZ_BUFFER_HINT_DYNAMIC_READ,
  GLITZ_BUFFER_HINT_DYNAMIC_COPY
} glitz_buffer_hint_t;

typedef enum {
  GLITZ_BUFFER_ACCESS_READ_ONLY,
  GLITZ_BUFFER_ACCESS_WRITE_ONLY,
  GLITZ_BUFFER_ACCESS_READ_WRITE
} glitz_buffer_access_t;

glitz_buffer_t *
glitz_vertex_buffer_create (glitz_drawable_t    *drawable,
			    void                *data,
			    unsigned int        size,
			    glitz_buffer_hint_t hint);

glitz_buffer_t *
glitz_pixel_buffer_create (glitz_drawable_t    *drawable,
			   void                *data,
			   unsigned int        size,
			   glitz_buffer_hint_t hint);

glitz_buffer_t *
glitz_buffer_create_for_data (void *data);

void
glitz_buffer_destroy (glitz_buffer_t *buffer);

void
glitz_buffer_reference (glitz_buffer_t *buffer);

void
glitz_buffer_set_data (glitz_buffer_t *buffer,
		       int            offset,
		       unsigned int   size,
		       const void     *data);

void
glitz_buffer_get_data (glitz_buffer_t *buffer,
		       int            offset,
		       unsigned int   size,
		       void           *data);

void *
glitz_buffer_map (glitz_buffer_t        *buffer,
		  glitz_buffer_access_t access);

glitz_status_t
glitz_buffer_unmap (glitz_buffer_t *buffer);


/* glitz_pixel.c */

typedef enum {
  GLITZ_PIXEL_SCANLINE_ORDER_TOP_DOWN,
  GLITZ_PIXEL_SCANLINE_ORDER_BOTTOM_UP
} glitz_pixel_scanline_order_t;

typedef struct _glitz_pixel_masks {
  int           bpp;
  unsigned long alpha_mask;
  unsigned long red_mask;
  unsigned long green_mask;
  unsigned long blue_mask;
} glitz_pixel_masks_t;

typedef struct _glitz_pixel_format {
  glitz_pixel_masks_t          masks;
  int                          xoffset;
  int                          skip_lines;
  int                          bytes_per_line;
  glitz_pixel_scanline_order_t scanline_order;
} glitz_pixel_format_t;

void
glitz_set_pixels (glitz_surface_t      *dst,
		  int                  x_dst,
		  int                  y_dst,
		  int                  width,
		  int                  height,
		  glitz_pixel_format_t *format,
		  glitz_buffer_t       *buffer);

void
glitz_get_pixels (glitz_surface_t      *src,
		  int                  x_src,
		  int                  y_src,
		  int                  width,
		  int                  height,
		  glitz_pixel_format_t *format,
		  glitz_buffer_t       *buffer);


/* glitz_geometry.c */

typedef enum {
  GLITZ_PRIMITIVE_POINTS,
  GLITZ_PRIMITIVE_LINES,
  GLITZ_PRIMITIVE_LINE_STRIP,
  GLITZ_PRIMITIVE_LINE_LOOP,
  GLITZ_PRIMITIVE_TRIANGLES,
  GLITZ_PRIMITIVE_TRIANGLE_STRIP,
  GLITZ_PRIMITIVE_TRIANGLE_FAN,
  GLITZ_PRIMITIVE_QUADS,
  GLITZ_PRIMITIVE_QUAD_STRIP,
  GLITZ_PRIMITIVE_POLYGON
} glitz_primitive_t;

typedef enum {
  GLITZ_DATA_TYPE_SHORT,
  GLITZ_DATA_TYPE_INT,
  GLITZ_DATA_TYPE_FLOAT,
  GLITZ_DATA_TYPE_DOUBLE
} glitz_data_type_t;

typedef enum {
  GLITZ_COORDINATE_SIZE_X,
  GLITZ_COORDINATE_SIZE_XY
} glitz_coordinate_size_t;

typedef struct _glitz_coordinate_attribute {
    glitz_data_type_t       type;
    glitz_coordinate_size_t size;
    int                     offset;
} glitz_coordinate_attribute_t;

#define GLITZ_VERTEX_ATTRIBUTE_SRC_COORD_MASK  (1L << 0)
#define GLITZ_VERTEX_ATTRIBUTE_MASK_COORD_MASK (1L << 1)

typedef struct _glitz_vertex_format {
  glitz_primitive_t            primitive;
  glitz_data_type_t            type;
  unsigned int                 bytes_per_vertex;
  unsigned long                attributes;
  glitz_coordinate_attribute_t src;
  glitz_coordinate_attribute_t mask;
} glitz_vertex_format_t;

typedef struct _glitz_bitmap_format {
  glitz_pixel_scanline_order_t scanline_order;
  unsigned int                 bytes_per_line;
  int                          pad;
} glitz_bitmap_format_t;

typedef enum {
  GLITZ_GEOMETRY_TYPE_NONE,
  GLITZ_GEOMETRY_TYPE_VERTEX,
  GLITZ_GEOMETRY_TYPE_BITMAP
} glitz_geometry_type_t;

typedef union _glitz_geometry_format {
  glitz_vertex_format_t vertex;
  glitz_bitmap_format_t bitmap;
} glitz_geometry_format_t;

void
glitz_set_geometry (glitz_surface_t         *dst,
		    glitz_geometry_type_t   type,
		    glitz_geometry_format_t *format,
		    glitz_buffer_t          *buffer);

void
glitz_set_array (glitz_surface_t    *dst,
		 int                first,
		 int                size,
		 unsigned int       count,
		 glitz_fixed16_16_t x_off,
		 glitz_fixed16_16_t y_off);

typedef struct _glitz_multi_array glitz_multi_array_t;

glitz_multi_array_t *
glitz_multi_array_create (unsigned int size);

void
glitz_multi_array_destroy (glitz_multi_array_t *array);

void
glitz_multi_array_reference (glitz_multi_array_t *array);

void
glitz_multi_array_add (glitz_multi_array_t *array,
		       int                 first,
		       int                 size,
		       unsigned int        count,
		       glitz_fixed16_16_t  x_off,
		       glitz_fixed16_16_t  y_off);

void
glitz_multi_array_reset (glitz_multi_array_t *array);

void
glitz_set_multi_array (glitz_surface_t     *dst,
		       glitz_multi_array_t *array,
		       glitz_fixed16_16_t  x_off,
		       glitz_fixed16_16_t  y_off);


/* glitz_trap.c */

int
glitz_add_trapezoids (glitz_buffer_t    *buffer,
		      int               offset,
		      unsigned int      size,
		      glitz_data_type_t type,
		      glitz_surface_t   *mask,
		      glitz_trapezoid_t *traps,
		      int               n_traps,
		      int               *n_added);

int
glitz_add_traps (glitz_buffer_t    *buffer,
		 int               offset,
		 unsigned int      size,
		 glitz_data_type_t type,
		 glitz_surface_t   *mask,
		 glitz_trap_t      *traps,
		 int               n_traps,
		 int               *n_added);


/* glitz.c */

void
glitz_composite (glitz_operator_t op,
		 glitz_surface_t  *src,
		 glitz_surface_t  *mask,
		 glitz_surface_t  *dst,
		 int              x_src,
		 int              y_src,
		 int              x_mask,
		 int              y_mask,
		 int              x_dst,
		 int              y_dst,
		 int              width,
		 int              height);

void
glitz_copy_area (glitz_surface_t *src,
		 glitz_surface_t *dst,
		 int             x_src,
		 int             y_src,
		 int             width,
		 int             height,
		 int             x_dst,
		 int             y_dst);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif /* GLITZ_H_INCLUDED */
