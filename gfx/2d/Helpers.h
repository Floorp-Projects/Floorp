/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_2D_HELPERS_H_
#define MOZILLA_GFX_2D_HELPERS_H_

#include "2D.h"

namespace mozilla {
namespace gfx {

class AutoRestoreTransform
{
 public:
  explicit AutoRestoreTransform(DrawTarget *aTarget)
   : mDrawTarget(aTarget),
     mOldTransform(aTarget->GetTransform())
  {
  }

  void Init(DrawTarget *aTarget)
  {
    MOZ_ASSERT(!mDrawTarget || aTarget == mDrawTarget);
    if (!mDrawTarget) {
      mDrawTarget = aTarget;
      mOldTransform = aTarget->GetTransform();
    }
  }

  ~AutoRestoreTransform()
  {
    if (mDrawTarget) {
      mDrawTarget->SetTransform(mOldTransform);
    }
  }

 private:
  RefPtr<DrawTarget> mDrawTarget;
  Matrix mOldTransform;
};

} // namespace gfx
} // namespace mozilla

#endif // MOZILLA_GFX_2D_HELPERS_H_
