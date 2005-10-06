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

#ifndef GLITZINT_H_INCLUDED
#define GLITZINT_H_INCLUDED

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include "glitz.h"

#if defined(__APPLE__) || defined(__sun__)
# define floorf(a)    floor (a)
# define ceilf(a)     ceil (a)
# define sinf(a)      sin (a)
# define cosf(a)      cos (a)
# define tanf(a)      tan (a)
# define asinf(a)     asin (a)
# define acosf(a)     acos (a)
# define atanf(a)     atan (a)
# define atan2f(a, b) atan2 (a, b)
# define sqrtf(a)     sqrt (a)
#endif

#if __GNUC__ >= 3 && defined(__ELF__)
# define slim_hidden_proto(name)	slim_hidden_proto1(name, INT_##name)
# define slim_hidden_def(name)		slim_hidden_def1(name, INT_##name)
# define slim_hidden_proto1(name, internal)				\
  extern __typeof (name) name						\
	__asm__ (slim_hidden_asmname (internal))			\
	__internal_linkage;
# define slim_hidden_def1(name, internal)				\
  extern __typeof (name) EXT_##name __asm__(slim_hidden_asmname(name))	\
	__attribute__((__alias__(slim_hidden_asmname(internal))))
# define slim_hidden_ulp		slim_hidden_ulp1(__USER_LABEL_PREFIX__)
# define slim_hidden_ulp1(x)		slim_hidden_ulp2(x)
# define slim_hidden_ulp2(x)		#x
# define slim_hidden_asmname(name)	slim_hidden_asmname1(name)
# define slim_hidden_asmname1(name)	slim_hidden_ulp #name
#else
# define slim_hidden_proto(name)
# define slim_hidden_def(name)
#endif

#if (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3)) && defined(__ELF__)
#define __internal_linkage	__attribute__((__visibility__("hidden")))
#else
#define __internal_linkage
#endif

#ifndef __GNUC__
#define __attribute__(x)
#endif

#define GLITZ_STATUS_NO_MEMORY_MASK          (1L << 0)
#define GLITZ_STATUS_BAD_COORDINATE_MASK     (1L << 1)
#define GLITZ_STATUS_NOT_SUPPORTED_MASK      (1L << 2)
#define GLITZ_STATUS_CONTENT_DESTROYED_MASK  (1L << 3)

#define GLITZ_DRAWABLE_FORMAT_ALL_EXCEPT_ID_MASK ((1L << 11) - 2)

#include "glitz_gl.h"

#define GLITZ_CONTEXT_STACK_SIZE 16

