/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is Oracle Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@pavlov.net>
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#ifndef GFX_XLIBSURFACE_H
#define GFX_XLIBSURFACE_H

#include "gfxASurface.h"

#include <X11/extensions/Xrender.h>
#include <X11/Xlib.h>

class THEBES_API gfxXlibSurface : public gfxASurface {
public:
    // create a surface for the specified dpy/drawable/visual.
    // Will use XGetGeometry to query the window/pixmap size.
    gfxXlibSurface(Display *dpy, Drawable drawable, Visual *visual);

    // create a surface for the specified dpy/drawable/visual,
    // with explicitly provided width/height.
    gfxXlibSurface(Display *dpy, Drawable drawable, Visual *visual, unsigned long width, unsigned long height);

    // create a new Pixmap on the display dpy, with
    // the root window as the parent and the default depth
    // for the default screen, and attach the given visual
    gfxXlibSurface(Display *dpy, Visual *visual, unsigned long width, unsigned long height);

    gfxXlibSurface(Display* dpy, Drawable drawable, XRenderPictFormat *format,
                   unsigned long width, unsigned long height);

    gfxXlibSurface(Display* dpy, XRenderPictFormat *format,
                   unsigned long width, unsigned long height);

    gfxXlibSurface(cairo_surface_t *csurf);

    virtual ~gfxXlibSurface();

    gfxSize GetSize() {
        if (mWidth == -1 || mHeight == -1)
            DoSizeQuery();
        return gfxSize(mWidth, mHeight);
    }

    Display* XDisplay() { return mDisplay; }
    Drawable XDrawable() { return mDrawable; }

    static XRenderPictFormat *FindRenderFormat(Display *dpy, gfxImageFormat format);

    // take ownership of a passed-in Pixmap, calling XFreePixmap on it
    // when the gfxXlibSurface is destroyed.
    void TakePixmap();

protected:
    // if TakePixmap() was already called on this
    PRBool mPixmapTaken;
    
    Display *mDisplay;
    Drawable mDrawable;

    void DoSizeQuery();

    long mWidth;
    long mHeight;
};

#endif /* GFX_XLIBSURFACE_H */
