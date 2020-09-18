/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DYNAMIC_RESAMPLER_H_
#define MOZILLA_DYNAMIC_RESAMPLER_H_

#include "AudioRingBuffer.h"
#include "AudioSegment.h"

#include <speex/speex_resampler.h>

namespace mozilla {

const uint32_t STEREO = 2;

/**
 * DynamicResampler allows updating on the fly the output sample rate and the
 * number of channels. In addition to that, it maintains an internal buffer for
 * the input data and allows pre-buffering as well. The Resample() method
 * strives to provide the requested number of output frames by using the input
 * data including any pre-buffering. If this is not possible then it will not
 * attempt to resample and it will return failure.
 *
 * Input data buffering makes use of the AudioRingBuffer. The capacity of the
 * buffer is 100ms of float audio and it is pre-allocated at the constructor.
 * No extra allocations take place when the input is appended. In addition to
 * that, due to special feature of AudioRingBuffer, no extra copies take place
 * when the input data is fed to the resampler.
 *
 * The sample format must be set before using any method. If the provided sample
 * format is of type short the pre-allocated capacity of the input buffer
 * becomes 200ms of short audio.
 *
 * The DynamicResampler is not thread-safe, so all the methods appart from the
 * constructor must be called on the same thread.
 */
class DynamicResampler final {
 public:
  /**
   * Provide the initial input and output rate and the amount of pre-buffering.
   * The channel count will be set to stereo. Memory allocation will take
   * place. The input buffer is non-interleaved.
   */
  DynamicResampler(uint32_t aInRate, uint32_t aOutRate,
                   uint32_t aPreBufferFrames = 0);
  ~DynamicResampler();

  /**
   * Set the sample format type to float or short.
   */
  void SetSampleFormat(AudioSampleFormat aFormat);
  uint32_t GetOutRate() const { return mOutRate; }
  uint32_t GetChannels() const { return mChannels; }

  /**
   * Append `aInFrames` number of frames from `aInBuffer` to the internal input
   * buffer. Memory copy/move takes place.
   */
  void AppendInput(const nsTArray<const float*>& aInBuffer, uint32_t aInFrames);
  void AppendInput(const nsTArray<const int16_t*>& aInBuffer,
                   uint32_t aInFrames);
  /**
   * Append `aInFrames` number of frames of silence to the internal input
   * buffer. Memory copy/move takes place.
   */
  void AppendInputSilence(const uint32_t aInFrames);
  /**
   * Return the number of frames stored in the internal input buffer.
   */
  uint32_t InFramesBuffered(uint32_t aChannelIndex) const;
  /**
   * Return the number of frames left to store in the internal input buffer.
   */
  uint32_t InFramesLeftToBuffer(uint32_t aChannelIndex) const;

  /*
   * Resampler as much frame is needed from the internal input buffer to the
   * `aOutBuffer` in order to provide all `aOutFrames` and return true. If there
   * not enough input frames to provide the requested output frames no
   * resampling is attempted and false is returned.
   */
  bool Resample(float* aOutBuffer, uint32_t* aOutFrames,
                uint32_t aChannelIndex);
  bool Resample(int16_t* aOutBuffer, uint32_t* aOutFrames,
                uint32_t aChannelIndex);

  /**
   * Update the output rate or/and the channel count. If a value is not updated
   * compared to the current one nothing happens. Changing the `aOutRate`
   * results in recalculation in the resampler. Changing `aChannels` results in
   * the reallocation of the internal input buffer with the exception of
   * changes between mono to stereo and vice versa where no reallocation takes
   * place. A stereo internal input buffer is always maintained even if the
   * sound is mono.
   */
  void UpdateResampler(uint32_t aOutRate, uint32_t aChannels);

  /**
   * Returns true if the resampler has enough input data to provide to the
   * output of the `Resample()` method `aOutFrames` number of frames. This is a
   * way to know in advance if the `Resampler` method will return true or false
   * given that nothing changes in between.
   */
  bool CanResample(uint32_t aOutFrames) const;

