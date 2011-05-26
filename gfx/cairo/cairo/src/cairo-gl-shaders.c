/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 T. Zachary Laine
 * Copyright © 2010 Eric Anholt
 * Copyright © 2010 Red Hat, Inc
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
 * The Initial Developer of the Original Code is T. Zachary Laine.
 *
 * Contributor(s):
 *	Benjamin Otte <otte@gnome.org>
 *	Eric Anholt <eric@anholt.net>
 *	T. Zachary Laine <whatwasthataddress@gmail.com>
 */

#include "cairoint.h"
#include "cairo-gl-private.h"
#include "cairo-error-private.h"
#include "cairo-output-stream-private.h"

typedef struct cairo_gl_shader_impl {
    void
    (*compile_shader) (GLuint *shader, GLenum type, const char *text);

    void
    (*link_shader) (GLuint *program, GLuint vert, GLuint frag);

    void
    (*destroy_shader) (GLuint shader);

    void
    (*destroy_program) (GLuint program);

    void
    (*bind_float) (cairo_gl_shader_t *shader,
		   const char *name,
		   float value);

    void
    (*bind_vec2) (cairo_gl_shader_t *shader,
		  const char *name,
		  float value0,
		  float value1);

    void
    (*bind_vec3) (cairo_gl_shader_t *shader,
		  const char *name,
		  float value0,
		  float value1,
		  float value2);

    void
    (*bind_vec4) (cairo_gl_shader_t *shader,
		  const char *name,
		  float value0, float value1,
		  float value2, float value3);

    void
    (*bind_matrix) (cairo_gl_shader_t *shader,
		    const char *name,
		    cairo_matrix_t* m);

    void
    (*bind_texture) (cairo_gl_shader_t *shader,
		     const char *name,
		     cairo_gl_tex_t tex_unit);

    void
    (*use) (cairo_gl_shader_t *shader);
} shader_impl_t;

static cairo_status_t
_cairo_gl_shader_compile (cairo_gl_context_t *ctx,
			  cairo_gl_shader_t *shader,
			  cairo_gl_var_type_t src,
			  cairo_gl_var_type_t mask,
			  const char *fragment_text);

/* ARB_shader_objects / ARB_vertex_shader / ARB_fragment_shader extensions
   API. */
static void
compile_shader_arb (GLuint *shader, GLenum type, const char *text)
{
    const char* strings[1] = { text };
    GLint gl_status;

    *shader = glCreateShaderObjectARB (type);
    glShaderSourceARB (*shader, 1, strings, 0);
    glCompileShaderARB (*shader);
    glGetObjectParameterivARB (*shader, GL_OBJECT_COMPILE_STATUS_ARB, &gl_status);
    if (gl_status == GL_FALSE) {
        GLint log_size;
        glGetObjectParameterivARB (*shader, GL_OBJECT_INFO_LOG_LENGTH_ARB, &log_size);
        if (0 < log_size) {
            char *log = _cairo_malloc (log_size);
            GLint chars;

            log[log_size - 1] = '\0';
            glGetInfoLogARB (*shader, log_size, &chars, log);
            printf ("OpenGL shader compilation failed.  Shader:\n"
                    "%s\n"
                    "OpenGL compilation log:\n"
                    "%s\n",
                    text, log);

            free (log);
        } else {
            printf ("OpenGL shader compilation failed.\n");
        }

	ASSERT_NOT_REACHED;
    }
}

static void
link_shader_arb (GLuint *program, GLuint vert, GLuint frag)
{
    GLint gl_status;

    *program = glCreateProgramObjectARB ();
    glAttachObjectARB (*program, vert);
    glAttachObjectARB (*program, frag);
    glLinkProgramARB (*program);
    glGetObjectParameterivARB (*program, GL_OBJECT_LINK_STATUS_ARB, &gl_status);
    if (gl_status == GL_FALSE) {
        GLint log_size;
        glGetObjectParameterivARB (*program, GL_OBJECT_INFO_LOG_LENGTH_ARB, &log_size);
        if (0 < log_size) {
            char *log = _cairo_malloc (log_size);
            GLint chars;

            log[log_size - 1] = '\0';
            glGetInfoLogARB (*program, log_size, &chars, log);
            printf ("OpenGL shader link failed:\n%s\n", log);

            free (log);
        } else {
            printf ("OpenGL shader link failed.\n");
        }

	ASSERT_NOT_REACHED;
    }
}

