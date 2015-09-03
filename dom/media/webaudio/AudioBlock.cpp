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
 *
 * Downstream references are accounted specially so that the creator of the
 * buffer can reuse and modify its contents next iteration if other references
 * are all downstream temporary references held by AudioBlock.
 *
 * This only guarantees 4-byte alignment of the data. For alignment we simply
 * assume that the memory from malloc is at least 4-byte aligned and that
 * AudioBlockBuffer's size is divisible by 4.
 */
class AudioBlockBuffer final : public ThreadSharedObject {
public:

  virtual AudioBlockBuffer* AsAudioBlockBuffer() override { return this; };

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

  // Graph thread only.
  void DownstreamRefAdded() { ++mDownstreamRefCount; }
  void DownstreamRefRemoved() {
    MOZ_ASSERT(mDownstreamRefCount > 0);
    --mDownstreamRefCount;
  }
  // Whether this is shared by any owners that are not downstream.
  // Called only from owners with a reference that is not a downstream
  // reference.  Graph thread only.
  bool HasLastingShares()
  {
    // mRefCnt is atomic and so reading its value is defined even when
    // modifications may happen on other threads.  mDownstreamRefCount is
    // not modified on any other thread.
    //
    // If all other references are downstream references (managed on this, the
    // graph thread), then other threads are not using this buffer and cannot
    // add further references.  This method can safely return false.  The
    // buffer contents can be modified.
    //
    // If there are other references that are not downstream references, then
    // this method will return true.  The buffer will be assumed to be still
    // in use and so will not be reused.
    nsrefcnt count = mRefCnt;
    // This test is strictly less than because the caller has a reference
    // that is not a downstream reference.
    MOZ_ASSERT(mDownstreamRefCount < count);
    return count != mDownstreamRefCount + 1;
  }

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

private:
  AudioBlockBuffer() {}
  ~AudioBlockBuffer() override { MOZ_ASSERT(mDownstreamRefCount == 0); }

  nsAutoRefCnt mDownstreamRefCount;
};

AudioBlock::~AudioBlock()
{
  ClearDownstreamMark();
}

void
AudioBlock::SetBuffer(ThreadSharedObject* aNewBuffer)
{
  if (aNewBuffer == mBuffer) {
    return;
  }

  ClearDownstreamMark();

  mBuffer = aNewBuffer;

  if (!aNewBuffer) {
    return;
  }

  AudioBlockBuffer* buffer = aNewBuffer->AsAudioBlockBuffer();
  if (buffer) {
    buffer->DownstreamRefAdded();
    mBufferIsDownstreamRef = true;
  }
}

void
AudioBlock::ClearDownstreamMark() {
  if (mBufferIsDownstreamRef) {
    mBuffer->AsAudioBlockBuffer()->DownstreamRefRemoved();
    mBufferIsDownstreamRef = false;
  }
}

void
AudioBlock::AssertNoLastingShares() {
  MOZ_ASSERT(!mBuffer->AsAudioBlockBuffer()->HasLastingShares());
}

void
AudioBlock::AllocateChannels(uint32_t aChannelCount)
{
  MOZ_ASSERT(mDuration == WEBAUDIO_BLOCK_SIZE);

  if (mBufferIsDownstreamRef) {
    // This is not our buffer to re-use.
    ClearDownstreamMark();
  } else if (mBuffer && ChannelCount() == aChannelCount) {
    AudioBlockBuffer* buffer = mBuffer->AsAudioBlockBuffer();
    if (buffer && !buffer->HasLastingShares()) {
      MOZ_ASSERT(mBufferFormat == AUDIO_FORMAT_FLOAT32);
      // No need to allocate again.
      mVolume = 1.0f;
      return;
    }
  }

  // XXX for SIMD purposes we should do something here to make sure the
  // channel buffers are 16-byte aligned.
  nsRefPtr<AudioBlockBuffer> buffer = AudioBlockBuffer::Create(aChannelCount);
  mChannelData.SetLength(aChannelCount);
  for (uint32_t i = 0; i < aChannelCount; ++i) {
    mChannelData[i] = buffer->ChannelData(i);
  }
  mBuffer = buffer.forget();
  mVolume = 1.0f;
  mBufferFormat = AUDIO_FORMAT_FLOAT32;
}

} // namespace mozilla
