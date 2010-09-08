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

#ifndef GFX_OS2_SURFACE_H
#define GFX_OS2_SURFACE_H

#include "gfxASurface.h"

#define INCL_GPIBITMAPS
#include <os2.h>
#include <cairo-os2.h>

class THEBES_API gfxOS2Surface : public gfxASurface {

public:
    // constructor used to create a memory surface of given size
    gfxOS2Surface(const gfxIntSize& aSize,
                  gfxASurface::gfxImageFormat aImageFormat);
    // constructor for surface connected to an onscreen window
    gfxOS2Surface(HWND aWnd);
    // constructor for surface connected to a printing device context
    gfxOS2Surface(HDC aDC, const gfxIntSize& aSize);
    virtual ~gfxOS2Surface();

    // Special functions that only make sense for the OS/2 port of cairo:

    // Update the cairo surface.
    // While gfxOS2Surface keeps track of the presentation handle itself,
    // use the one from WinBeginPaint() here.
    void Refresh(RECTL *aRect, HPS aPS);

    // Reset the cairo surface to the given size.
    int Resize(const gfxIntSize& aSize);

    HPS GetPS();
    virtual const gfxIntSize GetSize() const { return mSize; }

private:
    HWND mWnd; // non-null if created through the HWND constructor
    HDC mDC; // memory device context
    HPS mPS; // presentation space connected to window or memory device
    HBITMAP mBitmap; // bitmap for initialization of memory surface
    gfxIntSize mSize; // current size of the surface
};

#endif /* GFX_OS2_SURFACE_H */
