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
#include "WebMDemuxer.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/ContentChild.h"

#ifndef MOZ_WIDGET_ANDROID
#include "WebMSample.h"
#endif

namespace mozilla {

// Update this version number to force re-running the benchmark. Such as when
// an improvement to FFVP9 or LIBVPX is deemed worthwhile.
const uint32_t VP9Benchmark::sBenchmarkVersionID = 1;

const char* VP9Benchmark::sBenchmarkFpsPref = "media.benchmark.vp9.fps";
const char* VP9Benchmark::sBenchmarkFpsVersionCheck = "media.benchmark.vp9.versioncheck";
bool VP9Benchmark::sHasRunTest = false;

// static
bool
VP9Benchmark::IsVP9DecodeFast()
{
  MOZ_ASSERT(NS_IsMainThread());

#ifdef MOZ_WIDGET_ANDROID
  return true;
#else
  bool hasPref = Preferences::HasUserValue(sBenchmarkFpsPref);
  uint32_t hadRecentUpdate = Preferences::GetUint(sBenchmarkFpsVersionCheck, 0U);

  if (!sHasRunTest && (!hasPref || hadRecentUpdate != sBenchmarkVersionID)) {
    sHasRunTest = true;

    RefPtr<WebMDemuxer> demuxer =
      new WebMDemuxer(new BufferMediaResource(sWebMSample, sizeof(sWebMSample), nullptr,
                                              NS_LITERAL_CSTRING("video/webm")));
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
          Preferences::SetUint(sBenchmarkFpsVersionCheck, sBenchmarkVersionID);
        }
        Telemetry::Accumulate(Telemetry::ID::VIDEO_VP9_BENCHMARK_FPS, aDecodeFps);
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
  : QueueObject(AbstractThread::GetCurrent())
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
  mPlaybackState.Dispatch(
    NS_NewRunnableFunction([self]() { self->mPlaybackState.DemuxSamples(); }));
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

  MediaPrefs::GetSingleton();
}

BenchmarkPlayback::BenchmarkPlayback(Benchmark* aMainThreadState,
                                     MediaDataDemuxer* aDemuxer)
  : QueueObject(new TaskQueue(GetMediaThreadPool(MediaThreadType::PLAYBACK)))
  , mMainThreadState(aMainThreadState)
  , mDecoderTaskQueue(new TaskQueue(GetMediaThreadPool(
                        MediaThreadType::PLATFORM_DECODER)))
  , mDemuxer(aDemuxer)
  , mSampleIndex(0)
  , mFrameCount(0)
  , mFinished(false)
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
    [this, ref](DemuxerFailureReason aReason) { MainThreadShutdown(); });
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
      if (ref->mParameters.mStopAtFrame &&
          mSamples.Length() == (size_t)ref->mParameters.mStopAtFrame.ref()) {
        InitDecoder(Move(*mTrackDemuxer->GetInfo()));
      } else {
        Dispatch(NS_NewRunnableFunction([this, ref]() { DemuxNextSample(); }));
      }
    },
    [this, ref](DemuxerFailureReason aReason) {
      switch (aReason) {
        case DemuxerFailureReason::END_OF_STREAM:
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
  mDecoder = platform->CreateDecoder(aInfo, mDecoderTaskQueue, this,
     /* DecoderDoctorDiagnostics* */ nullptr);
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
    [this, ref](MediaDataDecoder::DecoderFailureReason aReason) {
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
    mDecoder->Flush();
    mDecoder->Shutdown();
    mDecoder = nullptr;
  }

  mDecoderTaskQueue->BeginShutdown();
  mDecoderTaskQueue->AwaitShutdownAndIdle();
  mDecoderTaskQueue = nullptr;

  if (mTrackDemuxer) {
    mTrackDemuxer->Reset();
    mTrackDemuxer->BreakCycles();
    mTrackDemuxer = nullptr;
  }

  RefPtr<Benchmark> ref(mMainThreadState);
  Thread()->AsTaskQueue()->BeginShutdown()->Then(
    ref->Thread(), __func__,
    [ref]() {  ref->Dispose(); },
    []() { MOZ_CRASH("not reached"); });
}

void
BenchmarkPlayback::Output(MediaData* aData)
{
  RefPtr<Benchmark> ref(mMainThreadState);
  Dispatch(NS_NewRunnableFunction([this, ref]() {
    mFrameCount++;
    if (mFrameCount == ref->mParameters.mStartupFrame) {
      mDecodeStartTime = TimeStamp::Now();
    }
    int32_t frames = mFrameCount - ref->mParameters.mStartupFrame;
    TimeDuration elapsedTime = TimeStamp::Now() - mDecodeStartTime;
    if (!mFinished &&
        (frames == ref->mParameters.mFramesToMeasure ||
         elapsedTime >= ref->mParameters.mTimeout)) {
      uint32_t decodeFps = frames / elapsedTime.ToSeconds();
      MainThreadShutdown();
      ref->Dispatch(NS_NewRunnableFunction([ref, decodeFps]() {
        ref->ReturnResult(decodeFps);
      }));
    }
  }));
}

void
BenchmarkPlayback::Error()
{
  RefPtr<Benchmark> ref(mMainThreadState);
  Dispatch(NS_NewRunnableFunction([this, ref]() {  MainThreadShutdown(); }));
}

void
BenchmarkPlayback::InputExhausted()
{
  RefPtr<Benchmark> ref(mMainThreadState);
  Dispatch(NS_NewRunnableFunction([this, ref]() {
    MOZ_ASSERT(OnThread());
    if (mFinished || mSampleIndex >= mSamples.Length()) {
      return;
    }
    mDecoder->Input(mSamples[mSampleIndex]);
    mSampleIndex++;
    if (mSampleIndex == mSamples.Length()) {
      if (ref->mParameters.mStopAtFrame) {
        mSampleIndex = 0;
      } else {
        mDecoder->Drain();
      }
    }
  }));
}

void
BenchmarkPlayback::DrainComplete()
{
  RefPtr<Benchmark> ref(mMainThreadState);
  Dispatch(NS_NewRunnableFunction([this, ref]() {
    int32_t frames = mFrameCount - ref->mParameters.mStartupFrame;
    TimeDuration elapsedTime = TimeStamp::Now() - mDecodeStartTime;
    uint32_t decodeFps = frames / elapsedTime.ToSeconds();
    MainThreadShutdown();
    ref->Dispatch(NS_NewRunnableFunction([ref, decodeFps]() {
      ref->ReturnResult(decodeFps);
    }));
  }));
}

bool
BenchmarkPlayback::OnReaderTaskQueue()
{
  return OnThread();
}

}
