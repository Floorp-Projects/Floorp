/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFMediaDataDecoder.h"

#include "VideoUtils.h"
#include "WMFUtils.h"
#include "mozilla/Logging.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/Telemetry.h"
#include "nsTArray.h"

#define LOG(...) MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, (__VA_ARGS__))

namespace mozilla {

WMFMediaDataDecoder::WMFMediaDataDecoder(MFTManager* aMFTManager)
    : mTaskQueue(
          new TaskQueue(GetMediaThreadPool(MediaThreadType::PLATFORM_DECODER),
                        "WMFMediaDataDecoder")),
      mMFTManager(aMFTManager) {}

WMFMediaDataDecoder::~WMFMediaDataDecoder() {}

RefPtr<MediaDataDecoder::InitPromise> WMFMediaDataDecoder::Init() {
  MOZ_ASSERT(!mIsShutDown);
  return InitPromise::CreateAndResolve(mMFTManager->GetType(), __func__);
}

// A single telemetry sample is reported for each MediaDataDecoder object
// that has detected error or produced output successfully.
static void SendTelemetry(unsigned long hr) {
  // Collapse the error codes into a range of 0-0xff that can be viewed in
  // telemetry histograms.  For most MF_E_* errors, unique samples are used,
  // retaining the least significant 7 or 8 bits.  Other error codes are
  // bucketed.
  uint32_t sample;
  if (SUCCEEDED(hr)) {
    sample = 0;
  } else if (hr < 0xc00d36b0) {
    sample = 1;  // low bucket
  } else if (hr < 0xc00d3700) {
    sample = hr & 0xffU;  // MF_E_*
  } else if (hr <= 0xc00d3705) {
    sample = 0x80 + (hr & 0xfU);  // more MF_E_*
  } else if (hr < 0xc00d6d60) {
    sample = 2;  // mid bucket
  } else if (hr <= 0xc00d6d78) {
    sample = hr & 0xffU;  // MF_E_TRANSFORM_*
  } else {
    sample = 3;  // high bucket
  }
}

RefPtr<ShutdownPromise> WMFMediaDataDecoder::Shutdown() {
  MOZ_DIAGNOSTIC_ASSERT(!mIsShutDown);
  mIsShutDown = true;

  return InvokeAsync(mTaskQueue, __func__, [self = RefPtr{this}, this] {
    if (mMFTManager) {
      mMFTManager->Shutdown();
      mMFTManager = nullptr;
      if (!mRecordedError && mHasSuccessfulOutput) {
        SendTelemetry(S_OK);
      }
    }
    return mTaskQueue->BeginShutdown();
  });
}

// Inserts data into the decoder's pipeline.
RefPtr<MediaDataDecoder::DecodePromise> WMFMediaDataDecoder::Decode(
    MediaRawData* aSample) {
  MOZ_DIAGNOSTIC_ASSERT(!mIsShutDown);

  return InvokeAsync<MediaRawData*>(
      mTaskQueue, this, __func__, &WMFMediaDataDecoder::ProcessDecode, aSample);
}

RefPtr<MediaDataDecoder::DecodePromise> WMFMediaDataDecoder::ProcessError(
    HRESULT aError, const char* aReason) {
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  if (!mRecordedError) {
    SendTelemetry(aError);
    mRecordedError = true;
  }

  nsPrintfCString markerString(
      "WMFMediaDataDecoder::ProcessError for decoder with description %s with "
      "reason: %s",
      GetDescriptionName().get(), aReason);
  LOG(markerString.get());
  PROFILER_MARKER_TEXT("WMFDecoder Error", MEDIA_PLAYBACK, {}, markerString);

  // TODO: For the error DXGI_ERROR_DEVICE_RESET, we could return
  // NS_ERROR_DOM_MEDIA_NEED_NEW_DECODER to get the latest device. Maybe retry
  // up to 3 times.
  return DecodePromise::CreateAndReject(
      MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                  RESULT_DETAIL("%s:%x", aReason, aError)),
      __func__);
}

RefPtr<MediaDataDecoder::DecodePromise> WMFMediaDataDecoder::ProcessDecode(
    MediaRawData* aSample) {
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  DecodedData results;
  HRESULT hr = mMFTManager->Input(aSample);
  if (hr == MF_E_NOTACCEPTING) {
    hr = ProcessOutput(results);
    if (FAILED(hr) && hr != MF_E_TRANSFORM_NEED_MORE_INPUT) {
      return ProcessError(hr, "MFTManager::Output(1)");
    }
    hr = mMFTManager->Input(aSample);
  }

  if (FAILED(hr)) {
    NS_WARNING("MFTManager rejected sample");
    return ProcessError(hr, "MFTManager::Input");
  }

  // MFT decoder sometime will return incorrect first output to us, which always
  // has 0 timestamp, even if the input we gave to MFT has timestamp that is way
  // later than 0. In order to distinguish that incorrect output with the real
  // first frame, we set the seek threshold to the first sample time that will
  // help us ignore any output time that is earlier than the first input time.
  // Only does that when manager doesn't have seek threshold, because we don't
  // want to mess up the seeking triggered from the higher level of media stack.
  if (!mHasGuardedAgainstIncorrectFirstSample &&
      !mMFTManager->HasSeekThreshold()) {
    mHasGuardedAgainstIncorrectFirstSample = true;
    mMFTManager->SetSeekThreshold(aSample->mTime);
  }

  if (!mLastTime || aSample->mTime > *mLastTime) {
    mLastTime = Some(aSample->mTime);
    mLastDuration = aSample->mDuration;
  }

  mSamplesCount++;
  mDrainStatus = DrainStatus::DRAINABLE;
  mLastStreamOffset = aSample->mOffset;

  hr = ProcessOutput(results);
  if (SUCCEEDED(hr) || hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
    return DecodePromise::CreateAndResolve(std::move(results), __func__);
  }
  return ProcessError(hr, "MFTManager::Output(2)");
}

