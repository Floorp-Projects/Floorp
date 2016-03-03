/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_BENCHMARK_H
#define MOZILLA_BENCHMARK_H

#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "QueueObject.h"

namespace mozilla {

class FlushableTaskQueue;
class Benchmark;
class BenchmarkPlayback;

class BenchmarkDecoder : public QueueObject, private MediaDataDecoderCallback
{
public:
  BenchmarkDecoder(Benchmark* aMainThreadState,
                   RefPtr<FlushableTaskQueue> aTaskQueue);
  void Init(TrackInfo&& aInfo, nsTArray<RefPtr<MediaRawData>>& aSamples);

  void MainThreadShutdown();
  MediaRawData* PopNextSample();

  void Output(MediaData* aData) override;
  void Error() override;
  void InputExhausted() override;
  void DrainComplete() override;
  bool OnReaderTaskQueue() override;

  RefPtr<FlushableTaskQueue> mTaskQueue;
  Benchmark* mMainThreadState;
  RefPtr<MediaDataDecoder> mDecoder;
  nsTArray<RefPtr<MediaRawData>> mSamples;
  size_t mSampleIndex;
  TimeStamp mDecodeStartTime;
  uint32_t mFrameCount;
  const uint32_t mFramesToMeasure;
  const TimeDuration mTimeout;
  static const uint32_t sStartupFrames;
  bool mFinished;
};

class BenchmarkPlayback : public QueueObject
{
public:
  explicit BenchmarkPlayback(Benchmark* aMainThreadState);
  void DemuxSamples();
  void Drain(RefPtr<MediaDataDecoder> aDecoder);
  void Shutdown();

private:
  Benchmark* mMainThreadState;
  BenchmarkDecoder mDecoderState;
};

class Benchmark : public QueueObject
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Benchmark)

  static bool IsVP9DecodeFast();

  void SaveResult(uint32_t aDecodeFps);
  void Drain(RefPtr<MediaDataDecoder> aDecoder);
  void Dispose();
  bool IsOnPlaybackThread();

private:
  Benchmark();
  virtual ~Benchmark() {}
  RefPtr<Benchmark> mKeepAliveUntilComplete;
  BenchmarkPlayback mPlaybackState;
  static const char* sBenchmarkFpsPref;
  static bool sHasRunTest;
};
}

#endif
