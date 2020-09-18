/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gmock/gmock.h"
#include "gtest/gtest-printers.h"
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
  uint32_t out_frames_used = out_frames;
  bool rv = dr.Resample(out_ch1, &out_frames_used, 0);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames_used, out_frames);
  rv = dr.Resample(out_ch2, &out_frames_used, 1);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames_used, out_frames);
  for (uint32_t i = 0; i < out_frames; ++i) {
    EXPECT_FLOAT_EQ(in_ch1[i], out_ch1[i]);
    EXPECT_FLOAT_EQ(in_ch2[i], out_ch2[i]);
  }

  // Continue with non zero
  for (uint32_t i = 0; i < in_frames; ++i) {
    in_ch1[i] = in_ch2[i] = 0.01f * i;
  }
  dr.AppendInput(in_buffer, in_frames);
  out_frames_used = out_frames;
  rv = dr.Resample(out_ch1, &out_frames_used, 0);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames_used, out_frames);
  rv = dr.Resample(out_ch2, &out_frames_used, 1);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames_used, out_frames);
  for (uint32_t i = 0; i < out_frames; ++i) {
    EXPECT_FLOAT_EQ(in_ch1[i], out_ch1[i]);
    EXPECT_FLOAT_EQ(in_ch2[i], out_ch2[i]);
  }

  // No more frames in the input buffer
  rv = dr.Resample(out_ch1, &out_frames_used, 0);
  EXPECT_FALSE(rv);
  EXPECT_EQ(out_frames_used, 0u);
  out_frames_used = 2;
  rv = dr.Resample(out_ch2, &out_frames_used, 1);
  EXPECT_FALSE(rv);
  EXPECT_EQ(out_frames_used, 0u);
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
  bool rv = dr.Resample(out_ch1, &out_frames, 0);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 2u);
  rv = dr.Resample(out_ch2, &out_frames, 1);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 2u);
  for (uint32_t i = 0; i < out_frames; ++i) {
    EXPECT_EQ(in_ch1[i], out_ch1[i]);
    EXPECT_EQ(in_ch2[i], out_ch2[i]);
  }

  // No more frames in the input buffer
  rv = dr.Resample(out_ch1, &out_frames, 0);
  EXPECT_FALSE(rv);
  EXPECT_EQ(out_frames, 0u);
  out_frames = 2;
  rv = dr.Resample(out_ch2, &out_frames, 1);
  EXPECT_FALSE(rv);
  EXPECT_EQ(out_frames, 0u);
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
  bool rv = dr.Resample(out_ch1, &out_frames, 0);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 2u);
  rv = dr.Resample(out_ch2, &out_frames, 1);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 2u);
  for (uint32_t i = 0; i < out_frames; ++i) {
    EXPECT_FLOAT_EQ(in_ch1[i], out_ch1[i]);
    EXPECT_FLOAT_EQ(in_ch2[i], out_ch2[i]);
  }

  out_frames = 1;
  rv = dr.Resample(out_ch1, &out_frames, 0);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 1u);
  rv = dr.Resample(out_ch2, &out_frames, 1);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 1u);
  for (uint32_t i = 0; i < out_frames; ++i) {
    EXPECT_FLOAT_EQ(in_ch1[i + 2], out_ch1[i]);
    EXPECT_FLOAT_EQ(in_ch2[i + 2], out_ch2[i]);
  }

  // No more frames, the input buffer has drained
  rv = dr.Resample(out_ch1, &out_frames, 0);
  EXPECT_FALSE(rv);
  EXPECT_EQ(out_frames, 0u);
  out_frames = 1;
  rv = dr.Resample(out_ch2, &out_frames, 1);
  EXPECT_FALSE(rv);
  EXPECT_EQ(out_frames, 0u);
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
  bool rv = dr.Resample(out_ch1, &out_frames, 0);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 2u);
  rv = dr.Resample(out_ch2, &out_frames, 1);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 2u);
  for (uint32_t i = 0; i < out_frames; ++i) {
    EXPECT_EQ(in_ch1[i], out_ch1[i]);
    EXPECT_EQ(in_ch2[i], out_ch2[i]);
  }

  out_frames = 1;
  rv = dr.Resample(out_ch1, &out_frames, 0);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 1u);
  rv = dr.Resample(out_ch2, &out_frames, 1);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 1u);
  for (uint32_t i = 0; i < out_frames; ++i) {
    EXPECT_EQ(in_ch1[i + 2], out_ch1[i]);
    EXPECT_EQ(in_ch2[i + 2], out_ch2[i]);
  }

  // No more frames, the input buffer has drained
  rv = dr.Resample(out_ch1, &out_frames, 0);
  EXPECT_FALSE(rv);
  EXPECT_EQ(out_frames, 0u);
  out_frames = 1;
  rv = dr.Resample(out_ch2, &out_frames, 1);
  EXPECT_FALSE(rv);
  EXPECT_EQ(out_frames, 0u);
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
  bool rv = dr.Resample(out_ch1, &out_frames, 0);
  EXPECT_FALSE(rv);
  EXPECT_EQ(out_frames, 0u);
  out_frames = 3;
  rv = dr.Resample(out_ch2, &out_frames, 1);
  EXPECT_FALSE(rv);
  EXPECT_EQ(out_frames, 0u);

  // Add one more frame
  in_buffer[0] = in_ch1 + 2;
  in_buffer[1] = in_ch2 + 2;
  dr.AppendInput(in_buffer, 1);
  out_frames = 3;
  rv = dr.Resample(out_ch1, &out_frames, 0);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 3u);
  rv = dr.Resample(out_ch2, &out_frames, 1);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 3u);
  for (uint32_t i = 0; i < out_frames; ++i) {
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
  bool rv = dr.Resample(out_ch1, &out_frames, 0);
  EXPECT_FALSE(rv);
  EXPECT_EQ(out_frames, 0u);
  out_frames = 3;
  rv = dr.Resample(out_ch2, &out_frames, 1);
  EXPECT_FALSE(rv);
  EXPECT_EQ(out_frames, 0u);

  // Add one more frame
  in_buffer[0] = in_ch1 + 2;
  in_buffer[1] = in_ch2 + 2;
  dr.AppendInput(in_buffer, 1);
  out_frames = 3;
  rv = dr.Resample(out_ch1, &out_frames, 0);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 3u);
  rv = dr.Resample(out_ch2, &out_frames, 1);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 3u);
  for (uint32_t i = 0; i < out_frames; ++i) {
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

  DynamicResampler dr(in_rate, out_rate, pre_buffer);
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

  dr.AppendInput(in_buffer, in_frames);
  bool rv = dr.Resample(out_ch1, &out_frames, 0);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 40u);
  rv = dr.Resample(out_ch2, &out_frames, 1);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 40u);
  for (uint32_t i = 0; i < out_frames; ++i) {
    // Only pre buffered data reach output
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
  rv = dr.Resample(out_ch1, &out_frames, 0);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 18u);
  rv = dr.Resample(out_ch2, &out_frames, 1);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 18u);
}