HRESULT
WMFMediaDataDecoder::ProcessOutput(DecodedData& aResults) {
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  RefPtr<MediaData> output;
  HRESULT hr = S_OK;
  while (SUCCEEDED(hr = mMFTManager->Output(mLastStreamOffset, output))) {
    MOZ_ASSERT(output.get(), "Upon success, we must receive an output");
    mHasSuccessfulOutput = true;
    aResults.AppendElement(std::move(output));
    if (mDrainStatus == DrainStatus::DRAINING) {
      break;
    }
  }
  return hr;
}

RefPtr<MediaDataDecoder::FlushPromise> WMFMediaDataDecoder::ProcessFlush() {
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  if (mMFTManager) {
    mMFTManager->Flush();
  }
  mDrainStatus = DrainStatus::DRAINED;
  mSamplesCount = 0;
  mLastTime.reset();
  mHasGuardedAgainstIncorrectFirstSample = false;
  return FlushPromise::CreateAndResolve(true, __func__);
}

RefPtr<MediaDataDecoder::FlushPromise> WMFMediaDataDecoder::Flush() {
  MOZ_DIAGNOSTIC_ASSERT(!mIsShutDown);

  return InvokeAsync(mTaskQueue, this, __func__,
                     &WMFMediaDataDecoder::ProcessFlush);
}

RefPtr<MediaDataDecoder::DecodePromise> WMFMediaDataDecoder::ProcessDrain() {
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  if (!mMFTManager || mDrainStatus == DrainStatus::DRAINED) {
    return DecodePromise::CreateAndResolve(DecodedData(), __func__);
  }

  if (mDrainStatus != DrainStatus::DRAINING) {
    // Order the decoder to drain...
    mMFTManager->Drain();
    mDrainStatus = DrainStatus::DRAINING;
  }

  // Then extract all available output.
  DecodedData results;
  HRESULT hr = ProcessOutput(results);
  if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
    mDrainStatus = DrainStatus::DRAINED;
  }
  if (SUCCEEDED(hr) || hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
    if (results.Length() > 0 &&
        results.LastElement()->mType == MediaData::Type::VIDEO_DATA) {
      const RefPtr<MediaData>& data = results.LastElement();
      if (mSamplesCount == 1 && data->mTime == media::TimeUnit::Zero()) {
        // WMF is unable to calculate a duration if only a single sample
        // was parsed. Additionally, the pts always comes out at 0 under those
        // circumstances.
        // Seeing that we've only fed the decoder a single frame, the pts
        // and duration are known, it's of the last sample.
        data->mTime = *mLastTime;
      }
      if (data->mTime == *mLastTime) {
        // The WMF Video decoder is sometimes unable to provide a valid duration
        // on the last sample even if it has been first set through
        // SetSampleTime (appears to always happen on Windows 7). So we force
        // set the duration of the last sample as it was input.
        data->mDuration = mLastDuration;
      }
    } else if (results.Length() == 1 &&
               results.LastElement()->mType == MediaData::Type::AUDIO_DATA) {
      // When we drain the audio decoder and one frame was queued (such as with
      // AAC) the MFT will re-calculate the starting time rather than use the
      // value set on the IMF Sample.
      // This is normally an okay thing to do; however when dealing with poorly
      // muxed content that has incorrect start time, it could lead to broken
      // A/V sync. So we ensure that we use the compressed sample's time
      // instead. Additionally, this is what all other audio decoders are doing
      // anyway.
      MOZ_ASSERT(mLastTime,
                 "We must have attempted to decode at least one frame to get "
                 "one decoded output");
      results.LastElement()->As<AudioData>()->SetOriginalStartTime(*mLastTime);
    }
    return DecodePromise::CreateAndResolve(std::move(results), __func__);
  }
  return ProcessError(hr, "MFTManager::Output");
}

RefPtr<MediaDataDecoder::DecodePromise> WMFMediaDataDecoder::Drain() {
  MOZ_DIAGNOSTIC_ASSERT(!mIsShutDown);

  return InvokeAsync(mTaskQueue, this, __func__,
                     &WMFMediaDataDecoder::ProcessDrain);
}

bool WMFMediaDataDecoder::IsHardwareAccelerated(
    nsACString& aFailureReason) const {
  MOZ_ASSERT(!mIsShutDown);

  return mMFTManager && mMFTManager->IsHardwareAccelerated(aFailureReason);
}

void WMFMediaDataDecoder::SetSeekThreshold(const media::TimeUnit& aTime) {
  MOZ_DIAGNOSTIC_ASSERT(!mIsShutDown);

  RefPtr<WMFMediaDataDecoder> self = this;
  nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction(
      "WMFMediaDataDecoder::SetSeekThreshold", [self, aTime]() {
        MOZ_ASSERT(self->mTaskQueue->IsCurrentThreadIn());
        media::TimeUnit threshold = aTime;
        self->mMFTManager->SetSeekThreshold(threshold);
      });
  nsresult rv = mTaskQueue->Dispatch(runnable.forget());
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
  Unused << rv;
}

}  // namespace mozilla
