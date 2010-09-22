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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Markus Stange.
 * Portions created by the Initial Developer are Copyright (C) 2010
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

#include "gfxDrawable.h"
#include "gfxASurface.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#ifdef MOZ_X11
#include "cairo.h"
#include "gfxXlibSurface.h"
#endif

gfxSurfaceDrawable::gfxSurfaceDrawable(gfxASurface* aSurface,
                                       const gfxIntSize aSize,
                                       const gfxMatrix aTransform)
 : gfxDrawable(aSize)
 , mSurface(aSurface)
 , mTransform(aTransform)
{
}

static gfxMatrix
DeviceToImageTransform(gfxContext* aContext,
                       const gfxMatrix& aUserSpaceToImageSpace)
{
    gfxFloat deviceX, deviceY;
    nsRefPtr<gfxASurface> currentTarget =
        aContext->CurrentSurface(&deviceX, &deviceY);
    gfxMatrix currentMatrix = aContext->CurrentMatrix();
    gfxMatrix deviceToUser = gfxMatrix(currentMatrix).Invert();
    deviceToUser.Translate(-gfxPoint(-deviceX, -deviceY));
    return gfxMatrix(deviceToUser).Multiply(aUserSpaceToImageSpace);
}

static void
PreparePatternForUntiledDrawing(gfxPattern* aPattern,
                                const gfxMatrix& aDeviceToImage,
                                gfxASurface::gfxSurfaceType aSurfaceType,
                                nsRefPtr<gfxASurface> currentTarget,
                                const gfxPattern::GraphicsFilter aDefaultFilter)
{
    // In theory we can handle this using cairo's EXTEND_PAD,
    // but implementation limitations mean we have to consult
    // the surface type.
    switch (aSurfaceType) {
        case gfxASurface::SurfaceTypeXlib:
        case gfxASurface::SurfaceTypeXcb:
        {
            // See bug 324698.  This is a workaround for EXTEND_PAD not being
            // implemented correctly on linux in the X server.
            //
            // Set the filter to CAIRO_FILTER_FAST --- otherwise,
            // pixman's sampling will sample transparency for the outside edges
            // and we'll get blurry edges.  CAIRO_EXTEND_PAD would also work
            // here, if available
            //
            // But don't do this for simple downscales because it's horrible.
            // Downscaling means that device-space coordinates are
            // scaled *up* to find the image pixel coordinates.
            //
            // Update 8/11/09: The underlying X server/driver bugs are now
            // fixed, and cairo uses the fast XRender code-path as of 1.9.2
            // (commit a1d0a06b6275cac3974be84919993e187394fe43) --
            // but only if running on a 1.7 X server.
            // So we enable EXTEND_PAD provided that we're running a recent
            // enough cairo version (obviously, this is only relevant if
            // --enable-system-cairo is used) AND running on a recent
            // enough X server.  This should finally bring linux up to par
            // with other systems.

            PRBool isDownscale =
                aDeviceToImage.xx >= 1.0 && aDeviceToImage.yy >= 1.0 &&
                aDeviceToImage.xy == 0.0 && aDeviceToImage.yx == 0.0;
            if (!isDownscale) {
#ifdef MOZ_X11
                PRBool fastExtendPad = PR_FALSE;
                if (currentTarget->GetType() == gfxASurface::SurfaceTypeXlib &&
                    cairo_version() >= CAIRO_VERSION_ENCODE(1,9,2)) {
                    gfxXlibSurface *xlibSurface =
                        static_cast<gfxXlibSurface *>(currentTarget.get());
                    Display *dpy = xlibSurface->XDisplay();
                    // This is the exact condition for cairo to use XRender for
                    // EXTEND_PAD
                    if (VendorRelease (dpy) < 60700000 &&
                        VendorRelease (dpy) >= 10699000)
                        fastExtendPad = PR_TRUE;
                }
                if (fastExtendPad) {
                    aPattern->SetExtend(gfxPattern::EXTEND_PAD);
                    aPattern->SetFilter(aDefaultFilter);
                } else
#endif
                    aPattern->SetFilter(gfxPattern::FILTER_FAST);
            }
            break;
        }

        case gfxASurface::SurfaceTypeQuartz:
        case gfxASurface::SurfaceTypeQuartzImage:
            // Don't set EXTEND_PAD, Mac seems to be OK. Really?
            aPattern->SetFilter(aDefaultFilter);
            break;

        default:
            // turn on EXTEND_PAD.
            // This is what we really want for all surface types, if the
            // implementation was universally good.
            aPattern->SetExtend(gfxPattern::EXTEND_PAD);
            aPattern->SetFilter(aDefaultFilter);
            break;
    }
}