TEST(TestDynamicResampler, UpdateOutRate_Short)
{
  uint32_t in_frames = 10;
  uint32_t out_frames = 40;
  uint32_t channels = 2;
  uint32_t in_rate = 24000;
  uint32_t out_rate = 48000;

  uint32_t pre_buffer = 20;

  DynamicResampler dr(in_rate, out_rate, pre_buffer);
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

  dr.AppendInput(in_buffer, in_frames);
  bool rv = dr.Resample(out_ch1, &out_frames, 0);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 40u);
  rv = dr.Resample(out_ch2, &out_frames, 1);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 40u);
  for (uint32_t i = 0; i < out_frames; ++i) {
    // Only pre buffered data reach output
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
  rv = dr.Resample(out_ch1, &out_frames, 0);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 18u);
  rv = dr.Resample(out_ch2, &out_frames, 1);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 18u);
}

TEST(TestDynamicResampler, BigRangeOutRates_Float)
{
  uint32_t in_frames = 10;
  uint32_t out_frames = 10;
  uint32_t channels = 2;
  uint32_t in_rate = 44100;
  uint32_t out_rate = 44100;
  uint32_t pre_buffer = 20;

  DynamicResampler dr(in_rate, out_rate, pre_buffer);
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
    uint32_t expected_out_frames = out_frames;
    for (uint32_t y = 0; y < 2; ++y) {
      dr.AppendInput(in_buffer, in_frames);
      bool rv = dr.Resample(out_ch1, &out_frames, 0);
      EXPECT_TRUE(rv);
      EXPECT_EQ(out_frames, expected_out_frames);
      rv = dr.Resample(out_ch2, &out_frames, 1);
      EXPECT_TRUE(rv);
      EXPECT_EQ(out_frames, expected_out_frames);
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

  DynamicResampler dr(in_rate, out_rate, pre_buffer);
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
    uint32_t expected_out_frames = out_frames;
    for (uint32_t y = 0; y < 2; ++y) {
      dr.AppendInput(in_buffer, in_frames);
      bool rv = dr.Resample(out_ch1, &out_frames, 0);
      EXPECT_TRUE(rv);
      EXPECT_EQ(out_frames, expected_out_frames);
      rv = dr.Resample(out_ch2, &out_frames, 1);
      EXPECT_TRUE(rv);
      EXPECT_EQ(out_frames, expected_out_frames);
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
  bool rv = dr.Resample(out_ch1, &out_frames, 0);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 10u);
  rv = dr.Resample(out_ch2, &out_frames, 1);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 10u);

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

  rv = dr.Resample(out_ch1, &out_frames, 0);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 10u);
  rv = dr.Resample(out_ch2, &out_frames, 1);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 10u);
  rv = dr.Resample(out_ch3, &out_frames, 2);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 10u);

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

  rv = dr.Resample(out_ch1, &out_frames, 0);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 10u);
  rv = dr.Resample(out_ch2, &out_frames, 1);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 10u);
  rv = dr.Resample(out_ch3, &out_frames, 2);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 10u);
  rv = dr.Resample(out_ch4, &out_frames, 3);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 10u);
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
  bool rv = dr.Resample(out_ch1, &out_frames, 0);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 10u);
  rv = dr.Resample(out_ch2, &out_frames, 1);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 10u);

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

  rv = dr.Resample(out_ch1, &out_frames, 0);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 10u);
  rv = dr.Resample(out_ch2, &out_frames, 1);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 10u);
  rv = dr.Resample(out_ch3, &out_frames, 2);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 10u);

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

  rv = dr.Resample(out_ch1, &out_frames, 0);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 10u);
  rv = dr.Resample(out_ch2, &out_frames, 1);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 10u);
  rv = dr.Resample(out_ch3, &out_frames, 2);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 10u);
  rv = dr.Resample(out_ch4, &out_frames, 3);
  EXPECT_TRUE(rv);
  EXPECT_EQ(out_frames, 10u);
}

