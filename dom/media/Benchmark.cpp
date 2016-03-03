/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Benchmark.h"
#include "DecoderTraits.h"
#include "PDMFactory.h"
#include "WebMDemuxer.h"
#include "BufferMediaResource.h"
#include "WebMSample.h"
#include "mozilla/Preferences.h"

namespace mozilla {

const char* Benchmark::sBenchmarkFpsPref = "media.benchmark.fps";
bool Benchmark::sHasRunTest = false;

const uint32_t BenchmarkDecoder::sStartupFrames = 1;

bool
Benchmark::IsVP9DecodeFast()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!sHasRunTest) {
    sHasRunTest = true;

    if (!Preferences::HasUserValue(sBenchmarkFpsPref)) {
      RefPtr<Benchmark> estimiser = new Benchmark();
    }
  }

  if (!Preferences::HasUserValue(sBenchmarkFpsPref)) {
    return false;
  }

  uint32_t decodeFps = Preferences::GetUint(sBenchmarkFpsPref);
  uint32_t threshold =
    Preferences::GetUint("media.benchmark.vp9.threshold", 150);

  return decodeFps >= threshold;
}

Benchmark::Benchmark()
  : QueueObject(AbstractThread::MainThread())
  , mKeepAliveUntilComplete(this)
  , mPlaybackState(this)
{
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<Benchmark> self = this;
  mPlaybackState.Dispatch(
    NS_NewRunnableFunction([self]() { self->mPlaybackState.DemuxSamples(); }));
}

void
Benchmark::SaveResult(uint32_t aDecodeFps)
{
  MOZ_ASSERT(NS_IsMainThread());

  Preferences::SetUint(sBenchmarkFpsPref, aDecodeFps);
}

void
Benchmark::Drain(RefPtr<MediaDataDecoder> aDecoder)
{
  RefPtr<Benchmark> self = this;
  mPlaybackState.Dispatch(NS_NewRunnableFunction([self, aDecoder]() {
    self->mPlaybackState.Drain(aDecoder);
  }));
}

void
Benchmark::Dispose()
{
  MOZ_ASSERT(NS_IsMainThread());

  mPlaybackState.Shutdown();
  mKeepAliveUntilComplete = nullptr;
}

bool
Benchmark::IsOnPlaybackThread()
{
  return mPlaybackState.OnThread();
}

BenchmarkPlayback::BenchmarkPlayback(Benchmark* aMainThreadState)
  : QueueObject(new TaskQueue(GetMediaThreadPool(MediaThreadType::PLAYBACK)))
  , mMainThreadState(aMainThreadState)
  , mDecoderState(aMainThreadState, new FlushableTaskQueue(GetMediaThreadPool(
                                      MediaThreadType::PLATFORM_DECODER)))
{
}

void
BenchmarkPlayback::DemuxSamples()
{
  MOZ_ASSERT(OnThread());

  RefPtr<MediaDataDemuxer> demuxer = new WebMDemuxer(
    new BufferMediaResource(sWebMSample, sizeof(sWebMSample), nullptr,
                            NS_LITERAL_CSTRING("video/webm")));
  RefPtr<Benchmark> ref = mMainThreadState;
  demuxer->Init()->Then(
    Thread(), __func__,
    [this, ref, demuxer](nsresult aResult) {
      MOZ_ASSERT(OnThread());
      RefPtr<MediaTrackDemuxer> track =
        demuxer->GetTrackDemuxer(TrackInfo::kVideoTrack, 0);

      RefPtr<MediaTrackDemuxer::SamplesPromise> promise = track->GetSamples(8);

      promise->Then(
        ref->Thread(), __func__,
        [this, ref, track](RefPtr<MediaTrackDemuxer::SamplesHolder> aHolder) {
          mDecoderState.Init(Move(*track->GetInfo()), aHolder->mSamples);
        },
        [ref](DemuxerFailureReason aReason) { ref->Dispose(); });
    },
    [ref](DemuxerFailureReason aReason) { ref->Dispose(); });
}