 private:
  template <typename T>
  void AppendInputInternal(const nsTArray<const T*>& aInBuffer,
                           uint32_t aInFrames) {
    MOZ_ASSERT(aInBuffer.Length() == (uint32_t)mChannels);
    for (uint32_t i = 0; i < mChannels; ++i) {
      PushInFrames(aInBuffer[i], aInFrames, i);
    }
  }

  void ResampleInternal(const float* aInBuffer, uint32_t* aInFrames,
                        float* aOutBuffer, uint32_t* aOutFrames,
                        uint32_t aChannelIndex);
  void ResampleInternal(const int16_t* aInBuffer, uint32_t* aInFrames,
                        int16_t* aOutBuffer, uint32_t* aOutFrames,
                        uint32_t aChannelIndex);

  template <typename T>
  bool ResampleInternal(T* aOutBuffer, uint32_t* aOutFrames,
                        uint32_t aChannelIndex) {
    MOZ_ASSERT(mInRate);
    MOZ_ASSERT(mOutRate);
    MOZ_ASSERT(mChannels);
    MOZ_ASSERT(aChannelIndex <= mChannels);
    MOZ_ASSERT(aChannelIndex <= mInternalInBuffer.Length());
    MOZ_ASSERT(aOutFrames);
    MOZ_ASSERT(*aOutFrames);

    // Not enough input, don't do anything
    if (!EnoughInFrames(*aOutFrames, aChannelIndex)) {
      *aOutFrames = 0;
      return false;
    }

    if (mInRate == mOutRate) {
      mInternalInBuffer[aChannelIndex].Read(Span(aOutBuffer, *aOutFrames));
      // Workaround to avoid discontinuity when the speex resampler operates
      // again. Feed it with the last 20 frames to warm up the internal memory
      // of the resampler and then skip memory equals to resampler's input
      // latency.
      mInputTail[aChannelIndex].StoreTail<T>(aOutBuffer, *aOutFrames);
      return true;
    }

    uint32_t totalOutFramesNeeded = *aOutFrames;

    mInternalInBuffer[aChannelIndex].ReadNoCopy(
        [this, &aOutBuffer, &totalOutFramesNeeded,
         aChannelIndex](const Span<const T>& aInBuffer) -> uint32_t {
          if (!totalOutFramesNeeded) {
            return 0;
          }
          uint32_t outFramesResampled = totalOutFramesNeeded;
          uint32_t inFrames = aInBuffer.Length();
          ResampleInternal(aInBuffer.data(), &inFrames, aOutBuffer,
                           &outFramesResampled, aChannelIndex);
          aOutBuffer += outFramesResampled;
          totalOutFramesNeeded -= outFramesResampled;
          mInputTail[aChannelIndex].StoreTail<T>(aInBuffer);
          return inFrames;
        });

    MOZ_ASSERT(totalOutFramesNeeded == 0);
    return true;
  }

  bool EnoughInFrames(uint32_t aOutFrames, uint32_t aChannelIndex) const;

  template <typename T>
  void PushInFrames(const T* aInBuffer, const uint32_t aInFrames,
                    uint32_t aChannelIndex) {
    MOZ_ASSERT(aInBuffer);
    MOZ_ASSERT(aInFrames);
    MOZ_ASSERT(mChannels);
    MOZ_ASSERT(aChannelIndex <= mChannels);
    MOZ_ASSERT(aChannelIndex <= mInternalInBuffer.Length());
    mInternalInBuffer[aChannelIndex].Write(Span(aInBuffer, aInFrames));
  }

  void WarmUpResampler(bool aSkipLatency);

 public:
  const uint32_t mInRate;
  const uint32_t mPreBufferFrames;

 private:
  uint32_t mChannels = 0;
  uint32_t mOutRate;

  AutoTArray<AudioRingBuffer, STEREO> mInternalInBuffer;

  SpeexResamplerState* mResampler = nullptr;
  AudioSampleFormat mSampleFormat = AUDIO_FORMAT_SILENCE;

