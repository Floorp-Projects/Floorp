/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioMixer.h"
#include "gtest/gtest.h"

using mozilla::AudioDataValue;
using mozilla::AudioSampleFormat;

namespace audio_mixer {

struct MixerConsumer : public mozilla::MixerCallbackReceiver {
  /* In this test, the different audio stream and channels are always created to
   * cancel each other. */
  void MixerCallback(mozilla::AudioChunk* aMixedBuffer, uint32_t aSampleRate) {
    bool silent = true;
    ASSERT_EQ(aMixedBuffer->mBufferFormat, mozilla::AUDIO_FORMAT_FLOAT32);
    for (uint32_t c = 0; c < aMixedBuffer->ChannelCount(); c++) {
      const float* channelData = aMixedBuffer->ChannelData<AudioDataValue>()[c];
      for (uint32_t i = 0; i < aMixedBuffer->mDuration; i++) {
        if (channelData[i] != 0.0) {
          fprintf(stderr, "Sample at %d in channel %c is not silent: %f\n", i,
                  c, channelData[i]);
          silent = false;
        }
      }
    }
    ASSERT_TRUE(silent);
  }
};

/* Helper function to give us the maximum and minimum value that don't clip,
 * for a given sample format (integer or floating-point). */
template <typename T>
T GetLowValue();

template <typename T>
T GetHighValue();

template <>
float GetLowValue<float>() {
  return -1.0;
}

template <>
short GetLowValue<short>() {
  return -INT16_MAX;
}

template <>
float GetHighValue<float>() {
  return 1.0;
}

template <>
short GetHighValue<short>() {
  return INT16_MAX;
}

void FillBuffer(AudioDataValue* aBuffer, uint32_t aLength,
                AudioDataValue aValue) {
  AudioDataValue* end = aBuffer + aLength;
  while (aBuffer != end) {
    *aBuffer++ = aValue;
  }
}

TEST(AudioMixer, Test)
{
  const uint32_t CHANNEL_LENGTH = 256;
  const uint32_t AUDIO_RATE = 44100;
  MixerConsumer consumer;
  AudioDataValue a[CHANNEL_LENGTH * 2];
  AudioDataValue b[CHANNEL_LENGTH * 2];
  FillBuffer(a, CHANNEL_LENGTH, GetLowValue<AudioDataValue>());
  FillBuffer(a + CHANNEL_LENGTH, CHANNEL_LENGTH,
             GetHighValue<AudioDataValue>());
  FillBuffer(b, CHANNEL_LENGTH, GetHighValue<AudioDataValue>());
  FillBuffer(b + CHANNEL_LENGTH, CHANNEL_LENGTH, GetLowValue<AudioDataValue>());

  {
    int iterations = 2;
    mozilla::AudioMixer mixer;

    fprintf(stderr, "Test AudioMixer constant buffer length.\n");

    while (iterations--) {
      mixer.StartMixing();
      mixer.Mix(a, 2, CHANNEL_LENGTH, AUDIO_RATE);
      mixer.Mix(b, 2, CHANNEL_LENGTH, AUDIO_RATE);
      consumer.MixerCallback(mixer.MixedChunk(), AUDIO_RATE);
    }
  }

  {
    mozilla::AudioMixer mixer;

    fprintf(stderr, "Test AudioMixer variable buffer length.\n");

    FillBuffer(a, CHANNEL_LENGTH / 2, GetLowValue<AudioDataValue>());
    FillBuffer(a + CHANNEL_LENGTH / 2, CHANNEL_LENGTH / 2,
               GetLowValue<AudioDataValue>());
    FillBuffer(b, CHANNEL_LENGTH / 2, GetHighValue<AudioDataValue>());
    FillBuffer(b + CHANNEL_LENGTH / 2, CHANNEL_LENGTH / 2,
               GetHighValue<AudioDataValue>());
    mixer.StartMixing();
    mixer.Mix(a, 2, CHANNEL_LENGTH / 2, AUDIO_RATE);
    mixer.Mix(b, 2, CHANNEL_LENGTH / 2, AUDIO_RATE);
    consumer.MixerCallback(mixer.MixedChunk(), AUDIO_RATE);
    FillBuffer(a, CHANNEL_LENGTH, GetLowValue<AudioDataValue>());
    FillBuffer(a + CHANNEL_LENGTH, CHANNEL_LENGTH,
               GetHighValue<AudioDataValue>());
    FillBuffer(b, CHANNEL_LENGTH, GetHighValue<AudioDataValue>());
    FillBuffer(b + CHANNEL_LENGTH, CHANNEL_LENGTH,
               GetLowValue<AudioDataValue>());
    mixer.StartMixing();
    mixer.Mix(a, 2, CHANNEL_LENGTH, AUDIO_RATE);
    mixer.Mix(b, 2, CHANNEL_LENGTH, AUDIO_RATE);
    consumer.MixerCallback(mixer.MixedChunk(), AUDIO_RATE);
    FillBuffer(a, CHANNEL_LENGTH / 2, GetLowValue<AudioDataValue>());
    FillBuffer(a + CHANNEL_LENGTH / 2, CHANNEL_LENGTH / 2,
               GetLowValue<AudioDataValue>());
    FillBuffer(b, CHANNEL_LENGTH / 2, GetHighValue<AudioDataValue>());
    FillBuffer(b + CHANNEL_LENGTH / 2, CHANNEL_LENGTH / 2,
               GetHighValue<AudioDataValue>());
    mixer.StartMixing();
    mixer.Mix(a, 2, CHANNEL_LENGTH / 2, AUDIO_RATE);
    mixer.Mix(b, 2, CHANNEL_LENGTH / 2, AUDIO_RATE);
    consumer.MixerCallback(mixer.MixedChunk(), AUDIO_RATE);
  }

  FillBuffer(a, CHANNEL_LENGTH, GetLowValue<AudioDataValue>());
  FillBuffer(b, CHANNEL_LENGTH, GetHighValue<AudioDataValue>());

  {
    mozilla::AudioMixer mixer;

    fprintf(stderr, "Test AudioMixer variable channel count.\n");

    mixer.StartMixing();
    mixer.Mix(a, 1, CHANNEL_LENGTH, AUDIO_RATE);
    mixer.Mix(b, 1, CHANNEL_LENGTH, AUDIO_RATE);
    consumer.MixerCallback(mixer.MixedChunk(), AUDIO_RATE);
    mixer.StartMixing();
    mixer.Mix(a, 1, CHANNEL_LENGTH, AUDIO_RATE);
    mixer.Mix(b, 1, CHANNEL_LENGTH, AUDIO_RATE);
    consumer.MixerCallback(mixer.MixedChunk(), AUDIO_RATE);
    mixer.StartMixing();
    mixer.Mix(a, 1, CHANNEL_LENGTH, AUDIO_RATE);
    mixer.Mix(b, 1, CHANNEL_LENGTH, AUDIO_RATE);
    consumer.MixerCallback(mixer.MixedChunk(), AUDIO_RATE);
  }

  {
    mozilla::AudioMixer mixer;
    fprintf(stderr, "Test AudioMixer variable stream count.\n");

    mixer.StartMixing();
    mixer.Mix(a, 2, CHANNEL_LENGTH, AUDIO_RATE);
    mixer.Mix(b, 2, CHANNEL_LENGTH, AUDIO_RATE);
    consumer.MixerCallback(mixer.MixedChunk(), AUDIO_RATE);
    mixer.StartMixing();
    mixer.Mix(a, 2, CHANNEL_LENGTH, AUDIO_RATE);
    mixer.Mix(b, 2, CHANNEL_LENGTH, AUDIO_RATE);
    mixer.Mix(a, 2, CHANNEL_LENGTH, AUDIO_RATE);
    mixer.Mix(b, 2, CHANNEL_LENGTH, AUDIO_RATE);
    consumer.MixerCallback(mixer.MixedChunk(), AUDIO_RATE);
    mixer.StartMixing();
    mixer.Mix(a, 2, CHANNEL_LENGTH, AUDIO_RATE);
    mixer.Mix(b, 2, CHANNEL_LENGTH, AUDIO_RATE);
    consumer.MixerCallback(mixer.MixedChunk(), AUDIO_RATE);
  }
}

}  // namespace audio_mixer
