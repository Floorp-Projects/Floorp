/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "2D.h"
#include "Logging.h"
#include "PathHelpers.h"

#include "DrawTargetCapture.h"

namespace mozilla {
namespace gfx {

already_AddRefed<DrawTargetCapture>
DrawTarget::CreateCaptureDT(const IntSize& aSize)
{
  RefPtr<DrawTargetCaptureImpl> dt = new DrawTargetCaptureImpl();

  if (!dt->Init(aSize, this)) {
    gfxWarning() << "Failed to initialize Capture DrawTarget!";
    return nullptr;
  }

  return dt.forget();
}

void
DrawTarget::DrawCapturedDT(DrawTargetCapture *aCaptureDT,
                           const Matrix& aTransform)
{
  if (aTransform.HasNonIntegerTranslation()) {
    gfxWarning() << "Non integer translations are not supported for DrawCaptureDT at this time!";
    return;
  }
  static_cast<DrawTargetCaptureImpl*>(aCaptureDT)->ReplayToDrawTarget(this, aTransform);
}

void
DrawTarget::PushDeviceSpaceClipRects(const IntRect* aRects, uint32_t aCount)
{
  Matrix oldTransform = GetTransform();
  SetTransform(Matrix());

  RefPtr<PathBuilder> pathBuilder = CreatePathBuilder();
  for (uint32_t i = 0; i < aCount; i++) {
    AppendRectToPath(pathBuilder, Rect(aRects[i]));
  }
  RefPtr<Path> path = pathBuilder->Finish();
  PushClip(path);

  SetTransform(oldTransform);
}

} // namespace gfx
} // namespace mozilla