TEST(TestAudioChunkList, Basic1)
{
  AudioChunkList list(256, 2);
  list.SetSampleFormat(AUDIO_FORMAT_FLOAT32);
  EXPECT_EQ(list.ChunkCapacity(), 128u);
  EXPECT_EQ(list.TotalCapacity(), 256u);

  AudioChunk& c1 = list.GetNext();
  float* c1_ch1 = c1.ChannelDataForWrite<float>(0);
  float* c1_ch2 = c1.ChannelDataForWrite<float>(1);
  EXPECT_EQ(c1.mBufferFormat, AUDIO_FORMAT_FLOAT32);
  for (uint32_t i = 0; i < list.ChunkCapacity(); ++i) {
    c1_ch1[i] = c1_ch2[i] = 0.01f * static_cast<float>(i);
  }
  AudioChunk& c2 = list.GetNext();
  EXPECT_EQ(c2.mBufferFormat, AUDIO_FORMAT_FLOAT32);
  EXPECT_NE(c1.mBuffer.get(), c2.mBuffer.get());
  AudioChunk& c3 = list.GetNext();
  EXPECT_EQ(c3.mBufferFormat, AUDIO_FORMAT_FLOAT32);
  // Cycle
  EXPECT_EQ(c1.mBuffer.get(), c3.mBuffer.get());
  float* c3_ch1 = c3.ChannelDataForWrite<float>(0);
  float* c3_ch2 = c3.ChannelDataForWrite<float>(1);
  for (uint32_t i = 0; i < list.ChunkCapacity(); ++i) {
    EXPECT_FLOAT_EQ(c1_ch1[i], c3_ch1[i]);
    EXPECT_FLOAT_EQ(c1_ch2[i], c3_ch2[i]);
  }
}

TEST(TestAudioChunkList, Basic2)
{
  AudioChunkList list(256, 2);
  list.SetSampleFormat(AUDIO_FORMAT_S16);
  EXPECT_EQ(list.ChunkCapacity(), 256u);
  EXPECT_EQ(list.TotalCapacity(), 512u);

  AudioChunk& c1 = list.GetNext();
  EXPECT_EQ(c1.mBufferFormat, AUDIO_FORMAT_S16);
  short* c1_ch1 = c1.ChannelDataForWrite<short>(0);
  short* c1_ch2 = c1.ChannelDataForWrite<short>(1);
  for (uint32_t i = 0; i < list.ChunkCapacity(); ++i) {
    c1_ch1[i] = c1_ch2[i] = static_cast<short>(i);
  }
  AudioChunk& c2 = list.GetNext();
  EXPECT_EQ(c2.mBufferFormat, AUDIO_FORMAT_S16);
  EXPECT_NE(c1.mBuffer.get(), c2.mBuffer.get());
  AudioChunk& c3 = list.GetNext();
  EXPECT_EQ(c3.mBufferFormat, AUDIO_FORMAT_S16);
  AudioChunk& c4 = list.GetNext();
  EXPECT_EQ(c4.mBufferFormat, AUDIO_FORMAT_S16);
  // Cycle
  AudioChunk& c5 = list.GetNext();
  EXPECT_EQ(c5.mBufferFormat, AUDIO_FORMAT_S16);
  EXPECT_EQ(c1.mBuffer.get(), c5.mBuffer.get());
  short* c5_ch1 = c5.ChannelDataForWrite<short>(0);
  short* c5_ch2 = c5.ChannelDataForWrite<short>(1);
  for (uint32_t i = 0; i < list.ChunkCapacity(); ++i) {
    EXPECT_EQ(c1_ch1[i], c5_ch1[i]);
    EXPECT_EQ(c1_ch2[i], c5_ch2[i]);
  }
}

TEST(TestAudioChunkList, Basic3)
{
  AudioChunkList list(260, 2);
  list.SetSampleFormat(AUDIO_FORMAT_FLOAT32);
  EXPECT_EQ(list.ChunkCapacity(), 128u);
  EXPECT_EQ(list.TotalCapacity(), 256u + 128u);

  AudioChunk& c1 = list.GetNext();
  AudioChunk& c2 = list.GetNext();
  EXPECT_NE(c1.mBuffer.get(), c2.mBuffer.get());
  AudioChunk& c3 = list.GetNext();
  EXPECT_NE(c1.mBuffer.get(), c3.mBuffer.get());
  AudioChunk& c4 = list.GetNext();
  EXPECT_EQ(c1.mBuffer.get(), c4.mBuffer.get());
}

TEST(TestAudioChunkList, Basic4)
{
  AudioChunkList list(260, 2);
  list.SetSampleFormat(AUDIO_FORMAT_S16);
  EXPECT_EQ(list.ChunkCapacity(), 256u);
  EXPECT_EQ(list.TotalCapacity(), 512u + 256u);

  AudioChunk& c1 = list.GetNext();
  AudioChunk& c2 = list.GetNext();
  EXPECT_NE(c1.mBuffer.get(), c2.mBuffer.get());
  AudioChunk& c3 = list.GetNext();
  EXPECT_NE(c1.mBuffer.get(), c3.mBuffer.get());
  AudioChunk& c4 = list.GetNext();
  EXPECT_EQ(c1.mBuffer.get(), c4.mBuffer.get());
}

