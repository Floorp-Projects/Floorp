/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2005 Red Hat, Inc.
 * Copyright © 2012 Intel Corporation
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
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributor(s):
 *	Owen Taylor <otaylor@redhat.com>
 *	Stuart Parmenter <stuart@mozilla.com>
 *	Vladimir Vukicevic <vladimir@pobox.com>
 */

#define WIN32_LEAN_AND_MEAN
/* We require Windows 2000 features such as ETO_PDY */
#if !defined(WINVER) || (WINVER < 0x0500)
# define WINVER 0x0500
#endif
#if !defined(_WIN32_WINNT) || (_WIN32_WINNT < 0x0500)
# define _WIN32_WINNT 0x0500
#endif

#include "cairoint.h"
#include "cairo-win32-private.h"

#include <wchar.h>
#include <windows.h>

void
_cairo_win32_debug_dump_hrgn (HRGN rgn, char *header)
{
    RGNDATA *rd;
    unsigned int z;

    if (header)
	fprintf (stderr, "%s\n", header);

    if (rgn == NULL) {
	fprintf (stderr, " NULL\n");
    }

    z = GetRegionData(rgn, 0, NULL);
    rd = (RGNDATA*) _cairo_malloc (z);
    z = GetRegionData(rgn, z, rd);

    fprintf (stderr, " %ld rects, bounds: %ld %ld %ld %ld\n",
	     rd->rdh.nCount,
	     rd->rdh.rcBound.left,
	     rd->rdh.rcBound.top,
	     rd->rdh.rcBound.right - rd->rdh.rcBound.left,
	     rd->rdh.rcBound.bottom - rd->rdh.rcBound.top);

    for (z = 0; z < rd->rdh.nCount; z++) {
	RECT r = ((RECT*)rd->Buffer)[z];
	fprintf (stderr, " [%d]: [%ld %ld %ld %ld]\n",
		 z, r.left, r.top, r.right - r.left, r.bottom - r.top);
    }

    free(rd);
    fflush (stderr);
}
