/*
 * Copyright © 2008 Rodrigo Kumpera
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Red Hat not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Red Hat makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 *
 * Author:  Rodrigo Kumpera (kumpera@gmail.com)
 * 
 */
#ifndef _PIXMAN_SSE_H_
#define _PIXMAN_SSE_H_

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include "pixman-private.h"

#ifdef USE_SSE2

#if !defined(__amd64__) && !defined(__x86_64__)
pixman_bool_t pixman_have_sse(void);
#else
#define pixman_have_sse() TRUE
#endif

#else
#define pixman_have_sse() FALSE
#endif

#ifdef USE_SSE2

void fbComposeSetupSSE(void);

#endif /* USE_SSE2 */

#endif /* _PIXMAN_SSE_H_ */
