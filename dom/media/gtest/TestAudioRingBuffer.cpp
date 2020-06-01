/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioRingBuffer.h"

#include "gtest/gtest.h"

TEST(TestAudioRingBuffer, BasicFloat)
{
  AudioRingBuffer ringBuffer(11 * sizeof(float));
  ringBuffer.SetSampleFormat(AUDIO_FORMAT_FLOAT32);

  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0);

  int rv = ringBuffer.WriteSilence(4);
  EXPECT_EQ(rv, 4);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 6);
  EXPECT_EQ(ringBuffer.AvailableRead(), 4);

  float in[4] = {.1, .2, .3, .4};
  rv = ringBuffer.Write(MakeSpan(in, 4));
  EXPECT_EQ(rv, 4);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 2);
  EXPECT_EQ(ringBuffer.AvailableRead(), 8);

  rv = ringBuffer.WriteSilence(4);
  EXPECT_EQ(rv, 2);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 0);
  EXPECT_EQ(ringBuffer.AvailableRead(), 10);

  rv = ringBuffer.Write(MakeSpan(in, 4));
  EXPECT_EQ(rv, 0);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 0);
  EXPECT_EQ(ringBuffer.AvailableRead(), 10);

  float out[4] = {};
  rv = ringBuffer.Read(MakeSpan(out, 4));
  EXPECT_EQ(rv, 4);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 4);
  EXPECT_EQ(ringBuffer.AvailableRead(), 6);
  for (float f : out) {
    EXPECT_FLOAT_EQ(f, 0.0);
  }

  rv = ringBuffer.Read(MakeSpan(out, 4));
  EXPECT_EQ(rv, 4);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 8);
  EXPECT_EQ(ringBuffer.AvailableRead(), 2);
  for (int i = 0; i < 4; ++i) {
    EXPECT_FLOAT_EQ(in[i], out[i]);
  }

  rv = ringBuffer.Read(MakeSpan(out, 4));
  EXPECT_EQ(rv, 2);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0);
  for (int i = 0; i < 2; ++i) {
    EXPECT_FLOAT_EQ(out[i], 0.0);
  }

  rv = ringBuffer.Clear();
  EXPECT_EQ(rv, 0);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0);
}

TEST(TestAudioRingBuffer, BasicShort)
{
  AudioRingBuffer ringBuffer(11 * sizeof(short));
  ringBuffer.SetSampleFormat(AUDIO_FORMAT_S16);

  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0);

  int rv = ringBuffer.WriteSilence(4);
  EXPECT_EQ(rv, 4);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 6);
  EXPECT_EQ(ringBuffer.AvailableRead(), 4);

  short in[4] = {1, 2, 3, 4};
  rv = ringBuffer.Write(MakeSpan(in, 4));
  EXPECT_EQ(rv, 4);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 2);
  EXPECT_EQ(ringBuffer.AvailableRead(), 8);

  rv = ringBuffer.WriteSilence(4);
  EXPECT_EQ(rv, 2);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 0);
  EXPECT_EQ(ringBuffer.AvailableRead(), 10);

  rv = ringBuffer.Write(MakeSpan(in, 4));
  EXPECT_EQ(rv, 0);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 0);
  EXPECT_EQ(ringBuffer.AvailableRead(), 10);

  short out[4] = {};
  rv = ringBuffer.Read(MakeSpan(out, 4));
  EXPECT_EQ(rv, 4);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 4);
  EXPECT_EQ(ringBuffer.AvailableRead(), 6);
  for (float f : out) {
    EXPECT_EQ(f, 0);
  }

  rv = ringBuffer.Read(MakeSpan(out, 4));
  EXPECT_EQ(rv, 4);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 8);
  EXPECT_EQ(ringBuffer.AvailableRead(), 2);
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(in[i], out[i]);
  }

  rv = ringBuffer.Read(MakeSpan(out, 4));
  EXPECT_EQ(rv, 2);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0);
  for (int i = 0; i < 2; ++i) {
    EXPECT_EQ(out[i], 0);
  }

  rv = ringBuffer.Clear();
  EXPECT_EQ(rv, 0);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0);
}

