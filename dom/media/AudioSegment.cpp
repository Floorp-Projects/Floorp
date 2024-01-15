/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioSegment.h"
#include "AudioMixer.h"
#include "AudioChannelFormat.h"
#include "MediaTrackGraph.h"  // for nsAutoRefTraits<SpeexResamplerState>
#include <speex/speex_resampler.h>

namespace mozilla {

const uint8_t
    SilentChannel::gZeroChannel[MAX_AUDIO_SAMPLE_SIZE *
                                SilentChannel::AUDIO_PROCESSING_FRAMES] = {0};

template <>
const float* SilentChannel::ZeroChannel<float>() {
  return reinterpret_cast<const float*>(SilentChannel::gZeroChannel);
}

template <>
const int16_t* SilentChannel::ZeroChannel<int16_t>() {
  return reinterpret_cast<const int16_t*>(SilentChannel::gZeroChannel);
}

void AudioSegment::ApplyVolume(float aVolume) {
  for (ChunkIterator ci(*this); !ci.IsEnded(); ci.Next()) {
    ci->mVolume *= aVolume;
  }
}

template <typename T>
void AudioSegment::Resample(nsAutoRef<SpeexResamplerState>& aResampler,
                            uint32_t* aResamplerChannelCount, uint32_t aInRate,
                            uint32_t aOutRate) {
  mDuration = 0;

  for (ChunkIterator ci(*this); !ci.IsEnded(); ci.Next()) {
    AutoTArray<nsTArray<T>, GUESS_AUDIO_CHANNELS> output;
    AutoTArray<const T*, GUESS_AUDIO_CHANNELS> bufferPtrs;
    AudioChunk& c = *ci;
    // If this chunk is null, don't bother resampling, just alter its duration
    if (c.IsNull()) {
      c.mDuration = (c.mDuration * aOutRate) / aInRate;
      mDuration += c.mDuration;
      continue;
    }
    uint32_t channels = c.mChannelData.Length();
    // This might introduce a discontinuity, but a channel count change in the
    // middle of a stream is not that common. This also initializes the
    // resampler as late as possible.
    if (channels != *aResamplerChannelCount) {
      SpeexResamplerState* state =
          speex_resampler_init(channels, aInRate, aOutRate,
                               SPEEX_RESAMPLER_QUALITY_DEFAULT, nullptr);
      MOZ_ASSERT(state);
      aResampler.own(state);
      *aResamplerChannelCount = channels;
    }
    output.SetLength(channels);
    bufferPtrs.SetLength(channels);
    uint32_t inFrames = c.mDuration;
    // Round up to allocate; the last frame may not be used.
    NS_ASSERTION((UINT64_MAX - aInRate + 1) / c.mDuration >= aOutRate,
                 "Dropping samples");
    uint32_t outSize =
        (static_cast<uint64_t>(c.mDuration) * aOutRate + aInRate - 1) / aInRate;
    for (uint32_t i = 0; i < channels; i++) {
      T* out = output[i].AppendElements(outSize);
      uint32_t outFrames = outSize;

      const T* in = static_cast<const T*>(c.mChannelData[i]);
      dom::WebAudioUtils::SpeexResamplerProcess(aResampler.get(), i, in,
                                                &inFrames, out, &outFrames);
      MOZ_ASSERT(inFrames == c.mDuration);

      bufferPtrs[i] = out;
      output[i].SetLength(outFrames);
    }
    MOZ_ASSERT(channels > 0);
    c.mDuration = output[0].Length();
    c.mBuffer = new mozilla::SharedChannelArrayBuffer<T>(std::move(output));
    for (uint32_t i = 0; i < channels; i++) {
      c.mChannelData[i] = bufferPtrs[i];
    }
    mDuration += c.mDuration;
  }
}

void AudioSegment::ResampleChunks(nsAutoRef<SpeexResamplerState>& aResampler,
                                  uint32_t* aResamplerChannelCount,
                                  uint32_t aInRate, uint32_t aOutRate) {
  if (mChunks.IsEmpty()) {
    return;
  }

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
      Resample<float>(aResampler, aResamplerChannelCount, aInRate, aOutRate);
      break;
    case AUDIO_FORMAT_S16:
      Resample<int16_t>(aResampler, aResamplerChannelCount, aInRate, aOutRate);
      break;
    default:
      MOZ_ASSERT(false);
      break;
  }
}

size_t AudioSegment::WriteToInterleavedBuffer(nsTArray<AudioDataValue>& aBuffer,
                                              uint32_t aChannels) const {
  size_t offset = 0;
  if (GetDuration() <= 0) {
    MOZ_ASSERT(GetDuration() == 0);
    return offset;
  }

  // Calculate how many samples in this segment
  size_t frames = static_cast<size_t>(GetDuration());
  CheckedInt<size_t> samples(frames);
  samples *= static_cast<size_t>(aChannels);
  MOZ_ASSERT(samples.isValid());
  if (!samples.isValid()) {
    return offset;
  }

  // Enlarge buffer space if needed
  if (samples.value() > aBuffer.Capacity()) {
    aBuffer.SetCapacity(samples.value());
  }
  aBuffer.SetLengthAndRetainStorage(samples.value());
  aBuffer.ClearAndRetainStorage();

  // Convert the de-interleaved chunks into an interleaved buffer. Note that
  // we may upmix or downmix the audio data if the channel in the chunks
  // mismatch with aChannels
  for (ConstChunkIterator ci(*this); !ci.IsEnded(); ci.Next()) {
    const AudioChunk& c = *ci;
    size_t samplesInChunk = static_cast<size_t>(c.mDuration) * aChannels;
    switch (c.mBufferFormat) {
      case AUDIO_FORMAT_S16:
        WriteChunk<int16_t>(c, aChannels, c.mVolume,
                            aBuffer.Elements() + offset);
        break;
      case AUDIO_FORMAT_FLOAT32:
        WriteChunk<float>(c, aChannels, c.mVolume, aBuffer.Elements() + offset);
        break;
      case AUDIO_FORMAT_SILENCE:
        PodZero(aBuffer.Elements() + offset, samplesInChunk);
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Unknown format");
        PodZero(aBuffer.Elements() + offset, samplesInChunk);
        break;
    }
    offset += samplesInChunk;
  }
  MOZ_DIAGNOSTIC_ASSERT(samples.value() == offset,
                        "Segment's duration is incorrect");
  aBuffer.SetLengthAndRetainStorage(offset);
  return offset;
}

