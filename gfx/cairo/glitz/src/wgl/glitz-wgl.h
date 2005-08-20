/*
 * Copyright © 2004 David Reveman, Peter Nilsson
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the names of
 * David Reveman and Peter Nilsson not be used in advertising or
 * publicity pertaining to distribution of the software without
 * specific, written prior permission. David Reveman and Peter Nilsson
 * makes no representations about the suitability of this software for
 * any purpose. It is provided "as is" without express or implied warranty.
 *
 * DAVID REVEMAN AND PETER NILSSON DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL DAVID REVEMAN AND
 * PETER NILSSON BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Based on glx code by David Reveman and Peter Nilsson
 *
 * Contributors:
 *  Tor Lillqvist <tml@iki.fi>
 *  Vladimir Vukicevic <vladimir@pobox.com>
 *
 */

#ifndef GLITZ_WGL_H_INCLUDED
#define GLITZ_WGL_H_INCLUDED

#include <glitz.h>

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#include <windows.h>

/* glitz_wgl_info.c */

typedef struct _glitz_wgl_thread_starter_arg_t {
  int (*user_thread_function) (void *);
  void *user_thread_function_arg;
} glitz_wgl_thread_starter_arg_t;

int
glitz_wgl_thread_starter (const glitz_wgl_thread_starter_arg_t *arg);

void
glitz_wgl_init (const char *gl_library);

void
glitz_wgl_fini (void);
  
  
/* glitz_wgl_format.c */

glitz_drawable_format_t *
glitz_wgl_find_drawable_format (unsigned long                  mask,
                                const glitz_drawable_format_t *templ,
                                int                            count);
  
/* glitz_wgl_drawable.c */

glitz_drawable_t *
glitz_wgl_create_drawable_for_window (glitz_drawable_format_t *format,
                                      HWND                     window,
                                      unsigned int             width,
                                      unsigned int             height);

glitz_drawable_t *
glitz_wgl_create_pbuffer_drawable (glitz_drawable_format_t    *format,
                                   unsigned int                width,
                                   unsigned int                height);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif /* GLITZ_WGL_H_INCLUDED */
