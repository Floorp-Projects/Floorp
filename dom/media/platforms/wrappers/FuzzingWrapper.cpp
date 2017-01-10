/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FuzzingWrapper.h"

mozilla::LogModule* GetFuzzingWrapperLog() {
  static mozilla::LazyLogModule log("MediaFuzzingWrapper");
  return log;
}
#define DFW_LOGD(arg, ...) MOZ_LOG(GetFuzzingWrapperLog(), mozilla::LogLevel::Debug, ("DecoderFuzzingWrapper(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
#define DFW_LOGV(arg, ...) MOZ_LOG(GetFuzzingWrapperLog(), mozilla::LogLevel::Verbose, ("DecoderFuzzingWrapper(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
#define CFW_LOGD(arg, ...) MOZ_LOG(GetFuzzingWrapperLog(), mozilla::LogLevel::Debug, ("DecoderCallbackFuzzingWrapper(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
#define CFW_LOGV(arg, ...) MOZ_LOG(GetFuzzingWrapperLog(), mozilla::LogLevel::Verbose, ("DecoderCallbackFuzzingWrapper(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

namespace mozilla {

DecoderFuzzingWrapper::DecoderFuzzingWrapper(
    already_AddRefed<MediaDataDecoder> aDecoder,
    already_AddRefed<DecoderCallbackFuzzingWrapper> aCallbackWrapper)
  : mDecoder(aDecoder)
  , mCallbackWrapper(aCallbackWrapper)
{
  DFW_LOGV("aDecoder=%p aCallbackWrapper=%p", mDecoder.get(), mCallbackWrapper.get());
}

DecoderFuzzingWrapper::~DecoderFuzzingWrapper()
{
  DFW_LOGV("");
}

RefPtr<MediaDataDecoder::InitPromise>
DecoderFuzzingWrapper::Init()
{
  DFW_LOGV("");
  MOZ_ASSERT(mDecoder);
  return mDecoder->Init();
}

void
DecoderFuzzingWrapper::Input(MediaRawData* aData)
{
  DFW_LOGV("aData.mTime=%lld", aData->mTime);
  MOZ_ASSERT(mDecoder);
  mDecoder->Input(aData);
}

void
DecoderFuzzingWrapper::Flush()
{
  DFW_LOGV("Calling mDecoder[%p]->Flush()", mDecoder.get());
  MOZ_ASSERT(mDecoder);
  // Flush may output some frames (though unlikely).
  // Flush may block a bit, it's ok if we output some frames in the meantime.
  mDecoder->Flush();
  DFW_LOGV("mDecoder[%p]->Flush()", mDecoder.get());
  // Clear any delayed output we may have.
  mCallbackWrapper->ClearDelayedOutput();
}

void
DecoderFuzzingWrapper::Drain()
{
  DFW_LOGV("");
  MOZ_ASSERT(mDecoder);
  // Note: The decoder should callback DrainComplete(), we'll drain the
  // delayed output (if any) then.
  mDecoder->Drain();
}

void
DecoderFuzzingWrapper::Shutdown()
{
  DFW_LOGV("");
  MOZ_ASSERT(mDecoder);
  // Both shutdowns below may block a bit.
  mDecoder->Shutdown();
  mCallbackWrapper->Shutdown();
}

bool
DecoderFuzzingWrapper::IsHardwareAccelerated(nsACString& aFailureReason) const
{
  DFW_LOGV("");
  MOZ_ASSERT(mDecoder);
  return mDecoder->IsHardwareAccelerated(aFailureReason);
}

DecoderCallbackFuzzingWrapper::DecoderCallbackFuzzingWrapper(MediaDataDecoderCallback* aCallback)
  : mCallback(aCallback)
  , mDontDelayInputExhausted(false)
  , mDraining(false)
  , mTaskQueue(new TaskQueue(SharedThreadPool::Get(NS_LITERAL_CSTRING("MediaFuzzingWrapper"), 1)))
{
  CFW_LOGV("aCallback=%p", aCallback);
}

DecoderCallbackFuzzingWrapper::~DecoderCallbackFuzzingWrapper()
{
  CFW_LOGV("");
}

void
DecoderCallbackFuzzingWrapper::SetVideoOutputMinimumInterval(
  TimeDuration aFrameOutputMinimumInterval)
{
  CFW_LOGD("aFrameOutputMinimumInterval=%fms",
      aFrameOutputMinimumInterval.ToMilliseconds());
  mFrameOutputMinimumInterval = aFrameOutputMinimumInterval;
}

void
DecoderCallbackFuzzingWrapper::SetDontDelayInputExhausted(
  bool aDontDelayInputExhausted)
{
  CFW_LOGD("aDontDelayInputExhausted=%d",
      aDontDelayInputExhausted);
  mDontDelayInputExhausted = aDontDelayInputExhausted;
}

void
DecoderCallbackFuzzingWrapper::Output(MediaData* aData)
{
  if (!mTaskQueue->IsCurrentThreadIn()) {
    nsCOMPtr<nsIRunnable> task =
      NewRunnableMethod<StoreRefPtrPassByPtr<MediaData>>(
        this, &DecoderCallbackFuzzingWrapper::Output, aData);
    mTaskQueue->Dispatch(task.forget());
    return;
  }
  CFW_LOGV("aData.mTime=%lld", aData->mTime);
  MOZ_ASSERT(mCallback);
  if (mFrameOutputMinimumInterval) {
    if (!mPreviousOutput.IsNull()) {
      if (!mDelayedOutput.empty()) {
        // We already have some delayed frames, just add this one to the queue.
        mDelayedOutput.push_back(MakePair<RefPtr<MediaData>, bool>(aData, false));
        CFW_LOGD("delaying output of sample@%lld, total queued:%d",
                 aData->mTime, int(mDelayedOutput.size()));
        return;
      }
      if (TimeStamp::Now() < mPreviousOutput + mFrameOutputMinimumInterval) {
        // Frame arriving too soon after the previous one, start queuing.
        mDelayedOutput.push_back(MakePair<RefPtr<MediaData>, bool>(aData, false));
        CFW_LOGD("delaying output of sample@%lld, first queued", aData->mTime);
        if (!mDelayedOutputTimer) {
          mDelayedOutputTimer = new MediaTimer();
        }
        ScheduleOutputDelayedFrame();
        return;
      }
    }
    // If we're here, we're going to actually output a frame -> Record time.
    mPreviousOutput = TimeStamp::Now();
  }

  // Passing the data straight through, no need to dispatch to another queue,
  // callback should deal with that.
  mCallback->Output(aData);
}

void
DecoderCallbackFuzzingWrapper::Error(const MediaResult& aError)
{
  if (!mTaskQueue->IsCurrentThreadIn()) {
    mTaskQueue->Dispatch(NewRunnableMethod<MediaResult>(
      this, &DecoderCallbackFuzzingWrapper::Error, aError));
    return;
  }
  CFW_LOGV("");
  MOZ_ASSERT(mCallback);
  ClearDelayedOutput();
  mCallback->Error(aError);
}

void
DecoderCallbackFuzzingWrapper::InputExhausted()
{
  if (!mTaskQueue->IsCurrentThreadIn()) {
    mTaskQueue->Dispatch(NewRunnableMethod(this, &DecoderCallbackFuzzingWrapper::InputExhausted));
    return;
  }
  if (!mDontDelayInputExhausted && !mDelayedOutput.empty()) {
    MediaDataAndInputExhausted& last = mDelayedOutput.back();
    CFW_LOGD("InputExhausted delayed until after output of sample@%lld",
             last.first()->mTime);
    last.second() = true;
    return;
  }
  CFW_LOGV("");
  MOZ_ASSERT(mCallback);
  mCallback->InputExhausted();
}

void
DecoderCallbackFuzzingWrapper::DrainComplete()
{
  if (!mTaskQueue->IsCurrentThreadIn()) {
    mTaskQueue->Dispatch(NewRunnableMethod(this, &DecoderCallbackFuzzingWrapper::DrainComplete));
    return;
  }
  MOZ_ASSERT(mCallback);
  if (mDelayedOutput.empty()) {
    // No queued output -> Draining is complete now.
    CFW_LOGV("No delayed output -> DrainComplete now");
    mCallback->DrainComplete();
  } else {
    // Queued output waiting -> Make sure we call DrainComplete when it's empty.
    CFW_LOGD("Delayed output -> DrainComplete later");
    mDraining = true;
  }
}

void
DecoderCallbackFuzzingWrapper::ReleaseMediaResources()
{
  if (!mTaskQueue->IsCurrentThreadIn()) {
    mTaskQueue->Dispatch(NewRunnableMethod(this, &DecoderCallbackFuzzingWrapper::ReleaseMediaResources));
    return;
  }
  CFW_LOGV("");
  MOZ_ASSERT(mCallback);
  mCallback->ReleaseMediaResources();
}

bool
DecoderCallbackFuzzingWrapper::OnReaderTaskQueue()
{
  CFW_LOGV("");
  MOZ_ASSERT(mCallback);
  return mCallback->OnReaderTaskQueue();
}

void
DecoderCallbackFuzzingWrapper::ScheduleOutputDelayedFrame()
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  if (mDelayedOutputRequest.Exists()) {
    // A delayed output is already scheduled, no need for more than one timer.
    return;
  }
  RefPtr<DecoderCallbackFuzzingWrapper> self = this;
  mDelayedOutputRequest.Begin(
    mDelayedOutputTimer->WaitUntil(
      mPreviousOutput + mFrameOutputMinimumInterval,
      __func__)
    ->Then(mTaskQueue, __func__,
           [self] () -> void {
             if (self->mDelayedOutputRequest.Exists()) {
               self->mDelayedOutputRequest.Complete();
               self->OutputDelayedFrame();
             }
           },
           [self] () -> void {
             if (self->mDelayedOutputRequest.Exists()) {
               self->mDelayedOutputRequest.Complete();
               self->ClearDelayedOutput();
             }
           }));
}

