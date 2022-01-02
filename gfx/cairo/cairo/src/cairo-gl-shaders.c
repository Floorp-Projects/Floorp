/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 T. Zachary Laine
 * Copyright © 2010 Eric Anholt
 * Copyright © 2010 Red Hat, Inc
 * Copyright © 2010 Linaro Limited
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
 *	Alexandros Frantzis <alexandros.frantzis@linaro.org>
 *  H. Lewin <heiko.lewin@gmx.de>
 */

#include "cairoint.h"
#include "cairo-gl-private.h"
#include "cairo-error-private.h"
#include "cairo-output-stream-private.h"

static cairo_status_t
_cairo_gl_shader_compile_and_link (cairo_gl_context_t *ctx,
				   cairo_gl_shader_t *shader,
				   cairo_gl_var_type_t src,
				   cairo_gl_var_type_t mask,
				   cairo_bool_t use_coverage,
				   const char *fragment_text);

typedef struct _cairo_shader_cache_entry {
    cairo_cache_entry_t base;

    unsigned vertex;

    cairo_gl_operand_type_t src;
    cairo_gl_operand_type_t mask;
    cairo_gl_operand_type_t dest;
    cairo_bool_t use_coverage;

    cairo_gl_shader_in_t in;
    GLint src_gl_filter;
    cairo_bool_t src_border_fade;
    cairo_extend_t src_extend;
    GLint mask_gl_filter;
    cairo_bool_t mask_border_fade;
    cairo_extend_t mask_extend;

    cairo_gl_context_t *ctx; /* XXX: needed to destroy the program */
    cairo_gl_shader_t shader;
} cairo_shader_cache_entry_t;

static cairo_bool_t
_cairo_gl_shader_cache_equal_desktop (const void *key_a, const void *key_b)
{
    const cairo_shader_cache_entry_t *a = key_a;
    const cairo_shader_cache_entry_t *b = key_b;
    cairo_bool_t both_have_npot_repeat =
	a->ctx->has_npot_repeat && b->ctx->has_npot_repeat;

    return (a->vertex == b->vertex &&
	    a->src  == b->src  &&
	    a->mask == b->mask &&
	    a->dest == b->dest &&
	    a->use_coverage == b->use_coverage &&
	    a->in   == b->in &&
	    (both_have_npot_repeat || a->src_extend == b->src_extend) &&
	    (both_have_npot_repeat || a->mask_extend == b->mask_extend));
}

/*
 * For GLES2 we use more complicated shaders to implement missing GL
 * features. In this case we need more parameters to uniquely identify
 * a shader (vs _cairo_gl_shader_cache_equal_desktop()).
 */
static cairo_bool_t
_cairo_gl_shader_cache_equal_gles2 (const void *key_a, const void *key_b)
{
    const cairo_shader_cache_entry_t *a = key_a;
    const cairo_shader_cache_entry_t *b = key_b;
    cairo_bool_t both_have_npot_repeat =
	a->ctx->has_npot_repeat && b->ctx->has_npot_repeat;

    return (a->vertex == b->vertex &&
	    a->src  == b->src  &&
	    a->mask == b->mask &&
	    a->dest == b->dest &&
	    a->use_coverage == b->use_coverage &&
	    a->in   == b->in   &&
	    a->src_gl_filter == b->src_gl_filter &&
	    a->src_border_fade == b->src_border_fade &&
	    (both_have_npot_repeat || a->src_extend == b->src_extend) &&
	    a->mask_gl_filter == b->mask_gl_filter &&
	    a->mask_border_fade == b->mask_border_fade &&
	    (both_have_npot_repeat || a->mask_extend == b->mask_extend));
}

