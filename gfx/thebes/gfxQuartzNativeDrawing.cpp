/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxQuartzNativeDrawing.h"
#include "gfxQuartzSurface.h"
#include "gfxPlatform.h"
#include "cairo-quartz.h"
#include "gfx2DGlue.h"

using namespace mozilla::gfx;
using namespace mozilla;

gfxQuartzNativeDrawing::gfxQuartzNativeDrawing(gfxContext* ctx,
                                               const gfxRect& nativeRect)
  : mContext(ctx)
  , mNativeRect(ToRect(nativeRect))
  , mCGContext(nullptr)
{
}

CGContextRef
gfxQuartzNativeDrawing::BeginNativeDrawing()
{
  NS_ASSERTION(!mCGContext, "BeginNativeDrawing called when drawing already in progress");

  DrawTarget *dt = mContext->GetDrawTarget();
  if (dt->GetBackendType() != BackendType::COREGRAPHICS ||
      dt->IsDualDrawTarget() ||
      dt->IsTiledDrawTarget()) {
    Matrix transform = dt->GetTransform();
    mNativeRect = transform.TransformBounds(mNativeRect);
    mNativeRect.RoundOut();

    // Quartz theme drawing often adjusts drawing rects, so make
    // sure our surface is big enough for that.
    mNativeRect.Inflate(5);

    if (mNativeRect.IsEmpty()) {
      return nullptr;
    }

    mDrawTarget = Factory::CreateDrawTarget(BackendType::COREGRAPHICS,
                                            IntSize(mNativeRect.width, mNativeRect.height),
                                            SurfaceFormat::B8G8R8A8);

    transform.PostTranslate(-mNativeRect.x, -mNativeRect.y);

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

  mBorrowedContext.Finish();
  if (mDrawTarget) {
    DrawTarget *dest = mContext->GetDrawTarget();
    RefPtr<SourceSurface> source = mDrawTarget->Snapshot();

    Matrix oldTransform = dest->GetTransform();
    dest->SetTransform(Matrix());

    dest->DrawSurface(source,
                      mNativeRect,
                      gfx::Rect(0, 0, mNativeRect.width, mNativeRect.height));


    dest->SetTransform(oldTransform);
  }
}
