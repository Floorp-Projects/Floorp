/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioRingBuffer.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mozilla/PodOperations.h"

using namespace mozilla;
using testing::ElementsAre;

TEST(TestAudioRingBuffer, BasicFloat)
{
  AudioRingBuffer ringBuffer(11 * sizeof(float));
  ringBuffer.SetSampleFormat(AUDIO_FORMAT_FLOAT32);

  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0u);

  uint32_t rv = ringBuffer.WriteSilence(4);
  EXPECT_EQ(rv, 4u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 6u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 4u);

  float in[4] = {.1, .2, .3, .4};
  rv = ringBuffer.Write(Span(in, 4));
  EXPECT_EQ(rv, 4u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 2u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 8u);

  rv = ringBuffer.WriteSilence(4);
  EXPECT_EQ(rv, 2u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 0u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 10u);

  rv = ringBuffer.Write(Span(in, 4));
  EXPECT_EQ(rv, 0u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 0u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 10u);

  float out[4] = {};
  rv = ringBuffer.Read(Span(out, 4));
  EXPECT_EQ(rv, 4u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 4u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 6u);
  for (float f : out) {
    EXPECT_FLOAT_EQ(f, 0.0);
  }

  rv = ringBuffer.Read(Span(out, 4));
  EXPECT_EQ(rv, 4u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 8u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 2u);
  for (uint32_t i = 0; i < 4; ++i) {
    EXPECT_FLOAT_EQ(in[i], out[i]);
  }

  rv = ringBuffer.Read(Span(out, 4));
  EXPECT_EQ(rv, 2u);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0u);
  for (uint32_t i = 0; i < 2; ++i) {
    EXPECT_FLOAT_EQ(out[i], 0.0);
  }

  rv = ringBuffer.Clear();
  EXPECT_EQ(rv, 0u);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0u);
}

TEST(TestAudioRingBuffer, BasicShort)
{
  AudioRingBuffer ringBuffer(11 * sizeof(short));
  ringBuffer.SetSampleFormat(AUDIO_FORMAT_S16);

  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0u);

  uint32_t rv = ringBuffer.WriteSilence(4);
  EXPECT_EQ(rv, 4u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 6u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 4u);

  short in[4] = {1, 2, 3, 4};
  rv = ringBuffer.Write(Span(in, 4));
  EXPECT_EQ(rv, 4u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 2u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 8u);

  rv = ringBuffer.WriteSilence(4);
  EXPECT_EQ(rv, 2u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 0u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 10u);

  rv = ringBuffer.Write(Span(in, 4));
  EXPECT_EQ(rv, 0u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 0u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 10u);

  short out[4] = {};
  rv = ringBuffer.Read(Span(out, 4));
  EXPECT_EQ(rv, 4u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 4u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 6u);
  for (float f : out) {
    EXPECT_EQ(f, 0);
  }

  rv = ringBuffer.Read(Span(out, 4));
  EXPECT_EQ(rv, 4u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 8u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 2u);
  for (uint32_t i = 0; i < 4; ++i) {
    EXPECT_EQ(in[i], out[i]);
  }

  rv = ringBuffer.Read(Span(out, 4));
  EXPECT_EQ(rv, 2u);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0u);
  for (uint32_t i = 0; i < 2; ++i) {
    EXPECT_EQ(out[i], 0);
  }

  rv = ringBuffer.Clear();
  EXPECT_EQ(rv, 0u);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0u);
}

TEST(TestAudioRingBuffer, BasicFloat2)
{
  AudioRingBuffer ringBuffer(11 * sizeof(float));
  ringBuffer.SetSampleFormat(AUDIO_FORMAT_FLOAT32);

  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0u);

  float in[4] = {.1, .2, .3, .4};
  uint32_t rv = ringBuffer.Write(Span(in, 4));
  EXPECT_EQ(rv, 4u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 6u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 4u);

  rv = ringBuffer.Write(Span(in, 4));
  EXPECT_EQ(rv, 4u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 2u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 8u);

  float out[4] = {};
  rv = ringBuffer.Read(Span(out, 4));
  EXPECT_EQ(rv, 4u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 6u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 4u);
  for (uint32_t i = 0; i < 4; ++i) {
    EXPECT_FLOAT_EQ(in[i], out[i]);
  }

  // WriteIndex = 12
  rv = ringBuffer.Write(Span(in, 4));
  EXPECT_EQ(rv, 4u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 2u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 8u);

  rv = ringBuffer.Read(Span(out, 4));
  EXPECT_EQ(rv, 4u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 6u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 4u);
  for (uint32_t i = 0; i < 4; ++i) {
    EXPECT_FLOAT_EQ(in[i], out[i]);
  }

  rv = ringBuffer.Read(Span(out, 8));
  EXPECT_EQ(rv, 4u);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0u);
  for (uint32_t i = 0; i < 4; ++i) {
    EXPECT_FLOAT_EQ(in[i], out[i]);
  }

  rv = ringBuffer.Read(Span(out, 8));
  EXPECT_EQ(rv, 0u);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0u);
  for (uint32_t i = 0; i < 4; ++i) {
    EXPECT_FLOAT_EQ(in[i], out[i]);
  }

  // WriteIndex = 16
  rv = ringBuffer.Write(Span(in, 4));
  EXPECT_EQ(rv, 4u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 6u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 4u);

  rv = ringBuffer.Write(Span(in, 4));
  EXPECT_EQ(rv, 4u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 2u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 8u);

  rv = ringBuffer.Write(Span(in, 4));
  EXPECT_EQ(rv, 2u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 0u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 10u);

  rv = ringBuffer.Write(Span(in, 4));
  EXPECT_EQ(rv, 0u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 0u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 10u);
}