static unsigned long
_cairo_gl_shader_cache_hash (const cairo_shader_cache_entry_t *entry)
{
    return (((uint32_t)entry->src << 24) | (entry->mask << 16) | (entry->dest << 8) | (entry->in << 1) | entry->use_coverage) ^ entry->vertex;
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
	"#ifdef GL_ES\n"
	"precision mediump float;\n"
	"#endif\n"
	"uniform vec4 color;\n"
	"void main()\n"
	"{\n"
	"	gl_FragColor = color;\n"
	"}\n";
    cairo_status_t status;

    if (_cairo_gl_get_version () >= CAIRO_GL_VERSION_ENCODE (2, 0) ||
	(_cairo_gl_has_extension ("GL_ARB_shader_objects") &&
	 _cairo_gl_has_extension ("GL_ARB_fragment_shader") &&
	 _cairo_gl_has_extension ("GL_ARB_vertex_shader"))) {
	ctx->has_shader_support = TRUE;
    } else {
	ctx->has_shader_support = FALSE;
	fprintf (stderr, "Error: The cairo gl backend requires shader support!\n");
	return CAIRO_STATUS_DEVICE_ERROR;
    }

    memset (ctx->vertex_shaders, 0, sizeof (ctx->vertex_shaders));

    status = _cairo_cache_init (&ctx->shaders,
				ctx->gl_flavor == CAIRO_GL_FLAVOR_DESKTOP ?
				    _cairo_gl_shader_cache_equal_desktop :
				    _cairo_gl_shader_cache_equal_gles2,
				NULL,
				_cairo_gl_shader_cache_destroy,
				CAIRO_GL_MAX_SHADERS_PER_CONTEXT);
    if (unlikely (status))
	return status;

    _cairo_gl_shader_init (&ctx->fill_rectangles_shader);
    status = _cairo_gl_shader_compile_and_link (ctx,
						&ctx->fill_rectangles_shader,
						CAIRO_GL_VAR_NONE,
						CAIRO_GL_VAR_NONE,
						FALSE,
						fill_fs_source);
    if (unlikely (status))
	return status;

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_gl_context_fini_shaders (cairo_gl_context_t *ctx)
{
    int i;

    for (i = 0; i < CAIRO_GL_VAR_TYPE_MAX; i++) {
	if (ctx->vertex_shaders[i])
	    ctx->dispatch.DeleteShader (ctx->vertex_shaders[i]);
    }

    _cairo_gl_shader_fini(ctx, &ctx->fill_rectangles_shader);
    _cairo_cache_fini (&ctx->shaders);
}

void
_cairo_gl_shader_fini (cairo_gl_context_t *ctx,
		       cairo_gl_shader_t *shader)
{
    if (shader->fragment_shader)
	ctx->dispatch.DeleteShader (shader->fragment_shader);

    if (shader->program)
	ctx->dispatch.DeleteProgram (shader->program);
}

static const char *operand_names[] = { "source", "mask", "dest" };

static cairo_gl_var_type_t
cairo_gl_operand_get_var_type (cairo_gl_operand_t *operand)
{
    switch (operand->type) {
    default:
    case CAIRO_GL_OPERAND_COUNT:
	ASSERT_NOT_REACHED;
    case CAIRO_GL_OPERAND_NONE:
    case CAIRO_GL_OPERAND_CONSTANT:
	return CAIRO_GL_VAR_NONE;
    case CAIRO_GL_OPERAND_LINEAR_GRADIENT:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_A0:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_NONE:
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_EXT:
	return operand->gradient.texgen ? CAIRO_GL_VAR_TEXGEN : CAIRO_GL_VAR_TEXCOORDS;
    case CAIRO_GL_OPERAND_TEXTURE:
	return operand->texture.texgen ? CAIRO_GL_VAR_TEXGEN : CAIRO_GL_VAR_TEXCOORDS;
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
				     "attribute vec4 MultiTexCoord%d;\n"
				     "varying vec2 %s_texcoords;\n",
				     name,
				     operand_names[name]);
	break;
    case CAIRO_GL_VAR_TEXGEN:
	_cairo_output_stream_printf (stream,
				     "uniform mat3 %s_texgen;\n"
				     "varying vec2 %s_texcoords;\n",
				     operand_names[name],
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
				     "    %s_texcoords = MultiTexCoord%d.xy;\n",
				     operand_names[name], name);
	break;

    case CAIRO_GL_VAR_TEXGEN:
	_cairo_output_stream_printf (stream,
				     "    %s_texcoords = (%s_texgen * Vertex.xyw).xy;\n",
				     operand_names[name], operand_names[name]);
	break;
    }
}

static void
cairo_gl_shader_dcl_coverage (cairo_output_stream_t *stream)
{
    _cairo_output_stream_printf (stream, "varying float coverage;\n");
}

static void
cairo_gl_shader_def_coverage (cairo_output_stream_t *stream)
{
    _cairo_output_stream_printf (stream, "    coverage = Color.a;\n");
}

static cairo_status_t
cairo_gl_shader_get_vertex_source (cairo_gl_var_type_t src,
				   cairo_gl_var_type_t mask,
				   cairo_bool_t use_coverage,
				   cairo_gl_var_type_t dest,
				   char **out)
{
    cairo_output_stream_t *stream = _cairo_memory_stream_create ();
    unsigned char *source;
    unsigned long length;
    cairo_status_t status;

    cairo_gl_shader_emit_variable (stream, src, CAIRO_GL_TEX_SOURCE);
    cairo_gl_shader_emit_variable (stream, mask, CAIRO_GL_TEX_MASK);
    if (use_coverage)
	cairo_gl_shader_dcl_coverage (stream);

    _cairo_output_stream_printf (stream,
				 "attribute vec4 Vertex;\n"
				 "attribute vec4 Color;\n"
				 "uniform mat4 ModelViewProjectionMatrix;\n"
				 "void main()\n"
				 "{\n"
				 "    gl_Position = ModelViewProjectionMatrix * Vertex;\n");

    cairo_gl_shader_emit_vertex (stream, src, CAIRO_GL_TEX_SOURCE);
    cairo_gl_shader_emit_vertex (stream, mask, CAIRO_GL_TEX_MASK);
    if (use_coverage)
	cairo_gl_shader_def_coverage (stream);

    _cairo_output_stream_write (stream,
				"}\n\0", 3);

    status = _cairo_memory_stream_destroy (stream, &source, &length);
    if (unlikely (status))
	return status;

    *out = (char *) source;
    return CAIRO_STATUS_SUCCESS;
}

