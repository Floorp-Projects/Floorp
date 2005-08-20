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

#ifndef GLITZ_AGL_H_INCLUDED
#define GLITZ_AGL_H_INCLUDED

#include <glitz.h>

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#include <Carbon/Carbon.h>


/* glitz_agl_info.c */

void
glitz_agl_init (void);

void
glitz_agl_fini (void);
  
  
/* glitz_agl_format.c */

glitz_drawable_format_t *
glitz_agl_find_drawable_format (unsigned long                 mask,
                                const glitz_drawable_format_t *templ,
                                int                           count);
  

/* glitz_agl_drawable.c */

glitz_drawable_t *
glitz_agl_create_drawable_for_window (glitz_drawable_format_t *format,
                                      WindowRef               window,
                                      unsigned int            width,
                                      unsigned int            height);

glitz_drawable_t *
glitz_agl_create_pbuffer_drawable (glitz_drawable_format_t *format,
                                   unsigned int            width,
                                   unsigned int            height);


#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif /* GLITZ_AGL_H_INCLUDED */
