/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxDrawable.h"
#include "gfxASurface.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "gfx2DGlue.h"
#ifdef MOZ_X11
#include "cairo.h"
#include "gfxXlibSurface.h"
#endif
#include "mozilla/gfx/Logging.h"

using namespace mozilla;
using namespace mozilla::gfx;

gfxSurfaceDrawable::gfxSurfaceDrawable(SourceSurface* aSurface,
                                       const IntSize aSize,
                                       const gfxMatrix aTransform)
 : gfxDrawable(aSize)
 , mSourceSurface(aSurface)
 , mTransform(aTransform)
{
  if (!mSourceSurface) {
    gfxWarning() << "Creating gfxSurfaceDrawable with null SourceSurface";
  }
}

bool
gfxSurfaceDrawable::DrawWithSamplingRect(gfxContext* aContext,
                                         const gfxRect& aFillRect,
                                         const gfxRect& aSamplingRect,
                                         bool aRepeat,
                                         const Filter& aFilter,
                                         gfxFloat aOpacity)
{
  if (!mSourceSurface) {
    return true;
  }

  // When drawing with CLAMP we can expand the sampling rect to the nearest pixel
  // without changing the result.
  gfxRect samplingRect = aSamplingRect;
  samplingRect.RoundOut();
  IntRect intRect(samplingRect.x, samplingRect.y, samplingRect.width, samplingRect.height);

  IntSize size = mSourceSurface->GetSize();
  if (!IntRect(0, 0, size.width, size.height).Contains(intRect)) {
    return false;
  }

  DrawInternal(aContext, aFillRect, intRect, false, aFilter, aOpacity, gfxMatrix());
  return true;
}

bool
gfxSurfaceDrawable::Draw(gfxContext* aContext,
                         const gfxRect& aFillRect,
                         bool aRepeat,
                         const Filter& aFilter,
                         gfxFloat aOpacity,
                         const gfxMatrix& aTransform)
{
  if (!mSourceSurface) {
    return true;
  }

  DrawInternal(aContext, aFillRect, IntRect(), aRepeat, aFilter, aOpacity, aTransform);
  return true;
}

void
gfxSurfaceDrawable::DrawInternal(gfxContext* aContext,
                                 const gfxRect& aFillRect,
                                 const IntRect& aSamplingRect,
                                 bool aRepeat,
                                 const Filter& aFilter,
                                 gfxFloat aOpacity,
                                 const gfxMatrix& aTransform)
{
    ExtendMode extend = ExtendMode::CLAMP;

    if (aRepeat) {
        extend = ExtendMode::REPEAT;
    }

    Matrix patternTransform = ToMatrix(aTransform * mTransform);
    patternTransform.Invert();

    SurfacePattern pattern(mSourceSurface, extend,
                           patternTransform, aFilter, aSamplingRect);

    Rect fillRect = ToRect(aFillRect);
    DrawTarget* dt = aContext->GetDrawTarget();

    if (aContext->CurrentOp() == CompositionOp::OP_SOURCE &&
        aOpacity == 1.0) {
        // Emulate cairo operator source which is bound by mask!
        dt->ClearRect(fillRect);
        dt->FillRect(fillRect, pattern);
    } else {
        dt->FillRect(fillRect, pattern,
                     DrawOptions(aOpacity, aContext->CurrentOp(),
                                 aContext->CurrentAntialiasMode()));
    }
}

gfxCallbackDrawable::gfxCallbackDrawable(gfxDrawingCallback* aCallback,
                                         const IntSize aSize)
 : gfxDrawable(aSize)
 , mCallback(aCallback)
{
}

already_AddRefed<gfxSurfaceDrawable>
gfxCallbackDrawable::MakeSurfaceDrawable(const Filter aFilter)
{
    SurfaceFormat format =
        gfxPlatform::GetPlatform()->Optimal2DFormatForContent(gfxContentType::COLOR_ALPHA);
    RefPtr<DrawTarget> dt =
        gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(mSize,
                                                                     format);
    if (!dt)
        return nullptr;

    nsRefPtr<gfxContext> ctx = new gfxContext(dt);
    Draw(ctx, gfxRect(0, 0, mSize.width, mSize.height), false, aFilter);

    RefPtr<SourceSurface> surface = dt->Snapshot();
    if (surface) {
        nsRefPtr<gfxSurfaceDrawable> drawable = new gfxSurfaceDrawable(surface, mSize);
        return drawable.forget();
    }
    return nullptr;
}

bool
gfxCallbackDrawable::Draw(gfxContext* aContext,
                          const gfxRect& aFillRect,
                          bool aRepeat,
                          const Filter& aFilter,
                          gfxFloat aOpacity,
                          const gfxMatrix& aTransform)
{
    if ((aRepeat || aOpacity != 1.0) && !mSurfaceDrawable) {
        mSurfaceDrawable = MakeSurfaceDrawable(aFilter);
    }

    if (mSurfaceDrawable)
        return mSurfaceDrawable->Draw(aContext, aFillRect, aRepeat, aFilter,
                                      aOpacity, aTransform);

    if (mCallback)
        return (*mCallback)(aContext, aFillRect, aFilter, aTransform);

    return false;
}

gfxPatternDrawable::gfxPatternDrawable(gfxPattern* aPattern,
                                       const IntSize aSize)
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
                              const Filter& aFilter,
                              const gfxMatrix& aTransform = gfxMatrix())
    {
        return mDrawable->Draw(aContext, aFillRect, false, aFilter, 1.0,
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
                         const Filter& aFilter,
                         gfxFloat aOpacity,
                         const gfxMatrix& aTransform)
{
    DrawTarget& aDrawTarget = *aContext->GetDrawTarget();

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
                                      aOpacity, aTransform);
    }

    gfxMatrix oldMatrix = mPattern->GetMatrix();
    mPattern->SetMatrix(aTransform * oldMatrix);
    DrawOptions drawOptions(aOpacity);
    aDrawTarget.FillRect(ToRect(aFillRect),
                         *mPattern->GetPattern(&aDrawTarget), drawOptions);
    mPattern->SetMatrix(oldMatrix);
    return true;
}
