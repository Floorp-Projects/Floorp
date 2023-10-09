/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioResampler.h"

namespace mozilla {

AudioResampler::AudioResampler(uint32_t aInRate, uint32_t aOutRate,
                               media::TimeUnit aPreBufferDuration,
                               const PrincipalHandle& aPrincipalHandle)
    : mResampler(aInRate, aOutRate, aPreBufferDuration),
      mOutputChunks(aOutRate / 10, STEREO, aPrincipalHandle) {}

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

AudioSegment AudioResampler::Resample(uint32_t aOutFrames, bool* aHasUnderrun) {
  MOZ_ASSERT(aHasUnderrun);

  AudioSegment segment;

  // We don't know what to do yet and we only have received silence if any just
  // return what they want and leave
  if (!mIsSampleFormatSet) {
    segment.AppendNullData(aOutFrames);
    return segment;
  }

  media::TimeUnit outDuration(aOutFrames, mResampler.GetOutRate());
  mResampler.EnsurePreBuffer(outDuration);

  const media::TimeUnit chunkCapacity(mOutputChunks.ChunkCapacity(),
                                      mResampler.GetOutRate());

  while (!outDuration.IsZero()) {
    MOZ_ASSERT(outDuration.IsPositive());
    AudioChunk& chunk = mOutputChunks.GetNext();
    const media::TimeUnit chunkDuration = std::min(outDuration, chunkCapacity);
    outDuration -= chunkDuration;

    const uint32_t outFrames =
        chunkDuration.ToTicksAtRate(mResampler.GetOutRate());
    for (uint32_t i = 0; i < chunk.ChannelCount(); ++i) {
      if (chunk.mBufferFormat == AUDIO_FORMAT_FLOAT32) {
        *aHasUnderrun |= mResampler.Resample(
            chunk.ChannelDataForWrite<float>(i), outFrames, i);
      } else {
        *aHasUnderrun |= mResampler.Resample(
            chunk.ChannelDataForWrite<int16_t>(i), outFrames, i);
      }
    }
    chunk.mDuration = outFrames;

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

uint32_t AudioResampler::InputCapacityFrames() const {
  return mResampler.InFramesBufferSize();
}

uint32_t AudioResampler::InputReadableFrames() const {
  return mResampler.InFramesBuffered(0);
}

}  // namespace mozilla