TEST(TestAudioRingBuffer, BasicFloat2)
{
  AudioRingBuffer ringBuffer(11 * sizeof(float));
  ringBuffer.SetSampleFormat(AUDIO_FORMAT_FLOAT32);

  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0);

  float in[4] = {.1, .2, .3, .4};
  int rv = ringBuffer.Write(MakeSpan(in, 4));
  EXPECT_EQ(rv, 4);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 6);
  EXPECT_EQ(ringBuffer.AvailableRead(), 4);

  rv = ringBuffer.Write(MakeSpan(in, 4));
  EXPECT_EQ(rv, 4);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 2);
  EXPECT_EQ(ringBuffer.AvailableRead(), 8);

  float out[4] = {};
  rv = ringBuffer.Read(MakeSpan(out, 4));
  EXPECT_EQ(rv, 4);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 6);
  EXPECT_EQ(ringBuffer.AvailableRead(), 4);
  for (int i = 0; i < 4; ++i) {
    EXPECT_FLOAT_EQ(in[i], out[i]);
  }

  // WriteIndex = 12
  rv = ringBuffer.Write(MakeSpan(in, 4));
  EXPECT_EQ(rv, 4);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 2);
  EXPECT_EQ(ringBuffer.AvailableRead(), 8);

  rv = ringBuffer.Read(MakeSpan(out, 4));
  EXPECT_EQ(rv, 4);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 6);
  EXPECT_EQ(ringBuffer.AvailableRead(), 4);
  for (int i = 0; i < 4; ++i) {
    EXPECT_FLOAT_EQ(in[i], out[i]);
  }

  rv = ringBuffer.Read(MakeSpan(out, 8));
  EXPECT_EQ(rv, 4);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0);
  for (int i = 0; i < 4; ++i) {
    EXPECT_FLOAT_EQ(in[i], out[i]);
  }

  rv = ringBuffer.Read(MakeSpan(out, 8));
  EXPECT_EQ(rv, 0);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0);
  for (int i = 0; i < 4; ++i) {
    EXPECT_FLOAT_EQ(in[i], out[i]);
  }

  // WriteIndex = 16
  rv = ringBuffer.Write(MakeSpan(in, 4));
  EXPECT_EQ(rv, 4);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 6);
  EXPECT_EQ(ringBuffer.AvailableRead(), 4);

  rv = ringBuffer.Write(MakeSpan(in, 4));
  EXPECT_EQ(rv, 4);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 2);
  EXPECT_EQ(ringBuffer.AvailableRead(), 8);

  rv = ringBuffer.Write(MakeSpan(in, 4));
  EXPECT_EQ(rv, 2);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 0);
  EXPECT_EQ(ringBuffer.AvailableRead(), 10);

  rv = ringBuffer.Write(MakeSpan(in, 4));
  EXPECT_EQ(rv, 0);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 0);
  EXPECT_EQ(ringBuffer.AvailableRead(), 10);
}

