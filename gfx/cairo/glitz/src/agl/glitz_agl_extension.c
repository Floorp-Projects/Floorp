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

#include "glitz_aglint.h"

static glitz_extension_map agl_extensions[] = {
  { 0.0, "GL_APPLE_pixel_buffer", GLITZ_AGL_FEATURE_PBUFFER_MASK },
  { 0.0, "GL_ARB_multisample", GLITZ_AGL_FEATURE_MULTISAMPLE_MASK },
  { 0.0, "GL_ARB_texture_rectangle",
    GLITZ_AGL_FEATURE_TEXTURE_RECTANGLE_MASK },
  { 0.0, "GL_EXT_texture_rectangle",
    GLITZ_AGL_FEATURE_TEXTURE_RECTANGLE_MASK },
  { 0.0, "GL_NV_texture_rectangle", GLITZ_AGL_FEATURE_TEXTURE_RECTANGLE_MASK },
  { 0.0, NULL, 0 }
};

glitz_status_t
glitz_agl_query_extensions (glitz_agl_thread_info_t *thread_info)
{
  GLint attrib[] = {
    AGL_RGBA,
    AGL_NO_RECOVERY,
    AGL_NONE
  };
  AGLPixelFormat pf;
  AGLContext context;

  thread_info->agl_feature_mask = 0;

  pf = aglChoosePixelFormat (NULL, 0, attrib);
  context = aglCreateContext (pf, NULL);
  
  if (context) {
    const char *gl_extensions_string;

    aglSetCurrentContext (context);
  
    gl_extensions_string = (const char *) glGetString (GL_EXTENSIONS);

    thread_info->agl_feature_mask =
      glitz_extensions_query (0.0,
                              gl_extensions_string,
                              agl_extensions);

    if (thread_info->agl_feature_mask & GLITZ_AGL_FEATURE_MULTISAMPLE_MASK) {
      const char *vendor;

      vendor = glGetString (GL_VENDOR);
    
      if (vendor) {
        
        /* NVIDIA's driver seem to support multisample with pbuffers */
        if (!strncmp ("NVIDIA", vendor, 6))
          thread_info->agl_feature_mask |=
            GLITZ_AGL_FEATURE_PBUFFER_MULTISAMPLE_MASK;
      }
    }

    aglSetCurrentContext (NULL);
    aglDestroyContext (context);
  }

  aglDestroyPixelFormat (pf);

  return GLITZ_STATUS_SUCCESS;
}
