/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_AsyncPanZoomAnimation_h_
#define mozilla_layers_AsyncPanZoomAnimation_h_

#include "APZUtils.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "nsISupportsImpl.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace layers {

struct FrameMetrics;

class WheelScrollAnimation;
class KeyboardScrollAnimation;
class OverscrollAnimation;
class SmoothMsdScrollAnimation;
class SmoothScrollAnimation;

class AsyncPanZoomAnimation {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AsyncPanZoomAnimation)

 public:
  explicit AsyncPanZoomAnimation() = default;

  virtual bool DoSample(FrameMetrics& aFrameMetrics,
                        const TimeDuration& aDelta) = 0;

  /**
   * Attempt to handle a main-thread scroll offset update without cancelling
   * the animation. This may or may not make sense depending on the type of
   * the animation and whether the scroll update is relative or absolute.
   *
   * If the scroll update is relative, |aRelativeDelta| will contain the
   * delta of the relative update. If the scroll update is absolute,
   * |aRelativeDelta| will be Nothing() (the animation can check the APZC's
   * FrameMetrics for the new absolute scroll offset if it wants to handle
   * and absolute update).
   *
   * Returns whether the animation could handle the scroll update. If the
   * return value is false, the animation will be cancelled.
   */
  virtual bool HandleScrollOffsetUpdate(const Maybe<CSSPoint>& aRelativeDelta) {
    return false;
  }

  bool Sample(FrameMetrics& aFrameMetrics, const TimeDuration& aDelta) {
    // In some situations, particularly when handoff is involved, it's possible
    // for |aDelta| to be negative on the first call to sample. Ignore such a
    // sample here, to avoid each derived class having to deal with this case.
    if (aDelta.ToMilliseconds() <= 0) {
      return true;
    }

    return DoSample(aFrameMetrics, aDelta);
  }

  /**
   * Get the deferred tasks in |mDeferredTasks| and place them in |aTasks|. See
   * |mDeferredTasks| for more information.  Clears |mDeferredTasks|.
   */
  nsTArray<RefPtr<Runnable>> TakeDeferredTasks() {
    return std::move(mDeferredTasks);
  }

  virtual KeyboardScrollAnimation* AsKeyboardScrollAnimation() {
    return nullptr;
  }
  virtual WheelScrollAnimation* AsWheelScrollAnimation() { return nullptr; }
  virtual SmoothMsdScrollAnimation* AsSmoothMsdScrollAnimation() {
    return nullptr;
  }
  virtual SmoothScrollAnimation* AsSmoothScrollAnimation() { return nullptr; }
  virtual OverscrollAnimation* AsOverscrollAnimation() { return nullptr; }

  virtual bool WantsRepaints() { return true; }

  virtual void Cancel(CancelAnimationFlags aFlags) {}

 protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~AsyncPanZoomAnimation() = default;

  /**
   * Tasks scheduled for execution after the APZC's mMonitor is released.
   * Derived classes can add tasks here in Sample(), and the APZC can call
   * ExecuteDeferredTasks() to execute them.
   */
  nsTArray<RefPtr<Runnable>> mDeferredTasks;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_AsyncPanZoomAnimation_h_
