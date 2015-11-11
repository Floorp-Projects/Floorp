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

extern mozilla::LogModule* GetPDMLog();
#define LOG(...) MOZ_LOG(GetPDMLog(), mozilla::LogLevel::Debug, (__VA_ARGS__))

namespace mozilla {

WMFMediaDataDecoder::WMFMediaDataDecoder(MFTManager* aMFTManager,
                                         FlushableTaskQueue* aTaskQueue,
                                         MediaDataDecoderCallback* aCallback)
  : mTaskQueue(aTaskQueue)
  , mCallback(aCallback)
  , mMFTManager(aMFTManager)
  , mMonitor("WMFMediaDataDecoder")
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

nsresult
WMFMediaDataDecoder::Shutdown()
{
  MOZ_DIAGNOSTIC_ASSERT(!mIsShutDown);

  if (mTaskQueue) {
    nsCOMPtr<nsIRunnable> runnable =
      NS_NewRunnableMethod(this, &WMFMediaDataDecoder::ProcessShutdown);
    mTaskQueue->Dispatch(runnable.forget());
  } else {
    ProcessShutdown();
  }
  mIsShutDown = true;
  return NS_OK;
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
nsresult
WMFMediaDataDecoder::Input(MediaRawData* aSample)
{
  MOZ_ASSERT(mCallback->OnReaderTaskQueue());
  MOZ_DIAGNOSTIC_ASSERT(!mIsShutDown);

  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethodWithArg<RefPtr<MediaRawData>>(
      this,
      &WMFMediaDataDecoder::ProcessDecode,
      RefPtr<MediaRawData>(aSample));
  mTaskQueue->Dispatch(runnable.forget());
  return NS_OK;
}

void
WMFMediaDataDecoder::ProcessDecode(MediaRawData* aSample)
{
  {
    MonitorAutoLock mon(mMonitor);
    if (mIsFlushing) {
      // Skip sample, to be released by runnable.
      return;
    }
  }

  HRESULT hr = mMFTManager->Input(aSample);
  if (FAILED(hr)) {
    NS_WARNING("MFTManager rejected sample");
    mCallback->Error();
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
    if (mTaskQueue->IsEmpty()) {
      mCallback->InputExhausted();
    }
  } else if (FAILED(hr)) {
    NS_WARNING("WMFMediaDataDecoder failed to output data");
    mCallback->Error();
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
  MonitorAutoLock mon(mMonitor);
  mIsFlushing = false;
  mon.NotifyAll();
}

nsresult
WMFMediaDataDecoder::Flush()
{
  MOZ_ASSERT(mCallback->OnReaderTaskQueue());
  MOZ_DIAGNOSTIC_ASSERT(!mIsShutDown);

  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethod(this, &WMFMediaDataDecoder::ProcessFlush);
  MonitorAutoLock mon(mMonitor);
  mIsFlushing = true;
  mTaskQueue->Dispatch(runnable.forget());
  while (mIsFlushing) {
    mon.Wait();
  }
  return NS_OK;
}

void
WMFMediaDataDecoder::ProcessDrain()
{
  bool isFlushing;
  {
    MonitorAutoLock mon(mMonitor);
    isFlushing = mIsFlushing;
  }
  if (!isFlushing && mMFTManager) {
    // Order the decoder to drain...
    mMFTManager->Drain();
    // Then extract all available output.
    ProcessOutput();
  }
  mCallback->DrainComplete();
}

nsresult
WMFMediaDataDecoder::Drain()
{
  MOZ_ASSERT(mCallback->OnReaderTaskQueue());
  MOZ_DIAGNOSTIC_ASSERT(!mIsShutDown);

  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethod(this, &WMFMediaDataDecoder::ProcessDrain);
  mTaskQueue->Dispatch(runnable.forget());
  return NS_OK;
}

bool
WMFMediaDataDecoder::IsHardwareAccelerated(nsACString& aFailureReason) const {
  MOZ_ASSERT(!mIsShutDown);

  return mMFTManager && mMFTManager->IsHardwareAccelerated(aFailureReason);
}

nsresult
WMFMediaDataDecoder::ConfigurationChanged(const TrackInfo& aConfig)
{
  MOZ_ASSERT(mCallback->OnReaderTaskQueue());

  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethodWithArg<UniquePtr<TrackInfo>&&>(
    this,
    &WMFMediaDataDecoder::ProcessConfigurationChanged,
    aConfig.Clone());
  mTaskQueue->Dispatch(runnable.forget());
  return NS_OK;

}

void
WMFMediaDataDecoder::ProcessConfigurationChanged(UniquePtr<TrackInfo>&& aConfig)
{
  if (mMFTManager) {
    mMFTManager->ConfigurationChanged(*aConfig);
  }
}

} // namespace mozilla
