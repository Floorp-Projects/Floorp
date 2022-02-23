/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DynamicResampler.h"

namespace mozilla {

DynamicResampler::DynamicResampler(uint32_t aInRate, uint32_t aOutRate,
                                   uint32_t aPreBufferFrames)
    : mInRate(aInRate), mPreBufferFrames(aPreBufferFrames), mOutRate(aOutRate) {
  MOZ_ASSERT(aInRate);
  MOZ_ASSERT(aOutRate);
  UpdateResampler(mOutRate, STEREO);
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
  if (mPreBufferFrames) {
    AppendInputSilence(mPreBufferFrames);
  }
}

bool DynamicResampler::Resample(float* aOutBuffer, uint32_t* aOutFrames,
                                uint32_t aChannelIndex) {
  MOZ_ASSERT(mSampleFormat == AUDIO_FORMAT_FLOAT32);
  return ResampleInternal(aOutBuffer, aOutFrames, aChannelIndex);
}

bool DynamicResampler::Resample(int16_t* aOutBuffer, uint32_t* aOutFrames,
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
}

void DynamicResampler::UpdateResampler(uint32_t aOutRate, uint32_t aChannels) {
  MOZ_ASSERT(aOutRate);
  MOZ_ASSERT(aChannels);

  if (mChannels != aChannels) {
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
      // Pre-allocate something big, twice the pre-buffer, or at least 100ms.
      AudioRingBuffer* b = mInternalInBuffer.AppendElement(
          sizeof(float) * std::max(2 * mPreBufferFrames, mInRate / 10));
      if (mSampleFormat != AUDIO_FORMAT_SILENCE) {
        // In ctor this update is not needed
        b->SetSampleFormat(mSampleFormat);
      }
    }
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
}

void DynamicResampler::AppendInput(const nsTArray<const float*>& aInBuffer,
                                   uint32_t aInFrames) {
  MOZ_ASSERT(mSampleFormat == AUDIO_FORMAT_FLOAT32);
  AppendInputInternal(aInBuffer, aInFrames);
}
void DynamicResampler::AppendInput(const nsTArray<const int16_t*>& aInBuffer,
                                   uint32_t aInFrames) {
  MOZ_ASSERT(mSampleFormat == AUDIO_FORMAT_S16);
  AppendInputInternal(aInBuffer, aInFrames);
}

bool DynamicResampler::EnoughInFrames(uint32_t aOutFrames,
                                      uint32_t aChannelIndex) const {
  if (mInRate == mOutRate) {
    return InFramesBuffered(aChannelIndex) >= aOutFrames;
  }
  if (!(mOutRate % mInRate) && !(aOutFrames % mOutRate / mInRate)) {
    return InFramesBuffered(aChannelIndex) >= aOutFrames / (mOutRate / mInRate);
  }
  if (!(mInRate % mOutRate) && !(aOutFrames % mOutRate / mInRate)) {
    return InFramesBuffered(aChannelIndex) >= aOutFrames * mInRate / mOutRate;
  }
  return InFramesBuffered(aChannelIndex) > aOutFrames * mInRate / mOutRate;
}

bool DynamicResampler::CanResample(uint32_t aOutFrames) const {
  for (uint32_t i = 0; i < mChannels; ++i) {
    if (!EnoughInFrames(aOutFrames, i)) {
      return false;
    }
  }
  return true;
}

void DynamicResampler::AppendInputSilence(const uint32_t aInFrames) {
  MOZ_ASSERT(aInFrames);
  MOZ_ASSERT(mChannels);
  MOZ_ASSERT(mInternalInBuffer.Length() >= (uint32_t)mChannels);
  for (uint32_t i = 0; i < mChannels; ++i) {
    mInternalInBuffer[i].WriteSilence(aInFrames);
  }
}

uint32_t DynamicResampler::InFramesBuffered(uint32_t aChannelIndex) const {
  MOZ_ASSERT(mChannels);
  MOZ_ASSERT(aChannelIndex <= mChannels);
  MOZ_ASSERT(aChannelIndex <= mInternalInBuffer.Length());
  return mInternalInBuffer[aChannelIndex].AvailableRead();
}

uint32_t DynamicResampler::InFramesLeftToBuffer(uint32_t aChannelIndex) const {
  MOZ_ASSERT(mChannels);
  MOZ_ASSERT(aChannelIndex <= mChannels);
  MOZ_ASSERT(aChannelIndex <= mInternalInBuffer.Length());
  return mInternalInBuffer[aChannelIndex].AvailableWrite();
}

AudioChunkList::AudioChunkList(uint32_t aTotalDuration, uint32_t aChannels) {
  uint32_t numOfChunks = aTotalDuration / mChunkCapacity;
  if (aTotalDuration % mChunkCapacity) {
    ++numOfChunks;
  }
  CreateChunks(numOfChunks, aChannels);
}

void AudioChunkList::CreateChunks(uint32_t aNumOfChunks, uint32_t aChannels) {
  MOZ_ASSERT(!mChunks.Length());
  MOZ_ASSERT(aNumOfChunks);
  MOZ_ASSERT(aChannels);
  mChunks.AppendElements(aNumOfChunks);

  for (AudioChunk& chunk : mChunks) {
    AutoTArray<nsTArray<float>, STEREO> buffer;
    buffer.AppendElements(aChannels);

    AutoTArray<const float*, STEREO> bufferPtrs;
    bufferPtrs.AppendElements(aChannels);

    for (uint32_t i = 0; i < aChannels; ++i) {
      float* ptr = buffer[i].AppendElements(mChunkCapacity);
      bufferPtrs[i] = ptr;
    }

    chunk.mBuffer = new mozilla::SharedChannelArrayBuffer(std::move(buffer));
    chunk.mChannelData.AppendElements(aChannels);
    for (uint32_t i = 0; i < aChannels; ++i) {
      chunk.mChannelData[i] = bufferPtrs[i];
    }
  }
}

void AudioChunkList::UpdateToMonoOrStereo(uint32_t aChannels) {
  MOZ_ASSERT(mChunks.Length());
  MOZ_ASSERT(mSampleFormat == AUDIO_FORMAT_S16 ||
             mSampleFormat == AUDIO_FORMAT_FLOAT32);
  MOZ_ASSERT(aChannels == 1 || aChannels == 2);

  for (AudioChunk& chunk : mChunks) {
    MOZ_ASSERT(chunk.ChannelCount() != (uint32_t)aChannels);
    MOZ_ASSERT(chunk.ChannelCount() == 1 || chunk.ChannelCount() == 2);
    chunk.mChannelData.SetLengthAndRetainStorage(aChannels);
    if (mSampleFormat == AUDIO_FORMAT_S16) {
      SharedChannelArrayBuffer<short>* channelArray =
          static_cast<SharedChannelArrayBuffer<short>*>(chunk.mBuffer.get());
      channelArray->mBuffers.SetLengthAndRetainStorage(aChannels);
      if (aChannels == 2) {
        // This an indirect allocation, unfortunately.
        channelArray->mBuffers[1].SetLength(mChunkCapacity);
        chunk.mChannelData[1] = channelArray->mBuffers[1].Elements();
      }
    } else {
      SharedChannelArrayBuffer<float>* channelArray =
          static_cast<SharedChannelArrayBuffer<float>*>(chunk.mBuffer.get());
      channelArray->mBuffers.SetLengthAndRetainStorage(aChannels);
      if (aChannels == 2) {
        // This an indirect allocation, unfortunately.
        channelArray->mBuffers[1].SetLength(mChunkCapacity);
        chunk.mChannelData[1] = channelArray->mBuffers[1].Elements();
      }
    }
  }
}

void AudioChunkList::SetSampleFormat(AudioSampleFormat aFormat) {
  MOZ_ASSERT(mSampleFormat == AUDIO_FORMAT_SILENCE);
  MOZ_ASSERT(aFormat == AUDIO_FORMAT_S16 || aFormat == AUDIO_FORMAT_FLOAT32);
  mSampleFormat = aFormat;
  if (mSampleFormat == AUDIO_FORMAT_S16) {
    mChunkCapacity = 2 * mChunkCapacity;
  }
}

AudioChunk& AudioChunkList::GetNext() {
  AudioChunk& chunk = mChunks[mIndex];
  MOZ_ASSERT(!chunk.mChannelData.IsEmpty());
  MOZ_ASSERT(chunk.mBuffer);
  MOZ_ASSERT(!chunk.mBuffer->IsShared());
  MOZ_ASSERT(mSampleFormat == AUDIO_FORMAT_S16 ||
             mSampleFormat == AUDIO_FORMAT_FLOAT32);
  chunk.mDuration = 0;
  chunk.mVolume = 1.0f;
  chunk.mPrincipalHandle = PRINCIPAL_HANDLE_NONE;
  chunk.mBufferFormat = mSampleFormat;
  IncrementIndex();
  return chunk;
}

void AudioChunkList::Update(uint32_t aChannels) {
  MOZ_ASSERT(mChunks.Length());
  if (mChunks[0].ChannelCount() == aChannels) {
    return;
  }

  // Special handling between mono and stereo to avoid reallocations.
  if (aChannels <= 2 && mChunks[0].ChannelCount() <= 2) {
    UpdateToMonoOrStereo(aChannels);
    return;
  }

  uint32_t numOfChunks = mChunks.Length();
  mChunks.ClearAndRetainStorage();
  CreateChunks(numOfChunks, aChannels);
}

AudioResampler::AudioResampler(uint32_t aInRate, uint32_t aOutRate,
                               uint32_t aPreBufferFrames)
    : mResampler(aInRate, aOutRate, aPreBufferFrames),
      mOutputChunks(aOutRate / 10, STEREO) {}

void AudioResampler::AppendInput(const AudioSegment& aInSegment) {
  MOZ_ASSERT(aInSegment.GetDuration());
  for (AudioSegment::ConstChunkIterator iter(aInSegment); !iter.IsEnded();
       iter.Next()) {
    const AudioChunk& chunk = *iter;
    if (!mIsSampleFormatSet) {
      // We don't know the format yet and all buffers are empty.
      if (chunk.mBufferFormat == AUDIO_FORMAT_SILENCE) {
        // Only silence has been received and the format is unkown. Igonre it,
        // if Resampler() is called it will return silence too.
        continue;
      }
      // First no silence data, set the format once for lifetime and let it
      // continue the rest of the flow. We will not get in here again.
      mOutputChunks.SetSampleFormat(chunk.mBufferFormat);
      mResampler.SetSampleFormat(chunk.mBufferFormat);
      mIsSampleFormatSet = true;
    }
    MOZ_ASSERT(mIsSampleFormatSet);
    if (chunk.IsNull()) {
      mResampler.AppendInputSilence(chunk.GetDuration());
      continue;
    }
    // Make sure the channel is up to date. An AudioSegment can contain chunks
    // with different channel count.
    UpdateChannels(chunk.mChannelData.Length());
    if (chunk.mBufferFormat == AUDIO_FORMAT_FLOAT32) {
      mResampler.AppendInput(chunk.ChannelData<float>(), chunk.GetDuration());
    } else {
      mResampler.AppendInput(chunk.ChannelData<int16_t>(), chunk.GetDuration());
    }
  }
}

AudioSegment AudioResampler::Resample(uint32_t aOutFrames) {
  AudioSegment segment;

  // We don't know what to do yet and we only have received silence if any just
  // return what they want and leave
  if (!mIsSampleFormatSet) {
    segment.AppendNullData(aOutFrames);
    return segment;
  }

  // Not enough input frames abort. We check for the requested frames plus one.
  // This is to make sure that the individual resample iteration that will
  // follow up, will have enough frames even if one of them consume an extra
  // frame.
  if (!mResampler.CanResample(aOutFrames + 1)) {
    return segment;
  }

  uint32_t totalFrames = aOutFrames;
  while (totalFrames) {
    MOZ_ASSERT(totalFrames > 0);
    AudioChunk& chunk = mOutputChunks.GetNext();
    uint32_t outFrames = std::min(totalFrames, mOutputChunks.ChunkCapacity());
    totalFrames -= outFrames;

    for (uint32_t i = 0; i < chunk.ChannelCount(); ++i) {
      uint32_t outFramesUsed = outFrames;
      if (chunk.mBufferFormat == AUDIO_FORMAT_FLOAT32) {
#ifdef DEBUG
        bool rv =
#endif
            mResampler.Resample(chunk.ChannelDataForWrite<float>(i),
                                &outFramesUsed, i);
        MOZ_ASSERT(rv);
      } else {
#ifdef DEBUG
        bool rv =
#endif
            mResampler.Resample(chunk.ChannelDataForWrite<int16_t>(i),
                                &outFramesUsed, i);
        MOZ_ASSERT(rv);
      }
      MOZ_ASSERT(outFramesUsed == outFrames);
      chunk.mDuration = outFrames;
    }

    // Create a copy in order to consume that copy and not the pre-allocated
    // chunk
    segment.AppendAndConsumeChunk(AudioChunk(chunk));
  }

  return segment;
}

void AudioResampler::Update(uint32_t aOutRate, uint32_t aChannels) {
  mResampler.UpdateResampler(aOutRate, aChannels);
  mOutputChunks.Update(aChannels);
}

uint32_t AudioResampler::InputReadableFrames() const {
  if (!mIsSampleFormatSet) {
    return mResampler.mPreBufferFrames;
  }
  return mResampler.InFramesBuffered(0);
}

uint32_t AudioResampler::InputWritableFrames() const {
  if (!mIsSampleFormatSet) {
    return mResampler.mPreBufferFrames;
  }
  return mResampler.InFramesLeftToBuffer(0);
}

}  // namespace mozilla
