/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_BENCHMARK_H
#define MOZILLA_BENCHMARK_H

#include "MediaDataDemuxer.h"
#include "PlatformDecoderModule.h"
#include "QueueObject.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"

namespace mozilla {

class TaskQueue;
class Benchmark;

class BenchmarkPlayback : public QueueObject {
  friend class Benchmark;
  BenchmarkPlayback(Benchmark* aGlobalState, MediaDataDemuxer* aDemuxer);
  void DemuxSamples();
  void DemuxNextSample();
  void GlobalShutdown();
  void InitDecoder(UniquePtr<TrackInfo>&& aInfo);

  void Output(MediaDataDecoder::DecodedData&& aResults);
  void Error(const MediaResult& aError);
  void InputExhausted();

  // Shutdown trackdemuxer and demuxer if any and shutdown the task queues.
  void FinalizeShutdown();

  Atomic<Benchmark*> mGlobalState;

  RefPtr<TaskQueue> mDecoderTaskQueue;
  RefPtr<MediaDataDecoder> mDecoder;

  // Object only accessed on Thread()
  RefPtr<MediaDataDemuxer> mDemuxer;
  RefPtr<MediaTrackDemuxer> mTrackDemuxer;
  nsTArray<RefPtr<MediaRawData>> mSamples;
  UniquePtr<TrackInfo> mInfo;
  size_t mSampleIndex;
  Maybe<TimeStamp> mDecodeStartTime;
  uint32_t mFrameCount;
  bool mFinished;
  bool mDrained;
};

// Init() must have been called at least once prior on the
// main thread.
class Benchmark : public QueueObject {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Benchmark)

  struct Parameters {
    Parameters()
        : mFramesToMeasure(UINT32_MAX),
          mStartupFrame(1),
          mTimeout(TimeDuration::Forever()) {}

    Parameters(uint32_t aFramesToMeasure, uint32_t aStartupFrame,
               uint32_t aStopAtFrame, const TimeDuration& aTimeout)
        : mFramesToMeasure(aFramesToMeasure),
          mStartupFrame(aStartupFrame),
          mStopAtFrame(Some(aStopAtFrame)),
          mTimeout(aTimeout) {}

    const uint32_t mFramesToMeasure;
    const uint32_t mStartupFrame;
    const Maybe<uint32_t> mStopAtFrame;
    const TimeDuration mTimeout;
  };

  typedef MozPromise<uint32_t, MediaResult, /* IsExclusive = */ true>
      BenchmarkPromise;

  explicit Benchmark(MediaDataDemuxer* aDemuxer,
                     const Parameters& aParameters = Parameters());
  RefPtr<BenchmarkPromise> Run();

  // Must be called on the main thread.
  static void Init();

 private:
  friend class BenchmarkPlayback;
  virtual ~Benchmark();
  void ReturnResult(uint32_t aDecodeFps);
  void ReturnError(const MediaResult& aError);
  void Dispose();
  const Parameters mParameters;
  RefPtr<Benchmark> mKeepAliveUntilComplete;
  BenchmarkPlayback mPlaybackState;
  MozPromiseHolder<BenchmarkPromise> mPromise;
};

class VP9Benchmark {
 public:
  static bool IsVP9DecodeFast(bool aDefault = false);
  static const char* sBenchmarkFpsPref;
  static const char* sBenchmarkFpsVersionCheck;
  static const uint32_t sBenchmarkVersionID;
  static bool sHasRunTest;
  // Return the value of media.benchmark.vp9.fps preference (which will be 0 if
  // not known)
  static uint32_t MediaBenchmarkVp9Fps();

 private:
  static bool ShouldRun();
};
}  // namespace mozilla

#endif
