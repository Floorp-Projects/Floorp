/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is gfx thebes code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Eric Butler <zantifon@gmail.com>
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

#ifndef GFX_BLUR_H
#define GFX_BLUR_H

#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "gfxRect.h"
#include "gfxTypes.h"

/**
 * Implementation of a box blur approximation of a Gaussian blur.
 *
 * Creates an 8-bit alpha channel context for callers to draw in, blurs the
 * contents of that context and applies it as an alpha mask on a
 * different existing context.
 *
 * A temporary surface is created in the Init function. The caller then draws
 * any desired content onto the context acquired through GetContext, and lastly
 * calls Paint to apply the blurred content as an alpha mask.
 */
class THEBES_API gfxAlphaBoxBlur
{
public:
    gfxAlphaBoxBlur();

    ~gfxAlphaBoxBlur();

    /**
     * Constructs a box blur and initializes the temporary surface.
     * @param aRect The coordinates of the surface to create in device units.
     *
     * @param aBlurRadius The blur radius in pixels
     *
     * @param aDirtyRect A pointer to a dirty rect, measured in device units, if available.
     *  This will be used for optimizing the blur operation. It is safe to pass NULL here.
     */
    gfxContext* Init(const gfxRect& aRect,
                     const gfxIntSize& aBlurRadius,
                     const gfxRect* aDirtyRect);

    /**
     * Returns the context that should be drawn to supply the alpha mask to be
     * blurred. If the returned surface is null, then there was an error in
     * its creation.
     */
    gfxContext* GetContext()
    {
        return mContext;
    }

    /**
     * Premultiplies the image by the given alpha.
     */
    void PremultiplyAlpha(gfxFloat alpha);

    /**
     * Does the actual blurring and mask applying. Users of this object
     * must have drawn whatever they want to be blurred onto the internal
     * gfxContext returned by GetContext before calling this.
     *
     * @param aDestinationCtx The graphics context on which to apply the
     *  blurred mask.
     */
    void Paint(gfxContext* aDestinationCtx, const gfxPoint& offset = gfxPoint(0.0, 0.0));

    /**
     * Calculates a blur radius that, when used with box blur, approximates
     * a Gaussian blur with the given standard deviation.
     */
    static gfxIntSize CalculateBlurRadius(const gfxPoint& aStandardDeviation);

protected:
    /**
     * The blur radius, in pixels.
     */
    gfxIntSize mBlurRadius;

    /**
     * The context of the temporary alpha surface.
     */
    nsRefPtr<gfxContext> mContext;

    /**
     * The temporary alpha surface.
     */
    nsRefPtr<gfxImageSurface> mImageSurface;

    /**
     * A copy of the dirty rect passed to Init(). This will only be valid if
     * mHasDirtyRect is TRUE.
     */
    gfxRect mDirtyRect;
    PRBool mHasDirtyRect;
};

#endif /* GFX_BLUR_H */