TEST(TestAudioRingBuffer, BasicShort2)
{
  AudioRingBuffer ringBuffer(11 * sizeof(int16_t));
  ringBuffer.SetSampleFormat(AUDIO_FORMAT_S16);

  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0u);

  int16_t in[4] = {1, 2, 3, 4};
  uint32_t rv = ringBuffer.Write(Span(in, 4));
  EXPECT_EQ(rv, 4u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 6u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 4u);

  rv = ringBuffer.Write(Span(in, 4));
  EXPECT_EQ(rv, 4u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 2u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 8u);

  int16_t out[4] = {};
  rv = ringBuffer.Read(Span(out, 4));
  EXPECT_EQ(rv, 4u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 6u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 4u);
  for (uint32_t i = 0; i < 4; ++i) {
    EXPECT_EQ(in[i], out[i]);
  }

  // WriteIndex = 12
  rv = ringBuffer.Write(Span(in, 4));
  EXPECT_EQ(rv, 4u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 2u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 8u);

  rv = ringBuffer.Read(Span(out, 4));
  EXPECT_EQ(rv, 4u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 6u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 4u);
  for (uint32_t i = 0; i < 4; ++i) {
    EXPECT_EQ(in[i], out[i]);
  }

  rv = ringBuffer.Read(Span(out, 8));
  EXPECT_EQ(rv, 4u);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0u);
  for (uint32_t i = 0; i < 4; ++i) {
    EXPECT_EQ(in[i], out[i]);
  }

  rv = ringBuffer.Read(Span(out, 8));
  EXPECT_EQ(rv, 0u);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0u);
  for (uint32_t i = 0; i < 4; ++i) {
    EXPECT_EQ(in[i], out[i]);
  }

  // WriteIndex = 16
  rv = ringBuffer.Write(Span(in, 4));
  EXPECT_EQ(rv, 4u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 6u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 4u);

  rv = ringBuffer.Write(Span(in, 4));
  EXPECT_EQ(rv, 4u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 2u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 8u);

  rv = ringBuffer.Write(Span(in, 4));
  EXPECT_EQ(rv, 2u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 0u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 10u);

  rv = ringBuffer.Write(Span(in, 4));
  EXPECT_EQ(rv, 0u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 0u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 10u);
}

TEST(TestAudioRingBuffer, NoCopyFloat)
{
  AudioRingBuffer ringBuffer(11 * sizeof(float));
  ringBuffer.SetSampleFormat(AUDIO_FORMAT_FLOAT32);

  float in[8] = {.0, .1, .2, .3, .4, .5, .6, .7};
  ringBuffer.Write(Span(in, 6));
  //  v ReadIndex
  // [x0: .0, x1: .1, x2: .2, x3: .3, x4: .4,
  //  x5: .5, x6: .0, x7: .0, x8: .0, x9: .0, x10: .0]

  float out[10] = {};
  float* out_ptr = out;

  uint32_t rv =
      ringBuffer.ReadNoCopy([&out_ptr](const Span<const float> aInBuffer) {
        PodMove(out_ptr, aInBuffer.data(), aInBuffer.Length());
        out_ptr += aInBuffer.Length();
        return aInBuffer.Length();
      });
  EXPECT_EQ(rv, 6u);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0u);
  for (uint32_t i = 0; i < rv; ++i) {
    EXPECT_FLOAT_EQ(out[i], in[i]);
  }

  ringBuffer.Write(Span(in, 8));
  // Now the buffer contains:
  // [x0: .5, x1: .6, x2: .2, x3: .3, x4: .4,
  //  x5: .5, x6: .0, x7: .1, x8: .2, x9: .3, x10: .4
  //          ^ ReadIndex
  out_ptr = out;  // reset the pointer before lambdas reuse
  rv = ringBuffer.ReadNoCopy([&out_ptr](const Span<const float> aInBuffer) {
    PodMove(out_ptr, aInBuffer.data(), aInBuffer.Length());
    out_ptr += aInBuffer.Length();
    return aInBuffer.Length();
  });
  EXPECT_EQ(rv, 8u);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0u);
  for (uint32_t i = 0; i < rv; ++i) {
    EXPECT_FLOAT_EQ(out[i], in[i]);
  }
}

TEST(TestAudioRingBuffer, NoCopyShort)
{
  AudioRingBuffer ringBuffer(11 * sizeof(short));
  ringBuffer.SetSampleFormat(AUDIO_FORMAT_S16);

  short in[8] = {0, 1, 2, 3, 4, 5, 6, 7};
  ringBuffer.Write(Span(in, 6));
  //  v ReadIndex
  // [x0: 0, x1: 1, x2: 2, x3: 3, x4: 4,
  //  x5: 5, x6: 0, x7: 0, x8: 0, x9: 0, x10: 0]

  short out[10] = {};
  short* out_ptr = out;

  uint32_t rv =
      ringBuffer.ReadNoCopy([&out_ptr](const Span<const short> aInBuffer) {
        PodMove(out_ptr, aInBuffer.data(), aInBuffer.Length());
        out_ptr += aInBuffer.Length();
        return aInBuffer.Length();
      });
  EXPECT_EQ(rv, 6u);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0u);
  for (uint32_t i = 0; i < rv; ++i) {
    EXPECT_EQ(out[i], in[i]);
  }

  ringBuffer.Write(Span(in, 8));
  // Now the buffer contains:
  // [x0: 5, x1: 6, x2: 2, x3: 3, x4: 4,
  //  x5: 5, x6: 0, x7: 1, x8: 2, x9: 3, x10: 4
  //          ^ ReadIndex
  out_ptr = out;  // reset the pointer before lambdas reuse
  rv = ringBuffer.ReadNoCopy([&out_ptr](const Span<const short> aInBuffer) {
    PodMove(out_ptr, aInBuffer.data(), aInBuffer.Length());
    out_ptr += aInBuffer.Length();
    return aInBuffer.Length();
  });
  EXPECT_EQ(rv, 8u);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0u);
  for (uint32_t i = 0; i < rv; ++i) {
    EXPECT_EQ(out[i], in[i]);
  }
}