TEST(TestAudioRingBuffer, BasicShort2)
{
  AudioRingBuffer ringBuffer(11 * sizeof(int16_t));
  ringBuffer.SetSampleFormat(AUDIO_FORMAT_S16);

  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0);

  int16_t in[4] = {1, 2, 3, 4};
  int rv = ringBuffer.Write(MakeSpan(in, 4));
  EXPECT_EQ(rv, 4);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 6);
  EXPECT_EQ(ringBuffer.AvailableRead(), 4);

  rv = ringBuffer.Write(MakeSpan(in, 4));
  EXPECT_EQ(rv, 4);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 2);
  EXPECT_EQ(ringBuffer.AvailableRead(), 8);

  int16_t out[4] = {};
  rv = ringBuffer.Read(MakeSpan(out, 4));
  EXPECT_EQ(rv, 4);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 6);
  EXPECT_EQ(ringBuffer.AvailableRead(), 4);
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(in[i], out[i]);
  }

  // WriteIndex = 12
  rv = ringBuffer.Write(MakeSpan(in, 4));
  EXPECT_EQ(rv, 4);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 2);
  EXPECT_EQ(ringBuffer.AvailableRead(), 8);

  rv = ringBuffer.Read(MakeSpan(out, 4));
  EXPECT_EQ(rv, 4);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 6);
  EXPECT_EQ(ringBuffer.AvailableRead(), 4);
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(in[i], out[i]);
  }

  rv = ringBuffer.Read(MakeSpan(out, 8));
  EXPECT_EQ(rv, 4);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0);
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(in[i], out[i]);
  }

  rv = ringBuffer.Read(MakeSpan(out, 8));
  EXPECT_EQ(rv, 0);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0);
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(in[i], out[i]);
  }

  // WriteIndex = 16
  rv = ringBuffer.Write(MakeSpan(in, 4));
  EXPECT_EQ(rv, 4);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 6);
  EXPECT_EQ(ringBuffer.AvailableRead(), 4);

  rv = ringBuffer.Write(MakeSpan(in, 4));
  EXPECT_EQ(rv, 4);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 2);
  EXPECT_EQ(ringBuffer.AvailableRead(), 8);

  rv = ringBuffer.Write(MakeSpan(in, 4));
  EXPECT_EQ(rv, 2);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 0);
  EXPECT_EQ(ringBuffer.AvailableRead(), 10);

  rv = ringBuffer.Write(MakeSpan(in, 4));
  EXPECT_EQ(rv, 0);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 0);
  EXPECT_EQ(ringBuffer.AvailableRead(), 10);
}

TEST(TestAudioRingBuffer, NoCopyFloat)
{
  AudioRingBuffer ringBuffer(11 * sizeof(float));
  ringBuffer.SetSampleFormat(AUDIO_FORMAT_FLOAT32);

  float in[8] = {.0, .1, .2, .3, .4, .5, .6, .7};
  ringBuffer.Write(MakeSpan(in, 6));
  //  v ReadIndex
  // [x0: .0, x1: .1, x2: .2, x3: .3, x4: .4,
  //  x5: .5, x6: .0, x7: .0, x8: .0, x9: .0, x10: .0]

  float out[10] = {};
  float* out_ptr = out;

  int rv = ringBuffer.ReadNoCopy([&out_ptr](const Span<const float> aInBuffer) {
    PodMove(out_ptr, aInBuffer.data(), aInBuffer.Length());
    out_ptr += aInBuffer.Length();
    return aInBuffer.Length();
  });
  EXPECT_EQ(rv, 6);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0);
  for (int i = 0; i < rv; ++i) {
    EXPECT_FLOAT_EQ(out[i], in[i]);
  }

  ringBuffer.Write(MakeSpan(in, 8));
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
  EXPECT_EQ(rv, 8);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0);
  for (int i = 0; i < rv; ++i) {
    EXPECT_FLOAT_EQ(out[i], in[i]);
  }
}

TEST(TestAudioRingBuffer, NoCopyShort)
{
  AudioRingBuffer ringBuffer(11 * sizeof(short));
  ringBuffer.SetSampleFormat(AUDIO_FORMAT_S16);

  short in[8] = {0, 1, 2, 3, 4, 5, 6, 7};
  ringBuffer.Write(MakeSpan(in, 6));
  //  v ReadIndex
  // [x0: 0, x1: 1, x2: 2, x3: 3, x4: 4,
  //  x5: 5, x6: 0, x7: 0, x8: 0, x9: 0, x10: 0]

  short out[10] = {};
  short* out_ptr = out;

  int rv = ringBuffer.ReadNoCopy([&out_ptr](const Span<const short> aInBuffer) {
    PodMove(out_ptr, aInBuffer.data(), aInBuffer.Length());
    out_ptr += aInBuffer.Length();
    return aInBuffer.Length();
  });
  EXPECT_EQ(rv, 6);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0);
  for (int i = 0; i < rv; ++i) {
    EXPECT_EQ(out[i], in[i]);
  }

  ringBuffer.Write(MakeSpan(in, 8));
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
  EXPECT_EQ(rv, 8);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0);
  for (int i = 0; i < rv; ++i) {
    EXPECT_EQ(out[i], in[i]);
  }
}

