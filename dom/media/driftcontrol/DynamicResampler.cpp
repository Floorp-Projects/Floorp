/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DynamicResampler.h"

namespace mozilla {

DynamicResampler::DynamicResampler(uint32_t aInRate, uint32_t aOutRate,
                                   media::TimeUnit aPreBufferDuration)
    : mInRate(aInRate),
      mPreBufferDuration(aPreBufferDuration),
      mOutRate(aOutRate) {
  MOZ_ASSERT(aInRate);
  MOZ_ASSERT(aOutRate);
  MOZ_ASSERT(aPreBufferDuration.IsPositiveOrZero());
  UpdateResampler(mOutRate, STEREO);
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

  EnsureInputBufferDuration(CalculateInputBufferDuration());
}

void DynamicResampler::EnsurePreBuffer(media::TimeUnit aDuration) {
  if (mIsPreBufferSet) {
    return;
  }

  media::TimeUnit buffered(mInternalInBuffer[0].AvailableRead(), mInRate);
  if (buffered.IsZero()) {
    // Wait for the first input segment before deciding how much to pre-buffer.
    // If it is large it indicates high-latency, and the buffer would have to
    // handle that.
    return;
  }

  mIsPreBufferSet = true;

  media::TimeUnit needed = aDuration + mPreBufferDuration;
  EnsureInputBufferDuration(needed);

  if (needed > buffered) {
    for (auto& b : mInternalInBuffer) {
      b.PrependSilence((needed - buffered).ToTicksAtRate(mInRate));
    }
  } else if (needed < buffered) {
    for (auto& b : mInternalInBuffer) {
      b.Discard((buffered - needed).ToTicksAtRate(mInRate));
    }
  }
}

void DynamicResampler::SetPreBufferDuration(media::TimeUnit aDuration) {
  MOZ_ASSERT(aDuration.IsPositive());
  mPreBufferDuration = aDuration;
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

  MOZ_ASSERT(aInBuffer);
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

  MOZ_ASSERT(aInBuffer);
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

void DynamicResampler::UpdateResampler(uint32_t aOutRate, uint32_t aChannels) {
  MOZ_ASSERT(aOutRate);
  MOZ_ASSERT(aChannels);

  if (mChannels != aChannels) {
    if (mResampler) {
      speex_resampler_destroy(mResampler);
    }
    mResampler = speex_resampler_init(aChannels, mInRate, aOutRate,
                                      SPEEX_RESAMPLER_QUALITY_MIN, nullptr);
    MOZ_ASSERT(mResampler);
    mChannels = aChannels;
    mOutRate = aOutRate;
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
    media::TimeUnit d = mSetBufferDuration;
    mSetBufferDuration = media::TimeUnit::Zero();
    EnsureInputBufferDuration(d);
    mInputTail.SetLength(mChannels);
    return;
  }

  if (mOutRate != aOutRate) {
    // If the rates was the same the resampler was not being used so warm up.
    if (mOutRate == mInRate) {
      WarmUpResampler(true);
    }

#ifdef DEBUG
    int rv =
#endif
        speex_resampler_set_rate(mResampler, mInRate, aOutRate);
    MOZ_ASSERT(rv == RESAMPLER_ERR_SUCCESS);
    mOutRate = aOutRate;
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
    int inputLatency = speex_resampler_get_input_latency(mResampler);
    MOZ_ASSERT(inputLatency > 0);
    uint32_t ratioNum, ratioDen;
    speex_resampler_get_ratio(mResampler, &ratioNum, &ratioDen);
    // Ratio at this point is one so only skip the input latency. No special
    // calculations are needed.
    speex_resampler_set_skip_frac_num(mResampler, inputLatency * ratioDen);
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
  return mSetBufferDuration.ToTicksAtRate(mInRate);
}

uint32_t DynamicResampler::InFramesBuffered(uint32_t aChannelIndex) const {
  MOZ_ASSERT(mChannels);
  MOZ_ASSERT(aChannelIndex <= mChannels);
  MOZ_ASSERT(aChannelIndex <= mInternalInBuffer.Length());
  if (!mIsPreBufferSet) {
    return mPreBufferDuration.ToTicksAtRate(mInRate);
  }
  return mInternalInBuffer[aChannelIndex].AvailableRead();
}

}  // namespace mozilla
