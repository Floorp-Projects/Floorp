/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2010 Eric Anholt
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

#include "cairoint.h"
#include "cairo-gl-private.h"

static GLint
_cairo_gl_compile_glsl(GLenum type, GLint *shader_out, const char *source)
{
    GLint ok;
    GLint shader;

    shader = glCreateShaderObjectARB (type);
    glShaderSourceARB (shader, 1, (const GLchar **)&source, NULL);
    glCompileShaderARB (shader);
    glGetObjectParameterivARB (shader, GL_OBJECT_COMPILE_STATUS_ARB, &ok);
    if (!ok) {
	GLchar *info;
	GLint size;

	glGetObjectParameterivARB (shader, GL_OBJECT_INFO_LOG_LENGTH_ARB,
				   &size);
	info = malloc (size);

	if (info)
	    glGetInfoLogARB (shader, size, NULL, info);
	fprintf (stderr, "Failed to compile %s: %s\n",
		 type == GL_FRAGMENT_SHADER ? "FS" : "VS",
		 info);
	fprintf (stderr, "Shader source:\n%s", source);
	fprintf (stderr, "GLSL compile failure\n");

	glDeleteObjectARB (shader);

	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    *shader_out = shader;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gl_load_glsl (GLint *shader_out,
		     const char *vs_source, const char *fs_source)
{
    GLint ok;
    GLint shader, vs, fs;
    cairo_status_t status;

    shader = glCreateProgramObjectARB ();

    status = _cairo_gl_compile_glsl (GL_VERTEX_SHADER_ARB, &vs, vs_source);
    if (_cairo_status_is_error (status))
	goto fail;
    status = _cairo_gl_compile_glsl (GL_FRAGMENT_SHADER_ARB, &fs, fs_source);
    if (_cairo_status_is_error (status))
	goto fail;

    glAttachObjectARB (shader, vs);
    glAttachObjectARB (shader, fs);
    glLinkProgram (shader);
    glGetObjectParameterivARB (shader, GL_OBJECT_LINK_STATUS_ARB, &ok);
    if (!ok) {
	GLchar *info;
	GLint size;

	glGetObjectParameterivARB (shader, GL_OBJECT_INFO_LOG_LENGTH_ARB,
				   &size);
	info = malloc (size);

	if (info)
	    glGetInfoLogARB (shader, size, NULL, info);
	fprintf (stderr, "Failed to link: %s\n", info);
	free (info);
	status = CAIRO_INT_STATUS_UNSUPPORTED;

	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    *shader_out = shader;

    return CAIRO_STATUS_SUCCESS;

fail:
    glDeleteObjectARB (shader);
    return status;
}