TEST(TestAudioRingBuffer, NoCopyFloat2)
{
  AudioRingBuffer ringBuffer(11 * sizeof(float));
  ringBuffer.SetSampleFormat(AUDIO_FORMAT_FLOAT32);

  float in[8] = {.0, .1, .2, .3, .4, .5, .6, .7};
  ringBuffer.Write(MakeSpan(in, 6));
  //  v ReadIndex
  // [x0: .0, x1: .1, x2: .2, x3: .3, x4: .4,
  //  x5: .5, x6: .0, x7: .0, x8: .0, x9: .0, x10: .0]

  float out[10] = {};
  float* out_ptr = out;
  int total_frames = 3;

  int rv = ringBuffer.ReadNoCopy(
      [&out_ptr, &total_frames](const Span<const float>& aInBuffer) {
        int inFramesUsed = std::min<int>(total_frames, aInBuffer.Length());
        PodMove(out_ptr, aInBuffer.data(), inFramesUsed);
        out_ptr += inFramesUsed;
        total_frames -= inFramesUsed;
        return inFramesUsed;
      });
  //                          v ReadIndex
  // [x0: .0, x1: .1, x2: .2, x3: .3, x4: .4,
  //  x5: .5, x6: .0, x7: .0, x8: .0, x9: .0, x10: .0]
  EXPECT_EQ(rv, 3);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 7);
  EXPECT_EQ(ringBuffer.AvailableRead(), 3);
  for (int i = 0; i < rv; ++i) {
    EXPECT_FLOAT_EQ(out[i], in[i]);
  }

  total_frames = 3;
  rv = ringBuffer.ReadNoCopy(
      [&out_ptr, &total_frames](const Span<const float>& aInBuffer) {
        int inFramesUsed = std::min<int>(total_frames, aInBuffer.Length());
        PodMove(out_ptr, aInBuffer.data(), inFramesUsed);
        out_ptr += inFramesUsed;
        total_frames -= inFramesUsed;
        return inFramesUsed;
      });
  // [x0: .0, x1: .1, x2: .2, x3: .3, x4: .4,
  //  x5: .5, x6: .0, x7: .0, x8: .0, x9: .0, x10: .0]
  //          ^ ReadIndex
  EXPECT_EQ(rv, 3);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0);
  for (int i = 0; i < rv; ++i) {
    EXPECT_FLOAT_EQ(out[i + 3], in[i + 3]);
  }

  ringBuffer.Write(MakeSpan(in, 8));
  // Now the buffer contains:
  // [x0: .5, x1: .6, x2: .7, x3: .3, x4: .4,
  //  x5: .5, x6: .0, x7: .1, x8: .2, x9: .3, x10: .4
  //          ^ ReadIndex

  // reset the pointer before lambdas reuse
  out_ptr = out;
  total_frames = 3;
  rv = ringBuffer.ReadNoCopy(
      [&out_ptr, &total_frames](const Span<const float>& aInBuffer) {
        int inFramesUsed = std::min<int>(total_frames, aInBuffer.Length());
        PodMove(out_ptr, aInBuffer.data(), inFramesUsed);
        out_ptr += inFramesUsed;
        total_frames -= inFramesUsed;
        return inFramesUsed;
      });
  // Now the buffer contains:
  // [x0: .5, x1: .6, x2: .2, x3: .3, x4: .4,
  //  x5: .5, x6: .0, x7: .1, x8: .2, x9: .3, x10: .4
  //                                  ^ ReadIndex
  EXPECT_EQ(rv, 3);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 5);
  EXPECT_EQ(ringBuffer.AvailableRead(), 5);
  for (int i = 0; i < rv; ++i) {
    EXPECT_FLOAT_EQ(out[i], in[i]);
  }

  total_frames = 3;
  rv = ringBuffer.ReadNoCopy(
      [&out_ptr, &total_frames](const Span<const float>& aInBuffer) {
        int inFramesUsed = std::min<int>(total_frames, aInBuffer.Length());
        PodMove(out_ptr, aInBuffer.data(), inFramesUsed);
        out_ptr += inFramesUsed;
        total_frames -= inFramesUsed;
        return inFramesUsed;
      });
  // Now the buffer contains:
  //          v ReadIndex
  // [x0: .5, x1: .6, x2: .7, x3: .3, x4: .4,
  //  x5: .5, x6: .0, x7: .1, x8: .2, x9: .3, x10: .4
  EXPECT_EQ(rv, 3);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 8);
  EXPECT_EQ(ringBuffer.AvailableRead(), 2);
  for (int i = 0; i < rv; ++i) {
    EXPECT_FLOAT_EQ(out[i + 3], in[i + 3]);
  }

  total_frames = 3;
  rv = ringBuffer.ReadNoCopy(
      [&out_ptr, &total_frames](const Span<const float>& aInBuffer) {
        int inFramesUsed = std::min<int>(total_frames, aInBuffer.Length());
        PodMove(out_ptr, aInBuffer.data(), inFramesUsed);
        out_ptr += inFramesUsed;
        total_frames -= inFramesUsed;
        return inFramesUsed;
      });
  // Now the buffer contains:
  //                          v ReadIndex
  // [x0: .5, x1: .6, x2: .7, x3: .3, x4: .4,
  //  x5: .5, x6: .0, x7: .1, x8: .2, x9: .3, x10: .4
  EXPECT_EQ(rv, 2);
  EXPECT_EQ(total_frames, 1);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0);
  for (int i = 0; i < rv; ++i) {
    EXPECT_FLOAT_EQ(out[i + 6], in[i + 6]);
  }
}