typedef struct _glitz_gl_proc_address_list_t {

  /* core */
  glitz_gl_enable_t                     enable;
  glitz_gl_disable_t                    disable;
  glitz_gl_get_error_t                  get_error;
  glitz_gl_get_string_t                 get_string;
  glitz_gl_enable_client_state_t        enable_client_state;
  glitz_gl_disable_client_state_t       disable_client_state;
  glitz_gl_vertex_pointer_t             vertex_pointer;
  glitz_gl_tex_coord_pointer_t          tex_coord_pointer;
  glitz_gl_draw_arrays_t                draw_arrays;
  glitz_gl_tex_env_f_t                  tex_env_f;
  glitz_gl_tex_env_fv_t                 tex_env_fv;
  glitz_gl_tex_gen_i_t                  tex_gen_i;
  glitz_gl_tex_gen_fv_t                 tex_gen_fv;
  glitz_gl_color_4us_t                  color_4us;
  glitz_gl_color_4f_t                   color_4f;
  glitz_gl_scissor_t                    scissor;
  glitz_gl_blend_func_t                 blend_func;
  glitz_gl_clear_t                      clear;
  glitz_gl_clear_color_t                clear_color;
  glitz_gl_clear_stencil_t              clear_stencil;
  glitz_gl_stencil_func_t               stencil_func;
  glitz_gl_stencil_op_t                 stencil_op;
  glitz_gl_push_attrib_t                push_attrib;
  glitz_gl_pop_attrib_t                 pop_attrib;
  glitz_gl_matrix_mode_t                matrix_mode;
  glitz_gl_push_matrix_t                push_matrix;
  glitz_gl_pop_matrix_t                 pop_matrix;
  glitz_gl_load_identity_t              load_identity;
  glitz_gl_load_matrix_f_t              load_matrix_f;
  glitz_gl_depth_range_t                depth_range;
  glitz_gl_viewport_t                   viewport;
  glitz_gl_raster_pos_2f_t              raster_pos_2f;
  glitz_gl_bitmap_t                     bitmap;
  glitz_gl_read_buffer_t                read_buffer;
  glitz_gl_draw_buffer_t                draw_buffer;
  glitz_gl_copy_pixels_t                copy_pixels;
  glitz_gl_flush_t                      flush;
  glitz_gl_finish_t                     finish;
  glitz_gl_pixel_store_i_t              pixel_store_i;
  glitz_gl_ortho_t                      ortho;
  glitz_gl_scale_f_t                    scale_f;
  glitz_gl_translate_f_t                translate_f;
  glitz_gl_hint_t                       hint;
  glitz_gl_depth_mask_t                 depth_mask;
  glitz_gl_polygon_mode_t               polygon_mode;
  glitz_gl_shade_model_t                shade_model;
  glitz_gl_color_mask_t                 color_mask;
  glitz_gl_read_pixels_t                read_pixels;
  glitz_gl_get_tex_image_t              get_tex_image;
  glitz_gl_tex_sub_image_2d_t           tex_sub_image_2d;
  glitz_gl_gen_textures_t               gen_textures;
  glitz_gl_delete_textures_t            delete_textures;
  glitz_gl_bind_texture_t               bind_texture;
  glitz_gl_tex_image_2d_t               tex_image_2d;
  glitz_gl_tex_parameter_i_t            tex_parameter_i;
  glitz_gl_tex_parameter_fv_t           tex_parameter_fv;
  glitz_gl_get_tex_level_parameter_iv_t get_tex_level_parameter_iv;
  glitz_gl_copy_tex_sub_image_2d_t      copy_tex_sub_image_2d;
  glitz_gl_get_integer_v_t              get_integer_v;

  /* extensions */
  glitz_gl_blend_color_t                blend_color;
  glitz_gl_active_texture_t             active_texture;
  glitz_gl_client_active_texture_t      client_active_texture;
  glitz_gl_multi_draw_arrays_t          multi_draw_arrays;
  glitz_gl_gen_programs_t               gen_programs;
  glitz_gl_delete_programs_t            delete_programs;
  glitz_gl_program_string_t             program_string;
  glitz_gl_bind_program_t               bind_program;
  glitz_gl_program_local_param_4fv_t    program_local_param_4fv;
  glitz_gl_get_program_iv_t             get_program_iv;
  glitz_gl_gen_buffers_t                gen_buffers;
  glitz_gl_delete_buffers_t             delete_buffers;
  glitz_gl_bind_buffer_t                bind_buffer;
  glitz_gl_buffer_data_t                buffer_data;
  glitz_gl_buffer_sub_data_t            buffer_sub_data;
  glitz_gl_get_buffer_sub_data_t        get_buffer_sub_data;
  glitz_gl_map_buffer_t                 map_buffer;
  glitz_gl_unmap_buffer_t               unmap_buffer;
  glitz_gl_gen_framebuffers_t           gen_framebuffers;
  glitz_gl_delete_framebuffers_t        delete_framebuffers;
  glitz_gl_bind_framebuffer_t           bind_framebuffer;
  glitz_gl_framebuffer_renderbuffer_t   framebuffer_renderbuffer;
  glitz_gl_framebuffer_texture_2d_t     framebuffer_texture_2d;
  glitz_gl_check_framebuffer_status_t   check_framebuffer_status;
  glitz_gl_gen_renderbuffers_t          gen_renderbuffers;
  glitz_gl_delete_renderbuffers_t       delete_renderbuffers;
  glitz_gl_bind_renderbuffer_t          bind_renderbuffer;
  glitz_gl_renderbuffer_storage_t       renderbuffer_storage;
  glitz_gl_get_renderbuffer_parameter_iv_t get_renderbuffer_parameter_iv;
} glitz_gl_proc_address_list_t;

typedef int glitz_surface_type_t;

#define GLITZ_SURFACE_TYPE_NA    -1
#define GLITZ_SURFACE_TYPE_NULL   0
#define GLITZ_SURFACE_TYPE_ARGB   1
#define GLITZ_SURFACE_TYPE_ARGBC  2
#define GLITZ_SURFACE_TYPE_ARGBF  3
#define GLITZ_SURFACE_TYPE_SOLID  4
#define GLITZ_SURFACE_TYPE_SOLIDC 5
#define GLITZ_SURFACE_TYPES       6

typedef int glitz_combine_type_t;

#define GLITZ_COMBINE_TYPE_NA             0
#define GLITZ_COMBINE_TYPE_ARGB           1
#define GLITZ_COMBINE_TYPE_ARGB_ARGB      2
#define GLITZ_COMBINE_TYPE_ARGB_ARGBC     3
#define GLITZ_COMBINE_TYPE_ARGB_ARGBF     4
#define GLITZ_COMBINE_TYPE_ARGB_SOLID     5
#define GLITZ_COMBINE_TYPE_ARGB_SOLIDC    6
#define GLITZ_COMBINE_TYPE_ARGBF          7
#define GLITZ_COMBINE_TYPE_ARGBF_ARGB     8
#define GLITZ_COMBINE_TYPE_ARGBF_ARGBC    9
#define GLITZ_COMBINE_TYPE_ARGBF_ARGBF   10
#define GLITZ_COMBINE_TYPE_ARGBF_SOLID   11
#define GLITZ_COMBINE_TYPE_ARGBF_SOLIDC  12
#define GLITZ_COMBINE_TYPE_SOLID         13
#define GLITZ_COMBINE_TYPE_SOLID_ARGB    14
#define GLITZ_COMBINE_TYPE_SOLID_ARGBC   15
#define GLITZ_COMBINE_TYPE_SOLID_ARGBF   16
#define GLITZ_COMBINE_TYPE_SOLID_SOLID   17
#define GLITZ_COMBINE_TYPE_SOLID_SOLIDC  18
#define GLITZ_COMBINE_TYPES              19