  class TailBuffer {
   public:
    template <typename T>
    T* Buffer() {
      return reinterpret_cast<T*>(mBuffer);
    }
    /* Store the MAXSIZE last elements of the buffer. */
    template <typename T>
    void StoreTail(const Span<const T>& aInBuffer) {
      StoreTail(aInBuffer.data(), aInBuffer.size());
    }
    template <typename T>
    void StoreTail(const T* aInBuffer, uint32_t aInFrames) {
      if (aInFrames >= MAXSIZE) {
        PodCopy(Buffer<T>(), aInBuffer + aInFrames - MAXSIZE, MAXSIZE);
        mSize = MAXSIZE;
      } else {
        PodCopy(Buffer<T>(), aInBuffer, aInFrames);
        mSize = aInFrames;
      }
    }
    uint32_t Length() { return mSize; }
    static const uint32_t MAXSIZE = 20;

   private:
    float mBuffer[MAXSIZE] = {};
    uint32_t mSize = 0;
  };
  AutoTArray<TailBuffer, STEREO> mInputTail;
};

/**
 * AudioChunkList provides a way to have preallocated audio buffers in
 * AudioSegment. The idea is that the amount of  AudioChunks is created in
 * advance. Each AudioChunk is able to hold a specific amount of audio
 * (capacity). The total capacity of AudioChunkList is specified by the number
 * of AudioChunks. The important aspect of the AudioChunkList is that
 * preallocates everything and reuse the same chunks similar to a ring buffer.
 *
 * Why the whole AudioChunk is preallocated and not some raw memory buffer? This
 * is due to the limitations of MediaTrackGraph. The way that MTG works depends
 * on `AudioSegment`s to convey the actual audio data. An AudioSegment consists
 * of AudioChunks. The AudioChunk is built in a way, that owns and allocates the
 * audio buffers. Thus, since the use of AudioSegment is mandatory if the audio
 * data was in a different form, the only way to use it from the audio thread
 * would be to create the AudioChunk there. That would result in a copy
 * operation (not very important) and most of all an allocation of the audio
 * buffer in the audio thread. This happens in many places inside MTG it's a bad
 * practice, though, and it has been avoided due to the AudioChunkList.
 *
 * After construction the sample format must be set, when it is available. It
 * can be set in the audio thread. Before setting the sample format is not
 * possible to use any method of AudioChunkList.
 *
 * Every AudioChunk in the AudioChunkList is preallocated with a capacity of 128
 * frames of float audio. Nevertheless, the sample format is not available at
 * that point. Thus if the sample format is set to short, the capacity of each
 * chunk changes to 256 number of frames, and the total duration becomes twice
 * big. There are methods to get the chunk capacity and total capacity in frames
 * and must always be used.
 *
 * Two things to note. First, when the channel count changes everything is
 * recreated which means reallocations. Second, the total capacity might differs
 * from the requested total capacity for two reasons. First, if the sample
 * format is set to short and second because the number of chunks in the list
 * divides exactly the final total capacity. The corresponding method must
 * always be used to query the total capacity.
 */
class AudioChunkList {
 public:
  /**
   * Constructor, the final total duration might be different from the requested
   * `aTotalDuration`. Memory allocation takes place.
   */
  AudioChunkList(uint32_t aTotalDuration, uint32_t aChannels);
  AudioChunkList(const AudioChunkList&) = delete;
  AudioChunkList(AudioChunkList&&) = delete;
  ~AudioChunkList() = default;

  /**
   * Set sample format. It must be done before any other method being used.
   */
  void SetSampleFormat(AudioSampleFormat aFormat);
  /**
   * Get the next available AudioChunk. The duration of the chunk will be zero
   * and the volume 1.0. However, the buffers will be there ready to be written.
   * Please note, that a reference of the preallocated chunk is returned. Thus
   * it _must not be consumed_ directly. If the chunk needs to be consumed it
   * must be copied to a temporary chunk first. For example:
   * ```
   *   AudioChunk& chunk = audioChunklist.GetNext();
   *   // Set up the chunk
   *   AudioChunk tmp = chunk;
   *   audioSegment.AppendAndConsumeChunk(&tmp);
   * ```
   * This way no memory allocation or copy, takes place.
   */
  AudioChunk& GetNext();

