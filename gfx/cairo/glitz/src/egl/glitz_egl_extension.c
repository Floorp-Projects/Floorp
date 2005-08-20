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

#include "glitz_eglint.h"

#if 0
static glitz_extension_map egl_extensions[] = {
  { 0.0, "EGL_SGIX_fbconfig", GLITZ_EGL_FEATURE_FBCONFIG_MASK },
  { 0.0, "EGL_SGIX_pbuffer", GLITZ_EGL_FEATURE_PBUFFER_MASK },
  { 0.0, "EGL_SGI_make_current_read",
    GLITZ_EGL_FEATURE_MAKE_CURRENT_READ_MASK },
  { 0.0, "EGL_ARB_multisample", GLITZ_EGL_FEATURE_MULTISAMPLE_MASK },
  { 0.0, NULL, 0 }
};
#endif

void
glitz_egl_query_extensions (glitz_egl_screen_info_t *screen_info,
                            glitz_gl_float_t        egl_version)
{
#if 0
  const char *egl_extensions_string;

  egl_extensions_string =
    eglQueryExtensionsString (screen_info->display_info->display,
                              screen_info->screen);
  
  screen_info->egl_feature_mask =
    glitz_extensions_query (egl_version,
                            egl_extensions_string,
                            egl_extensions);
  
  if (screen_info->egl_feature_mask & GLITZ_EGL_FEATURE_MULTISAMPLE_MASK) {
    const char *vendor;

    vendor = eglGetClientString (screen_info->display_info->display,
                                 EGL_VENDOR);
    
    if (vendor) {
      
      /* NVIDIA's driver seem to support multisample with pbuffers */
      if (!strncmp ("NVIDIA", vendor, 6))
        screen_info->egl_feature_mask |=
          GLITZ_EGL_FEATURE_PBUFFER_MULTISAMPLE_MASK;
    }
  }
#endif
}
