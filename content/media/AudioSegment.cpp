/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioSegment.h"

#include "AudioStream.h"

namespace mozilla {

template <class SrcT, class DestT>
static void
InterleaveAndConvertBuffer(const SrcT** aSourceChannels,
                           int32_t aLength, float aVolume,
                           int32_t aChannels,
                           DestT* aOutput)
{
  DestT* output = aOutput;
  for (int32_t i = 0; i < aLength; ++i) {
    for (int32_t channel = 0; channel < aChannels; ++channel) {
      float v = AudioSampleToFloat(aSourceChannels[channel][i])*aVolume;
      *output = FloatToAudioSample<DestT>(v);
      ++output;
    }
  }
}

static inline void
InterleaveAndConvertBuffer(const int16_t** aSourceChannels,
                           int32_t aLength, float aVolume,
                           int32_t aChannels,
                           int16_t* aOutput)
{
  int16_t* output = aOutput;
  if (0.0f <= aVolume && aVolume <= 1.0f) {
    int32_t scale = int32_t((1 << 16) * aVolume);
    for (int32_t i = 0; i < aLength; ++i) {
      for (int32_t channel = 0; channel < aChannels; ++channel) {
        int16_t s = aSourceChannels[channel][i];
        *output = int16_t((int32_t(s) * scale) >> 16);
        ++output;
      }
    }
    return;
  }

  for (int32_t i = 0; i < aLength; ++i) {
    for (int32_t channel = 0; channel < aChannels; ++channel) {
      float v = AudioSampleToFloat(aSourceChannels[channel][i])*aVolume;
      *output = FloatToAudioSample<int16_t>(v);
      ++output;
    }
  }
}

static void
InterleaveAndConvertBuffer(const void** aSourceChannels,
                           AudioSampleFormat aSourceFormat,
                           int32_t aLength, float aVolume,
                           int32_t aChannels,
                           AudioDataValue* aOutput)
{
  switch (aSourceFormat) {
  case AUDIO_FORMAT_FLOAT32:
    InterleaveAndConvertBuffer(reinterpret_cast<const float**>(aSourceChannels),
                               aLength,
                               aVolume,
                               aChannels,
                               aOutput);
    break;
  case AUDIO_FORMAT_S16:
    InterleaveAndConvertBuffer(reinterpret_cast<const int16_t**>(aSourceChannels),
                               aLength,
                               aVolume,
                               aChannels,
                               aOutput);
    break;
  }
}

void
AudioSegment::ApplyVolume(float aVolume)
{
  for (ChunkIterator ci(*this); !ci.IsEnded(); ci.Next()) {
    ci->mVolume *= aVolume;
  }
}

static const int STATIC_AUDIO_SAMPLES = 10000;

void
AudioSegment::WriteTo(AudioStream* aOutput)
{
  NS_ASSERTION(mChannels == aOutput->GetChannels(), "Wrong number of channels");
  nsAutoTArray<AudioDataValue,STATIC_AUDIO_SAMPLES> buf;
  for (ChunkIterator ci(*this); !ci.IsEnded(); ci.Next()) {
    AudioChunk& c = *ci;
    if (uint64_t(mChannels)*c.mDuration > INT32_MAX) {
      NS_ERROR("Buffer overflow");
      return;
    }
    buf.SetLength(int32_t(mChannels*c.mDuration));
    if (c.mBuffer) {
      InterleaveAndConvertBuffer(c.mChannelData.Elements(), c.mBufferFormat,
                                 int32_t(c.mDuration), c.mVolume,
                                 aOutput->GetChannels(),
                                 buf.Elements());
    } else {
      // Assumes that a bit pattern of zeroes == 0.0f
      memset(buf.Elements(), 0, buf.Length()*sizeof(AudioDataValue));
    }
    aOutput->Write(buf.Elements(), int32_t(c.mDuration));
  }
}

}
