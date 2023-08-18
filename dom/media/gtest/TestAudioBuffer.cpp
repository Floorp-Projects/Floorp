/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaData.h"
#include "gtest/gtest.h"

using mozilla::AlignedFloatBuffer;
using mozilla::AudioDataValue;
using mozilla::FloatToAudioSample;
using mozilla::InflatableShortBuffer;

void FillSine(InflatableShortBuffer& aBuf, AlignedFloatBuffer& aFloatBuf) {
  // Write a constant-pitch sine wave in both the integer and float buffers.
  float phase = 0;
  float phaseIncrement = 2 * M_PI * 440. / 44100.f;
  for (uint32_t i = 0; i < aBuf.Length(); i++) {
    aBuf.get()[i] = FloatToAudioSample<int16_t>(sin(phase));
    aFloatBuf.get()[i] = sin(phase);
    phase += phaseIncrement;
    if (phase >= 2 * M_PI) {
      phase -= 2 * M_PI;
    }
  }
}

TEST(InflatableAudioBuffer, Test)
{
  for (uint32_t i = 1; i < 10000; i++) {
    InflatableShortBuffer buf(i);
    AlignedFloatBuffer bufFloat(i);
    FillSine(buf, bufFloat);
    AlignedFloatBuffer inflated = buf.Inflate();
    for (uint32_t j = 0; j < buf.Length(); j++) {
      // Accept a very small difference because floats are floored in the
      // conversion to integer.
      if (std::abs(bufFloat.get()[j] - inflated.get()[j]) * 32767. > 1.0) {
        fprintf(stderr, "%f != %f (size: %u, index: %u)\n", bufFloat.get()[j],
                inflated.get()[j], i, j);
        ASSERT_TRUE(false);
      }
    }
  }

}  // namespace audio_mixer
