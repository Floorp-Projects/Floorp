/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_AsyncPanZoomAnimation_h_
#define mozilla_layers_AsyncPanZoomAnimation_h_

#include "base/message_loop.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Vector.h"
#include "FrameMetrics.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace layers {

class WheelScrollAnimation;

class AsyncPanZoomAnimation {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AsyncPanZoomAnimation)

public:
  explicit AsyncPanZoomAnimation(const TimeDuration& aRepaintInterval)
    : mRepaintInterval(aRepaintInterval)
  { }

  virtual bool DoSample(FrameMetrics& aFrameMetrics,
                        const TimeDuration& aDelta) = 0;

  bool Sample(FrameMetrics& aFrameMetrics,
              const TimeDuration& aDelta) {
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
  Vector<Task*> TakeDeferredTasks() {
    return Move(mDeferredTasks);
  }

  /**
   * Specifies how frequently (at most) we want to do repaints during the
   * animation sequence. TimeDuration::Forever() will cause it to only repaint
   * at the end of the animation.
   */
  TimeDuration mRepaintInterval;

public:
  virtual WheelScrollAnimation* AsWheelScrollAnimation() {
    return nullptr;
  }

protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~AsyncPanZoomAnimation()
  { }

  /**
   * Tasks scheduled for execution after the APZC's mMonitor is released.
   * Derived classes can add tasks here in Sample(), and the APZC can call
   * ExecuteDeferredTasks() to execute them.
   */
  Vector<Task*> mDeferredTasks;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_AsyncPanZoomAnimation_h_
