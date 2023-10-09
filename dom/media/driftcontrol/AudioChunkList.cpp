/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioChunkList.h"

namespace mozilla {

AudioChunkList::AudioChunkList(uint32_t aTotalDuration, uint32_t aChannels,
                               const PrincipalHandle& aPrincipalHandle)
    : mPrincipalHandle(aPrincipalHandle) {
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
    AutoTArray<nsTArray<float>, 2> buffer;
    buffer.AppendElements(aChannels);

    AutoTArray<const float*, 2> bufferPtrs;
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
  chunk.mPrincipalHandle = mPrincipalHandle;
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

}  // namespace mozilla
