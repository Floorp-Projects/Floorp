/* cairo - a vector graphics library with display and print output
 *
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
 * Contributor(s):
 *      Alexandros Frantzis <alexandros.frantzis@linaro.org>
 */

#include "cairoint.h"
#include "cairo-gl-private.h"
#include "cairo-gl-dispatch-private.h"
#if CAIRO_HAS_DLSYM
#include <dlfcn.h>
#endif

#if CAIRO_HAS_DLSYM
static void *
_cairo_gl_dispatch_open_lib (void)
{
    return dlopen (NULL, RTLD_LAZY);
}

static void
_cairo_gl_dispatch_close_lib (void *handle)
{
    dlclose (handle);
}

static cairo_gl_generic_func_t
_cairo_gl_dispatch_get_proc_addr (void *handle, const char *name)
{
    return (cairo_gl_generic_func_t) dlsym (handle, name);
}
#else
static void *
_cairo_gl_dispatch_open_lib (void)
{
    return NULL;
}

static void
_cairo_gl_dispatch_close_lib (void *handle)
{
    return;
}

static cairo_gl_generic_func_t
_cairo_gl_dispatch_get_proc_addr (void *handle, const char *name)
{
    return NULL;
}
#endif /* CAIRO_HAS_DLSYM */


static void
_cairo_gl_dispatch_init_entries (cairo_gl_dispatch_t *dispatch,
				 cairo_gl_get_proc_addr_func_t get_proc_addr,
				 cairo_gl_dispatch_entry_t *entries,
				 cairo_gl_dispatch_name_t dispatch_name)
{
    cairo_gl_dispatch_entry_t *entry = entries;
    void *handle = _cairo_gl_dispatch_open_lib ();

    while (entry->name[CAIRO_GL_DISPATCH_NAME_CORE] != NULL) {
	void *dispatch_ptr = &((char *) dispatch)[entry->offset];
	const char *name = entry->name[dispatch_name];

	/*
	 * In strictly conforming EGL implementations, eglGetProcAddress() can
	 * be used only to get extension functions, but some of the functions
	 * we want belong to core GL(ES). If the *GetProcAddress function
	 * provided by the context fails, try to get the address of the wanted
	 * GL function using standard system facilities (eg dlsym() in *nix
	 * systems).
	 */
	cairo_gl_generic_func_t func = get_proc_addr (name);
	if (func == NULL)
	    func = _cairo_gl_dispatch_get_proc_addr (handle, name);

	*((cairo_gl_generic_func_t *) dispatch_ptr) = func;

	++entry;
    }

    _cairo_gl_dispatch_close_lib (handle);
}

