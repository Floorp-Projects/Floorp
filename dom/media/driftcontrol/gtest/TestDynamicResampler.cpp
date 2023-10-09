/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "DynamicResampler.h"

using namespace mozilla;

TEST(TestDynamicResampler, SameRates_Float1)
{
  const uint32_t in_frames = 100;
  const uint32_t out_frames = 100;
  uint32_t channels = 2;
  uint32_t in_rate = 44100;
  uint32_t out_rate = 44100;

  DynamicResampler dr(in_rate, out_rate);
  dr.SetSampleFormat(AUDIO_FORMAT_FLOAT32);
  EXPECT_EQ(dr.GetOutRate(), out_rate);
  EXPECT_EQ(dr.GetChannels(), channels);

  // float in_ch1[] = {.1, .2, .3, .4, .5, .6, .7, .8, .9, 1.0};
  // float in_ch2[] = {.1, .2, .3, .4, .5, .6, .7, .8, .9, 1.0};
  float in_ch1[in_frames] = {};
  float in_ch2[in_frames] = {};
  AutoTArray<const float*, 2> in_buffer;
  in_buffer.AppendElements(channels);
  in_buffer[0] = in_ch1;
  in_buffer[1] = in_ch2;

  float out_ch1[out_frames] = {};
  float out_ch2[out_frames] = {};

  // Warm up with zeros
  dr.AppendInput(in_buffer, in_frames);
  bool hasUnderrun = dr.Resample(out_ch1, out_frames, 0);
  EXPECT_FALSE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch2, out_frames, 1);
  EXPECT_FALSE(hasUnderrun);
  for (uint32_t i = 0; i < out_frames; ++i) {
    EXPECT_FLOAT_EQ(in_ch1[i], out_ch1[i]);
    EXPECT_FLOAT_EQ(in_ch2[i], out_ch2[i]);
  }

  // Continue with non zero
  for (uint32_t i = 0; i < in_frames; ++i) {
    in_ch1[i] = in_ch2[i] = 0.01f * i;
  }
  dr.AppendInput(in_buffer, in_frames);
  hasUnderrun = dr.Resample(out_ch1, out_frames, 0);
  EXPECT_FALSE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch2, out_frames, 1);
  EXPECT_FALSE(hasUnderrun);
  for (uint32_t i = 0; i < out_frames; ++i) {
    EXPECT_FLOAT_EQ(in_ch1[i], out_ch1[i]);
    EXPECT_FLOAT_EQ(in_ch2[i], out_ch2[i]);
  }

  // No more frames in the input buffer
  hasUnderrun = dr.Resample(out_ch1, out_frames, 0);
  EXPECT_TRUE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch2, out_frames, 1);
  EXPECT_TRUE(hasUnderrun);
}

TEST(TestDynamicResampler, SameRates_Short1)
{
  uint32_t in_frames = 2;
  uint32_t out_frames = 2;
  uint32_t channels = 2;
  uint32_t in_rate = 44100;
  uint32_t out_rate = 44100;

  DynamicResampler dr(in_rate, out_rate);
  dr.SetSampleFormat(AUDIO_FORMAT_S16);
  EXPECT_EQ(dr.GetOutRate(), out_rate);
  EXPECT_EQ(dr.GetChannels(), channels);

  short in_ch1[] = {1, 2, 3};
  short in_ch2[] = {4, 5, 6};
  AutoTArray<const short*, 2> in_buffer;
  in_buffer.AppendElements(channels);
  in_buffer[0] = in_ch1;
  in_buffer[1] = in_ch2;

  short out_ch1[3] = {};
  short out_ch2[3] = {};

  dr.AppendInput(in_buffer, in_frames);
  bool hasUnderrun = dr.Resample(out_ch1, out_frames, 0);
  EXPECT_FALSE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch2, out_frames, 1);
  EXPECT_FALSE(hasUnderrun);
  for (uint32_t i = 0; i < out_frames; ++i) {
    EXPECT_EQ(in_ch1[i], out_ch1[i]);
    EXPECT_EQ(in_ch2[i], out_ch2[i]);
  }

  // No more frames in the input buffer
  hasUnderrun = dr.Resample(out_ch1, out_frames, 0);
  EXPECT_TRUE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch2, out_frames, 1);
  EXPECT_TRUE(hasUnderrun);
}

