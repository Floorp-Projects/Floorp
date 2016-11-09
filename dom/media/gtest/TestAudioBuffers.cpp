/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>
#include "AudioBufferUtils.h"
#include "gtest/gtest.h"

const uint32_t FRAMES = 256;
const uint32_t CHANNELS = 2;
const uint32_t SAMPLES = CHANNELS * FRAMES;

TEST(AudioBuffers, Test)
{
  mozilla::AudioCallbackBufferWrapper<float, CHANNELS> mBuffer;
  mozilla::SpillBuffer<float, 128, CHANNELS> b;
  float fromCallback[SAMPLES];
  float other[SAMPLES];

  for (uint32_t i = 0; i < SAMPLES; i++) {
    other[i] = 1.0;
    fromCallback[i] = 0.0;
  }

  // Set the buffer in the wrapper from the callback
  mBuffer.SetBuffer(fromCallback, FRAMES);

  // Fill the SpillBuffer with data.
  ASSERT_TRUE(b.Fill(other, 15) == 15);
  ASSERT_TRUE(b.Fill(other, 17) == 17);
  for (uint32_t i = 0; i < 32 * CHANNELS; i++) {
    other[i] = 0.0;
  }

  // Empty it in the AudioCallbackBufferWrapper
  ASSERT_TRUE(b.Empty(mBuffer) == 32);

  // Check available return something reasonnable
  ASSERT_TRUE(mBuffer.Available() == FRAMES - 32);

  // Fill the buffer with the rest of the data
  mBuffer.WriteFrames(other + 32 * CHANNELS, FRAMES - 32);

  // Check the buffer is now full
  ASSERT_TRUE(mBuffer.Available() == 0);

  for (uint32_t i = 0 ; i < SAMPLES; i++) {
    ASSERT_TRUE(fromCallback[i] == 1.0) <<
      "Difference at " << i << " (" << fromCallback[i] << " != " << 1.0 <<
      ")\n";
  }

  ASSERT_TRUE(b.Fill(other, FRAMES) == 128);
  ASSERT_TRUE(b.Fill(other, FRAMES) == 0);
  ASSERT_TRUE(b.Empty(mBuffer) == 0);
}