#define GLITZ_TEXTURE_NONE 0
#define GLITZ_TEXTURE_2D   1
#define GLITZ_TEXTURE_RECT 2
#define GLITZ_TEXTURE_LAST 3

#define GLITZ_FP_CONVOLUTION                 0
#define GLITZ_FP_LINEAR_GRADIENT_TRANSPARENT 1
#define GLITZ_FP_LINEAR_GRADIENT_NEAREST     2
#define GLITZ_FP_LINEAR_GRADIENT_REPEAT      3
#define GLITZ_FP_LINEAR_GRADIENT_REFLECT     4
#define GLITZ_FP_RADIAL_GRADIENT_TRANSPARENT 5
#define GLITZ_FP_RADIAL_GRADIENT_NEAREST     6
#define GLITZ_FP_RADIAL_GRADIENT_REPEAT      7
#define GLITZ_FP_RADIAL_GRADIENT_REFLECT     8
#define GLITZ_FP_TYPES                       9

typedef struct _glitz_program_t {
  glitz_gl_int_t *name;
  unsigned int   size;
} glitz_program_t;

typedef struct _glitz_filter_map_t {
  glitz_program_t fp[GLITZ_TEXTURE_LAST][GLITZ_TEXTURE_LAST];
} glitz_filter_map_t;

typedef struct _glitz_program_map_t {
  glitz_filter_map_t filters[GLITZ_COMBINE_TYPES][GLITZ_FP_TYPES];
} glitz_program_map_t;

typedef enum {
  GLITZ_NONE,
  GLITZ_ANY_CONTEXT_CURRENT,
  GLITZ_CONTEXT_CURRENT,
  GLITZ_DRAWABLE_CURRENT
} glitz_constraint_t;

typedef struct _glitz_region_t {
  glitz_box_t extents;
  glitz_box_t *box;
  int         n_box;
  void        *data;
  int         size;
} glitz_region_t;

#define NULL_BOX ((glitz_box_t *) 0)

#define REGION_INIT(region, __box) \
  { \
    if (__box) { \
      (region)->extents = *(__box); \
      (region)->box = &(region)->extents; \
      (region)->n_box = 1; \
    } else { \
      (region)->extents.x1 = 0; \
      (region)->extents.y1 = 0; \
      (region)->extents.x2 = 0; \
      (region)->extents.y2 = 0; \
      (region)->box = NULL; \
      (region)->n_box = 0; \
    } \
  }

#define REGION_EMPTY(region) \
  { \
    (region)->extents.x1 = 0; \
    (region)->extents.y1 = 0; \
    (region)->extents.x2 = 0; \
    (region)->extents.y2 = 0; \
    (region)->box = NULL; \
    (region)->n_box = 0; \
  }

#define REGION_UNINIT(region) \
  { \
    REGION_EMPTY (region); \
    if ((region)->data) \
      free ((region)->data); \
    (region)->data = NULL; \
    (region)->size = 0; \
  }

#define REGION_NOTEMPTY(region) \
  ((region)->n_box)

#define REGION_RECTS(region) \
  ((region)->box)

#define REGION_NUM_RECTS(region) \
  ((region)->n_box)

#define REGION_EXTENTS(region) \
 (&(region)->extents)

#define REGION_UNION(region, box) \
  glitz_region_union (region, box)

extern glitz_status_t __internal_linkage
glitz_region_union (glitz_region_t *region,
		    glitz_box_t    *box);

#define GLITZ_DRAWABLE_TYPE_WINDOW_MASK  (1L << 0)
#define GLITZ_DRAWABLE_TYPE_PBUFFER_MASK (1L << 1)
#define GLITZ_DRAWABLE_TYPE_FBO_MASK     (1L << 2)

#define GLITZ_INT_FORMAT_WINDOW_MASK  (1L << 17)
#define GLITZ_INT_FORMAT_PBUFFER_MASK (1L << 18)
#define GLITZ_INT_FORMAT_FBO_MASK     (1L << 19)

typedef struct _glitz_int_drawable_format_t {
    glitz_drawable_format_t d;
    unsigned int            types;
    int                     caveat;
    union {
	void	  *ptr;
	long	  val;
	unsigned long uval;
	void	  *(*fptr) (void);
    } u;
} glitz_int_drawable_format_t;

typedef struct glitz_backend {
  glitz_drawable_t *
  (*create_pbuffer)            (void                    *drawable,
				glitz_drawable_format_t *format,
				unsigned int            width,
				unsigned int            height);

  void
  (*destroy)                   (void *drawable);

  glitz_bool_t
  (*push_current)              (void               *drawable,
				glitz_surface_t    *surface,
				glitz_constraint_t constraint);

  glitz_surface_t *
  (*pop_current)               (void *drawable);

  void
  (*attach_notify)             (void            *drawable,
				glitz_surface_t *surface);

  void
  (*detach_notify)             (void            *drawable,
				glitz_surface_t *surface);

  glitz_bool_t
  (*swap_buffers)              (void *drawable);


  glitz_context_t *
  (*create_context)            (void                    *drawable,
				glitz_drawable_format_t *format);

  void
  (*destroy_context)           (void *context);

  void
  (*copy_context)              (void          *src,
				void          *dst,
				unsigned long mask);

  void
  (*make_current)              (void *drawable,
				void *context);

  glitz_function_pointer_t
  (*get_proc_address)          (void       *context,
				const char *name);

  glitz_gl_proc_address_list_t *gl;

  glitz_int_drawable_format_t  *drawable_formats;
  int                          n_drawable_formats;

  glitz_gl_int_t               *texture_formats;
  glitz_format_t               *formats;
  int                          n_formats;

  glitz_gl_float_t             gl_version;
  glitz_gl_int_t               max_viewport_dims[2];
  glitz_gl_int_t               max_texture_2d_size;
  glitz_gl_int_t               max_texture_rect_size;
  unsigned long                feature_mask;

  glitz_program_map_t          *program_map;
} glitz_backend_t;

