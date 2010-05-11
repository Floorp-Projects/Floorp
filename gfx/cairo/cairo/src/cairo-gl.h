/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 Eric Anholt
 * Copyright © 2009 Chris Wilson
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
 * The Initial Developer of the Original Code is Eric Anholt.
 */

#ifndef CAIRO_GL_H
#define CAIRO_GL_H

#include "cairo.h"

#if CAIRO_HAS_GL_SURFACE

CAIRO_BEGIN_DECLS

typedef struct _cairo_gl_context cairo_gl_context_t;

cairo_public cairo_gl_context_t *
cairo_gl_context_reference (cairo_gl_context_t *context);

cairo_public void
cairo_gl_context_destroy (cairo_gl_context_t *context);

cairo_public cairo_surface_t *
cairo_gl_surface_create (cairo_gl_context_t *ctx,
			 cairo_content_t content,
			 int width, int height);

cairo_public void
cairo_gl_surface_set_size (cairo_surface_t *surface, int width, int height);

cairo_public int
cairo_gl_surface_get_width (cairo_surface_t *abstract_surface);

cairo_public int
cairo_gl_surface_get_height (cairo_surface_t *abstract_surface);

cairo_public void
cairo_gl_surface_swapbuffers (cairo_surface_t *surface);

cairo_public cairo_status_t
cairo_gl_surface_glfinish (cairo_surface_t *surface);

#if CAIRO_HAS_GLX_FUNCTIONS
#include <GL/glx.h>

cairo_public cairo_gl_context_t *
cairo_glx_context_create (Display *dpy, GLXContext gl_ctx);

cairo_public cairo_surface_t *
cairo_gl_surface_create_for_window (cairo_gl_context_t *ctx,
				    Window win,
				    int width, int height);
#endif

#if CAIRO_HAS_EAGLE_FUNCTIONS
#include <eagle.h>

cairo_public cairo_gl_context_t *
cairo_eagle_context_create (EGLDisplay display, EGLContext context);

cairo_public cairo_surface_t *
cairo_gl_surface_create_for_eagle (cairo_gl_context_t *ctx,
				   EGLSurface surface,
				   int width, int height);
#endif

CAIRO_END_DECLS

#else  /* CAIRO_HAS_GL_SURFACE */
# error Cairo was not compiled with support for the GL backend
#endif /* CAIRO_HAS_GL_SURFACE */

#endif /* CAIRO_GL_H */