static void
destroy_shader_arb (GLuint shader)
{
  glDeleteObjectARB (shader);
}

static void
destroy_program_arb (GLuint shader)
{
  glDeleteObjectARB (shader);
}

static void
bind_float_arb (cairo_gl_shader_t *shader,
		const char *name,
		float value)
{
    GLint location = glGetUniformLocationARB (shader->program, name);
    assert (location != -1);
    glUniform1fARB (location, value);
}

static void
bind_vec2_arb (cairo_gl_shader_t *shader,
	       const char *name,
	       float value0,
	       float value1)
{
    GLint location = glGetUniformLocationARB (shader->program, name);
    assert (location != -1);
    glUniform2fARB (location, value0, value1);
}

static void
bind_vec3_arb (cairo_gl_shader_t *shader,
	       const char *name,
	       float value0,
	       float value1,
	       float value2)
{
    GLint location = glGetUniformLocationARB (shader->program, name);
    assert (location != -1);
    glUniform3fARB (location, value0, value1, value2);
}

static void
bind_vec4_arb (cairo_gl_shader_t *shader,
	       const char *name,
	       float value0,
	       float value1,
	       float value2,
	       float value3)
{
    GLint location = glGetUniformLocationARB (shader->program, name);
    assert (location != -1);
    glUniform4fARB (location, value0, value1, value2, value3);
}

static void
bind_matrix_arb (cairo_gl_shader_t *shader,
		 const char *name,
		 cairo_matrix_t* m)
{
    GLint location = glGetUniformLocationARB (shader->program, name);
    float gl_m[9] = {
        m->xx, m->xy, m->x0,
        m->yx, m->yy, m->y0,
        0,     0,     1
    };
    assert (location != -1);
    glUniformMatrix3fvARB (location, 1, GL_TRUE, gl_m);
}

static void
bind_texture_arb (cairo_gl_shader_t *shader,
		  const char *name,
		  cairo_gl_tex_t tex_unit)
{
    GLint location = glGetUniformLocationARB (shader->program, name);
    assert (location != -1);
    glUniform1iARB (location, tex_unit);
}

static void
use_program_arb (cairo_gl_shader_t *shader)
{
    if (shader)
	glUseProgramObjectARB (shader->program);
    else
	glUseProgramObjectARB (0);
}

/* OpenGL Core 2.0 API. */
static void
compile_shader_core_2_0 (GLuint *shader, GLenum type, const char *text)
{
    const char* strings[1] = { text };
    GLint gl_status;

    *shader = glCreateShader (type);
    glShaderSource (*shader, 1, strings, 0);
    glCompileShader (*shader);
    glGetShaderiv (*shader, GL_COMPILE_STATUS, &gl_status);
    if (gl_status == GL_FALSE) {
        GLint log_size;
        glGetShaderiv (*shader, GL_INFO_LOG_LENGTH, &log_size);
        if (0 < log_size) {
            char *log = _cairo_malloc (log_size);
            GLint chars;

            log[log_size - 1] = '\0';
            glGetShaderInfoLog (*shader, log_size, &chars, log);
            printf ("OpenGL shader compilation failed.  Shader:\n"
                    "%s\n"
                    "OpenGL compilation log:\n"
                    "%s\n",
                    text, log);

            free (log);
        } else {
            printf ("OpenGL shader compilation failed.\n");
        }

	ASSERT_NOT_REACHED;
    }
}

static void
link_shader_core_2_0 (GLuint *program, GLuint vert, GLuint frag)
{
    GLint gl_status;

    *program = glCreateProgram ();
    glAttachShader (*program, vert);
    glAttachShader (*program, frag);
    glLinkProgram (*program);
    glGetProgramiv (*program, GL_LINK_STATUS, &gl_status);
    if (gl_status == GL_FALSE) {
        GLint log_size;
        glGetProgramiv (*program, GL_INFO_LOG_LENGTH, &log_size);
        if (0 < log_size) {
            char *log = _cairo_malloc (log_size);
            GLint chars;

            log[log_size - 1] = '\0';
            glGetProgramInfoLog (*program, log_size, &chars, log);
            printf ("OpenGL shader link failed:\n%s\n", log);

            free (log);
        } else {
            printf ("OpenGL shader link failed.\n");
        }

	ASSERT_NOT_REACHED;
    }
}

