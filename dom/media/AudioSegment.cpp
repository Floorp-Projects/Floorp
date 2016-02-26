/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioSegment.h"

#include "AudioMixer.h"
#include "AudioChannelFormat.h"
#include "Latency.h"
#include <speex/speex_resampler.h>

namespace mozilla {

const uint8_t SilentChannel::gZeroChannel[MAX_AUDIO_SAMPLE_SIZE*SilentChannel::AUDIO_PROCESSING_FRAMES] = {0};

template<>
const float* SilentChannel::ZeroChannel<float>()
{
  return reinterpret_cast<const float*>(SilentChannel::gZeroChannel);
}

template<>
const int16_t* SilentChannel::ZeroChannel<int16_t>()
{
  return reinterpret_cast<const int16_t*>(SilentChannel::gZeroChannel);
}

void
AudioSegment::ApplyVolume(float aVolume)
{
  for (ChunkIterator ci(*this); !ci.IsEnded(); ci.Next()) {
    ci->mVolume *= aVolume;
  }
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

// This helps to to safely get a pointer to the position we want to start
// writing a planar audio buffer, depending on the channel and the offset in the
// buffer.
static AudioDataValue*
PointerForOffsetInChannel(AudioDataValue* aData, size_t aLengthSamples,
                          uint32_t aChannelCount, uint32_t aChannel,
                          uint32_t aOffsetSamples)
{
  size_t samplesPerChannel = aLengthSamples / aChannelCount;
  size_t beginningOfChannel = samplesPerChannel * aChannel;
  MOZ_ASSERT(aChannel * samplesPerChannel + aOffsetSamples < aLengthSamples,
             "Offset request out of bounds.");
  return aData + beginningOfChannel + aOffsetSamples;
}

void
AudioSegment::Mix(AudioMixer& aMixer, uint32_t aOutputChannels,
                  uint32_t aSampleRate)
{
  AutoTArray<AudioDataValue, SilentChannel::AUDIO_PROCESSING_FRAMES* GUESS_AUDIO_CHANNELS>
  buf;
  AutoTArray<const AudioDataValue*, GUESS_AUDIO_CHANNELS> channelData;
  uint32_t offsetSamples = 0;
  uint32_t duration = GetDuration();

  if (duration <= 0) {
    MOZ_ASSERT(duration == 0);
    return;
  }

  uint32_t outBufferLength = duration * aOutputChannels;
  buf.SetLength(outBufferLength);

  for (ChunkIterator ci(*this); !ci.IsEnded(); ci.Next()) {
    AudioChunk& c = *ci;
    uint32_t frames = c.mDuration;

    // If the chunk is silent, simply write the right number of silence in the
    // buffers.
    if (c.mBufferFormat == AUDIO_FORMAT_SILENCE) {
      for (uint32_t channel = 0; channel < aOutputChannels; channel++) {
        AudioDataValue* ptr =
          PointerForOffsetInChannel(buf.Elements(), outBufferLength,
                                    aOutputChannels, channel, offsetSamples);
        PodZero(ptr, frames);
      }
    } else {
      // Othewise, we need to upmix or downmix appropriately, depending on the
      // desired input and output channels.
      channelData.SetLength(c.mChannelData.Length());
      for (uint32_t i = 0; i < channelData.Length(); ++i) {
        channelData[i] = static_cast<const AudioDataValue*>(c.mChannelData[i]);
      }
      if (channelData.Length() < aOutputChannels) {
        // Up-mix.
        AudioChannelsUpMix(&channelData, aOutputChannels, SilentChannel::ZeroChannel<AudioDataValue>());
        for (uint32_t channel = 0; channel < aOutputChannels; channel++) {
          AudioDataValue* ptr =
            PointerForOffsetInChannel(buf.Elements(), outBufferLength,
                                      aOutputChannels, channel, offsetSamples);
          PodCopy(ptr, reinterpret_cast<const AudioDataValue*>(channelData[channel]),
                  frames);
        }
        MOZ_ASSERT(channelData.Length() == aOutputChannels);
      } else if (channelData.Length() > aOutputChannels) {
        // Down mix.
        AutoTArray<AudioDataValue*, GUESS_AUDIO_CHANNELS> outChannelPtrs;
        outChannelPtrs.SetLength(aOutputChannels);
        uint32_t offsetSamples = 0;
        for (uint32_t channel = 0; channel < aOutputChannels; channel++) {
          outChannelPtrs[channel] =
            PointerForOffsetInChannel(buf.Elements(), outBufferLength,
                                      aOutputChannels, channel, offsetSamples);
        }
        AudioChannelsDownMix(channelData, outChannelPtrs.Elements(),
                             aOutputChannels, frames);
      } else {
        // The channel count is already what we want, just copy it over.
        for (uint32_t channel = 0; channel < aOutputChannels; channel++) {
          AudioDataValue* ptr =
            PointerForOffsetInChannel(buf.Elements(), outBufferLength,
                                      aOutputChannels, channel, offsetSamples);
          PodCopy(ptr, reinterpret_cast<const AudioDataValue*>(channelData[channel]),
                  frames);
        }
      }
    }
    offsetSamples += frames;
  }

  if (offsetSamples) {
    MOZ_ASSERT(offsetSamples == outBufferLength / aOutputChannels,
               "We forgot to write some samples?");
    aMixer.Mix(buf.Elements(), aOutputChannels, offsetSamples, aSampleRate);
  }
}

void
AudioSegment::WriteTo(uint64_t aID, AudioMixer& aMixer, uint32_t aOutputChannels, uint32_t aSampleRate)
{
  AutoTArray<AudioDataValue,SilentChannel::AUDIO_PROCESSING_FRAMES*GUESS_AUDIO_CHANNELS> buf;
  // Offset in the buffer that will be written to the mixer, in samples.
  uint32_t offset = 0;

  if (GetDuration() <= 0) {
    MOZ_ASSERT(GetDuration() == 0);
    return;
  }

  uint32_t outBufferLength = GetDuration() * aOutputChannels;
  buf.SetLength(outBufferLength);


  for (ChunkIterator ci(*this); !ci.IsEnded(); ci.Next()) {
    AudioChunk& c = *ci;

    switch (c.mBufferFormat) {
      case AUDIO_FORMAT_S16:
        WriteChunk<int16_t>(c, aOutputChannels, buf.Elements() + offset);
        break;
      case AUDIO_FORMAT_FLOAT32:
        WriteChunk<float>(c, aOutputChannels, buf.Elements() + offset);
        break;
      case AUDIO_FORMAT_SILENCE:
        // The mixer is expecting interleaved data, so this is ok.
        PodZero(buf.Elements() + offset, c.mDuration * aOutputChannels);
        break;
      default:
        MOZ_ASSERT(false, "Not handled");
    }

    offset += c.mDuration * aOutputChannels;

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

} // namespace mozilla