TEST(TestAudioChunkList, UpdateChannels)
{
  AudioChunkList list(256, 2);
  list.SetSampleFormat(AUDIO_FORMAT_FLOAT32);

  AudioChunk& c1 = list.GetNext();
  AudioChunk& c2 = list.GetNext();
  EXPECT_EQ(c1.ChannelCount(), 2u);
  EXPECT_EQ(c2.ChannelCount(), 2u);

  // Update to Quad
  list.Update(4);

  AudioChunk& c3 = list.GetNext();
  AudioChunk& c4 = list.GetNext();
  EXPECT_EQ(c3.ChannelCount(), 4u);
  EXPECT_EQ(c4.ChannelCount(), 4u);
}

TEST(TestAudioChunkList, UpdateBetweenMonoAndStereo)
{
  AudioChunkList list(256, 2);
  list.SetSampleFormat(AUDIO_FORMAT_FLOAT32);

  AudioChunk& c1 = list.GetNext();
  float* c1_ch1 = c1.ChannelDataForWrite<float>(0);
  float* c1_ch2 = c1.ChannelDataForWrite<float>(1);
  for (uint32_t i = 0; i < list.ChunkCapacity(); ++i) {
    c1_ch1[i] = c1_ch2[i] = 0.01f * static_cast<float>(i);
  }

  AudioChunk& c2 = list.GetNext();
  EXPECT_EQ(c1.ChannelCount(), 2u);
  EXPECT_EQ(c2.ChannelCount(), 2u);

  // Downmix to mono
  list.Update(1);

  AudioChunk& c3 = list.GetNext();
  float* c3_ch1 = c3.ChannelDataForWrite<float>(0);
  for (uint32_t i = 0; i < list.ChunkCapacity(); ++i) {
    EXPECT_FLOAT_EQ(c3_ch1[i], c1_ch1[i]);
  }

  AudioChunk& c4 = list.GetNext();
  EXPECT_EQ(c3.ChannelCount(), 1u);
  EXPECT_EQ(c4.ChannelCount(), 1u);
  EXPECT_EQ(static_cast<SharedChannelArrayBuffer<float>*>(c3.mBuffer.get())
                ->mBuffers[0]
                .Length(),
            list.ChunkCapacity());

  // Upmix to stereo
  list.Update(2);

  AudioChunk& c5 = list.GetNext();
  AudioChunk& c6 = list.GetNext();
  EXPECT_EQ(c5.ChannelCount(), 2u);
  EXPECT_EQ(c6.ChannelCount(), 2u);
  EXPECT_EQ(static_cast<SharedChannelArrayBuffer<float>*>(c5.mBuffer.get())
                ->mBuffers[0]
                .Length(),
            list.ChunkCapacity());
  EXPECT_EQ(static_cast<SharedChannelArrayBuffer<float>*>(c5.mBuffer.get())
                ->mBuffers[1]
                .Length(),
            list.ChunkCapacity());

  // Downmix to mono
  list.Update(1);

  AudioChunk& c7 = list.GetNext();
  float* c7_ch1 = c7.ChannelDataForWrite<float>(0);
  for (uint32_t i = 0; i < list.ChunkCapacity(); ++i) {
    EXPECT_FLOAT_EQ(c7_ch1[i], c1_ch1[i]);
  }

  AudioChunk& c8 = list.GetNext();
  EXPECT_EQ(c7.ChannelCount(), 1u);
  EXPECT_EQ(c8.ChannelCount(), 1u);
  EXPECT_EQ(static_cast<SharedChannelArrayBuffer<float>*>(c7.mBuffer.get())
                ->mBuffers[0]
                .Length(),
            list.ChunkCapacity());
}

TEST(TestAudioChunkList, ConsumeAndForget)
{
  AudioSegment s;
  AudioChunkList list(256, 2);
  list.SetSampleFormat(AUDIO_FORMAT_FLOAT32);

  AudioChunk& c1 = list.GetNext();
  AudioChunk tmp = c1;
  s.AppendAndConsumeChunk(&tmp);
  EXPECT_FALSE(c1.mBuffer.get() == nullptr);
  EXPECT_EQ(c1.ChannelData<float>().Length(), 2u);

  AudioChunk& c2 = list.GetNext();
  tmp = c2;
  s.AppendAndConsumeChunk(&tmp);
  EXPECT_FALSE(c2.mBuffer.get() == nullptr);
  EXPECT_EQ(c2.ChannelData<float>().Length(), 2u);

  s.ForgetUpTo(256);
  list.GetNext();
  list.GetNext();
}

template <class T>
AudioChunk CreateAudioChunk(uint32_t aFrames, uint32_t aChannels,
                            AudioSampleFormat aSampleFormat) {
  AudioChunk chunk;
  nsTArray<nsTArray<T>> buffer;
  buffer.AppendElements(aChannels);

  nsTArray<const T*> bufferPtrs;
  bufferPtrs.AppendElements(aChannels);

  for (uint32_t i = 0; i < aChannels; ++i) {
    T* ptr = buffer[i].AppendElements(aFrames);
    bufferPtrs[i] = ptr;
    for (uint32_t j = 0; j < aFrames; ++j) {
      if (aSampleFormat == AUDIO_FORMAT_FLOAT32) {
        ptr[j] = 0.01 * j;
      } else {
        ptr[j] = j;
      }
    }
  }

  chunk.mBuffer = new mozilla::SharedChannelArrayBuffer(std::move(buffer));
  chunk.mBufferFormat = aSampleFormat;
  chunk.mChannelData.AppendElements(aChannels);
  for (uint32_t i = 0; i < aChannels; ++i) {
    chunk.mChannelData[i] = bufferPtrs[i];
  }
  chunk.mDuration = aFrames;
  return chunk;
}

