/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DecoderBenchmark.h"
#include "mozilla/BenchmarkStorageChild.h"
#include "mozilla/media/MediaUtils.h"

namespace mozilla {

void DecoderBenchmark::StoreScore(const nsACString& aDecoderName,
                                  const nsACString& aKey,
                                  RefPtr<FrameStatistics> aStats) {
  uint64_t parsedFrames = aStats->GetParsedFrames();
  uint64_t droppedFrames = aStats->GetDroppedFrames();

  MOZ_ASSERT(droppedFrames <= parsedFrames);
  MOZ_ASSERT(parsedFrames >= mLastParsedFrames);
  MOZ_ASSERT(droppedFrames >= mLastDroppedFrames);

  uint64_t diffParsedFrames = parsedFrames - mLastParsedFrames;
  uint64_t diffDroppedFrames = droppedFrames - mLastDroppedFrames;

  /* Update now in case the method returns at the if check bellow. */
  mLastParsedFrames = parsedFrames;
  mLastDroppedFrames = droppedFrames;

  /* A minimum number of 10 frames is required to store the score. */
  if (diffParsedFrames < 10) {
    return;
  }

  int32_t percentage =
      100 - 100 * float(diffDroppedFrames) / float(diffParsedFrames);

  MOZ_ASSERT(percentage >= 0);

  Put(aDecoderName, aKey, percentage);
}

void DecoderBenchmark::Put(const nsACString& aDecoderName,
                           const nsACString& aKey, int32_t aValue) {
  MOZ_ASSERT(NS_IsMainThread());
  const nsCString name(aDecoderName);
  const nsCString key(aKey);
  BenchmarkStorageChild::Instance()->SendPut(name, key, aValue);
}

RefPtr<BenchmarkScorePromise> DecoderBenchmark::GetScore(
    const nsACString& aDecoderName, const nsACString& aKey) {
  if (NS_IsMainThread()) {
    return Get(aDecoderName, aKey);
  }

  RefPtr<DecoderBenchmark> self = this;
  const nsCString decoderName(aDecoderName);
  const nsCString key(aKey);
  return InvokeAsync(
      GetMainThreadSerialEventTarget(), __func__,
      [self, decoderName, key] { return self->Get(decoderName, key); });
}

RefPtr<BenchmarkScorePromise> DecoderBenchmark::Get(
    const nsACString& aDecoderName, const nsACString& aKey) {
  MOZ_ASSERT(NS_IsMainThread());

  const nsCString name(aDecoderName);
  const nsCString key(aKey);
  return BenchmarkStorageChild::Instance()->SendGet(name, key)->Then(
      GetCurrentThreadSerialEventTarget(), __func__,
      [](int32_t aResult) {
        return BenchmarkScorePromise::CreateAndResolve(aResult, __func__);
      },
      [](ipc::ResponseRejectReason&&) {
        return BenchmarkScorePromise::CreateAndReject(NS_ERROR_FAILURE,
                                                      __func__);
      });
}

/* The key string consists of the video properties resolution, framerate,
 * and bitdepth. There are various levels for each of them. The key is
 * formated by the closest level, for example, a video with width=1920,
 * height=1080, frameRate=24, and bitdepth=8bit will have the key:
 * "ResolutionLevel5-FrameRateLevel1-8bit". */

#define NELEMS(x) (sizeof(x) / sizeof((x)[0]))

/* Keep them sorted */
const uint32_t PixelLevels[] = {
    /* 256x144   =*/36864,
    /* 426x240   =*/102240,
    /* 640x360   =*/230400,
    /* 854x480   =*/409920,
    /* 1280x720  =*/921600,
    /* 1920x1080 =*/2073600,
    /* 2560x1440 =*/3686400,
    /* 3840x2160 =*/8294400,
};
const size_t PixelLevelsSize = NELEMS(PixelLevels);

const uint32_t FrameRateLevels[] = {
    15, 24, 30, 50, 60,
};
const size_t FrameRateLevelsSize = NELEMS(FrameRateLevels);

nsCString FindLevel(const uint32_t aLevels[], const size_t length,
                    uint32_t aValue) {
  MOZ_ASSERT(aValue);
  if (aValue <= aLevels[0]) {
    return NS_LITERAL_CSTRING("Level0");
  }
  nsAutoCString level("Level");
  size_t lastIndex = length - 1;
  if (aValue >= aLevels[lastIndex]) {
    level.AppendInt(static_cast<uint32_t>(lastIndex));
    return std::move(level);
  }
  for (size_t i = 0; i < lastIndex; ++i) {
    if (aValue >= aLevels[i + 1]) {
      continue;
    }
    if (aValue - aLevels[i] < aLevels[i + 1] - aValue) {
      level.AppendInt(static_cast<uint32_t>(i));
      return std::move(level);
    }
    level.AppendInt(static_cast<uint32_t>(i + 1));
    return std::move(level);
  }
  MOZ_CRASH("Array is not sorted");
  return NS_LITERAL_CSTRING("");
}

nsCString BitDepthToStr(uint8_t aBitDepth) {
  switch (aBitDepth) {
    case 8:  // ColorDepth::COLOR_8
      return NS_LITERAL_CSTRING("-8bit");
    case 10:  // ColorDepth::COLOR_10
    case 12:  // ColorDepth::COLOR_12
    case 16:  // ColorDepth::COLOR_16
      return NS_LITERAL_CSTRING("-non8bit");
  }
  MOZ_ASSERT_UNREACHABLE("invalid color depth value");
  return NS_LITERAL_CSTRING("");
}

nsCString CreateStoreKey(const DecoderBenchmarkInfo& aBenchInfo) {
  nsAutoCString key("Resolution");
  key.Append(FindLevel(PixelLevels, PixelLevelsSize,
                       aBenchInfo.mWidth * aBenchInfo.mHeight));

  key.Append("-FrameRate");
  key.Append(
      FindLevel(FrameRateLevels, FrameRateLevelsSize, aBenchInfo.mFrameRate));

  key.Append(BitDepthToStr(aBenchInfo.mBitDepth));

  return std::move(key);
}

void DecoderBenchmark::Store(const DecoderBenchmarkInfo& aBenchInfo,
                             RefPtr<FrameStatistics> aStats) {
  if (!XRE_IsContentProcess()) {
    NS_WARNING(
        "Storing a benchmark is only allowed only from the content process.");
    return;
  }
  StoreScore(aBenchInfo.mContentType, CreateStoreKey(aBenchInfo), aStats);
}

/* static */
RefPtr<BenchmarkScorePromise> DecoderBenchmark::Get(
    const DecoderBenchmarkInfo& aBenchInfo) {
  if (!XRE_IsContentProcess()) {
    NS_WARNING(
        "Getting a benchmark is only allowed only from the content process.");
    return BenchmarkScorePromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }
  // There is no need for any of the data members to query the database, thus
  // it can be a static method.
  auto bench = MakeRefPtr<DecoderBenchmark>();
  return bench->GetScore(aBenchInfo.mContentType, CreateStoreKey(aBenchInfo));
}

static nsDataHashtable<nsCStringHashKey, int32_t> DecoderVersionTable() {
  nsDataHashtable<nsCStringHashKey, int32_t> decoderVersionTable;

  /*
   * For the decoders listed here, the benchmark version number will be checked.
   * If the version number does not exist in the database or is different than
   * the version number listed here, all the benchmark entries for this decoder
   * will be erased. An example of assigning the version number `1` for AV1
   * decoder is:
   *
   * decoderVersionTable.Put(NS_LITERAL_CSTRING("video/av1"), 1);
   *
   * For the decoders not listed here the `CheckVersion` method exits early, to
   * avoid sending unecessary IPC messages.
   */

  return decoderVersionTable;
}

/* static */
void DecoderBenchmark::CheckVersion(const nsACString& aDecoderName) {
  if (!XRE_IsContentProcess()) {
    NS_WARNING(
        "Checking version is only allowed only from the content process.");
    return;
  }

  nsCString name(aDecoderName);
  int32_t version;
  if (!DecoderVersionTable().Get(name, &version)) {
    // A version is not set for that decoder ignore.
    return;
  }

  if (NS_IsMainThread()) {
    BenchmarkStorageChild::Instance()->SendCheckVersion(name, version);
    return;
  }

  DebugOnly<nsresult> rv =
      GetMainThreadEventTarget()->Dispatch(NS_NewRunnableFunction(
          "DecoderBenchmark::CheckVersion", [name, version]() {
            BenchmarkStorageChild::Instance()->SendCheckVersion(name, version);
          }));
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

}  // namespace mozilla