struct _glitz_drawable {
  glitz_backend_t             *backend;
  int                         ref_count;
  glitz_int_drawable_format_t *format;
  int                         width, height;
  glitz_rectangle_t           viewport;
  glitz_bool_t                update_all;
  glitz_surface_t             *front;
  glitz_surface_t             *back;
};

#define GLITZ_GL_DRAWABLE(drawable) \
  glitz_gl_proc_address_list_t *gl = (drawable)->backend->gl;

#define DRAWABLE_IS_FBO(drawable) \
  ((drawable)->format->types == GLITZ_DRAWABLE_TYPE_FBO_MASK)

typedef struct _glitz_vec2_t {
  glitz_float_t v[2];
} glitz_vec2_t;

typedef struct _glitz_vec4_t {
  glitz_float_t v[4];
} glitz_vec4_t;

#define GLITZ_TEXTURE_FLAG_ALLOCATED_MASK    (1L <<  0)
#define GLITZ_TEXTURE_FLAG_REPEATABLE_MASK   (1L <<  1)
#define GLITZ_TEXTURE_FLAG_PADABLE_MASK      (1L <<  2)
#define GLITZ_TEXTURE_FLAG_INVALID_SIZE_MASK (1L <<  3)

#define TEXTURE_ALLOCATED(texture) \
  ((texture)->flags & GLITZ_TEXTURE_FLAG_ALLOCATED_MASK)

#define TEXTURE_REPEATABLE(texture) \
  ((texture)->flags & GLITZ_TEXTURE_FLAG_REPEATABLE_MASK)

#define TEXTURE_PADABLE(texture) \
  ((texture)->flags & GLITZ_TEXTURE_FLAG_PADABLE_MASK)

#define TEXTURE_INVALID_SIZE(texture) \
  ((texture)->flags & GLITZ_TEXTURE_FLAG_INVALID_SIZE_MASK)

typedef struct _glitz_texture_parameters {
    glitz_gl_enum_t filter[2];
    glitz_gl_enum_t wrap[2];
    glitz_color_t   border_color;
} glitz_texture_parameters_t;

typedef struct _glitz_texture {
  glitz_gl_uint_t name;
  glitz_gl_enum_t target;
  glitz_gl_int_t  format;
  unsigned long   flags;

  glitz_texture_parameters_t param;

  int width;
  int height;

  glitz_box_t box;

  glitz_float_t texcoord_width_unit;
  glitz_float_t texcoord_height_unit;
} glitz_texture_t;

struct _glitz_texture_object {
  glitz_surface_t            *surface;
  int                        ref_count;
  glitz_texture_parameters_t param;
};

struct _glitz_buffer {
  glitz_gl_uint_t  name;
  glitz_gl_enum_t  target;
  void             *data;
  int              owns_data;
  int              ref_count;
  glitz_surface_t  *front_surface;
  glitz_surface_t  *back_surface;
  glitz_drawable_t *drawable;
};

struct _glitz_multi_array {
  int ref_count;
  int size;
  int n_arrays;
  int *first;
  int *sizes;
  int *count;
  int *span, *current_span;
  glitz_vec2_t *off;
};

typedef struct _glitz_int_coordinate {
  glitz_gl_enum_t type;
  int             size, offset;
} glitz_int_coordinate_t;

typedef struct _glitz_vertex_info {
  glitz_gl_enum_t        prim;
  glitz_gl_enum_t        type;
  glitz_int_coordinate_t src;
  glitz_int_coordinate_t mask;
} glitz_vertex_info_t;

typedef struct _glitz_bitmap_info {
  glitz_bool_t     top_down;
  glitz_gl_int_t   pad;
  glitz_gl_ubyte_t *base;
} glitz_bitmap_info_t;

typedef struct _glitz_geometry {
  glitz_geometry_type_t type;
  glitz_buffer_t        *buffer;
  glitz_gl_sizei_t      stride;
  glitz_gl_float_t      data[8];
  glitz_gl_int_t        first;
  glitz_gl_int_t        size;
  glitz_gl_sizei_t      count;
  glitz_vec2_t          off;
  glitz_multi_array_t   *array;
  unsigned long         attributes;
  union {
    glitz_vertex_info_t v;
    glitz_bitmap_info_t b;
  } u;
} glitz_geometry_t;

