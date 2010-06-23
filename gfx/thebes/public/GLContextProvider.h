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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.com>
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


#ifndef GLCONTEXTPROVIDER_H_
#define GLCONTEXTPROVIDER_H_

#include "GLContext.h"
#include "gfxTypes.h"
#include "gfxPoint.h"
#include "nsAutoPtr.h"

class nsIWidget;
class gfxASurface;

namespace mozilla {
namespace gl {

class THEBES_API GLContextProvider 
{
public:
    struct ContextFormat {
        static const ContextFormat BasicRGBA32Format;

        enum StandardContextFormat {
            Empty,
            BasicRGBA32,
            StrictBasicRGBA32,
            BasicRGBX32,
            StrictBasicRGBX32
        };

        ContextFormat(const StandardContextFormat cf) {
            memset(this, 0, sizeof(ContextFormat));

            switch (cf) {
            case BasicRGBA32:
                red = green = blue = alpha = 8;
                minRed = minGreen = minBlue = minAlpha = 1;
                break;

            case StrictBasicRGBA32:
                red = green = blue = alpha = 8;
                minRed = minGreen = minBlue = minAlpha = 8;
                break;

            case BasicRGBX32:
                red = green = blue = 8;
                minRed = minGreen = minBlue = 1;
                break;

            case StrictBasicRGBX32:
                red = green = blue = alpha = 8;
                minRed = minGreen = minBlue = 8;
                break;

            default:
                break;
            }
        }

        int depth, minDepth;
        int stencil, minStencil;
        int red, minRed;
        int green, minGreen;
        int blue, minBlue;
        int alpha, minAlpha;

        int colorBits() const { return red + green + blue; }
    };

    /**
     * Creates a PBuffer.
     *
     * @param aSize Size of the pbuffer to create
     * @param aFormat A ContextFormat describing the desired context attributes.  Defaults to a basic RGBA32 context.
     *
     * @return Context to use for this Pbuffer
     */
    already_AddRefed<GLContext> CreatePBuffer(const gfxIntSize &aSize,
                                              const ContextFormat& aFormat = ContextFormat::BasicRGBA32Format);

    /**
     * Create a context that renders to the surface of the widget that is
     * passed in.
     *
     * @param Widget whose surface to create a context for
     * @return Context to use for this window
     */
    already_AddRefed<GLContext> CreateForWindow(nsIWidget *aWidget);

    /**
     * Try to create a GL context from native surface for arbitrary gfxASurface
     * If surface not compatible this will return NULL
     *
     * @param aSurface surface to create a context for
     * @return Context to use for this surface
     */
    already_AddRefed<GLContext> CreateForNativePixmapSurface(gfxASurface *aSurface);
};

/** Same as GLContextProvider but for off-screen Mesa rendering */
class THEBES_API GLContextProviderOSMesa
{
public:
    typedef GLContextProvider::ContextFormat ContextFormat;

    /**
     * Creates a PBuffer.
     *
     * @param aSize Size of the pbuffer to create
     * @param aFormat A ContextFormat describing the desired context attributes.  Defaults to a basic RGBA32 context.
     *
     * @return Context to use for this Pbuffer
     */
    static already_AddRefed<GLContext> CreatePBuffer(const gfxIntSize &aSize,
                                              const ContextFormat& aFormat = ContextFormat::BasicRGBA32Format);

    /**
     * Create a context that renders to the surface of the widget that is
     * passed in.
     *
     * @param Widget whose surface to create a context for
     * @return Context to use for this window
     */
    static already_AddRefed<GLContext> CreateForWindow(nsIWidget *aWidget);

    /**
     * Try to create a GL context from native surface for arbitrary gfxASurface
     * If surface not compatible this will return NULL
     *
     * @param aSurface surface to create a context for
     * @return Context to use for this surface
     */
    static already_AddRefed<GLContext> CreateForNativePixmapSurface(gfxASurface *aSurface);
};

extern GLContextProvider THEBES_API sGLContextProvider;

}
}

#endif
