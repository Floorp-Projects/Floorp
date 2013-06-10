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
  MOZ_ASSERT(!aChunk->IsNull(), "You should pass a non-null chunk");
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
AudioBlockCopyChannelWithScale(const float* aInput,
                               float aScale,
                               float* aOutput)
{
  if (aScale == 1.0f) {
    memcpy(aOutput, aInput, WEBAUDIO_BLOCK_SIZE*sizeof(float));
  } else {
    for (uint32_t i = 0; i < WEBAUDIO_BLOCK_SIZE; ++i) {
      aOutput[i] = aInput[i]*aScale;
    }
  }
}

void
BufferComplexMultiply(const float* aInput,
                      const float* aScale,
                      float* aOutput,
                      uint32_t aSize)
{
  for (uint32_t i = 0; i < aSize * 2; i += 2) {
    float real1 = aInput[i];
    float imag1 = aInput[i + 1];
    float real2 = aScale[i];
    float imag2 = aScale[i + 1];
    float realResult = real1 * real2 - imag1 * imag2;
    float imagResult = real1 * imag2 + imag1 * real2;
    aOutput[i] = realResult;
    aOutput[i + 1] = imagResult;
  }
}

void
AudioBlockCopyChannelWithScale(const float aInput[WEBAUDIO_BLOCK_SIZE],
                               const float aScale[WEBAUDIO_BLOCK_SIZE],
                               float aOutput[WEBAUDIO_BLOCK_SIZE])
{
  for (uint32_t i = 0; i < WEBAUDIO_BLOCK_SIZE; ++i) {
    aOutput[i] = aInput[i]*aScale[i];
  }
}

void
AudioBlockInPlaceScale(float aBlock[WEBAUDIO_BLOCK_SIZE],
                       uint32_t aChannelCount,
                       float aScale)
{
  if (aScale == 1.0f) {
    return;
  }
  for (uint32_t i = 0; i < WEBAUDIO_BLOCK_SIZE * aChannelCount; ++i) {
    *aBlock++ *= aScale;
  }
}

void
AudioBlockPanMonoToStereo(const float aInput[WEBAUDIO_BLOCK_SIZE],
                          float aGainL, float aGainR,
                          float aOutputL[WEBAUDIO_BLOCK_SIZE],
                          float aOutputR[WEBAUDIO_BLOCK_SIZE])
{
  AudioBlockCopyChannelWithScale(aInput, aGainL, aOutputL);
  AudioBlockCopyChannelWithScale(aInput, aGainR, aOutputR);
}

void
AudioBlockPanStereoToStereo(const float aInputL[WEBAUDIO_BLOCK_SIZE],
                            const float aInputR[WEBAUDIO_BLOCK_SIZE],
                            float aGainL, float aGainR, bool aIsOnTheLeft,
                            float aOutputL[WEBAUDIO_BLOCK_SIZE],
                            float aOutputR[WEBAUDIO_BLOCK_SIZE])
{
  uint32_t i;

  if (aIsOnTheLeft) {
    for (i = 0; i < WEBAUDIO_BLOCK_SIZE; ++i) {
      *aOutputL++ = *aInputL++ + *aInputR * aGainL;
      *aOutputR++ = *aInputR++ * aGainR;
    }
  } else {
    for (i = 0; i < WEBAUDIO_BLOCK_SIZE; ++i) {
      *aOutputL++ = *aInputL * aGainL;
      *aOutputR++ = *aInputR++ + *aInputL++ * aGainR;
    }
  }
}
}
