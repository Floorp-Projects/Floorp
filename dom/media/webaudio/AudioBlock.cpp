/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioBlock.h"

namespace mozilla {

void
AllocateAudioBlock(uint32_t aChannelCount, AudioChunk* aChunk)
{
  if (aChunk->mBuffer && !aChunk->mBuffer->IsShared() &&
      aChunk->ChannelCount() == aChannelCount) {
    MOZ_ASSERT(aChunk->mBufferFormat == AUDIO_FORMAT_FLOAT32);
    MOZ_ASSERT(aChunk->mDuration == WEBAUDIO_BLOCK_SIZE);
    // No need to allocate again.
    aChunk->mVolume = 1.0f;
    return;
  }

  CheckedInt<size_t> size = WEBAUDIO_BLOCK_SIZE;
  size *= aChannelCount;
  size *= sizeof(float);
  if (!size.isValid()) {
    MOZ_CRASH();
  }
  // XXX for SIMD purposes we should do something here to make sure the
  // channel buffers are 16-byte aligned.
  nsRefPtr<SharedBuffer> buffer = SharedBuffer::Create(size.value());
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

} // namespace mozilla
