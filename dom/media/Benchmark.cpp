/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Benchmark.h"

#include "BufferMediaResource.h"
#include "MediaData.h"
#include "PDMFactory.h"
#include "VideoUtils.h"
#include "WebMDemuxer.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/Preferences.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/gfx/gfxVars.h"
#include "nsIGfxInfo.h"

#ifndef MOZ_WIDGET_ANDROID
#  include "WebMSample.h"
#endif

using namespace mozilla::gfx;

namespace mozilla {

// Update this version number to force re-running the benchmark. Such as when
// an improvement to FFVP9 or LIBVPX is deemed worthwhile.
const uint32_t VP9Benchmark::sBenchmarkVersionID = 5;

const char* VP9Benchmark::sBenchmarkFpsPref = "media.benchmark.vp9.fps";
const char* VP9Benchmark::sBenchmarkFpsVersionCheck =
    "media.benchmark.vp9.versioncheck";
bool VP9Benchmark::sHasRunTest = false;

// static
bool VP9Benchmark::ShouldRun() {
#if defined(MOZ_WIDGET_ANDROID)
  // Assume that the VP9 software decoder will always be too slow.
  return false;
#else
#  if defined(MOZ_APPLEMEDIA)
  const nsCOMPtr<nsIGfxInfo> gfxInfo = services::GetGfxInfo();
  nsString vendorID, deviceID;
  gfxInfo->GetAdapterVendorID(vendorID);
  // We won't run the VP9 benchmark on mac using an Intel GPU as performance are
  // poor, see bug 1404042.
  if (vendorID.EqualsLiteral("0x8086")) {
    return false;
  }
  // Fall Through
#  endif
  return true;
#endif
}

// static
uint32_t VP9Benchmark::MediaBenchmarkVp9Fps() {
  if (!ShouldRun()) {
    return 0;
  }
  return StaticPrefs::media_benchmark_vp9_fps();
}

// static
bool VP9Benchmark::IsVP9DecodeFast(bool aDefault) {
#if defined(MOZ_WIDGET_ANDROID)
  return false;
#else
  if (!ShouldRun()) {
    return false;
  }
  static StaticMutex sMutex;
  uint32_t decodeFps = StaticPrefs::media_benchmark_vp9_fps();
  uint32_t hadRecentUpdate = StaticPrefs::media_benchmark_vp9_versioncheck();
  bool needBenchmark;
  {
    StaticMutexAutoLock lock(sMutex);
    needBenchmark = !sHasRunTest &&
                    (decodeFps == 0 || hadRecentUpdate != sBenchmarkVersionID);
    sHasRunTest = true;
  }

  if (needBenchmark) {
    RefPtr<WebMDemuxer> demuxer = new WebMDemuxer(
        new BufferMediaResource(sWebMSample, sizeof(sWebMSample)));
    RefPtr<Benchmark> estimiser = new Benchmark(
        demuxer,
        {StaticPrefs::media_benchmark_frames(),  // frames to measure
         1,  // start benchmarking after decoding this frame.
         8,  // loop after decoding that many frames.
         TimeDuration::FromMilliseconds(
             StaticPrefs::media_benchmark_timeout())});
    estimiser->Run()->Then(
        AbstractThread::MainThread(), __func__,
        [](uint32_t aDecodeFps) {
          if (XRE_IsContentProcess()) {
            dom::ContentChild* contentChild = dom::ContentChild::GetSingleton();
            if (contentChild) {
              contentChild->SendNotifyBenchmarkResult(NS_LITERAL_STRING("VP9"),
                                                      aDecodeFps);
            }
          } else {
            Preferences::SetUint(sBenchmarkFpsPref, aDecodeFps);
            Preferences::SetUint(sBenchmarkFpsVersionCheck,
                                 sBenchmarkVersionID);
          }
          Telemetry::Accumulate(Telemetry::HistogramID::VIDEO_VP9_BENCHMARK_FPS,
                                aDecodeFps);
        },
        []() {});
  }

  if (decodeFps == 0) {
    return aDefault;
  }

  return decodeFps >= StaticPrefs::media_benchmark_vp9_threshold();
#endif
}

Benchmark::Benchmark(MediaDataDemuxer* aDemuxer, const Parameters& aParameters)
    : QueueObject(new TaskQueue(GetMediaThreadPool(MediaThreadType::PLAYBACK),
                                "Benchmark::QueueObject")),
      mParameters(aParameters),
      mKeepAliveUntilComplete(this),
      mPlaybackState(this, aDemuxer) {
  MOZ_COUNT_CTOR(Benchmark);
}

Benchmark::~Benchmark() { MOZ_COUNT_DTOR(Benchmark); }

RefPtr<Benchmark::BenchmarkPromise> Benchmark::Run() {
  RefPtr<Benchmark> self = this;
  return InvokeAsync(Thread(), __func__, [self] {
    RefPtr<BenchmarkPromise> p = self->mPromise.Ensure(__func__);
    self->mPlaybackState.Dispatch(NS_NewRunnableFunction(
        "Benchmark::Run", [self]() { self->mPlaybackState.DemuxSamples(); }));
    return p;
  });
}

void Benchmark::ReturnResult(uint32_t aDecodeFps) {
  MOZ_ASSERT(OnThread());

  mPromise.ResolveIfExists(aDecodeFps, __func__);
}

void Benchmark::ReturnError(const MediaResult& aError) {
  MOZ_ASSERT(OnThread());

  mPromise.RejectIfExists(aError, __func__);
}

void Benchmark::Dispose() {
  MOZ_ASSERT(OnThread());

  mKeepAliveUntilComplete = nullptr;
}

void Benchmark::Init() {
  MOZ_ASSERT(NS_IsMainThread());
  gfxVars::Initialize();
}

BenchmarkPlayback::BenchmarkPlayback(Benchmark* aGlobalState,
                                     MediaDataDemuxer* aDemuxer)
    : QueueObject(new TaskQueue(GetMediaThreadPool(MediaThreadType::PLAYBACK),
                                "BenchmarkPlayback::QueueObject")),
      mGlobalState(aGlobalState),
      mDecoderTaskQueue(
          new TaskQueue(GetMediaThreadPool(MediaThreadType::PLATFORM_DECODER),
                        "BenchmarkPlayback::mDecoderTaskQueue")),
      mDemuxer(aDemuxer),
      mSampleIndex(0),
      mFrameCount(0),
      mFinished(false),
      mDrained(false) {}

void BenchmarkPlayback::DemuxSamples() {
  MOZ_ASSERT(OnThread());

  RefPtr<Benchmark> ref(mGlobalState);
  mDemuxer->Init()->Then(
      Thread(), __func__,
      [this, ref](nsresult aResult) {
        MOZ_ASSERT(OnThread());
        if (mDemuxer->GetNumberTracks(TrackInfo::kVideoTrack)) {
          mTrackDemuxer = mDemuxer->GetTrackDemuxer(TrackInfo::kVideoTrack, 0);
        } else if (mDemuxer->GetNumberTracks(TrackInfo::kAudioTrack)) {
          mTrackDemuxer = mDemuxer->GetTrackDemuxer(TrackInfo::kAudioTrack, 0);
        }
        if (!mTrackDemuxer) {
          Error(MediaResult(NS_ERROR_FAILURE, "Can't create track demuxer"));
          return;
        }
        DemuxNextSample();
      },
      [this, ref](const MediaResult& aError) { Error(aError); });
}

void BenchmarkPlayback::DemuxNextSample() {
  MOZ_ASSERT(OnThread());

  RefPtr<Benchmark> ref(mGlobalState);
  RefPtr<MediaTrackDemuxer::SamplesPromise> promise =
      mTrackDemuxer->GetSamples();
  promise->Then(
      Thread(), __func__,
      [this, ref](RefPtr<MediaTrackDemuxer::SamplesHolder> aHolder) {
        mSamples.AppendElements(std::move(aHolder->GetMovableSamples()));
        if (ref->mParameters.mStopAtFrame &&
            mSamples.Length() == ref->mParameters.mStopAtFrame.ref()) {
          InitDecoder(mTrackDemuxer->GetInfo());
        } else {
          Dispatch(
              NS_NewRunnableFunction("BenchmarkPlayback::DemuxNextSample",
                                     [this, ref]() { DemuxNextSample(); }));
        }
      },
      [this, ref](const MediaResult& aError) {
        switch (aError.Code()) {
          case NS_ERROR_DOM_MEDIA_END_OF_STREAM:
            InitDecoder(mTrackDemuxer->GetInfo());
            break;
          default:
            Error(aError);
            break;
        }
      });
}

void BenchmarkPlayback::InitDecoder(UniquePtr<TrackInfo>&& aInfo) {
  MOZ_ASSERT(OnThread());

  if (!aInfo) {
    Error(MediaResult(NS_ERROR_FAILURE, "Invalid TrackInfo"));
    return;
  }

  RefPtr<PDMFactory> platform = new PDMFactory();
  mInfo = std::move(aInfo);
  mDecoder = platform->CreateDecoder({*mInfo, mDecoderTaskQueue});
  if (!mDecoder) {
    Error(MediaResult(NS_ERROR_FAILURE, "Failed to create decoder"));
    return;
  }
  RefPtr<Benchmark> ref(mGlobalState);
  mDecoder->Init()->Then(
      Thread(), __func__,
      [this, ref](TrackInfo::TrackType aTrackType) { InputExhausted(); },
      [this, ref](const MediaResult& aError) { Error(aError); });
}

void BenchmarkPlayback::FinalizeShutdown() {
  MOZ_ASSERT(OnThread());

  MOZ_ASSERT(mFinished, "GlobalShutdown must have been run");
  MOZ_ASSERT(!mDecoder, "mDecoder must have been shutdown already");
  MOZ_ASSERT(!mDemuxer, "mDemuxer must have been shutdown already");
  MOZ_DIAGNOSTIC_ASSERT(mDecoderTaskQueue->IsEmpty());
  mDecoderTaskQueue = nullptr;

  RefPtr<Benchmark> ref(mGlobalState);
  ref->Thread()->Dispatch(NS_NewRunnableFunction(
      "BenchmarkPlayback::FinalizeShutdown", [ref]() { ref->Dispose(); }));
}

void BenchmarkPlayback::GlobalShutdown() {
  MOZ_ASSERT(OnThread());

  MOZ_ASSERT(!mFinished, "We've already shutdown");

  mFinished = true;

  if (mTrackDemuxer) {
    mTrackDemuxer->Reset();
    mTrackDemuxer->BreakCycles();
    mTrackDemuxer = nullptr;
  }
  mDemuxer = nullptr;

  if (mDecoder) {
    RefPtr<Benchmark> ref(mGlobalState);
    mDecoder->Flush()->Then(
        Thread(), __func__,
        [ref, this]() {
          mDecoder->Shutdown()->Then(
              Thread(), __func__, [ref, this]() { FinalizeShutdown(); },
              []() { MOZ_CRASH("not reached"); });
          mDecoder = nullptr;
          mInfo = nullptr;
        },
        []() { MOZ_CRASH("not reached"); });
  } else {
    FinalizeShutdown();
  }
}

void BenchmarkPlayback::Output(MediaDataDecoder::DecodedData&& aResults) {
  MOZ_ASSERT(OnThread());
  MOZ_ASSERT(!mFinished);

  RefPtr<Benchmark> ref(mGlobalState);
  mFrameCount += aResults.Length();
  if (!mDecodeStartTime && mFrameCount >= ref->mParameters.mStartupFrame) {
    mDecodeStartTime = Some(TimeStamp::Now());
  }
  TimeStamp now = TimeStamp::Now();
  uint32_t frames = mFrameCount - ref->mParameters.mStartupFrame;
  TimeDuration elapsedTime = now - mDecodeStartTime.refOr(now);
  if (((frames == ref->mParameters.mFramesToMeasure) &&
       mFrameCount > ref->mParameters.mStartupFrame && frames > 0) ||
      elapsedTime >= ref->mParameters.mTimeout || mDrained) {
    uint32_t decodeFps = frames / elapsedTime.ToSeconds();
    GlobalShutdown();
    ref->Dispatch(NS_NewRunnableFunction(
        "BenchmarkPlayback::Output",
        [ref, decodeFps]() { ref->ReturnResult(decodeFps); }));
  }
}

void BenchmarkPlayback::Error(const MediaResult& aError) {
  MOZ_ASSERT(OnThread());

  RefPtr<Benchmark> ref(mGlobalState);
  GlobalShutdown();
  ref->Dispatch(
      NS_NewRunnableFunction("BenchmarkPlayback::Error",
                             [ref, aError]() { ref->ReturnError(aError); }));
}

void BenchmarkPlayback::InputExhausted() {
  MOZ_ASSERT(OnThread());
  MOZ_ASSERT(!mFinished);

  if (mSampleIndex >= mSamples.Length()) {
    Error(MediaResult(NS_ERROR_FAILURE, "Nothing left to decode"));
    return;
  }

  RefPtr<MediaRawData> sample = mSamples[mSampleIndex];
  RefPtr<Benchmark> ref(mGlobalState);
  RefPtr<MediaDataDecoder::DecodePromise> p = mDecoder->Decode(sample);

  mSampleIndex++;
  if (mSampleIndex == mSamples.Length() && !ref->mParameters.mStopAtFrame) {
    // Complete current frame decode then drain if still necessary.
    p->Then(
        Thread(), __func__,
        [ref, this](MediaDataDecoder::DecodedData&& aResults) {
          Output(std::move(aResults));
          if (!mFinished) {
            mDecoder->Drain()->Then(
                Thread(), __func__,
                [ref, this](MediaDataDecoder::DecodedData&& aResults) {
                  mDrained = true;
                  Output(std::move(aResults));
                  MOZ_ASSERT(mFinished, "We must be done now");
                },
                [ref, this](const MediaResult& aError) { Error(aError); });
          }
        },
        [ref, this](const MediaResult& aError) { Error(aError); });
  } else {
    if (mSampleIndex == mSamples.Length() && ref->mParameters.mStopAtFrame) {
      mSampleIndex = 0;
    }
    // Continue decoding
    p->Then(
        Thread(), __func__,
        [ref, this](MediaDataDecoder::DecodedData&& aResults) {
          Output(std::move(aResults));
          if (!mFinished) {
            InputExhausted();
          }
        },
        [ref, this](const MediaResult& aError) { Error(aError); });
  }
}

}  // namespace mozilla
