/* vim: set sw=4 sts=4 et cin: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is OS/2 code in Thebes.
 *
 * The Initial Developer of the Original Code is
 * Peter Weilbacher <mozilla@Weilbacher.org>.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "gfxOS2Surface.h"

#include <stdio.h>

/**********************************************************************
 * class gfxOS2Surface
 **********************************************************************/

gfxOS2Surface::gfxOS2Surface(HPS aPS, const gfxIntSize& aSize)
    : mOwnsPS(PR_FALSE), mPS(aPS), mSize(aSize)
{
#ifdef DEBUG_thebes
    printf("gfxOS2Surface::gfxOS2Surface(HPS, ...)\n");
#endif

    cairo_surface_t *surf = cairo_os2_surface_create(mPS, mSize.width, mSize.height);
#ifdef DEBUG_thebes
    printf("  type(%#x)=%d (own=%d, h/w=%d/%d)\n", surf, cairo_surface_get_type(surf),
           mOwnsPS, mSize.width, mSize.height);
#endif
    cairo_surface_mark_dirty(surf);
    Init(surf);
}

gfxOS2Surface::gfxOS2Surface(HWND aWnd)
{
#ifdef DEBUG_thebes
    printf("gfxOS2Surface::gfxOS2Surface(HWND)\n");
#endif

    if (!mPS) {
        // it must have been passed to us from somwhere else
        mOwnsPS = PR_TRUE;
        mPS = WinGetPS(aWnd);
    } else {
        mOwnsPS = PR_FALSE;
    }

    RECTL rectl;
    WinQueryWindowRect(aWnd, &rectl);
    mSize.width = rectl.xRight - rectl.xLeft;
    mSize.height = rectl.yTop - rectl.yBottom;
    if (mSize.width == 0) mSize.width = 10;   // XXX fake some surface area to make
    if (mSize.height == 0) mSize.height = 10; // cairo_os2_surface_create() return something sensible
    cairo_surface_t *surf = cairo_os2_surface_create(mPS, mSize.width, mSize.height);
#ifdef DEBUG_thebes
    printf("  type(%#x)=%d (own=%d, h/w=%d/%d)\n", surf,
           cairo_surface_get_type(surf), mOwnsPS, mSize.width, mSize.height);
#endif
    cairo_os2_surface_set_hwnd(surf, aWnd); // XXX is this needed here??
    cairo_surface_mark_dirty(surf);
    Init(surf);
}

gfxOS2Surface::~gfxOS2Surface()
{
#ifdef DEBUG_thebes
    printf("gfxOS2Surface::~gfxOS2Surface()\n");
#endif

    if (mOwnsPS)
        WinReleasePS(mPS);
}