static cairo_status_t
_cairo_gl_dispatch_init_buffers (cairo_gl_dispatch_t *dispatch,
				 cairo_gl_get_proc_addr_func_t get_proc_addr,
				 int gl_version, cairo_gl_flavor_t gl_flavor)
{
    cairo_gl_dispatch_name_t dispatch_name;

    if (gl_flavor == CAIRO_GL_FLAVOR_DESKTOP)
    {
	if (gl_version >= CAIRO_GL_VERSION_ENCODE (1, 5))
	    dispatch_name = CAIRO_GL_DISPATCH_NAME_CORE;
	else if (_cairo_gl_has_extension ("GL_ARB_vertex_buffer_object"))
	    dispatch_name = CAIRO_GL_DISPATCH_NAME_EXT;
	else
	    return CAIRO_STATUS_DEVICE_ERROR;
    }
    else if (gl_flavor == CAIRO_GL_FLAVOR_ES3)
    {
	dispatch_name = CAIRO_GL_DISPATCH_NAME_CORE;
    }
    else if (gl_flavor == CAIRO_GL_FLAVOR_ES2 &&
	     gl_version >= CAIRO_GL_VERSION_ENCODE (2, 0))
    {
	dispatch_name = CAIRO_GL_DISPATCH_NAME_ES;
    }
    else
    {
	return CAIRO_STATUS_DEVICE_ERROR;
    }

    _cairo_gl_dispatch_init_entries (dispatch, get_proc_addr,
				     dispatch_buffers_entries, dispatch_name);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gl_dispatch_init_shaders (cairo_gl_dispatch_t *dispatch,
				 cairo_gl_get_proc_addr_func_t get_proc_addr,
				 int gl_version, cairo_gl_flavor_t gl_flavor)
{
    cairo_gl_dispatch_name_t dispatch_name;

    if (gl_flavor == CAIRO_GL_FLAVOR_DESKTOP)
    {
	if (gl_version >= CAIRO_GL_VERSION_ENCODE (2, 0))
	    dispatch_name = CAIRO_GL_DISPATCH_NAME_CORE;
	else if (_cairo_gl_has_extension ("GL_ARB_shader_objects"))
	    dispatch_name = CAIRO_GL_DISPATCH_NAME_EXT;
	else
	    return CAIRO_STATUS_DEVICE_ERROR;
    }
    else if (gl_flavor == CAIRO_GL_FLAVOR_ES3)
    {
	dispatch_name = CAIRO_GL_DISPATCH_NAME_CORE;
    }
    else if (gl_flavor == CAIRO_GL_FLAVOR_ES2 &&
	     gl_version >= CAIRO_GL_VERSION_ENCODE (2, 0))
    {
	dispatch_name = CAIRO_GL_DISPATCH_NAME_ES;
    }
    else
    {
	return CAIRO_STATUS_DEVICE_ERROR;
    }

    _cairo_gl_dispatch_init_entries (dispatch, get_proc_addr,
				     dispatch_shaders_entries, dispatch_name);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gl_dispatch_init_fbo (cairo_gl_dispatch_t *dispatch,
			     cairo_gl_get_proc_addr_func_t get_proc_addr,
			     int gl_version, cairo_gl_flavor_t gl_flavor)
{
    cairo_gl_dispatch_name_t dispatch_name;

    if (gl_flavor == CAIRO_GL_FLAVOR_DESKTOP)
    {
	if (gl_version >= CAIRO_GL_VERSION_ENCODE (3, 0) ||
	    _cairo_gl_has_extension ("GL_ARB_framebuffer_object"))
	    dispatch_name = CAIRO_GL_DISPATCH_NAME_CORE;
	else if (_cairo_gl_has_extension ("GL_EXT_framebuffer_object"))
	    dispatch_name = CAIRO_GL_DISPATCH_NAME_EXT;
	else
	    return CAIRO_STATUS_DEVICE_ERROR;
    }
    else if (gl_flavor == CAIRO_GL_FLAVOR_ES3)
    {
	dispatch_name = CAIRO_GL_DISPATCH_NAME_CORE;
    }
    else if (gl_flavor == CAIRO_GL_FLAVOR_ES2 &&
	     gl_version >= CAIRO_GL_VERSION_ENCODE (2, 0))
    {
	dispatch_name = CAIRO_GL_DISPATCH_NAME_ES;
    }
    else
    {
	return CAIRO_STATUS_DEVICE_ERROR;
    }

    _cairo_gl_dispatch_init_entries (dispatch, get_proc_addr,
				     dispatch_fbo_entries, dispatch_name);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_gl_dispatch_init_multisampling (cairo_gl_dispatch_t *dispatch,
				       cairo_gl_get_proc_addr_func_t get_proc_addr,
				       int gl_version,
				       cairo_gl_flavor_t gl_flavor)
{
    /* For the multisampling table, there are two GLES versions of the
     * extension, so we put one in the EXT slot and one in the real ES slot.*/
    cairo_gl_dispatch_name_t dispatch_name = CAIRO_GL_DISPATCH_NAME_CORE;
    if (gl_flavor == CAIRO_GL_FLAVOR_ES2) {
	if (_cairo_gl_has_extension ("GL_EXT_multisampled_render_to_texture"))
	    dispatch_name = CAIRO_GL_DISPATCH_NAME_EXT;
	else if (_cairo_gl_has_extension ("GL_IMG_multisampled_render_to_texture"))
	    dispatch_name = CAIRO_GL_DISPATCH_NAME_ES;
    }
    _cairo_gl_dispatch_init_entries (dispatch, get_proc_addr,
				     dispatch_multisampling_entries,
				     dispatch_name);
    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gl_dispatch_init (cairo_gl_dispatch_t *dispatch,
			 cairo_gl_get_proc_addr_func_t get_proc_addr)
{
    cairo_status_t status;
    int gl_version;
    cairo_gl_flavor_t gl_flavor;

    gl_version = _cairo_gl_get_version ();
    gl_flavor = _cairo_gl_get_flavor ();

    status = _cairo_gl_dispatch_init_buffers (dispatch, get_proc_addr,
					      gl_version, gl_flavor);
    if (status != CAIRO_STATUS_SUCCESS)
	return status;

    status = _cairo_gl_dispatch_init_shaders (dispatch, get_proc_addr,
					      gl_version, gl_flavor);
    if (status != CAIRO_STATUS_SUCCESS)
	return status;

    status = _cairo_gl_dispatch_init_fbo (dispatch, get_proc_addr,
					  gl_version, gl_flavor);
    if (status != CAIRO_STATUS_SUCCESS)
	return status;

    status = _cairo_gl_dispatch_init_multisampling (dispatch, get_proc_addr,
						    gl_version, gl_flavor);
    if (status != CAIRO_STATUS_SUCCESS)
	return status;

    return CAIRO_STATUS_SUCCESS;
}
