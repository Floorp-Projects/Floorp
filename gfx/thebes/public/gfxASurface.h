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

#ifndef GFX_ASURFACE_H
#define GFX_ASURFACE_H

#include <cairo.h>

#include "gfxTypes.h"

/**
 * A surface is something you can draw on. Instantiate a subclass of this
 * abstract class, and use gfxContext to draw on this surface.
 */
class gfxASurface {
    THEBES_DECL_REFCOUNTING_ABSTRACT

public:
    /**
     * The format for an image surface. For all formats with alpha data, 0
     * means transparent, 1 or 255 means fully opaque.
     */
    typedef enum {
        ImageFormatARGB32, ///< ARGB data in native endianness, using premultiplied alpha
        ImageFormatRGB24,  ///< xRGB data in native endianness
        ImageFormatA8,     ///< Only an alpha channel
        ImageFormatA1      ///< Packed transparency information (one byte refers to 8 pixels)
    } gfxImageFormat;

    /*** this DOES NOT addref the surface */
    cairo_surface_t* CairoSurface() { return mSurface; }

protected:
    void Init(cairo_surface_t* surface) {
        mDestroyed = PR_FALSE;
        mSurface = surface;
    }

    void Destroy() {
        if (mDestroyed) {
            NS_WARNING("Calling Destroy on an already-destroyed surface!");
            return;
        }

        cairo_surface_destroy(mSurface);
        mDestroyed = PR_TRUE;
    }

    PRBool Destroyed() {
        return mDestroyed;
    }

    virtual ~gfxASurface() {
        if (!mDestroyed) {
            NS_WARNING("gfxASurface::~gfxASurface called, but cairo surface was not destroyed! (Did someone forget to call Destroy()?)");
        }
    }
private:
    cairo_surface_t* mSurface;
    PRBool mDestroyed;
};

#endif /* GFX_ASURFACE_H */
