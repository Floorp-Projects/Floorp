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

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include "glitz_glxint.h"

static glitz_extension_map glx_extensions[] = {
    { 0.0, "GLX_EXT_visual_rating", GLITZ_GLX_FEATURE_VISUAL_RATING_MASK },
    { 0.0, "GLX_SGIX_fbconfig", GLITZ_GLX_FEATURE_FBCONFIG_MASK },
    { 0.0, "GLX_SGIX_pbuffer", GLITZ_GLX_FEATURE_PBUFFER_MASK },
    { 0.0, "GLX_SGI_make_current_read",
      GLITZ_GLX_FEATURE_MAKE_CURRENT_READ_MASK },
    { 0.0, "GLX_ARB_multisample", GLITZ_GLX_FEATURE_MULTISAMPLE_MASK },
    { 0.0, NULL, 0 }
};

void
glitz_glx_query_extensions (glitz_glx_screen_info_t *screen_info,
			    glitz_gl_float_t        glx_version)
{
    const char *glx_extensions_string;

    glx_extensions_string =
	glXQueryExtensionsString (screen_info->display_info->display,
				  screen_info->screen);

    screen_info->glx_feature_mask =
	glitz_extensions_query (glx_version,
				glx_extensions_string,
				glx_extensions);

    if (screen_info->glx_feature_mask & GLITZ_GLX_FEATURE_MULTISAMPLE_MASK) {
	const char *vendor;

	vendor = glXGetClientString (screen_info->display_info->display,
				     GLX_VENDOR);

	if (vendor) {

	    /* NVIDIA's driver seem to support multisample with pbuffers */
	    if (!strncmp ("NVIDIA", vendor, 6))
		screen_info->glx_feature_mask |=
		    GLITZ_GLX_FEATURE_PBUFFER_MULTISAMPLE_MASK;
	}
    }
}
