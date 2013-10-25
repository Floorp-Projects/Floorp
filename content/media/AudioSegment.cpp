/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioSegment.h"

#include "AudioStream.h"
#include "AudioChannelFormat.h"
#include "Latency.h"

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

void
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
DownmixAndInterleave(const nsTArray<const void*>& aChannelData,
                     AudioSampleFormat aSourceFormat, int32_t aDuration,
                     float aVolume, uint32_t aOutputChannels,
                     AudioDataValue* aOutput)
{
  nsAutoTArray<const void*,GUESS_AUDIO_CHANNELS> channelData;
  nsAutoTArray<float,AUDIO_PROCESSING_FRAMES*GUESS_AUDIO_CHANNELS> downmixConversionBuffer;
  nsAutoTArray<float,AUDIO_PROCESSING_FRAMES*GUESS_AUDIO_CHANNELS> downmixOutputBuffer;

  channelData.SetLength(aChannelData.Length());
  if (aSourceFormat != AUDIO_FORMAT_FLOAT32) {
    NS_ASSERTION(aSourceFormat == AUDIO_FORMAT_S16, "unknown format");
    downmixConversionBuffer.SetLength(aDuration*aChannelData.Length());
    for (uint32_t i = 0; i < aChannelData.Length(); ++i) {
      float* conversionBuf = downmixConversionBuffer.Elements() + (i*aDuration);
      const int16_t* sourceBuf = static_cast<const int16_t*>(aChannelData[i]);
      for (uint32_t j = 0; j < (uint32_t)aDuration; ++j) {
        conversionBuf[j] = AudioSampleToFloat(sourceBuf[j]);
      }
      channelData[i] = conversionBuf;
    }
  } else {
    for (uint32_t i = 0; i < aChannelData.Length(); ++i) {
      channelData[i] = aChannelData[i];
    }
  }

  downmixOutputBuffer.SetLength(aDuration*aOutputChannels);
  nsAutoTArray<float*,GUESS_AUDIO_CHANNELS> outputChannelBuffers;
  nsAutoTArray<const void*,GUESS_AUDIO_CHANNELS> outputChannelData;
  outputChannelBuffers.SetLength(aOutputChannels);
  outputChannelData.SetLength(aOutputChannels);
  for (uint32_t i = 0; i < (uint32_t)aOutputChannels; ++i) {
    outputChannelData[i] = outputChannelBuffers[i] =
        downmixOutputBuffer.Elements() + aDuration*i;
  }
  if (channelData.Length() > aOutputChannels) {
    AudioChannelsDownMix(channelData, outputChannelBuffers.Elements(),
                         aOutputChannels, aDuration);
  }
  InterleaveAndConvertBuffer(outputChannelData.Elements(), AUDIO_FORMAT_FLOAT32,
                             aDuration, aVolume, aOutputChannels, aOutput);
}

void
AudioSegment::WriteTo(uint64_t aID, AudioStream* aOutput)
{
  uint32_t outputChannels = aOutput->GetChannels();
  nsAutoTArray<AudioDataValue,AUDIO_PROCESSING_FRAMES*GUESS_AUDIO_CHANNELS> buf;
  nsAutoTArray<const void*,GUESS_AUDIO_CHANNELS> channelData;

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
          DownmixAndInterleave(channelData, c.mBufferFormat, duration,
                               c.mVolume, outputChannels, buf.Elements());
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
      aOutput->Write(buf.Elements(), int32_t(duration), &(c.mTimeStamp));
      if(!c.mTimeStamp.IsNull()) {
        TimeStamp now = TimeStamp::Now();
        // would be more efficient to c.mTimeStamp to ms on create time then pass here
        LogTime(AsyncLatencyLogger::AudioMediaStreamTrack, aID,
                (now - c.mTimeStamp).ToMilliseconds(), c.mTimeStamp);
      }
      offset += duration;
    }
  }
  aOutput->Start();
}

}
