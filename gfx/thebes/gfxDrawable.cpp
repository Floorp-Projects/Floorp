/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
    if (!currentTarget) {
        // This happens if we're dealing with an Azure target.
        aPattern->SetExtend(gfxPattern::EXTEND_PAD);
        aPattern->SetFilter(aDefaultFilter);
        return;
    }

    // In theory we can handle this using cairo's EXTEND_PAD,
    // but implementation limitations mean we have to consult
    // the surface type.
    switch (currentTarget->GetType()) {

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
            if (static_cast<gfxXlibSurface*>(currentTarget)->IsPadSlow()) {
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
        return nullptr;

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
