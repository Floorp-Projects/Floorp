/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 Eric Anholt
 * Copyright © 2009 Chris Wilson
 * Copyright © 2005,2010 Red Hat, Inc
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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *	Benjamin Otte <otte@gnome.org>
 *	Carl Worth <cworth@cworth.org>
 *	Chris Wilson <chris@chris-wilson.co.uk>
 *	Eric Anholt <eric@anholt.net>
 *	Alexandros Frantzis <alexandros.frantzis@linaro.org>
 */

#include "cairoint.h"

#include "cairo-error-private.h"
#include "cairo-gl-private.h"

#define MAX_MSAA_SAMPLES 4

static void
_gl_lock (void *device)
{
    cairo_gl_context_t *ctx = (cairo_gl_context_t *) device;

    ctx->acquire (ctx);
}

static void
_gl_unlock (void *device)
{
    cairo_gl_context_t *ctx = (cairo_gl_context_t *) device;

    ctx->release (ctx);
}

static cairo_status_t
_gl_flush (void *device)
{
    cairo_gl_context_t *ctx;
    cairo_status_t status;

    status = _cairo_gl_context_acquire (device, &ctx);
    if (unlikely (status))
	return status;

    _cairo_gl_composite_flush (ctx);

    _cairo_gl_context_destroy_operand (ctx, CAIRO_GL_TEX_SOURCE);
    _cairo_gl_context_destroy_operand (ctx, CAIRO_GL_TEX_MASK);

    if (ctx->clip_region) {
	cairo_region_destroy (ctx->clip_region);
	ctx->clip_region = NULL;
    }

    ctx->current_target = NULL;
    ctx->current_operator = -1;
    ctx->vertex_size = 0;
    ctx->pre_shader = NULL;
    _cairo_gl_set_shader (ctx, NULL);

    ctx->dispatch.BindBuffer (GL_ARRAY_BUFFER, 0);

    glDisable (GL_SCISSOR_TEST);
    glDisable (GL_BLEND);

    return _cairo_gl_context_release (ctx, status);
}

static void
_gl_finish (void *device)
{
    cairo_gl_context_t *ctx = device;
    int n;

    _gl_lock (device);

    _cairo_cache_fini (&ctx->gradients);

    _cairo_gl_context_fini_shaders (ctx);

    for (n = 0; n < ARRAY_LENGTH (ctx->glyph_cache); n++)
	_cairo_gl_glyph_cache_fini (ctx, &ctx->glyph_cache[n]);

    _gl_unlock (device);
}

static void
_gl_destroy (void *device)
{
    cairo_gl_context_t *ctx = device;

    ctx->acquire (ctx);

    while (! cairo_list_is_empty (&ctx->fonts)) {
	cairo_gl_font_t *font;

	font = cairo_list_first_entry (&ctx->fonts,
				       cairo_gl_font_t,
				       link);

	cairo_list_del (&font->base.link);
	cairo_list_del (&font->link);
	free (font);
    }

    _cairo_array_fini (&ctx->tristrip_indices);

    cairo_region_destroy (ctx->clip_region);
    _cairo_clip_destroy (ctx->clip);

    free (ctx->vb);

    ctx->destroy (ctx);

    free (ctx);
}

static const cairo_device_backend_t _cairo_gl_device_backend = {
    CAIRO_DEVICE_TYPE_GL,

    _gl_lock,
    _gl_unlock,

    _gl_flush, /* flush */
    _gl_finish,
    _gl_destroy,
};

static cairo_bool_t
_cairo_gl_msaa_compositor_enabled (void)
{
    const char *env = getenv ("CAIRO_GL_COMPOSITOR");
    return env && strcmp(env, "msaa") == 0;
}

