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

  virtual void OnAudioDecoded(AudioData* aSample) MOZ_OVERRIDE {
    MonitorAutoLock lock(mMonitor);
    nsAutoPtr<AudioData> sample(aSample);
    if (!mTarget || !mTaskQueue) {
      // We've been shutdown, abort.
      return;
    }
    RefPtr<nsIRunnable> task(new DeliverAudioTask(sample.forget(), mTarget));
    mTaskQueue->Dispatch(task);
  }

  virtual void OnAudioEOS() MOZ_OVERRIDE {
    MonitorAutoLock lock(mMonitor);
    if (!mTarget || !mTaskQueue) {
      // We've been shutdown, abort.
      return;
    }
    RefPtr<nsIRunnable> task(NS_NewRunnableMethod(mTarget, &Target::OnAudioEOS));
    if (NS_FAILED(mTaskQueue->Dispatch(task))) {
      NS_WARNING("Failed to dispatch OnAudioEOS task");
    }
  }

  virtual void OnVideoDecoded(VideoData* aSample) MOZ_OVERRIDE {
    MonitorAutoLock lock(mMonitor);
    nsAutoPtr<VideoData> sample(aSample);
    if (!mTarget || !mTaskQueue) {
      // We've been shutdown, abort.
      return;
    }
    RefPtr<nsIRunnable> task(new DeliverVideoTask(sample.forget(), mTarget));
    mTaskQueue->Dispatch(task);
  }

  virtual void OnVideoEOS() MOZ_OVERRIDE {
    MonitorAutoLock lock(mMonitor);
    if (!mTarget || !mTaskQueue) {
      // We've been shutdown, abort.
      return;
    }
    RefPtr<nsIRunnable> task(NS_NewRunnableMethod(mTarget, &Target::OnVideoEOS));
    if (NS_FAILED(mTaskQueue->Dispatch(task))) {
      NS_WARNING("Failed to dispatch OnVideoEOS task");
    }
  }

  virtual void OnDecodeError() MOZ_OVERRIDE {
    MonitorAutoLock lock(mMonitor);
    if (!mTarget || !mTaskQueue) {
      // We've been shutdown, abort.
      return;
    }
    RefPtr<nsIRunnable> task(NS_NewRunnableMethod(mTarget, &Target::OnDecodeError));
    if (NS_FAILED(mTaskQueue->Dispatch(task))) {
      NS_WARNING("Failed to dispatch OnAudioDecoded task");
    }
  }

  void BreakCycles() {
    MonitorAutoLock lock(mMonitor);
    mTarget = nullptr;
    mTaskQueue = nullptr;
  }

private:

  class DeliverAudioTask : public nsRunnable {
  public:
    DeliverAudioTask(AudioData* aSample, Target* aTarget)
      : mSample(aSample)
      , mTarget(aTarget)
    {
      MOZ_COUNT_CTOR(DeliverAudioTask);
    }
    ~DeliverAudioTask()
    {
      MOZ_COUNT_DTOR(DeliverAudioTask);
    }
    NS_METHOD Run() {
      mTarget->OnAudioDecoded(mSample.forget());
      return NS_OK;
    }
  private:
    nsAutoPtr<AudioData> mSample;
    RefPtr<Target> mTarget;
  };

  class DeliverVideoTask : public nsRunnable {
  public:
    DeliverVideoTask(VideoData* aSample, Target* aTarget)
      : mSample(aSample)
      , mTarget(aTarget)
    {
      MOZ_COUNT_CTOR(DeliverVideoTask);
    }
    ~DeliverVideoTask()
    {
      MOZ_COUNT_DTOR(DeliverVideoTask);
    }
    NS_METHOD Run() {
      mTarget->OnVideoDecoded(mSample.forget());
      return NS_OK;
    }
  private:
    nsAutoPtr<VideoData> mSample;
    RefPtr<Target> mTarget;
  };

  Monitor mMonitor;
  RefPtr<MediaTaskQueue> mTaskQueue;
  RefPtr<Target> mTarget;
};

}

#endif // MediaDataDecodedListener_h_
