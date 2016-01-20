/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"
#include "AudioCompactor.h"
#include "MediaDecoderReader.h"

using mozilla::AudioCompactor;
using mozilla::AudioData;
using mozilla::AudioDataValue;
using mozilla::MediaDecoderReader;
using mozilla::MediaQueue;

class MemoryFunctor : public nsDequeFunctor {
public:
  MemoryFunctor() : mSize(0) {}
  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf);

  void* operator()(void* aObject) override {
    const AudioData* audioData = static_cast<const AudioData*>(aObject);
    mSize += audioData->SizeOfIncludingThis(MallocSizeOf);
    return nullptr;
  }

  size_t mSize;
};

class TestCopy
{
public:
  TestCopy(uint32_t aFrames, uint32_t aChannels,
           uint32_t &aCallCount, uint32_t &aFrameCount)
    : mFrames(aFrames)
    , mChannels(aChannels)
    , mCallCount(aCallCount)
    , mFrameCount(aFrameCount)
  { }

  uint32_t operator()(AudioDataValue *aBuffer, uint32_t aSamples)
  {
    mCallCount += 1;
    uint32_t frames = std::min(mFrames - mFrameCount, aSamples / mChannels);
    mFrameCount += frames;
    return frames;
  }

private:
  const uint32_t mFrames;
  const uint32_t mChannels;
  uint32_t &mCallCount;
  uint32_t &mFrameCount;
};

static void TestAudioCompactor(size_t aBytes)
{
  MediaQueue<AudioData> queue;
  AudioCompactor compactor(queue);

  uint64_t offset = 0;
  uint64_t time = 0;
  uint32_t sampleRate = 44000;
  uint32_t channels = 2;
  uint32_t frames = aBytes / (channels * sizeof(AudioDataValue));
  size_t maxSlop = aBytes / AudioCompactor::MAX_SLOP_DIVISOR;

  uint32_t callCount = 0;
  uint32_t frameCount = 0;

  compactor.Push(offset, time, sampleRate, frames, channels,
                 TestCopy(frames, channels, callCount, frameCount));

  EXPECT_GT(callCount, 0U) << "copy functor never called";
  EXPECT_EQ(frames, frameCount) << "incorrect number of frames copied";

  MemoryFunctor memoryFunc;
  queue.LockedForEach(memoryFunc);
  size_t allocSize = memoryFunc.mSize - (callCount * sizeof(AudioData));
  size_t slop = allocSize - aBytes;
  EXPECT_LE(slop, maxSlop) << "allowed too much allocation slop";
}

TEST(Media, AudioCompactor_4000)
{
  TestAudioCompactor(4000);
}

TEST(Media, AudioCompactor_4096)
{
  TestAudioCompactor(4096);
}

TEST(Media, AudioCompactor_5000)
{
  TestAudioCompactor(5000);
}

TEST(Media, AudioCompactor_5256)
{
  TestAudioCompactor(5256);
}

TEST(Media, AudioCompactor_NativeCopy)
{
  const uint32_t channels = 2;
  const size_t srcBytes = 32;
  const uint32_t srcSamples = srcBytes / sizeof(AudioDataValue);
  const uint32_t srcFrames = srcSamples / channels;
  uint8_t src[srcBytes];

  for (uint32_t i = 0; i < srcBytes; ++i) {
    src[i] = i;
  }

  AudioCompactor::NativeCopy copy(src, srcBytes, channels);

  const uint32_t dstSamples = srcSamples * 2;
  AudioDataValue dst[dstSamples];

  const AudioDataValue notCopied = 0xffff;
  for (uint32_t i = 0; i < dstSamples; ++i) {
    dst[i] = notCopied;
  }

  const uint32_t copyCount = 8;
  uint32_t copiedFrames = 0;
  uint32_t nextSample = 0;
  for (uint32_t i = 0; i < copyCount; ++i) {
    uint32_t copySamples = dstSamples / copyCount;
    copiedFrames += copy(dst + nextSample, copySamples);
    nextSample += copySamples;
  }

  EXPECT_EQ(srcFrames, copiedFrames) << "copy exact number of source frames";

  // Verify that the only the correct bytes were copied.
  for (uint32_t i = 0; i < dstSamples; ++i) {
    if (i < srcSamples) {
      EXPECT_NE(notCopied, dst[i]) << "should have copied over these bytes";
    } else {
      EXPECT_EQ(notCopied, dst[i]) << "should not have copied over these bytes";
    }
  }
}