  /**
   * Get the capacity of each individual AudioChunk in the list.
   */
  uint32_t ChunkCapacity() const {
    MOZ_ASSERT(mSampleFormat == AUDIO_FORMAT_S16 ||
               mSampleFormat == AUDIO_FORMAT_FLOAT32);
    return mChunkCapacity;
  }
  /**
   * Get the total capacity of AudioChunkList.
   */
  uint32_t TotalCapacity() const {
    MOZ_ASSERT(mSampleFormat == AUDIO_FORMAT_S16 ||
               mSampleFormat == AUDIO_FORMAT_FLOAT32);
    return CheckedInt<uint32_t>(mChunkCapacity * mChunks.Length()).value();
  }

  /**
   * Update the channel count of the AudioChunkList. Memory allocation is
   * taking place.
   */
  void Update(uint32_t aChannels);

 private:
  void IncrementIndex() {
    ++mIndex;
    mIndex = CheckedInt<uint32_t>(mIndex % mChunks.Length()).value();
  }
  void CreateChunks(uint32_t aNumOfChunks, uint32_t aChannels);
  void UpdateToMonoOrStereo(uint32_t aChannels);

 private:
  nsTArray<AudioChunk> mChunks;
  uint32_t mIndex = 0;
  uint32_t mChunkCapacity = 128;
  AudioSampleFormat mSampleFormat = AUDIO_FORMAT_SILENCE;
};

/**
 * Audio Resampler is a resampler able to change the output rate and channels
 * count on the fly. The API is simple and it is based in AudioSegment in order
 * to be used MTG. All memory allocations, for input and output buffers, happen
 * in the constructor and when channel count changes. The memory is recycled in
 * order to avoid reallocations. It also supports prebuffering of silence. It
 * consists of DynamicResampler and AudioChunkList so please read their
 * documentation if you are interested in more details.
 *
 * The output buffer is preallocated  and returned in the form of AudioSegment.
 * The intention is to be used directly in a MediaTrack. Since an AudioChunk
 * must no be "shared" in order to be written, the AudioSegment returned by
 * resampler method must be cleaned up in order to be able for the `AudioChunk`s
 * that it consists of to be reused. For `MediaTrack::mSegment` this happens
 * every ~50ms (look at MediaTrack::AdvanceTimeVaryingValuesToCurrentTime). Thus
 * memory capacity of 100ms has been preallocated for internal input and output
 * buffering.
 */
class AudioResampler final {
 public:
  AudioResampler(uint32_t aInRate, uint32_t aOutRate,
                 uint32_t aPreBufferFrames = 0);

  /**
   * Append input data into the resampler internal buffer. Copy/move of the
   * memory is taking place. Also, the channel count will change according to
   * the channel count of the chunks.
   */
  void AppendInput(const AudioSegment& aInSegment);
  /**
   * Get the number of frames that can be read from the internal input buffer
   * before it becomes empty.
   */
  uint32_t InputReadableFrames() const;
  /**
   * Get the number of frames that can be written to the internal input buffer
   * before it becomes full.
   */
  uint32_t InputWritableFrames() const;

  /*
   * Reguest `aOutFrames` of audio in the output sample rate. The internal
   * buffered input is used. If there is no enough input for that amount of
   * output and empty AudioSegment is returned
   */
  AudioSegment Resample(uint32_t aOutFrames);

  /*
   * Updates the output rate that will be used by the resampler.
   */
  void UpdateOutRate(uint32_t aOutRate) {
    Update(aOutRate, mResampler.GetChannels());
  }

 private:
  void UpdateChannels(uint32_t aChannels) {
    Update(mResampler.GetOutRate(), aChannels);
  }
  void Update(uint32_t aOutRate, uint32_t aChannels);

 private:
  DynamicResampler mResampler;
  AudioChunkList mOutputChunks;
  bool mIsSampleFormatSet = false;
};

}  // namespace mozilla

#endif  // MOZILLA_DYNAMIC_RESAMPLER_H_
