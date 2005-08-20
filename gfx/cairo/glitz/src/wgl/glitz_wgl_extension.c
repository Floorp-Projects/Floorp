/*
 * Copyright © 2004 David Reveman
 * 
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the names of
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
 * Based on glx code by David Reveman
 *
 * Contributors:
 *  Tor Lillqvist <tml@iki.fi>
 *  Vladimir Vukicevic <vladimir@pobox.com>
 *
 */

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include "glitz_wglint.h"

#include <stdio.h>
#include <ctype.h>

static glitz_extension_map wgl_extensions[] = {
  { 0.0, "WGL_ARB_pixel_format", GLITZ_WGL_FEATURE_PIXEL_FORMAT_MASK },
  { 0.0, "WGL_ARB_pbuffer", GLITZ_WGL_FEATURE_PBUFFER_MASK },
  { 0.0, "WGL_ARB_make_current_read",
    GLITZ_WGL_FEATURE_MAKE_CURRENT_READ_MASK },
  { 0.0, "WGL_ARB_multisample", GLITZ_WGL_FEATURE_MULTISAMPLE_MASK },
  { 0.0, NULL, 0 }
};

void
glitz_wgl_query_extensions (glitz_wgl_screen_info_t *screen_info,
			    glitz_gl_float_t        wgl_version)
{
  typedef const char * (WINAPI * PFNWGLGETEXTENSIONSSTRINGARBPROC) (HDC hdc);
  PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB;
  const char *wgl_extensions_string;

  wglGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC) wglGetProcAddress ("wglGetExtensionsStringARB");

  if (wglGetExtensionsStringARB == NULL)
    return;

  wgl_extensions_string = (*wglGetExtensionsStringARB) (screen_info->root_dc);
  
  screen_info->wgl_feature_mask =
    glitz_extensions_query (wgl_version,
                            wgl_extensions_string,
                            wgl_extensions);
  
  if (screen_info->wgl_feature_mask & GLITZ_WGL_FEATURE_MULTISAMPLE_MASK) {
    const char *renderer;

    renderer = glGetString (GL_RENDERER);    
    if (renderer) {
      
      /* nVIDIA's driver seem to support multisample with pbuffers (?) */
      if (!strncmp ("GeForce", renderer, 7) ||
	  !strncmp ("Quadro", renderer, 6))
        screen_info->wgl_feature_mask |=
          GLITZ_WGL_FEATURE_PBUFFER_MULTISAMPLE_MASK;
    }
  }
}
