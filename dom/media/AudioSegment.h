/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_AUDIOSEGMENT_H_
#define MOZILLA_AUDIOSEGMENT_H_

#include <speex/speex_resampler.h>
#include "MediaTrackGraph.h"
#include "MediaSegment.h"
#include "AudioSampleFormat.h"
#include "AudioChannelFormat.h"
#include "SharedBuffer.h"
#include "WebAudioUtils.h"
#include "nsAutoRef.h"
#ifdef MOZILLA_INTERNAL_API
#  include "mozilla/TimeStamp.h"
#endif
#include <float.h>

namespace mozilla {
struct AudioChunk;
class AudioSegment;
}  // namespace mozilla
MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR(mozilla::AudioChunk)

/**
 * This allows compilation of nsTArray<AudioSegment> and
 * AutoTArray<AudioSegment> since without it, static analysis fails on the
 * mChunks member being a non-memmovable AutoTArray.
 *
 * Note that AudioSegment(const AudioSegment&) is deleted, so this should
 * never come into effect.
 */
MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR(mozilla::AudioSegment)

namespace mozilla {

template <typename T>
class SharedChannelArrayBuffer : public ThreadSharedObject {
 public:
  explicit SharedChannelArrayBuffer(nsTArray<nsTArray<T> >* aBuffers) {
    mBuffers.SwapElements(*aBuffers);
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override {
    size_t amount = 0;
    amount += mBuffers.ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (size_t i = 0; i < mBuffers.Length(); i++) {
      amount += mBuffers[i].ShallowSizeOfExcludingThis(aMallocSizeOf);
    }

    return amount;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  nsTArray<nsTArray<T> > mBuffers;
};

class AudioMixer;

/**
 * For auto-arrays etc, guess this as the common number of channels.
 */
const int GUESS_AUDIO_CHANNELS = 2;

// We ensure that the graph advances in steps that are multiples of the Web
// Audio block size
const uint32_t WEBAUDIO_BLOCK_SIZE_BITS = 7;
const uint32_t WEBAUDIO_BLOCK_SIZE = 1 << WEBAUDIO_BLOCK_SIZE_BITS;

template <typename SrcT, typename DestT>
static void InterleaveAndConvertBuffer(const SrcT* const* aSourceChannels,
                                       uint32_t aLength, float aVolume,
                                       uint32_t aChannels, DestT* aOutput) {
  DestT* output = aOutput;
  for (size_t i = 0; i < aLength; ++i) {
    for (size_t channel = 0; channel < aChannels; ++channel) {
      float v = AudioSampleToFloat(aSourceChannels[channel][i]) * aVolume;
      *output = FloatToAudioSample<DestT>(v);
      ++output;
    }
  }
}

template <typename SrcT, typename DestT>
static void DeinterleaveAndConvertBuffer(const SrcT* aSourceBuffer,
                                         uint32_t aFrames, uint32_t aChannels,
                                         DestT** aOutput) {
  for (size_t i = 0; i < aChannels; i++) {
    size_t interleavedIndex = i;
    for (size_t j = 0; j < aFrames; j++) {
      ConvertAudioSample(aSourceBuffer[interleavedIndex], aOutput[i][j]);
      interleavedIndex += aChannels;
    }
  }
}

class SilentChannel {
 public:
  static const int AUDIO_PROCESSING_FRAMES = 640; /* > 10ms of 48KHz audio */
  static const uint8_t
      gZeroChannel[MAX_AUDIO_SAMPLE_SIZE * AUDIO_PROCESSING_FRAMES];
  // We take advantage of the fact that zero in float and zero in int have the
  // same all-zeros bit layout.
  template <typename T>
  static const T* ZeroChannel();
};

/**
 * Given an array of input channels (aChannelData), downmix to aOutputChannels,
 * interleave the channel data. A total of aOutputChannels*aDuration
 * interleaved samples will be copied to a channel buffer in aOutput.
 */
template <typename SrcT, typename DestT>
void DownmixAndInterleave(const nsTArray<const SrcT*>& aChannelData,
                          int32_t aDuration, float aVolume,
                          uint32_t aOutputChannels, DestT* aOutput) {
  if (aChannelData.Length() == aOutputChannels) {
    InterleaveAndConvertBuffer(aChannelData.Elements(), aDuration, aVolume,
                               aOutputChannels, aOutput);
  } else {
    AutoTArray<SrcT*, GUESS_AUDIO_CHANNELS> outputChannelData;
    AutoTArray<SrcT,
               SilentChannel::AUDIO_PROCESSING_FRAMES * GUESS_AUDIO_CHANNELS>
        outputBuffers;
    outputChannelData.SetLength(aOutputChannels);
    outputBuffers.SetLength(aDuration * aOutputChannels);
    for (uint32_t i = 0; i < aOutputChannels; i++) {
      outputChannelData[i] = outputBuffers.Elements() + aDuration * i;
    }
    AudioChannelsDownMix(aChannelData, outputChannelData.Elements(),
                         aOutputChannels, aDuration);
    InterleaveAndConvertBuffer(outputChannelData.Elements(), aDuration, aVolume,
                               aOutputChannels, aOutput);
  }
}

/**
 * An AudioChunk represents a multi-channel buffer of audio samples.
 * It references an underlying ThreadSharedObject which manages the lifetime
 * of the buffer. An AudioChunk maintains its own duration and channel data
 * pointers so it can represent a subinterval of a buffer without copying.
 * An AudioChunk can store its individual channels anywhere; it maintains
 * separate pointers to each channel's buffer.
 */
struct AudioChunk {
  typedef mozilla::AudioSampleFormat SampleFormat;

  // Generic methods
  void SliceTo(TrackTime aStart, TrackTime aEnd) {
    MOZ_ASSERT(aStart >= 0 && aStart < aEnd && aEnd <= mDuration,
               "Slice out of bounds");
    if (mBuffer) {
      MOZ_ASSERT(aStart < INT32_MAX,
                 "Can't slice beyond 32-bit sample lengths");
      for (uint32_t channel = 0; channel < mChannelData.Length(); ++channel) {
        mChannelData[channel] = AddAudioSampleOffset(
            mChannelData[channel], mBufferFormat, int32_t(aStart));
      }
    }
    mDuration = aEnd - aStart;
  }
  TrackTime GetDuration() const { return mDuration; }
  bool CanCombineWithFollowing(const AudioChunk& aOther) const {
    if (aOther.mBuffer != mBuffer) {
      return false;
    }
    if (!mBuffer) {
      return true;
    }
    if (aOther.mVolume != mVolume) {
      return false;
    }
    if (aOther.mPrincipalHandle != mPrincipalHandle) {
      return false;
    }
    NS_ASSERTION(aOther.mBufferFormat == mBufferFormat,
                 "Wrong metadata about buffer");
    NS_ASSERTION(aOther.mChannelData.Length() == mChannelData.Length(),
                 "Mismatched channel count");
    if (mDuration > INT32_MAX) {
      return false;
    }
    for (uint32_t channel = 0; channel < mChannelData.Length(); ++channel) {
      if (aOther.mChannelData[channel] !=
          AddAudioSampleOffset(mChannelData[channel], mBufferFormat,
                               int32_t(mDuration))) {
        return false;
      }
    }
    return true;
  }
  bool IsNull() const { return mBuffer == nullptr; }
  void SetNull(TrackTime aDuration) {
    mBuffer = nullptr;
    mChannelData.Clear();
    mDuration = aDuration;
    mVolume = 1.0f;
    mBufferFormat = AUDIO_FORMAT_SILENCE;
    mPrincipalHandle = PRINCIPAL_HANDLE_NONE;
  }

  uint32_t ChannelCount() const { return mChannelData.Length(); }

  bool IsMuted() const { return mVolume == 0.0f; }

  bool IsAudible() const {
    for (auto&& channel : mChannelData) {
      // Transform sound into dB RMS and assume that the value smaller than -100
      // is inaudible.
      float dbrms = 0.0;
      for (uint32_t idx = 0; idx < mDuration; idx++) {
        dbrms += std::pow(static_cast<const AudioDataValue*>(channel)[idx], 2);
      }
      dbrms /= mDuration;
      dbrms = std::sqrt(dbrms) != 0.0 ? 20 * log10(dbrms) : -1000.0;
      if (dbrms > -100.0) {
        return true;
      }
    }
    return false;
  }

  size_t SizeOfExcludingThisIfUnshared(MallocSizeOf aMallocSizeOf) const {
    return SizeOfExcludingThis(aMallocSizeOf, true);
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf, bool aUnshared) const {
    size_t amount = 0;

    // Possibly owned:
    // - mBuffer - Can hold data that is also in the decoded audio queue. If it
    //             is not shared, or unshared == false it gets counted.
    if (mBuffer && (!aUnshared || !mBuffer->IsShared())) {
      amount += mBuffer->SizeOfIncludingThis(aMallocSizeOf);
    }

    // Memory in the array is owned by mBuffer.
    amount += mChannelData.ShallowSizeOfExcludingThis(aMallocSizeOf);
    return amount;
  }

  template <typename T>
  const nsTArray<const T*>& ChannelData() const {
    MOZ_ASSERT(AudioSampleTypeToFormat<T>::Format == mBufferFormat);
    return *reinterpret_cast<const AutoTArray<const T*, GUESS_AUDIO_CHANNELS>*>(
        &mChannelData);
  }

  /**
   * ChannelFloatsForWrite() should be used only when mBuffer is owned solely
   * by the calling thread.
   */
  template <typename T>
  T* ChannelDataForWrite(size_t aChannel) {
    MOZ_ASSERT(AudioSampleTypeToFormat<T>::Format == mBufferFormat);
    MOZ_ASSERT(!mBuffer->IsShared());
    return static_cast<T*>(const_cast<void*>(mChannelData[aChannel]));
  }

  const PrincipalHandle& GetPrincipalHandle() const { return mPrincipalHandle; }

  TrackTime mDuration = 0;             // in frames within the buffer
  RefPtr<ThreadSharedObject> mBuffer;  // the buffer object whose lifetime is
                                       // managed; null means data is all zeroes
  // one pointer per channel; empty if and only if mBuffer is null
  CopyableAutoTArray<const void*, GUESS_AUDIO_CHANNELS> mChannelData;
  float mVolume = 1.0f;  // volume multiplier to apply
  // format of frames in mBuffer (or silence if mBuffer is null)
  SampleFormat mBufferFormat = AUDIO_FORMAT_SILENCE;
  // principalHandle for the data in this chunk.
  // This can be compared to an nsIPrincipal* when back on main thread.
  PrincipalHandle mPrincipalHandle = PRINCIPAL_HANDLE_NONE;
};

/**
 * A list of audio samples consisting of a sequence of slices of SharedBuffers.
 * The audio rate is determined by the track, not stored in this class.
 */
class AudioSegment : public MediaSegmentBase<AudioSegment, AudioChunk> {
 public:
  typedef mozilla::AudioSampleFormat SampleFormat;

  AudioSegment() : MediaSegmentBase<AudioSegment, AudioChunk>(AUDIO) {}

  AudioSegment(AudioSegment&& aSegment) = default;

  AudioSegment(const AudioSegment&) = delete;
  AudioSegment& operator=(const AudioSegment&) = delete;

  ~AudioSegment() = default;

  // Resample the whole segment in place.  `aResampler` is an instance of a
  // resampler, initialized with `aResamplerChannelCount` channels. If this
  // function finds a chunk with more channels, `aResampler` is destroyed and a
  // new resampler is created, and `aResamplerChannelCount` is updated with the
  // new channel count value.
  template <typename T>
  void Resample(nsAutoRef<SpeexResamplerState>& aResampler,
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
      NS_ASSERTION((UINT32_MAX - aInRate + 1) / c.mDuration >= aOutRate,
                   "Dropping samples");
      uint32_t outSize = (c.mDuration * aOutRate + aInRate - 1) / aInRate;
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
      c.mBuffer = new mozilla::SharedChannelArrayBuffer<T>(&output);
      for (uint32_t i = 0; i < channels; i++) {
        c.mChannelData[i] = bufferPtrs[i];
      }
      mDuration += c.mDuration;
    }
  }

  void ResampleChunks(nsAutoRef<SpeexResamplerState>& aResampler,
                      uint32_t* aResamplerChannelCount, uint32_t aInRate,
                      uint32_t aOutRate);
  void AppendFrames(already_AddRefed<ThreadSharedObject> aBuffer,
                    const nsTArray<const float*>& aChannelData,
                    int32_t aDuration,
                    const PrincipalHandle& aPrincipalHandle) {
    AudioChunk* chunk = AppendChunk(aDuration);
    chunk->mBuffer = aBuffer;

    MOZ_ASSERT(chunk->mBuffer || aChannelData.IsEmpty(),
               "Appending invalid data ?");

    for (uint32_t channel = 0; channel < aChannelData.Length(); ++channel) {
      chunk->mChannelData.AppendElement(aChannelData[channel]);
    }
    chunk->mBufferFormat = AUDIO_FORMAT_FLOAT32;
    chunk->mPrincipalHandle = aPrincipalHandle;
  }
  void AppendFrames(already_AddRefed<ThreadSharedObject> aBuffer,
                    const nsTArray<const int16_t*>& aChannelData,
                    int32_t aDuration,
                    const PrincipalHandle& aPrincipalHandle) {
    AudioChunk* chunk = AppendChunk(aDuration);
    chunk->mBuffer = aBuffer;

    MOZ_ASSERT(chunk->mBuffer || aChannelData.IsEmpty(),
               "Appending invalid data ?");

    for (uint32_t channel = 0; channel < aChannelData.Length(); ++channel) {
      chunk->mChannelData.AppendElement(aChannelData[channel]);
    }
    chunk->mBufferFormat = AUDIO_FORMAT_S16;
    chunk->mPrincipalHandle = aPrincipalHandle;
  }
  // Consumes aChunk, and returns a pointer to the persistent copy of aChunk
  // in the segment.
  AudioChunk* AppendAndConsumeChunk(AudioChunk* aChunk) {
    AudioChunk* chunk = AppendChunk(aChunk->mDuration);
    chunk->mBuffer = std::move(aChunk->mBuffer);
    chunk->mChannelData.SwapElements(aChunk->mChannelData);

    MOZ_ASSERT(chunk->mBuffer || aChunk->mChannelData.IsEmpty(),
               "Appending invalid data ?");

    chunk->mVolume = aChunk->mVolume;
    chunk->mBufferFormat = aChunk->mBufferFormat;
    chunk->mPrincipalHandle = aChunk->mPrincipalHandle;
    return chunk;
  }
  void ApplyVolume(float aVolume);
  // Mix the segment into a mixer, interleaved. This is useful to output a
  // segment to a system audio callback. It up or down mixes to aChannelCount
  // channels.
  void WriteTo(AudioMixer& aMixer, uint32_t aChannelCount,
               uint32_t aSampleRate);
  // Mix the segment into a mixer, keeping it planar, up or down mixing to
  // aChannelCount channels.
  void Mix(AudioMixer& aMixer, uint32_t aChannelCount, uint32_t aSampleRate);

  // Returns the maximum
  uint32_t MaxChannelCount() {
    // Find the first chunk that has non-zero channels. A chunk that hs zero
    // channels is just silence and we can simply discard it.
    uint32_t channelCount = 0;
    for (ChunkIterator ci(*this); !ci.IsEnded(); ci.Next()) {
      if (ci->ChannelCount()) {
        channelCount = std::max(channelCount, ci->ChannelCount());
      }
    }
    return channelCount;
  }

  static Type StaticType() { return AUDIO; }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }
};

template <typename SrcT>
void WriteChunk(AudioChunk& aChunk, uint32_t aOutputChannels,
                AudioDataValue* aOutputBuffer) {
  AutoTArray<const SrcT*, GUESS_AUDIO_CHANNELS> channelData;

  channelData = aChunk.ChannelData<SrcT>().Clone();

  if (channelData.Length() < aOutputChannels) {
    // Up-mix. Note that this might actually make channelData have more
    // than aOutputChannels temporarily.
    AudioChannelsUpMix(&channelData, aOutputChannels,
                       SilentChannel::ZeroChannel<SrcT>());
  }
  if (channelData.Length() > aOutputChannels) {
    // Down-mix.
    DownmixAndInterleave(channelData, aChunk.mDuration, aChunk.mVolume,
                         aOutputChannels, aOutputBuffer);
  } else {
    InterleaveAndConvertBuffer(channelData.Elements(), aChunk.mDuration,
                               aChunk.mVolume, aOutputChannels, aOutputBuffer);
  }
}

}  // namespace mozilla

#endif /* MOZILLA_AUDIOSEGMENT_H_ */
