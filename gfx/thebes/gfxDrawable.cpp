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
gfxSurfaceDrawable::DrawWithSamplingRect(DrawTarget* aDrawTarget,
                                         CompositionOp aOp,
                                         AntialiasMode aAntialiasMode,
                                         const gfxRect& aFillRect,
                                         const gfxRect& aSamplingRect,
                                         ExtendMode aExtendMode,
                                         const SamplingFilter aSamplingFilter,
                                         gfxFloat aOpacity)
{
  if (!mSourceSurface) {
    return true;
  }

  // When drawing with CLAMP we can expand the sampling rect to the nearest pixel
  // without changing the result.
  IntRect intRect = IntRect::RoundOut(aSamplingRect.x, aSamplingRect.y,
                                      aSamplingRect.width, aSamplingRect.height);

  IntSize size = mSourceSurface->GetSize();
  if (!IntRect(IntPoint(), size).Contains(intRect)) {
    return false;
  }

  DrawInternal(aDrawTarget, aOp, aAntialiasMode, aFillRect, intRect,
               ExtendMode::CLAMP, aSamplingFilter, aOpacity, gfxMatrix());
  return true;
}

bool
gfxSurfaceDrawable::Draw(gfxContext* aContext,
                         const gfxRect& aFillRect,
                         ExtendMode aExtendMode,
                         const SamplingFilter aSamplingFilter,
                         gfxFloat aOpacity,
                         const gfxMatrix& aTransform)

{
  if (!mSourceSurface) {
    return true;
  }

  DrawInternal(aContext->GetDrawTarget(), aContext->CurrentOp(),
               aContext->CurrentAntialiasMode(), aFillRect, IntRect(),
               aExtendMode, aSamplingFilter, aOpacity, aTransform);
  return true;
}

void
gfxSurfaceDrawable::DrawInternal(DrawTarget* aDrawTarget,
                                 CompositionOp aOp,
                                 AntialiasMode aAntialiasMode,
                                 const gfxRect& aFillRect,
                                 const IntRect& aSamplingRect,
                                 ExtendMode aExtendMode,
                                 const SamplingFilter aSamplingFilter,
                                 gfxFloat aOpacity,
                                 const gfxMatrix& aTransform)
{
    Matrix patternTransform = ToMatrix(aTransform * mTransform);
    patternTransform.Invert();

    SurfacePattern pattern(mSourceSurface, aExtendMode,
                           patternTransform, aSamplingFilter, aSamplingRect);

    Rect fillRect = ToRect(aFillRect);

    if (aOp == CompositionOp::OP_SOURCE && aOpacity == 1.0) {
        // Emulate cairo operator source which is bound by mask!
        aDrawTarget->ClearRect(fillRect);
        aDrawTarget->FillRect(fillRect, pattern);
    } else {
        aDrawTarget->FillRect(fillRect, pattern,
                              DrawOptions(aOpacity, aOp, aAntialiasMode));
    }
}

gfxCallbackDrawable::gfxCallbackDrawable(gfxDrawingCallback* aCallback,
                                         const IntSize aSize)
 : gfxDrawable(aSize)
 , mCallback(aCallback)
{
}

already_AddRefed<gfxSurfaceDrawable>
gfxCallbackDrawable::MakeSurfaceDrawable(const SamplingFilter aSamplingFilter)
{
    SurfaceFormat format =
        gfxPlatform::GetPlatform()->Optimal2DFormatForContent(gfxContentType::COLOR_ALPHA);
    RefPtr<DrawTarget> dt =
        gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(mSize,
                                                                     format);
    if (!dt || !dt->IsValid())
        return nullptr;

    RefPtr<gfxContext> ctx = gfxContext::CreateOrNull(dt);
    MOZ_ASSERT(ctx); // already checked for target above
    Draw(ctx, gfxRect(0, 0, mSize.width, mSize.height), ExtendMode::CLAMP,
         aSamplingFilter);

    RefPtr<SourceSurface> surface = dt->Snapshot();
    if (surface) {
        RefPtr<gfxSurfaceDrawable> drawable = new gfxSurfaceDrawable(surface, mSize);
        return drawable.forget();
    }
    return nullptr;
}

