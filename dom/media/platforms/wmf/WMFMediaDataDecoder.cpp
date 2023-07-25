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
    : mTaskQueue(TaskQueue::Create(
          GetMediaThreadPool(MediaThreadType::PLATFORM_DECODER),
          "WMFMediaDataDecoder")),
      mMFTManager(aMFTManager) {}

WMFMediaDataDecoder::~WMFMediaDataDecoder() {}

RefPtr<MediaDataDecoder::InitPromise> WMFMediaDataDecoder::Init() {
  MOZ_ASSERT(!mIsShutDown);
  return InitPromise::CreateAndResolve(mMFTManager->GetType(), __func__);
}

RefPtr<ShutdownPromise> WMFMediaDataDecoder::Shutdown() {
  MOZ_DIAGNOSTIC_ASSERT(!mIsShutDown);
  mIsShutDown = true;

  return InvokeAsync(mTaskQueue, __func__, [self = RefPtr{this}, this] {
    if (mMFTManager) {
      mMFTManager->Shutdown();
      mMFTManager = nullptr;
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

  nsPrintfCString markerString(
      "WMFMediaDataDecoder::ProcessError for decoder with description %s with "
      "reason: %s",
      GetDescriptionName().get(), aReason);
  LOG("%s", markerString.get());
  PROFILER_MARKER_TEXT("WMFDecoder Error", MEDIA_PLAYBACK, {}, markerString);

  // TODO: For the error DXGI_ERROR_DEVICE_RESET, we could return
  // NS_ERROR_DOM_MEDIA_NEED_NEW_DECODER to get the latest device. Maybe retry
  // up to 3 times.
  return DecodePromise::CreateAndReject(
      MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                  RESULT_DETAIL("%s:%lx", aReason, aError)),
      __func__);
}

RefPtr<MediaDataDecoder::DecodePromise> WMFMediaDataDecoder::ProcessDecode(
    MediaRawData* aSample) {
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  DecodedData results;
  LOG("ProcessDecode, type=%s, sample=%" PRId64,
      TrackTypeToStr(mMFTManager->GetType()), aSample->mTime.ToMicroseconds());
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

  if (mOutputsCount == 0) {
    mInputTimesSet.insert(aSample->mTime.ToMicroseconds());
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

bool WMFMediaDataDecoder::ShouldGuardAgaintIncorrectFirstSample(
    MediaData* aOutput) const {
  // Incorrect first samples have only been observed in video tracks, so only
  // guard video tracks.
  if (mMFTManager->GetType() != TrackInfo::kVideoTrack) {
    return false;
  }

  // This is not the first output sample so we don't need to guard it.
  if (mOutputsCount != 0) {
    return false;
  }

  // Output isn't in the map which contains the inputs we gave to the decoder.
  // This is probably the invalid first sample. MFT decoder sometime will return
  // incorrect first output to us, which always has 0 timestamp, even if the
  // input we gave to MFT has timestamp that is way later than 0.
  MOZ_ASSERT(!mInputTimesSet.empty());
  return mInputTimesSet.find(aOutput->mTime.ToMicroseconds()) ==
             mInputTimesSet.end() &&
         aOutput->mTime.ToMicroseconds() == 0;
}

HRESULT
WMFMediaDataDecoder::ProcessOutput(DecodedData& aResults) {
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  RefPtr<MediaData> output;
  HRESULT hr = S_OK;
  while (SUCCEEDED(hr = mMFTManager->Output(mLastStreamOffset, output))) {
    MOZ_ASSERT(output.get(), "Upon success, we must receive an output");
    if (ShouldGuardAgaintIncorrectFirstSample(output)) {
      LOG("Discarding sample with time %" PRId64
          " because of ShouldGuardAgaintIncorrectFirstSample check",
          output->mTime.ToMicroseconds());
      continue;
    }
    if (++mOutputsCount == 1) {
      // Got first valid sample, don't need to guard following sample anymore.
      mInputTimesSet.clear();
    }
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
  LOG("ProcessFlush, type=%s", TrackTypeToStr(mMFTManager->GetType()));
  mDrainStatus = DrainStatus::DRAINED;
  mSamplesCount = 0;
  mOutputsCount = 0;
  mLastTime.reset();
  mInputTimesSet.clear();
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