TEST(TestDynamicResampler, SameRates_Float2)
{
  uint32_t in_frames = 3;
  uint32_t out_frames = 2;
  uint32_t channels = 2;
  uint32_t in_rate = 44100;
  uint32_t out_rate = 44100;

  DynamicResampler dr(in_rate, out_rate);
  dr.SetSampleFormat(AUDIO_FORMAT_FLOAT32);

  float in_ch1[] = {0.1, 0.2, 0.3};
  float in_ch2[] = {0.4, 0.5, 0.6};
  AutoTArray<const float*, 2> in_buffer;
  in_buffer.AppendElements(channels);
  in_buffer[0] = in_ch1;
  in_buffer[1] = in_ch2;

  float out_ch1[3] = {};
  float out_ch2[3] = {};

  dr.AppendInput(in_buffer, in_frames);
  bool hasUnderrun = dr.Resample(out_ch1, out_frames, 0);
  EXPECT_FALSE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch2, out_frames, 1);
  EXPECT_FALSE(hasUnderrun);
  for (uint32_t i = 0; i < out_frames; ++i) {
    EXPECT_FLOAT_EQ(in_ch1[i], out_ch1[i]);
    EXPECT_FLOAT_EQ(in_ch2[i], out_ch2[i]);
  }

  out_frames = 1;
  hasUnderrun = dr.Resample(out_ch1, out_frames, 0);
  EXPECT_FALSE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch2, out_frames, 1);
  EXPECT_FALSE(hasUnderrun);
  for (uint32_t i = 0; i < out_frames; ++i) {
    EXPECT_FLOAT_EQ(in_ch1[i + 2], out_ch1[i]);
    EXPECT_FLOAT_EQ(in_ch2[i + 2], out_ch2[i]);
  }

  // No more frames, the input buffer has drained
  hasUnderrun = dr.Resample(out_ch1, out_frames, 0);
  EXPECT_TRUE(hasUnderrun);
  dr.Resample(out_ch2, out_frames, 1);
  EXPECT_TRUE(hasUnderrun);
}

TEST(TestDynamicResampler, SameRates_Short2)
{
  uint32_t in_frames = 3;
  uint32_t out_frames = 2;
  uint32_t channels = 2;
  uint32_t in_rate = 44100;
  uint32_t out_rate = 44100;

  DynamicResampler dr(in_rate, out_rate);
  dr.SetSampleFormat(AUDIO_FORMAT_S16);

  short in_ch1[] = {1, 2, 3};
  short in_ch2[] = {4, 5, 6};
  AutoTArray<const short*, 2> in_buffer;
  in_buffer.AppendElements(channels);
  in_buffer[0] = in_ch1;
  in_buffer[1] = in_ch2;

  short out_ch1[3] = {};
  short out_ch2[3] = {};

  dr.AppendInput(in_buffer, in_frames);
  bool hasUnderrun = dr.Resample(out_ch1, out_frames, 0);
  EXPECT_FALSE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch2, out_frames, 1);
  EXPECT_FALSE(hasUnderrun);
  for (uint32_t i = 0; i < out_frames; ++i) {
    EXPECT_EQ(in_ch1[i], out_ch1[i]);
    EXPECT_EQ(in_ch2[i], out_ch2[i]);
  }

  out_frames = 1;
  hasUnderrun = dr.Resample(out_ch1, out_frames, 0);
  EXPECT_FALSE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch2, out_frames, 1);
  EXPECT_FALSE(hasUnderrun);
  for (uint32_t i = 0; i < out_frames; ++i) {
    EXPECT_EQ(in_ch1[i + 2], out_ch1[i]);
    EXPECT_EQ(in_ch2[i + 2], out_ch2[i]);
  }

  // No more frames, the input buffer has drained
  hasUnderrun = dr.Resample(out_ch1, out_frames, 0);
  EXPECT_TRUE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch2, out_frames, 1);
  EXPECT_TRUE(hasUnderrun);
}

