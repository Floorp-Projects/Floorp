/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PrintTarget.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Logging.h"

namespace mozilla {
namespace gfx {

PrintTarget::PrintTarget(const IntSize& aSize)
  : mSize(aSize)
  , mIsFinished(false)
{
}

PrintTarget::~PrintTarget()
{
}

already_AddRefed<DrawTarget>
PrintTarget::CreateRecordingDrawTarget(DrawEventRecorder* aRecorder,
                                       DrawTarget* aDrawTarget)
{
  MOZ_ASSERT(aRecorder);
  MOZ_ASSERT(aDrawTarget);

  RefPtr<DrawTarget> dt;

  if (aRecorder) {
    // It doesn't really matter what we pass as the DrawTarget here.
    dt = gfx::Factory::CreateRecordingDrawTarget(aRecorder, aDrawTarget);
  }

  if (!dt || !dt->IsValid()) {
    gfxCriticalNote
      << "Failed to create a recording DrawTarget for PrintTarget";
    return nullptr;
  }

  return dt.forget();
}

void
PrintTarget::Finish()
{
  if (mIsFinished) {
    return;
  }
  mIsFinished = true;
}

} // namespace gfx
} // namespace mozilla