void
DecoderCallbackFuzzingWrapper::OutputDelayedFrame()
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  if (mDelayedOutput.empty()) {
    if (mDraining) {
      // No more output, and we were draining -> Send DrainComplete.
      mDraining = false;
      mCallback->DrainComplete();
    }
    return;
  }
  MediaDataAndInputExhausted& data = mDelayedOutput.front();
  CFW_LOGD("Outputting delayed sample@%lld, remaining:%d",
          data.first()->mTime, int(mDelayedOutput.size() - 1));
  mPreviousOutput = TimeStamp::Now();
  mCallback->Output(data.first());
  if (data.second()) {
    CFW_LOGD("InputExhausted after delayed sample@%lld", data.first()->mTime);
    mCallback->InputExhausted();
  }
  mDelayedOutput.pop_front();
  if (!mDelayedOutput.empty()) {
    // More output -> Send it later.
    ScheduleOutputDelayedFrame();
  } else if (mDraining) {
    // No more output, and we were draining -> Send DrainComplete.
    CFW_LOGD("DrainComplete");
    mDraining = false;
    mCallback->DrainComplete();
  }
}

void
DecoderCallbackFuzzingWrapper::ClearDelayedOutput()
{
  if (!mTaskQueue->IsCurrentThreadIn()) {
    DFW_LOGV("(dispatching self)");
    mTaskQueue->Dispatch(NewRunnableMethod(this, &DecoderCallbackFuzzingWrapper::ClearDelayedOutput));
    return;
  }
  DFW_LOGV("");
  // In case a timer hasn't lapsed yet, before destroying the timer and its
  // attached waitUntil() promise, the 'Then' request must be disconnected.
  mDelayedOutputRequest.DisconnectIfExists();
  mDelayedOutputTimer = nullptr;
  mDelayedOutput.clear();
}

void
DecoderCallbackFuzzingWrapper::Shutdown()
{
  CFW_LOGV("Clear delayed output (if any) before shutting down mTaskQueue");
  ClearDelayedOutput();
  // Await idle here, so that 'ClearDelayedOutput' runs to completion before
  // the task queue is shutdown (and tasks can't be queued anymore).
  mTaskQueue->AwaitIdle();

  CFW_LOGV("Shutting down mTaskQueue");
  mTaskQueue->BeginShutdown();
  mTaskQueue->AwaitIdle();
  CFW_LOGV("mTaskQueue shut down");
}

} // namespace mozilla
