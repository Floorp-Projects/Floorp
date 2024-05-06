/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2007 Mathias Hasselmann
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
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA
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
 * Contributor(s):
 *	Mathias Hasselmann <mathias.hasselmann@gmx.de>
 */

#include "cairoint.h"

#include "cairo-mutex-private.h"

#define CAIRO_MUTEX_DECLARE(mutex) cairo_mutex_t mutex = CAIRO_MUTEX_NIL_INITIALIZER;
#include "cairo-mutex-list-private.h"
#undef   CAIRO_MUTEX_DECLARE

#if _CAIRO_MUTEX_IMPL_USE_STATIC_INITIALIZER || _CAIRO_MUTEX_IMPL_USE_STATIC_FINALIZER

#define _CAIRO_MUTEX_UNINITIALIZED 0
#define _CAIRO_MUTEX_INITIALIZING  1
#define _CAIRO_MUTEX_INITIALIZED   2

# if _CAIRO_MUTEX_IMPL_USE_STATIC_INITIALIZER
#  define _CAIRO_MUTEX_IMPL_INITIALIZED_DEFAULT_VALUE _CAIRO_MUTEX_UNINITIALIZED
# else
#  define _CAIRO_MUTEX_IMPL_INITIALIZED_DEFAULT_VALUE _CAIRO_MUTEX_INITIALIZED
# endif

int _cairo_mutex_initialized = _CAIRO_MUTEX_IMPL_INITIALIZED_DEFAULT_VALUE;

# undef _CAIRO_MUTEX_IMPL_INITIALIZED_DEFAULT_VALUE

#endif

#if _CAIRO_MUTEX_IMPL_USE_STATIC_INITIALIZER
void _cairo_mutex_initialize (void)
{
#if HAS_ATOMIC_OPS
    int old_value;
    do {
        old_value = _cairo_atomic_int_cmpxchg_return_old (&_cairo_mutex_initialized,
                                                          _CAIRO_MUTEX_UNINITIALIZED,
                                                          _CAIRO_MUTEX_INITIALIZING);
        if (old_value == _CAIRO_MUTEX_INITIALIZED)
            return;

    } while (old_value == _CAIRO_MUTEX_INITIALIZING);
#else
    if (_cairo_mutex_initialized == _CAIRO_MUTEX_INITIALIZED)
        return;

    _cairo_mutex_initialized = _CAIRO_MUTEX_INITIALIZED;
#endif

#define  CAIRO_MUTEX_DECLARE(mutex) CAIRO_MUTEX_INIT (mutex);
#include "cairo-mutex-list-private.h"
#undef   CAIRO_MUTEX_DECLARE

#if HAS_ATOMIC_OPS
    _cairo_atomic_int_set_relaxed (&_cairo_mutex_initialized, _CAIRO_MUTEX_INITIALIZED);
#endif
}
#endif

#if _CAIRO_MUTEX_IMPL_USE_STATIC_FINALIZER
void _cairo_mutex_finalize (void)
{
#if HAS_ATOMIC_OPS
    if (!_cairo_atomic_int_cmpxchg (&_cairo_mutex_initialized,
                                    _CAIRO_MUTEX_INITIALIZED,
                                    _CAIRO_MUTEX_UNINITIALIZED))
        return;
#else
    if (_cairo_mutex_initialized != _CAIRO_MUTEX_INITIALIZED)
        return;

    _cairo_mutex_initialized = _CAIRO_MUTEX_UNINITIALIZED;
#endif

#define  CAIRO_MUTEX_DECLARE(mutex) CAIRO_MUTEX_FINI (mutex);
#include "cairo-mutex-list-private.h"
#undef   CAIRO_MUTEX_DECLARE
}
#endif
