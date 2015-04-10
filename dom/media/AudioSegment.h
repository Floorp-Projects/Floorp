/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_AUDIOSEGMENT_H_
#define MOZILLA_AUDIOSEGMENT_H_

#include "MediaSegment.h"
#include "AudioSampleFormat.h"
#include "SharedBuffer.h"
#include "WebAudioUtils.h"
#ifdef MOZILLA_INTERNAL_API
#include "mozilla/TimeStamp.h"
#endif

namespace mozilla {

template<typename T>
class SharedChannelArrayBuffer : public ThreadSharedObject {
public:
  explicit SharedChannelArrayBuffer(nsTArray<nsTArray<T> >* aBuffers)
  {
    mBuffers.SwapElements(*aBuffers);
  }

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    size_t amount = 0;
    amount += mBuffers.SizeOfExcludingThis(aMallocSizeOf);
    for (size_t i = 0; i < mBuffers.Length(); i++) {
      amount += mBuffers[i].SizeOfExcludingThis(aMallocSizeOf);
    }

    return amount;
  }

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  nsTArray<nsTArray<T> > mBuffers;
};

class AudioStream;
class AudioMixer;

/**
 * For auto-arrays etc, guess this as the common number of channels.
 */
const int GUESS_AUDIO_CHANNELS = 2;

// We ensure that the graph advances in steps that are multiples of the Web
// Audio block size
const uint32_t WEBAUDIO_BLOCK_SIZE_BITS = 7;
const uint32_t WEBAUDIO_BLOCK_SIZE = 1 << WEBAUDIO_BLOCK_SIZE_BITS;

void InterleaveAndConvertBuffer(const void** aSourceChannels,
                                AudioSampleFormat aSourceFormat,
                                int32_t aLength, float aVolume,
                                int32_t aChannels,
                                AudioDataValue* aOutput);

/**
 * Given an array of input channels (aChannelData), downmix to aOutputChannels,
 * interleave the channel data. A total of aOutputChannels*aDuration
 * interleaved samples will be copied to a channel buffer in aOutput.
 */
void DownmixAndInterleave(const nsTArray<const void*>& aChannelData,
                          AudioSampleFormat aSourceFormat, int32_t aDuration,
                          float aVolume, uint32_t aOutputChannels,
                          AudioDataValue* aOutput);

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
  void SliceTo(StreamTime aStart, StreamTime aEnd)
  {
    MOZ_ASSERT(aStart >= 0 && aStart < aEnd && aEnd <= mDuration,
               "Slice out of bounds");
    if (mBuffer) {
      MOZ_ASSERT(aStart < INT32_MAX, "Can't slice beyond 32-bit sample lengths");
      for (uint32_t channel = 0; channel < mChannelData.Length(); ++channel) {
        mChannelData[channel] = AddAudioSampleOffset(mChannelData[channel],
            mBufferFormat, int32_t(aStart));
      }
    }
    mDuration = aEnd - aStart;
  }
  StreamTime GetDuration() const { return mDuration; }
  bool CanCombineWithFollowing(const AudioChunk& aOther) const
  {
    if (aOther.mBuffer != mBuffer) {
      return false;
    }
    if (mBuffer) {
      NS_ASSERTION(aOther.mBufferFormat == mBufferFormat,
                   "Wrong metadata about buffer");
      NS_ASSERTION(aOther.mChannelData.Length() == mChannelData.Length(),
                   "Mismatched channel count");
      if (mDuration > INT32_MAX) {
        return false;
      }
      for (uint32_t channel = 0; channel < mChannelData.Length(); ++channel) {
        if (aOther.mChannelData[channel] != AddAudioSampleOffset(mChannelData[channel],
            mBufferFormat, int32_t(mDuration))) {
          return false;
        }
      }
    }
    return true;
  }
  bool IsNull() const { return mBuffer == nullptr; }
  void SetNull(StreamTime aDuration)
  {
    mBuffer = nullptr;
    mChannelData.Clear();
    mDuration = aDuration;
    mVolume = 1.0f;
    mBufferFormat = AUDIO_FORMAT_SILENCE;
  }
  int ChannelCount() const { return mChannelData.Length(); }

  bool IsMuted() const { return mVolume == 0.0f; }

  size_t SizeOfExcludingThisIfUnshared(MallocSizeOf aMallocSizeOf) const
  {
    return SizeOfExcludingThis(aMallocSizeOf, true);
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf, bool aUnshared) const
  {
    size_t amount = 0;

    // Possibly owned:
    // - mBuffer - Can hold data that is also in the decoded audio queue. If it
    //             is not shared, or unshared == false it gets counted.
    if (mBuffer && (!aUnshared || !mBuffer->IsShared())) {
      amount += mBuffer->SizeOfIncludingThis(aMallocSizeOf);
    }

    // Memory in the array is owned by mBuffer.
    amount += mChannelData.SizeOfExcludingThis(aMallocSizeOf);
    return amount;
  }

  StreamTime mDuration; // in frames within the buffer
  nsRefPtr<ThreadSharedObject> mBuffer; // the buffer object whose lifetime is managed; null means data is all zeroes
  nsTArray<const void*> mChannelData; // one pointer per channel; empty if and only if mBuffer is null
  float mVolume; // volume multiplier to apply (1.0f if mBuffer is nonnull)
  SampleFormat mBufferFormat; // format of frames in mBuffer (only meaningful if mBuffer is nonnull)
#ifdef MOZILLA_INTERNAL_API
  mozilla::TimeStamp mTimeStamp;           // time at which this has been fetched from the MediaEngine
#endif
};

/**
 * A list of audio samples consisting of a sequence of slices of SharedBuffers.
 * The audio rate is determined by the track, not stored in this class.
 */
