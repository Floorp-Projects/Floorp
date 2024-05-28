/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_DRIFTCONTROL_DYNAMICRESAMPLER_H_
#define DOM_MEDIA_DRIFTCONTROL_DYNAMICRESAMPLER_H_

#include "AudioRingBuffer.h"
#include "AudioSegment.h"
#include "TimeUnits.h"
#include "WavDumper.h"

#include <speex/speex_resampler.h>

namespace mozilla {

const uint32_t STEREO = 2;

/**
 * DynamicResampler allows updating on the fly the output sample rate and the
 * number of channels. In addition to that, it maintains an internal buffer for
 * the input data and allows pre-buffering as well. The Resample() method
 * strives to provide the requested number of output frames by using the input
 * data including any pre-buffering. If there are fewer frames in the internal
 * buffer than is requested, the internal buffer is padded with enough silence
 * to allow the requested to be resampled and returned.
 *
 * Input data buffering makes use of the AudioRingBuffer. The capacity of the
 * buffer is initially 100ms of audio and it is pre-allocated during
 * SetSampleFormat(). Should the input data grow beyond that, the input buffer
 * is re-allocated on the fly. In addition to that, due to special feature of
 * AudioRingBuffer, no extra copies take place when the input data is fed to the
 * resampler.
 *
 * The sample format must be set before using any method.
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
                   uint32_t aInputPreBufferFrameCount = 0);
  ~DynamicResampler();

  /**
   * Set the sample format type to float or short.
   */
  void SetSampleFormat(AudioSampleFormat aFormat);
  uint32_t GetInRate() const { return mInRate; }
  uint32_t GetChannels() const { return mChannels; }

  /**
   * Append `aInFrames` number of frames from `aInBuffer` to the internal input
   * buffer. Memory copy/move takes place.
   */
  void AppendInput(Span<const float* const> aInBuffer, uint32_t aInFrames);
  void AppendInput(Span<const int16_t* const> aInBuffer, uint32_t aInFrames);
  /**
   * Append `aInFrames` number of frames of silence to the internal input
   * buffer. Memory copy/move takes place.
   */
  void AppendInputSilence(const uint32_t aInFrames);
  /**
   * Return the number of frames the internal input buffer can store.
   */
  uint32_t InFramesBufferSize() const;
  /**
   * Return the number of frames stored in the internal input buffer.
   */
  uint32_t InFramesBuffered(uint32_t aChannelIndex) const;

  /**
   * Prepends existing input data with a silent pre-buffer if not already done.
   * Data will be prepended so that after resampling aDuration of data,
   * the buffering level will be as close as possible to
   * mInputPreBufferFrameCount, which is the desired buffering level.
   */
  void EnsurePreBuffer(media::TimeUnit aDuration);

  /**
   * Set the number of frames that should be used for input pre-buffering.
   */
  void SetInputPreBufferFrameCount(uint32_t aInputPreBufferFrameCount);

  /*
   * Resample as much frames as needed from the internal input buffer to the
   * `aOutBuffer` in order to provide all `aOutFrames`.
   *
   * On first call, prepends the input buffer with silence so that after
   * resampling aOutFrames frames of data, the input buffer holds data as close
   * as possible to the configured pre-buffer size.
   *
   * If there are not enough input frames to provide the requested output
   * frames, the input buffer is padded with enough silence to allow the
   * requested frames to be resampled, and the pre-buffer is reset so that the
   * next call will be treated as the first.
   *
   * Returns true if the internal input buffer underran and had to be padded
   * with silence, otherwise false.
   */
  bool Resample(float* aOutBuffer, uint32_t aOutFrames, uint32_t aChannelIndex);
  bool Resample(int16_t* aOutBuffer, uint32_t aOutFrames,
                uint32_t aChannelIndex);

  /**
   * Update the output rate or/and the channel count. If a value is not updated
   * compared to the current one nothing happens. Changing the `aInRate`
   * results in recalculation in the resampler. Changing `aChannels` results in
   * the reallocation of the internal input buffer with the exception of
   * changes between mono to stereo and vice versa where no reallocation takes
   * place. A stereo internal input buffer is always maintained even if the
   * sound is mono.
   */
  void UpdateResampler(uint32_t aInRate, uint32_t aChannels);

