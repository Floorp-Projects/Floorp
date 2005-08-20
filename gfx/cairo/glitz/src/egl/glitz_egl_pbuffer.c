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

EGLSurface
glitz_egl_pbuffer_create (glitz_egl_screen_info_t *screen_info,
                          EGLConfig               egl_config,
                          int                     width,
                          int                     height)
{
  if (egl_config) {
    int attributes[9];

    attributes[0] = EGL_WIDTH;
    attributes[1] = width;

    attributes[2] = EGL_HEIGHT;
    attributes[3] = height;
#if 0    
    attributes[4] = EGL_LARGEST_PBUFFER;
    attributes[5] = 0;

    attributes[6] = EGL_PRESERVED_CONTENTS;
    attributes[7] = 1;
    attributes[8] = 0;
#endif
    return
        eglCreatePbufferSurface(screen_info->display_info->egl_display,
                                         egl_config, attributes);
  } else
    return (EGLSurface) 0;
}

void 
glitz_egl_pbuffer_destroy (glitz_egl_screen_info_t *screen_info,
                           EGLSurface              egl_pbuffer)
{
  eglDestroySurface(screen_info->display_info->egl_display,
                                    egl_pbuffer);
}
