/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFMediaDataDecoder.h"
#include "VideoUtils.h"
#include "WMFUtils.h"
#include "mozilla/Telemetry.h"
#include "nsTArray.h"

#include "mozilla/Logging.h"
#include "mozilla/SyncRunnable.h"

#define LOG(...) MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, (__VA_ARGS__))

namespace mozilla {

WMFMediaDataDecoder::WMFMediaDataDecoder(MFTManager* aMFTManager,
                                         TaskQueue* aTaskQueue)
  : mTaskQueue(aTaskQueue)
  , mMFTManager(aMFTManager)
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

RefPtr<ShutdownPromise>
WMFMediaDataDecoder::Shutdown()
{
  MOZ_DIAGNOSTIC_ASSERT(!mIsShutDown);

  mIsShutDown = true;

  if (mTaskQueue) {
    return InvokeAsync(mTaskQueue, this, __func__,
                       &WMFMediaDataDecoder::ProcessShutdown);
  }
  return ProcessShutdown();
}

RefPtr<ShutdownPromise>
WMFMediaDataDecoder::ProcessShutdown()
{
  if (mMFTManager) {
    mMFTManager->Shutdown();
    mMFTManager = nullptr;
    if (!mRecordedError && mHasSuccessfulOutput) {
      SendTelemetry(S_OK);
    }
  }
  return ShutdownPromise::CreateAndResolve(true, __func__);
}

// Inserts data into the decoder's pipeline.
RefPtr<MediaDataDecoder::DecodePromise>
WMFMediaDataDecoder::Decode(MediaRawData* aSample)
{
  MOZ_DIAGNOSTIC_ASSERT(!mIsShutDown);

  return InvokeAsync<MediaRawData*>(mTaskQueue, this, __func__,
                                    &WMFMediaDataDecoder::ProcessDecode,
                                    aSample);
}

RefPtr<MediaDataDecoder::DecodePromise>
WMFMediaDataDecoder::ProcessDecode(MediaRawData* aSample)
{
  HRESULT hr = mMFTManager->Input(aSample);
  if (hr == MF_E_NOTACCEPTING) {
    ProcessOutput();
    hr = mMFTManager->Input(aSample);
  }

  if (FAILED(hr)) {
    NS_WARNING("MFTManager rejected sample");
    if (!mRecordedError) {
      SendTelemetry(hr);
      mRecordedError = true;
    }
    return DecodePromise::CreateAndReject(
      MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                  RESULT_DETAIL("MFTManager::Input:%x", hr)),
      __func__);
  }

  mDrainStatus = DrainStatus::DRAINABLE;
  mLastStreamOffset = aSample->mOffset;

  RefPtr<DecodePromise> p = mDecodePromise.Ensure(__func__);
  ProcessOutput();
  return p;
}

void
WMFMediaDataDecoder::ProcessOutput()
{
  RefPtr<MediaData> output;
  HRESULT hr = S_OK;
  DecodedData results;
  while (SUCCEEDED(hr = mMFTManager->Output(mLastStreamOffset, output))) {
    MOZ_ASSERT(output.get(), "Upon success, we must receive an output");
    mHasSuccessfulOutput = true;
    results.AppendElement(Move(output));
    if (mDrainStatus == DrainStatus::DRAINING) {
      break;
    }
  }

  if (SUCCEEDED(hr) || hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT && !mDrainPromise.IsEmpty()) {
      mDrainStatus = DrainStatus::DRAINED;
    }
    if (!mDecodePromise.IsEmpty()) {
      mDecodePromise.Resolve(Move(results), __func__);
    } else {
      mDrainPromise.Resolve(Move(results), __func__);
    }
    return;
  }

  NS_WARNING("WMFMediaDataDecoder failed to output data");
  const auto error = MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                                  RESULT_DETAIL("MFTManager::Output:%x", hr));
  if (!mDecodePromise.IsEmpty()) {
    mDecodePromise.Reject(error, __func__);
  }
  else {
    mDrainPromise.Reject(error, __func__);
  }

  if (!mRecordedError) {
    SendTelemetry(hr);
    mRecordedError = true;
  }
}

RefPtr<MediaDataDecoder::FlushPromise>
WMFMediaDataDecoder::ProcessFlush()
{
  if (mMFTManager) {
    mMFTManager->Flush();
  }
  mDrainStatus = DrainStatus::DRAINED;
  return FlushPromise::CreateAndResolve(true, __func__);
}

RefPtr<MediaDataDecoder::FlushPromise>
WMFMediaDataDecoder::Flush()
{
  MOZ_DIAGNOSTIC_ASSERT(!mIsShutDown);

  return InvokeAsync(mTaskQueue, this, __func__,
                     &WMFMediaDataDecoder::ProcessFlush);
}

RefPtr<MediaDataDecoder::DecodePromise>
WMFMediaDataDecoder::ProcessDrain()
{
  if (!mMFTManager || mDrainStatus == DrainStatus::DRAINED) {
    return DecodePromise::CreateAndResolve(DecodedData(), __func__);
  }
  if (mDrainStatus != DrainStatus::DRAINING) {
    // Order the decoder to drain...
    mMFTManager->Drain();
    mDrainStatus = DrainStatus::DRAINING;
  }
  // Then extract all available output.
  RefPtr<DecodePromise> p = mDrainPromise.Ensure(__func__);
  ProcessOutput();
  return p;
}

RefPtr<MediaDataDecoder::DecodePromise>
WMFMediaDataDecoder::Drain()
{
  MOZ_DIAGNOSTIC_ASSERT(!mIsShutDown);

  return InvokeAsync(mTaskQueue, this, __func__,
                     &WMFMediaDataDecoder::ProcessDrain);
}

bool
WMFMediaDataDecoder::IsHardwareAccelerated(nsACString& aFailureReason) const {
  MOZ_ASSERT(!mIsShutDown);

  return mMFTManager && mMFTManager->IsHardwareAccelerated(aFailureReason);
}

void
WMFMediaDataDecoder::SetSeekThreshold(const media::TimeUnit& aTime)
{
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
