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

# include "cairo-compiler-private.h"

#if HAVE_CONFIG_H
#include "config.h"
#endif

/* The autoconf on OpenBSD 4.5 produces the malformed constant name
 * SIZEOF_VOID__ rather than SIZEOF_VOID_P.  Work around that here. */
#if !defined(SIZEOF_VOID_P) && defined(SIZEOF_VOID__)
# define SIZEOF_VOID_P SIZEOF_VOID__
#endif

CAIRO_BEGIN_DECLS

#if HAVE_INTEL_ATOMIC_PRIMITIVES

#define HAS_ATOMIC_OPS 1

typedef int cairo_atomic_int_t;

# define _cairo_atomic_int_get(x) (*x)
# define _cairo_atomic_int_set(x, value) ((*x) = value)

# define _cairo_atomic_int_inc(x) ((void) __sync_fetch_and_add(x, 1))
# define _cairo_atomic_int_dec_and_test(x) (__sync_fetch_and_add(x, -1) == 1)
# define _cairo_atomic_int_cmpxchg(x, oldv, newv) __sync_val_compare_and_swap (x, oldv, newv)

#if SIZEOF_VOID_P==SIZEOF_INT
typedef int cairo_atomic_intptr_t;
#elif SIZEOF_VOID_P==SIZEOF_LONG
typedef long cairo_atomic_intptr_t;
#elif SIZEOF_VOID_P==SIZEOF_LONG_LONG
typedef long long cairo_atomic_intptr_t;
#else
#error No matching integer pointer type
#endif

# define _cairo_atomic_ptr_cmpxchg(x, oldv, newv) \
    (void*)__sync_val_compare_and_swap ((cairo_atomic_intptr_t*)x, (cairo_atomic_intptr_t)oldv, (cairo_atomic_intptr_t)newv)

#endif

#if HAVE_LIB_ATOMIC_OPS
#include <atomic_ops.h>

#define HAS_ATOMIC_OPS 1

typedef  AO_t cairo_atomic_int_t;

# define _cairo_atomic_int_get(x) (AO_load_full (x))
# define _cairo_atomic_int_set(x, value) (AO_store_full (x))

# define _cairo_atomic_int_inc(x) ((void) AO_fetch_and_add1_full(x))
# define _cairo_atomic_int_dec_and_test(x) (AO_fetch_and_sub1_full(x) == 1)
# define _cairo_atomic_int_cmpxchg(x, oldv, newv) ((cairo_atomic_int_t) AO_compare_and_swap_full(x, oldv, newv) ? oldv : *x)

#if SIZEOF_VOID_P==SIZEOF_INT
typedef unsigned int cairo_atomic_intptr_t;
#elif SIZEOF_VOID_P==SIZEOF_LONG
typedef unsigned long cairo_atomic_intptr_t;
#elif SIZEOF_VOID_P==SIZEOF_LONG_LONG
typedef unsigned long long cairo_atomic_intptr_t;
#else
#error No matching integer pointer type
#endif

# define _cairo_atomic_ptr_cmpxchg(x, oldv, newv) \
    (void*) (cairo_atomic_intptr_t) _cairo_atomic_int_cmpxchg ((cairo_atomic_intptr_t*)(x), (cairo_atomic_intptr_t)oldv, (cairo_atomic_intptr_t)newv)

#endif


#ifndef HAS_ATOMIC_OPS

typedef int cairo_atomic_int_t;

cairo_private void
_cairo_atomic_int_inc (int *x);

cairo_private cairo_bool_t
_cairo_atomic_int_dec_and_test (int *x);

cairo_private int
_cairo_atomic_int_cmpxchg (int *x, int oldv, int newv);

cairo_private void *
_cairo_atomic_ptr_cmpxchg (void **x, void *oldv, void *newv);

#ifdef ATOMIC_OP_NEEDS_MEMORY_BARRIER

# include "cairo-compiler-private.h"

cairo_private int
_cairo_atomic_int_get (int *x);

cairo_private void
_cairo_atomic_int_set (int *x, int value);

#else

# define _cairo_atomic_int_get(x) (*x)
# define _cairo_atomic_int_set(x, value) ((*x) = value)

#endif

#endif

#define _cairo_atomic_uint_get(x) _cairo_atomic_int_get(x)
#define _cairo_atomic_uint_cmpxchg(x, oldv, newv) \
    _cairo_atomic_int_cmpxchg((cairo_atomic_int_t *)x, oldv, newv)

#define _cairo_status_set_error(status, err) do { \
    /* hide compiler warnings about cairo_status_t != int (gcc treats its as \
     * an unsigned integer instead, and about ignoring the return value. */  \
    int ret__ = _cairo_atomic_int_cmpxchg ((cairo_atomic_int_t *) status, CAIRO_STATUS_SUCCESS, err); \
    (void) ret__; \
} while (0)

CAIRO_END_DECLS

#endif
