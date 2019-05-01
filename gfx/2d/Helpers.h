/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_2D_HELPERS_H_
#define MOZILLA_GFX_2D_HELPERS_H_

#include "2D.h"

namespace mozilla {
namespace gfx {

class AutoRestoreTransform final {
 public:
  AutoRestoreTransform() = default;

  explicit AutoRestoreTransform(DrawTarget* aTarget)
      : mDrawTarget(aTarget), mOldTransform(aTarget->GetTransform()) {}

  void Init(DrawTarget* aTarget) {
    MOZ_ASSERT(!mDrawTarget || aTarget == mDrawTarget);
    if (!mDrawTarget) {
      mDrawTarget = aTarget;
      mOldTransform = aTarget->GetTransform();
    }
  }

  ~AutoRestoreTransform() {
    if (mDrawTarget) {
      mDrawTarget->SetTransform(mOldTransform);
    }
  }

 private:
  RefPtr<DrawTarget> mDrawTarget;
  Matrix mOldTransform;
};

class AutoPopClips final {
 public:
  explicit AutoPopClips(DrawTarget* aTarget)
      : mDrawTarget(aTarget), mPushCount(0) {
    MOZ_ASSERT(mDrawTarget);
  }

  ~AutoPopClips() { PopAll(); }

  void PushClip(const Path* aPath) {
    mDrawTarget->PushClip(aPath);
    ++mPushCount;
  }

  void PushClipRect(const Rect& aRect) {
    mDrawTarget->PushClipRect(aRect);
    ++mPushCount;
  }

  void PopClip() {
    MOZ_ASSERT(mPushCount > 0);
    mDrawTarget->PopClip();
    --mPushCount;
  }

  void PopAll() {
    while (mPushCount-- > 0) {
      mDrawTarget->PopClip();
    }
  }

 private:
  RefPtr<DrawTarget> mDrawTarget;
  int32_t mPushCount;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // MOZILLA_GFX_2D_HELPERS_H_