TEST(TestAudioRingBuffer, NoCopyShort2)
{
  AudioRingBuffer ringBuffer(11 * sizeof(short));
  ringBuffer.SetSampleFormat(AUDIO_FORMAT_S16);

  short in[8] = {0, 1, 2, 3, 4, 5, 6, 7};
  ringBuffer.Write(MakeSpan(in, 6));
  //  v ReadIndex
  // [x0: 0, x1: 1, x2: 2, x3: 3, x4: 4,
  //  x5: 5, x6: 0, x7: 0, x8: 0, x9: 0, x10: 0]

  short out[10] = {};
  short* out_ptr = out;
  int total_frames = 3;

  int rv = ringBuffer.ReadNoCopy(
      [&out_ptr, &total_frames](const Span<const short>& aInBuffer) {
        int inFramesUsed = std::min<int>(total_frames, aInBuffer.Length());
        PodMove(out_ptr, aInBuffer.data(), inFramesUsed);
        out_ptr += inFramesUsed;
        total_frames -= inFramesUsed;
        return inFramesUsed;
      });
  //                       v ReadIndex
  // [x0: 0, x1: 1, x2: 2, x3: 3, x4: 4,
  //  x5: 5, x6: 0, x7: 0, x8: 0, x9: 0, x10: 0]
  EXPECT_EQ(rv, 3);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 7);
  EXPECT_EQ(ringBuffer.AvailableRead(), 3);
  for (int i = 0; i < rv; ++i) {
    EXPECT_EQ(out[i], in[i]);
  }

  total_frames = 3;
  rv = ringBuffer.ReadNoCopy(
      [&out_ptr, &total_frames](const Span<const short>& aInBuffer) {
        int inFramesUsed = std::min<int>(total_frames, aInBuffer.Length());
        PodMove(out_ptr, aInBuffer.data(), inFramesUsed);
        out_ptr += inFramesUsed;
        total_frames -= inFramesUsed;
        return inFramesUsed;
      });
  // [x0: 0, x1: 1, x2: 2, x3: 3, x4: 4,
  //  x5: 5, x6: 0, x7: 0, x8: 0, x9: 0, x10: .0]
  //         ^ ReadIndex
  EXPECT_EQ(rv, 3);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0);
  for (int i = 0; i < rv; ++i) {
    EXPECT_EQ(out[i + 3], in[i + 3]);
  }

  ringBuffer.Write(MakeSpan(in, 8));
  // Now the buffer contains:
  // [x0: 5, x1: 6, x2: 7, x3: 3, x4: 4,
  //  x5: 5, x6: 0, x7: 1, x8: 2, x9: 3, x10: 4
  //         ^ ReadIndex

  // reset the pointer before lambdas reuse
  out_ptr = out;
  total_frames = 3;
  rv = ringBuffer.ReadNoCopy(
      [&out_ptr, &total_frames](const Span<const short>& aInBuffer) {
        int inFramesUsed = std::min<int>(total_frames, aInBuffer.Length());
        PodMove(out_ptr, aInBuffer.data(), inFramesUsed);
        out_ptr += inFramesUsed;
        total_frames -= inFramesUsed;
        return inFramesUsed;
      });
  // Now the buffer contains:
  // [x0: 5, x1: 6, x2: 2, x3: 3, x4: 4,
  //  x5: 5, x6: 0, x7: 1, x8: 2, x9: 3, x10: 4
  //                              ^ ReadIndex
  EXPECT_EQ(rv, 3);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 5);
  EXPECT_EQ(ringBuffer.AvailableRead(), 5);
  for (int i = 0; i < rv; ++i) {
    EXPECT_EQ(out[i], in[i]);
  }

  total_frames = 3;
  rv = ringBuffer.ReadNoCopy(
      [&out_ptr, &total_frames](const Span<const short>& aInBuffer) {
        int inFramesUsed = std::min<int>(total_frames, aInBuffer.Length());
        PodMove(out_ptr, aInBuffer.data(), inFramesUsed);
        out_ptr += inFramesUsed;
        total_frames -= inFramesUsed;
        return inFramesUsed;
      });
  // Now the buffer contains:
  //         v ReadIndex
  // [x0: 5, x1: 6, x2: 7, x3: 3, x4: 4,
  //  x5: 5, x6: 0, x7: 1, x8: 2, x9: 3, x10: 4
  EXPECT_EQ(rv, 3);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 8);
  EXPECT_EQ(ringBuffer.AvailableRead(), 2);
  for (int i = 0; i < rv; ++i) {
    EXPECT_EQ(out[i + 3], in[i + 3]);
  }

  total_frames = 3;
  rv = ringBuffer.ReadNoCopy(
      [&out_ptr, &total_frames](const Span<const short>& aInBuffer) {
        int inFramesUsed = std::min<int>(total_frames, aInBuffer.Length());
        PodMove(out_ptr, aInBuffer.data(), inFramesUsed);
        out_ptr += inFramesUsed;
        total_frames -= inFramesUsed;
        return inFramesUsed;
      });
  // Now the buffer contains:
  //                       v ReadIndex
  // [x0: 5, x1: 6, x2: 7, x3: 3, x4: 4,
  //  x5: 5, x6: 0, x7: 1, x8: 2, x9: 3, x10: 4
  EXPECT_EQ(rv, 2);
  EXPECT_EQ(total_frames, 1);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0);
  for (int i = 0; i < rv; ++i) {
    EXPECT_EQ(out[i + 6], in[i + 6]);
  }
}

