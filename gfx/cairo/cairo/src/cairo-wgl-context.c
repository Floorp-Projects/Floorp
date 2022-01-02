/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 Eric Anholt
 * Copyright © 2009 Chris Wilson
 * Copyright © 2005 Red Hat, Inc
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
 *	Carl Worth <cworth@cworth.org>
 *	Chris Wilson <chris@chris-wilson.co.uk>
 *	Zoxc <zoxc32@gmail.com>
 */

#include "cairoint.h"

#include "cairo-gl-private.h"

#include "cairo-error-private.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct _cairo_wgl_context {
    cairo_gl_context_t base;

    HDC dummy_dc;
    HWND dummy_wnd;
    HGLRC rc;

    HDC prev_dc;
    HGLRC prev_rc;
} cairo_wgl_context_t;

typedef struct _cairo_wgl_surface {
    cairo_gl_surface_t base;

    HDC dc;
} cairo_wgl_surface_t;

static void
_wgl_acquire (void *abstract_ctx)
{
    cairo_wgl_context_t *ctx = abstract_ctx;

    HDC current_dc;

    ctx->prev_dc = wglGetCurrentDC ();
    ctx->prev_rc = wglGetCurrentContext ();

    if (ctx->base.current_target == NULL ||
        _cairo_gl_surface_is_texture (ctx->base.current_target))
    {
        current_dc = ctx->dummy_dc;
    }
    else
    {
        cairo_wgl_surface_t *surface = (cairo_wgl_surface_t *) ctx->base.current_target;
        current_dc = surface->dc;
    }

    if (ctx->prev_dc != current_dc ||
	(ctx->prev_rc != ctx->rc &&
	 current_dc != ctx->dummy_dc))
    {
        wglMakeCurrent (current_dc, ctx->rc);
    }
}

static void
_wgl_make_current (void *abstract_ctx, cairo_gl_surface_t *abstract_surface)
{
    cairo_wgl_context_t *ctx = abstract_ctx;
    cairo_wgl_surface_t *surface = (cairo_wgl_surface_t *) abstract_surface;

    /* Set the window as the target of our context. */
    wglMakeCurrent (surface->dc, ctx->rc);
}

static void
_wgl_release (void *abstract_ctx)
{
    cairo_wgl_context_t *ctx = abstract_ctx;

    if (ctx->prev_dc != wglGetCurrentDC () ||
        ctx->prev_rc != wglGetCurrentContext ())
    {
        wglMakeCurrent (ctx->prev_dc,
                        ctx->prev_rc);
    }
}

static void
_wgl_swap_buffers (void *abstract_ctx,
		   cairo_gl_surface_t *abstract_surface)
{
    cairo_wgl_surface_t *surface = (cairo_wgl_surface_t *) abstract_surface;

    SwapBuffers (surface->dc);
}

static void
_wgl_destroy (void *abstract_ctx)
{
    cairo_wgl_context_t *ctx = abstract_ctx;

    if (ctx->dummy_dc != 0) {	
        wglMakeCurrent (ctx->dummy_dc, 0);
        ReleaseDC (ctx->dummy_wnd, ctx->dummy_dc);
        DestroyWindow (ctx->dummy_wnd);
    }
}

static cairo_status_t
_wgl_dummy_ctx (cairo_wgl_context_t *ctx)
{
    WNDCLASSEXA wincl;
    PIXELFORMATDESCRIPTOR pfd;
    int format;
    HDC dc;

    ZeroMemory (&wincl, sizeof (WNDCLASSEXA));
    wincl.cbSize = sizeof (WNDCLASSEXA);
    wincl.hInstance = GetModuleHandle (0);
    wincl.lpszClassName = "cairo_wgl_context_dummy";
    wincl.lpfnWndProc = DefWindowProcA;
    wincl.style = CS_OWNDC;

    RegisterClassExA (&wincl);

    ctx->dummy_wnd = CreateWindowA ("cairo_wgl_context_dummy", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    ctx->dummy_dc = GetDC (ctx->dummy_wnd);

    ZeroMemory (&pfd, sizeof (PIXELFORMATDESCRIPTOR));
    pfd.nSize = sizeof (PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cDepthBits = 16;
    pfd.iLayerType = PFD_MAIN_PLANE;

    format = ChoosePixelFormat (ctx->dummy_dc, &pfd);
    SetPixelFormat (ctx->dummy_dc, format, &pfd);

    wglMakeCurrent(ctx->dummy_dc, ctx->rc);

    return CAIRO_STATUS_SUCCESS;
}

cairo_device_t *
cairo_wgl_device_create (HGLRC rc)
{
    cairo_wgl_context_t *ctx;
    cairo_status_t status;

    ctx = calloc (1, sizeof (cairo_wgl_context_t));
    if (unlikely (ctx == NULL))
        return _cairo_gl_context_create_in_error (CAIRO_STATUS_NO_MEMORY);

    ctx->rc = rc;
    ctx->prev_dc = 0;
    ctx->prev_rc = 0;

    status = _wgl_dummy_ctx (ctx);
    if (unlikely (status)) {
        free (ctx);
        return _cairo_gl_context_create_in_error (status);
    }

    ctx->base.acquire = _wgl_acquire;
    ctx->base.release = _wgl_release;
    ctx->base.make_current = _wgl_make_current;
    ctx->base.swap_buffers = _wgl_swap_buffers;
    ctx->base.destroy = _wgl_destroy;

    status = _cairo_gl_dispatch_init (&ctx->base.dispatch,
				      (cairo_gl_get_proc_addr_func_t) wglGetProcAddress);
    if (unlikely (status)) {
	free (ctx);
	return _cairo_gl_context_create_in_error (status);
    }

    status = _cairo_gl_context_init (&ctx->base);
    if (unlikely (status)) {
        free (ctx);
        return _cairo_gl_context_create_in_error (status);
    }

    ctx->base.release (ctx);

    return &ctx->base.base;
}

HGLRC
cairo_wgl_device_get_context (cairo_device_t *device)
{
    cairo_wgl_context_t *ctx;

    if (device->backend->type != CAIRO_DEVICE_TYPE_GL) {
        _cairo_error_throw (CAIRO_STATUS_DEVICE_TYPE_MISMATCH);
        return NULL;
    }

    ctx = (cairo_wgl_context_t *) device;

    return ctx->rc;
}

cairo_surface_t *
cairo_gl_surface_create_for_dc (cairo_device_t	*device,
				HDC			 dc,
				int			 width,
				int			 height)
{
    cairo_wgl_surface_t *surface;

    if (unlikely (device->status))
        return _cairo_surface_create_in_error (device->status);

    if (device->backend->type != CAIRO_DEVICE_TYPE_GL)
        return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH));

    if (width <= 0 || height <= 0)
        return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_SIZE));

    surface = calloc (1, sizeof (cairo_wgl_surface_t));
    if (unlikely (surface == NULL))
        return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_gl_surface_init (device, &surface->base,
			    CAIRO_CONTENT_COLOR_ALPHA, width, height);
    surface->dc = dc;

    return &surface->base.base;
}