TEST(TestDynamicResampler, SameRates_Float3)
{
  uint32_t in_frames = 2;
  uint32_t out_frames = 3;
  uint32_t channels = 2;
  uint32_t in_rate = 44100;
  uint32_t out_rate = 44100;

  DynamicResampler dr(in_rate, out_rate);
  dr.SetSampleFormat(AUDIO_FORMAT_FLOAT32);

  float in_ch1[] = {0.1, 0.2, 0.3};
  float in_ch2[] = {0.4, 0.5, 0.6};
  AutoTArray<const float*, 2> in_buffer;
  in_buffer.AppendElements(channels);
  in_buffer[0] = in_ch1;
  in_buffer[1] = in_ch2;

  float out_ch1[3] = {};
  float out_ch2[3] = {};

  // Not enough frames in the input buffer
  dr.AppendInput(in_buffer, in_frames);
  bool hasUnderrun = dr.Resample(out_ch1, out_frames, 0);
  EXPECT_TRUE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch2, out_frames, 1);
  EXPECT_TRUE(hasUnderrun);

  // Add one frame
  in_buffer[0] = in_ch1 + 2;
  in_buffer[1] = in_ch2 + 2;
  dr.AppendInput(in_buffer, 1);
  out_frames = 1;
  hasUnderrun = dr.Resample(out_ch1 + 2, out_frames, 0);
  EXPECT_FALSE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch2 + 2, out_frames, 1);
  EXPECT_FALSE(hasUnderrun);
  for (uint32_t i = 0; i < 3; ++i) {
    EXPECT_FLOAT_EQ(in_ch1[i], out_ch1[i]);
    EXPECT_FLOAT_EQ(in_ch2[i], out_ch2[i]);
  }
}

TEST(TestDynamicResampler, SameRates_Short3)
{
  uint32_t in_frames = 2;
  uint32_t out_frames = 3;
  uint32_t channels = 2;
  uint32_t in_rate = 44100;
  uint32_t out_rate = 44100;

  DynamicResampler dr(in_rate, out_rate);
  dr.SetSampleFormat(AUDIO_FORMAT_S16);

  short in_ch1[] = {1, 2, 3};
  short in_ch2[] = {4, 5, 6};
  AutoTArray<const short*, 2> in_buffer;
  in_buffer.AppendElements(channels);
  in_buffer[0] = in_ch1;
  in_buffer[1] = in_ch2;

  short out_ch1[3] = {};
  short out_ch2[3] = {};

  // Not enough frames in the input buffer
  dr.AppendInput(in_buffer, in_frames);
  bool hasUnderrun = dr.Resample(out_ch1, out_frames, 0);
  EXPECT_TRUE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch2, out_frames, 1);
  EXPECT_TRUE(hasUnderrun);

  // Add one frame
  in_buffer[0] = in_ch1 + 2;
  in_buffer[1] = in_ch2 + 2;
  dr.AppendInput(in_buffer, 1);
  out_frames = 1;
  hasUnderrun = dr.Resample(out_ch1 + 2, out_frames, 0);
  EXPECT_FALSE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch2 + 2, out_frames, 1);
  EXPECT_FALSE(hasUnderrun);
  for (uint32_t i = 0; i < 3; ++i) {
    EXPECT_EQ(in_ch1[i], out_ch1[i]);
    EXPECT_EQ(in_ch2[i], out_ch2[i]);
  }
}