PRBool
gfxSurfaceDrawable::Draw(gfxContext* aContext,
                         const gfxRect& aFillRect,
                         PRBool aRepeat,
                         const gfxPattern::GraphicsFilter& aFilter,
                         const gfxMatrix& aTransform)
{
    nsRefPtr<gfxPattern> pattern = new gfxPattern(mSurface);
    if (aRepeat) {
        pattern->SetExtend(gfxPattern::EXTEND_REPEAT);
        pattern->SetFilter(aFilter);
    } else {
        nsRefPtr<gfxASurface> currentTarget = aContext->CurrentSurface();
        gfxASurface::gfxSurfaceType surfaceType = currentTarget->GetType();
        gfxMatrix deviceSpaceToImageSpace =
            DeviceToImageTransform(aContext, aTransform);
        PreparePatternForUntiledDrawing(pattern, deviceSpaceToImageSpace,
                                        surfaceType, currentTarget, aFilter);
    }
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
    pattern->SetFilter(gfxPattern::FILTER_FAST); 
#endif
    pattern->SetMatrix(gfxMatrix(aTransform).Multiply(mTransform));
    aContext->NewPath();
    aContext->SetPattern(pattern);
    aContext->Rectangle(aFillRect);
    aContext->Fill();
    return PR_TRUE;
}

gfxCallbackDrawable::gfxCallbackDrawable(gfxDrawingCallback* aCallback,
                                         const gfxIntSize aSize)
 : gfxDrawable(aSize)
 , mCallback(aCallback)
{
}

already_AddRefed<gfxSurfaceDrawable>
gfxCallbackDrawable::MakeSurfaceDrawable(const gfxPattern::GraphicsFilter aFilter)
{
    nsRefPtr<gfxASurface> surface =
        gfxPlatform::GetPlatform()->CreateOffscreenSurface(mSize, gfxASurface::CONTENT_COLOR_ALPHA);
    if (!surface || surface->CairoStatus() != 0)
        return nsnull;

    nsRefPtr<gfxContext> ctx = new gfxContext(surface);
    Draw(ctx, gfxRect(0, 0, mSize.width, mSize.height), PR_FALSE, aFilter);
    nsRefPtr<gfxSurfaceDrawable> drawable = new gfxSurfaceDrawable(surface, mSize);
    return drawable.forget();
}

PRBool
gfxCallbackDrawable::Draw(gfxContext* aContext,
                          const gfxRect& aFillRect,
                          PRBool aRepeat,
                          const gfxPattern::GraphicsFilter& aFilter,
                          const gfxMatrix& aTransform)
{
    if (aRepeat && !mSurfaceDrawable) {
        mSurfaceDrawable = MakeSurfaceDrawable(aFilter);
    }

    if (mSurfaceDrawable)
        return mSurfaceDrawable->Draw(aContext, aFillRect, aRepeat, aFilter,
                                      aTransform);

    if (mCallback)
        return (*mCallback)(aContext, aFillRect, aFilter, aTransform);

    return PR_FALSE;
}

gfxPatternDrawable::gfxPatternDrawable(gfxPattern* aPattern,
                                       const gfxIntSize aSize)
 : gfxDrawable(aSize)
 , mPattern(aPattern)
{
}

class DrawingCallbackFromDrawable : public gfxDrawingCallback {
public:
    DrawingCallbackFromDrawable(gfxDrawable* aDrawable)
     : mDrawable(aDrawable) {
        NS_ASSERTION(aDrawable, "aDrawable is null!");
    }

    virtual ~DrawingCallbackFromDrawable() {}

    virtual PRBool operator()(gfxContext* aContext,
                              const gfxRect& aFillRect,
                              const gfxPattern::GraphicsFilter& aFilter,
                              const gfxMatrix& aTransform = gfxMatrix())
    {
        return mDrawable->Draw(aContext, aFillRect, PR_FALSE, aFilter,
                               aTransform);
    }
private:
    nsRefPtr<gfxDrawable> mDrawable;
};

already_AddRefed<gfxCallbackDrawable>
gfxPatternDrawable::MakeCallbackDrawable()
{
    nsRefPtr<gfxDrawingCallback> callback =
        new DrawingCallbackFromDrawable(this);
    nsRefPtr<gfxCallbackDrawable> callbackDrawable =
        new gfxCallbackDrawable(callback, mSize);
    return callbackDrawable.forget();
}

PRBool
gfxPatternDrawable::Draw(gfxContext* aContext,
                         const gfxRect& aFillRect,
                         PRBool aRepeat,
                         const gfxPattern::GraphicsFilter& aFilter,
                         const gfxMatrix& aTransform)
{
    if (!mPattern)
        return PR_FALSE;

    if (aRepeat) {
        // We can't use mPattern directly: We want our repeated tiles to have
        // the size mSize, which might not be the case in mPattern.
        // So we need to draw mPattern into a surface of size mSize, create
        // a pattern from the surface and draw that pattern.
        // gfxCallbackDrawable and gfxSurfaceDrawable already know how to do
        // those things, so we use them here. Drawing mPattern into the surface
        // will happen through this Draw() method with aRepeat = PR_FALSE.
        nsRefPtr<gfxCallbackDrawable> callbackDrawable = MakeCallbackDrawable();
        return callbackDrawable->Draw(aContext, aFillRect, PR_TRUE, aFilter,
                                      aTransform);
    }

    aContext->NewPath();
    gfxMatrix oldMatrix = mPattern->GetMatrix();
    mPattern->SetMatrix(gfxMatrix(aTransform).Multiply(oldMatrix));
    aContext->SetPattern(mPattern);
    aContext->Rectangle(aFillRect);
    aContext->Fill();
    mPattern->SetMatrix(oldMatrix);
    return PR_TRUE;
}