TEST(TestAudioRingBuffer, NoCopyFloat2)
{
  AudioRingBuffer ringBuffer(11 * sizeof(float));
  ringBuffer.SetSampleFormat(AUDIO_FORMAT_FLOAT32);

  float in[8] = {.0, .1, .2, .3, .4, .5, .6, .7};
  ringBuffer.Write(Span(in, 6));
  //  v ReadIndex
  // [x0: .0, x1: .1, x2: .2, x3: .3, x4: .4,
  //  x5: .5, x6: .0, x7: .0, x8: .0, x9: .0, x10: .0]

  float out[10] = {};
  float* out_ptr = out;
  uint32_t total_frames = 3;

  uint32_t rv = ringBuffer.ReadNoCopy(
      [&out_ptr, &total_frames](const Span<const float>& aInBuffer) {
        uint32_t inFramesUsed =
            std::min<uint32_t>(total_frames, aInBuffer.Length());
        PodMove(out_ptr, aInBuffer.data(), inFramesUsed);
        out_ptr += inFramesUsed;
        total_frames -= inFramesUsed;
        return inFramesUsed;
      });
  //                          v ReadIndex
  // [x0: .0, x1: .1, x2: .2, x3: .3, x4: .4,
  //  x5: .5, x6: .0, x7: .0, x8: .0, x9: .0, x10: .0]
  EXPECT_EQ(rv, 3u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 7u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 3u);
  for (uint32_t i = 0; i < rv; ++i) {
    EXPECT_FLOAT_EQ(out[i], in[i]);
  }

  total_frames = 3;
  rv = ringBuffer.ReadNoCopy(
      [&out_ptr, &total_frames](const Span<const float>& aInBuffer) {
        uint32_t inFramesUsed =
            std::min<uint32_t>(total_frames, aInBuffer.Length());
        PodMove(out_ptr, aInBuffer.data(), inFramesUsed);
        out_ptr += inFramesUsed;
        total_frames -= inFramesUsed;
        return inFramesUsed;
      });
  // [x0: .0, x1: .1, x2: .2, x3: .3, x4: .4,
  //  x5: .5, x6: .0, x7: .0, x8: .0, x9: .0, x10: .0]
  //          ^ ReadIndex
  EXPECT_EQ(rv, 3u);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0u);
  for (uint32_t i = 0; i < rv; ++i) {
    EXPECT_FLOAT_EQ(out[i + 3], in[i + 3]);
  }

  ringBuffer.Write(Span(in, 8));
  // Now the buffer contains:
  // [x0: .5, x1: .6, x2: .7, x3: .3, x4: .4,
  //  x5: .5, x6: .0, x7: .1, x8: .2, x9: .3, x10: .4
  //          ^ ReadIndex

  // reset the pointer before lambdas reuse
  out_ptr = out;
  total_frames = 3;
  rv = ringBuffer.ReadNoCopy(
      [&out_ptr, &total_frames](const Span<const float>& aInBuffer) {
        uint32_t inFramesUsed =
            std::min<uint32_t>(total_frames, aInBuffer.Length());
        PodMove(out_ptr, aInBuffer.data(), inFramesUsed);
        out_ptr += inFramesUsed;
        total_frames -= inFramesUsed;
        return inFramesUsed;
      });
  // Now the buffer contains:
  // [x0: .5, x1: .6, x2: .2, x3: .3, x4: .4,
  //  x5: .5, x6: .0, x7: .1, x8: .2, x9: .3, x10: .4
  //                                  ^ ReadIndex
  EXPECT_EQ(rv, 3u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 5u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 5u);
  for (uint32_t i = 0; i < rv; ++i) {
    EXPECT_FLOAT_EQ(out[i], in[i]);
  }

  total_frames = 3;
  rv = ringBuffer.ReadNoCopy(
      [&out_ptr, &total_frames](const Span<const float>& aInBuffer) {
        uint32_t inFramesUsed =
            std::min<uint32_t>(total_frames, aInBuffer.Length());
        PodMove(out_ptr, aInBuffer.data(), inFramesUsed);
        out_ptr += inFramesUsed;
        total_frames -= inFramesUsed;
        return inFramesUsed;
      });
  // Now the buffer contains:
  //          v ReadIndex
  // [x0: .5, x1: .6, x2: .7, x3: .3, x4: .4,
  //  x5: .5, x6: .0, x7: .1, x8: .2, x9: .3, x10: .4
  EXPECT_EQ(rv, 3u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 8u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 2u);
  for (uint32_t i = 0; i < rv; ++i) {
    EXPECT_FLOAT_EQ(out[i + 3], in[i + 3]);
  }

  total_frames = 3;
  rv = ringBuffer.ReadNoCopy(
      [&out_ptr, &total_frames](const Span<const float>& aInBuffer) {
        uint32_t inFramesUsed =
            std::min<uint32_t>(total_frames, aInBuffer.Length());
        PodMove(out_ptr, aInBuffer.data(), inFramesUsed);
        out_ptr += inFramesUsed;
        total_frames -= inFramesUsed;
        return inFramesUsed;
      });
  // Now the buffer contains:
  //                          v ReadIndex
  // [x0: .5, x1: .6, x2: .7, x3: .3, x4: .4,
  //  x5: .5, x6: .0, x7: .1, x8: .2, x9: .3, x10: .4
  EXPECT_EQ(rv, 2u);
  EXPECT_EQ(total_frames, 1u);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0u);
  for (uint32_t i = 0; i < rv; ++i) {
    EXPECT_FLOAT_EQ(out[i + 6], in[i + 6]);
  }
}