#define GLITZ_SURFACE_FLAG_SOLID_MASK                   (1L <<  0)
#define GLITZ_SURFACE_FLAG_REPEAT_MASK                  (1L <<  1)
#define GLITZ_SURFACE_FLAG_MIRRORED_MASK                (1L <<  2)
#define GLITZ_SURFACE_FLAG_PAD_MASK                     (1L <<  3)
#define GLITZ_SURFACE_FLAG_COMPONENT_ALPHA_MASK         (1L <<  4)
#define GLITZ_SURFACE_FLAG_DITHER_MASK                  (1L <<  5)
#define GLITZ_SURFACE_FLAG_MULTISAMPLE_MASK             (1L <<  6)
#define GLITZ_SURFACE_FLAG_NICEST_MULTISAMPLE_MASK      (1L <<  7)
#define GLITZ_SURFACE_FLAG_SOLID_DAMAGE_MASK            (1L <<  8)
#define GLITZ_SURFACE_FLAG_FRAGMENT_FILTER_MASK         (1L <<  9)
#define GLITZ_SURFACE_FLAG_LINEAR_TRANSFORM_FILTER_MASK (1L << 10)
#define GLITZ_SURFACE_FLAG_IGNORE_WRAP_MASK             (1L << 11)
#define GLITZ_SURFACE_FLAG_EYE_COORDS_MASK              (1L << 12)
#define GLITZ_SURFACE_FLAG_TRANSFORM_MASK               (1L << 13)
#define GLITZ_SURFACE_FLAG_PROJECTIVE_TRANSFORM_MASK    (1L << 14)
#define GLITZ_SURFACE_FLAG_GEN_S_COORDS_MASK            (1L << 15)
#define GLITZ_SURFACE_FLAG_GEN_T_COORDS_MASK            (1L << 16)

#define GLITZ_SURFACE_FLAGS_GEN_COORDS_MASK  \
    (GLITZ_SURFACE_FLAG_GEN_S_COORDS_MASK | \
     GLITZ_SURFACE_FLAG_GEN_T_COORDS_MASK)

#define SURFACE_SOLID(surface) \
  ((surface)->flags & GLITZ_SURFACE_FLAG_SOLID_MASK)

#define SURFACE_REPEAT(surface) \
  (((surface)->flags & GLITZ_SURFACE_FLAG_REPEAT_MASK) && \
   (!((surface)->flags & GLITZ_SURFACE_FLAG_IGNORE_WRAP_MASK)))

#define SURFACE_MIRRORED(surface) \
  ((surface)->flags & GLITZ_SURFACE_FLAG_MIRRORED_MASK)

#define SURFACE_PAD(surface) \
  (((surface)->flags & GLITZ_SURFACE_FLAG_PAD_MASK) && \
   (!((surface)->flags & GLITZ_SURFACE_FLAG_IGNORE_WRAP_MASK)))

#define SURFACE_COMPONENT_ALPHA(surface) \
  ((surface)->flags & GLITZ_SURFACE_FLAG_COMPONENT_ALPHA_MASK)

#define SURFACE_DITHER(surface) \
  ((surface)->flags & GLITZ_SURFACE_FLAG_DITHER_MASK)

#define SURFACE_SOLID_DAMAGE(surface) \
  ((surface)->flags & GLITZ_SURFACE_FLAG_SOLID_DAMAGE_MASK)

#define SURFACE_FRAGMENT_FILTER(surface) \
  ((surface)->flags & GLITZ_SURFACE_FLAG_FRAGMENT_FILTER_MASK)

#define SURFACE_LINEAR_TRANSFORM_FILTER(surface) \
  ((surface)->flags & GLITZ_SURFACE_FLAG_LINEAR_TRANSFORM_FILTER_MASK)

#define SURFACE_EYE_COORDS(surface) \
  ((surface)->flags & GLITZ_SURFACE_FLAG_EYE_COORDS_MASK)

#define SURFACE_TRANSFORM(surface) \
  ((surface)->flags & GLITZ_SURFACE_FLAG_TRANSFORM_MASK)

#define SURFACE_PROJECTIVE_TRANSFORM(surface) \
  ((surface)->flags & GLITZ_SURFACE_FLAG_PROJECTIVE_TRANSFORM_MASK)

typedef struct _glitz_filter_params_t glitz_filter_params_t;

typedef struct _glitz_matrix {
  glitz_float_t t[16];
  glitz_float_t m[16];
} glitz_matrix_t;

#define GLITZ_DAMAGE_TEXTURE_MASK  (1 << 0)
#define GLITZ_DAMAGE_DRAWABLE_MASK (1 << 1)
#define GLITZ_DAMAGE_SOLID_MASK    (1 << 2)

struct _glitz_surface {
  int                   ref_count;
  glitz_format_t        *format;
  glitz_texture_t       texture;
  glitz_drawable_t      *drawable;
  glitz_drawable_t      *attached;
  unsigned long         status_mask;
  glitz_filter_t        filter;
  glitz_filter_params_t *filter_params;
  glitz_matrix_t        *transform;
  int                   x, y;
  glitz_box_t           box;
  short                 x_clip, y_clip;
  glitz_box_t           *clip;
  int                   n_clip;
  glitz_gl_enum_t       buffer;
  unsigned long         flags;
  glitz_color_t         solid;
  glitz_geometry_t      geometry;
  void                  *arrays;
  int                   n_arrays;
  int                   *first;
  unsigned int          *count;
  glitz_vec2_t          *off;
  int                   default_first;
  unsigned int          default_count;
  glitz_vec2_t          default_off;
  int                   *primcount;
  glitz_region_t        texture_damage;
  glitz_region_t        drawable_damage;
  unsigned int          flip_count;
  glitz_gl_int_t        fb;
};