// This helps to to safely get a pointer to the position we want to start
// writing a planar audio buffer, depending on the channel and the offset in the
// buffer.
static AudioDataValue* PointerForOffsetInChannel(AudioDataValue* aData,
                                                 size_t aLengthSamples,
                                                 uint32_t aChannelCount,
                                                 uint32_t aChannel,
                                                 uint32_t aOffsetSamples) {
  size_t samplesPerChannel = aLengthSamples / aChannelCount;
  size_t beginningOfChannel = samplesPerChannel * aChannel;
  MOZ_ASSERT(aChannel * samplesPerChannel + aOffsetSamples < aLengthSamples,
             "Offset request out of bounds.");
  return aData + beginningOfChannel + aOffsetSamples;
}

template <typename SrcT>
static void DownMixChunk(const AudioChunk& aChunk,
                         Span<AudioDataValue* const> aOutputChannels) {
  Span<const SrcT* const> channelData = aChunk.ChannelData<SrcT>();
  uint32_t frameCount = aChunk.mDuration;
  if (channelData.Length() > aOutputChannels.Length()) {
    // Down mix.
    AudioChannelsDownMix(channelData, aOutputChannels, frameCount);
    for (AudioDataValue* outChannel : aOutputChannels) {
      ScaleAudioSamples(outChannel, frameCount, aChunk.mVolume);
    }
  } else {
    // The channel count is already what we want.
    for (uint32_t channel = 0; channel < aOutputChannels.Length(); channel++) {
      ConvertAudioSamplesWithScale(channelData[channel],
                                   aOutputChannels[channel], frameCount,
                                   aChunk.mVolume);
    }
  }
}

void AudioChunk::DownMixTo(
    Span<AudioDataValue* const> aOutputChannelPtrs) const {
  switch (mBufferFormat) {
    case AUDIO_FORMAT_FLOAT32:
      DownMixChunk<float>(*this, aOutputChannelPtrs);
      return;
    case AUDIO_FORMAT_S16:
      DownMixChunk<int16_t>(*this, aOutputChannelPtrs);
      return;
    case AUDIO_FORMAT_SILENCE:
      for (AudioDataValue* outChannel : aOutputChannelPtrs) {
        std::fill_n(outChannel, mDuration, static_cast<AudioDataValue>(0));
      }
      return;
      // Avoid `default:` so that `-Wswitch` catches missing enumerators at
      // compile time.
  }
  MOZ_ASSERT_UNREACHABLE("buffer format");
}

void AudioSegment::Mix(AudioMixer& aMixer, uint32_t aOutputChannels,
                       uint32_t aSampleRate) {
  AutoTArray<AudioDataValue,
             SilentChannel::AUDIO_PROCESSING_FRAMES * GUESS_AUDIO_CHANNELS>
      buf;
  AudioChunk upMixChunk;
  uint32_t offsetSamples = 0;
  uint32_t duration = GetDuration();

  if (duration <= 0) {
    MOZ_ASSERT(duration == 0);
    return;
  }

  uint32_t outBufferLength = duration * aOutputChannels;
  buf.SetLength(outBufferLength);

  AutoTArray<AudioDataValue*, GUESS_AUDIO_CHANNELS> outChannelPtrs;
  outChannelPtrs.SetLength(aOutputChannels);

  uint32_t frames;
  for (ChunkIterator ci(*this); !ci.IsEnded();
       ci.Next(), offsetSamples += frames) {
    const AudioChunk& c = *ci;
    frames = c.mDuration;
    for (uint32_t channel = 0; channel < aOutputChannels; channel++) {
      outChannelPtrs[channel] =
          PointerForOffsetInChannel(buf.Elements(), outBufferLength,
                                    aOutputChannels, channel, offsetSamples);
    }

    // If the chunk is silent, simply write the right number of silence in the
    // buffers.
    if (c.mBufferFormat == AUDIO_FORMAT_SILENCE) {
      for (AudioDataValue* outChannel : outChannelPtrs) {
        PodZero(outChannel, frames);
      }
      continue;
    }
    // We need to upmix and downmix appropriately, depending on the
    // desired input and output channels.
    const AudioChunk* downMixInput = &c;
    if (c.ChannelCount() < aOutputChannels) {
      // Up-mix.
      upMixChunk = c;
      AudioChannelsUpMix<void>(&upMixChunk.mChannelData, aOutputChannels,
                               SilentChannel::gZeroChannel);
      downMixInput = &upMixChunk;
    }
    downMixInput->DownMixTo(outChannelPtrs);
  }

  if (offsetSamples) {
    MOZ_ASSERT(offsetSamples == outBufferLength / aOutputChannels,
               "We forgot to write some samples?");
    aMixer.Mix(buf.Elements(), aOutputChannels, offsetSamples, aSampleRate);
  }
}

}  // namespace mozilla