TEST(TestAudioRingBuffer, NoCopyShort2)
{
  AudioRingBuffer ringBuffer(11 * sizeof(short));
  ringBuffer.SetSampleFormat(AUDIO_FORMAT_S16);

  short in[8] = {0, 1, 2, 3, 4, 5, 6, 7};
  ringBuffer.Write(Span(in, 6));
  //  v ReadIndex
  // [x0: 0, x1: 1, x2: 2, x3: 3, x4: 4,
  //  x5: 5, x6: 0, x7: 0, x8: 0, x9: 0, x10: 0]

  short out[10] = {};
  short* out_ptr = out;
  uint32_t total_frames = 3;

  uint32_t rv = ringBuffer.ReadNoCopy(
      [&out_ptr, &total_frames](const Span<const short>& aInBuffer) {
        uint32_t inFramesUsed =
            std::min<uint32_t>(total_frames, aInBuffer.Length());
        PodMove(out_ptr, aInBuffer.data(), inFramesUsed);
        out_ptr += inFramesUsed;
        total_frames -= inFramesUsed;
        return inFramesUsed;
      });
  //                       v ReadIndex
  // [x0: 0, x1: 1, x2: 2, x3: 3, x4: 4,
  //  x5: 5, x6: 0, x7: 0, x8: 0, x9: 0, x10: 0]
  EXPECT_EQ(rv, 3u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 7u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 3u);
  for (uint32_t i = 0; i < rv; ++i) {
    EXPECT_EQ(out[i], in[i]);
  }

  total_frames = 3;
  rv = ringBuffer.ReadNoCopy(
      [&out_ptr, &total_frames](const Span<const short>& aInBuffer) {
        uint32_t inFramesUsed =
            std::min<uint32_t>(total_frames, aInBuffer.Length());
        PodMove(out_ptr, aInBuffer.data(), inFramesUsed);
        out_ptr += inFramesUsed;
        total_frames -= inFramesUsed;
        return inFramesUsed;
      });
  // [x0: 0, x1: 1, x2: 2, x3: 3, x4: 4,
  //  x5: 5, x6: 0, x7: 0, x8: 0, x9: 0, x10: .0]
  //         ^ ReadIndex
  EXPECT_EQ(rv, 3u);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0u);
  for (uint32_t i = 0; i < rv; ++i) {
    EXPECT_EQ(out[i + 3], in[i + 3]);
  }

  ringBuffer.Write(Span(in, 8));
  // Now the buffer contains:
  // [x0: 5, x1: 6, x2: 7, x3: 3, x4: 4,
  //  x5: 5, x6: 0, x7: 1, x8: 2, x9: 3, x10: 4
  //         ^ ReadIndex

  // reset the pointer before lambdas reuse
  out_ptr = out;
  total_frames = 3;
  rv = ringBuffer.ReadNoCopy(
      [&out_ptr, &total_frames](const Span<const short>& aInBuffer) {
        uint32_t inFramesUsed =
            std::min<uint32_t>(total_frames, aInBuffer.Length());
        PodMove(out_ptr, aInBuffer.data(), inFramesUsed);
        out_ptr += inFramesUsed;
        total_frames -= inFramesUsed;
        return inFramesUsed;
      });
  // Now the buffer contains:
  // [x0: 5, x1: 6, x2: 2, x3: 3, x4: 4,
  //  x5: 5, x6: 0, x7: 1, x8: 2, x9: 3, x10: 4
  //                              ^ ReadIndex
  EXPECT_EQ(rv, 3u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 5u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 5u);
  for (uint32_t i = 0; i < rv; ++i) {
    EXPECT_EQ(out[i], in[i]);
  }

  total_frames = 3;
  rv = ringBuffer.ReadNoCopy(
      [&out_ptr, &total_frames](const Span<const short>& aInBuffer) {
        uint32_t inFramesUsed =
            std::min<uint32_t>(total_frames, aInBuffer.Length());
        PodMove(out_ptr, aInBuffer.data(), inFramesUsed);
        out_ptr += inFramesUsed;
        total_frames -= inFramesUsed;
        return inFramesUsed;
      });
  // Now the buffer contains:
  //         v ReadIndex
  // [x0: 5, x1: 6, x2: 7, x3: 3, x4: 4,
  //  x5: 5, x6: 0, x7: 1, x8: 2, x9: 3, x10: 4
  EXPECT_EQ(rv, 3u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 8u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 2u);
  for (uint32_t i = 0; i < rv; ++i) {
    EXPECT_EQ(out[i + 3], in[i + 3]);
  }

  total_frames = 3;
  rv = ringBuffer.ReadNoCopy(
      [&out_ptr, &total_frames](const Span<const short>& aInBuffer) {
        uint32_t inFramesUsed =
            std::min<uint32_t>(total_frames, aInBuffer.Length());
        PodMove(out_ptr, aInBuffer.data(), inFramesUsed);
        out_ptr += inFramesUsed;
        total_frames -= inFramesUsed;
        return inFramesUsed;
      });
  // Now the buffer contains:
  //                       v ReadIndex
  // [x0: 5, x1: 6, x2: 7, x3: 3, x4: 4,
  //  x5: 5, x6: 0, x7: 1, x8: 2, x9: 3, x10: 4
  EXPECT_EQ(rv, 2u);
  EXPECT_EQ(total_frames, 1u);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0u);
  for (uint32_t i = 0; i < rv; ++i) {
    EXPECT_EQ(out[i + 6], in[i + 6]);
  }
}

