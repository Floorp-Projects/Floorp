/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioSegment.h"
#include <iostream>
#include "gtest/gtest.h"

using namespace mozilla;

namespace audio_segment {

/* Helper function to give us the maximum and minimum value that don't clip,
 * for a given sample format (integer or floating-point). */
template <typename T>
T GetLowValue();

template <typename T>
T GetHighValue();

template <typename T>
T GetSilentValue();

template <>
float GetLowValue<float>() {
  return -1.0;
}

template <>
int16_t GetLowValue<short>() {
  return -INT16_MAX;
}

template <>
float GetHighValue<float>() {
  return 1.0;
}

template <>
int16_t GetHighValue<short>() {
  return INT16_MAX;
}

template <>
float GetSilentValue() {
  return 0.0;
}

template <>
int16_t GetSilentValue() {
  return 0;
}

// Get an array of planar audio buffers that has the inverse of the index of the
// channel (1-indexed) as samples.
template <typename T>
const T* const* GetPlanarChannelArray(size_t aChannels, size_t aSize) {
  T** channels = new T*[aChannels];
  for (size_t c = 0; c < aChannels; c++) {
    channels[c] = new T[aSize];
    for (size_t i = 0; i < aSize; i++) {
      channels[c][i] = FloatToAudioSample<T>(1. / (c + 1));
    }
  }
  return channels;
}

template <typename T>
void DeletePlanarChannelsArray(const T* const* aArrays, size_t aChannels) {
  for (size_t channel = 0; channel < aChannels; channel++) {
    delete[] aArrays[channel];
  }
  delete[] aArrays;
}

template <typename T>
T** GetPlanarArray(size_t aChannels, size_t aSize) {
  T** channels = new T*[aChannels];
  for (size_t c = 0; c < aChannels; c++) {
    channels[c] = new T[aSize];
    for (size_t i = 0; i < aSize; i++) {
      channels[c][i] = 0.0f;
    }
  }
  return channels;
}

template <typename T>
void DeletePlanarArray(T** aArrays, size_t aChannels) {
  for (size_t channel = 0; channel < aChannels; channel++) {
    delete[] aArrays[channel];
  }
  delete[] aArrays;
}

// Get an array of audio samples that have the inverse of the index of the
// channel (1-indexed) as samples.
template <typename T>
const T* GetInterleavedChannelArray(size_t aChannels, size_t aSize) {
  size_t sampleCount = aChannels * aSize;
  T* samples = new T[sampleCount];
  for (size_t i = 0; i < sampleCount; i++) {
    uint32_t channel = (i % aChannels) + 1;
    samples[i] = FloatToAudioSample<T>(1. / channel);
  }
  return samples;
}

template <typename T>
void DeleteInterleavedChannelArray(const T* aArray) {
  delete[] aArray;
}

bool FuzzyEqual(float aLhs, float aRhs) { return std::abs(aLhs - aRhs) < 0.01; }

template <typename SrcT, typename DstT>
void TestInterleaveAndConvert() {
  size_t arraySize = 1024;
  size_t maxChannels = 8;  // 7.1
  for (uint32_t channels = 1; channels < maxChannels; channels++) {
    const SrcT* const* src = GetPlanarChannelArray<SrcT>(channels, arraySize);
    DstT* dst = new DstT[channels * arraySize];

    InterleaveAndConvertBuffer(src, arraySize, 1.0, channels, dst);

    uint32_t channelIndex = 0;
    for (size_t i = 0; i < arraySize * channels; i++) {
      ASSERT_TRUE(FuzzyEqual(
          dst[i], FloatToAudioSample<DstT>(1. / (channelIndex + 1))));
      channelIndex++;
      channelIndex %= channels;
    }

    DeletePlanarChannelsArray(src, channels);
    delete[] dst;
  }
}

template <typename SrcT, typename DstT>
void TestDeinterleaveAndConvert() {
  size_t arraySize = 1024;
  size_t maxChannels = 8;  // 7.1
  for (uint32_t channels = 1; channels < maxChannels; channels++) {
    const SrcT* src = GetInterleavedChannelArray<SrcT>(channels, arraySize);
    DstT** dst = GetPlanarArray<DstT>(channels, arraySize);

    DeinterleaveAndConvertBuffer(src, arraySize, channels, dst);

    for (size_t channel = 0; channel < channels; channel++) {
      for (size_t i = 0; i < arraySize; i++) {
        ASSERT_TRUE(FuzzyEqual(dst[channel][i],
                               FloatToAudioSample<DstT>(1. / (channel + 1))));
      }
    }

    DeleteInterleavedChannelArray(src);
    DeletePlanarArray(dst, channels);
  }
}

uint8_t gSilence[4096] = {0};

template <typename T>
T* SilentChannel() {
  return reinterpret_cast<T*>(gSilence);
}

template <typename T>
void TestUpmixStereo() {
  size_t arraySize = 1024;
  nsTArray<T*> channels;
  nsTArray<const T*> channelsptr;

  channels.SetLength(1);
  channelsptr.SetLength(1);

  channels[0] = new T[arraySize];

  for (size_t i = 0; i < arraySize; i++) {
    channels[0][i] = GetHighValue<T>();
  }
  channelsptr[0] = channels[0];

  AudioChannelsUpMix(&channelsptr, 2, SilentChannel<T>());

  for (size_t channel = 0; channel < 2; channel++) {
    for (size_t i = 0; i < arraySize; i++) {
      ASSERT_TRUE(channelsptr[channel][i] == GetHighValue<T>());
    }
  }
  delete[] channels[0];
}

template <typename T>
void TestDownmixStereo() {
  const size_t arraySize = 1024;
  nsTArray<const T*> inputptr;
  nsTArray<T*> input;
  T** output;

  output = new T*[1];
  output[0] = new T[arraySize];

  input.SetLength(2);
  inputptr.SetLength(2);

  for (size_t channel = 0; channel < input.Length(); channel++) {
    input[channel] = new T[arraySize];
    for (size_t i = 0; i < arraySize; i++) {
      input[channel][i] = channel == 0 ? GetLowValue<T>() : GetHighValue<T>();
    }
    inputptr[channel] = input[channel];
  }

  AudioChannelsDownMix(inputptr, output, 1, arraySize);

  for (size_t i = 0; i < arraySize; i++) {
    ASSERT_TRUE(output[0][i] == GetSilentValue<T>());
    ASSERT_TRUE(output[0][i] == GetSilentValue<T>());
  }

  delete[] output[0];
  delete[] output;
}

TEST(AudioSegment, Test)
{
  TestInterleaveAndConvert<float, float>();
  TestInterleaveAndConvert<float, int16_t>();
  TestInterleaveAndConvert<int16_t, float>();
  TestInterleaveAndConvert<int16_t, int16_t>();
  TestDeinterleaveAndConvert<float, float>();
  TestDeinterleaveAndConvert<float, int16_t>();
  TestDeinterleaveAndConvert<int16_t, float>();
  TestDeinterleaveAndConvert<int16_t, int16_t>();
  TestUpmixStereo<float>();
  TestUpmixStereo<int16_t>();
  TestDownmixStereo<float>();
  TestDownmixStereo<int16_t>();
}

template <class T, uint32_t Channels>
void fillChunk(AudioChunk* aChunk, int aDuration) {
  static_assert(Channels != 0, "Filling 0 channels is a no-op");

  aChunk->mDuration = aDuration;

  AutoTArray<nsTArray<T>, Channels> buffer;
  buffer.SetLength(Channels);
  aChunk->mChannelData.ClearAndRetainStorage();
  aChunk->mChannelData.SetCapacity(Channels);
  for (nsTArray<T>& channel : buffer) {
    T* ch = channel.AppendElements(aDuration);
    for (int i = 0; i < aDuration; ++i) {
      ch[i] = GetHighValue<T>();
    }
    aChunk->mChannelData.AppendElement(ch);
  }

  aChunk->mBuffer = new mozilla::SharedChannelArrayBuffer<T>(std::move(buffer));
  aChunk->mBufferFormat = AudioSampleTypeToFormat<T>::Format;
}

TEST(AudioSegment, FlushAfter_ZeroDuration)
{
  AudioChunk c;
  fillChunk<float, 2>(&c, 10);

  AudioSegment s;
  s.AppendAndConsumeChunk(std::move(c));
  s.FlushAfter(0);
  EXPECT_EQ(s.GetDuration(), 0);
}

TEST(AudioSegment, FlushAfter_SmallerDuration)
{
  // It was crashing when the first chunk was silence (null) and FlushAfter
  // was called for a duration, smaller or equal to the duration of the
  // first chunk.
  TrackTime duration = 10;
  TrackTime smaller_duration = 8;
  AudioChunk c1;
  c1.SetNull(duration);
  AudioChunk c2;
  fillChunk<float, 2>(&c2, duration);

  AudioSegment s;
  s.AppendAndConsumeChunk(std::move(c1));
  s.AppendAndConsumeChunk(std::move(c2));
  s.FlushAfter(smaller_duration);
  EXPECT_EQ(s.GetDuration(), smaller_duration) << "Check new duration";

  TrackTime chunkByChunkDuration = 0;
  for (AudioSegment::ChunkIterator iter(s); !iter.IsEnded(); iter.Next()) {
    chunkByChunkDuration += iter->GetDuration();
  }
  EXPECT_EQ(s.GetDuration(), chunkByChunkDuration)
      << "Confirm duration chunk by chunk";
}

TEST(AudioSegment, MemoizedOutputChannelCount)
{
  AudioSegment s;
  EXPECT_EQ(s.MaxChannelCount(), 0U) << "0 channels on init";

  s.AppendNullData(1);
  EXPECT_EQ(s.MaxChannelCount(), 0U) << "Null data has 0 channels";

  s.Clear();
  EXPECT_EQ(s.MaxChannelCount(), 0U) << "Still 0 after clearing";

  AudioChunk c1;
  fillChunk<float, 1>(&c1, 1);
  s.AppendAndConsumeChunk(std::move(c1));
  EXPECT_EQ(s.MaxChannelCount(), 1U) << "A single chunk's channel count";

  AudioChunk c2;
  fillChunk<float, 2>(&c2, 1);
  s.AppendAndConsumeChunk(std::move(c2));
  EXPECT_EQ(s.MaxChannelCount(), 2U) << "The max of two chunks' channel count";

  s.ForgetUpTo(2);
  EXPECT_EQ(s.MaxChannelCount(), 2U) << "Memoized value with null chunks";

  s.Clear();
  EXPECT_EQ(s.MaxChannelCount(), 2U) << "Still memoized after clearing";

  AudioChunk c3;
  fillChunk<float, 1>(&c3, 1);
  s.AppendAndConsumeChunk(std::move(c3));
  EXPECT_EQ(s.MaxChannelCount(), 1U) << "Real chunk trumps memoized value";

  s.Clear();
  EXPECT_EQ(s.MaxChannelCount(), 1U) << "Memoized value was updated";
}

}  // namespace audio_segment
