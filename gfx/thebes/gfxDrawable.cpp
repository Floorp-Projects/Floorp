/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxDrawable.h"
#include "gfxASurface.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "gfxColor.h"
#include "gfx2DGlue.h"
#ifdef MOZ_X11
#include "cairo.h"
#include "gfxXlibSurface.h"
#endif

using namespace mozilla;
using namespace mozilla::gfx;

gfxSurfaceDrawable::gfxSurfaceDrawable(SourceSurface* aSurface,
                                       const gfxIntSize aSize,
                                       const gfxMatrix aTransform)
 : gfxDrawable(aSize)
 , mSourceSurface(aSurface)
 , mTransform(aTransform)
{
}

bool
gfxSurfaceDrawable::Draw(gfxContext* aContext,
                         const gfxRect& aFillRect,
                         bool aRepeat,
                         const GraphicsFilter& aFilter,
                         const gfxMatrix& aTransform)
{
    ExtendMode extend = ExtendMode::CLAMP;

    if (aRepeat) {
        extend = ExtendMode::REPEAT;
    }

    Matrix patternTransform = ToMatrix(aTransform * mTransform);
    patternTransform.Invert();

    SurfacePattern pattern(mSourceSurface, extend,
                           patternTransform, ToFilter(aFilter));

    Rect fillRect = ToRect(aFillRect);
    DrawTarget* dt = aContext->GetDrawTarget();

    if (aContext->CurrentOperator() == gfxContext::OPERATOR_CLEAR) {
        dt->ClearRect(fillRect);
    } else if (aContext->CurrentOperator() == gfxContext::OPERATOR_SOURCE) {
        // Emulate cairo operator source which is bound by mask!
        dt->ClearRect(fillRect);
        dt->FillRect(fillRect, pattern);
    } else {
        CompositionOp op = CompositionOpForOp(aContext->CurrentOperator());
        AntialiasMode aaMode =
            aContext->CurrentAntialiasMode() == gfxContext::MODE_ALIASED ?
                AntialiasMode::NONE :
                AntialiasMode::SUBPIXEL;
        dt->FillRect(fillRect, pattern, DrawOptions(1.0f, op, aaMode));
    }
    return true;
}

gfxCallbackDrawable::gfxCallbackDrawable(gfxDrawingCallback* aCallback,
                                         const gfxIntSize aSize)
 : gfxDrawable(aSize)
 , mCallback(aCallback)
{
}

already_AddRefed<gfxSurfaceDrawable>
gfxCallbackDrawable::MakeSurfaceDrawable(const GraphicsFilter aFilter)
{
    SurfaceFormat format =
        gfxPlatform::GetPlatform()->Optimal2DFormatForContent(gfxContentType::COLOR_ALPHA);
    RefPtr<DrawTarget> dt =
        gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(mSize.ToIntSize(),
                                                                     format);
    if (!dt)
        return nullptr;

    nsRefPtr<gfxContext> ctx = new gfxContext(dt);
    Draw(ctx, gfxRect(0, 0, mSize.width, mSize.height), false, aFilter);

    RefPtr<SourceSurface> surface = dt->Snapshot();
    nsRefPtr<gfxSurfaceDrawable> drawable = new gfxSurfaceDrawable(surface, mSize);
    return drawable.forget();
}

bool
gfxCallbackDrawable::Draw(gfxContext* aContext,
                          const gfxRect& aFillRect,
                          bool aRepeat,
                          const GraphicsFilter& aFilter,
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

gfxPatternDrawable::~gfxPatternDrawable()
{
}

class DrawingCallbackFromDrawable : public gfxDrawingCallback {
public:
    explicit DrawingCallbackFromDrawable(gfxDrawable* aDrawable)
     : mDrawable(aDrawable) {
        NS_ASSERTION(aDrawable, "aDrawable is null!");
    }

    virtual ~DrawingCallbackFromDrawable() {}

    virtual bool operator()(gfxContext* aContext,
                              const gfxRect& aFillRect,
                              const GraphicsFilter& aFilter,
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
                         const GraphicsFilter& aFilter,
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
    mPattern->SetMatrix(aTransform * oldMatrix);
    aContext->SetPattern(mPattern);
    aContext->Rectangle(aFillRect);
    aContext->Fill();
    mPattern->SetMatrix(oldMatrix);
    return true;
}