TEST(TestAudioRingBuffer, DiscardFloat)
{
  AudioRingBuffer ringBuffer(11 * sizeof(float));
  ringBuffer.SetSampleFormat(AUDIO_FORMAT_FLOAT32);

  float in[8] = {.0, .1, .2, .3, .4, .5, .6, .7};
  ringBuffer.Write(MakeSpan(in, 8));

  int rv = ringBuffer.Discard(3);
  EXPECT_EQ(rv, 3);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 5);
  EXPECT_EQ(ringBuffer.AvailableRead(), 5);

  float out[8] = {};
  rv = ringBuffer.Read(MakeSpan(out, 3));
  EXPECT_EQ(rv, 3);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 8);
  EXPECT_EQ(ringBuffer.AvailableRead(), 2);
  for (int i = 0; i < rv; ++i) {
    EXPECT_FLOAT_EQ(out[i], in[i + 3]);
  }

  rv = ringBuffer.Discard(3);
  EXPECT_EQ(rv, 2);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0);

  ringBuffer.WriteSilence(4);
  rv = ringBuffer.Discard(6);
  EXPECT_EQ(rv, 4);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0);
}

TEST(TestAudioRingBuffer, DiscardShort)
{
  AudioRingBuffer ringBuffer(11 * sizeof(short));
  ringBuffer.SetSampleFormat(AUDIO_FORMAT_S16);

  short in[8] = {0, 1, 2, 3, 4, 5, 6, 7};
  ringBuffer.Write(MakeSpan(in, 8));

  int rv = ringBuffer.Discard(3);
  EXPECT_EQ(rv, 3);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 5);
  EXPECT_EQ(ringBuffer.AvailableRead(), 5);

  short out[8] = {};
  rv = ringBuffer.Read(MakeSpan(out, 3));
  EXPECT_EQ(rv, 3);
  EXPECT_TRUE(!ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 8);
  EXPECT_EQ(ringBuffer.AvailableRead(), 2);
  for (int i = 0; i < rv; ++i) {
    EXPECT_EQ(out[i], in[i + 3]);
  }

  rv = ringBuffer.Discard(3);
  EXPECT_EQ(rv, 2);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0);

  ringBuffer.WriteSilence(4);
  rv = ringBuffer.Discard(6);
  EXPECT_EQ(rv, 4);
  EXPECT_TRUE(ringBuffer.IsEmpty());
  EXPECT_TRUE(!ringBuffer.IsFull());
  EXPECT_EQ(ringBuffer.AvailableWrite(), 10);
  EXPECT_EQ(ringBuffer.AvailableRead(), 0);
}