TEST(TestAudioRingBuffer, DiscardFloat)
{
  AudioRingBuffer ringBuffer(11 * sizeof(float));
  ringBuffer.SetSampleFormat(AUDIO_FORMAT_FLOAT32);

  float in[8] = {.0, .1, .2, .3, .4, .5, .6, .7};
  ringBuffer.Write(Span(in, 8));

  uint32_t rv = ringBuffer.Discard(3);
  EXPECT_EQ(rv, 3u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 5u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 5u);

  float out[8] = {};
  rv = ringBuffer.Read(Span(out, 3));
  EXPECT_EQ(rv, 3u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 8u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 2u);
  for (uint32_t i = 0; i < rv; ++i) {
    EXPECT_FLOAT_EQ(out[i], in[i + 3]);
  }

  rv = ringBuffer.Discard(3);
  EXPECT_EQ(rv, 2u);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0u);

  ringBuffer.WriteSilence(4);
  rv = ringBuffer.Discard(6);
  EXPECT_EQ(rv, 4u);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0u);
}

TEST(TestAudioRingBuffer, DiscardShort)
{
  AudioRingBuffer ringBuffer(11 * sizeof(short));
  ringBuffer.SetSampleFormat(AUDIO_FORMAT_S16);

  short in[8] = {0, 1, 2, 3, 4, 5, 6, 7};
  ringBuffer.Write(Span(in, 8));

  uint32_t rv = ringBuffer.Discard(3);
  EXPECT_EQ(rv, 3u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 5u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 5u);

  short out[8] = {};
  rv = ringBuffer.Read(Span(out, 3));
  EXPECT_EQ(rv, 3u);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 8u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 2u);
  for (uint32_t i = 0; i < rv; ++i) {
    EXPECT_EQ(out[i], in[i + 3]);
  }

  rv = ringBuffer.Discard(3);
  EXPECT_EQ(rv, 2u);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0u);

  ringBuffer.WriteSilence(4);
  rv = ringBuffer.Discard(6);
  EXPECT_EQ(rv, 4u);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10u);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0u);
}

TEST(TestRingBuffer, WriteFromRing1)
{
  AudioRingBuffer ringBuffer1(11 * sizeof(float));
  ringBuffer1.SetSampleFormat(AUDIO_FORMAT_FLOAT32);
  AudioRingBuffer ringBuffer2(11 * sizeof(float));
  ringBuffer2.SetSampleFormat(AUDIO_FORMAT_FLOAT32);

  float in[4] = {.1, .2, .3, .4};
  uint32_t rv = ringBuffer1.Write(Span<const float>(in, 4));
  EXPECT_EQ(rv, 4u);

  EXPECT_EQ(ringBuffer2.AvailableRead(), 0u);
  rv = ringBuffer2.Write(ringBuffer1, 4);
  EXPECT_EQ(rv, 4u);
  EXPECT_EQ(ringBuffer2.AvailableRead(), 4u);

  float out[4] = {};
  rv = ringBuffer2.Read(Span<float>(out, 4));
  EXPECT_EQ(rv, 4u);
  for (uint32_t i = 0; i < 4; ++i) {
    EXPECT_FLOAT_EQ(in[i], out[i]);
  }
}

TEST(TestRingBuffer, WriteFromRing2)
{
  AudioRingBuffer ringBuffer1(11 * sizeof(float));
  ringBuffer1.SetSampleFormat(AUDIO_FORMAT_FLOAT32);
  AudioRingBuffer ringBuffer2(11 * sizeof(float));
  ringBuffer2.SetSampleFormat(AUDIO_FORMAT_FLOAT32);

  // Advance the index
  ringBuffer2.WriteSilence(8);
  ringBuffer2.Clear();

  float in[4] = {.1, .2, .3, .4};
  uint32_t rv = ringBuffer1.Write(Span<const float>(in, 4));
  EXPECT_EQ(rv, 4u);
  rv = ringBuffer2.Write(ringBuffer1, 4);
  EXPECT_EQ(rv, 4u);
  EXPECT_EQ(ringBuffer2.AvailableRead(), 4u);

  float out[4] = {};
  rv = ringBuffer2.Read(Span<float>(out, 4));
  EXPECT_EQ(rv, 4u);
  for (uint32_t i = 0; i < 4; ++i) {
    EXPECT_FLOAT_EQ(in[i], out[i]);
  }
}

TEST(TestRingBuffer, WriteFromRing3)
{
  AudioRingBuffer ringBuffer1(11 * sizeof(float));
  ringBuffer1.SetSampleFormat(AUDIO_FORMAT_FLOAT32);
  AudioRingBuffer ringBuffer2(11 * sizeof(float));
  ringBuffer2.SetSampleFormat(AUDIO_FORMAT_FLOAT32);

  // Advance the index
  ringBuffer2.WriteSilence(8);
  ringBuffer2.Clear();
  ringBuffer2.WriteSilence(4);
  ringBuffer2.Clear();

  float in[4] = {.1, .2, .3, .4};
  uint32_t rv = ringBuffer1.Write(Span<const float>(in, 4));
  EXPECT_EQ(rv, 4u);
  rv = ringBuffer2.Write(ringBuffer1, 4);
  EXPECT_EQ(rv, 4u);
  EXPECT_EQ(ringBuffer2.AvailableRead(), 4u);

  float out[4] = {};
  rv = ringBuffer2.Read(Span<float>(out, 4));
  EXPECT_EQ(rv, 4u);
  for (uint32_t i = 0; i < 4; ++i) {
    EXPECT_FLOAT_EQ(in[i], out[i]);
  }
}