/*
 * Returns whether an operand needs a special border fade fragment shader
 * to simulate the GL_CLAMP_TO_BORDER wrapping method that is missing in GLES2.
 */
static cairo_bool_t
_cairo_gl_shader_needs_border_fade (cairo_gl_operand_t *operand)
{
    cairo_extend_t extend =_cairo_gl_operand_get_extend (operand);

    return extend == CAIRO_EXTEND_NONE &&
	   (operand->type == CAIRO_GL_OPERAND_TEXTURE ||
	    operand->type == CAIRO_GL_OPERAND_LINEAR_GRADIENT ||
	    operand->type == CAIRO_GL_OPERAND_RADIAL_GRADIENT_NONE ||
	    operand->type == CAIRO_GL_OPERAND_RADIAL_GRADIENT_A0);
}

static void
cairo_gl_shader_emit_color (cairo_output_stream_t *stream,
			    cairo_gl_context_t *ctx,
			    cairo_gl_operand_t *op,
			    cairo_gl_tex_t name)
{
    const char *namestr = operand_names[name];
    const char *rectstr = (ctx->tex_target == GL_TEXTURE_RECTANGLE ? "Rect" : "");

    switch (op->type) {
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
	     "uniform vec2 %s_texdims;\n"
	     "varying vec2 %s_texcoords;\n"
	     "vec4 get_%s()\n"
	     "{\n",
	     rectstr, namestr, namestr, namestr, namestr);
	if ((ctx->gl_flavor == CAIRO_GL_FLAVOR_ES3 ||
	     ctx->gl_flavor == CAIRO_GL_FLAVOR_ES2) &&
	    _cairo_gl_shader_needs_border_fade (op))
	{
	    _cairo_output_stream_printf (stream,
		"    vec2 border_fade = %s_border_fade (%s_texcoords, %s_texdims);\n"
		"    vec4 texel = texture2D%s (%s_sampler, %s_texcoords);\n"
		"    return texel * border_fade.x * border_fade.y;\n"
		"}\n",
		namestr, namestr, namestr, rectstr, namestr, namestr);
	}
	else
	{
	    _cairo_output_stream_printf (stream,
		"    return texture2D%s (%s_sampler, %s_wrap (%s_texcoords));\n"
		"}\n",
		rectstr, namestr, namestr, namestr);
	}
	break;
    case CAIRO_GL_OPERAND_LINEAR_GRADIENT:
	_cairo_output_stream_printf (stream,
	    "varying vec2 %s_texcoords;\n"
	    "uniform vec2 %s_texdims;\n"
	    "uniform sampler2D%s %s_sampler;\n"
	    "\n"
	    "vec4 get_%s()\n"
	    "{\n",
	    namestr, namestr, rectstr, namestr, namestr);
	if ((ctx->gl_flavor == CAIRO_GL_FLAVOR_ES3 ||
	     ctx->gl_flavor == CAIRO_GL_FLAVOR_ES2) &&
	    _cairo_gl_shader_needs_border_fade (op))
	{
	    _cairo_output_stream_printf (stream,
		"    float border_fade = %s_border_fade (%s_texcoords.x, %s_texdims.x);\n"
		"    vec4 texel = texture2D%s (%s_sampler, vec2 (%s_texcoords.x, 0.5));\n"
		"    return texel * border_fade;\n"
		"}\n",
		namestr, namestr, namestr, rectstr, namestr, namestr);
	}
	else
	{
	    _cairo_output_stream_printf (stream,
		"    return texture2D%s (%s_sampler, %s_wrap (vec2 (%s_texcoords.x, 0.5)));\n"
		"}\n",
		rectstr, namestr, namestr, namestr);
	}
	break;
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_A0:
	_cairo_output_stream_printf (stream,
	    "varying vec2 %s_texcoords;\n"
	    "uniform vec2 %s_texdims;\n"
	    "uniform sampler2D%s %s_sampler;\n"
	    "uniform vec3 %s_circle_d;\n"
	    "uniform float %s_radius_0;\n"
	    "\n"
	    "vec4 get_%s()\n"
	    "{\n"
	    "    vec3 pos = vec3 (%s_texcoords, %s_radius_0);\n"
	    "    \n"
	    "    float B = dot (pos, %s_circle_d);\n"
	    "    float C = dot (pos, vec3 (pos.xy, -pos.z));\n"
	    "    \n"
	    "    float t = 0.5 * C / B;\n"
	    "    float is_valid = step (-%s_radius_0, t * %s_circle_d.z);\n",
	    namestr, namestr, rectstr, namestr, namestr, namestr, namestr,
	    namestr, namestr, namestr, namestr, namestr);
	if ((ctx->gl_flavor == CAIRO_GL_FLAVOR_ES3 ||
	     ctx->gl_flavor == CAIRO_GL_FLAVOR_ES2) &&
	    _cairo_gl_shader_needs_border_fade (op))
	{
	    _cairo_output_stream_printf (stream,
		"    float border_fade = %s_border_fade (t, %s_texdims.x);\n"
		"    vec4 texel = texture2D%s (%s_sampler, vec2 (t, 0.5));\n"
		"    return mix (vec4 (0.0), texel * border_fade, is_valid);\n"
		"}\n",
		namestr, namestr, rectstr, namestr);
	}
	else
	{
	    _cairo_output_stream_printf (stream,
		"    vec4 texel = texture2D%s (%s_sampler, %s_wrap (vec2 (t, 0.5)));\n"
		"    return mix (vec4 (0.0), texel, is_valid);\n"
		"}\n",
		rectstr, namestr, namestr);
	}
	break;
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_NONE:
	_cairo_output_stream_printf (stream,
	    "varying vec2 %s_texcoords;\n"
	    "uniform vec2 %s_texdims;\n"
	    "uniform sampler2D%s %s_sampler;\n"
	    "uniform vec3 %s_circle_d;\n"
	    "uniform float %s_a;\n"
	    "uniform float %s_radius_0;\n"
	    "\n"
	    "vec4 get_%s()\n"
	    "{\n"
	    "    vec3 pos = vec3 (%s_texcoords, %s_radius_0);\n"
	    "    \n"
	    "    float B = dot (pos, %s_circle_d);\n"
	    "    float C = dot (pos, vec3 (pos.xy, -pos.z));\n"
	    "    \n"
	    "    float det = dot (vec2 (B, %s_a), vec2 (B, -C));\n"
	    "    float sqrtdet = sqrt (abs (det));\n"
	    "    vec2 t = (B + vec2 (sqrtdet, -sqrtdet)) / %s_a;\n"
	    "    \n"
	    "    vec2 is_valid = step (vec2 (0.0), t) * step (t, vec2(1.0));\n"
	    "    float has_color = step (0., det) * max (is_valid.x, is_valid.y);\n"
	    "    \n"
	    "    float upper_t = mix (t.y, t.x, is_valid.x);\n",
	    namestr, namestr, rectstr, namestr, namestr, namestr, namestr,
	    namestr, namestr, namestr, namestr, namestr, namestr);
	if ((ctx->gl_flavor == CAIRO_GL_FLAVOR_ES3 ||
	     ctx->gl_flavor == CAIRO_GL_FLAVOR_ES2) &&
	    _cairo_gl_shader_needs_border_fade (op))
	{
	    _cairo_output_stream_printf (stream,
		"    float border_fade = %s_border_fade (upper_t, %s_texdims.x);\n"
		"    vec4 texel = texture2D%s (%s_sampler, vec2 (upper_t, 0.5));\n"
		"    return mix (vec4 (0.0), texel * border_fade, has_color);\n"
		"}\n",
		namestr, namestr, rectstr, namestr);
	}
	else
	{
	    _cairo_output_stream_printf (stream,
		"    vec4 texel = texture2D%s (%s_sampler, %s_wrap (vec2(upper_t, 0.5)));\n"
		"    return mix (vec4 (0.0), texel, has_color);\n"
		"}\n",
		rectstr, namestr, namestr);
	}
	break;
    case CAIRO_GL_OPERAND_RADIAL_GRADIENT_EXT:
	_cairo_output_stream_printf (stream,
	    "varying vec2 %s_texcoords;\n"
	    "uniform sampler2D%s %s_sampler;\n"
	    "uniform vec3 %s_circle_d;\n"
	    "uniform float %s_a;\n"
	    "uniform float %s_radius_0;\n"
	    "\n"
	    "vec4 get_%s()\n"
	    "{\n"
	    "    vec3 pos = vec3 (%s_texcoords, %s_radius_0);\n"
	    "    \n"
	    "    float B = dot (pos, %s_circle_d);\n"
	    "    float C = dot (pos, vec3 (pos.xy, -pos.z));\n"
	    "    \n"
	    "    float det = dot (vec2 (B, %s_a), vec2 (B, -C));\n"
	    "    float sqrtdet = sqrt (abs (det));\n"
	    "    vec2 t = (B + vec2 (sqrtdet, -sqrtdet)) / %s_a;\n"
	    "    \n"
	    "    vec2 is_valid = step (vec2 (-%s_radius_0), t * %s_circle_d.z);\n"
	    "    float has_color = step (0., det) * max (is_valid.x, is_valid.y);\n"
	    "    \n"
	    "    float upper_t = mix (t.y, t.x, is_valid.x);\n"
	    "    vec4 texel = texture2D%s (%s_sampler, %s_wrap (vec2(upper_t, 0.5)));\n"
	    "    return mix (vec4 (0.0), texel, has_color);\n"
	    "}\n",
	    namestr, rectstr, namestr, namestr, namestr, namestr,
	    namestr, namestr, namestr, namestr, namestr,
	    namestr, namestr, namestr, rectstr, namestr, namestr);
	break;
    }
}

