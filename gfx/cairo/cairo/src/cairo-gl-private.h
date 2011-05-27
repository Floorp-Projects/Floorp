/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 Eric Anholt
 * Copyright © 2009 Chris Wilson
 * Copyright © 2005,2010 Red Hat, Inc
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
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA
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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *	Benjamin Otte <otte@gnome.org>
 *	Carl Worth <cworth@cworth.org>
 *	Chris Wilson <chris@chris-wilson.co.uk>
 *	Eric Anholt <eric@anholt.net>
 *	T. Zachary Laine <whatwasthataddress@gmail.com>
 */

#ifndef CAIRO_GL_PRIVATE_H
#define CAIRO_GL_PRIVATE_H

#include "cairoint.h"

#include "cairo-gl-gradient-private.h"

#include "cairo-device-private.h"
#include "cairo-error-private.h"
#include "cairo-rtree-private.h"

#include <assert.h>

#include <GL/glew.h>

#include "cairo-gl.h"

#include <GL/gl.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/glext.h>

#define DEBUG_GL 0

#if DEBUG_GL && __GNUC__
#define UNSUPPORTED(reason) ({ \
    fprintf (stderr, \
	     "cairo-gl: hit unsupported operation in %s(), line %d: %s\n", \
	     __FUNCTION__, __LINE__, reason); \
    CAIRO_INT_STATUS_UNSUPPORTED; \
})
#else
#define UNSUPPORTED(reason) CAIRO_INT_STATUS_UNSUPPORTED
#endif

/* maximal number of shaders we keep in the cache.
 * Random number that is hopefully big enough to not cause many cache evictions. */
#define CAIRO_GL_MAX_SHADERS_PER_CONTEXT 64

/* VBO size that we allocate, smaller size means we gotta flush more often */
#define CAIRO_GL_VBO_SIZE 16384

typedef struct _cairo_gl_surface {
    cairo_surface_t base;

    int width, height;

    GLuint tex; /* GL texture object containing our data. */
    GLuint fb; /* GL framebuffer object wrapping our data. */
    GLuint depth; /* GL framebuffer object holding depth */
    int owns_tex;
} cairo_gl_surface_t;

typedef struct cairo_gl_glyph_cache {
    cairo_rtree_t rtree;
    cairo_surface_pattern_t pattern;
} cairo_gl_glyph_cache_t;

typedef enum cairo_gl_tex {
    CAIRO_GL_TEX_SOURCE = 0,
    CAIRO_GL_TEX_MASK = 1,
    CAIRO_GL_TEX_TEMP = 2
} cairo_gl_tex_t;

typedef enum cairo_gl_operand_type {
    CAIRO_GL_OPERAND_NONE,
    CAIRO_GL_OPERAND_CONSTANT,
    CAIRO_GL_OPERAND_TEXTURE,
    CAIRO_GL_OPERAND_LINEAR_GRADIENT,
    CAIRO_GL_OPERAND_RADIAL_GRADIENT,
    CAIRO_GL_OPERAND_SPANS,

    CAIRO_GL_OPERAND_COUNT
} cairo_gl_operand_type_t;

typedef struct cairo_gl_shader_impl cairo_gl_shader_impl_t;

typedef struct cairo_gl_shader {
    GLuint fragment_shader;
    GLuint program;
} cairo_gl_shader_t;

typedef enum cairo_gl_shader_in {
    CAIRO_GL_SHADER_IN_NORMAL,
    CAIRO_GL_SHADER_IN_CA_SOURCE,
    CAIRO_GL_SHADER_IN_CA_SOURCE_ALPHA,

    CAIRO_GL_SHADER_IN_COUNT
} cairo_gl_shader_in_t;

typedef enum cairo_gl_var_type {
  CAIRO_GL_VAR_NONE,
  CAIRO_GL_VAR_TEXCOORDS,
  CAIRO_GL_VAR_COVERAGE
} cairo_gl_var_type_t;

#define cairo_gl_var_type_hash(src,mask,dest) ((mask) << 2 | (src << 1) | (dest))
#define CAIRO_GL_VAR_TYPE_MAX ((CAIRO_GL_VAR_COVERAGE << 2) | (CAIRO_GL_VAR_TEXCOORDS << 1) | CAIRO_GL_VAR_TEXCOORDS)

/* This union structure describes a potential source or mask operand to the
 * compositing equation.
 */
typedef struct cairo_gl_operand {
    cairo_gl_operand_type_t type;
    union {
	struct {
	    GLuint tex;
	    cairo_gl_surface_t *surface;
	    cairo_surface_attributes_t attributes;
	} texture;
	struct {
	    GLfloat color[4];
	} constant;
	struct {
	    cairo_gl_gradient_t *gradient;
	    cairo_matrix_t m;
	    float segment_x;
	    float segment_y;
            cairo_extend_t extend;
	} linear;
	struct {
	    cairo_gl_gradient_t *gradient;
	    cairo_matrix_t m;
	    float circle_1_x;
	    float circle_1_y;
	    float radius_0;
	    float radius_1;
            cairo_extend_t extend;
	} radial;
    };
    unsigned int vertex_offset;
} cairo_gl_operand_t;