#define GLITZ_GL_SURFACE(surface) \
  glitz_gl_proc_address_list_t *gl = (surface)->drawable->backend->gl;

struct _glitz_context {
  int                           ref_count;
  glitz_drawable_t              *drawable;
  void                          *closure;
  glitz_lose_current_function_t lose_current;
};

typedef struct _glitz_composite_op_t glitz_composite_op_t;

typedef void (*glitz_combine_function_t) (glitz_composite_op_t *);

typedef struct _glitz_combine_t {
  glitz_combine_type_t     type;
  glitz_combine_function_t enable;
  int                      texture_units;
  int                      source_shader;
} glitz_combine_t;

struct _glitz_composite_op_t {
  glitz_combine_type_t         type;
  glitz_combine_t              *combine;
  glitz_gl_proc_address_list_t *gl;
  glitz_operator_t             render_op;
  glitz_surface_t              *src;
  glitz_surface_t              *mask;
  glitz_surface_t              *dst;
  glitz_color_t                *solid;
  glitz_color_t                alpha_mask;
  int                          per_component;
  glitz_gl_uint_t              fp;
  int                          count;
};

typedef struct _glitz_extension_map {
  glitz_gl_float_t version;
  char             *name;
  int              mask;
} glitz_extension_map;

extern void __internal_linkage
glitz_set_operator (glitz_gl_proc_address_list_t *gl,
		    glitz_operator_t             op);

unsigned long
glitz_extensions_query (glitz_gl_float_t    version,
			const char          *extensions_string,
			glitz_extension_map *extensions_map);

typedef glitz_function_pointer_t (* glitz_get_proc_address_proc_t)
     (const char *name, void *closure);

void
glitz_backend_init (glitz_backend_t               *backend,
		    glitz_get_proc_address_proc_t get_proc_address,
		    void                          *closure);

extern unsigned int __internal_linkage
glitz_uint_to_power_of_two (unsigned int x);

extern void __internal_linkage
glitz_set_raster_pos (glitz_gl_proc_address_list_t *gl,
		      glitz_float_t                x,
		      glitz_float_t                y);

extern void __internal_linkage
glitz_clamp_value (glitz_float_t *value,
		   glitz_float_t min,
		   glitz_float_t max);

void
glitz_initiate_state (glitz_gl_proc_address_list_t *gl);

void
glitz_create_surface_formats (glitz_gl_proc_address_list_t *gl,
			      glitz_format_t               **formats,
			      glitz_gl_int_t               **texture_formats,
			      int                          *n_formats);

extern void __internal_linkage
_glitz_add_drawable_formats (glitz_gl_proc_address_list_t *gl,
			     unsigned long		  feature_mask,
			     glitz_int_drawable_format_t  **formats,
			     int                          *n_formats);

void
glitz_drawable_format_copy (const glitz_drawable_format_t *src,
			    glitz_drawable_format_t	  *dst,
			    unsigned long		  mask);

glitz_drawable_format_t *
glitz_drawable_format_find (glitz_int_drawable_format_t       *formats,
			    int                               n_formats,
			    unsigned long                     mask,
			    const glitz_int_drawable_format_t *templ,
			    int                               count);

void
glitz_texture_init (glitz_texture_t *texture,
		    int             width,
		    int             height,
		    glitz_gl_int_t  texture_format,
		    unsigned long   feature_mask,
		    glitz_bool_t    unnormalized);

void
glitz_texture_fini (glitz_gl_proc_address_list_t *gl,
		    glitz_texture_t              *texture);

void
glitz_texture_size_check (glitz_gl_proc_address_list_t *gl,
			  glitz_texture_t              *texture,
			  glitz_gl_int_t               max_2d,
			  glitz_gl_int_t               max_rect);

void
glitz_texture_allocate (glitz_gl_proc_address_list_t *gl,
			glitz_texture_t              *texture);

extern void __internal_linkage
glitz_texture_ensure_parameters (glitz_gl_proc_address_list_t *gl,
				 glitz_texture_t	      *texture,
				 glitz_texture_parameters_t   *param);

void
glitz_texture_bind (glitz_gl_proc_address_list_t *gl,
		    glitz_texture_t              *texture);

void
glitz_texture_unbind (glitz_gl_proc_address_list_t *gl,
		      glitz_texture_t              *texture);

void
glitz_texture_copy_drawable (glitz_gl_proc_address_list_t *gl,
			     glitz_texture_t              *texture,
			     glitz_drawable_t             *drawable,
			     int                          x_drawable,
			     int                          y_drawable,
			     int                          width,
			     int                          height,
			     int                          x_texture,
			     int                          y_texture);

void
glitz_texture_set_tex_gen (glitz_gl_proc_address_list_t *gl,
			   glitz_texture_t              *texture,
			   glitz_geometry_t             *geometry,
			   int                          x_src,
			   int                          y_src,
			   unsigned long                flags,
			   glitz_int_coordinate_t       *coord);