/*
 * Emits the border fade functions used by an operand.
 *
 * If bilinear filtering is used, the emitted function performs a linear
 * fade to transparency effect in the intervals [-1/2n, 1/2n] and
 * [1 - 1/2n, 1 + 1/2n] (n: texture size).
 *
 * If nearest filtering is used, the emitted function just returns
 * 0.0 for all values outside [0, 1).
 */
static void
_cairo_gl_shader_emit_border_fade (cairo_output_stream_t *stream,
				   cairo_gl_operand_t *operand,
				   cairo_gl_tex_t name)
{
    const char *namestr = operand_names[name];
    GLint gl_filter = _cairo_gl_operand_get_gl_filter (operand);

    /* 2D version */
    _cairo_output_stream_printf (stream,
	"vec2 %s_border_fade (vec2 coords, vec2 dims)\n"
	"{\n",
	namestr);

    if (gl_filter == GL_LINEAR)
	_cairo_output_stream_printf (stream,
	    "    return clamp(-abs(dims * (coords - 0.5)) + (dims + vec2(1.0)) * 0.5, 0.0, 1.0);\n");
    else
	_cairo_output_stream_printf (stream,
	    "    bvec2 in_tex1 = greaterThanEqual (coords, vec2 (0.0));\n"
	    "    bvec2 in_tex2 = lessThan (coords, vec2 (1.0));\n"
	    "    return vec2 (float (all (in_tex1) && all (in_tex2)));\n");

    _cairo_output_stream_printf (stream, "}\n");

    /* 1D version */
    _cairo_output_stream_printf (stream,
	"float %s_border_fade (float x, float dim)\n"
	"{\n",
	namestr);
    if (gl_filter == GL_LINEAR)
	_cairo_output_stream_printf (stream,
	    "    return clamp(-abs(dim * (x - 0.5)) + (dim + 1.0) * 0.5, 0.0, 1.0);\n");
    else
	_cairo_output_stream_printf (stream,
	    "    bool in_tex = x >= 0.0 && x < 1.0;\n"
	    "    return float (in_tex);\n");

    _cairo_output_stream_printf (stream, "}\n");
}