struct _cairo_gl_context {
    cairo_device_t base;

    GLuint dummy_tex;
    GLuint texture_load_pbo;
    GLuint vbo;
    GLint max_framebuffer_size;
    GLint max_texture_size;
    GLint max_textures;
    GLenum tex_target;

    const cairo_gl_shader_impl_t *shader_impl;

    GLuint vertex_shaders[CAIRO_GL_VAR_TYPE_MAX + 1];
    cairo_gl_shader_t fill_rectangles_shader;
    cairo_cache_t shaders;

    cairo_cache_t gradients;

    cairo_gl_glyph_cache_t glyph_cache[2];
    cairo_list_t fonts;

    cairo_gl_surface_t *current_target;
    cairo_operator_t current_operator;
    cairo_gl_shader_t *pre_shader; /* for component alpha */
    cairo_gl_shader_t *current_shader;

    cairo_gl_operand_t operands[2];

    char *vb;
    unsigned int vb_offset;
    unsigned int vertex_size;
    cairo_region_t *clip_region;

    void (*acquire) (void *ctx);
    void (*release) (void *ctx);

    void (*make_current) (void *ctx, cairo_gl_surface_t *surface);
    void (*swap_buffers)(void *ctx, cairo_gl_surface_t *surface);
    void (*destroy) (void *ctx);
};

typedef struct _cairo_gl_composite {
    cairo_gl_surface_t *dst;
    cairo_operator_t op;
    cairo_region_t *clip_region;

    cairo_gl_operand_t src;
    cairo_gl_operand_t mask;
} cairo_gl_composite_t;

cairo_private extern const cairo_surface_backend_t _cairo_gl_surface_backend;

static cairo_always_inline GLenum
_cairo_gl_get_error (void)
{
    GLenum err = glGetError();

    if (unlikely (err))
        while (glGetError ());

    return err;
}

static inline cairo_device_t *
_cairo_gl_context_create_in_error (cairo_status_t status)
{
    return (cairo_device_t *) _cairo_device_create_in_error (status);
}

cairo_private cairo_status_t
_cairo_gl_context_init (cairo_gl_context_t *ctx);

cairo_private void
_cairo_gl_surface_init (cairo_device_t *device,
			cairo_gl_surface_t *surface,
			cairo_content_t content,
			int width, int height);

static cairo_always_inline cairo_bool_t cairo_warn
_cairo_gl_surface_is_texture (cairo_gl_surface_t *surface)
{
    return surface->tex != 0;
}

cairo_private cairo_status_t
_cairo_gl_surface_draw_image (cairo_gl_surface_t *dst,
			      cairo_image_surface_t *src,
			      int src_x, int src_y,
			      int width, int height,
			      int dst_x, int dst_y);

static cairo_always_inline cairo_bool_t
_cairo_gl_device_has_glsl (cairo_device_t *device)
{
    return ((cairo_gl_context_t *) device)->shader_impl != NULL;
}

static cairo_always_inline cairo_bool_t
_cairo_gl_device_requires_power_of_two_textures (cairo_device_t *device)
{
    return ((cairo_gl_context_t *) device)->tex_target == GL_TEXTURE_RECTANGLE_EXT;
}