 private:
  template <typename T>
  void AppendInputInternal(Span<const T* const>& aInBuffer,
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
  bool ResampleInternal(T* aOutBuffer, uint32_t aOutFrames,
                        uint32_t aChannelIndex) {
    MOZ_ASSERT(mInRate);
    MOZ_ASSERT(mOutRate);
    MOZ_ASSERT(mChannels);
    MOZ_ASSERT(aChannelIndex < mChannels);
    MOZ_ASSERT(aChannelIndex < mInternalInBuffer.Length());
    MOZ_ASSERT(aOutFrames);

    uint32_t outFramesNeeded = aOutFrames;
    T* nextOutFrame = aOutBuffer;
    if (mInRate == mOutRate) {
      if (!mResamplerIsBypassed) {
        uint32_t latency = speex_resampler_get_input_latency(mResampler);
        mInternalInBuffer[aChannelIndex].ReadNoCopy(
            [&](const Span<const T>& aInBuffer) -> uint32_t {
              // Although unlikely with the sample rates used with this class,
              // the resampler input latency may temporarily be higher than
              // indicated, after a change in resampling rate that reduces the
              // indicated latency. The resampler's "magic" samples cause
              // this. All frames in the resampler are extracted when
              // `latency` output frames have been extracted.
              uint32_t outFramesResampled = std::min(outFramesNeeded, latency);
              uint32_t inFrames = aInBuffer.Length();
              ResampleInternal(aInBuffer.Elements(), &inFrames, nextOutFrame,
                               &outFramesResampled, aChannelIndex);
              nextOutFrame += outFramesResampled;
              outFramesNeeded -= outFramesResampled;
              if (outFramesResampled == latency) {
                mResamplerIsBypassed = true;
                // The last `latency` frames of input to the resampler will not
                // be extracted from the resampler. Leave them in
                // mInternalInBuffer to be copied directly to nextOutFrame.
                MOZ_ASSERT(inFrames >= latency);
                return inFrames - latency;
              }
              return inFrames;
            });
      }
      bool underrun = false;
      if (uint32_t buffered = mInternalInBuffer[aChannelIndex].AvailableRead();
          buffered < outFramesNeeded) {
        underrun = true;
        mIsPreBufferSet = false;
        mInternalInBuffer[aChannelIndex].WriteSilence(outFramesNeeded -
                                                      buffered);
      }
      DebugOnly<uint32_t> numFramesRead = mInternalInBuffer[aChannelIndex].Read(
          Span(nextOutFrame, outFramesNeeded));
      MOZ_ASSERT(numFramesRead == outFramesNeeded);
      // Workaround to avoid discontinuity when the speex resampler operates
      // again. Feed it with the last 20 frames to warm up the internal memory
      // of the resampler and then skip memory equals to resampler's input
      // latency.
      mInputTail[aChannelIndex].StoreTail<T>(aOutBuffer, aOutFrames);
      if (aChannelIndex == 0 && !mIsWarmingUp) {
        mInputStreamFile.Write(nextOutFrame, outFramesNeeded);
        mOutputStreamFile.Write(nextOutFrame, outFramesNeeded);
      }
      return underrun;
    }

    auto resample = [&](const T* aInBuffer, uint32_t aInLength) -> uint32_t {
      uint32_t outFramesResampled = outFramesNeeded;
      uint32_t inFrames = aInLength;
      ResampleInternal(aInBuffer, &inFrames, nextOutFrame, &outFramesResampled,
                       aChannelIndex);
      nextOutFrame += outFramesResampled;
      outFramesNeeded -= outFramesResampled;
      mInputTail[aChannelIndex].StoreTail<T>(aInBuffer, inFrames);
      return inFrames;
    };

    MOZ_ASSERT(!mResamplerIsBypassed);
    mInternalInBuffer[aChannelIndex].ReadNoCopy(
        [&](const Span<const T>& aInBuffer) -> uint32_t {
          if (!outFramesNeeded) {
            return 0;
          }
          return resample(aInBuffer.Elements(), aInBuffer.Length());
        });

    if (outFramesNeeded == 0) {
      return false;
    }

    while (outFramesNeeded > 0) {
      MOZ_ASSERT(mInternalInBuffer[aChannelIndex].AvailableRead() == 0);
      // Round up.
      uint32_t totalInFramesNeeded =
          ((CheckedUint32(outFramesNeeded) * mInRate + mOutRate - 1) / mOutRate)
              .value();
      resample(nullptr, totalInFramesNeeded);
    }
    mIsPreBufferSet = false;
    return true;
  }

  template <typename T>
  void PushInFrames(const T* aInBuffer, const uint32_t aInFrames,
                    uint32_t aChannelIndex) {
    MOZ_ASSERT(aInBuffer);
    MOZ_ASSERT(aInFrames);
    MOZ_ASSERT(mChannels);
    MOZ_ASSERT(aChannelIndex < mChannels);
    MOZ_ASSERT(aChannelIndex < mInternalInBuffer.Length());
    EnsureInputBufferSizeInFrames(
        mInternalInBuffer[aChannelIndex].AvailableRead() + aInFrames);
    mInternalInBuffer[aChannelIndex].Write(Span(aInBuffer, aInFrames));
  }

  void WarmUpResampler(bool aSkipLatency);

  bool EnsureInputBufferSizeInFrames(uint32_t aSizeInFrames) {
    uint32_t sampleSize = 0;
    if (mSampleFormat == AUDIO_FORMAT_FLOAT32) {
      sampleSize = sizeof(float);
    } else if (mSampleFormat == AUDIO_FORMAT_S16) {
      sampleSize = sizeof(short);
    }

    if (sampleSize == 0) {
      // No sample format set, we wouldn't know how many bytes to allocate.
      return true;
    }

    uint32_t sizeInFrames = InFramesBufferSize();
    if (aSizeInFrames <= sizeInFrames) {
      // Buffer size is sufficient.
      return true;  // no reallocation necessary
    }

    // 5 second cap.
    const uint32_t cap = 5 * mInRate;
    if (sizeInFrames >= cap) {
      // Already at the cap.
      return false;
    }

    // As a backoff strategy, at least double the previous size.
    sizeInFrames *= 2;

    if (aSizeInFrames > sizeInFrames) {
      // A larger buffer than the normal backoff strategy provides is needed, or
      // this is the first time setting the buffer size. Add another 50ms, as
      // some jitter is expected.
      sizeInFrames = aSizeInFrames + mInRate / 20;
    }

    // mInputPreBufferFrameCount is an indication of the desired average
    // buffering.  Provide for at least twice this.
    sizeInFrames = std::max(sizeInFrames, mInputPreBufferFrameCount * 2);

    sizeInFrames = std::min(cap, sizeInFrames);

    bool success = true;
    for (auto& b : mInternalInBuffer) {
      success = success && b.EnsureLengthBytes(sampleSize * sizeInFrames);
    }

    if (success) {
      // All buffers have the new size.
      return true;
    }

    // Allocating an input buffer failed. We stick with the old buffer size.
    NS_WARNING(nsPrintfCString("Failed to allocate a buffer of %u bytes (%u "
                               "frames). Expect glitches.",
                               sampleSize * sizeInFrames, sizeInFrames)
                   .get());
    return false;
  }

 public:
  const uint32_t mOutRate;

 private:
  bool mIsPreBufferSet = false;
  bool mIsWarmingUp = false;
  // The resampler can be bypassed when the input and output rates match and
  // any frames buffered in the resampler have been extracted.  This initial
  // value is reset on construction by UpdateResampler() if the rates differ.
  bool mResamplerIsBypassed = true;
  uint32_t mInputPreBufferFrameCount;
  uint32_t mChannels = 0;
  uint32_t mInRate;

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
      const T* inBuffer = aInBuffer;
      mSize = std::min(aInFrames, MAXSIZE);
      if (inBuffer) {
        PodCopy(Buffer<T>(), inBuffer + aInFrames - mSize, mSize);
      } else {
        std::fill_n(Buffer<T>(), mSize, static_cast<T>(0));
      }
    }
    uint32_t Length() { return mSize; }
    static constexpr uint32_t MAXSIZE = 20;

   private:
    float mBuffer[MAXSIZE] = {};
    uint32_t mSize = 0;
  };
  AutoTArray<TailBuffer, STEREO> mInputTail;

  WavDumper mInputStreamFile;
  WavDumper mOutputStreamFile;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_DRIFTCONTROL_DYNAMICRESAMPLER_H_