TEST(TestAudioRingBuffer, WriteFromRingShort)
{
  AudioRingBuffer ringBuffer1(11 * sizeof(short));
  ringBuffer1.SetSampleFormat(AUDIO_FORMAT_S16);

  short in[8] = {0, 1, 2, 3, 4, 5, 6, 7};
  uint32_t rv = ringBuffer1.Write(Span(in, 8));
  EXPECT_EQ(rv, 8u);

  AudioRingBuffer ringBuffer2(11 * sizeof(short));
  ringBuffer2.SetSampleFormat(AUDIO_FORMAT_S16);

  rv = ringBuffer2.Write(ringBuffer1, 4);
  EXPECT_EQ(rv, 4u);
  EXPECT_EQ(ringBuffer2.AvailableRead(), 4u);
  EXPECT_EQ(ringBuffer1.AvailableRead(), 8u);

  short out[4] = {};
  rv = ringBuffer2.Read(Span(out, 4));
  for (uint32_t i = 0; i < rv; ++i) {
    EXPECT_EQ(out[i], in[i]);
  }

  rv = ringBuffer2.Write(ringBuffer1, 4);
  EXPECT_EQ(rv, 4u);
  EXPECT_EQ(ringBuffer2.AvailableRead(), 4u);
  EXPECT_EQ(ringBuffer1.AvailableRead(), 8u);

  ringBuffer1.Discard(4);
  rv = ringBuffer2.Write(ringBuffer1, 4);
  EXPECT_EQ(rv, 4u);
  EXPECT_EQ(ringBuffer2.AvailableRead(), 8u);
  EXPECT_EQ(ringBuffer1.AvailableRead(), 4u);

  short out2[8] = {};
  rv = ringBuffer2.Read(Span(out2, 8));
  for (uint32_t i = 0; i < rv; ++i) {
    EXPECT_EQ(out2[i], in[i]);
  }
}

TEST(TestAudioRingBuffer, WriteFromRingFloat)
{
  AudioRingBuffer ringBuffer1(11 * sizeof(float));
  ringBuffer1.SetSampleFormat(AUDIO_FORMAT_FLOAT32);

  float in[8] = {.0, .1, .2, .3, .4, .5, .6, .7};
  uint32_t rv = ringBuffer1.Write(Span(in, 8));
  EXPECT_EQ(rv, 8u);

  AudioRingBuffer ringBuffer2(11 * sizeof(float));
  ringBuffer2.SetSampleFormat(AUDIO_FORMAT_FLOAT32);

  rv = ringBuffer2.Write(ringBuffer1, 4);
  EXPECT_EQ(rv, 4u);
  EXPECT_EQ(ringBuffer2.AvailableRead(), 4u);
  EXPECT_EQ(ringBuffer1.AvailableRead(), 8u);

  float out[4] = {};
  rv = ringBuffer2.Read(Span(out, 4));
  for (uint32_t i = 0; i < rv; ++i) {
    EXPECT_FLOAT_EQ(out[i], in[i]);
  }

  rv = ringBuffer2.Write(ringBuffer1, 4);
  EXPECT_EQ(rv, 4u);
  EXPECT_EQ(ringBuffer2.AvailableRead(), 4u);
  EXPECT_EQ(ringBuffer1.AvailableRead(), 8u);

  ringBuffer1.Discard(4);
  rv = ringBuffer2.Write(ringBuffer1, 4);
  EXPECT_EQ(rv, 4u);
  EXPECT_EQ(ringBuffer2.AvailableRead(), 8u);
  EXPECT_EQ(ringBuffer1.AvailableRead(), 4u);

  float out2[8] = {};
  rv = ringBuffer2.Read(Span(out2, 8));
  for (uint32_t i = 0; i < rv; ++i) {
    EXPECT_FLOAT_EQ(out2[i], in[i]);
  }
}

TEST(TestAudioRingBuffer, PrependSilenceWrapsFloat)
{
  AudioRingBuffer rb(9 * sizeof(float));
  rb.SetSampleFormat(AUDIO_FORMAT_FLOAT32);

  float in[6] = {.2, .3, .4, .5, .6, .7};
  uint32_t rv = rb.Write(Span(in, 6));
  EXPECT_EQ(rv, 6u);

  float out[8] = {};
  auto outSpan = Span(out, 8);
  rv = rb.Read(outSpan.Subspan(0, 1));
  EXPECT_EQ(rv, 1u);

  // PrependSilence will have to wrap around the start and put the silent
  // samples at indices 0 and 8 of the ring buffer.
  rv = rb.PrependSilence(2);
  EXPECT_EQ(rv, 2u);

  rv = rb.Read(outSpan.Subspan(1, 7));
  EXPECT_EQ(rv, 7u);

  EXPECT_THAT(out, ElementsAre(.2, 0, 0, .3, .4, .5, .6, .7));
}

TEST(TestAudioRingBuffer, PrependSilenceWrapsShort)
{
  AudioRingBuffer rb(9 * sizeof(short));
  rb.SetSampleFormat(AUDIO_FORMAT_S16);

  short in[6] = {2, 3, 4, 5, 6, 7};
  uint32_t rv = rb.Write(Span(in, 6));
  EXPECT_EQ(rv, 6u);

  short out[8] = {};
  auto outSpan = Span(out, 8);
  rv = rb.Read(outSpan.Subspan(0, 1));
  EXPECT_EQ(rv, 1u);

  // PrependSilence will have to wrap around the start and put the silent
  // samples at indices 0 and 8 of the ring buffer.
  rv = rb.PrependSilence(2);
  EXPECT_EQ(rv, 2u);

  rv = rb.Read(outSpan.Subspan(1, 7));
  EXPECT_EQ(rv, 7u);

  EXPECT_THAT(out, ElementsAre(2, 0, 0, 3, 4, 5, 6, 7));
}