template <class T>
AudioSegment CreateAudioSegment(uint32_t aFrames, uint32_t aChannels,
                                AudioSampleFormat aSampleFormat) {
  AudioSegment segment;
  AudioChunk chunk = CreateAudioChunk<T>(aFrames, aChannels, aSampleFormat);
  segment.AppendAndConsumeChunk(&chunk);
  return segment;
}

TEST(TestAudioResampler, OutAudioSegment_Float)
{
  uint32_t in_frames = 10;
  uint32_t out_frames = 40;
  uint32_t channels = 2;
  uint32_t in_rate = 24000;
  uint32_t out_rate = 48000;

  uint32_t pre_buffer = 21;

  AudioResampler dr(in_rate, out_rate, pre_buffer);

  AudioSegment inSegment =
      CreateAudioSegment<float>(in_frames, channels, AUDIO_FORMAT_FLOAT32);
  dr.AppendInput(inSegment);

  AudioSegment s = dr.Resample(out_frames);
  EXPECT_EQ(s.GetDuration(), 40);
  EXPECT_EQ(s.GetType(), MediaSegment::AUDIO);
  EXPECT_TRUE(!s.IsNull());
  EXPECT_TRUE(!s.IsEmpty());

  for (AudioSegment::ChunkIterator ci(s); !ci.IsEnded(); ci.Next()) {
    AudioChunk& c = *ci;
    EXPECT_EQ(c.ChannelCount(), 2u);
    for (uint32_t i = 0; i < out_frames; ++i) {
      // Only pre buffered data reach output
      EXPECT_FLOAT_EQ(c.ChannelData<float>()[0][i], 0.0);
      EXPECT_FLOAT_EQ(c.ChannelData<float>()[1][i], 0.0);
    }
  }

  // Update out rate
  out_rate = 44100;
  dr.UpdateOutRate(out_rate);
  out_frames = in_frames * out_rate / in_rate;
  EXPECT_EQ(out_frames, 18u);
  // Even if we provide no input if we have enough buffered input, we can create
  // output
  AudioSegment s1 = dr.Resample(out_frames);
  EXPECT_EQ(s1.GetDuration(), out_frames);
  EXPECT_EQ(s1.GetType(), MediaSegment::AUDIO);
  EXPECT_TRUE(!s1.IsNull());
  EXPECT_TRUE(!s1.IsEmpty());
}

TEST(TestAudioResampler, OutAudioSegment_Short)
{
  uint32_t in_frames = 10;
  uint32_t out_frames = 40;
  uint32_t channels = 2;
  uint32_t in_rate = 24000;
  uint32_t out_rate = 48000;

  uint32_t pre_buffer = 21;

  AudioResampler dr(in_rate, out_rate, pre_buffer);

  AudioSegment inSegment =
      CreateAudioSegment<short>(in_frames, channels, AUDIO_FORMAT_S16);
  dr.AppendInput(inSegment);

  AudioSegment s = dr.Resample(out_frames);
  EXPECT_EQ(s.GetDuration(), 40);
  EXPECT_EQ(s.GetType(), MediaSegment::AUDIO);
  EXPECT_TRUE(!s.IsNull());
  EXPECT_TRUE(!s.IsEmpty());

  for (AudioSegment::ChunkIterator ci(s); !ci.IsEnded(); ci.Next()) {
    AudioChunk& c = *ci;
    EXPECT_EQ(c.ChannelCount(), 2u);
    for (uint32_t i = 0; i < out_frames; ++i) {
      // Only pre buffered data reach output
      EXPECT_FLOAT_EQ(c.ChannelData<short>()[0][i], 0.0);
      EXPECT_FLOAT_EQ(c.ChannelData<short>()[1][i], 0.0);
    }
  }

  // Update out rate
  out_rate = 44100;
  dr.UpdateOutRate(out_rate);
  out_frames = in_frames * out_rate / in_rate;
  EXPECT_EQ(out_frames, 18u);
  // Even if we provide no input if we have enough buffered input, we can create
  // output
  AudioSegment s1 = dr.Resample(out_frames);
  EXPECT_EQ(s1.GetDuration(), out_frames);
  EXPECT_EQ(s1.GetType(), MediaSegment::AUDIO);
  EXPECT_TRUE(!s1.IsNull());
  EXPECT_TRUE(!s1.IsEmpty());
}

TEST(TestAudioResampler, OutAudioSegmentFail_Float)
{
  const uint32_t in_frames = 130;
  const uint32_t out_frames = 300;
  uint32_t channels = 2;
  uint32_t in_rate = 24000;
  uint32_t out_rate = 48000;

  uint32_t pre_buffer = 5;

  AudioResampler dr(in_rate, out_rate, pre_buffer);
  AudioSegment inSegment =
      CreateAudioSegment<float>(in_frames, channels, AUDIO_FORMAT_FLOAT32);
  dr.AppendInput(inSegment);

  AudioSegment s = dr.Resample(out_frames);
  EXPECT_EQ(s.GetDuration(), 0);
  EXPECT_EQ(s.GetType(), MediaSegment::AUDIO);
  EXPECT_TRUE(s.IsNull());
  EXPECT_TRUE(s.IsEmpty());
}

