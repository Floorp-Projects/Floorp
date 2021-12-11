/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioBlock.h"
#include "AlignmentUtils.h"

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
 * We guarantee 16 byte alignment of the channel data.
 */
class AudioBlockBuffer final : public ThreadSharedObject {
 public:
  virtual AudioBlockBuffer* AsAudioBlockBuffer() override { return this; };

  uint32_t ChannelsAllocated() const { return mChannelsAllocated; }
  float* ChannelData(uint32_t aChannel) const {
    float* base =
        reinterpret_cast<float*>(((uintptr_t)(this + 1) + 15) & ~0x0F);
    ASSERT_ALIGNED16(base);
    return base + aChannel * WEBAUDIO_BLOCK_SIZE;
  }

  static already_AddRefed<AudioBlockBuffer> Create(uint32_t aChannelCount) {
    CheckedInt<size_t> size = WEBAUDIO_BLOCK_SIZE;
    size *= aChannelCount;
    size *= sizeof(float);
    size += sizeof(AudioBlockBuffer);
    size += 15;  // padding for alignment
    if (!size.isValid()) {
      MOZ_CRASH();
    }

    void* m = operator new(size.value());
    RefPtr<AudioBlockBuffer> p = new (m) AudioBlockBuffer(aChannelCount);
    NS_ASSERTION((reinterpret_cast<char*>(p.get() + 1) -
                  reinterpret_cast<char*>(p.get())) %
                         4 ==
                     0,
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
  bool HasLastingShares() const {
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

  virtual size_t SizeOfIncludingThis(
      MallocSizeOf aMallocSizeOf) const override {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

 private:
  explicit AudioBlockBuffer(uint32_t aChannelsAllocated)
      : mChannelsAllocated(aChannelsAllocated) {}
  ~AudioBlockBuffer() override { MOZ_ASSERT(mDownstreamRefCount == 0); }

  nsAutoRefCnt mDownstreamRefCount;
  const uint32_t mChannelsAllocated;
};

AudioBlock::~AudioBlock() { ClearDownstreamMark(); }

void AudioBlock::SetBuffer(ThreadSharedObject* aNewBuffer) {
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

void AudioBlock::ClearDownstreamMark() {
  if (mBufferIsDownstreamRef) {
    mBuffer->AsAudioBlockBuffer()->DownstreamRefRemoved();
    mBufferIsDownstreamRef = false;
  }
}

bool AudioBlock::CanWrite() {
  // If mBufferIsDownstreamRef is set then the buffer is not ours to use.
  // It may be in use by another node which is not downstream.
  return !mBufferIsDownstreamRef &&
         !mBuffer->AsAudioBlockBuffer()->HasLastingShares();
}

void AudioBlock::AllocateChannels(uint32_t aChannelCount) {
  MOZ_ASSERT(mDuration == WEBAUDIO_BLOCK_SIZE);

  if (mBufferIsDownstreamRef) {
    // This is not our buffer to re-use.
    ClearDownstreamMark();
  } else if (mBuffer) {
    AudioBlockBuffer* buffer = mBuffer->AsAudioBlockBuffer();
    if (buffer && !buffer->HasLastingShares() &&
        buffer->ChannelsAllocated() >= aChannelCount) {
      MOZ_ASSERT(mBufferFormat == AUDIO_FORMAT_FLOAT32);
      // No need to allocate again.
      uint32_t previousChannelCount = ChannelCount();
      mChannelData.SetLength(aChannelCount);
      for (uint32_t i = previousChannelCount; i < aChannelCount; ++i) {
        mChannelData[i] = buffer->ChannelData(i);
      }
      mVolume = 1.0f;
      return;
    }
  }

  RefPtr<AudioBlockBuffer> buffer = AudioBlockBuffer::Create(aChannelCount);
  mChannelData.SetLength(aChannelCount);
  for (uint32_t i = 0; i < aChannelCount; ++i) {
    mChannelData[i] = buffer->ChannelData(i);
  }
  mBuffer = std::move(buffer);
  mVolume = 1.0f;
  mBufferFormat = AUDIO_FORMAT_FLOAT32;
}

}  // namespace mozilla
