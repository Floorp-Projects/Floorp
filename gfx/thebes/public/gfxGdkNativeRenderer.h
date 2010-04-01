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
 * The Original Code is Novell code.
 *
 * The Initial Developer of the Original Code is Novell.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   rocallahan@novell.com
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

#ifndef GFXGDKNATIVERENDER_H_
#define GFXGDKNATIVERENDER_H_

#include "gfxColor.h"
#include <gdk/gdk.h>

class gfxASurface;
class gfxContext;

/**
 * This class lets us take code that draws into an GDK drawable and lets us
 * use it to draw into any Thebes context. The user should subclass this class,
 * override NativeDraw, and then call Draw(). The drawing will be subjected
 * to all Thebes transformations, clipping etc.
 */
class THEBES_API gfxGdkNativeRenderer {
public:
    /**
     * Perform the native drawing.
     * @param offsetX draw at this offset into the given drawable
     * @param offsetY draw at this offset into the given drawable
     * @param clipRects an array of rects; clip to the union
     * @param numClipRects the number of rects in the array, or zero if
     * no clipping is required
     */
    virtual nsresult NativeDraw(GdkDrawable * drawable, short offsetX, 
            short offsetY, GdkRectangle * clipRects, PRUint32 numClipRects) = 0;
  
    enum {
        // If set, then Draw() is opaque, i.e., every pixel in the intersection
        // of the clipRect and (offset.x,offset.y,bounds.width,bounds.height)
        // will be set and there is no dependence on what the existing pixels
        // in the drawable are set to.
        DRAW_IS_OPAQUE = 0x01,
        // If set, then offset may be non-zero; if not set, then Draw() can
        // only be called with offset==(0,0)
        DRAW_SUPPORTS_OFFSET = 0x02,
        // If set, then numClipRects can be zero or one
        DRAW_SUPPORTS_CLIP_RECT = 0x04,
        // If set, then numClipRects can be any value. If neither this
        // nor CLIP_RECT are set, then numClipRects will be zero
        DRAW_SUPPORTS_CLIP_LIST = 0x08,
        // If set, then the visual passed in can be any visual, otherwise the
        // visual passed in must be the default visual for dpy's default screen
        DRAW_SUPPORTS_NONDEFAULT_VISUAL = 0x10,
        // If set, then the Screen 'screen' in the callback can be different
        // from the default Screen of the default display and can be
        // on a different display.
        DRAW_SUPPORTS_ALTERNATE_SCREEN = 0x20
    };

    struct DrawOutput {
        nsRefPtr<gfxASurface> mSurface;
        PRPackedBool mUniformAlpha;
        PRPackedBool mUniformColor;
        gfxRGBA      mColor;
    };

    /**
     * @param flags see above
     * @param bounds Draw()'s drawing is guaranteed to be restricted to
     * the rectangle (offset.x,offset.y,bounds.width,bounds.height)
     * @param dpy a display to use for the drawing if ctx doesn't have one
     * @param resultSurface if non-null, we will try to capture a copy of the
     * rendered image into a surface similar to the surface of ctx; if
     * successful, a pointer to the new gfxASurface is stored in *resultSurface,
     * otherwise *resultSurface is set to nsnull.
     */
    nsresult Draw(gfxContext* ctx, int width, int height,
                  PRUint32 flags, DrawOutput* output);
};

#endif /*GFXGDKNATIVERENDER_H_*/
