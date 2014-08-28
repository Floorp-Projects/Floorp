/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioSegment.h"

#include "AudioStream.h"
#include "AudioMixer.h"
#include "AudioChannelFormat.h"
#include "Latency.h"
#include <speex/speex_resampler.h>

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
   case AUDIO_FORMAT_SILENCE:
    // nothing to do here.
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

void AudioSegment::ResampleChunks(SpeexResamplerState* aResampler, uint32_t aInRate, uint32_t aOutRate)
{
  if (mChunks.IsEmpty()) {
    return;
  }

  MOZ_ASSERT(aResampler || IsNull(), "We can only be here without a resampler if this segment is null.");

  AudioSampleFormat format = AUDIO_FORMAT_SILENCE;
  for (ChunkIterator ci(*this); !ci.IsEnded(); ci.Next()) {
    if (ci->mBufferFormat != AUDIO_FORMAT_SILENCE) {
      format = ci->mBufferFormat;
    }
  }

  switch (format) {
    // If the format is silence at this point, all the chunks are silent. The
    // actual function we use does not matter, it's just a matter of changing
    // the chunks duration.
    case AUDIO_FORMAT_SILENCE:
    case AUDIO_FORMAT_FLOAT32:
      Resample<float>(aResampler, aInRate, aOutRate);
    break;
    case AUDIO_FORMAT_S16:
      Resample<int16_t>(aResampler, aInRate, aOutRate);
    break;
    default:
      MOZ_ASSERT(false);
    break;
  }
}

void
AudioSegment::WriteTo(uint64_t aID, AudioMixer& aMixer, uint32_t aOutputChannels, uint32_t aSampleRate)
{
  nsAutoTArray<AudioDataValue,AUDIO_PROCESSING_FRAMES*GUESS_AUDIO_CHANNELS> buf;
  nsAutoTArray<const void*,GUESS_AUDIO_CHANNELS> channelData;
  // Offset in the buffer that will end up sent to the AudioStream, in samples.
  uint32_t offset = 0;

  if (!GetDuration()) {
    return;
  }

  uint32_t outBufferLength = GetDuration() * aOutputChannels;
  buf.SetLength(outBufferLength);


  for (ChunkIterator ci(*this); !ci.IsEnded(); ci.Next()) {
    AudioChunk& c = *ci;
    uint32_t frames = c.mDuration;

    // If we have written data in the past, or we have real (non-silent) data
    // to write, we can proceed. Otherwise, it means we just started the
    // AudioStream, and we don't have real data to write to it (just silence).
    // To avoid overbuffering in the AudioStream, we simply drop the silence,
    // here. The stream will underrun and output silence anyways.
    if (c.mBuffer && c.mBufferFormat != AUDIO_FORMAT_SILENCE) {
      channelData.SetLength(c.mChannelData.Length());
      for (uint32_t i = 0; i < channelData.Length(); ++i) {
        channelData[i] = c.mChannelData[i];
      }
      if (channelData.Length() < aOutputChannels) {
        // Up-mix. Note that this might actually make channelData have more
        // than aOutputChannels temporarily.
        AudioChannelsUpMix(&channelData, aOutputChannels, gZeroChannel);
      }
      if (channelData.Length() > aOutputChannels) {
        // Down-mix.
        DownmixAndInterleave(channelData, c.mBufferFormat, frames,
                             c.mVolume, aOutputChannels, buf.Elements() + offset);
      } else {
        InterleaveAndConvertBuffer(channelData.Elements(), c.mBufferFormat,
                                   frames, c.mVolume,
                                   aOutputChannels,
                                   buf.Elements() + offset);
      }
    } else {
      // Assumes that a bit pattern of zeroes == 0.0f
      memset(buf.Elements() + offset, 0, aOutputChannels * frames * sizeof(AudioDataValue));
    }

    offset += frames * aOutputChannels;

    if (!c.mTimeStamp.IsNull()) {
      TimeStamp now = TimeStamp::Now();
      // would be more efficient to c.mTimeStamp to ms on create time then pass here
      LogTime(AsyncLatencyLogger::AudioMediaStreamTrack, aID,
              (now - c.mTimeStamp).ToMilliseconds(), c.mTimeStamp);
    }
  }

  if (offset) {
    aMixer.Mix(buf.Elements(), aOutputChannels, offset / aOutputChannels, aSampleRate);
  }
}

}
