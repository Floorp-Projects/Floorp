/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DynamicResampler.h"

namespace mozilla {

DynamicResampler::DynamicResampler(uint32_t aInRate, uint32_t aOutRate,
                                   uint32_t aInputPreBufferFrameCount)
    : mOutRate(aOutRate),
      mInputPreBufferFrameCount(aInputPreBufferFrameCount),
      mInRate(aInRate) {
  MOZ_ASSERT(aInRate);
  MOZ_ASSERT(aOutRate);
  UpdateResampler(mInRate, STEREO);
  mInputStreamFile.Open("DynamicResamplerInFirstChannel", 1, mInRate);
  mOutputStreamFile.Open("DynamicResamplerOutFirstChannel", 1, mOutRate);
}

DynamicResampler::~DynamicResampler() {
  if (mResampler) {
    speex_resampler_destroy(mResampler);
  }
}

void DynamicResampler::SetSampleFormat(AudioSampleFormat aFormat) {
  MOZ_ASSERT(mSampleFormat == AUDIO_FORMAT_SILENCE);
  MOZ_ASSERT(aFormat == AUDIO_FORMAT_S16 || aFormat == AUDIO_FORMAT_FLOAT32);

  mSampleFormat = aFormat;
  for (AudioRingBuffer& b : mInternalInBuffer) {
    b.SetSampleFormat(mSampleFormat);
  }

  // Pre-allocate something big.
  // EnsureInputBufferDuration() adds 50ms for jitter to this first allocation
  // so the 50ms argument means at least 100ms.
  EnsureInputBufferSizeInFrames(mInRate / 20);
}

void DynamicResampler::EnsurePreBuffer(media::TimeUnit aDuration) {
  if (mIsPreBufferSet) {
    return;
  }

  uint32_t buffered = mInternalInBuffer[0].AvailableRead();
  if (buffered == 0) {
    // Wait for the first input segment before deciding how much to pre-buffer.
    // If it is large it indicates high-latency, and the buffer would have to
    // handle that.  This also means that the pre-buffer is not set up just
    // before a large input segment would extend the buffering beyond the
    // desired level.
    return;
  }

  mIsPreBufferSet = true;

  uint32_t needed =
      aDuration.ToTicksAtRate(mInRate) + mInputPreBufferFrameCount;
  EnsureInputBufferSizeInFrames(needed);

  if (needed > buffered) {
    for (auto& b : mInternalInBuffer) {
      b.PrependSilence(needed - buffered);
    }
  } else if (needed < buffered) {
    for (auto& b : mInternalInBuffer) {
      b.Discard(buffered - needed);
    }
  }
}

void DynamicResampler::SetInputPreBufferFrameCount(
    uint32_t aInputPreBufferFrameCount) {
  mInputPreBufferFrameCount = aInputPreBufferFrameCount;
}

bool DynamicResampler::Resample(float* aOutBuffer, uint32_t aOutFrames,
                                uint32_t aChannelIndex) {
  MOZ_ASSERT(mSampleFormat == AUDIO_FORMAT_FLOAT32);
  return ResampleInternal(aOutBuffer, aOutFrames, aChannelIndex);
}

bool DynamicResampler::Resample(int16_t* aOutBuffer, uint32_t aOutFrames,
                                uint32_t aChannelIndex) {
  MOZ_ASSERT(mSampleFormat == AUDIO_FORMAT_S16);
  return ResampleInternal(aOutBuffer, aOutFrames, aChannelIndex);
}

void DynamicResampler::ResampleInternal(const float* aInBuffer,
                                        uint32_t* aInFrames, float* aOutBuffer,
                                        uint32_t* aOutFrames,
                                        uint32_t aChannelIndex) {
  MOZ_ASSERT(mResampler);
  MOZ_ASSERT(mChannels);
  MOZ_ASSERT(mInRate);
  MOZ_ASSERT(mOutRate);

  MOZ_ASSERT(aInFrames);
  MOZ_ASSERT(*aInFrames > 0);
  MOZ_ASSERT(aOutBuffer);
  MOZ_ASSERT(aOutFrames);
  MOZ_ASSERT(*aOutFrames > 0);

  MOZ_ASSERT(aChannelIndex <= mChannels);

#ifdef DEBUG
  int rv =
#endif
      speex_resampler_process_float(mResampler, aChannelIndex, aInBuffer,
                                    aInFrames, aOutBuffer, aOutFrames);
  MOZ_ASSERT(rv == RESAMPLER_ERR_SUCCESS);

  if (aChannelIndex == 0 && !mIsWarmingUp) {
    mInputStreamFile.Write(aInBuffer, *aInFrames);
    mOutputStreamFile.Write(aOutBuffer, *aOutFrames);
  }
}

void DynamicResampler::ResampleInternal(const int16_t* aInBuffer,
                                        uint32_t* aInFrames,
                                        int16_t* aOutBuffer,
                                        uint32_t* aOutFrames,
                                        uint32_t aChannelIndex) {
  MOZ_ASSERT(mResampler);
  MOZ_ASSERT(mChannels);
  MOZ_ASSERT(mInRate);
  MOZ_ASSERT(mOutRate);

  MOZ_ASSERT(aInFrames);
  MOZ_ASSERT(*aInFrames > 0);
  MOZ_ASSERT(aOutBuffer);
  MOZ_ASSERT(aOutFrames);
  MOZ_ASSERT(*aOutFrames > 0);

  MOZ_ASSERT(aChannelIndex <= mChannels);

#ifdef DEBUG
  int rv =
#endif
      speex_resampler_process_int(mResampler, aChannelIndex, aInBuffer,
                                  aInFrames, aOutBuffer, aOutFrames);
  MOZ_ASSERT(rv == RESAMPLER_ERR_SUCCESS);

  if (aChannelIndex == 0 && !mIsWarmingUp) {
    mInputStreamFile.Write(aInBuffer, *aInFrames);
    mOutputStreamFile.Write(aOutBuffer, *aOutFrames);
  }
}

void DynamicResampler::UpdateResampler(uint32_t aInRate, uint32_t aChannels) {
  MOZ_ASSERT(aInRate);
  MOZ_ASSERT(aChannels);

  if (mChannels != aChannels) {
    uint32_t bufferSizeInFrames = InFramesBufferSize();
    if (mResampler) {
      speex_resampler_destroy(mResampler);
    }
    mResampler = speex_resampler_init(aChannels, aInRate, mOutRate,
                                      SPEEX_RESAMPLER_QUALITY_MIN, nullptr);
    MOZ_ASSERT(mResampler);
    mChannels = aChannels;
    mInRate = aInRate;
    mResamplerIsBypassed &= aInRate == mOutRate;
    // Between mono and stereo changes, keep always allocated 2 channels to
    // avoid reallocations in the most common case.
    if ((mChannels == STEREO || mChannels == 1) &&
        mInternalInBuffer.Length() == STEREO) {
      // Don't worry if format is not set it will write silence then.
      if ((mSampleFormat == AUDIO_FORMAT_S16 ||
           mSampleFormat == AUDIO_FORMAT_FLOAT32) &&
          mChannels == STEREO) {
        // The mono channel is always up to date. When we are going from mono
        // to stereo upmix the mono to stereo channel
        uint32_t bufferedDuration = mInternalInBuffer[0].AvailableRead();
        mInternalInBuffer[1].Clear();
        if (bufferedDuration) {
          mInternalInBuffer[1].Write(mInternalInBuffer[0], bufferedDuration);
        }
      }
      // Maintain stereo size
      mInputTail.SetLength(STEREO);
      WarmUpResampler(false);
      return;
    }
    // upmix or downmix, for now just clear but it has to be updated
    // because allocates and this is executed in audio thread.
    mInternalInBuffer.Clear();
    for (uint32_t i = 0; i < mChannels; ++i) {
      AudioRingBuffer* b = mInternalInBuffer.AppendElement(0);

      if (mSampleFormat != AUDIO_FORMAT_SILENCE) {
        // In ctor this update is not needed
        b->SetSampleFormat(mSampleFormat);
      }
    }
    EnsureInputBufferSizeInFrames(bufferSizeInFrames);
    mInputTail.SetLength(mChannels);
    return;
  }

  if (mInRate != aInRate) {
    // If the rates was the same the resampler was not being used so warm up.
    if (mResamplerIsBypassed) {
      mResamplerIsBypassed = false;
      WarmUpResampler(true);
    }

#ifdef DEBUG
    int rv =
#endif
        speex_resampler_set_rate(mResampler, aInRate, mOutRate);
    MOZ_ASSERT(rv == RESAMPLER_ERR_SUCCESS);
    mInRate = aInRate;
  }
}

void DynamicResampler::WarmUpResampler(bool aSkipLatency) {
  MOZ_ASSERT(mInputTail.Length());
  mIsWarmingUp = true;
  for (uint32_t i = 0; i < mChannels; ++i) {
    if (!mInputTail[i].Length()) {
      continue;
    }
    uint32_t inFrames = mInputTail[i].Length();
    uint32_t outFrames = 5 * TailBuffer::MAXSIZE;  // something big
    if (mSampleFormat == AUDIO_FORMAT_S16) {
      short outBuffer[5 * TailBuffer::MAXSIZE] = {};
      ResampleInternal(mInputTail[i].Buffer<short>(), &inFrames, outBuffer,
                       &outFrames, i);
      MOZ_ASSERT(inFrames == (uint32_t)mInputTail[i].Length());
    } else {
      float outBuffer[100] = {};
      ResampleInternal(mInputTail[i].Buffer<float>(), &inFrames, outBuffer,
                       &outFrames, i);
      MOZ_ASSERT(inFrames == (uint32_t)mInputTail[i].Length());
    }
  }
  if (aSkipLatency) {
    // Don't generate output frames corresponding to times before the next
    // input sample.
    speex_resampler_skip_zeros(mResampler);
  }
  mIsWarmingUp = false;
}

void DynamicResampler::AppendInput(Span<const float* const> aInBuffer,
                                   uint32_t aInFrames) {
  MOZ_ASSERT(mSampleFormat == AUDIO_FORMAT_FLOAT32);
  AppendInputInternal(aInBuffer, aInFrames);
}
void DynamicResampler::AppendInput(Span<const int16_t* const> aInBuffer,
                                   uint32_t aInFrames) {
  MOZ_ASSERT(mSampleFormat == AUDIO_FORMAT_S16);
  AppendInputInternal(aInBuffer, aInFrames);
}

void DynamicResampler::AppendInputSilence(const uint32_t aInFrames) {
  MOZ_ASSERT(aInFrames);
  MOZ_ASSERT(mChannels);
  MOZ_ASSERT(mInternalInBuffer.Length() >= (uint32_t)mChannels);
  for (uint32_t i = 0; i < mChannels; ++i) {
    mInternalInBuffer[i].WriteSilence(aInFrames);
  }
}

uint32_t DynamicResampler::InFramesBufferSize() const {
  if (mSampleFormat == AUDIO_FORMAT_SILENCE) {
    return 0;
  }
  // Buffers may have different capacities if a memory allocation has failed.
  MOZ_ASSERT(!mInternalInBuffer.IsEmpty());
  uint32_t min = std::numeric_limits<uint32_t>::max();
  for (const auto& b : mInternalInBuffer) {
    min = std::min(min, b.Capacity());
  }
  return min;
}

uint32_t DynamicResampler::InFramesBuffered(uint32_t aChannelIndex) const {
  MOZ_ASSERT(mChannels);
  MOZ_ASSERT(aChannelIndex <= mChannels);
  MOZ_ASSERT(aChannelIndex <= mInternalInBuffer.Length());
  if (!mIsPreBufferSet) {
    return mInputPreBufferFrameCount;
  }
  return mInternalInBuffer[aChannelIndex].AvailableRead();
}

}  // namespace mozilla