static void
destroy_shader_core_2_0 (GLuint shader)
{
  glDeleteShader (shader);
}

static void
destroy_program_core_2_0 (GLuint shader)
{
    glDeleteProgram (shader);
}

static void
bind_float_core_2_0 (cairo_gl_shader_t *shader,
		     const char *name,
		     float value)
{
    GLint location = glGetUniformLocation (shader->program, name);
    assert (location != -1);
    glUniform1f (location, value);
}

static void
bind_vec2_core_2_0 (cairo_gl_shader_t *shader,
		    const char *name,
		    float value0,
		    float value1)
{
    GLint location = glGetUniformLocation (shader->program, name);
    assert (location != -1);
    glUniform2f (location, value0, value1);
}

static void
bind_vec3_core_2_0 (cairo_gl_shader_t *shader,
		    const char *name,
		    float value0,
		    float value1,
		    float value2)
{
    GLint location = glGetUniformLocation (shader->program, name);
    assert (location != -1);
    glUniform3f (location, value0, value1, value2);
}

static void
bind_vec4_core_2_0 (cairo_gl_shader_t *shader,
		    const char *name,
		    float value0,
		    float value1,
		    float value2,
		    float value3)
{
    GLint location = glGetUniformLocation (shader->program, name);
    assert (location != -1);
    glUniform4f (location, value0, value1, value2, value3);
}

static void
bind_matrix_core_2_0 (cairo_gl_shader_t *shader, const char *name, cairo_matrix_t* m)
{
    GLint location = glGetUniformLocation (shader->program, name);
    float gl_m[16] = {
        m->xx, m->xy, m->x0,
        m->yx, m->yy, m->y0,
        0,     0,     1
    };
    assert (location != -1);
    glUniformMatrix3fv (location, 1, GL_TRUE, gl_m);
}

static void
bind_texture_core_2_0 (cairo_gl_shader_t *shader, const char *name, cairo_gl_tex_t tex_unit)
{
    GLint location = glGetUniformLocation (shader->program, name);
    assert (location != -1);
    glUniform1i (location, tex_unit);
}

static void
use_program_core_2_0 (cairo_gl_shader_t *shader)
{
    if (shader)
	glUseProgram (shader->program);
    else
	glUseProgram (0);
}

static const cairo_gl_shader_impl_t shader_impl_core_2_0 = {
    compile_shader_core_2_0,
    link_shader_core_2_0,
    destroy_shader_core_2_0,
    destroy_program_core_2_0,
    bind_float_core_2_0,
    bind_vec2_core_2_0,
    bind_vec3_core_2_0,
    bind_vec4_core_2_0,
    bind_matrix_core_2_0,
    bind_texture_core_2_0,
    use_program_core_2_0,
};

static const cairo_gl_shader_impl_t shader_impl_arb = {
    compile_shader_arb,
    link_shader_arb,
    destroy_shader_arb,
    destroy_program_arb,
    bind_float_arb,
    bind_vec2_arb,
    bind_vec3_arb,
    bind_vec4_arb,
    bind_matrix_arb,
    bind_texture_arb,
    use_program_arb,
};

typedef struct _cairo_shader_cache_entry {
    cairo_cache_entry_t base;

    cairo_gl_operand_type_t src;
    cairo_gl_operand_type_t mask;
    cairo_gl_operand_type_t dest;
    cairo_gl_shader_in_t in;

    cairo_gl_context_t *ctx; /* XXX: needed to destroy the program */
    cairo_gl_shader_t shader;
} cairo_shader_cache_entry_t;

static cairo_bool_t
_cairo_gl_shader_cache_equal (const void *key_a, const void *key_b)
{
    const cairo_shader_cache_entry_t *a = key_a;
    const cairo_shader_cache_entry_t *b = key_b;

    return a->src  == b->src  &&
           a->mask == b->mask &&
           a->dest == b->dest &&
           a->in   == b->in;
}

static unsigned long
_cairo_gl_shader_cache_hash (const cairo_shader_cache_entry_t *entry)
{
    return (entry->src << 24) | (entry->mask << 16) | (entry->dest << 8) | (entry->in);
}

static void
_cairo_gl_shader_cache_destroy (void *data)
{
    cairo_shader_cache_entry_t *entry = data;

    _cairo_gl_shader_fini (entry->ctx, &entry->shader);
    if (entry->ctx->current_shader == &entry->shader)
        entry->ctx->current_shader = NULL;
    free (entry);
}

