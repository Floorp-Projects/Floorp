/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(AudioCompactor_h)
#  define AudioCompactor_h

#  include "MediaQueue.h"
#  include "MediaData.h"
#  include "VideoUtils.h"

namespace mozilla {

class AudioCompactor {
 public:
  explicit AudioCompactor(MediaQueue<AudioData>& aQueue) : mQueue(aQueue) {
    // Determine padding size used by AlignedBuffer.
    size_t paddedSize = AlignedAudioBuffer::AlignmentPaddingSize();
    mSamplesPadding = paddedSize / sizeof(AudioDataValue);
    if (mSamplesPadding * sizeof(AudioDataValue) < paddedSize) {
      // Round up.
      mSamplesPadding++;
    }
  }

  // Push audio data into the underlying queue with minimal heap allocation
  // slop.  This method is responsible for allocating AudioDataValue[] buffers.
  // The caller must provide a functor to copy the data into the buffers.  The
  // functor must provide the following signature:
  //
  //   uint32_t operator()(AudioDataValue *aBuffer, uint32_t aSamples);
  //
  // The functor must copy as many complete frames as possible to the provided
  // buffer given its length (in AudioDataValue elements).  The number of frames
  // copied must be returned.  This copy functor must support being called
  // multiple times in order to copy the audio data fully.  The copy functor
  // must copy full frames as partial frames will be ignored.
  template <typename CopyFunc>
  bool Push(int64_t aOffset, int64_t aTime, int32_t aSampleRate,
            uint32_t aFrames, uint32_t aChannels, CopyFunc aCopyFunc) {
    auto time = media::TimeUnit::FromMicroseconds(aTime);

    // If we are losing more than a reasonable amount to padding, try to chunk
    // the data.
    size_t maxSlop = AudioDataSize(aFrames, aChannels) / MAX_SLOP_DIVISOR;

    while (aFrames > 0) {
      uint32_t samples = GetChunkSamples(aFrames, aChannels, maxSlop);
      if (samples / aChannels > mSamplesPadding / aChannels + 1) {
        samples -= mSamplesPadding;
      }
      AlignedAudioBuffer buffer(samples);
      if (!buffer) {
        return false;
      }

      // Copy audio data to buffer using caller-provided functor.
      uint32_t framesCopied = aCopyFunc(buffer.get(), samples);

      NS_ASSERTION(framesCopied <= aFrames, "functor copied too many frames");
      buffer.SetLength(size_t(framesCopied) * aChannels);

      auto duration = FramesToTimeUnit(framesCopied, aSampleRate);
      if (!duration.IsValid()) {
        return false;
      }

      RefPtr<AudioData> data = new AudioData(aOffset, time, std::move(buffer),
                                             aChannels, aSampleRate);
      MOZ_DIAGNOSTIC_ASSERT(duration == data->mDuration, "must be equal");
      mQueue.Push(data);

      // Remove the frames we just pushed into the queue and loop if there is
      // more to be done.
      time += duration;
      aFrames -= framesCopied;

      // NOTE: No need to update aOffset as its only an approximation anyway.
    }

    return true;
  }

  // Copy functor suitable for copying audio samples already in the
  // AudioDataValue format/layout expected by AudioStream on this platform.
  class NativeCopy {
   public:
    NativeCopy(const uint8_t* aSource, size_t aSourceBytes, uint32_t aChannels)
        : mSource(aSource),
          mSourceBytes(aSourceBytes),
          mChannels(aChannels),
          mNextByte(0) {}

    uint32_t operator()(AudioDataValue* aBuffer, uint32_t aSamples);

   private:
    const uint8_t* const mSource;
    const size_t mSourceBytes;
    const uint32_t mChannels;
    size_t mNextByte;
  };

  // Allow 12.5% slop before chunking kicks in.  Public so that the gtest can
  // access it.
  static const size_t MAX_SLOP_DIVISOR = 8;

 private:
  // Compute the number of AudioDataValue samples that will be fit the most
  // frames while keeping heap allocation slop less than the given threshold.
  static uint32_t GetChunkSamples(uint32_t aFrames, uint32_t aChannels,
                                  size_t aMaxSlop);

  static size_t BytesPerFrame(uint32_t aChannels) {
    return sizeof(AudioDataValue) * aChannels;
  }

  static size_t AudioDataSize(uint32_t aFrames, uint32_t aChannels) {
    return aFrames * BytesPerFrame(aChannels);
  }

  MediaQueue<AudioData>& mQueue;
  size_t mSamplesPadding;
};

}  // namespace mozilla

#endif  // AudioCompactor_h
