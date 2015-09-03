/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioBlock.h"

namespace mozilla {

/**
 * Heap-allocated buffer of channels of 128-sample float arrays, with
 * threadsafe refcounting.  Typically you would allocate one of these, fill it
 * in, and then treat it as immutable while it's shared.
 * This only guarantees 4-byte alignment of the data. For alignment we simply
 * assume that the memory from malloc is at least 4-byte aligned and the
 * refcount's size is large enough that AudioBlockBuffer's size is divisible
 * by 4.
 */
class AudioBlockBuffer final : public ThreadSharedObject {
public:
  float* ChannelData(uint32_t aChannel)
  {
    return reinterpret_cast<float*>(this + 1) + aChannel * WEBAUDIO_BLOCK_SIZE;
  }

  static already_AddRefed<AudioBlockBuffer> Create(uint32_t aChannelCount)
  {
    CheckedInt<size_t> size = WEBAUDIO_BLOCK_SIZE;
    size *= aChannelCount;
    size *= sizeof(float);
    size += sizeof(AudioBlockBuffer);
    if (!size.isValid()) {
      MOZ_CRASH();
    }
    void* m = moz_xmalloc(size.value());
    nsRefPtr<AudioBlockBuffer> p = new (m) AudioBlockBuffer();
    NS_ASSERTION((reinterpret_cast<char*>(p.get() + 1) - reinterpret_cast<char*>(p.get())) % 4 == 0,
                 "AudioBlockBuffers should be at least 4-byte aligned");
    return p.forget();
  }

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

private:
  AudioBlockBuffer() {}
};

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

  // XXX for SIMD purposes we should do something here to make sure the
  // channel buffers are 16-byte aligned.
  nsRefPtr<AudioBlockBuffer> buffer = AudioBlockBuffer::Create(aChannelCount);
  aChunk->mDuration = WEBAUDIO_BLOCK_SIZE;
  aChunk->mChannelData.SetLength(aChannelCount);
  for (uint32_t i = 0; i < aChannelCount; ++i) {
    aChunk->mChannelData[i] = buffer->ChannelData(i);
  }
  aChunk->mBuffer = buffer.forget();
  aChunk->mVolume = 1.0f;
  aChunk->mBufferFormat = AUDIO_FORMAT_FLOAT32;
}

} // namespace mozilla