extern void __internal_linkage
_glitz_surface_sync_texture (glitz_surface_t *surface);

extern glitz_texture_t __internal_linkage *
glitz_surface_get_texture (glitz_surface_t *surface,
			   glitz_bool_t    allocate);

extern void __internal_linkage
glitz_surface_sync_solid (glitz_surface_t *surface);

extern glitz_bool_t __internal_linkage
glitz_surface_push_current (glitz_surface_t    *surface,
			    glitz_constraint_t constraint);

extern void __internal_linkage
glitz_surface_pop_current (glitz_surface_t *surface);

extern void __internal_linkage
glitz_surface_damage (glitz_surface_t *surface,
		      glitz_box_t     *box,
		      int             what);

extern void __internal_linkage
glitz_surface_sync_drawable (glitz_surface_t *surface);

extern void __internal_linkage
glitz_surface_status_add (glitz_surface_t *surface,
			  int             flags);

extern unsigned long __internal_linkage
glitz_status_to_status_mask (glitz_status_t status);

extern glitz_status_t __internal_linkage
glitz_status_pop_from_mask (unsigned long *mask);

void
glitz_program_map_init (glitz_program_map_t *map);

void
glitz_program_map_fini (glitz_gl_proc_address_list_t *gl,
			glitz_program_map_t          *map);

extern glitz_gl_uint_t __internal_linkage
glitz_get_fragment_program (glitz_composite_op_t *op,
			    int                  fp_type,
			    int                  id);

extern void __internal_linkage
glitz_composite_op_init (glitz_composite_op_t *op,
			 glitz_operator_t     render_op,
			 glitz_surface_t      *src,
			 glitz_surface_t      *mask,
			 glitz_surface_t      *dst);

extern void __internal_linkage
glitz_composite_enable (glitz_composite_op_t *op);

extern void __internal_linkage
glitz_composite_disable (glitz_composite_op_t *op);

extern void __internal_linkage *
glitz_buffer_bind (glitz_buffer_t  *buffer,
		   glitz_gl_enum_t target);

extern void __internal_linkage
glitz_buffer_unbind (glitz_buffer_t *buffer);

extern glitz_status_t __internal_linkage
glitz_filter_set_params (glitz_surface_t    *surface,
			 glitz_filter_t     filter,
			 glitz_fixed16_16_t *params,
			 int                n_params);

extern void __internal_linkage
glitz_filter_set_type (glitz_surface_t *surface,
		       glitz_filter_t  filter);

extern glitz_gl_uint_t __internal_linkage
glitz_filter_get_vertex_program (glitz_surface_t      *surface,
				 glitz_composite_op_t *op);

extern glitz_gl_uint_t __internal_linkage
glitz_filter_get_fragment_program (glitz_surface_t      *surface,
				   glitz_composite_op_t *op);

extern void __internal_linkage
glitz_filter_enable (glitz_surface_t      *surface,
		     glitz_composite_op_t *op);

extern void __internal_linkage
glitz_geometry_enable_none (glitz_gl_proc_address_list_t *gl,
			    glitz_surface_t              *dst,
			    glitz_box_t                  *box);

extern void __internal_linkage
glitz_geometry_enable (glitz_gl_proc_address_list_t *gl,
		       glitz_surface_t              *dst,
		       glitz_box_t                  *box);

extern void __internal_linkage
glitz_geometry_disable (glitz_surface_t *dst);

extern void __internal_linkage
glitz_geometry_draw_arrays (glitz_gl_proc_address_list_t *gl,
			    glitz_surface_t              *dst,
			    glitz_geometry_type_t        type,
			    glitz_box_t                  *bounds,
			    int                          damage);

void
_glitz_drawable_init (glitz_drawable_t	          *drawable,
		      glitz_int_drawable_format_t *format,
		      glitz_backend_t	          *backend,
		      int		          width,
		      int		          height);

extern glitz_drawable_t __internal_linkage *
_glitz_fbo_drawable_create (glitz_drawable_t	        *other,
			    glitz_int_drawable_format_t *format,
			    int	                        width,
			    int	                        height);

void
_glitz_context_init (glitz_context_t  *context,
		     glitz_drawable_t *drawable);

void
_glitz_context_fini (glitz_context_t *context);


#define MAXSHORT SHRT_MAX
#define MINSHORT SHRT_MIN

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#define LSBFirst 0
#define MSBFirst 1

#ifdef WORDS_BIGENDIAN
#  define IMAGE_BYTE_ORDER MSBFirst
#  define BITMAP_BIT_ORDER MSBFirst
#else
#  define IMAGE_BYTE_ORDER LSBFirst
#  define BITMAP_BIT_ORDER LSBFirst
#endif

#define GLITZ_PI 3.14159265358979323846f

/* Fixed point updates from Carl Worth, USC, Information Sciences Institute */

#ifdef _MSC_VER
typedef __int64 glitz_fixed_32_32;
#else
#  if defined(__alpha__) || defined(__alpha) || \
      defined(ia64) || defined(__ia64__) || \
      defined(__sparc64__) || \
      defined(__s390x__) || \
      defined(x86_64) || defined (__x86_64__)