TEST(TestDynamicResampler, UpdateOutRate_Float)
{
  uint32_t in_frames = 10;
  uint32_t out_frames = 40;
  uint32_t channels = 2;
  uint32_t in_rate = 24000;
  uint32_t out_rate = 48000;

  uint32_t pre_buffer = 20;

  DynamicResampler dr(in_rate, out_rate, media::TimeUnit(pre_buffer, in_rate));
  dr.SetSampleFormat(AUDIO_FORMAT_FLOAT32);
  EXPECT_EQ(dr.GetOutRate(), out_rate);
  EXPECT_EQ(dr.GetChannels(), channels);

  float in_ch1[10] = {};
  float in_ch2[10] = {};
  for (uint32_t i = 0; i < in_frames; ++i) {
    in_ch1[i] = in_ch2[i] = 0.01f * i;
  }
  AutoTArray<const float*, 2> in_buffer;
  in_buffer.AppendElements(channels);
  in_buffer[0] = in_ch1;
  in_buffer[1] = in_ch2;

  float out_ch1[40] = {};
  float out_ch2[40] = {};

  dr.AppendInputSilence(pre_buffer - in_frames);
  dr.AppendInput(in_buffer, in_frames);
  out_frames = 20u;
  bool hasUnderrun = dr.Resample(out_ch1, out_frames, 0);
  EXPECT_FALSE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch2, out_frames, 1);
  EXPECT_FALSE(hasUnderrun);
  for (uint32_t i = 0; i < out_frames; ++i) {
    // Half the input pre-buffer (10) is silence, and half the output (20).
    EXPECT_FLOAT_EQ(out_ch1[i], 0.0);
    EXPECT_FLOAT_EQ(out_ch2[i], 0.0);
  }

  // Update out rate
  out_rate = 44100;
  dr.UpdateResampler(out_rate, channels);
  EXPECT_EQ(dr.GetOutRate(), out_rate);
  EXPECT_EQ(dr.GetChannels(), channels);
  out_frames = in_frames * out_rate / in_rate;
  EXPECT_EQ(out_frames, 18u);
  // Even if we provide no input if we have enough buffered input, we can create
  // output
  hasUnderrun = dr.Resample(out_ch1, out_frames, 0);
  EXPECT_FALSE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch2, out_frames, 1);
  EXPECT_FALSE(hasUnderrun);
}

TEST(TestDynamicResampler, UpdateOutRate_Short)
{
  uint32_t in_frames = 10;
  uint32_t out_frames = 40;
  uint32_t channels = 2;
  uint32_t in_rate = 24000;
  uint32_t out_rate = 48000;

  uint32_t pre_buffer = 20;

  DynamicResampler dr(in_rate, out_rate, media::TimeUnit(pre_buffer, in_rate));
  dr.SetSampleFormat(AUDIO_FORMAT_S16);
  EXPECT_EQ(dr.GetOutRate(), out_rate);
  EXPECT_EQ(dr.GetChannels(), channels);

  short in_ch1[10] = {};
  short in_ch2[10] = {};
  for (uint32_t i = 0; i < in_frames; ++i) {
    in_ch1[i] = in_ch2[i] = i;
  }
  AutoTArray<const short*, 2> in_buffer;
  in_buffer.AppendElements(channels);
  in_buffer[0] = in_ch1;
  in_buffer[1] = in_ch2;

  short out_ch1[40] = {};
  short out_ch2[40] = {};

  dr.AppendInputSilence(pre_buffer - in_frames);
  dr.AppendInput(in_buffer, in_frames);
  out_frames = 20u;
  bool hasUnderrun = dr.Resample(out_ch1, out_frames, 0);
  EXPECT_FALSE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch2, out_frames, 1);
  EXPECT_FALSE(hasUnderrun);
  for (uint32_t i = 0; i < out_frames; ++i) {
    // Half the input pre-buffer (10) is silence, and half the output (20).
    EXPECT_EQ(out_ch1[i], 0.0);
    EXPECT_EQ(out_ch2[i], 0.0);
  }

  // Update out rate
  out_rate = 44100;
  dr.UpdateResampler(out_rate, channels);
  EXPECT_EQ(dr.GetOutRate(), out_rate);
  EXPECT_EQ(dr.GetChannels(), channels);
  out_frames = in_frames * out_rate / in_rate;
  EXPECT_EQ(out_frames, 18u);
  // Even if we provide no input if we have enough buffered input, we can create
  // output
  hasUnderrun = dr.Resample(out_ch1, out_frames, 0);
  EXPECT_FALSE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch2, out_frames, 1);
  EXPECT_FALSE(hasUnderrun);
}