static void
_cairo_gl_shader_init (cairo_gl_shader_t *shader)
{
    shader->fragment_shader = 0;
    shader->program = 0;
}

cairo_status_t
_cairo_gl_context_init_shaders (cairo_gl_context_t *ctx)
{
    static const char *fill_fs_source =
	"uniform vec4 color;\n"
	"void main()\n"
	"{\n"
	"	gl_FragColor = color;\n"
	"}\n";
    cairo_status_t status;

    /* XXX multiple device support? */
    if (GLEW_VERSION_2_0) {
        ctx->shader_impl = &shader_impl_core_2_0;
    } else if (GLEW_ARB_shader_objects &&
               GLEW_ARB_fragment_shader &&
               GLEW_ARB_vertex_program) {
        ctx->shader_impl = &shader_impl_arb;
    } else {
        ctx->shader_impl = NULL;
    }

    memset (ctx->vertex_shaders, 0, sizeof (ctx->vertex_shaders));

    status = _cairo_cache_init (&ctx->shaders,
                                _cairo_gl_shader_cache_equal,
                                NULL,
                                _cairo_gl_shader_cache_destroy,
                                CAIRO_GL_MAX_SHADERS_PER_CONTEXT);
    if (unlikely (status))
	return status;

    if (ctx->shader_impl != NULL) {
	_cairo_gl_shader_init (&ctx->fill_rectangles_shader);
	status = _cairo_gl_shader_compile (ctx,
					   &ctx->fill_rectangles_shader,
					   CAIRO_GL_VAR_NONE,
					   CAIRO_GL_VAR_NONE,
					   fill_fs_source);
	if (unlikely (status))
	    return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_gl_context_fini_shaders (cairo_gl_context_t *ctx)
{
    int i;

    for (i = 0; i <= CAIRO_GL_VAR_TYPE_MAX; i++) {
	if (ctx->vertex_shaders[i])
	    ctx->shader_impl->destroy_shader (ctx->vertex_shaders[i]);
    }

    _cairo_cache_fini (&ctx->shaders);
}

void
_cairo_gl_shader_fini (cairo_gl_context_t *ctx,
		       cairo_gl_shader_t *shader)
{
    if (shader->fragment_shader)
        ctx->shader_impl->destroy_shader (shader->fragment_shader);

    if (shader->program)
        ctx->shader_impl->destroy_program (shader->program);
}

static const char *operand_names[] = { "source", "mask", "dest" };

static cairo_gl_var_type_t
cairo_gl_operand_get_var_type (cairo_gl_operand_type_t type)
{
    switch (type) {
    default:
    case CAIRO_GL_OPERAND_COUNT:
        ASSERT_NOT_REACHED;
    case CAIRO_GL_OPERAND_NONE:
    case CAIRO_GL_OPERAND_CONSTANT:
    case CAIRO_GL_OPERAND_LINEAR_GRADIENT:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT:
        return CAIRO_GL_VAR_NONE;
    case CAIRO_GL_OPERAND_TEXTURE:
        return CAIRO_GL_VAR_TEXCOORDS;
    case CAIRO_GL_OPERAND_SPANS:
        return CAIRO_GL_VAR_COVERAGE;
    }
}

static void
cairo_gl_shader_emit_variable (cairo_output_stream_t *stream,
                               cairo_gl_var_type_t type,
                               cairo_gl_tex_t name)
{
    switch (type) {
    default:
        ASSERT_NOT_REACHED;
    case CAIRO_GL_VAR_NONE:
        break;
    case CAIRO_GL_VAR_TEXCOORDS:
        _cairo_output_stream_printf (stream, 
                                     "varying vec2 %s_texcoords;\n", 
                                     operand_names[name]);
        break;
    case CAIRO_GL_VAR_COVERAGE:
        _cairo_output_stream_printf (stream, 
                                     "varying float %s_coverage;\n", 
                                     operand_names[name]);
        break;
    }
}

static void
cairo_gl_shader_emit_vertex (cairo_output_stream_t *stream,
                             cairo_gl_var_type_t type,
                             cairo_gl_tex_t name)
{
    switch (type) {
    default:
        ASSERT_NOT_REACHED;
    case CAIRO_GL_VAR_NONE:
        break;
    case CAIRO_GL_VAR_TEXCOORDS:
        _cairo_output_stream_printf (stream, 
                                     "    %s_texcoords = gl_MultiTexCoord%d.xy;\n",
                                     operand_names[name], name);
        break;
    case CAIRO_GL_VAR_COVERAGE:
        _cairo_output_stream_printf (stream, 
                                     "    %s_coverage = gl_Color.a;\n",
                                     operand_names[name]);
        break;
    }
}

static cairo_status_t
cairo_gl_shader_get_vertex_source (cairo_gl_var_type_t src,
                                   cairo_gl_var_type_t mask,
                                   cairo_gl_var_type_t dest,
				   char **out)
{
    cairo_output_stream_t *stream = _cairo_memory_stream_create ();
    unsigned char *source;
    unsigned int length;
    cairo_status_t status;

    cairo_gl_shader_emit_variable (stream, src, CAIRO_GL_TEX_SOURCE);
    cairo_gl_shader_emit_variable (stream, mask, CAIRO_GL_TEX_MASK);

    _cairo_output_stream_printf (stream,
				 "void main()\n"
				 "{\n"
				 "    gl_Position = ftransform();\n");

    cairo_gl_shader_emit_vertex (stream, src, CAIRO_GL_TEX_SOURCE);
    cairo_gl_shader_emit_vertex (stream, mask, CAIRO_GL_TEX_MASK);

    _cairo_output_stream_write (stream,
				"}\n\0", 3);

    status = _cairo_memory_stream_destroy (stream, &source, &length);
    if (unlikely (status))
	return status;

    *out = (char *) source;
    return CAIRO_STATUS_SUCCESS;
}

static void
cairo_gl_shader_emit_color (cairo_output_stream_t *stream,
                            GLuint tex_target,
                            cairo_gl_operand_type_t type,
                            cairo_gl_tex_t name)
{
    const char *namestr = operand_names[name];
    const char *rectstr = (tex_target == GL_TEXTURE_RECTANGLE_EXT ? "Rect" : "");

    switch (type) {
    case CAIRO_GL_OPERAND_COUNT:
    default:
        ASSERT_NOT_REACHED;
        break;
    case CAIRO_GL_OPERAND_NONE:
        _cairo_output_stream_printf (stream, 
            "vec4 get_%s()\n"
            "{\n"
            "    return vec4 (0, 0, 0, 1);\n"
            "}\n",
            namestr);
        break;
    case CAIRO_GL_OPERAND_CONSTANT:
        _cairo_output_stream_printf (stream, 
            "uniform vec4 %s_constant;\n"
            "vec4 get_%s()\n"
            "{\n"
            "    return %s_constant;\n"
            "}\n",
            namestr, namestr, namestr);
        break;
    case CAIRO_GL_OPERAND_TEXTURE:
        _cairo_output_stream_printf (stream, 
            "uniform sampler2D%s %s_sampler;\n"
            "varying vec2 %s_texcoords;\n"
            "vec4 get_%s()\n"
            "{\n"
            "    return texture2D%s(%s_sampler, %s_texcoords);\n"
            "}\n",
            rectstr, namestr, namestr, namestr, rectstr, namestr, namestr);
        break;
    case CAIRO_GL_OPERAND_LINEAR_GRADIENT:
        _cairo_output_stream_printf (stream, 
            "uniform sampler1D %s_sampler;\n"
            "uniform mat3 %s_matrix;\n"
            "uniform vec2 %s_segment;\n"
            "\n"
            "vec4 get_%s()\n"
            "{\n"
            "    vec2 pos = (%s_matrix * vec3 (gl_FragCoord.xy, 1.0)).xy;\n"
            "    float t = dot (pos, %s_segment) / dot (%s_segment, %s_segment);\n"
            "    return texture1D (%s_sampler, t);\n"
            "}\n",
            namestr, namestr, namestr, namestr, namestr, 
            namestr, namestr, namestr, namestr);
        break;
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT:
        _cairo_output_stream_printf (stream, 
            "uniform sampler1D %s_sampler;\n"
            "uniform mat3 %s_matrix;\n"
            "uniform vec2 %s_circle_1;\n"
            "uniform float %s_radius_0;\n"
            "uniform float %s_radius_1;\n"
            "\n"
            "vec4 get_%s()\n"
            "{\n"
            "    vec2 pos = (%s_matrix * vec3 (gl_FragCoord.xy, 1.0)).xy;\n"
            "    \n"
            "    float dr = %s_radius_1 - %s_radius_0;\n"
            "    float dot_circle_1 = dot (%s_circle_1, %s_circle_1);\n"
            "    float dot_pos_circle_1 = dot (pos, %s_circle_1);\n"
            "    \n"
            "    float A = dot_circle_1 - dr * dr;\n"
            "    float B = -2.0 * (dot_pos_circle_1 + %s_radius_0 * dr);\n"
            "    float C = dot (pos, pos) - %s_radius_0 * %s_radius_0;\n"
            "    float det = B * B - 4.0 * A * C;\n"
            "    det = max (det, 0.0);\n"
            "    \n"
            "    float sqrt_det = sqrt (det);\n"
            "    sqrt_det *= sign(A);\n"
            "    \n"
            "    float t = (-B + sqrt_det) / (2.0 * A);\n"
            "    return texture1D (%s_sampler, t);\n"
            "}\n",
            namestr, namestr, namestr, namestr, namestr, 
            namestr, namestr, namestr, namestr, namestr, 
            namestr, namestr, namestr, namestr, namestr, 
            namestr);
        break;
    case CAIRO_GL_OPERAND_SPANS:
        _cairo_output_stream_printf (stream, 
            "varying float %s_coverage;\n"
            "vec4 get_%s()\n"
            "{\n"
            "    return vec4(0, 0, 0, %s_coverage);\n"
            "}\n",
            namestr, namestr, namestr);
        break;
    }
}

static cairo_status_t
cairo_gl_shader_get_fragment_source (GLuint tex_target,
                                     cairo_gl_shader_in_t in,
                                     cairo_gl_operand_type_t src,
                                     cairo_gl_operand_type_t mask,
                                     cairo_gl_operand_type_t dest,
				     char **out)
{
    cairo_output_stream_t *stream = _cairo_memory_stream_create ();
    unsigned char *source;
    unsigned int length;
    cairo_status_t status;

    cairo_gl_shader_emit_color (stream, tex_target, src, CAIRO_GL_TEX_SOURCE);
    cairo_gl_shader_emit_color (stream, tex_target, mask, CAIRO_GL_TEX_MASK);

    _cairo_output_stream_printf (stream,
        "void main()\n"
        "{\n");
    switch (in) {
    case CAIRO_GL_SHADER_IN_COUNT:
    default:
        ASSERT_NOT_REACHED;
    case CAIRO_GL_SHADER_IN_NORMAL:
        _cairo_output_stream_printf (stream,
            "    gl_FragColor = get_source() * get_mask().a;\n");
        break;
    case CAIRO_GL_SHADER_IN_CA_SOURCE:
        _cairo_output_stream_printf (stream,
            "    gl_FragColor = get_source() * get_mask();\n");
        break;
    case CAIRO_GL_SHADER_IN_CA_SOURCE_ALPHA:
        _cairo_output_stream_printf (stream,
            "    gl_FragColor = get_source().a * get_mask();\n");
        break;
    }

    _cairo_output_stream_write (stream,
                                "}\n\0", 3);

    status = _cairo_memory_stream_destroy (stream, &source, &length);
    if (unlikely (status))
        return status;

    *out = (char *) source;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gl_shader_compile (cairo_gl_context_t *ctx,
			  cairo_gl_shader_t *shader,
			  cairo_gl_var_type_t src,
			  cairo_gl_var_type_t mask,
			  const char *fragment_text)
{
    unsigned int vertex_shader;
    cairo_status_t status;

    if (ctx->shader_impl == NULL)
        return CAIRO_STATUS_SUCCESS;

    assert (shader->program == 0);

    vertex_shader = cairo_gl_var_type_hash (src, mask, CAIRO_GL_VAR_NONE);
    if (ctx->vertex_shaders[vertex_shader] == 0) {
	char *source;

	status = cairo_gl_shader_get_vertex_source (src,
						    mask,
						    CAIRO_GL_VAR_NONE,
						    &source);
        if (unlikely (status))
            goto FAILURE;

	ctx->shader_impl->compile_shader (&ctx->vertex_shaders[vertex_shader],
					  GL_VERTEX_SHADER,
					  source);
        free (source);
    }

    ctx->shader_impl->compile_shader (&shader->fragment_shader,
				      GL_FRAGMENT_SHADER,
				      fragment_text);

    ctx->shader_impl->link_shader (&shader->program,
				   ctx->vertex_shaders[vertex_shader],
				   shader->fragment_shader);

    return CAIRO_STATUS_SUCCESS;

 FAILURE:
    _cairo_gl_shader_fini (ctx, shader);
    shader->fragment_shader = 0;
    shader->program = 0;

    return status;
}

void
_cairo_gl_shader_bind_float (cairo_gl_context_t *ctx,
			     const char *name,
			     float value)
{
    ctx->shader_impl->bind_float (ctx->current_shader, name, value);
}

void
_cairo_gl_shader_bind_vec2 (cairo_gl_context_t *ctx,
			    const char *name,
			    float value0,
			    float value1)
{
    ctx->shader_impl->bind_vec2 (ctx->current_shader, name, value0, value1);
}

void
_cairo_gl_shader_bind_vec3 (cairo_gl_context_t *ctx,
			    const char *name,
			    float value0,
			    float value1,
			    float value2)
{
    ctx->shader_impl->bind_vec3 (ctx->current_shader, name, value0, value1, value2);
}

void
_cairo_gl_shader_bind_vec4 (cairo_gl_context_t *ctx,
			    const char *name,
			    float value0, float value1,
			    float value2, float value3)
{
    ctx->shader_impl->bind_vec4 (ctx->current_shader, name, value0, value1, value2, value3);
}

void
_cairo_gl_shader_bind_matrix (cairo_gl_context_t *ctx,
			      const char *name, cairo_matrix_t* m)
{
    ctx->shader_impl->bind_matrix (ctx->current_shader, name, m);
}

void
_cairo_gl_shader_bind_texture (cairo_gl_context_t *ctx,
			       const char *name, GLuint tex_unit)
{
    ctx->shader_impl->bind_texture (ctx->current_shader, name, tex_unit);
}

void
_cairo_gl_set_shader (cairo_gl_context_t *ctx,
		      cairo_gl_shader_t *shader)
{
    if (ctx->shader_impl == NULL)
	return;

    if (ctx->current_shader == shader)
        return;

    ctx->shader_impl->use (shader);

    ctx->current_shader = shader;
}

cairo_status_t
_cairo_gl_get_shader_by_type (cairo_gl_context_t *ctx,
                              cairo_gl_operand_type_t source,
                              cairo_gl_operand_type_t mask,
                              cairo_gl_shader_in_t in,
                              cairo_gl_shader_t **shader)
{
    cairo_shader_cache_entry_t lookup, *entry;
    char *fs_source;
    cairo_status_t status;

    if (ctx->shader_impl == NULL) {
        *shader = NULL;
	return CAIRO_STATUS_SUCCESS;
    }

    lookup.src = source;
    lookup.mask = mask;
    lookup.dest = CAIRO_GL_OPERAND_NONE;
    lookup.in = in;
    lookup.base.hash = _cairo_gl_shader_cache_hash (&lookup);
    lookup.base.size = 1;

    entry = _cairo_cache_lookup (&ctx->shaders, &lookup.base);
    if (entry) {
        assert (entry->shader.program);
        *shader = &entry->shader;
	return CAIRO_STATUS_SUCCESS;
    }

    status = cairo_gl_shader_get_fragment_source (ctx->tex_target,
						  in,
						  source,
						  mask,
						  CAIRO_GL_OPERAND_NONE,
						  &fs_source);
    if (unlikely (status))
	return status;

    entry = malloc (sizeof (cairo_shader_cache_entry_t));
    if (unlikely (entry == NULL)) {
        free (fs_source);
        return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    memcpy (entry, &lookup, sizeof (cairo_shader_cache_entry_t));

    entry->ctx = ctx;
    _cairo_gl_shader_init (&entry->shader);
    status = _cairo_gl_shader_compile (ctx,
				       &entry->shader,
				       cairo_gl_operand_get_var_type (source),
				       cairo_gl_operand_get_var_type (mask),
				       fs_source);
    free (fs_source);

    if (unlikely (status)) {
	free (entry);
	return status;
    }

    status = _cairo_cache_insert (&ctx->shaders, &entry->base);
    if (unlikely (status)) {
	_cairo_gl_shader_fini (ctx, &entry->shader);
	free (entry);
	return status;
    }

    *shader = &entry->shader;

    return CAIRO_STATUS_SUCCESS;
}
