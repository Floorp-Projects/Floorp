/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>
#include <math.h>
#include <memory>
#include "../AudioPacketizer.h"
#include "gtest/gtest.h"

using namespace mozilla;

template <typename T>
class AutoBuffer {
 public:
  explicit AutoBuffer(size_t aLength) { mStorage = new T[aLength]; }
  ~AutoBuffer() { delete[] mStorage; }
  T* Get() { return mStorage; }

 private:
  T* mStorage;
};

int16_t Sequence(int16_t* aBuffer, uint32_t aSize, uint32_t aStart = 0) {
  uint32_t i;
  for (i = 0; i < aSize; i++) {
    aBuffer[i] = aStart + i;
  }
  return aStart + i;
}

void IsSequence(std::unique_ptr<int16_t[]> aBuffer, uint32_t aSize,
                uint32_t aStart = 0) {
  for (uint32_t i = 0; i < aSize; i++) {
    ASSERT_TRUE(aBuffer[i] == static_cast<int64_t>(aStart + i))
    << "Buffer is not a sequence at offset " << i << std::endl;
  }
  // Buffer is a sequence.
}

void Zero(std::unique_ptr<int16_t[]> aBuffer, uint32_t aSize) {
  for (uint32_t i = 0; i < aSize; i++) {
    ASSERT_TRUE(aBuffer[i] == 0)
    << "Buffer is not null at offset " << i << std::endl;
  }
}

double sine(uint32_t aPhase) { return sin(aPhase * 2 * M_PI * 440 / 44100); }

TEST(AudioPacketizer, Test)
{
  for (int16_t channels = 1; channels < 2; channels++) {
    // Test that the packetizer returns zero on underrun
    {
      AudioPacketizer<int16_t, int16_t> ap(441, channels);
      for (int16_t i = 0; i < 10; i++) {
        std::unique_ptr<int16_t[]> out(ap.Output());
        Zero(std::move(out), 441);
      }
    }
    // Simple test, with input/output buffer size aligned on the packet size,
    // alternating Input and Output calls.
    {
      AudioPacketizer<int16_t, int16_t> ap(441, channels);
      int16_t seqEnd = 0;
      for (int16_t i = 0; i < 10; i++) {
        AutoBuffer<int16_t> b(441 * channels);
        int16_t prevEnd = seqEnd;
        seqEnd = Sequence(b.Get(), channels * 441, prevEnd);
        ap.Input(b.Get(), 441);
        std::unique_ptr<int16_t[]> out(ap.Output());
        IsSequence(std::move(out), 441 * channels, prevEnd);
      }
    }
    // Simple test, with input/output buffer size aligned on the packet size,
    // alternating two Input and Output calls.
    {
      AudioPacketizer<int16_t, int16_t> ap(441, channels);
      int16_t seqEnd = 0;
      for (int16_t i = 0; i < 10; i++) {
        AutoBuffer<int16_t> b(441 * channels);
        AutoBuffer<int16_t> b1(441 * channels);
        int16_t prevEnd0 = seqEnd;
        seqEnd = Sequence(b.Get(), 441 * channels, prevEnd0);
        int16_t prevEnd1 = seqEnd;
        seqEnd = Sequence(b1.Get(), 441 * channels, seqEnd);
        ap.Input(b.Get(), 441);
        ap.Input(b1.Get(), 441);
        std::unique_ptr<int16_t[]> out(ap.Output());
        std::unique_ptr<int16_t[]> out2(ap.Output());
        IsSequence(std::move(out), 441 * channels, prevEnd0);
        IsSequence(std::move(out2), 441 * channels, prevEnd1);
      }
    }
    // Input/output buffer size not aligned on the packet size,
    // alternating two Input and Output calls.
    {
      AudioPacketizer<int16_t, int16_t> ap(441, channels);
      int16_t prevEnd = 0;
      int16_t prevSeq = 0;
      for (int16_t i = 0; i < 10; i++) {
        AutoBuffer<int16_t> b(480 * channels);
        AutoBuffer<int16_t> b1(480 * channels);
        prevSeq = Sequence(b.Get(), 480 * channels, prevSeq);
        prevSeq = Sequence(b1.Get(), 480 * channels, prevSeq);
        ap.Input(b.Get(), 480);
        ap.Input(b1.Get(), 480);
        std::unique_ptr<int16_t[]> out(ap.Output());
        std::unique_ptr<int16_t[]> out2(ap.Output());
        IsSequence(std::move(out), 441 * channels, prevEnd);
        prevEnd += 441 * channels;
        IsSequence(std::move(out2), 441 * channels, prevEnd);
        prevEnd += 441 * channels;
      }
      printf("Available: %d\n", ap.PacketsAvailable());
    }

    // "Real-life" test case: streaming a sine wave through a packetizer, and
    // checking that we have the right output.
    // 128 is, for example, the size of a Web Audio API block, and 441 is the
    // size of a webrtc.org packet when the sample rate is 44100 (10ms)
    {
      AudioPacketizer<int16_t, int16_t> ap(441, channels);
      AutoBuffer<int16_t> b(128 * channels);
      uint32_t phase = 0;
      uint32_t outPhase = 0;
      for (int16_t i = 0; i < 1000; i++) {
        for (int32_t j = 0; j < 128; j++) {
          for (int32_t c = 0; c < channels; c++) {
            // int16_t sinewave at 440Hz/44100Hz sample rate
            b.Get()[j * channels + c] = (2 << 14) * sine(phase);
          }
          phase++;
        }
        ap.Input(b.Get(), 128);
        while (ap.PacketsAvailable()) {
          std::unique_ptr<int16_t[]> packet(ap.Output());
          for (uint32_t k = 0; k < ap.mPacketSize; k++) {
            for (int32_t c = 0; c < channels; c++) {
              ASSERT_TRUE(packet[k * channels + c] ==
                          static_cast<int16_t>(((2 << 14) * sine(outPhase))));
            }
            outPhase++;
          }
        }
      }
    }
    // Test that clearing the packetizer empties it and starts returning zeros.
    {
      AudioPacketizer<int16_t, int16_t> ap(441, channels);
      AutoBuffer<int16_t> b(440 * channels);
      Sequence(b.Get(), 440 * channels);
      ap.Input(b.Get(), 440);
      EXPECT_EQ(ap.FramesAvailable(), 440U);
      ap.Clear();
      EXPECT_EQ(ap.FramesAvailable(), 0U);
      EXPECT_TRUE(ap.Empty());
      std::unique_ptr<int16_t[]> out(ap.Output());
      Zero(std::move(out), 441);
    }
  }
}