TEST(TestDynamicResampler, BigRangeOutRates_Float)
{
  uint32_t in_frames = 10;
  uint32_t out_frames = 10;
  uint32_t channels = 2;
  uint32_t in_rate = 44100;
  uint32_t out_rate = 44100;
  uint32_t pre_buffer = 20;

  DynamicResampler dr(in_rate, out_rate, media::TimeUnit(pre_buffer, in_rate));
  dr.SetSampleFormat(AUDIO_FORMAT_FLOAT32);

  const uint32_t in_capacity = 40;
  float in_ch1[in_capacity] = {};
  float in_ch2[in_capacity] = {};
  for (uint32_t i = 0; i < in_capacity; ++i) {
    in_ch1[i] = in_ch2[i] = 0.01f * i;
  }
  AutoTArray<const float*, 2> in_buffer;
  in_buffer.AppendElements(channels);
  in_buffer[0] = in_ch1;
  in_buffer[1] = in_ch2;

  const uint32_t out_capacity = 1000;
  float out_ch1[out_capacity] = {};
  float out_ch2[out_capacity] = {};

  for (uint32_t rate = 10000; rate < 90000; ++rate) {
    out_rate = rate;
    dr.UpdateResampler(out_rate, channels);
    EXPECT_EQ(dr.GetOutRate(), out_rate);
    EXPECT_EQ(dr.GetChannels(), channels);
    in_frames = 20;  // more than we need
    out_frames = in_frames * out_rate / in_rate;
    for (uint32_t y = 0; y < 2; ++y) {
      dr.AppendInput(in_buffer, in_frames);
      bool hasUnderrun = dr.Resample(out_ch1, out_frames, 0);
      EXPECT_FALSE(hasUnderrun);
      hasUnderrun = dr.Resample(out_ch2, out_frames, 1);
      EXPECT_FALSE(hasUnderrun);
    }
  }
}

TEST(TestDynamicResampler, BigRangeOutRates_Short)
{
  uint32_t in_frames = 10;
  uint32_t out_frames = 10;
  uint32_t channels = 2;
  uint32_t in_rate = 44100;
  uint32_t out_rate = 44100;
  uint32_t pre_buffer = 20;

  DynamicResampler dr(in_rate, out_rate, media::TimeUnit(pre_buffer, in_rate));
  dr.SetSampleFormat(AUDIO_FORMAT_S16);

  const uint32_t in_capacity = 40;
  short in_ch1[in_capacity] = {};
  short in_ch2[in_capacity] = {};
  for (uint32_t i = 0; i < in_capacity; ++i) {
    in_ch1[i] = in_ch2[i] = i;
  }
  AutoTArray<const short*, 2> in_buffer;
  in_buffer.AppendElements(channels);
  in_buffer[0] = in_ch1;
  in_buffer[1] = in_ch2;

  const uint32_t out_capacity = 1000;
  short out_ch1[out_capacity] = {};
  short out_ch2[out_capacity] = {};

  for (uint32_t rate = 10000; rate < 90000; ++rate) {
    out_rate = rate;
    dr.UpdateResampler(out_rate, channels);
    in_frames = 20;  // more than we need
    out_frames = in_frames * out_rate / in_rate;
    for (uint32_t y = 0; y < 2; ++y) {
      dr.AppendInput(in_buffer, in_frames);
      bool hasUnderrun = dr.Resample(out_ch1, out_frames, 0);
      EXPECT_FALSE(hasUnderrun);
      hasUnderrun = dr.Resample(out_ch2, out_frames, 1);
      EXPECT_FALSE(hasUnderrun);
    }
  }
}