void
BenchmarkPlayback::Drain(RefPtr<MediaDataDecoder> aDecoder)
{
  MOZ_ASSERT(OnThread());

  aDecoder->Drain();
}

void
BenchmarkPlayback::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());

  mDecoderState.Thread()->AsTaskQueue()->BeginShutdown();
  Thread()->AsTaskQueue()->BeginShutdown();
}

BenchmarkDecoder::BenchmarkDecoder(Benchmark* aMainThreadState,
                                   RefPtr<FlushableTaskQueue> aTaskQueue)
  : QueueObject(aTaskQueue)
  , mTaskQueue(aTaskQueue)
  , mMainThreadState(aMainThreadState)
  , mSampleIndex(0)
  , mFrameCount(0)
  , mFramesToMeasure(Preferences::GetUint("media.benchmark.frames", 300))
  , mTimeout(TimeDuration::FromMilliseconds(
      Preferences::GetUint("media.benchmark.timeout", 1000)))
  , mFinished(false)
{
  MOZ_ASSERT(NS_IsMainThread());
}

void
BenchmarkDecoder::Init(TrackInfo&& aInfo,
                       nsTArray<RefPtr<MediaRawData> >& aSamples)
{
  MOZ_ASSERT(NS_IsMainThread());

  mSamples = aSamples;

  RefPtr<PDMFactory> platform = new PDMFactory();
  platform->Init();

  mDecoder = platform->CreateDecoder(aInfo, mTaskQueue, this);
  RefPtr<Benchmark> ref = mMainThreadState;
  mDecoder->Init()->Then(
    ref->Thread(), __func__,
    [this, ref, aSamples](TrackInfo::TrackType aTrackType) {
      Dispatch(NS_NewRunnableFunction([this, ref]() { InputExhausted(); }));
    },
    [this, ref](MediaDataDecoder::DecoderFailureReason aReason) {
      ref->Dispose();
    });
}

void
BenchmarkDecoder::MainThreadShutdown()
{
  MOZ_ASSERT(OnThread());

  RefPtr<Benchmark> ref = mMainThreadState;
  ref->Dispatch(NS_NewRunnableFunction([ref]() { ref->Dispose(); }));
}

void
BenchmarkDecoder::Output(MediaData* aData)
{
  MOZ_ASSERT(OnThread());

  if (mFinished) {
    return;
  }
  mFrameCount++;
  if (mFrameCount == sStartupFrames) {
    mDecodeStartTime = TimeStamp::Now();
  }
  uint32_t frames = mFrameCount - sStartupFrames;
  TimeDuration elapsedTime = TimeStamp::Now() - mDecodeStartTime;
  if (frames == mFramesToMeasure || elapsedTime >= mTimeout) {
    uint32_t decodeFps = frames / elapsedTime.ToSeconds();
    mFinished = true;

    RefPtr<Benchmark> ref = mMainThreadState;
    RefPtr<MediaDataDecoder> decoder = mDecoder;
    ref->Dispatch(NS_NewRunnableFunction([ref, decoder, decodeFps]() {
      ref->Drain(decoder);
      ref->SaveResult(decodeFps);
    }));
  }
}

void
BenchmarkDecoder::Error()
{
  MOZ_ASSERT(OnThread());

  MainThreadShutdown();
}

void
BenchmarkDecoder::InputExhausted()
{
  MOZ_ASSERT(OnThread());

  mDecoder->Input(mSamples[mSampleIndex]);
  mSampleIndex++;
  if (mSampleIndex == mSamples.Length()) {
    mSampleIndex = 0;
  }
}

void
BenchmarkDecoder::DrainComplete()
{
  MOZ_ASSERT(OnThread());

  MainThreadShutdown();
}

bool
BenchmarkDecoder::OnReaderTaskQueue()
{
  return mMainThreadState->IsOnPlaybackThread();
}
}
