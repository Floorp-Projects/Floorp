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
class AudioBlock : private AudioChunk
{
public:
  AudioBlock() {
    mDuration = WEBAUDIO_BLOCK_SIZE;
  }
  AudioBlock(const AudioChunk& aChunk) {
    mDuration = WEBAUDIO_BLOCK_SIZE;
    operator=(aChunk);
  }
  ~AudioBlock();

  using AudioChunk::GetDuration;
  using AudioChunk::IsNull;
  using AudioChunk::ChannelCount;
  using AudioChunk::ChannelData;
  using AudioChunk::SizeOfExcludingThisIfUnshared;
  using AudioChunk::SizeOfExcludingThis;
  // mDuration is not exposed.  Use GetDuration().
  // mBuffer is not exposed.  Use SetBuffer().
  using AudioChunk::mChannelData;
  using AudioChunk::mVolume;
  using AudioChunk::mBufferFormat;

  const AudioChunk& AsAudioChunk() const { return *this; }
  AudioChunk* AsMutableChunk() {
    void ClearDownstreamMark();
    return this;
  }

  /**
   * Allocates, if necessary, aChannelCount buffers of WEBAUDIO_BLOCK_SIZE float
   * samples for writing.
   */
  void AllocateChannels(uint32_t aChannelCount);

  float* ChannelFloatsForWrite(size_t aChannel)
  {
    MOZ_ASSERT(mBufferFormat == AUDIO_FORMAT_FLOAT32);
#if DEBUG
    AssertNoLastingShares();
#endif
    return static_cast<float*>(const_cast<void*>(mChannelData[aChannel]));
  }

  void SetBuffer(ThreadSharedObject* aNewBuffer);
  void SetNull(StreamTime aDuration) {
    MOZ_ASSERT(aDuration == WEBAUDIO_BLOCK_SIZE);
    SetBuffer(nullptr);
    mChannelData.Clear();
    mVolume = 1.0f;
    mBufferFormat = AUDIO_FORMAT_SILENCE;
  }

  AudioBlock& operator=(const AudioChunk& aChunk) {
    MOZ_ASSERT(aChunk.mDuration == WEBAUDIO_BLOCK_SIZE);
    SetBuffer(aChunk.mBuffer);
    mChannelData = aChunk.mChannelData;
    mVolume = aChunk.mVolume;
    mBufferFormat = aChunk.mBufferFormat;
    return *this;
  }

  bool IsMuted() const { return mVolume == 0.0f; }

  bool IsSilentOrSubnormal() const
  {
    if (!mBuffer) {
      return true;
    }

    for (uint32_t i = 0, length = mChannelData.Length(); i < length; ++i) {
      const float* channel = static_cast<const float*>(mChannelData[i]);
      for (StreamTime frame = 0; frame < mDuration; ++frame) {
        if (fabs(channel[frame]) >= FLT_MIN) {
          return false;
        }
      }
    }

    return true;
  }

private:
  void ClearDownstreamMark();
  void AssertNoLastingShares();

  // mBufferIsDownstreamRef is set only when mBuffer references an
  // AudioBlockBuffer created in a different AudioBlock.  That can happen when
  // this AudioBlock is on a node downstream from the node which created the
  // buffer.  When this is set, the AudioBlockBuffer is notified that this
  // reference does prevent the upstream node from re-using the buffer next
  // iteration and modifying its contents.  The AudioBlockBuffer is also
  // notified when mBuffer releases this reference.
  bool mBufferIsDownstreamRef = false;
};

} // namespace mozilla

#endif // MOZILLA_AUDIOBLOCK_H_
