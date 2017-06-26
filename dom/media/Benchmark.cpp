/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Benchmark.h"

#include "BufferMediaResource.h"
#include "MediaData.h"
#include "MediaPrefs.h"
#include "PDMFactory.h"
#include "VideoUtils.h"
#include "WebMDemuxer.h"
#include "gfxPrefs.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/Preferences.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/gfx/gfxVars.h"

#ifndef MOZ_WIDGET_ANDROID
#include "WebMSample.h"
#endif

using namespace mozilla::gfx;

namespace mozilla {

// Update this version number to force re-running the benchmark. Such as when
// an improvement to FFVP9 or LIBVPX is deemed worthwhile.
const uint32_t VP9Benchmark::sBenchmarkVersionID = 2;

const char* VP9Benchmark::sBenchmarkFpsPref = "media.benchmark.vp9.fps";
const char* VP9Benchmark::sBenchmarkFpsVersionCheck = "media.benchmark.vp9.versioncheck";
bool VP9Benchmark::sHasRunTest = false;

// static
bool
VP9Benchmark::IsVP9DecodeFast()
{
  MOZ_ASSERT(NS_IsMainThread());

#ifdef MOZ_WIDGET_ANDROID
  return false;
#else
  bool hasPref = Preferences::HasUserValue(sBenchmarkFpsPref);
  uint32_t hadRecentUpdate = Preferences::GetUint(sBenchmarkFpsVersionCheck, 0U);

  if (!sHasRunTest && (!hasPref || hadRecentUpdate != sBenchmarkVersionID)) {
    sHasRunTest = true;

    RefPtr<WebMDemuxer> demuxer = new WebMDemuxer(
      new BufferMediaResource(sWebMSample, sizeof(sWebMSample), nullptr));
    RefPtr<Benchmark> estimiser =
      new Benchmark(demuxer,
                    {
                      Preferences::GetInt("media.benchmark.frames", 300), // frames to measure
                      1, // start benchmarking after decoding this frame.
                      8, // loop after decoding that many frames.
                      TimeDuration::FromMilliseconds(
                        Preferences::GetUint("media.benchmark.timeout", 1000))
                    });
    estimiser->Run()->Then(
      SystemGroup::AbstractMainThreadFor(TaskCategory::Other), __func__,
      [](uint32_t aDecodeFps) {
        if (XRE_IsContentProcess()) {
          dom::ContentChild* contentChild = dom::ContentChild::GetSingleton();
          if (contentChild) {
            contentChild->SendNotifyBenchmarkResult(NS_LITERAL_STRING("VP9"),
                                                    aDecodeFps);
          }
        } else {
          Preferences::SetUint(sBenchmarkFpsPref, aDecodeFps);
          Preferences::SetUint(sBenchmarkFpsVersionCheck, sBenchmarkVersionID);
        }
        Telemetry::Accumulate(Telemetry::HistogramID::VIDEO_VP9_BENCHMARK_FPS, aDecodeFps);
      },
      []() { });
  }

  if (!hasPref) {
    return false;
  }

  uint32_t decodeFps = Preferences::GetUint(sBenchmarkFpsPref);
  uint32_t threshold =
    Preferences::GetUint("media.benchmark.vp9.threshold", 150);

  return decodeFps >= threshold;
#endif
}

Benchmark::Benchmark(MediaDataDemuxer* aDemuxer, const Parameters& aParameters)
  : QueueObject(AbstractThread::MainThread())
  , mParameters(aParameters)
  , mKeepAliveUntilComplete(this)
  , mPlaybackState(this, aDemuxer)
{
  MOZ_COUNT_CTOR(Benchmark);
  MOZ_ASSERT(Thread(), "Must be run in task queue");
}

Benchmark::~Benchmark()
{
  MOZ_COUNT_DTOR(Benchmark);
}

RefPtr<Benchmark::BenchmarkPromise>
Benchmark::Run()
{
  MOZ_ASSERT(OnThread());

  RefPtr<BenchmarkPromise> p = mPromise.Ensure(__func__);
  RefPtr<Benchmark> self = this;
  mPlaybackState.Dispatch(NS_NewRunnableFunction(
    "Benchmark::Run", [self]() { self->mPlaybackState.DemuxSamples(); }));
  return p;
}

void
Benchmark::ReturnResult(uint32_t aDecodeFps)
{
  MOZ_ASSERT(OnThread());

  mPromise.ResolveIfExists(aDecodeFps, __func__);
}

void
Benchmark::Dispose()
{
  MOZ_ASSERT(OnThread());

  mKeepAliveUntilComplete = nullptr;
  mPromise.RejectIfExists(false, __func__);
}

void
Benchmark::Init()
{
  MOZ_ASSERT(NS_IsMainThread());
  gfxVars::Initialize();
  gfxPrefs::GetSingleton();
  MediaPrefs::GetSingleton();
}

BenchmarkPlayback::BenchmarkPlayback(Benchmark* aMainThreadState,
                                     MediaDataDemuxer* aDemuxer)
  : QueueObject(new TaskQueue(
      GetMediaThreadPool(MediaThreadType::PLAYBACK),
      "BenchmarkPlayback::QueueObject"))
  , mMainThreadState(aMainThreadState)
  , mDecoderTaskQueue(new TaskQueue(
      GetMediaThreadPool(MediaThreadType::PLATFORM_DECODER),
      "BenchmarkPlayback::mDecoderTaskQueue"))
  , mDemuxer(aDemuxer)
  , mSampleIndex(0)
  , mFrameCount(0)
  , mFinished(false)
  , mDrained(false)
{
  MOZ_ASSERT(static_cast<Benchmark*>(mMainThreadState)->OnThread());
}

void
BenchmarkPlayback::DemuxSamples()
{
  MOZ_ASSERT(OnThread());

  RefPtr<Benchmark> ref(mMainThreadState);
  mDemuxer->Init()->Then(
    Thread(), __func__,
    [this, ref](nsresult aResult) {
      MOZ_ASSERT(OnThread());
      mTrackDemuxer =
        mDemuxer->GetTrackDemuxer(TrackInfo::kVideoTrack, 0);
      if (!mTrackDemuxer) {
        MainThreadShutdown();
        return;
      }
      DemuxNextSample();
    },
    [this, ref](const MediaResult& aError) { MainThreadShutdown(); });
}

void
BenchmarkPlayback::DemuxNextSample()
{
  MOZ_ASSERT(OnThread());

  RefPtr<Benchmark> ref(mMainThreadState);
  RefPtr<MediaTrackDemuxer::SamplesPromise> promise = mTrackDemuxer->GetSamples();
  promise->Then(
    Thread(), __func__,
    [this, ref](RefPtr<MediaTrackDemuxer::SamplesHolder> aHolder) {
      mSamples.AppendElements(Move(aHolder->mSamples));
      if (ref->mParameters.mStopAtFrame
          && mSamples.Length() == (size_t)ref->mParameters.mStopAtFrame.ref()) {
        InitDecoder(Move(*mTrackDemuxer->GetInfo()));
      } else {
        Dispatch(NS_NewRunnableFunction("BenchmarkPlayback::DemuxNextSample",
                                        [this, ref]() { DemuxNextSample(); }));
      }
    },
    [this, ref](const MediaResult& aError) {
      switch (aError.Code()) {
        case NS_ERROR_DOM_MEDIA_END_OF_STREAM:
          InitDecoder(Move(*mTrackDemuxer->GetInfo()));
          break;
        default:
          MainThreadShutdown();
      }
    });
}

void
BenchmarkPlayback::InitDecoder(TrackInfo&& aInfo)
{
  MOZ_ASSERT(OnThread());

  RefPtr<PDMFactory> platform = new PDMFactory();
  mDecoder = platform->CreateDecoder({ aInfo, mDecoderTaskQueue });
  if (!mDecoder) {
    MainThreadShutdown();
    return;
  }
  RefPtr<Benchmark> ref(mMainThreadState);
  mDecoder->Init()->Then(
    Thread(), __func__,
    [this, ref](TrackInfo::TrackType aTrackType) {
      InputExhausted();
    },
    [this, ref](const MediaResult& aError) {
      MainThreadShutdown();
    });
}

void
BenchmarkPlayback::MainThreadShutdown()
{
  MOZ_ASSERT(OnThread());

  if (mFinished) {
    // Nothing more to do.
    return;
  }
  mFinished = true;

  if (mDecoder) {
    RefPtr<Benchmark> ref(mMainThreadState);
    mDecoder->Flush()->Then(
      Thread(), __func__,
      [ref, this]() {
        mDecoder->Shutdown()->Then(
          Thread(), __func__,
          [ref, this]() {
            mDecoderTaskQueue->BeginShutdown();
            mDecoderTaskQueue->AwaitShutdownAndIdle();
            mDecoderTaskQueue = nullptr;

            if (mTrackDemuxer) {
              mTrackDemuxer->Reset();
              mTrackDemuxer->BreakCycles();
              mTrackDemuxer = nullptr;
            }

            Thread()->AsTaskQueue()->BeginShutdown()->Then(
              ref->Thread(), __func__,
              [ref]() { ref->Dispose(); },
              []() { MOZ_CRASH("not reached"); });
          },
          []() { MOZ_CRASH("not reached"); });
        mDecoder = nullptr;
      },
      []() { MOZ_CRASH("not reached"); });
  }
}

void
BenchmarkPlayback::Output(const MediaDataDecoder::DecodedData& aResults)
{
  MOZ_ASSERT(OnThread());
  RefPtr<Benchmark> ref(mMainThreadState);
  mFrameCount += aResults.Length();
  if (!mDecodeStartTime && mFrameCount >= ref->mParameters.mStartupFrame) {
    mDecodeStartTime = Some(TimeStamp::Now());
  }
  TimeStamp now = TimeStamp::Now();
  int32_t frames = mFrameCount - ref->mParameters.mStartupFrame;
  TimeDuration elapsedTime = now - mDecodeStartTime.refOr(now);
  if (!mFinished
      && (((frames == ref->mParameters.mFramesToMeasure) && frames > 0)
          || elapsedTime >= ref->mParameters.mTimeout
          || mDrained)) {
    uint32_t decodeFps = frames / elapsedTime.ToSeconds();
    MainThreadShutdown();
    ref->Dispatch(
      NS_NewRunnableFunction("BenchmarkPlayback::Output", [ref, decodeFps]() {
        ref->ReturnResult(decodeFps);
      }));
  }
}

void
BenchmarkPlayback::InputExhausted()
{
  MOZ_ASSERT(OnThread());
  if (mFinished || mSampleIndex >= mSamples.Length()) {
    return;
  }
  RefPtr<Benchmark> ref(mMainThreadState);
  mDecoder->Decode(mSamples[mSampleIndex])
    ->Then(Thread(), __func__,
           [ref, this](const MediaDataDecoder::DecodedData& aResults) {
             Output(aResults);
             InputExhausted();
           },
           [ref, this](const MediaResult& aError) { MainThreadShutdown(); });
  mSampleIndex++;
  if (mSampleIndex == mSamples.Length()) {
    if (ref->mParameters.mStopAtFrame) {
      mSampleIndex = 0;
    } else {
      mDecoder->Drain()->Then(
        Thread(), __func__,
        [ref, this](const MediaDataDecoder::DecodedData& aResults) {
          mDrained = true;
          Output(aResults);
        },
        [ref, this](const MediaResult& aError) { MainThreadShutdown(); });
    }
  }
}

} // namespace mozilla
