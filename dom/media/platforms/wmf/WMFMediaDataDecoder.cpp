/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFMediaDataDecoder.h"
#include "VideoUtils.h"
#include "WMFUtils.h"
#include "nsTArray.h"
#include "mozilla/Telemetry.h"

#include "mozilla/Logging.h"
#include "mozilla/SyncRunnable.h"

#define LOG(...) MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, (__VA_ARGS__))

namespace mozilla {

WMFMediaDataDecoder::WMFMediaDataDecoder(MFTManager* aMFTManager,
                                         TaskQueue* aTaskQueue,
                                         MediaDataDecoderCallback* aCallback)
  : mTaskQueue(aTaskQueue)
  , mCallback(aCallback)
  , mMFTManager(aMFTManager)
  , mIsFlushing(false)
  , mIsShutDown(false)
{
}

WMFMediaDataDecoder::~WMFMediaDataDecoder()
{
}

RefPtr<MediaDataDecoder::InitPromise>
WMFMediaDataDecoder::Init()
{
  MOZ_ASSERT(!mIsShutDown);
  return InitPromise::CreateAndResolve(mMFTManager->GetType(), __func__);
}

// A single telemetry sample is reported for each MediaDataDecoder object
// that has detected error or produced output successfully.
static void
SendTelemetry(unsigned long hr)
{
  // Collapse the error codes into a range of 0-0xff that can be viewed in
  // telemetry histograms.  For most MF_E_* errors, unique samples are used,
  // retaining the least significant 7 or 8 bits.  Other error codes are
  // bucketed.
  uint32_t sample;
  if (SUCCEEDED(hr)) {
    sample = 0;
  } else if (hr < 0xc00d36b0) {
    sample = 1; // low bucket
  } else if (hr < 0xc00d3700) {
    sample = hr & 0xffU; // MF_E_*
  } else if (hr <= 0xc00d3705) {
    sample = 0x80 + (hr & 0xfU); // more MF_E_*
  } else if (hr < 0xc00d6d60) {
    sample = 2; // mid bucket
  } else if (hr <= 0xc00d6d78) {
    sample = hr & 0xffU; // MF_E_TRANSFORM_*
  } else {
    sample = 3; // high bucket
  }

  nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction(
    [sample] {
      Telemetry::Accumulate(Telemetry::MEDIA_WMF_DECODE_ERROR, sample);
    });
  NS_DispatchToMainThread(runnable);
}

void
WMFMediaDataDecoder::Shutdown()
{
  MOZ_DIAGNOSTIC_ASSERT(!mIsShutDown);

  if (mTaskQueue) {
    mTaskQueue->Dispatch(NewRunnableMethod(this, &WMFMediaDataDecoder::ProcessShutdown));
  } else {
    ProcessShutdown();
  }
  mIsShutDown = true;
}

void
WMFMediaDataDecoder::ProcessShutdown()
{
  if (mMFTManager) {
    mMFTManager->Shutdown();
    mMFTManager = nullptr;
    if (!mRecordedError && mHasSuccessfulOutput) {
      SendTelemetry(S_OK);
    }
  }
}

// Inserts data into the decoder's pipeline.
void
WMFMediaDataDecoder::Input(MediaRawData* aSample)
{
  MOZ_ASSERT(mCallback->OnReaderTaskQueue());
  MOZ_DIAGNOSTIC_ASSERT(!mIsShutDown);

  nsCOMPtr<nsIRunnable> runnable =
    NewRunnableMethod<RefPtr<MediaRawData>>(
      this,
      &WMFMediaDataDecoder::ProcessDecode,
      RefPtr<MediaRawData>(aSample));
  mTaskQueue->Dispatch(runnable.forget());
}

void
WMFMediaDataDecoder::ProcessDecode(MediaRawData* aSample)
{
  if (mIsFlushing) {
    // Skip sample, to be released by runnable.
    return;
  }

  HRESULT hr = mMFTManager->Input(aSample);
  if (FAILED(hr)) {
    NS_WARNING("MFTManager rejected sample");
    mCallback->Error(MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                                 RESULT_DETAIL("MFTManager::Input:%x", hr)));
    if (!mRecordedError) {
      SendTelemetry(hr);
      mRecordedError = true;
    }
    return;
  }

  mLastStreamOffset = aSample->mOffset;

  ProcessOutput();
}

void
WMFMediaDataDecoder::ProcessOutput()
{
  RefPtr<MediaData> output;
  HRESULT hr = S_OK;
  while (SUCCEEDED(hr = mMFTManager->Output(mLastStreamOffset, output)) &&
         output) {
    mHasSuccessfulOutput = true;
    mCallback->Output(output);
  }
  if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
    mCallback->InputExhausted();
  } else if (FAILED(hr)) {
    NS_WARNING("WMFMediaDataDecoder failed to output data");
    mCallback->Error(MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                                 RESULT_DETAIL("MFTManager::Output:%x", hr)));
    if (!mRecordedError) {
      SendTelemetry(hr);
      mRecordedError = true;
    }
  }
}

void
WMFMediaDataDecoder::ProcessFlush()
{
  if (mMFTManager) {
    mMFTManager->Flush();
  }
}

void
WMFMediaDataDecoder::Flush()
{
  MOZ_ASSERT(mCallback->OnReaderTaskQueue());
  MOZ_DIAGNOSTIC_ASSERT(!mIsShutDown);

  mIsFlushing = true;
  nsCOMPtr<nsIRunnable> runnable =
    NewRunnableMethod(this, &WMFMediaDataDecoder::ProcessFlush);
  SyncRunnable::DispatchToThread(mTaskQueue, runnable);
  mIsFlushing = false;
}

void
WMFMediaDataDecoder::ProcessDrain()
{
  if (!mIsFlushing && mMFTManager) {
    // Order the decoder to drain...
    mMFTManager->Drain();
    // Then extract all available output.
    ProcessOutput();
  }
  mCallback->DrainComplete();
}

void
WMFMediaDataDecoder::Drain()
{
  MOZ_ASSERT(mCallback->OnReaderTaskQueue());
  MOZ_DIAGNOSTIC_ASSERT(!mIsShutDown);

  mTaskQueue->Dispatch(NewRunnableMethod(this, &WMFMediaDataDecoder::ProcessDrain));
}

bool
WMFMediaDataDecoder::IsHardwareAccelerated(nsACString& aFailureReason) const {
  MOZ_ASSERT(!mIsShutDown);

  return mMFTManager && mMFTManager->IsHardwareAccelerated(aFailureReason);
}

void
WMFMediaDataDecoder::SetSeekThreshold(const media::TimeUnit& aTime)
{
  MOZ_ASSERT(mCallback->OnReaderTaskQueue());
  MOZ_DIAGNOSTIC_ASSERT(!mIsShutDown);

  RefPtr<WMFMediaDataDecoder> self = this;
  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableFunction([self, aTime]() {
    media::TimeUnit threshold = aTime;
    self->mMFTManager->SetSeekThreshold(threshold);
  });
  mTaskQueue->Dispatch(runnable.forget());
}

} // namespace mozilla