class AudioSegment : public MediaSegmentBase<AudioSegment, AudioChunk> {
public:
  typedef mozilla::AudioSampleFormat SampleFormat;

  AudioSegment() : MediaSegmentBase<AudioSegment, AudioChunk>(AUDIO) {}

  // Resample the whole segment in place.
  template<typename T>
  void Resample(SpeexResamplerState* aResampler, uint32_t aInRate, uint32_t aOutRate)
  {
    mDuration = 0;
#ifdef DEBUG
    uint32_t segmentChannelCount = ChannelCount();
#endif

    for (ChunkIterator ci(*this); !ci.IsEnded(); ci.Next()) {
      nsAutoTArray<nsTArray<T>, GUESS_AUDIO_CHANNELS> output;
      nsAutoTArray<const T*, GUESS_AUDIO_CHANNELS> bufferPtrs;
      AudioChunk& c = *ci;
      // If this chunk is null, don't bother resampling, just alter its duration
      if (c.IsNull()) {
        c.mDuration = (c.mDuration * aOutRate) / aInRate;
        mDuration += c.mDuration;
        continue;
      }
      uint32_t channels = c.mChannelData.Length();
      MOZ_ASSERT(channels == segmentChannelCount);
      output.SetLength(channels);
      bufferPtrs.SetLength(channels);
#if !defined(MOZILLA_XPCOMRT_API)
// FIXME Bug 1126414 - XPCOMRT does not support dom::WebAudioUtils::SpeexResamplerProcess
      uint32_t inFrames = c.mDuration;
#endif // !defined(MOZILLA_XPCOMRT_API)
      // Round up to allocate; the last frame may not be used.
      NS_ASSERTION((UINT32_MAX - aInRate + 1) / c.mDuration >= aOutRate,
                   "Dropping samples");
      uint32_t outSize = (c.mDuration * aOutRate + aInRate - 1) / aInRate;
      for (uint32_t i = 0; i < channels; i++) {
        T* out = output[i].AppendElements(outSize);
        uint32_t outFrames = outSize;

#if !defined(MOZILLA_XPCOMRT_API)
// FIXME Bug 1126414 - XPCOMRT does not support dom::WebAudioUtils::SpeexResamplerProcess
        const T* in = static_cast<const T*>(c.mChannelData[i]);
        dom::WebAudioUtils::SpeexResamplerProcess(aResampler, i,
                                                  in, &inFrames,
                                                  out, &outFrames);
        MOZ_ASSERT(inFrames == c.mDuration);
#endif // !defined(MOZILLA_XPCOMRT_API)

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

  void ResampleChunks(SpeexResamplerState* aResampler,
                      uint32_t aInRate,
                      uint32_t aOutRate);

  void AppendFrames(already_AddRefed<ThreadSharedObject> aBuffer,
                    const nsTArray<const float*>& aChannelData,
                    int32_t aDuration)
  {
    AudioChunk* chunk = AppendChunk(aDuration);
    chunk->mBuffer = aBuffer;
    for (uint32_t channel = 0; channel < aChannelData.Length(); ++channel) {
      chunk->mChannelData.AppendElement(aChannelData[channel]);
    }
    chunk->mVolume = 1.0f;
    chunk->mBufferFormat = AUDIO_FORMAT_FLOAT32;
#ifdef MOZILLA_INTERNAL_API
    chunk->mTimeStamp = TimeStamp::Now();
#endif
  }
  void AppendFrames(already_AddRefed<ThreadSharedObject> aBuffer,
                    const nsTArray<const int16_t*>& aChannelData,
                    int32_t aDuration)
  {
    AudioChunk* chunk = AppendChunk(aDuration);
    chunk->mBuffer = aBuffer;
    for (uint32_t channel = 0; channel < aChannelData.Length(); ++channel) {
      chunk->mChannelData.AppendElement(aChannelData[channel]);
    }
    chunk->mVolume = 1.0f;
    chunk->mBufferFormat = AUDIO_FORMAT_S16;
#ifdef MOZILLA_INTERNAL_API
    chunk->mTimeStamp = TimeStamp::Now();
#endif
  }
  // Consumes aChunk, and returns a pointer to the persistent copy of aChunk
  // in the segment.
  AudioChunk* AppendAndConsumeChunk(AudioChunk* aChunk)
  {
    AudioChunk* chunk = AppendChunk(aChunk->mDuration);
    chunk->mBuffer = aChunk->mBuffer.forget();
    chunk->mChannelData.SwapElements(aChunk->mChannelData);
    chunk->mVolume = aChunk->mVolume;
    chunk->mBufferFormat = aChunk->mBufferFormat;
#ifdef MOZILLA_INTERNAL_API
    chunk->mTimeStamp = TimeStamp::Now();
#endif
    return chunk;
  }
  void ApplyVolume(float aVolume);
  void WriteTo(uint64_t aID, AudioMixer& aMixer, uint32_t aChannelCount, uint32_t aSampleRate);

  int ChannelCount() {
    NS_WARN_IF_FALSE(!mChunks.IsEmpty(),
        "Cannot query channel count on a AudioSegment with no chunks.");
    // Find the first chunk that has non-zero channels. A chunk that hs zero
    // channels is just silence and we can simply discard it.
    for (ChunkIterator ci(*this); !ci.IsEnded(); ci.Next()) {
      if (ci->ChannelCount()) {
        return ci->ChannelCount();
      }
    }
    return 0;
  }

  bool IsNull() const {
    for (ChunkIterator ci(*const_cast<AudioSegment*>(this)); !ci.IsEnded();
         ci.Next()) {
      if (!ci->IsNull()) {
        return false;
      }
    }
    return true;
  }

  static Type StaticType() { return AUDIO; }

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }
};

}

#endif /* MOZILLA_AUDIOSEGMENT_H_ */
