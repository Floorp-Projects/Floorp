/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioNodeEngine.h"

namespace mozilla {

void
AllocateAudioBlock(uint32_t aChannelCount, AudioChunk* aChunk)
{
  // XXX for SIMD purposes we should do something here to make sure the
  // channel buffers are 16-byte aligned.
  nsRefPtr<SharedBuffer> buffer =
    SharedBuffer::Create(WEBAUDIO_BLOCK_SIZE*aChannelCount*sizeof(float));
  aChunk->mDuration = WEBAUDIO_BLOCK_SIZE;
  aChunk->mChannelData.SetLength(aChannelCount);
  float* data = static_cast<float*>(buffer->Data());
  for (uint32_t i = 0; i < aChannelCount; ++i) {
    aChunk->mChannelData[i] = data + i*WEBAUDIO_BLOCK_SIZE;
  }
  aChunk->mBuffer = buffer.forget();
  aChunk->mVolume = 1.0f;
  aChunk->mBufferFormat = AUDIO_FORMAT_FLOAT32;
}

void
WriteZeroesToAudioBlock(AudioChunk* aChunk, uint32_t aStart, uint32_t aLength)
{
  MOZ_ASSERT(aStart + aLength <= WEBAUDIO_BLOCK_SIZE);
  if (aLength == 0)
    return;
  for (uint32_t i = 0; i < aChunk->mChannelData.Length(); ++i) {
    memset(static_cast<float*>(const_cast<void*>(aChunk->mChannelData[i])) + aStart,
           0, aLength*sizeof(float));
  }
}

void
AudioBlockAddChannelWithScale(const float aInput[WEBAUDIO_BLOCK_SIZE],
                              float aScale,
                              float aOutput[WEBAUDIO_BLOCK_SIZE])
{
  if (aScale == 1.0f) {
    for (uint32_t i = 0; i < WEBAUDIO_BLOCK_SIZE; ++i) {
      aOutput[i] += aInput[i];
    }
  } else {
    for (uint32_t i = 0; i < WEBAUDIO_BLOCK_SIZE; ++i) {
      aOutput[i] += aInput[i]*aScale;
    }
  }
}

void
AudioBlockCopyChannelWithScale(const float aInput[WEBAUDIO_BLOCK_SIZE],
                               float aScale,
                               float aOutput[WEBAUDIO_BLOCK_SIZE])
{
  if (aScale == 1.0f) {
    memcpy(aOutput, aInput, WEBAUDIO_BLOCK_SIZE*sizeof(float));
  } else {
    for (uint32_t i = 0; i < WEBAUDIO_BLOCK_SIZE; ++i) {
      aOutput[i] = aInput[i]*aScale;
    }
  }
}

}