typedef long glitz_fixed_32_32;
# else
#  if defined(__GNUC__) && \
    ((__GNUC__ > 2) || \
     ((__GNUC__ == 2) && defined(__GNUC_MINOR__) && (__GNUC_MINOR__ > 7)))
__extension__
#  endif
typedef long long int glitz_fixed_32_32;
# endif
#endif

typedef uint32_t glitz_fixed_1_31;
typedef uint32_t glitz_fixed_1_16;
typedef int32_t glitz_fixed_16_16;

/*
 * An unadorned "glitz_fixed" is the same as glitz_fixed_16_16,
 * (since it's quite common in the code)
 */
typedef glitz_fixed_16_16 glitz_fixed;

#define FIXED_BITS 16

#define FIXED_TO_INT(f) (int) ((f) >> FIXED_BITS)
#define INT_TO_FIXED(i) ((glitz_fixed) (i) << FIXED_BITS)
#define FIXED_E ((glitz_fixed) 1)
#define FIXED1 (INT_TO_FIXED (1))
#define FIXED1_MINUS_E (FIXED1 - FIXED_E)
#define FIXED_FRAC(f) ((f) & FIXED1_MINUS_E)
#define FIXED_FLOOR(f) ((f) & ~FIXED1_MINUS_E)
#define FIXED_CEIL(f) FIXED_FLOOR ((f) + FIXED1_MINUS_E)

#define FIXED_FRACTION(f) ((f) & FIXED1_MINUS_E)
#define FIXED_MOD2(f) ((f) & (FIXED1 | FIXED1_MINUS_E))

#define FIXED_TO_FLOAT(f) (((glitz_float_t) (f)) / 65536)
#define FLOAT_TO_FIXED(f) ((int) ((f) * 65536))

#define SHORT_MULT(s1, s2) \
  ((s1 == 0xffff)? s2: ((s2 == 0xffff)? s1: \
  ((unsigned short) (((unsigned int) s1 * s2) / 0xffff))))

#define POWER_OF_TWO(v) ((v & (v - 1)) == 0)

/*
 * VC's math.h is pretty horrible
 */
#ifdef _MSC_VER
#define ceilf(_X)  ((float)ceil((double)(_X)))
#define sqrtf(_X)  ((float)sqrt((double)(_X)))
#define floorf(_X)  ((float)floor((double)(_X)))
#define sinf(_X) ((float)sin((double)(_X)))
#define cosf(_X) ((float)cos((double)(_X)))
#define atan2f(_X,_Y) ((float)atan2((double)(_X),(double)(_Y)))
#define sqrtf(_X) ((float)sqrt((double)(_X)))
#endif

/* Avoid unnecessary PLT entries. */

slim_hidden_proto(glitz_find_drawable_format)
slim_hidden_proto(glitz_find_pbuffer_format)
slim_hidden_proto(glitz_create_drawable)
slim_hidden_proto(glitz_create_pbuffer_drawable)
slim_hidden_proto(glitz_drawable_get_width)
slim_hidden_proto(glitz_drawable_get_height)
slim_hidden_proto(glitz_drawable_swap_buffers)
slim_hidden_proto(glitz_drawable_flush)
slim_hidden_proto(glitz_drawable_finish)
slim_hidden_proto(glitz_drawable_get_features)
slim_hidden_proto(glitz_drawable_get_format)
slim_hidden_proto(glitz_surface_set_transform)
slim_hidden_proto(glitz_surface_set_fill)
slim_hidden_proto(glitz_surface_set_component_alpha)
slim_hidden_proto(glitz_surface_set_dither)
slim_hidden_proto(glitz_surface_set_filter)
slim_hidden_proto(glitz_surface_get_width)
slim_hidden_proto(glitz_surface_get_height)
slim_hidden_proto(glitz_surface_flush)
slim_hidden_proto(glitz_surface_get_status)
slim_hidden_proto(glitz_surface_get_format)
slim_hidden_proto(glitz_surface_get_drawable)
slim_hidden_proto(glitz_surface_get_attached_drawable)
slim_hidden_proto(glitz_surface_translate_point)
slim_hidden_proto(glitz_surface_set_clip_region)
slim_hidden_proto(glitz_set_rectangle)
slim_hidden_proto(glitz_set_rectangles)
slim_hidden_proto(glitz_set_geometry)
slim_hidden_proto(glitz_set_array)
slim_hidden_proto(glitz_multi_array_create)
slim_hidden_proto(glitz_multi_array_add)
slim_hidden_proto(glitz_multi_array_reset)
slim_hidden_proto(glitz_set_multi_array)
slim_hidden_proto(glitz_buffer_set_data)
slim_hidden_proto(glitz_buffer_get_data)
slim_hidden_proto(glitz_context_create)
slim_hidden_proto(glitz_context_destroy)
slim_hidden_proto(glitz_context_reference)
slim_hidden_proto(glitz_context_copy)
slim_hidden_proto(glitz_context_set_user_data)
slim_hidden_proto(glitz_context_get_proc_address)
slim_hidden_proto(glitz_context_make_current)
slim_hidden_proto(glitz_context_bind_texture)

#endif /* GLITZINT_H_INCLUDED */