static bool
IsRepeatingExtendMode(ExtendMode aExtendMode)
{
  switch (aExtendMode) {
  case ExtendMode::REPEAT:
  case ExtendMode::REPEAT_X:
  case ExtendMode::REPEAT_Y:
    return true;
  default:
    return false;
  }
}

bool
gfxCallbackDrawable::Draw(gfxContext* aContext,
                          const gfxRect& aFillRect,
                          ExtendMode aExtendMode,
                          const SamplingFilter aSamplingFilter,
                          gfxFloat aOpacity,
                          const gfxMatrix& aTransform)
{
    if ((IsRepeatingExtendMode(aExtendMode) || aOpacity != 1.0 || aContext->CurrentOp() != CompositionOp::OP_OVER) &&
        !mSurfaceDrawable) {
        mSurfaceDrawable = MakeSurfaceDrawable(aSamplingFilter);
    }

    if (mSurfaceDrawable)
        return mSurfaceDrawable->Draw(aContext, aFillRect, aExtendMode,
                                      aSamplingFilter,
                                      aOpacity, aTransform);

    if (mCallback)
        return (*mCallback)(aContext, aFillRect, aSamplingFilter, aTransform);

    return false;
}

gfxPatternDrawable::gfxPatternDrawable(gfxPattern* aPattern,
                                       const IntSize aSize)
 : gfxDrawable(aSize)
 , mPattern(aPattern)
{
}

gfxPatternDrawable::~gfxPatternDrawable() = default;

class DrawingCallbackFromDrawable : public gfxDrawingCallback {
public:
    explicit DrawingCallbackFromDrawable(gfxDrawable* aDrawable)
     : mDrawable(aDrawable) {
        NS_ASSERTION(aDrawable, "aDrawable is null!");
    }

    ~DrawingCallbackFromDrawable() override = default;

    bool operator()(gfxContext* aContext,
                    const gfxRect& aFillRect,
                    const SamplingFilter aSamplingFilter,
                    const gfxMatrix& aTransform = gfxMatrix()) override
    {
        return mDrawable->Draw(aContext, aFillRect, ExtendMode::CLAMP,
                               aSamplingFilter, 1.0,
                               aTransform);
    }
private:
    RefPtr<gfxDrawable> mDrawable;
};

already_AddRefed<gfxCallbackDrawable>
gfxPatternDrawable::MakeCallbackDrawable()
{
    RefPtr<gfxDrawingCallback> callback =
        new DrawingCallbackFromDrawable(this);
    RefPtr<gfxCallbackDrawable> callbackDrawable =
        new gfxCallbackDrawable(callback, mSize);
    return callbackDrawable.forget();
}

bool
gfxPatternDrawable::Draw(gfxContext* aContext,
                         const gfxRect& aFillRect,
                         ExtendMode aExtendMode,
                         const SamplingFilter aSamplingFilter,
                         gfxFloat aOpacity,
                         const gfxMatrix& aTransform)
{
    DrawTarget& aDrawTarget = *aContext->GetDrawTarget();

    if (!mPattern)
        return false;

    if (IsRepeatingExtendMode(aExtendMode)) {
        // We can't use mPattern directly: We want our repeated tiles to have
        // the size mSize, which might not be the case in mPattern.
        // So we need to draw mPattern into a surface of size mSize, create
        // a pattern from the surface and draw that pattern.
        // gfxCallbackDrawable and gfxSurfaceDrawable already know how to do
        // those things, so we use them here. Drawing mPattern into the surface
        // will happen through this Draw() method with aRepeat = false.
        RefPtr<gfxCallbackDrawable> callbackDrawable = MakeCallbackDrawable();
        return callbackDrawable->Draw(aContext, aFillRect, aExtendMode,
                                      aSamplingFilter,
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