/*
 * Emits the wrap function used by an operand.
 *
 * In OpenGL ES 2.0, repeat wrap modes (GL_REPEAT and GL_MIRRORED REPEAT) are
 * only available for NPOT textures if the GL_OES_texture_npot is supported.
 * If GL_OES_texture_npot is not supported, we need to implement the wrapping
 * functionality in the shader.
 */
static void
_cairo_gl_shader_emit_wrap (cairo_gl_context_t *ctx,
			    cairo_output_stream_t *stream,
			    cairo_gl_operand_t *operand,
			    cairo_gl_tex_t name)
{
    const char *namestr = operand_names[name];
    cairo_extend_t extend = _cairo_gl_operand_get_extend (operand);

    _cairo_output_stream_printf (stream,
	"vec2 %s_wrap(vec2 coords)\n"
	"{\n",
	namestr);

    if (! ctx->has_npot_repeat &&
	(extend == CAIRO_EXTEND_REPEAT || extend == CAIRO_EXTEND_REFLECT))
    {
	if (extend == CAIRO_EXTEND_REPEAT) {
	    _cairo_output_stream_printf (stream,
		"    return fract(coords);\n");
	} else { /* CAIRO_EXTEND_REFLECT */
	    _cairo_output_stream_printf (stream,
		"    return mix(fract(coords), 1.0 - fract(coords), floor(mod(coords, 2.0)));\n");
	}
    }
    else
    {
	_cairo_output_stream_printf (stream, "    return coords;\n");
    }

    _cairo_output_stream_printf (stream, "}\n");
}