TEST(TestAudioRingBuffer, PrependSilenceNoWrapFloat)
{
  AudioRingBuffer rb(9 * sizeof(float));
  rb.SetSampleFormat(AUDIO_FORMAT_FLOAT32);

  float in[6] = {.2, .3, .4, .5, .6, .7};
  uint32_t rv = rb.Write(Span(in, 6));
  EXPECT_EQ(rv, 6u);

  float out[8] = {};
  auto outSpan = Span(out, 8);
  rv = rb.Read(outSpan.To(4));
  EXPECT_EQ(rv, 4u);

  // PrependSilence will put the silent samples at indices 2 and 3 of the ring
  // buffer.
  rv = rb.PrependSilence(2);
  EXPECT_EQ(rv, 2u);

  rv = rb.Read(outSpan.Subspan(4, 4));
  EXPECT_EQ(rv, 4u);

  EXPECT_THAT(out, ElementsAre(.2, .3, .4, .5, 0, 0, .6, .7));
}

TEST(TestAudioRingBuffer, PrependSilenceNoWrapShort)
{
  AudioRingBuffer rb(9 * sizeof(short));
  rb.SetSampleFormat(AUDIO_FORMAT_S16);

  short in[6] = {2, 3, 4, 5, 6, 7};
  uint32_t rv = rb.Write(Span(in, 6));
  EXPECT_EQ(rv, 6u);

  short out[8] = {};
  auto outSpan = Span(out, 8);
  rv = rb.Read(outSpan.To(4));
  EXPECT_EQ(rv, 4u);

  // PrependSilence will put the silent samples at indices 2 and 3 of the ring
  // buffer.
  rv = rb.PrependSilence(2);
  EXPECT_EQ(rv, 2u);

  rv = rb.Read(outSpan.Subspan(4, 4));
  EXPECT_EQ(rv, 4u);

  EXPECT_THAT(out, ElementsAre(2, 3, 4, 5, 0, 0, 6, 7));
}

TEST(TestAudioRingBuffer, SetLengthBytesNoWrapFloat)
{
  AudioRingBuffer rb(6 * sizeof(float));
  rb.SetSampleFormat(AUDIO_FORMAT_FLOAT32);

  float in[5] = {.1, .2, .3, .4, .5};
  uint32_t rv = rb.Write(Span(in, 5));
  EXPECT_EQ(rv, 5u);
  EXPECT_EQ(rb.AvailableRead(), 5u);
  EXPECT_EQ(rb.AvailableWrite(), 0u);
  EXPECT_EQ(rb.Capacity(), 6u);

  EXPECT_TRUE(rb.SetLengthBytes(11 * sizeof(float)));
  float out[10] = {};
  rv = rb.Read(Span(out, 10));
  EXPECT_EQ(rv, 5u);
  EXPECT_EQ(rb.AvailableRead(), 0u);
  EXPECT_EQ(rb.AvailableWrite(), 10u);
  EXPECT_EQ(rb.Capacity(), 11u);
  EXPECT_THAT(out, ElementsAre(.1, .2, .3, .4, .5, 0, 0, 0, 0, 0));
}

TEST(TestAudioRingBuffer, SetLengthBytesNoWrapShort)
{
  AudioRingBuffer rb(6 * sizeof(short));
  rb.SetSampleFormat(AUDIO_FORMAT_S16);

  short in[5] = {1, 2, 3, 4, 5};
  uint32_t rv = rb.Write(Span(in, 5));
  EXPECT_EQ(rv, 5u);
  EXPECT_EQ(rb.AvailableRead(), 5u);
  EXPECT_EQ(rb.AvailableWrite(), 0u);
  EXPECT_EQ(rb.Capacity(), 6u);

  EXPECT_TRUE(rb.SetLengthBytes(11 * sizeof(short)));
  short out[10] = {};
  rv = rb.Read(Span(out, 10));
  EXPECT_EQ(rv, 5u);
  EXPECT_EQ(rb.AvailableRead(), 0u);
  EXPECT_EQ(rb.AvailableWrite(), 10u);
  EXPECT_EQ(rb.Capacity(), 11u);
  EXPECT_THAT(out, ElementsAre(1, 2, 3, 4, 5, 0, 0, 0, 0, 0));
}

TEST(TestAudioRingBuffer, SetLengthBytesWrap1PartFloat)
{
  AudioRingBuffer rb(6 * sizeof(float));
  rb.SetSampleFormat(AUDIO_FORMAT_FLOAT32);

  EXPECT_EQ(rb.WriteSilence(3), 3u);
  EXPECT_EQ(rb.AvailableRead(), 3u);
  EXPECT_EQ(rb.AvailableWrite(), 2u);
  EXPECT_EQ(rb.Capacity(), 6u);

  float outSilence[3] = {};
  EXPECT_EQ(rb.Read(Span(outSilence, 3)), 3u);
  EXPECT_EQ(rb.AvailableRead(), 0u);
  EXPECT_EQ(rb.AvailableWrite(), 5u);

  float in[5] = {.1, .2, .3, .4, .5};
  EXPECT_EQ(rb.Write(Span(in, 5)), 5u);
  EXPECT_EQ(rb.AvailableRead(), 5u);
  EXPECT_EQ(rb.AvailableWrite(), 0u);

  EXPECT_TRUE(rb.SetLengthBytes(11 * sizeof(float)));
  EXPECT_EQ(rb.AvailableRead(), 5u);
  EXPECT_EQ(rb.AvailableWrite(), 5u);

  float in2[2] = {.6, .7};
  EXPECT_EQ(rb.Write(Span(in2, 2)), 2u);
  EXPECT_EQ(rb.AvailableRead(), 7u);
  EXPECT_EQ(rb.AvailableWrite(), 3u);

  float out[10] = {};
  EXPECT_EQ(rb.Read(Span(out, 10)), 7u);
  EXPECT_EQ(rb.AvailableRead(), 0u);
  EXPECT_EQ(rb.AvailableWrite(), 10u);
  EXPECT_EQ(rb.Capacity(), 11u);
  EXPECT_THAT(out, ElementsAre(.1, .2, .3, .4, .5, .6, .7, 0, 0, 0));
}

