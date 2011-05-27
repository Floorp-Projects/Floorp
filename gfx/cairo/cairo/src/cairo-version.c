/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
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
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 */

#define CAIRO_VERSION_H 1

#include "cairoint.h"

/* get the "real" version info instead of dummy cairo-version.h */
#undef CAIRO_VERSION_H
#include "cairo-features.h"

/**
 * cairo_version:
 *
 * Returns the version of the cairo library encoded in a single
 * integer as per %CAIRO_VERSION_ENCODE. The encoding ensures that
 * later versions compare greater than earlier versions.
 *
 * A run-time comparison to check that cairo's version is greater than
 * or equal to version X.Y.Z could be performed as follows:
 *
 * <informalexample><programlisting>
 * if (cairo_version() >= CAIRO_VERSION_ENCODE(X,Y,Z)) {...}
 * </programlisting></informalexample>
 *
 * See also cairo_version_string() as well as the compile-time
 * equivalents %CAIRO_VERSION and %CAIRO_VERSION_STRING.
 *
 * Return value: the encoded version.
 **/
int
cairo_version (void)
{
    return CAIRO_VERSION;
}

/**
 * cairo_version_string:
 *
 * Returns the version of the cairo library as a human-readable string
 * of the form "X.Y.Z".
 *
 * See also cairo_version() as well as the compile-time equivalents
 * %CAIRO_VERSION_STRING and %CAIRO_VERSION.
 *
 * Return value: a string containing the version.
 **/
const char*
cairo_version_string (void)
{
    return CAIRO_VERSION_STRING;
}
slim_hidden_def (cairo_version_string);
