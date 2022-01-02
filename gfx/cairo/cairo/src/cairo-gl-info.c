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
 *      Heiko Lewin <heiko.lewin@gmx.de>
 */

#include "cairoint.h"
#include "cairo-gl-private.h"

#include <errno.h>

int
_cairo_gl_get_version (void)
{
    int major, minor;
    const char *version = (const char *) glGetString (GL_VERSION);
    const char *dot = version == NULL ? NULL : strchr (version, '.');
    const char *major_start = dot;

    /* Sanity check */
    if (dot == NULL || dot == version || *(dot + 1) == '\0') {
	major = 0;
	minor = 0;
    } else {
	/* Find the start of the major version in the string */
	while (major_start > version && *major_start != ' ')
	    --major_start;
	major = strtol (major_start, NULL, 10);
	minor = strtol (dot + 1, NULL, 10);
    }

    return CAIRO_GL_VERSION_ENCODE (major, minor);
}


cairo_gl_flavor_t
_cairo_gl_degrade_flavor_by_build_features (cairo_gl_flavor_t flavor) {
    switch(flavor) {
    case CAIRO_GL_FLAVOR_DESKTOP:
#if CAIRO_HAS_GL_SURFACE
	return CAIRO_GL_FLAVOR_DESKTOP;
#else
	return CAIRO_GL_FLAVOR_NONE;
#endif

    case CAIRO_GL_FLAVOR_ES3:
#if CAIRO_HAS_GLESV3_SURFACE
	return CAIRO_GL_FLAVOR_ES3;
#else
	/* intentional fall through: degrade to GLESv2 if GLESv3-surfaces are not available */
#endif

    case CAIRO_GL_FLAVOR_ES2:
#if CAIRO_HAS_GLESV2_SURFACE
	return CAIRO_GL_FLAVOR_ES2;
#else
	/* intentional fall through: no OpenGL in first place or no surfaces for it's version */
#endif

    default:
	return CAIRO_GL_FLAVOR_NONE;
    }
}

cairo_gl_flavor_t
_cairo_gl_get_flavor (void)
{
    const char *version = (const char *) glGetString (GL_VERSION);
    cairo_gl_flavor_t flavor;

    if (version == NULL) {
	flavor = CAIRO_GL_FLAVOR_NONE;
    } else if (strstr (version, "OpenGL ES 3") != NULL) {
	flavor = CAIRO_GL_FLAVOR_ES3;
    } else if (strstr (version, "OpenGL ES 2") != NULL) {
	flavor = CAIRO_GL_FLAVOR_ES2;
    } else {
	flavor = CAIRO_GL_FLAVOR_DESKTOP;
    }

    return _cairo_gl_degrade_flavor_by_build_features(flavor);
}

unsigned long
_cairo_gl_get_vbo_size (void)
{
    unsigned long vbo_size;

    const char *env = getenv ("CAIRO_GL_VBO_SIZE");
    if (env == NULL) {
	vbo_size = CAIRO_GL_VBO_SIZE_DEFAULT;
    } else {
	errno = 0;
	vbo_size = strtol (env, NULL, 10);
	assert (errno == 0);
	assert (vbo_size > 0);
    }

    return vbo_size;
}

cairo_bool_t
_cairo_gl_has_extension (const char *ext)
{
    const char *extensions = (const char *) glGetString (GL_EXTENSIONS);
    size_t len = strlen (ext);
    const char *ext_ptr = extensions;

    if (unlikely (ext_ptr == NULL))
	return 0;

    while ((ext_ptr = strstr (ext_ptr, ext)) != NULL) {
	if (ext_ptr[len] == ' ' || ext_ptr[len] == '\0')
	    break;
	ext_ptr += len;
    }

    return (ext_ptr != NULL);
}
