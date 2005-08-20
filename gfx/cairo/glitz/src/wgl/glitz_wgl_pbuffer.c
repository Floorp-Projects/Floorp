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

HPBUFFERARB
glitz_wgl_pbuffer_create (glitz_wgl_screen_info_t    *screen_info,
			  int                        pixel_format,
                          unsigned int               width,
                          unsigned int               height,
			  HDC                        *dcp)
{
  if (pixel_format) {
    HPBUFFERARB retval;
    int pbuffer_attr[3], i = 0;

    pbuffer_attr[i++] = WGL_PBUFFER_LARGEST_ARB;
    pbuffer_attr[i++] = 0;
    pbuffer_attr[i++] = 0;

    retval =
      screen_info->wgl.create_pbuffer (screen_info->root_dc, pixel_format,
				       width, height, pbuffer_attr);
    if (retval != NULL) {
      *dcp = screen_info->wgl.get_pbuffer_dc (retval);
      return retval;
    }
  }
  *dcp = NULL;
  return NULL;
}

void 
glitz_wgl_pbuffer_destroy (glitz_wgl_screen_info_t *screen_info,
                           HPBUFFERARB             pbuffer,
			   HDC                     dc)
{
  screen_info->wgl.release_pbuffer_dc (pbuffer, dc);
  screen_info->wgl.destroy_pbuffer (pbuffer);
}
