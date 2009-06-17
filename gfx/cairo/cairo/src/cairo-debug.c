/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2005 Red Hat, Inc.
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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 */

#include "cairoint.h"

/**
 * cairo_debug_reset_static_data:
 *
 * Resets all static data within cairo to its original state,
 * (ie. identical to the state at the time of program invocation). For
 * example, all caches within cairo will be flushed empty.
 *
 * This function is intended to be useful when using memory-checking
 * tools such as valgrind. When valgrind's memcheck analyzes a
 * cairo-using program without a call to cairo_debug_reset_static_data(),
 * it will report all data reachable via cairo's static objects as
 * "still reachable". Calling cairo_debug_reset_static_data() just prior
 * to program termination will make it easier to get squeaky clean
 * reports from valgrind.
 *
 * WARNING: It is only safe to call this function when there are no
 * active cairo objects remaining, (ie. the appropriate destroy
 * functions have been called as necessary). If there are active cairo
 * objects, this call is likely to cause a crash, (eg. an assertion
 * failure due to a hash table being destroyed when non-empty).
 **/
void
cairo_debug_reset_static_data (void)
{
    CAIRO_MUTEX_INITIALIZE ();

    _cairo_scaled_font_map_destroy ();

    _cairo_toy_font_face_reset_static_data ();

#if CAIRO_HAS_FT_FONT
    _cairo_ft_font_reset_static_data ();
#endif

    _cairo_intern_string_reset_static_data ();

    _cairo_scaled_font_reset_static_data ();

    _cairo_pattern_reset_static_data ();

    CAIRO_MUTEX_FINALIZE ();
}

#if HAVE_VALGRIND
#include <memcheck.h>

void
_cairo_debug_check_image_surface_is_defined (const cairo_surface_t *surface)
{
    const cairo_image_surface_t *image = (cairo_image_surface_t *) surface;
    const uint8_t *bits;
    int row, width;

    if (surface == NULL)
	return;

    if (! RUNNING_ON_VALGRIND)
	return;

    bits = image->data;
    switch (image->format) {
    case CAIRO_FORMAT_A1:
	width = (image->width + 7)/8;
	break;
    case CAIRO_FORMAT_A8:
	width = image->width;
	break;
    case CAIRO_FORMAT_RGB24:
    case CAIRO_FORMAT_ARGB32:
	width = image->width*4;
	break;
    default:
	ASSERT_NOT_REACHED;
	return;
    }

    for (row = 0; row < image->height; row++) {
	VALGRIND_CHECK_MEM_IS_DEFINED (bits, width);
	/* and then silence any future valgrind warnings */
	VALGRIND_MAKE_MEM_DEFINED (bits, width);
	bits += image->stride;
    }
}
#endif
