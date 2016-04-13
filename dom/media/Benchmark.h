/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_BENCHMARK_H
#define MOZILLA_BENCHMARK_H

#include "MediaDataDemuxer.h"
#include "QueueObject.h"
#include "PlatformDecoderModule.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/TimeStamp.h"
#include "nsCOMPtr.h"

namespace mozilla {

class FlushableTaskQueue;
class Benchmark;

class BenchmarkPlayback : public QueueObject, private MediaDataDecoderCallback
{
  friend class Benchmark;
  explicit BenchmarkPlayback(Benchmark* aMainThreadState, MediaDataDemuxer* aDemuxer);
  void DemuxSamples();
  void DemuxNextSample();
  void MainThreadShutdown();
  void InitDecoder(TrackInfo&& aInfo);

  // MediaDataDecoderCallback
  // Those methods are called on the MediaDataDecoder's task queue.
  void Output(MediaData* aData) override;
  void Error() override;
  void InputExhausted() override;
  void DrainComplete() override;
  bool OnReaderTaskQueue() override;

  Atomic<Benchmark*> mMainThreadState;

  RefPtr<FlushableTaskQueue> mDecoderTaskQueue;
  RefPtr<MediaDataDecoder> mDecoder;

  // Object only accessed on Thread()
  RefPtr<MediaDataDemuxer> mDemuxer;
  RefPtr<MediaTrackDemuxer> mTrackDemuxer;
  nsTArray<RefPtr<MediaRawData>> mSamples;
  size_t mSampleIndex;
  TimeStamp mDecodeStartTime;
  uint32_t mFrameCount;
  bool mFinished;
};

// Init() must have been called at least once prior on the
// main thread.
class Benchmark : public QueueObject
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Benchmark)

  struct Parameters
  {
    Parameters()
      : mFramesToMeasure(-1)
      , mStartupFrame(1)
      , mTimeout(TimeDuration::Forever()) {}

    Parameters(int32_t aFramesToMeasure,
               uint32_t aStartupFrame,
               int32_t aStopAtFrame,
               const TimeDuration& aTimeout)
      : mFramesToMeasure(aFramesToMeasure)
      , mStartupFrame(aStartupFrame)
      , mStopAtFrame(Some(aStopAtFrame))
      , mTimeout(aTimeout) {}

    const int32_t mFramesToMeasure;
    const uint32_t mStartupFrame;
    const Maybe<int32_t> mStopAtFrame;
    const TimeDuration mTimeout;
  };

  typedef MozPromise<uint32_t, bool, /* IsExclusive = */ true> BenchmarkPromise;

  explicit Benchmark(MediaDataDemuxer* aDemuxer, const Parameters& aParameters = Parameters());
  RefPtr<BenchmarkPromise> Run();

  static void Init();

private:
  friend class BenchmarkPlayback;
  virtual ~Benchmark();
  void ReturnResult(uint32_t aDecodeFps);
  void Dispose();
  const Parameters mParameters;
  RefPtr<Benchmark> mKeepAliveUntilComplete;
  BenchmarkPlayback mPlaybackState;
  MozPromiseHolder<BenchmarkPromise> mPromise;
};

class VP9Benchmark
{
public:
  static bool IsVP9DecodeFast();
  static const char* sBenchmarkFpsPref;
  static const char* sBenchmarkFpsVersionCheck;
  static const uint32_t sBenchmarkVersionID;
  static bool sHasRunTest;
};
}

#endif