static cairo_bool_t
test_can_read_bgra (cairo_gl_flavor_t gl_flavor)
{
    /* Desktop GL always supports BGRA formats. */
    if (gl_flavor == CAIRO_GL_FLAVOR_DESKTOP)
	return TRUE;

    assert (gl_flavor == CAIRO_GL_FLAVOR_ES3 ||
	    gl_flavor == CAIRO_GL_FLAVOR_ES2);

   /* For OpenGL ES we have to look for the specific extension and BGRA only
    * matches cairo's integer packed bytes on little-endian machines. */
    if (!_cairo_is_little_endian())
	return FALSE;
    return _cairo_gl_has_extension ("EXT_read_format_bgra");
}

cairo_status_t
_cairo_gl_context_init (cairo_gl_context_t *ctx)
{
    cairo_status_t status;
    cairo_gl_dispatch_t *dispatch = &ctx->dispatch;
    int gl_version = _cairo_gl_get_version ();
    cairo_gl_flavor_t gl_flavor = _cairo_gl_get_flavor ();
    int n;

    cairo_bool_t is_desktop = gl_flavor == CAIRO_GL_FLAVOR_DESKTOP;
    cairo_bool_t is_gles = (gl_flavor == CAIRO_GL_FLAVOR_ES3 ||
			    gl_flavor == CAIRO_GL_FLAVOR_ES2);

    _cairo_device_init (&ctx->base, &_cairo_gl_device_backend);

    /* XXX The choice of compositor should be made automatically at runtime.
     * However, it is useful to force one particular compositor whilst
     * testing.
     */
     if (_cairo_gl_msaa_compositor_enabled ())
	ctx->compositor = _cairo_gl_msaa_compositor_get ();
    else
	ctx->compositor = _cairo_gl_span_compositor_get ();


    ctx->thread_aware = TRUE;

    memset (ctx->glyph_cache, 0, sizeof (ctx->glyph_cache));
    cairo_list_init (&ctx->fonts);

    /* Support only GL version >= 1.3 */
    if (gl_version < CAIRO_GL_VERSION_ENCODE (1, 3))
	return _cairo_error (CAIRO_STATUS_DEVICE_ERROR);

    /* Check for required extensions */
    if (is_desktop) {
	if (_cairo_gl_has_extension ("GL_ARB_texture_non_power_of_two")) {
	    ctx->tex_target = GL_TEXTURE_2D;
	    ctx->has_npot_repeat = TRUE;
	} else if (_cairo_gl_has_extension ("GL_ARB_texture_rectangle")) {
	    ctx->tex_target = GL_TEXTURE_RECTANGLE;
	    ctx->has_npot_repeat = FALSE;
	} else
	    return _cairo_error (CAIRO_STATUS_DEVICE_ERROR);
    } else {
	ctx->tex_target = GL_TEXTURE_2D;
	if (_cairo_gl_has_extension ("GL_OES_texture_npot") ||
	    _cairo_gl_has_extension ("GL_IMG_texture_npot"))
	    ctx->has_npot_repeat = TRUE;
	else
	    ctx->has_npot_repeat = FALSE;
    }

    if (is_desktop && gl_version < CAIRO_GL_VERSION_ENCODE (2, 1) &&
	! _cairo_gl_has_extension ("GL_ARB_pixel_buffer_object"))
	return _cairo_error (CAIRO_STATUS_DEVICE_ERROR);

    if (is_gles && ! _cairo_gl_has_extension ("GL_EXT_texture_format_BGRA8888"))
	return _cairo_error (CAIRO_STATUS_DEVICE_ERROR);

    ctx->has_map_buffer =
	is_desktop || (is_gles && _cairo_gl_has_extension ("GL_OES_mapbuffer"));

    ctx->can_read_bgra = test_can_read_bgra (gl_flavor);

    ctx->has_mesa_pack_invert =
	_cairo_gl_has_extension ("GL_MESA_pack_invert");

    ctx->has_packed_depth_stencil =
	(is_desktop && _cairo_gl_has_extension ("GL_EXT_packed_depth_stencil")) ||
	(is_gles && _cairo_gl_has_extension ("GL_OES_packed_depth_stencil"));

    ctx->num_samples = 1;

#if CAIRO_HAS_GL_SURFACE
    if (is_desktop && ctx->has_packed_depth_stencil &&
	(gl_version >= CAIRO_GL_VERSION_ENCODE (3, 0) ||
	 _cairo_gl_has_extension ("GL_ARB_framebuffer_object") ||
	 (_cairo_gl_has_extension ("GL_EXT_framebuffer_blit") &&
	  _cairo_gl_has_extension ("GL_EXT_framebuffer_multisample")))) {
	glGetIntegerv(GL_MAX_SAMPLES_EXT, &ctx->num_samples);
    }
#endif

#if CAIRO_HAS_GLESV3_SURFACE
    if (is_gles && ctx->has_packed_depth_stencil) {
	glGetIntegerv(GL_MAX_SAMPLES, &ctx->num_samples);
    }

#elif CAIRO_HAS_GLESV2_SURFACE && defined(GL_MAX_SAMPLES_EXT)
    if (is_gles && ctx->has_packed_depth_stencil &&
	_cairo_gl_has_extension ("GL_EXT_multisampled_render_to_texture")) {
	glGetIntegerv(GL_MAX_SAMPLES_EXT, &ctx->num_samples);
    }

    if (is_gles && ctx->has_packed_depth_stencil &&
	_cairo_gl_has_extension ("GL_IMG_multisampled_render_to_texture")) {
	glGetIntegerv(GL_MAX_SAMPLES_IMG, &ctx->num_samples);
    }
#endif

    /* we always use renderbuffer for rendering in glesv3 */
    if (ctx->gl_flavor == CAIRO_GL_FLAVOR_ES3)
	ctx->supports_msaa = TRUE;
    else
	ctx->supports_msaa = ctx->num_samples > 1;
    if (ctx->num_samples > MAX_MSAA_SAMPLES)
	ctx->num_samples = MAX_MSAA_SAMPLES;

    ctx->current_operator = -1;
    ctx->gl_flavor = gl_flavor;

    status = _cairo_gl_context_init_shaders (ctx);
    if (unlikely (status))
	return status;

    status = _cairo_cache_init (&ctx->gradients,
				_cairo_gl_gradient_equal,
				NULL,
				(cairo_destroy_func_t) _cairo_gl_gradient_destroy,
				CAIRO_GL_GRADIENT_CACHE_SIZE);
    if (unlikely (status))
	return status;

    ctx->vbo_size = _cairo_gl_get_vbo_size();

    ctx->vb = _cairo_malloc (ctx->vbo_size);
    if (unlikely (ctx->vb == NULL)) {
	    _cairo_cache_fini (&ctx->gradients);
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    ctx->primitive_type = CAIRO_GL_PRIMITIVE_TYPE_TRIANGLES;
    _cairo_array_init (&ctx->tristrip_indices, sizeof (unsigned short));

    /* PBO for any sort of texture upload */
    dispatch->GenBuffers (1, &ctx->texture_load_pbo);

    ctx->max_framebuffer_size = 0;
    glGetIntegerv (GL_MAX_RENDERBUFFER_SIZE, &ctx->max_framebuffer_size);
    ctx->max_texture_size = 0;
    glGetIntegerv (GL_MAX_TEXTURE_SIZE, &ctx->max_texture_size);
    ctx->max_textures = 0;
    glGetIntegerv (GL_MAX_TEXTURE_IMAGE_UNITS, &ctx->max_textures);

    for (n = 0; n < ARRAY_LENGTH (ctx->glyph_cache); n++)
	_cairo_gl_glyph_cache_init (&ctx->glyph_cache[n]);

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_gl_context_activate (cairo_gl_context_t *ctx,
			    cairo_gl_tex_t      tex_unit)
{
    if (ctx->max_textures <= (GLint) tex_unit) {
	if (tex_unit < 2) {
	    _cairo_gl_composite_flush (ctx);
	    _cairo_gl_context_destroy_operand (ctx, ctx->max_textures - 1);
	}
	glActiveTexture (ctx->max_textures - 1);
    } else {
	glActiveTexture (GL_TEXTURE0 + tex_unit);
    }
}

static GLenum
_get_depth_stencil_format (cairo_gl_context_t *ctx)
{
    /* This is necessary to properly handle the situation where both
       OpenGL and OpenGLES are active and returning a sane default. */
#if CAIRO_HAS_GL_SURFACE
    if (ctx->gl_flavor == CAIRO_GL_FLAVOR_DESKTOP)
	return GL_DEPTH_STENCIL;
#endif

#if CAIRO_HAS_GLESV2_SURFACE && !CAIRO_HAS_GLESV3_SURFACE
    if (ctx->gl_flavor == CAIRO_GL_FLAVOR_DESKTOP)
	return GL_DEPTH24_STENCIL8_OES;
#endif

#if CAIRO_HAS_GL_SURFACE
    return GL_DEPTH_STENCIL;
#elif CAIRO_HAS_GLESV3_SURFACE
    return GL_DEPTH24_STENCIL8;
#elif CAIRO_HAS_GLESV2_SURFACE
    return GL_DEPTH24_STENCIL8_OES;
#endif
}

#if CAIRO_HAS_GLESV2_SURFACE
static void
_cairo_gl_ensure_msaa_gles_framebuffer (cairo_gl_context_t *ctx,
					cairo_gl_surface_t *surface)
{
    if (surface->msaa_active)
	return;

    ctx->dispatch.FramebufferTexture2DMultisample(GL_FRAMEBUFFER,
						  GL_COLOR_ATTACHMENT0,
						  ctx->tex_target,
						  surface->tex,
						  0,
						  ctx->num_samples);

    /* From now on MSAA will always be active on this surface. */
    surface->msaa_active = TRUE;
}
#endif

void
_cairo_gl_ensure_framebuffer (cairo_gl_context_t *ctx,
			      cairo_gl_surface_t *surface)
{
    GLenum status;
    cairo_gl_dispatch_t *dispatch = &ctx->dispatch;

    if (likely (surface->fb))
	return;

    /* Create a framebuffer object wrapping the texture so that we can render
     * to it.
     */
    dispatch->GenFramebuffers (1, &surface->fb);
    dispatch->BindFramebuffer (GL_FRAMEBUFFER, surface->fb);

    /* Unlike for desktop GL we only maintain one multisampling framebuffer
       for OpenGLES since the EXT_multisampled_render_to_texture extension
       does not require an explicit multisample resolution. */
#if CAIRO_HAS_GLESV2_SURFACE
    if (surface->supports_msaa && _cairo_gl_msaa_compositor_enabled () &&
	ctx->gl_flavor == CAIRO_GL_FLAVOR_ES2) {
	_cairo_gl_ensure_msaa_gles_framebuffer (ctx, surface);
    } else
#endif
	dispatch->FramebufferTexture2D (GL_FRAMEBUFFER,
					GL_COLOR_ATTACHMENT0,
					ctx->tex_target,
					surface->tex,
					0);

#if CAIRO_HAS_GL_SURFACE
    glDrawBuffer (GL_COLOR_ATTACHMENT0);
    glReadBuffer (GL_COLOR_ATTACHMENT0);
#endif

    status = dispatch->CheckFramebufferStatus (GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
	const char *str;
	switch (status) {
	//case GL_FRAMEBUFFER_UNDEFINED: str= "undefined"; break;
	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: str= "incomplete attachment"; break;
	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: str= "incomplete/missing attachment"; break;
	case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER: str= "incomplete draw buffer"; break;
	case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER: str= "incomplete read buffer"; break;
	case GL_FRAMEBUFFER_UNSUPPORTED: str= "unsupported"; break;
	case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: str= "incomplete multiple"; break;
	default: str = "unknown error"; break;
	}

	fprintf (stderr,
		 "destination is framebuffer incomplete: %s [%#x]\n",
		 str, status);
    }
}
#if CAIRO_HAS_GL_SURFACE || CAIRO_HAS_GLESV3_SURFACE
static void
_cairo_gl_ensure_multisampling (cairo_gl_context_t *ctx,
				cairo_gl_surface_t *surface)
{
    assert (surface->supports_msaa);
    assert (ctx->gl_flavor == CAIRO_GL_FLAVOR_DESKTOP ||
	ctx->gl_flavor == CAIRO_GL_FLAVOR_ES3);

    if (surface->msaa_fb)
	return;

    /* We maintain a separate framebuffer for multisampling operations.
       This allows us to do a fast paint to the non-multisampling framebuffer
       when mulitsampling is disabled. */
    ctx->dispatch.GenFramebuffers (1, &surface->msaa_fb);
    ctx->dispatch.BindFramebuffer (GL_FRAMEBUFFER, surface->msaa_fb);
    ctx->dispatch.GenRenderbuffers (1, &surface->msaa_rb);
    ctx->dispatch.BindRenderbuffer (GL_RENDERBUFFER, surface->msaa_rb);

    /* FIXME: For now we assume that textures passed from the outside have GL_RGBA
       format, but eventually we need to expose a way for the API consumer to pass
       this information. */
    ctx->dispatch.RenderbufferStorageMultisample (GL_RENDERBUFFER,
						  ctx->num_samples,
#if CAIRO_HAS_GLESV3_SURFACE
						  GL_RGBA8,
#else
						  GL_RGBA,
#endif
						  surface->width,
						  surface->height);
    ctx->dispatch.FramebufferRenderbuffer (GL_FRAMEBUFFER,
					   GL_COLOR_ATTACHMENT0,
					   GL_RENDERBUFFER,
					   surface->msaa_rb);

    /* Cairo surfaces start out initialized to transparent (black) */
    glDisable (GL_SCISSOR_TEST);
    glClearColor (0, 0, 0, 0);
    glClear (GL_COLOR_BUFFER_BIT);

    /* for glesv3 with multisample renderbuffer, we always render to
       this renderbuffer */
    if (ctx->gl_flavor == CAIRO_GL_FLAVOR_ES3)
	surface->msaa_active = TRUE;
}
#endif

static cairo_bool_t
_cairo_gl_ensure_msaa_depth_stencil_buffer (cairo_gl_context_t *ctx,
					    cairo_gl_surface_t *surface)
{
    cairo_gl_dispatch_t *dispatch = &ctx->dispatch;
    if (surface->msaa_depth_stencil)
	return TRUE;

    _cairo_gl_ensure_framebuffer (ctx, surface);
#if CAIRO_HAS_GL_SURFACE || CAIRO_HAS_GLESV3_SURFACE
    if (ctx->gl_flavor == CAIRO_GL_FLAVOR_DESKTOP ||
	ctx->gl_flavor == CAIRO_GL_FLAVOR_ES3)
	_cairo_gl_ensure_multisampling (ctx, surface);
#endif

    dispatch->GenRenderbuffers (1, &surface->msaa_depth_stencil);
    dispatch->BindRenderbuffer (GL_RENDERBUFFER,
				surface->msaa_depth_stencil);

    dispatch->RenderbufferStorageMultisample (GL_RENDERBUFFER,
					      ctx->num_samples,
					      _get_depth_stencil_format (ctx),
					      surface->width,
					      surface->height);

#if CAIRO_HAS_GL_SURFACE || CAIRO_HAS_GLESV3_SURFACE
    if (ctx->gl_flavor == CAIRO_GL_FLAVOR_DESKTOP ||
	ctx->gl_flavor == CAIRO_GL_FLAVOR_ES3) {
	dispatch->FramebufferRenderbuffer (GL_FRAMEBUFFER,
					   GL_DEPTH_STENCIL_ATTACHMENT,
					   GL_RENDERBUFFER,
					   surface->msaa_depth_stencil);
    }
#endif

#if CAIRO_HAS_GLESV2_SURFACE
    if (ctx->gl_flavor == CAIRO_GL_FLAVOR_ES2) {
	dispatch->FramebufferRenderbuffer (GL_FRAMEBUFFER,
					   GL_DEPTH_ATTACHMENT,
					   GL_RENDERBUFFER,
					   surface->msaa_depth_stencil);
	dispatch->FramebufferRenderbuffer (GL_FRAMEBUFFER,
					   GL_STENCIL_ATTACHMENT,
					   GL_RENDERBUFFER,
					   surface->msaa_depth_stencil);
    }
#endif

    if (dispatch->CheckFramebufferStatus (GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
	dispatch->DeleteRenderbuffers (1, &surface->msaa_depth_stencil);
	surface->msaa_depth_stencil = 0;
	return FALSE;
    }

    return TRUE;
}

static cairo_bool_t
_cairo_gl_ensure_depth_stencil_buffer (cairo_gl_context_t *ctx,
				       cairo_gl_surface_t *surface)
{
    cairo_gl_dispatch_t *dispatch = &ctx->dispatch;

    if (surface->depth_stencil)
	return TRUE;

    _cairo_gl_ensure_framebuffer (ctx, surface);

    dispatch->GenRenderbuffers (1, &surface->depth_stencil);
    dispatch->BindRenderbuffer (GL_RENDERBUFFER, surface->depth_stencil);
    dispatch->RenderbufferStorage (GL_RENDERBUFFER,
				   _get_depth_stencil_format (ctx),
				   surface->width, surface->height);

    dispatch->FramebufferRenderbuffer (GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
				       GL_RENDERBUFFER, surface->depth_stencil);
    dispatch->FramebufferRenderbuffer (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
				       GL_RENDERBUFFER, surface->depth_stencil);
    if (dispatch->CheckFramebufferStatus (GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
	dispatch->DeleteRenderbuffers (1, &surface->depth_stencil);
	surface->depth_stencil = 0;
	return FALSE;
    }

    return TRUE;
}

cairo_bool_t
_cairo_gl_ensure_stencil (cairo_gl_context_t *ctx,
			  cairo_gl_surface_t *surface)
{
    if (! _cairo_gl_surface_is_texture (surface))
	return TRUE; /* best guess for now, will check later */
    if (! ctx->has_packed_depth_stencil)
	return FALSE;

    if (surface->msaa_active)
	return _cairo_gl_ensure_msaa_depth_stencil_buffer (ctx, surface);
    else
	return _cairo_gl_ensure_depth_stencil_buffer (ctx, surface);
}

/*
 * Stores a parallel projection transformation in matrix 'm',
 * using column-major order.
 *
 * This is equivalent to:
 *
 * glLoadIdentity()
 * gluOrtho2D()
 *
 * The calculation for the ortho transformation was taken from the
 * mesa source code.
 */
static void
_gl_identity_ortho (GLfloat *m,
		    GLfloat left, GLfloat right,
		    GLfloat bottom, GLfloat top)
{
#define M(row,col)  m[col*4+row]
    M(0,0) = 2.f / (right - left);
    M(0,1) = 0.f;
    M(0,2) = 0.f;
    M(0,3) = -(right + left) / (right - left);

    M(1,0) = 0.f;
    M(1,1) = 2.f / (top - bottom);
    M(1,2) = 0.f;
    M(1,3) = -(top + bottom) / (top - bottom);

    M(2,0) = 0.f;
    M(2,1) = 0.f;
    M(2,2) = -1.f;
    M(2,3) = 0.f;

    M(3,0) = 0.f;
    M(3,1) = 0.f;
    M(3,2) = 0.f;
    M(3,3) = 1.f;
#undef M
}

#if CAIRO_HAS_GL_SURFACE || CAIRO_HAS_GLESV3_SURFACE
static void
bind_multisample_framebuffer (cairo_gl_context_t *ctx,
			       cairo_gl_surface_t *surface)
{
    cairo_bool_t stencil_test_enabled;
    cairo_bool_t scissor_test_enabled;

    assert (surface->supports_msaa);
    assert (ctx->gl_flavor == CAIRO_GL_FLAVOR_DESKTOP ||
	ctx->gl_flavor == CAIRO_GL_FLAVOR_ES3);

    _cairo_gl_ensure_framebuffer (ctx, surface);
    _cairo_gl_ensure_multisampling (ctx, surface);

    if (surface->msaa_active) {
#if CAIRO_HAS_GL_SURFACE
	glEnable (GL_MULTISAMPLE);
#endif
	ctx->dispatch.BindFramebuffer (GL_FRAMEBUFFER, surface->msaa_fb);
	if (ctx->gl_flavor == CAIRO_GL_FLAVOR_ES3)
	    surface->content_in_texture = FALSE;
	return;
    }

    _cairo_gl_composite_flush (ctx);

    stencil_test_enabled = glIsEnabled (GL_STENCIL_TEST);
    scissor_test_enabled = glIsEnabled (GL_SCISSOR_TEST);
    glDisable (GL_STENCIL_TEST);
    glDisable (GL_SCISSOR_TEST);

#if CAIRO_HAS_GL_SURFACE
    glEnable (GL_MULTISAMPLE);
#endif

    /* The last time we drew to the surface, we were not using multisampling,
       so we need to blit from the non-multisampling framebuffer into the
       multisampling framebuffer. */
    ctx->dispatch.BindFramebuffer (GL_DRAW_FRAMEBUFFER, surface->msaa_fb);
    ctx->dispatch.BindFramebuffer (GL_READ_FRAMEBUFFER, surface->fb);
    ctx->dispatch.BlitFramebuffer (0, 0, surface->width, surface->height,
				   0, 0, surface->width, surface->height,
				   GL_COLOR_BUFFER_BIT
#if CAIRO_HAS_GL_SURFACE
				   | GL_STENCIL_BUFFER_BIT
#endif
				   ,
				   GL_NEAREST);
    ctx->dispatch.BindFramebuffer (GL_FRAMEBUFFER, surface->msaa_fb);

    if (stencil_test_enabled)
	glEnable (GL_STENCIL_TEST);
    if (scissor_test_enabled)
	glEnable (GL_SCISSOR_TEST);
    if (ctx->gl_flavor == CAIRO_GL_FLAVOR_ES3)
	surface->content_in_texture = FALSE;
}
#endif

#if CAIRO_HAS_GL_SURFACE || CAIRO_HAS_GLESV3_SURFACE
static void
bind_singlesample_framebuffer (cairo_gl_context_t *ctx,
			       cairo_gl_surface_t *surface)
{
    cairo_bool_t stencil_test_enabled;
    cairo_bool_t scissor_test_enabled;

    assert (ctx->gl_flavor == CAIRO_GL_FLAVOR_DESKTOP ||
	ctx->gl_flavor == CAIRO_GL_FLAVOR_ES3);
    _cairo_gl_ensure_framebuffer (ctx, surface);

    if (! surface->msaa_active) {
#if CAIRO_HAS_GL_SURFACE
	glDisable (GL_MULTISAMPLE);
#endif

	ctx->dispatch.BindFramebuffer (GL_FRAMEBUFFER, surface->fb);
	return;
    }

    _cairo_gl_composite_flush (ctx);

    stencil_test_enabled = glIsEnabled (GL_STENCIL_TEST);
    scissor_test_enabled = glIsEnabled (GL_SCISSOR_TEST);
    glDisable (GL_STENCIL_TEST);
    glDisable (GL_SCISSOR_TEST);

#if CAIRO_HAS_GL_SURFACE
    glDisable (GL_MULTISAMPLE);
#endif

    /* The last time we drew to the surface, we were using multisampling,
       so we need to blit from the multisampling framebuffer into the
       non-multisampling framebuffer. */
    ctx->dispatch.BindFramebuffer (GL_DRAW_FRAMEBUFFER, surface->fb);
    ctx->dispatch.BindFramebuffer (GL_READ_FRAMEBUFFER, surface->msaa_fb);
    ctx->dispatch.BlitFramebuffer (0, 0, surface->width, surface->height,
				   0, 0, surface->width, surface->height,
				   GL_COLOR_BUFFER_BIT, GL_NEAREST);
    ctx->dispatch.BindFramebuffer (GL_FRAMEBUFFER, surface->fb);

    if (stencil_test_enabled)
	glEnable (GL_STENCIL_TEST);
    if (scissor_test_enabled)
	glEnable (GL_SCISSOR_TEST);
}
#endif

void
_cairo_gl_context_bind_framebuffer (cairo_gl_context_t *ctx,
				    cairo_gl_surface_t *surface,
				    cairo_bool_t multisampling)
{
    if (_cairo_gl_surface_is_texture (surface)) {
	/* OpenGL ES surfaces only have either a multisample framebuffer or a
	 * singlesample framebuffer, so we cannot switch back and forth. */
	if (ctx->gl_flavor == CAIRO_GL_FLAVOR_ES2) {
	    _cairo_gl_ensure_framebuffer (ctx, surface);
	    ctx->dispatch.BindFramebuffer (GL_FRAMEBUFFER, surface->fb);
	    return;
	}

#if CAIRO_HAS_GL_SURFACE || CAIRO_HAS_GLESV3_SURFACE
	if (multisampling)
	    bind_multisample_framebuffer (ctx, surface);
	else
	    bind_singlesample_framebuffer (ctx, surface);
#endif
    } else {
	ctx->dispatch.BindFramebuffer (GL_FRAMEBUFFER, 0);

#if CAIRO_HAS_GL_SURFACE
	if (ctx->gl_flavor == CAIRO_GL_FLAVOR_DESKTOP) {
	    if (multisampling)
		glEnable (GL_MULTISAMPLE);
	    else
		glDisable (GL_MULTISAMPLE);
	}
#endif
    }

    if (ctx->gl_flavor == CAIRO_GL_FLAVOR_DESKTOP)
	surface->msaa_active = multisampling;
}

void
_cairo_gl_context_set_destination (cairo_gl_context_t *ctx,
				   cairo_gl_surface_t *surface,
				   cairo_bool_t multisampling)
{
    cairo_bool_t changing_surface, changing_sampling;

    /* The decision whether or not to use multisampling happens when
     * we create an OpenGL ES surface, so we can never switch modes. */
    if (ctx->gl_flavor == CAIRO_GL_FLAVOR_ES2)
	multisampling = surface->msaa_active;
    /* For GLESV3, we always use renderbuffer for drawing */
    else if (ctx->gl_flavor == CAIRO_GL_FLAVOR_ES3)
	multisampling = TRUE;

    changing_surface = ctx->current_target != surface || surface->needs_update;
    changing_sampling = (surface->msaa_active != multisampling ||
			 surface->content_in_texture);
    if (! changing_surface && ! changing_sampling)
	return;

    if (! changing_surface) {
	_cairo_gl_composite_flush (ctx);
	_cairo_gl_context_bind_framebuffer (ctx, surface, multisampling);
	return;
    }

    _cairo_gl_composite_flush (ctx);

    ctx->current_target = surface;
    surface->needs_update = FALSE;

    if (! _cairo_gl_surface_is_texture (surface)) {
	ctx->make_current (ctx, surface);
    }

    _cairo_gl_context_bind_framebuffer (ctx, surface, multisampling);

    if (! _cairo_gl_surface_is_texture (surface)) {
#if CAIRO_HAS_GL_SURFACE
	glDrawBuffer (GL_BACK_LEFT);
	glReadBuffer (GL_BACK_LEFT);
#endif
    }

    glDisable (GL_DITHER);
    glViewport (0, 0, surface->width, surface->height);

    if (_cairo_gl_surface_is_texture (surface))
	_gl_identity_ortho (ctx->modelviewprojection_matrix,
			    0, surface->width, 0, surface->height);
    else
	_gl_identity_ortho (ctx->modelviewprojection_matrix,
			    0, surface->width, surface->height, 0);
}

void
cairo_gl_device_set_thread_aware (cairo_device_t	*device,
				  cairo_bool_t		 thread_aware)
{
    if (device->backend->type != CAIRO_DEVICE_TYPE_GL) {
	_cairo_error_throw (CAIRO_STATUS_DEVICE_TYPE_MISMATCH);
	return;
    }
    ((cairo_gl_context_t *) device)->thread_aware = thread_aware;
}
