/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>
#include "AudioBufferUtils.h"
#include "gtest/gtest.h"
#include <vector>

const uint32_t FRAMES = 256;

void test_for_number_of_channels(const uint32_t channels) {
  const uint32_t samples = channels * FRAMES;

  mozilla::AudioCallbackBufferWrapper<float> mBuffer(channels);
  mozilla::SpillBuffer<float, 128> b(channels);
  std::vector<float> fromCallback(samples, 0.0);
  std::vector<float> other(samples, 1.0);

  // Set the buffer in the wrapper from the callback
  mBuffer.SetBuffer(fromCallback.data(), FRAMES);

  // Fill the SpillBuffer with data.
  ASSERT_TRUE(b.Fill(other.data(), 15) == 15);
  ASSERT_TRUE(b.Fill(other.data(), 17) == 17);
  for (uint32_t i = 0; i < 32 * channels; i++) {
    other[i] = 0.0;
  }

  // Empty it in the AudioCallbackBufferWrapper
  ASSERT_TRUE(b.Empty(mBuffer) == 32);

  // Check available return something reasonnable
  ASSERT_TRUE(mBuffer.Available() == FRAMES - 32);

  // Fill the buffer with the rest of the data
  mBuffer.WriteFrames(other.data() + 32 * channels, FRAMES - 32);

  // Check the buffer is now full
  ASSERT_TRUE(mBuffer.Available() == 0);

  for (uint32_t i = 0; i < samples; i++) {
    ASSERT_TRUE(fromCallback[i] == 1.0)
    << "Difference at " << i << " (" << fromCallback[i] << " != " << 1.0
    << ")\n";
  }

  ASSERT_TRUE(b.Fill(other.data(), FRAMES) == 128);
  ASSERT_TRUE(b.Fill(other.data(), FRAMES) == 0);
  ASSERT_TRUE(b.Empty(mBuffer) == 0);
}

TEST(AudioBuffers, Test)
{
  for (uint32_t ch = 1; ch <= 8; ++ch) {
    test_for_number_of_channels(ch);
  }
}