TEST(TestAudioResampler, InAudioSegment_Float)
{
  uint32_t in_frames = 10;
  uint32_t out_frames = 40;
  uint32_t channels = 2;
  uint32_t in_rate = 24000;
  uint32_t out_rate = 48000;

  uint32_t pre_buffer = 10;
  AudioResampler dr(in_rate, out_rate, pre_buffer);

  AudioSegment inSegment;

  AudioChunk chunk1;
  chunk1.SetNull(in_frames / 2);
  inSegment.AppendAndConsumeChunk(&chunk1);

  AudioChunk chunk2;
  nsTArray<nsTArray<float>> buffer;
  buffer.AppendElements(channels);

  nsTArray<const float*> bufferPtrs;
  bufferPtrs.AppendElements(channels);

  for (uint32_t i = 0; i < channels; ++i) {
    float* ptr = buffer[i].AppendElements(5);
    bufferPtrs[i] = ptr;
    for (uint32_t j = 0; j < 5; ++j) {
      ptr[j] = 0.01f * j;
    }
  }

  chunk2.mBuffer = new mozilla::SharedChannelArrayBuffer(std::move(buffer));
  chunk2.mBufferFormat = AUDIO_FORMAT_FLOAT32;
  chunk2.mChannelData.AppendElements(channels);
  for (uint32_t i = 0; i < channels; ++i) {
    chunk2.mChannelData[i] = bufferPtrs[i];
  }
  chunk2.mDuration = in_frames / 2;
  inSegment.AppendAndConsumeChunk(&chunk2);

  dr.AppendInput(inSegment);
  AudioSegment outSegment = dr.Resample(out_frames);
  // Faild because the first chunk is ignored
  EXPECT_EQ(outSegment.GetDuration(), 0u);
  EXPECT_EQ(outSegment.MaxChannelCount(), 0u);

  // Add the 5 more frames that are missing
  dr.AppendInput(inSegment);
  AudioSegment outSegment2 = dr.Resample(out_frames);
  EXPECT_EQ(outSegment2.GetDuration(), 40u);
  EXPECT_EQ(outSegment2.MaxChannelCount(), 2u);
}

TEST(TestAudioResampler, InAudioSegment_Short)
{
  uint32_t in_frames = 10;
  uint32_t out_frames = 40;
  uint32_t channels = 2;
  uint32_t in_rate = 24000;
  uint32_t out_rate = 48000;

  uint32_t pre_buffer = 10;
  AudioResampler dr(in_rate, out_rate, pre_buffer);

  AudioSegment inSegment;

  // The null chunk at the beginning will be ignored.
  AudioChunk chunk1;
  chunk1.SetNull(in_frames / 2);
  inSegment.AppendAndConsumeChunk(&chunk1);

  AudioChunk chunk2;
  nsTArray<nsTArray<short>> buffer;
  buffer.AppendElements(channels);

  nsTArray<const short*> bufferPtrs;
  bufferPtrs.AppendElements(channels);

  for (uint32_t i = 0; i < channels; ++i) {
    short* ptr = buffer[i].AppendElements(5);
    bufferPtrs[i] = ptr;
    for (uint32_t j = 0; j < 5; ++j) {
      ptr[j] = j;
    }
  }

  chunk2.mBuffer = new mozilla::SharedChannelArrayBuffer(std::move(buffer));
  chunk2.mBufferFormat = AUDIO_FORMAT_S16;
  chunk2.mChannelData.AppendElements(channels);
  for (uint32_t i = 0; i < channels; ++i) {
    chunk2.mChannelData[i] = bufferPtrs[i];
  }
  chunk2.mDuration = in_frames / 2;
  inSegment.AppendAndConsumeChunk(&chunk2);

  dr.AppendInput(inSegment);
  AudioSegment outSegment = dr.Resample(out_frames);
  // Faild because the first chunk is ignored
  EXPECT_EQ(outSegment.GetDuration(), 0u);
  EXPECT_EQ(outSegment.MaxChannelCount(), 0u);

  dr.AppendInput(inSegment);
  AudioSegment outSegment2 = dr.Resample(out_frames);
  EXPECT_EQ(outSegment2.GetDuration(), 40u);
  EXPECT_EQ(outSegment2.MaxChannelCount(), 2u);
}

TEST(TestAudioResampler, ChannelChange_MonoToStereo)
{
  uint32_t in_frames = 10;
  uint32_t out_frames = 40;
  // uint32_t channels = 2;
  uint32_t in_rate = 24000;
  uint32_t out_rate = 48000;

  uint32_t pre_buffer = 0;

  AudioResampler dr(in_rate, out_rate, pre_buffer);

  AudioChunk monoChunk =
      CreateAudioChunk<float>(in_frames, 1, AUDIO_FORMAT_FLOAT32);
  AudioChunk stereoChunk =
      CreateAudioChunk<float>(in_frames, 2, AUDIO_FORMAT_FLOAT32);

  AudioSegment inSegment;
  inSegment.AppendAndConsumeChunk(&monoChunk);
  inSegment.AppendAndConsumeChunk(&stereoChunk);
  dr.AppendInput(inSegment);

  AudioSegment s = dr.Resample(out_frames);
  EXPECT_EQ(s.GetDuration(), 40);
  EXPECT_EQ(s.GetType(), MediaSegment::AUDIO);
  EXPECT_TRUE(!s.IsNull());
  EXPECT_TRUE(!s.IsEmpty());
  EXPECT_EQ(s.MaxChannelCount(), 2u);
}

