/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DECODER_BENCHMARK_H
#define MOZILLA_DECODER_BENCHMARK_H

#include "FrameStatistics.h"
#include "mozilla/BenchmarkStorageChild.h"
#include "mozilla/KeyValueStorage.h"

namespace mozilla {

typedef KeyValueStorage::GetPromise BenchmarkScorePromise;

struct DecoderBenchmarkInfo final {
  const nsCString mContentType;
  const int32_t mWidth;
  const int32_t mHeight;
  const int32_t mFrameRate;
  const uint32_t mBitDepth;
};

class DecoderBenchmark final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DecoderBenchmark)

 public:
  void Store(const DecoderBenchmarkInfo& aBenchInfo,
             RefPtr<FrameStatistics> aStats);

  static RefPtr<BenchmarkScorePromise> Get(
      const DecoderBenchmarkInfo& aBenchInfo);

  /* For the specific decoder, specified by aDecoderName, it compares the
   * version number, from a static list of versions, to the version number
   * found in the database. If those numbers are different all benchmark
   * entries for that decoder are deleted. */
  static void CheckVersion(const nsACString& aDecoderName);

 private:
  void StoreScore(const nsACString& aDecoderName, const nsACString& aKey,
                  RefPtr<FrameStatistics> aStats);

  RefPtr<BenchmarkScorePromise> GetScore(const nsACString& aDecoderName,
                                         const nsACString& aKey);

  void Put(const nsACString& aDecoderName, const nsACString& aKey,
           int32_t aValue);

  RefPtr<BenchmarkScorePromise> Get(const nsACString& aDecoderName,
                                    const nsACString& aKey);
  ~DecoderBenchmark() = default;

  // Keep the last ParsedFrames and DroppedFrames from FrameStatistics.
  // FrameStatistics keep an ever-increasing counter across the entire video and
  // even when there are resolution changes. This code is called whenever there
  // is a resolution change and we need to calculate the benchmark since the
  // last call.
  uint64_t mLastParsedFrames = 0;
  uint64_t mLastDroppedFrames = 0;
};

}  // namespace mozilla

#endif  // MOZILLA_DECODER_BENCHMARK_H