TEST(TestDynamicResampler, UpdateChannels_Float)
{
  uint32_t in_frames = 10;
  uint32_t out_frames = 10;
  uint32_t channels = 2;
  uint32_t in_rate = 44100;
  uint32_t out_rate = 48000;

  DynamicResampler dr(in_rate, out_rate);
  dr.SetSampleFormat(AUDIO_FORMAT_FLOAT32);

  float in_ch1[10] = {};
  float in_ch2[10] = {};
  for (uint32_t i = 0; i < in_frames; ++i) {
    in_ch1[i] = in_ch2[i] = 0.01f * i;
  }
  AutoTArray<const float*, 2> in_buffer;
  in_buffer.AppendElements(channels);
  in_buffer[0] = in_ch1;
  in_buffer[1] = in_ch2;

  float out_ch1[10] = {};
  float out_ch2[10] = {};

  dr.AppendInput(in_buffer, in_frames);
  bool hasUnderrun = dr.Resample(out_ch1, out_frames, 0);
  EXPECT_FALSE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch2, out_frames, 1);
  EXPECT_FALSE(hasUnderrun);

  // Add 3rd channel
  dr.UpdateResampler(out_rate, 3);
  EXPECT_EQ(dr.GetOutRate(), out_rate);
  EXPECT_EQ(dr.GetChannels(), 3u);

  float in_ch3[10] = {};
  for (uint32_t i = 0; i < in_frames; ++i) {
    in_ch3[i] = 0.01f * i;
  }
  in_buffer.AppendElement();
  in_buffer[2] = in_ch3;
  float out_ch3[10] = {};

  dr.AppendInput(in_buffer, in_frames);

  hasUnderrun = dr.Resample(out_ch1, out_frames, 0);
  EXPECT_FALSE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch2, out_frames, 1);
  EXPECT_FALSE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch3, out_frames, 2);
  EXPECT_FALSE(hasUnderrun);

  float in_ch4[10] = {};
  for (uint32_t i = 0; i < in_frames; ++i) {
    in_ch3[i] = 0.01f * i;
  }
  in_buffer.AppendElement();
  in_buffer[3] = in_ch4;
  float out_ch4[10] = {};

  dr.UpdateResampler(out_rate, 4);
  EXPECT_EQ(dr.GetOutRate(), out_rate);
  EXPECT_EQ(dr.GetChannels(), 4u);
  dr.AppendInput(in_buffer, in_frames);

  hasUnderrun = dr.Resample(out_ch1, out_frames, 0);
  EXPECT_FALSE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch2, out_frames, 1);
  EXPECT_FALSE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch3, out_frames, 2);
  EXPECT_FALSE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch4, out_frames, 3);
  EXPECT_FALSE(hasUnderrun);
}

TEST(TestDynamicResampler, UpdateChannels_Short)
{
  uint32_t in_frames = 10;
  uint32_t out_frames = 10;
  uint32_t channels = 2;
  uint32_t in_rate = 44100;
  uint32_t out_rate = 48000;

  DynamicResampler dr(in_rate, out_rate);
  dr.SetSampleFormat(AUDIO_FORMAT_S16);

  short in_ch1[10] = {};
  short in_ch2[10] = {};
  for (uint32_t i = 0; i < in_frames; ++i) {
    in_ch1[i] = in_ch2[i] = i;
  }
  AutoTArray<const short*, 2> in_buffer;
  in_buffer.AppendElements(channels);
  in_buffer[0] = in_ch1;
  in_buffer[1] = in_ch2;

  short out_ch1[10] = {};
  short out_ch2[10] = {};

  dr.AppendInput(in_buffer, in_frames);
  bool hasUnderrun = dr.Resample(out_ch1, out_frames, 0);
  EXPECT_FALSE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch2, out_frames, 1);
  EXPECT_FALSE(hasUnderrun);

  // Add 3rd channel
  dr.UpdateResampler(out_rate, 3);
  EXPECT_EQ(dr.GetOutRate(), out_rate);
  EXPECT_EQ(dr.GetChannels(), 3u);

  short in_ch3[10] = {};
  for (uint32_t i = 0; i < in_frames; ++i) {
    in_ch3[i] = i;
  }
  in_buffer.AppendElement();
  in_buffer[2] = in_ch3;
  short out_ch3[10] = {};

  dr.AppendInput(in_buffer, in_frames);

  hasUnderrun = dr.Resample(out_ch1, out_frames, 0);
  EXPECT_FALSE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch2, out_frames, 1);
  EXPECT_FALSE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch3, out_frames, 2);
  EXPECT_FALSE(hasUnderrun);

  // Check update with AudioSegment
  short in_ch4[10] = {};
  for (uint32_t i = 0; i < in_frames; ++i) {
    in_ch3[i] = i;
  }
  in_buffer.AppendElement();
  in_buffer[3] = in_ch4;
  short out_ch4[10] = {};

  dr.UpdateResampler(out_rate, 4);
  EXPECT_EQ(dr.GetOutRate(), out_rate);
  EXPECT_EQ(dr.GetChannels(), 4u);
  dr.AppendInput(in_buffer, in_frames);

  hasUnderrun = dr.Resample(out_ch1, out_frames, 0);
  EXPECT_FALSE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch2, out_frames, 1);
  EXPECT_FALSE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch3, out_frames, 2);
  EXPECT_FALSE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch4, out_frames, 3);
  EXPECT_FALSE(hasUnderrun);
}

