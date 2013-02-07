/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioSegment.h"

#include "AudioStream.h"
#include "AudioChannelFormat.h"

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

static const int AUDIO_PROCESSING_FRAMES = 640; /* > 10ms of 48KHz audio */
static const uint8_t gZeroChannel[MAX_AUDIO_SAMPLE_SIZE*AUDIO_PROCESSING_FRAMES] = {0};

void
AudioSegment::WriteTo(AudioStream* aOutput)
{
  uint32_t outputChannels = aOutput->GetChannels();
  nsAutoTArray<AudioDataValue,AUDIO_PROCESSING_FRAMES*GUESS_AUDIO_CHANNELS> buf;
  nsAutoTArray<const void*,GUESS_AUDIO_CHANNELS> channelData;
  nsAutoTArray<float,AUDIO_PROCESSING_FRAMES*GUESS_AUDIO_CHANNELS> downmixConversionBuffer;
  nsAutoTArray<float,AUDIO_PROCESSING_FRAMES*GUESS_AUDIO_CHANNELS> downmixOutputBuffer;

  for (ChunkIterator ci(*this); !ci.IsEnded(); ci.Next()) {
    AudioChunk& c = *ci;
    TrackTicks offset = 0;
    while (offset < c.mDuration) {
      TrackTicks durationTicks =
        std::min<TrackTicks>(c.mDuration - offset, AUDIO_PROCESSING_FRAMES);
      if (uint64_t(outputChannels)*durationTicks > INT32_MAX || offset > INT32_MAX) {
        NS_ERROR("Buffer overflow");
        return;
      }
      uint32_t duration = uint32_t(durationTicks);
      buf.SetLength(outputChannels*duration);
      if (c.mBuffer) {
        channelData.SetLength(c.mChannelData.Length());
        for (uint32_t i = 0; i < channelData.Length(); ++i) {
          channelData[i] =
            AddAudioSampleOffset(c.mChannelData[i], c.mBufferFormat, int32_t(offset));
        }

        if (channelData.Length() < outputChannels) {
          // Up-mix. Note that this might actually make channelData have more
          // than outputChannels temporarily.
          AudioChannelsUpMix(&channelData, outputChannels, gZeroChannel);
        }

        if (channelData.Length() > outputChannels) {
          // Down-mix.
          if (c.mBufferFormat != AUDIO_FORMAT_FLOAT32) {
            NS_ASSERTION(c.mBufferFormat == AUDIO_FORMAT_S16, "unknown format");
            downmixConversionBuffer.SetLength(duration*channelData.Length());
            for (uint32_t i = 0; i < channelData.Length(); ++i) {
              float* conversionBuf = downmixConversionBuffer.Elements() + (i*duration);
              const int16_t* sourceBuf = static_cast<const int16_t*>(channelData[i]);
              for (uint32_t j = 0; j < duration; ++j) {
                conversionBuf[j] = AudioSampleToFloat(sourceBuf[j]);
              }
              channelData[i] = conversionBuf;
            }
          }

          downmixOutputBuffer.SetLength(duration*outputChannels);
          nsAutoTArray<float*,GUESS_AUDIO_CHANNELS> outputChannelBuffers;
          nsAutoTArray<const void*,GUESS_AUDIO_CHANNELS> outputChannelData;
          outputChannelBuffers.SetLength(outputChannels);
          outputChannelData.SetLength(outputChannels);
          for (uint32_t i = 0; i < outputChannels; ++i) {
            outputChannelData[i] = outputChannelBuffers[i] =
                downmixOutputBuffer.Elements() + duration*i;
          }
          AudioChannelsDownMix(channelData, outputChannelBuffers.Elements(),
                               outputChannels, duration);
          InterleaveAndConvertBuffer(outputChannelData.Elements(), AUDIO_FORMAT_FLOAT32,
                                     duration, c.mVolume,
                                     outputChannels,
                                     buf.Elements());
        } else {
          InterleaveAndConvertBuffer(channelData.Elements(), c.mBufferFormat,
                                     duration, c.mVolume,
                                     outputChannels,
                                     buf.Elements());
        }
      } else {
        // Assumes that a bit pattern of zeroes == 0.0f
        memset(buf.Elements(), 0, buf.Length()*sizeof(AudioDataValue));
      }
      aOutput->Write(buf.Elements(), int32_t(duration));
      offset += duration;
    }
  }
  aOutput->Start();
}

}
