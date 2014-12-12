/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaDataDecodedListener_h_
#define MediaDataDecodedListener_h_

#include "mozilla/Monitor.h"
#include "MediaDecoderReader.h"

namespace mozilla {

class MediaDecoderStateMachine;
class MediaData;

// A RequestSampleCallback implementation that forwards samples onto the
// MediaDecoderStateMachine via tasks that run on the supplied task queue.
template<class Target>
class MediaDataDecodedListener : public RequestSampleCallback {
public:
  MediaDataDecodedListener(Target* aTarget,
                           MediaTaskQueue* aTaskQueue)
    : mMonitor("MediaDataDecodedListener")
    , mTaskQueue(aTaskQueue)
    , mTarget(aTarget)
  {
    MOZ_ASSERT(aTarget);
    MOZ_ASSERT(aTaskQueue);
  }

  void BreakCycles() {
    MonitorAutoLock lock(mMonitor);
    mTarget = nullptr;
    mTaskQueue = nullptr;
  }

  virtual void OnSeekCompleted(nsresult aResult) MOZ_OVERRIDE {
    MonitorAutoLock lock(mMonitor);
    if (!mTarget || !mTaskQueue) {
      // We've been shutdown, abort.
      return;
    }
    RefPtr<nsIRunnable> task(NS_NewRunnableMethodWithArg<nsresult>(mTarget,
                                                                   &Target::OnSeekCompleted,
                                                                   aResult));
    if (NS_FAILED(mTaskQueue->Dispatch(task))) {
      NS_WARNING("Failed to dispatch OnSeekCompleted task");
    }
  }

private:
  Monitor mMonitor;
  RefPtr<MediaTaskQueue> mTaskQueue;
  RefPtr<Target> mTarget;
};

} /* namespace mozilla */

#endif // MediaDataDecodedListener_h_