TEST(TestAudioResampler, ChannelChange_StereoToMono)
{
  uint32_t in_frames = 10;
  uint32_t out_frames = 40;
  // uint32_t channels = 2;
  uint32_t in_rate = 24000;
  uint32_t out_rate = 48000;

  uint32_t pre_buffer = 0;

  AudioResampler dr(in_rate, out_rate, pre_buffer);

  AudioChunk monoChunk =
      CreateAudioChunk<float>(in_frames, 1, AUDIO_FORMAT_FLOAT32);
  AudioChunk stereoChunk =
      CreateAudioChunk<float>(in_frames, 2, AUDIO_FORMAT_FLOAT32);

  AudioSegment inSegment;
  inSegment.AppendAndConsumeChunk(&stereoChunk);
  inSegment.AppendAndConsumeChunk(&monoChunk);
  dr.AppendInput(inSegment);

  AudioSegment s = dr.Resample(out_frames);
  EXPECT_EQ(s.GetDuration(), 40);
  EXPECT_EQ(s.GetType(), MediaSegment::AUDIO);
  EXPECT_TRUE(!s.IsNull());
  EXPECT_TRUE(!s.IsEmpty());
  EXPECT_EQ(s.MaxChannelCount(), 1u);
}

TEST(TestAudioResampler, ChannelChange_StereoToQuad)
{
  uint32_t in_frames = 10;
  uint32_t out_frames = 40;
  // uint32_t channels = 2;
  uint32_t in_rate = 24000;
  uint32_t out_rate = 48000;

  uint32_t pre_buffer = 0;

  AudioResampler dr(in_rate, out_rate, pre_buffer);

  AudioChunk stereoChunk =
      CreateAudioChunk<float>(in_frames, 2, AUDIO_FORMAT_FLOAT32);
  AudioChunk quadChunk =
      CreateAudioChunk<float>(in_frames, 4, AUDIO_FORMAT_FLOAT32);

  AudioSegment inSegment;
  inSegment.AppendAndConsumeChunk(&stereoChunk);
  inSegment.AppendAndConsumeChunk(&quadChunk);
  dr.AppendInput(inSegment);

  AudioSegment s = dr.Resample(out_frames);
  EXPECT_EQ(s.GetDuration(), 0);
  EXPECT_EQ(s.GetType(), MediaSegment::AUDIO);
  EXPECT_TRUE(s.IsNull());
  EXPECT_TRUE(s.IsEmpty());

  AudioSegment s2 = dr.Resample(out_frames / 2);
  EXPECT_EQ(s2.GetDuration(), out_frames / 2);
  EXPECT_EQ(s2.GetType(), MediaSegment::AUDIO);
  EXPECT_TRUE(!s2.IsNull());
  EXPECT_TRUE(!s2.IsEmpty());
}

TEST(TestAudioResampler, ChannelChange_QuadToStereo)
{
  uint32_t in_frames = 10;
  uint32_t out_frames = 40;
  // uint32_t channels = 2;
  uint32_t in_rate = 24000;
  uint32_t out_rate = 48000;

  AudioResampler dr(in_rate, out_rate);

  AudioChunk stereoChunk =
      CreateAudioChunk<float>(in_frames, 2, AUDIO_FORMAT_FLOAT32);
  AudioChunk quadChunk =
      CreateAudioChunk<float>(in_frames, 4, AUDIO_FORMAT_FLOAT32);

  AudioSegment inSegment;
  inSegment.AppendAndConsumeChunk(&quadChunk);
  inSegment.AppendAndConsumeChunk(&stereoChunk);
  dr.AppendInput(inSegment);

  AudioSegment s = dr.Resample(out_frames);
  EXPECT_EQ(s.GetDuration(), 0);
  EXPECT_EQ(s.GetType(), MediaSegment::AUDIO);
  EXPECT_TRUE(s.IsNull());
  EXPECT_TRUE(s.IsEmpty());

  AudioSegment s2 = dr.Resample(out_frames / 2);
  EXPECT_EQ(s2.GetDuration(), out_frames / 2);
  EXPECT_EQ(s2.GetType(), MediaSegment::AUDIO);
  EXPECT_TRUE(!s2.IsNull());
  EXPECT_TRUE(!s2.IsEmpty());
}

void printAudioSegment(const AudioSegment& segment);

TEST(TestAudioResampler, ChannelChange_Discontinuity)
{
  uint32_t in_rate = 24000;
  uint32_t out_rate = 48000;

  const float amplitude = 0.5;
  const float frequency = 200;
  const float phase = 0.0;
  float time = 0.0;
  const float deltaTime = 1.0f / static_cast<float>(in_rate);

  uint32_t in_frames = in_rate / 100;
  uint32_t out_frames = out_rate / 100;
  AudioResampler dr(in_rate, out_rate);

  AudioChunk monoChunk =
      CreateAudioChunk<float>(in_frames, 1, AUDIO_FORMAT_FLOAT32);
  for (uint32_t i = 0; i < monoChunk.GetDuration(); ++i) {
    double value = amplitude * sin(2 * M_PI * frequency * time + phase);
    monoChunk.ChannelDataForWrite<float>(0)[i] = static_cast<float>(value);
    time += deltaTime;
  }
  AudioChunk stereoChunk =
      CreateAudioChunk<float>(in_frames, 2, AUDIO_FORMAT_FLOAT32);
  for (uint32_t i = 0; i < stereoChunk.GetDuration(); ++i) {
    double value = amplitude * sin(2 * M_PI * frequency * time + phase);
    stereoChunk.ChannelDataForWrite<float>(0)[i] = static_cast<float>(value);
    if (stereoChunk.ChannelCount() == 2) {
      stereoChunk.ChannelDataForWrite<float>(1)[i] = value;
    }
    time += deltaTime;
  }

  AudioSegment inSegment;
  inSegment.AppendAndConsumeChunk(&stereoChunk);
  // printAudioSegment(inSegment);

  dr.AppendInput(inSegment);
  AudioSegment s = dr.Resample(out_frames);
  // printAudioSegment(s);

  AudioSegment inSegment2;
  inSegment2.AppendAndConsumeChunk(&monoChunk);
  // The resampler here is updated due to the channel change and that creates
  // discontinuity.
  dr.AppendInput(inSegment2);
  AudioSegment s2 = dr.Resample(out_frames);
  // printAudioSegment(s2);

  EXPECT_EQ(s2.GetDuration(), 480);
  EXPECT_EQ(s2.GetType(), MediaSegment::AUDIO);
  EXPECT_TRUE(!s2.IsNull());
  EXPECT_TRUE(!s2.IsEmpty());
  EXPECT_EQ(s2.MaxChannelCount(), 1u);
}

