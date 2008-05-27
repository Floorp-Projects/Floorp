/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2007 Chris Wilson
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Chris Wilson <chris@chris-wilson.co.uk>
 */

#ifndef CAIRO_ATOMIC_PRIVATE_H
#define CAIRO_ATOMIC_PRIVATE_H

#if HAVE_CONFIG_H
#include "config.h"
#endif

CAIRO_BEGIN_DECLS

#define CAIRO_HAS_ATOMIC_OPS 1

#if CAIRO_HAS_INTEL_ATOMIC_PRIMITIVES

typedef int cairo_atomic_int_t;

# define _cairo_atomic_int_inc(x) ((void) __sync_fetch_and_add(x, 1))
# define _cairo_atomic_int_dec_and_test(x) (__sync_fetch_and_add(x, -1) == 1)
# define _cairo_atomic_int_cmpxchg(x, oldv, newv) __sync_val_compare_and_swap (x, oldv, newv)

#else

# include "cairo-compiler-private.h"

# undef CAIRO_HAS_ATOMIC_OPS

typedef int cairo_atomic_int_t;

cairo_private void
_cairo_atomic_int_inc (int *x);

cairo_private cairo_bool_t
_cairo_atomic_int_dec_and_test (int *x);

cairo_private int
_cairo_atomic_int_cmpxchg (int *x, int oldv, int newv);

#endif


#ifdef CAIRO_ATOMIC_OP_NEEDS_MEMORY_BARRIER

# include "cairo-compiler-private.h"

cairo_private int
_cairo_atomic_int_get (int *x);

cairo_private void
_cairo_atomic_int_set (int *x, int value);

#else

# define _cairo_atomic_int_get(x) (*x)
# define _cairo_atomic_int_set(x, value) ((*x) = value)

#endif


#define _cairo_status_set_error(status, err) do { \
    /* hide compiler warnings about cairo_status_t != int (gcc treats its as \
     * an unsigned integer instead, and about ignoring the return value. */  \
    int ret__ = _cairo_atomic_int_cmpxchg ((int *) status, CAIRO_STATUS_SUCCESS, err); \
    (void) ret__; \
} while (0)

CAIRO_END_DECLS

#endif