static cairo_status_t
cairo_gl_shader_get_fragment_source (cairo_gl_context_t *ctx,
				     cairo_gl_shader_in_t in,
				     cairo_gl_operand_t *src,
				     cairo_gl_operand_t *mask,
				     cairo_bool_t use_coverage,
				     cairo_gl_operand_type_t dest_type,
				     char **out)
{
    cairo_output_stream_t *stream = _cairo_memory_stream_create ();
    unsigned char *source;
    unsigned long length;
    cairo_status_t status;
    const char *coverage_str;

    _cairo_output_stream_printf (stream,
	"#ifdef GL_ES\n"
	"precision mediump float;\n"
	"#endif\n");

    _cairo_gl_shader_emit_wrap (ctx, stream, src, CAIRO_GL_TEX_SOURCE);
    _cairo_gl_shader_emit_wrap (ctx, stream, mask, CAIRO_GL_TEX_MASK);

    if (ctx->gl_flavor == CAIRO_GL_FLAVOR_ES3 ||
	ctx->gl_flavor == CAIRO_GL_FLAVOR_ES2) {
	if (_cairo_gl_shader_needs_border_fade (src))
	    _cairo_gl_shader_emit_border_fade (stream, src, CAIRO_GL_TEX_SOURCE);
	if (_cairo_gl_shader_needs_border_fade (mask))
	    _cairo_gl_shader_emit_border_fade (stream, mask, CAIRO_GL_TEX_MASK);
    }

    cairo_gl_shader_emit_color (stream, ctx, src, CAIRO_GL_TEX_SOURCE);
    cairo_gl_shader_emit_color (stream, ctx, mask, CAIRO_GL_TEX_MASK);

    coverage_str = "";
    if (use_coverage) {
	_cairo_output_stream_printf (stream, "varying float coverage;\n");
	coverage_str = " * coverage";
    }

    _cairo_output_stream_printf (stream,
	"void main()\n"
	"{\n");
    switch (in) {
    case CAIRO_GL_SHADER_IN_COUNT:
    default:
	ASSERT_NOT_REACHED;
    case CAIRO_GL_SHADER_IN_NORMAL:
	_cairo_output_stream_printf (stream,
	    "    gl_FragColor = get_source() * get_mask().a%s;\n",
	    coverage_str);
	break;
    case CAIRO_GL_SHADER_IN_CA_SOURCE:
	_cairo_output_stream_printf (stream,
	    "    gl_FragColor = get_source() * get_mask()%s;\n",
	    coverage_str);
	break;
    case CAIRO_GL_SHADER_IN_CA_SOURCE_ALPHA:
	_cairo_output_stream_printf (stream,
	    "    gl_FragColor = get_source().a * get_mask()%s;\n",
	    coverage_str);
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

static void
compile_shader (cairo_gl_context_t *ctx,
		GLuint *shader,
		GLenum type,
		const char *source)
{
    cairo_gl_dispatch_t *dispatch = &ctx->dispatch;
    GLint success, log_size, num_chars;
    char *log;

    *shader = dispatch->CreateShader (type);
    dispatch->ShaderSource (*shader, 1, &source, 0);
    dispatch->CompileShader (*shader);
    dispatch->GetShaderiv (*shader, GL_COMPILE_STATUS, &success);

    if (success)
	return;

    dispatch->GetShaderiv (*shader, GL_INFO_LOG_LENGTH, &log_size);
    if (log_size < 0) {
	printf ("OpenGL shader compilation failed.\n");
	ASSERT_NOT_REACHED;
	return;
    }

    log = _cairo_malloc (log_size + 1);
    dispatch->GetShaderInfoLog (*shader, log_size, &num_chars, log);
    log[num_chars] = '\0';

    printf ("OpenGL shader compilation failed.  Shader:\n%s\n", source);
    printf ("OpenGL compilation log:\n%s\n", log);

    free (log);
    ASSERT_NOT_REACHED;
}

static void
link_shader_program (cairo_gl_context_t *ctx,
		     GLuint *program,
		     GLuint vert,
		     GLuint frag)
{
    cairo_gl_dispatch_t *dispatch = &ctx->dispatch;
    GLint success, log_size, num_chars;
    char *log;

    *program = dispatch->CreateProgram ();
    dispatch->AttachShader (*program, vert);
    dispatch->AttachShader (*program, frag);

    dispatch->BindAttribLocation (*program, CAIRO_GL_VERTEX_ATTRIB_INDEX,
				  "Vertex");
    dispatch->BindAttribLocation (*program, CAIRO_GL_COLOR_ATTRIB_INDEX,
				  "Color");
    dispatch->BindAttribLocation (*program, CAIRO_GL_TEXCOORD0_ATTRIB_INDEX,
				  "MultiTexCoord0");
    dispatch->BindAttribLocation (*program, CAIRO_GL_TEXCOORD1_ATTRIB_INDEX,
				  "MultiTexCoord1");

    dispatch->LinkProgram (*program);
    dispatch->GetProgramiv (*program, GL_LINK_STATUS, &success);
    if (success)
	return;

    dispatch->GetProgramiv (*program, GL_INFO_LOG_LENGTH, &log_size);
    if (log_size < 0) {
	printf ("OpenGL shader link failed.\n");
	ASSERT_NOT_REACHED;
	return;
    }

    log = _cairo_malloc (log_size + 1);
    dispatch->GetProgramInfoLog (*program, log_size, &num_chars, log);
    log[num_chars] = '\0';

    printf ("OpenGL shader link failed:\n%s\n", log);
    free (log);
    ASSERT_NOT_REACHED;
}

static GLint
_cairo_gl_get_op_uniform_location(cairo_gl_context_t *ctx,
				  cairo_gl_shader_t  *shader,
				  cairo_gl_tex_t      tex_unit,
				  const char         *suffix)
{
    cairo_gl_dispatch_t *dispatch = &ctx->dispatch;
    char uniform_name[100];
    const char *unit_name[2] = { "source", "mask" };

    snprintf (uniform_name, sizeof (uniform_name), "%s_%s",
	      unit_name[tex_unit], suffix);

    return dispatch->GetUniformLocation (shader->program, uniform_name);
}

static cairo_status_t
_cairo_gl_shader_compile_and_link (cairo_gl_context_t *ctx,
				   cairo_gl_shader_t *shader,
				   cairo_gl_var_type_t src,
				   cairo_gl_var_type_t mask,
				   cairo_bool_t use_coverage,
				   const char *fragment_text)
{
    cairo_gl_dispatch_t *dispatch = &ctx->dispatch;
    unsigned int vertex_shader;
    cairo_status_t status;
    int i;

    assert (shader->program == 0);

    vertex_shader = cairo_gl_var_type_hash (src, mask, use_coverage,
					    CAIRO_GL_VAR_NONE);
    if (ctx->vertex_shaders[vertex_shader] == 0) {
	char *source;

	status = cairo_gl_shader_get_vertex_source (src,
						    mask,
						    use_coverage,
						    CAIRO_GL_VAR_NONE,
						    &source);
	if (unlikely (status))
	    goto FAILURE;

	compile_shader (ctx, &ctx->vertex_shaders[vertex_shader],
			GL_VERTEX_SHADER, source);
	free (source);
    }

    compile_shader (ctx, &shader->fragment_shader,
		    GL_FRAGMENT_SHADER, fragment_text);

    link_shader_program (ctx, &shader->program,
			 ctx->vertex_shaders[vertex_shader],
			 shader->fragment_shader);

    shader->mvp_location =
	dispatch->GetUniformLocation (shader->program,
				      "ModelViewProjectionMatrix");

    for (i = 0; i < 2; i++) {
	shader->constant_location[i] =
	    _cairo_gl_get_op_uniform_location (ctx, shader, i, "constant");
	shader->a_location[i] =
	    _cairo_gl_get_op_uniform_location (ctx, shader, i, "a");
	shader->circle_d_location[i] =
	    _cairo_gl_get_op_uniform_location (ctx, shader, i, "circle_d");
	shader->radius_0_location[i] =
	    _cairo_gl_get_op_uniform_location (ctx, shader, i, "radius_0");
	shader->texdims_location[i] =
	    _cairo_gl_get_op_uniform_location (ctx, shader, i, "texdims");
	shader->texgen_location[i] =
	    _cairo_gl_get_op_uniform_location (ctx, shader, i, "texgen");
    }

    return CAIRO_STATUS_SUCCESS;

 FAILURE:
    _cairo_gl_shader_fini (ctx, shader);
    shader->fragment_shader = 0;
    shader->program = 0;

    return status;
}

/* We always bind the source to texture unit 0 if present, and mask to
 * texture unit 1 if present, so we can just initialize these once at
 * compile time.
 */
static cairo_status_t
_cairo_gl_shader_set_samplers (cairo_gl_context_t *ctx,
			       cairo_gl_shader_t *shader)
{
    cairo_gl_dispatch_t *dispatch = &ctx->dispatch;
    GLint location;
    GLint saved_program;

    /* We have to save/restore the current program because we might be
     * asked for a different program while a shader is bound.  This shouldn't
     * be a performance issue, since this is only called once per compile.
     */
    glGetIntegerv (GL_CURRENT_PROGRAM, &saved_program);
    dispatch->UseProgram (shader->program);

    location = dispatch->GetUniformLocation (shader->program, "source_sampler");
    if (location != -1) {
	dispatch->Uniform1i (location, CAIRO_GL_TEX_SOURCE);
    }

    location = dispatch->GetUniformLocation (shader->program, "mask_sampler");
    if (location != -1) {
	dispatch->Uniform1i (location, CAIRO_GL_TEX_MASK);
    }
    if(_cairo_gl_get_error()) return CAIRO_STATUS_DEVICE_ERROR;
    dispatch->UseProgram (saved_program);
    /* Pop and ignore a possible gl-error when restoring the previous program.
     * It may be that being selected in the gl-context was the last reference
     * to the shader.
     */
    _cairo_gl_get_error(); 
    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_gl_shader_bind_float (cairo_gl_context_t *ctx,
			     GLint location,
			     float value)
{
    cairo_gl_dispatch_t *dispatch = &ctx->dispatch;
    assert (location != -1);
    dispatch->Uniform1f (location, value);
}

void
_cairo_gl_shader_bind_vec2 (cairo_gl_context_t *ctx,
			    GLint location,
			    float value0,
			    float value1)
{
    cairo_gl_dispatch_t *dispatch = &ctx->dispatch;
    assert (location != -1);
    dispatch->Uniform2f (location, value0, value1);
}

void
_cairo_gl_shader_bind_vec3 (cairo_gl_context_t *ctx,
			    GLint location,
			    float value0,
			    float value1,
			    float value2)
{
    cairo_gl_dispatch_t *dispatch = &ctx->dispatch;
    assert (location != -1);
    dispatch->Uniform3f (location, value0, value1, value2);
}

void
_cairo_gl_shader_bind_vec4 (cairo_gl_context_t *ctx,
			    GLint location,
			    float value0, float value1,
			    float value2, float value3)
{
    cairo_gl_dispatch_t *dispatch = &ctx->dispatch;
    assert (location != -1);
    dispatch->Uniform4f (location, value0, value1, value2, value3);
}

void
_cairo_gl_shader_bind_matrix (cairo_gl_context_t *ctx,
			      GLint location,
			      const cairo_matrix_t* m)
{
    cairo_gl_dispatch_t *dispatch = &ctx->dispatch;
    float gl_m[9] = {
	m->xx, m->yx, 0,
	m->xy, m->yy, 0,
	m->x0, m->y0, 1
    };
    assert (location != -1);
    dispatch->UniformMatrix3fv (location, 1, GL_FALSE, gl_m);
}

void
_cairo_gl_shader_bind_matrix4f (cairo_gl_context_t *ctx,
				GLint location, GLfloat* gl_m)
{
    cairo_gl_dispatch_t *dispatch = &ctx->dispatch;
    assert (location != -1);
    dispatch->UniformMatrix4fv (location, 1, GL_FALSE, gl_m);
}

void
_cairo_gl_set_shader (cairo_gl_context_t *ctx,
		      cairo_gl_shader_t *shader)
{
    if (ctx->current_shader == shader)
	return;

    if (shader)
	ctx->dispatch.UseProgram (shader->program);
    else
	ctx->dispatch.UseProgram (0);

    ctx->current_shader = shader;
}

cairo_status_t
_cairo_gl_get_shader_by_type (cairo_gl_context_t *ctx,
			      cairo_gl_operand_t *source,
			      cairo_gl_operand_t *mask,
			      cairo_bool_t use_coverage,
			      cairo_gl_shader_in_t in,
			      cairo_gl_shader_t **shader)
{
    cairo_shader_cache_entry_t lookup, *entry;
    char *fs_source;
    cairo_status_t status;

    lookup.ctx = ctx;

    lookup.vertex = cairo_gl_var_type_hash (cairo_gl_operand_get_var_type (source),
					    cairo_gl_operand_get_var_type (mask),
					    use_coverage,
					    CAIRO_GL_VAR_NONE);

    lookup.src = source->type;
    lookup.mask = mask->type;
    lookup.dest = CAIRO_GL_OPERAND_NONE;
    lookup.use_coverage = use_coverage;
    lookup.in = in;
    lookup.src_gl_filter = _cairo_gl_operand_get_gl_filter (source);
    lookup.src_border_fade = _cairo_gl_shader_needs_border_fade (source);
    lookup.src_extend = _cairo_gl_operand_get_extend (source);
    lookup.mask_gl_filter = _cairo_gl_operand_get_gl_filter (mask);
    lookup.mask_border_fade = _cairo_gl_shader_needs_border_fade (mask);
    lookup.mask_extend = _cairo_gl_operand_get_extend (mask);
    lookup.base.hash = _cairo_gl_shader_cache_hash (&lookup);
    lookup.base.size = 1;

    entry = _cairo_cache_lookup (&ctx->shaders, &lookup.base);
    if (entry) {
	assert (entry->shader.program);
	*shader = &entry->shader;
	return CAIRO_STATUS_SUCCESS;
    }

    status = cairo_gl_shader_get_fragment_source (ctx,
						  in,
						  source,
						  mask,
						  use_coverage,
						  CAIRO_GL_OPERAND_NONE,
						  &fs_source);
    if (unlikely (status))
	return status;

    entry = _cairo_malloc (sizeof (cairo_shader_cache_entry_t));
    if (unlikely (entry == NULL)) {
	free (fs_source);
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    memcpy (entry, &lookup, sizeof (cairo_shader_cache_entry_t));

    entry->ctx = ctx;
    _cairo_gl_shader_init (&entry->shader);
    status = _cairo_gl_shader_compile_and_link (ctx,
						&entry->shader,
						cairo_gl_operand_get_var_type (source),
						cairo_gl_operand_get_var_type (mask),
						use_coverage,
						fs_source);
    free (fs_source);

    if (unlikely (status)) {
	free (entry);
	return status;
    }

    status = _cairo_gl_shader_set_samplers (ctx, &entry->shader);
    if (unlikely (status)) {
	_cairo_gl_shader_fini (ctx, &entry->shader);
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