TEST(TestAudioResampler, ChannelChange_Discontinuity2)
{
  uint32_t in_rate = 24000;
  uint32_t out_rate = 48000;

  const float amplitude = 0.5;
  const float frequency = 200;
  const float phase = 0.0;
  float time = 0.0;
  const float deltaTime = 1.0f / static_cast<float>(in_rate);

  uint32_t in_frames = in_rate / 100;
  uint32_t out_frames = out_rate / 100;
  AudioResampler dr(in_rate, out_rate, 10);

  AudioChunk monoChunk =
      CreateAudioChunk<float>(in_frames / 2, 1, AUDIO_FORMAT_FLOAT32);
  for (uint32_t i = 0; i < monoChunk.GetDuration(); ++i) {
    double value = amplitude * sin(2 * M_PI * frequency * time + phase);
    monoChunk.ChannelDataForWrite<float>(0)[i] = static_cast<float>(value);
    time += deltaTime;
  }
  AudioChunk stereoChunk =
      CreateAudioChunk<float>(in_frames / 2, 2, AUDIO_FORMAT_FLOAT32);
  for (uint32_t i = 0; i < stereoChunk.GetDuration(); ++i) {
    double value = amplitude * sin(2 * M_PI * frequency * time + phase);
    stereoChunk.ChannelDataForWrite<float>(0)[i] = static_cast<float>(value);
    if (stereoChunk.ChannelCount() == 2) {
      stereoChunk.ChannelDataForWrite<float>(1)[i] = value;
    }
    time += deltaTime;
  }

  AudioSegment inSegment;
  inSegment.AppendAndConsumeChunk(&monoChunk);
  inSegment.AppendAndConsumeChunk(&stereoChunk);
  // printAudioSegment(inSegment);

  dr.AppendInput(inSegment);
  AudioSegment s1 = dr.Resample(out_frames);
  // printAudioSegment(s1);

  EXPECT_EQ(s1.GetDuration(), 480);
  EXPECT_EQ(s1.GetType(), MediaSegment::AUDIO);
  EXPECT_TRUE(!s1.IsNull());
  EXPECT_TRUE(!s1.IsEmpty());
  EXPECT_EQ(s1.MaxChannelCount(), 2u);

  // The resampler here is updated due to the channel change and that creates
  // discontinuity.
  dr.AppendInput(inSegment);
  AudioSegment s2 = dr.Resample(out_frames);
  // printAudioSegment(s2);

  EXPECT_EQ(s2.GetDuration(), 480);
  EXPECT_EQ(s2.GetType(), MediaSegment::AUDIO);
  EXPECT_TRUE(!s2.IsNull());
  EXPECT_TRUE(!s2.IsEmpty());
  EXPECT_EQ(s2.MaxChannelCount(), 2u);
}

TEST(TestAudioResampler, ChannelChange_Discontinuity3)
{
  uint32_t in_rate = 48000;
  uint32_t out_rate = 48000;

  const float amplitude = 0.5;
  const float frequency = 200;
  const float phase = 0.0;
  float time = 0.0;
  const float deltaTime = 1.0f / static_cast<float>(in_rate);

  uint32_t in_frames = in_rate / 100;
  uint32_t out_frames = out_rate / 100;
  AudioResampler dr(in_rate, out_rate, 10);

  AudioChunk stereoChunk =
      CreateAudioChunk<float>(in_frames, 2, AUDIO_FORMAT_FLOAT32);
  for (uint32_t i = 0; i < stereoChunk.GetDuration(); ++i) {
    double value = amplitude * sin(2 * M_PI * frequency * time + phase);
    stereoChunk.ChannelDataForWrite<float>(0)[i] = static_cast<float>(value);
    if (stereoChunk.ChannelCount() == 2) {
      stereoChunk.ChannelDataForWrite<float>(1)[i] = value;
    }
    time += deltaTime;
  }

  AudioSegment inSegment;
  inSegment.AppendAndConsumeChunk(&stereoChunk);
  // printAudioSegment(inSegment);

  dr.AppendInput(inSegment);
  AudioSegment s = dr.Resample(out_frames);
  // printAudioSegment(s);

  // The resampler here is updated due to the rate change. This is because the
  // in and out rate was the same so a pass through logice was used. By updating
  // the out rate to something different than the in rate, the resampler will
  // start being use dand discontinuity will exist.
  dr.UpdateOutRate(out_rate + 100);
  dr.AppendInput(inSegment);
  AudioSegment s2 = dr.Resample(out_frames);
  // printAudioSegment(s2);

  EXPECT_EQ(s2.GetDuration(), 480);
  EXPECT_EQ(s2.GetType(), MediaSegment::AUDIO);
  EXPECT_TRUE(!s2.IsNull());
  EXPECT_TRUE(!s2.IsEmpty());
  EXPECT_EQ(s2.MaxChannelCount(), 2u);
}
