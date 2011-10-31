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
#include "mozilla/arm.h"
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
                                gfxASurface *currentTarget,
                                const gfxPattern::GraphicsFilter aDefaultFilter)
{
    // In theory we can handle this using cairo's EXTEND_PAD,
    // but implementation limitations mean we have to consult
    // the surface type.
    switch (currentTarget->GetType()) {

        // The printing surfaces don't natively support or need
        // EXTEND_PAD for padding the edges. Using EXTEND_PAD this way
        // is suboptimal as it will result in the printing surface
        // creating a new image for each fill operation. The pattern
        // will be painted to the image to pad out the pattern, then
        // the new image will be used as the source. This increases
        // printing time and memory use, and prevents the use of mime
        // data from cairo_surface_set_mime_data(). Bug 691061.
        case gfxASurface::SurfaceTypePDF:
        case gfxASurface::SurfaceTypePS:
        case gfxASurface::SurfaceTypeWin32Printing:
            aPattern->SetExtend(gfxPattern::EXTEND_NONE);
            aPattern->SetFilter(aDefaultFilter);
            break;

#ifdef MOZ_X11
        case gfxASurface::SurfaceTypeXlib:
        {
            // See bugs 324698, 422179, and 468496.  This is a workaround for
            // XRender's RepeatPad not being implemented correctly on old X
            // servers.
            //
            // In this situation, cairo avoids XRender and instead reads back
            // to perform EXTEND_PAD with pixman.  This is too slow so we
            // avoid EXTEND_PAD and set the filter to CAIRO_FILTER_FAST ---
            // otherwise, pixman's sampling will sample transparency for the
            // outside edges and we'll get blurry edges.
            //
            // But don't do this for simple downscales because it's horrible.
            // Downscaling means that device-space coordinates are
            // scaled *up* to find the image pixel coordinates.
            //
            // Cairo, and hence Gecko, can use RepeatPad on Xorg 1.7. We
            // enable EXTEND_PAD provided that we're running on a recent
            // enough X server.

            gfxXlibSurface *xlibSurface =
                static_cast<gfxXlibSurface *>(currentTarget);
            Display *dpy = xlibSurface->XDisplay();
            // This is the exact condition for cairo to avoid XRender for
            // EXTEND_PAD
            if (VendorRelease(dpy) >= 60700000 ||
                VendorRelease(dpy) < 10699000) {

                bool isDownscale =
                    aDeviceToImage.xx >= 1.0 && aDeviceToImage.yy >= 1.0 &&
                    aDeviceToImage.xy == 0.0 && aDeviceToImage.yx == 0.0;

                gfxPattern::GraphicsFilter filter =
                    isDownscale ? aDefaultFilter : gfxPattern::FILTER_FAST;
                aPattern->SetFilter(filter);

                // Use the default EXTEND_NONE
                break;
            }
            // else fall through to EXTEND_PAD and the default filter.
        }
#endif

        default:
            // turn on EXTEND_PAD.
            // This is what we really want for all surface types, if the
            // implementation was universally good.
            aPattern->SetExtend(gfxPattern::EXTEND_PAD);
            aPattern->SetFilter(aDefaultFilter);
            break;
    }
}

bool
gfxSurfaceDrawable::Draw(gfxContext* aContext,
                         const gfxRect& aFillRect,
                         bool aRepeat,
                         const gfxPattern::GraphicsFilter& aFilter,
                         const gfxMatrix& aTransform)
{
    nsRefPtr<gfxPattern> pattern = new gfxPattern(mSurface);
    if (aRepeat) {
        pattern->SetExtend(gfxPattern::EXTEND_REPEAT);
        pattern->SetFilter(aFilter);
    } else {
        gfxPattern::GraphicsFilter filter = aFilter;
        if (aContext->CurrentMatrix().HasOnlyIntegerTranslation() &&
            aTransform.HasOnlyIntegerTranslation())
        {
          // If we only have integer translation, no special filtering needs to
          // happen and we explicitly use FILTER_FAST. This is fast for some
          // backends.
          filter = gfxPattern::FILTER_FAST;
        }
        nsRefPtr<gfxASurface> currentTarget = aContext->CurrentSurface();
        gfxMatrix deviceSpaceToImageSpace =
            DeviceToImageTransform(aContext, aTransform);
        PreparePatternForUntiledDrawing(pattern, deviceSpaceToImageSpace,
                                        currentTarget, filter);
    }
    pattern->SetMatrix(gfxMatrix(aTransform).Multiply(mTransform));
    aContext->NewPath();
    aContext->SetPattern(pattern);
    aContext->Rectangle(aFillRect);
    aContext->Fill();
    return true;
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
    Draw(ctx, gfxRect(0, 0, mSize.width, mSize.height), false, aFilter);
    nsRefPtr<gfxSurfaceDrawable> drawable = new gfxSurfaceDrawable(surface, mSize);
    return drawable.forget();
}

bool
gfxCallbackDrawable::Draw(gfxContext* aContext,
                          const gfxRect& aFillRect,
                          bool aRepeat,
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

    return false;
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

    virtual bool operator()(gfxContext* aContext,
                              const gfxRect& aFillRect,
                              const gfxPattern::GraphicsFilter& aFilter,
                              const gfxMatrix& aTransform = gfxMatrix())
    {
        return mDrawable->Draw(aContext, aFillRect, false, aFilter,
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

bool
gfxPatternDrawable::Draw(gfxContext* aContext,
                         const gfxRect& aFillRect,
                         bool aRepeat,
                         const gfxPattern::GraphicsFilter& aFilter,
                         const gfxMatrix& aTransform)
{
    if (!mPattern)
        return false;

    if (aRepeat) {
        // We can't use mPattern directly: We want our repeated tiles to have
        // the size mSize, which might not be the case in mPattern.
        // So we need to draw mPattern into a surface of size mSize, create
        // a pattern from the surface and draw that pattern.
        // gfxCallbackDrawable and gfxSurfaceDrawable already know how to do
        // those things, so we use them here. Drawing mPattern into the surface
        // will happen through this Draw() method with aRepeat = false.
        nsRefPtr<gfxCallbackDrawable> callbackDrawable = MakeCallbackDrawable();
        return callbackDrawable->Draw(aContext, aFillRect, true, aFilter,
                                      aTransform);
    }

    aContext->NewPath();
    gfxMatrix oldMatrix = mPattern->GetMatrix();
    mPattern->SetMatrix(gfxMatrix(aTransform).Multiply(oldMatrix));
    aContext->SetPattern(mPattern);
    aContext->Rectangle(aFillRect);
    aContext->Fill();
    mPattern->SetMatrix(oldMatrix);
    return true;
}
