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
 * Contributor(s):
 *	Chris Wilson <chris@chris-wilson.co.uk>
 */

#include "cairoint.h"

#include "cairo-atomic-private.h"
#include "cairo-mutex-private.h"

#ifndef HAS_ATOMIC_OPS
void
_cairo_atomic_int_inc (int *x)
{
    CAIRO_MUTEX_LOCK (_cairo_atomic_mutex);
    *x += 1;
    CAIRO_MUTEX_UNLOCK (_cairo_atomic_mutex);
}

cairo_bool_t
_cairo_atomic_int_dec_and_test (int *x)
{
    cairo_bool_t ret;

    CAIRO_MUTEX_LOCK (_cairo_atomic_mutex);
    ret = --*x == 0;
    CAIRO_MUTEX_UNLOCK (_cairo_atomic_mutex);

    return ret;
}

int
_cairo_atomic_int_cmpxchg (int *x, int oldv, int newv)
{
    int ret;

    CAIRO_MUTEX_LOCK (_cairo_atomic_mutex);
    ret = *x;
    if (ret == oldv)
	*x = newv;
    CAIRO_MUTEX_UNLOCK (_cairo_atomic_mutex);

    return ret;
}

#endif

#ifdef ATOMIC_OP_NEEDS_MEMORY_BARRIER
int
_cairo_atomic_int_get (int *x)
{
    int ret;

    CAIRO_MUTEX_LOCK (_cairo_atomic_mutex);
    ret = *x;
    CAIRO_MUTEX_UNLOCK (_cairo_atomic_mutex);

    return ret;
}

void
_cairo_atomic_int_set (int *x, int value)
{
    CAIRO_MUTEX_LOCK (_cairo_atomic_mutex);
    *x = value;
    CAIRO_MUTEX_UNLOCK (_cairo_atomic_mutex);
}
#endif
