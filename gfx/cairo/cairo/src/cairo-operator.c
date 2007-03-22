/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2006 Keith Packard
 * Copyright © 2006 Red Hat, Inc
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
 *	Carl D. Worth <cworth@cworth.org>
 *	Keith Packard <keithp@keithp.com>
 */

#include "cairoint.h"

/* The analysis here assumes destination alpha semantics (that is
 * CAIRO_CONTENT_COLOR_ALPHA). More things can be considered opaque
 * otherwise (CAIRO_CONTENT_COLOR) so we'll probably want to add a
 * cairo_content_t parameter to this function
 *
 * We also need a definition of what "opaque" means. Is it, "does not
 * requiring 'knowing' the original contents of destination, nor does
 * it set the destination alpha to anything but 1.0" ?
 */
cairo_bool_t
_cairo_operator_always_opaque (cairo_operator_t op)
{
    switch (op) {
    case CAIRO_OPERATOR_CLEAR:
	return FALSE;

    case CAIRO_OPERATOR_SOURCE:
	return FALSE;

    case CAIRO_OPERATOR_OVER:
    case CAIRO_OPERATOR_IN:
    case CAIRO_OPERATOR_OUT:
    case CAIRO_OPERATOR_ATOP:
	return FALSE;

    case CAIRO_OPERATOR_DEST:
	return TRUE;

    case CAIRO_OPERATOR_DEST_OVER:
    case CAIRO_OPERATOR_DEST_IN:
    case CAIRO_OPERATOR_DEST_OUT:
    case CAIRO_OPERATOR_DEST_ATOP:
	return FALSE;

    case CAIRO_OPERATOR_XOR:
    case CAIRO_OPERATOR_ADD:
    case CAIRO_OPERATOR_SATURATE:
	return FALSE;
    }
    return FALSE;
}

/* As above, we'll probably want to add a cairo_content_t parameter to
 * this function
 *
 * We also need a definition of what "translucent" means.
 */
cairo_bool_t
_cairo_operator_always_translucent (cairo_operator_t op)
{
    switch (op) {
    case CAIRO_OPERATOR_CLEAR:
	return TRUE;

    case CAIRO_OPERATOR_SOURCE:
	return FALSE;

    case CAIRO_OPERATOR_OVER:
    case CAIRO_OPERATOR_IN:
    case CAIRO_OPERATOR_OUT:
    case CAIRO_OPERATOR_ATOP:
	return FALSE;

    case CAIRO_OPERATOR_DEST:
	return FALSE;

    case CAIRO_OPERATOR_DEST_OVER:
    case CAIRO_OPERATOR_DEST_IN:
    case CAIRO_OPERATOR_DEST_OUT:
    case CAIRO_OPERATOR_DEST_ATOP:
	return FALSE;

    case CAIRO_OPERATOR_XOR:
    case CAIRO_OPERATOR_ADD:
    case CAIRO_OPERATOR_SATURATE:
	return TRUE;
    }
    return TRUE;
}
