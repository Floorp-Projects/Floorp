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

GLXPbuffer
glitz_glx_pbuffer_create (glitz_glx_screen_info_t *screen_info,
                          GLXFBConfig             fbconfig,
                          int                     width,
                          int                     height)
{
  if (fbconfig) {
    int attributes[9];

    attributes[0] = GLX_PBUFFER_WIDTH;
    attributes[1] = width;

    attributes[2] = GLX_PBUFFER_HEIGHT;
    attributes[3] = height;
    
    attributes[4] = GLX_LARGEST_PBUFFER;
    attributes[5] = 0;

    attributes[6] = GLX_PRESERVED_CONTENTS;
    attributes[7] = 1;
    attributes[8] = 0;

    return
        screen_info->glx.create_pbuffer (screen_info->display_info->display,
                                         fbconfig, attributes);
  } else
    return (GLXPbuffer) 0;
}

void 
glitz_glx_pbuffer_destroy (glitz_glx_screen_info_t *screen_info,
                           GLXPbuffer              pbuffer)
{
  screen_info->glx.destroy_pbuffer (screen_info->display_info->display,
                                    pbuffer);
}