TEST(TestAudioRingBuffer, SetLengthBytesWrap1PartShort)
{
  AudioRingBuffer rb(6 * sizeof(short));
  rb.SetSampleFormat(AUDIO_FORMAT_S16);

  EXPECT_EQ(rb.WriteSilence(3), 3u);
  EXPECT_EQ(rb.AvailableRead(), 3u);
  EXPECT_EQ(rb.AvailableWrite(), 2u);
  EXPECT_EQ(rb.Capacity(), 6u);

  short outSilence[3] = {};
  EXPECT_EQ(rb.Read(Span(outSilence, 3)), 3u);
  EXPECT_EQ(rb.AvailableRead(), 0u);
  EXPECT_EQ(rb.AvailableWrite(), 5u);

  short in[5] = {1, 2, 3, 4, 5};
  EXPECT_EQ(rb.Write(Span(in, 5)), 5u);
  EXPECT_EQ(rb.AvailableRead(), 5u);
  EXPECT_EQ(rb.AvailableWrite(), 0u);

  EXPECT_TRUE(rb.SetLengthBytes(11 * sizeof(short)));
  EXPECT_EQ(rb.AvailableRead(), 5u);
  EXPECT_EQ(rb.AvailableWrite(), 5u);

  short in2[2] = {6, 7};
  EXPECT_EQ(rb.Write(Span(in2, 2)), 2u);
  EXPECT_EQ(rb.AvailableRead(), 7u);
  EXPECT_EQ(rb.AvailableWrite(), 3u);

  short out[10] = {};
  EXPECT_EQ(rb.Read(Span(out, 10)), 7u);
  EXPECT_EQ(rb.AvailableRead(), 0u);
  EXPECT_EQ(rb.AvailableWrite(), 10u);
  EXPECT_EQ(rb.Capacity(), 11u);
  EXPECT_THAT(out, ElementsAre(1, 2, 3, 4, 5, 6, 7, 0, 0, 0));
}

TEST(TestAudioRingBuffer, SetLengthBytesWrap2PartsFloat)
{
  AudioRingBuffer rb(6 * sizeof(float));
  rb.SetSampleFormat(AUDIO_FORMAT_FLOAT32);

  EXPECT_EQ(rb.WriteSilence(3), 3u);
  EXPECT_EQ(rb.AvailableRead(), 3u);
  EXPECT_EQ(rb.AvailableWrite(), 2u);
  EXPECT_EQ(rb.Capacity(), 6u);

  float outSilence[3] = {};
  EXPECT_EQ(rb.Read(Span(outSilence, 3)), 3u);
  EXPECT_EQ(rb.AvailableRead(), 0u);
  EXPECT_EQ(rb.AvailableWrite(), 5u);

  float in[5] = {.1, .2, .3, .4, .5};
  EXPECT_EQ(rb.Write(Span(in, 5)), 5u);
  EXPECT_EQ(rb.AvailableRead(), 5u);
  EXPECT_EQ(rb.AvailableWrite(), 0u);

  EXPECT_TRUE(rb.SetLengthBytes(8 * sizeof(float)));
  EXPECT_EQ(rb.AvailableRead(), 5u);
  EXPECT_EQ(rb.AvailableWrite(), 2u);

  float in2[2] = {.6, .7};
  EXPECT_EQ(rb.Write(Span(in2, 2)), 2u);
  EXPECT_EQ(rb.AvailableRead(), 7u);
  EXPECT_EQ(rb.AvailableWrite(), 0u);

  float out[8] = {};
  EXPECT_EQ(rb.Read(Span(out, 8)), 7u);
  EXPECT_EQ(rb.AvailableRead(), 0u);
  EXPECT_EQ(rb.AvailableWrite(), 7u);
  EXPECT_EQ(rb.Capacity(), 8u);
  EXPECT_THAT(out, ElementsAre(.1, .2, .3, .4, .5, .6, .7, 0));
}

TEST(TestAudioRingBuffer, SetLengthBytesWrap2PartsShort)
{
  AudioRingBuffer rb(6 * sizeof(short));
  rb.SetSampleFormat(AUDIO_FORMAT_S16);

  EXPECT_EQ(rb.WriteSilence(3), 3u);
  EXPECT_EQ(rb.AvailableRead(), 3u);
  EXPECT_EQ(rb.AvailableWrite(), 2u);
  EXPECT_EQ(rb.Capacity(), 6u);

  short outSilence[3] = {};
  EXPECT_EQ(rb.Read(Span(outSilence, 3)), 3u);
  EXPECT_EQ(rb.AvailableRead(), 0u);
  EXPECT_EQ(rb.AvailableWrite(), 5u);

  short in[5] = {1, 2, 3, 4, 5};
  EXPECT_EQ(rb.Write(Span(in, 5)), 5u);
  EXPECT_EQ(rb.AvailableRead(), 5u);
  EXPECT_EQ(rb.AvailableWrite(), 0u);

  EXPECT_TRUE(rb.SetLengthBytes(8 * sizeof(short)));
  EXPECT_EQ(rb.AvailableRead(), 5u);
  EXPECT_EQ(rb.AvailableWrite(), 2u);

  short in2[2] = {6, 7};
  EXPECT_EQ(rb.Write(Span(in2, 2)), 2u);
  EXPECT_EQ(rb.AvailableRead(), 7u);
  EXPECT_EQ(rb.AvailableWrite(), 0u);

  short out[8] = {};
  EXPECT_EQ(rb.Read(Span(out, 8)), 7u);
  EXPECT_EQ(rb.AvailableRead(), 0u);
  EXPECT_EQ(rb.AvailableWrite(), 7u);
  EXPECT_EQ(rb.Capacity(), 8u);
  EXPECT_THAT(out, ElementsAre(1, 2, 3, 4, 5, 6, 7, 0));
}