TEST(TestRingBuffer, WriteFromRing1)
{
  AudioRingBuffer ringBuffer1(11 * sizeof(float));
  ringBuffer1.SetSampleFormat(AUDIO_FORMAT_FLOAT32);
  AudioRingBuffer ringBuffer2(11 * sizeof(float));
  ringBuffer2.SetSampleFormat(AUDIO_FORMAT_FLOAT32);

  float in[4] = {.1, .2, .3, .4};
  int rv = ringBuffer1.Write(Span<const float>(in, 4));
  EXPECT_EQ(rv, 4);

  EXPECT_EQ(ringBuffer2.AvailableRead(), 0);
  rv = ringBuffer2.Write(ringBuffer1, 4);
  EXPECT_EQ(rv, 4);
  EXPECT_EQ(ringBuffer2.AvailableRead(), 4);

  float out[4] = {};
  rv = ringBuffer2.Read(Span<float>(out, 4));
  EXPECT_EQ(rv, 4);
  for (int i = 0; i < 4; ++i) {
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
  int rv = ringBuffer1.Write(Span<const float>(in, 4));
  EXPECT_EQ(rv, 4);
  rv = ringBuffer2.Write(ringBuffer1, 4);
  EXPECT_EQ(rv, 4);
  EXPECT_EQ(ringBuffer2.AvailableRead(), 4);

  float out[4] = {};
  rv = ringBuffer2.Read(Span<float>(out, 4));
  EXPECT_EQ(rv, 4);
  for (int i = 0; i < 4; ++i) {
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
  int rv = ringBuffer1.Write(Span<const float>(in, 4));
  EXPECT_EQ(rv, 4);
  rv = ringBuffer2.Write(ringBuffer1, 4);
  EXPECT_EQ(rv, 4);
  EXPECT_EQ(ringBuffer2.AvailableRead(), 4);

  float out[4] = {};
  rv = ringBuffer2.Read(Span<float>(out, 4));
  EXPECT_EQ(rv, 4);
  for (int i = 0; i < 4; ++i) {
    EXPECT_FLOAT_EQ(in[i], out[i]);
  }
}

TEST(TestAudioRingBuffer, WriteFromRingShort)
{
  AudioRingBuffer ringBuffer1(11 * sizeof(short));
  ringBuffer1.SetSampleFormat(AUDIO_FORMAT_S16);

  short in[8] = {0, 1, 2, 3, 4, 5, 6, 7};
  int rv = ringBuffer1.Write(MakeSpan(in, 8));
  EXPECT_EQ(rv, 8);

  AudioRingBuffer ringBuffer2(11 * sizeof(short));
  ringBuffer2.SetSampleFormat(AUDIO_FORMAT_S16);

  rv = ringBuffer2.Write(ringBuffer1, 4);
  EXPECT_EQ(rv, 4);
  EXPECT_EQ(ringBuffer2.AvailableRead(), 4);
  EXPECT_EQ(ringBuffer1.AvailableRead(), 8);

  short out[4] = {};
  rv = ringBuffer2.Read(MakeSpan(out, 4));
  for (int i = 0; i < rv; ++i) {
    EXPECT_EQ(out[i], in[i]);
  }

  rv = ringBuffer2.Write(ringBuffer1, 4);
  EXPECT_EQ(rv, 4);
  EXPECT_EQ(ringBuffer2.AvailableRead(), 4);
  EXPECT_EQ(ringBuffer1.AvailableRead(), 8);

  ringBuffer1.Discard(4);
  rv = ringBuffer2.Write(ringBuffer1, 4);
  EXPECT_EQ(rv, 4);
  EXPECT_EQ(ringBuffer2.AvailableRead(), 8);
  EXPECT_EQ(ringBuffer1.AvailableRead(), 4);

  short out2[8] = {};
  rv = ringBuffer2.Read(MakeSpan(out2, 8));
  for (int i = 0; i < rv; ++i) {
    EXPECT_EQ(out2[i], in[i]);
  }
}

TEST(TestAudioRingBuffer, WriteFromRingFloat)
{
  AudioRingBuffer ringBuffer1(11 * sizeof(float));
  ringBuffer1.SetSampleFormat(AUDIO_FORMAT_FLOAT32);

  float in[8] = {.0, .1, .2, .3, .4, .5, .6, .7};
  int rv = ringBuffer1.Write(MakeSpan(in, 8));
  EXPECT_EQ(rv, 8);

  AudioRingBuffer ringBuffer2(11 * sizeof(float));
  ringBuffer2.SetSampleFormat(AUDIO_FORMAT_FLOAT32);

  rv = ringBuffer2.Write(ringBuffer1, 4);
  EXPECT_EQ(rv, 4);
  EXPECT_EQ(ringBuffer2.AvailableRead(), 4);
  EXPECT_EQ(ringBuffer1.AvailableRead(), 8);

  float out[4] = {};
  rv = ringBuffer2.Read(MakeSpan(out, 4));
  for (int i = 0; i < rv; ++i) {
    EXPECT_FLOAT_EQ(out[i], in[i]);
  }

  rv = ringBuffer2.Write(ringBuffer1, 4);
  EXPECT_EQ(rv, 4);
  EXPECT_EQ(ringBuffer2.AvailableRead(), 4);
  EXPECT_EQ(ringBuffer1.AvailableRead(), 8);

  ringBuffer1.Discard(4);
  rv = ringBuffer2.Write(ringBuffer1, 4);
  EXPECT_EQ(rv, 4);
  EXPECT_EQ(ringBuffer2.AvailableRead(), 8);
  EXPECT_EQ(ringBuffer1.AvailableRead(), 4);

  float out2[8] = {};
  rv = ringBuffer2.Read(MakeSpan(out2, 8));
  for (int i = 0; i < rv; ++i) {
    EXPECT_FLOAT_EQ(out2[i], in[i]);
  }
}
