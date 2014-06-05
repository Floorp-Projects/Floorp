/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxQuartzNativeDrawing.h"
#include "gfxQuartzSurface.h"
#include "gfxPlatform.h"
#include "cairo-quartz.h"

using namespace mozilla::gfx;
using namespace mozilla;

gfxQuartzNativeDrawing::gfxQuartzNativeDrawing(gfxContext* ctx,
                                               const gfxRect& nativeRect,
                                               gfxFloat aBackingScale)
  : mContext(ctx)
  , mNativeRect(nativeRect)
  , mBackingScale(aBackingScale)
  , mCGContext(nullptr)
{
  mNativeRect.RoundOut();
}

CGContextRef
gfxQuartzNativeDrawing::BeginNativeDrawing()
{
  NS_ASSERTION(!mCGContext, "BeginNativeDrawing called when drawing already in progress");

  if (mContext->IsCairo()) {
    // We're past that now. Any callers that still supply a Cairo context
    // don't deserve native theming.
    NS_WARNING("gfxQuartzNativeDrawing being used with a gfxContext that is not backed by a DrawTarget");
    return nullptr;
  }

  DrawTarget *dt = mContext->GetDrawTarget();
  if (dt->GetType() != BackendType::COREGRAPHICS || dt->IsDualDrawTarget()) {
    IntSize backingSize(NSToIntFloor(mNativeRect.width * mBackingScale),
                        NSToIntFloor(mNativeRect.height * mBackingScale));

    if (backingSize.IsEmpty()) {
      return nullptr;
    }

    mDrawTarget = Factory::CreateDrawTarget(BackendType::COREGRAPHICS, backingSize, SurfaceFormat::B8G8R8A8);

    Matrix transform;
    transform.Scale(mBackingScale, mBackingScale);
    transform.Translate(-mNativeRect.x, -mNativeRect.y);

    mDrawTarget->SetTransform(transform);
    dt = mDrawTarget;
  }

  mCGContext = mBorrowedContext.Init(dt);
  MOZ_ASSERT(mCGContext);
  return mCGContext;
}

void
gfxQuartzNativeDrawing::EndNativeDrawing()
{
  NS_ASSERTION(mCGContext, "EndNativeDrawing called without BeginNativeDrawing");
  MOZ_ASSERT(!mContext->IsCairo(), "BeginNativeDrawing succeeded with cairo context?");

  mBorrowedContext.Finish();
  if (mDrawTarget) {
    DrawTarget *dest = mContext->GetDrawTarget();
    RefPtr<SourceSurface> source = mDrawTarget->Snapshot();

    IntSize backingSize(NSToIntFloor(mNativeRect.width * mBackingScale),
                        NSToIntFloor(mNativeRect.height * mBackingScale));

    Matrix oldTransform = dest->GetTransform();
    Matrix newTransform = oldTransform;
    newTransform.Translate(mNativeRect.x, mNativeRect.y);
    newTransform.Scale(1.0f / mBackingScale, 1.0f / mBackingScale);

    dest->SetTransform(newTransform);

    dest->DrawSurface(source,
                      gfx::Rect(0, 0, backingSize.width, backingSize.height),
                      gfx::Rect(0, 0, backingSize.width, backingSize.height));


    dest->SetTransform(oldTransform);
  }
}