TEST(TestDynamicResampler, Underrun)
{
  const uint32_t in_frames = 100;
  const uint32_t out_frames = 200;
  uint32_t channels = 2;
  uint32_t in_rate = 48000;
  uint32_t out_rate = 48000;

  DynamicResampler dr(in_rate, out_rate);
  dr.SetSampleFormat(AUDIO_FORMAT_FLOAT32);
  EXPECT_EQ(dr.GetOutRate(), out_rate);
  EXPECT_EQ(dr.GetChannels(), channels);

  float in_ch1[in_frames] = {};
  float in_ch2[in_frames] = {};
  AutoTArray<const float*, 2> in_buffer;
  in_buffer.AppendElements(channels);
  in_buffer[0] = in_ch1;
  in_buffer[1] = in_ch2;

  float out_ch1[out_frames] = {};
  float out_ch2[out_frames] = {};

  for (uint32_t i = 0; i < in_frames; ++i) {
    in_ch1[i] = 0.01f * i;
    in_ch2[i] = -0.01f * i;
  }
  dr.AppendInput(in_buffer, in_frames);
  bool hasUnderrun = dr.Resample(out_ch1, out_frames, 0);
  EXPECT_TRUE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch2, out_frames, 1);
  EXPECT_TRUE(hasUnderrun);
  for (uint32_t i = 0; i < in_frames; ++i) {
    EXPECT_EQ(out_ch1[i], in_ch1[i]);
    EXPECT_EQ(out_ch2[i], in_ch2[i]);
  }
  for (uint32_t i = in_frames; i < out_frames; ++i) {
    EXPECT_EQ(out_ch1[i], 0.0f) << "for i=" << i;
    EXPECT_EQ(out_ch2[i], 0.0f) << "for i=" << i;
  }

  // No more frames in the input buffer
  hasUnderrun = dr.Resample(out_ch1, out_frames, 0);
  EXPECT_TRUE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch2, out_frames, 1);
  EXPECT_TRUE(hasUnderrun);
  for (uint32_t i = 0; i < out_frames; ++i) {
    EXPECT_EQ(out_ch1[i], 0.0f) << "for i=" << i;
    EXPECT_EQ(out_ch2[i], 0.0f) << "for i=" << i;
  }

  // Now try with resampling.
  dr.UpdateResampler(out_rate / 2, channels);
  dr.AppendInput(in_buffer, in_frames);
  hasUnderrun = dr.Resample(out_ch1, out_frames, 0);
  EXPECT_TRUE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch2, out_frames, 1);
  EXPECT_TRUE(hasUnderrun);
  // There is some buffering in the resampler, which is why the below is not
  // exact.
  for (uint32_t i = 0; i < 50; ++i) {
    EXPECT_GT(out_ch1[i], 0.0f) << "for i=" << i;
    EXPECT_LT(out_ch2[i], 0.0f) << "for i=" << i;
  }
  for (uint32_t i = 50; i < 54; ++i) {
    EXPECT_NE(out_ch1[i], 0.0f) << "for i=" << i;
    EXPECT_NE(out_ch2[i], 0.0f) << "for i=" << i;
  }
  for (uint32_t i = 54; i < out_frames; ++i) {
    EXPECT_EQ(out_ch1[i], 0.0f) << "for i=" << i;
    EXPECT_EQ(out_ch2[i], 0.0f) << "for i=" << i;
  }

  // No more frames in the input buffer
  hasUnderrun = dr.Resample(out_ch1, out_frames, 0);
  EXPECT_TRUE(hasUnderrun);
  hasUnderrun = dr.Resample(out_ch2, out_frames, 1);
  EXPECT_TRUE(hasUnderrun);
  for (uint32_t i = 0; i < out_frames; ++i) {
    EXPECT_EQ(out_ch1[i], 0.0f) << "for i=" << i;
    EXPECT_EQ(out_ch2[i], 0.0f) << "for i=" << i;
  }
}
