/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxQuartzNativeDrawing.h"
#include "gfxPlatform.h"
#include "mozilla/gfx/Helpers.h"

using namespace mozilla::gfx;
using namespace mozilla;

gfxQuartzNativeDrawing::gfxQuartzNativeDrawing(DrawTarget& aDrawTarget,
                                               const Rect& nativeRect)
  : mDrawTarget(&aDrawTarget)
  , mNativeRect(nativeRect)
  , mCGContext(nullptr)
{
}

CGContextRef
gfxQuartzNativeDrawing::BeginNativeDrawing()
{
  NS_ASSERTION(!mCGContext, "BeginNativeDrawing called when drawing already in progress");

  DrawTarget *dt = mDrawTarget;
  if (dt->IsDualDrawTarget() || dt->IsTiledDrawTarget()) {
    // We need a DrawTarget that we can get a CGContextRef from:
    Matrix transform = dt->GetTransform();

    mNativeRect = transform.TransformBounds(mNativeRect);
    mNativeRect.RoundOut();
    // Quartz theme drawing often adjusts drawing rects, so make
    // sure our surface is big enough for that.
    mNativeRect.Inflate(5);
    if (mNativeRect.IsEmpty()) {
      return nullptr;
    }

    mTempDrawTarget =
      Factory::CreateDrawTarget(BackendType::SKIA,
                                IntSize::Truncate(mNativeRect.width, mNativeRect.height),
                                SurfaceFormat::B8G8R8A8);

    if (mTempDrawTarget) {
        transform.PostTranslate(-mNativeRect.x, -mNativeRect.y);
        mTempDrawTarget->SetTransform(transform);
    }
    dt = mTempDrawTarget;
  }
  if (dt) {
    mCGContext = mBorrowedContext.Init(dt);
    MOZ_ASSERT(mCGContext);
  }
  return mCGContext;
}

void
gfxQuartzNativeDrawing::EndNativeDrawing()
{
  NS_ASSERTION(mCGContext, "EndNativeDrawing called without BeginNativeDrawing");

  mBorrowedContext.Finish();
  if (mTempDrawTarget) {
    RefPtr<SourceSurface> source = mTempDrawTarget->Snapshot();

    AutoRestoreTransform autoRestore(mDrawTarget);
    mDrawTarget->SetTransform(Matrix());
    mDrawTarget->DrawSurface(source, mNativeRect,
                             Rect(0, 0, mNativeRect.width, mNativeRect.height));
  }
}
