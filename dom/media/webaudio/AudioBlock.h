/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef MOZILLA_AUDIOBLOCK_H_
#define MOZILLA_AUDIOBLOCK_H_

#include "AudioSegment.h"

namespace mozilla {

/**
 * An AudioChunk whose buffer contents need to be valid only for one
 * processing block iteration, after which contents can be overwritten if the
 * buffer has not been passed to longer term storage or to another thread,
 * which may happen though AsAudioChunk() or AsMutableChunk().
 *
 * Use on graph thread only.
 */
class AudioBlock : private AudioChunk {
 public:
  AudioBlock() {
    mDuration = WEBAUDIO_BLOCK_SIZE;
    mBufferFormat = AUDIO_FORMAT_SILENCE;
  }
  // No effort is made in constructors to ensure that mBufferIsDownstreamRef
  // is set because the block is expected to be a temporary and so the
  // reference will be released before the next iteration.
  // The custom copy constructor is required so as not to set
  // mBufferIsDownstreamRef without notifying AudioBlockBuffer.
  AudioBlock(const AudioBlock& aBlock) : AudioChunk(aBlock.AsAudioChunk()) {}
  explicit AudioBlock(const AudioChunk& aChunk) : AudioChunk(aChunk) {
    MOZ_ASSERT(aChunk.mDuration == WEBAUDIO_BLOCK_SIZE);
  }
  ~AudioBlock();

  using AudioChunk::ChannelCount;
  using AudioChunk::ChannelData;
  using AudioChunk::GetDuration;
  using AudioChunk::IsAudible;
  using AudioChunk::IsNull;
  using AudioChunk::SizeOfExcludingThis;
  using AudioChunk::SizeOfExcludingThisIfUnshared;
  // mDuration is not exposed.  Use GetDuration().
  // mBuffer is not exposed.  Use Get/SetBuffer().
  using AudioChunk::mBufferFormat;
  using AudioChunk::mChannelData;
  using AudioChunk::mVolume;

  const AudioChunk& AsAudioChunk() const { return *this; }
  AudioChunk* AsMutableChunk() {
    ClearDownstreamMark();
    return this;
  }

  /**
   * Allocates, if necessary, aChannelCount buffers of WEBAUDIO_BLOCK_SIZE float
   * samples for writing.
   */
  void AllocateChannels(uint32_t aChannelCount);

  /**
   * ChannelFloatsForWrite() should only be used when the buffers have been
   * created with AllocateChannels().
   */
  float* ChannelFloatsForWrite(size_t aChannel) {
    MOZ_ASSERT(mBufferFormat == AUDIO_FORMAT_FLOAT32);
    MOZ_ASSERT(CanWrite());
    return static_cast<float*>(const_cast<void*>(mChannelData[aChannel]));
  }

  ThreadSharedObject* GetBuffer() const { return mBuffer; }
  void SetBuffer(ThreadSharedObject* aNewBuffer);
  void SetNull(TrackTime aDuration) {
    MOZ_ASSERT(aDuration == WEBAUDIO_BLOCK_SIZE);
    SetBuffer(nullptr);
    mChannelData.Clear();
    mVolume = 1.0f;
    mBufferFormat = AUDIO_FORMAT_SILENCE;
  }

  AudioBlock& operator=(const AudioBlock& aBlock) {
    // Instead of just copying, mBufferIsDownstreamRef must be first cleared
    // if set.  It is set again for the new mBuffer if possible.  This happens
    // in SetBuffer().
    return *this = aBlock.AsAudioChunk();
  }
  AudioBlock& operator=(const AudioChunk& aChunk) {
    MOZ_ASSERT(aChunk.mDuration == WEBAUDIO_BLOCK_SIZE);
    SetBuffer(aChunk.mBuffer);
    mChannelData = aChunk.mChannelData.Clone();
    mVolume = aChunk.mVolume;
    mBufferFormat = aChunk.mBufferFormat;
    return *this;
  }

  bool IsMuted() const { return mVolume == 0.0f; }

  bool IsSilentOrSubnormal() const {
    if (!mBuffer) {
      return true;
    }

    for (uint32_t i = 0, length = mChannelData.Length(); i < length; ++i) {
      const float* channel = static_cast<const float*>(mChannelData[i]);
      for (TrackTime frame = 0; frame < mDuration; ++frame) {
        if (fabs(channel[frame]) >= FLT_MIN) {
          return false;
        }
      }
    }

    return true;
  }

 private:
  void ClearDownstreamMark();
  bool CanWrite();

  // mBufferIsDownstreamRef is set only when mBuffer references an
  // AudioBlockBuffer created in a different AudioBlock.  That can happen when
  // this AudioBlock is on a node downstream from the node which created the
  // buffer.  When this is set, the AudioBlockBuffer is notified that this
  // reference does not prevent the upstream node from re-using the buffer next
  // iteration and modifying its contents.  The AudioBlockBuffer is also
  // notified when mBuffer releases this reference.
  bool mBufferIsDownstreamRef = false;
};

}  // namespace mozilla

MOZ_DECLARE_RELOCATE_USING_MOVE_CONSTRUCTOR(mozilla::AudioBlock)

#endif  // MOZILLA_AUDIOBLOCK_H_