static cairo_always_inline cairo_status_t cairo_warn
_cairo_gl_context_acquire (cairo_device_t *device,
			   cairo_gl_context_t **ctx)
{
    cairo_status_t status;

    status = cairo_device_acquire (device);
    if (unlikely (status))
	return status;

    /* clear potential previous GL errors */
    _cairo_gl_get_error ();

    *ctx = (cairo_gl_context_t *) device;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_always_inline cairo_warn cairo_status_t
_cairo_gl_context_release (cairo_gl_context_t *ctx, cairo_status_t status)
{
    GLenum err;

    err = _cairo_gl_get_error ();

    if (unlikely (err)) {
        cairo_status_t new_status;
	new_status = _cairo_error (CAIRO_STATUS_DEVICE_ERROR);
        if (status == CAIRO_STATUS_SUCCESS)
            status = new_status;
    }

    cairo_device_release (&(ctx)->base);

    return status;
}

cairo_private void
_cairo_gl_context_set_destination (cairo_gl_context_t *ctx, cairo_gl_surface_t *surface);

cairo_private void
_cairo_gl_context_activate (cairo_gl_context_t *ctx,
                            cairo_gl_tex_t      tex_unit);

cairo_private cairo_bool_t
_cairo_gl_operator_is_supported (cairo_operator_t op);

cairo_private cairo_status_t
_cairo_gl_composite_init (cairo_gl_composite_t *setup,
                          cairo_operator_t op,
                          cairo_gl_surface_t *dst,
                          cairo_bool_t has_component_alpha,
                          const cairo_rectangle_int_t *rect);

cairo_private void
_cairo_gl_composite_fini (cairo_gl_composite_t *setup);

cairo_private void
_cairo_gl_composite_set_clip_region (cairo_gl_composite_t *setup,
                                     cairo_region_t *clip_region);

cairo_private cairo_int_status_t
_cairo_gl_composite_set_source (cairo_gl_composite_t *setup,
			        const cairo_pattern_t *pattern,
                                int src_x, int src_y,
                                int dst_x, int dst_y,
                                int width, int height);

cairo_private cairo_int_status_t
_cairo_gl_composite_set_mask (cairo_gl_composite_t *setup,
			      const cairo_pattern_t *pattern,
                              int src_x, int src_y,
                              int dst_x, int dst_y,
                              int width, int height);

cairo_private void
_cairo_gl_composite_set_mask_spans (cairo_gl_composite_t *setup);

cairo_private cairo_status_t
_cairo_gl_composite_begin (cairo_gl_composite_t *setup,
                           cairo_gl_context_t **ctx);

cairo_private void
_cairo_gl_composite_emit_rect (cairo_gl_context_t *ctx,
                               GLfloat x1,
                               GLfloat y1,
                               GLfloat x2,
                               GLfloat y2,
                               uint8_t alpha);

cairo_private void
_cairo_gl_composite_emit_glyph (cairo_gl_context_t *ctx,
                                GLfloat x1,
                                GLfloat y1,
                                GLfloat x2,
                                GLfloat y2,
                                GLfloat glyph_x1,
                                GLfloat glyph_y1,
                                GLfloat glyph_x2,
                                GLfloat glyph_y2);

cairo_private void
_cairo_gl_composite_flush (cairo_gl_context_t *ctx);

cairo_private void
_cairo_gl_context_destroy_operand (cairo_gl_context_t *ctx,
                                   cairo_gl_tex_t tex_unit);

cairo_private cairo_bool_t
_cairo_gl_get_image_format_and_type (pixman_format_code_t pixman_format,
				     GLenum *internal_format, GLenum *format,
				     GLenum *type, cairo_bool_t *has_alpha);

cairo_private void
_cairo_gl_surface_scaled_font_fini ( cairo_scaled_font_t  *scaled_font);

cairo_private void
_cairo_gl_surface_scaled_glyph_fini (cairo_scaled_glyph_t *scaled_glyph,
				     cairo_scaled_font_t  *scaled_font);

cairo_private void
_cairo_gl_glyph_cache_init (cairo_gl_glyph_cache_t *cache);

cairo_private void
_cairo_gl_glyph_cache_fini (cairo_gl_context_t *ctx,
			    cairo_gl_glyph_cache_t *cache);

cairo_private cairo_int_status_t
_cairo_gl_surface_show_glyphs (void			*abstract_dst,
			       cairo_operator_t		 op,
			       const cairo_pattern_t	*source,
			       cairo_glyph_t		*glyphs,
			       int			 num_glyphs,
			       cairo_scaled_font_t	*scaled_font,
			       cairo_clip_t		*clip,
			       int			*remaining_glyphs);

static inline int
_cairo_gl_y_flip (cairo_gl_surface_t *surface, int y)
{
    if (surface->fb)
	return y;
    else
	return (surface->height - 1) - y;
}

cairo_private cairo_status_t
_cairo_gl_context_init_shaders (cairo_gl_context_t *ctx);

cairo_private void
_cairo_gl_context_fini_shaders (cairo_gl_context_t *ctx);

static cairo_always_inline cairo_bool_t
_cairo_gl_context_is_flushed (cairo_gl_context_t *ctx)
{
    return ctx->vb == NULL;
}

cairo_private cairo_status_t
_cairo_gl_get_shader_by_type (cairo_gl_context_t *ctx,
                              cairo_gl_operand_type_t source,
                              cairo_gl_operand_type_t mask,
                              cairo_gl_shader_in_t in,
                              cairo_gl_shader_t **shader);

cairo_private void
_cairo_gl_shader_bind_float (cairo_gl_context_t *ctx,
			     const char *name,
			     float value);

cairo_private void
_cairo_gl_shader_bind_vec2 (cairo_gl_context_t *ctx,
			    const char *name,
			    float value0, float value1);

cairo_private void
_cairo_gl_shader_bind_vec3 (cairo_gl_context_t *ctx,
			    const char *name,
			    float value0,
			    float value1,
			    float value2);

cairo_private void
_cairo_gl_shader_bind_vec4 (cairo_gl_context_t *ctx,
			    const char *name,
			    float value0, float value1,
			    float value2, float value3);

cairo_private void
_cairo_gl_shader_bind_matrix (cairo_gl_context_t *ctx,
			      const char *name,
			      cairo_matrix_t* m);

cairo_private void
_cairo_gl_shader_bind_texture (cairo_gl_context_t *ctx,
			       const char *name,
			       GLuint tex_unit);

cairo_private void
_cairo_gl_set_shader (cairo_gl_context_t *ctx,
		      cairo_gl_shader_t *shader);

cairo_private void
_cairo_gl_shader_fini (cairo_gl_context_t *ctx, cairo_gl_shader_t *shader);

slim_hidden_proto (cairo_gl_surface_create);
slim_hidden_proto (cairo_gl_surface_create_for_texture);

#endif /* CAIRO_GL_PRIVATE_H */
