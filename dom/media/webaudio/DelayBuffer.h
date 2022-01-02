/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DelayBuffer_h_
#define DelayBuffer_h_

#include "nsTArray.h"
#include "AudioBlock.h"
#include "AudioSegment.h"
#include "mozilla/dom/AudioNodeBinding.h"  // for ChannelInterpretation

namespace mozilla {

class DelayBuffer final {
  typedef dom::ChannelInterpretation ChannelInterpretation;

 public:
  explicit DelayBuffer(float aMaxDelayTicks)
      // Round the maximum delay up to the next tick.
      : mMaxDelayTicks(std::ceil(aMaxDelayTicks)),
        mCurrentChunk(0)
  // mLastReadChunk is initialized in EnsureBuffer
#ifdef DEBUG
        ,
        mHaveWrittenBlock(false)
#endif
  {
    // The 180 second limit in AudioContext::CreateDelay() and the
    // 1 << MEDIA_TIME_FRAC_BITS limit on sample rate provide a limit on the
    // maximum delay.
    MOZ_ASSERT(aMaxDelayTicks <=
               float(std::numeric_limits<decltype(mMaxDelayTicks)>::max()));
  }

  // Write a WEBAUDIO_BLOCK_SIZE block for aChannelCount channels.
  void Write(const AudioBlock& aInputChunk);

  // Read a block with an array of delays, in ticks, for each sample frame.
  // Each delay should be >= 0 and <= MaxDelayTicks().
  void Read(const float aPerFrameDelays[WEBAUDIO_BLOCK_SIZE],
            AudioBlock* aOutputChunk,
            ChannelInterpretation aChannelInterpretation);
  // Read a block with a constant delay. The delay should be >= 0 and
  // <= MaxDelayTicks().
  void Read(float aDelayTicks, AudioBlock* aOutputChunk,
            ChannelInterpretation aChannelInterpretation);

  // Read into one of the channels of aOutputChunk, given an array of
  // delays in ticks.  This is useful when delays are different on different
  // channels.  aOutputChunk must have already been allocated with at least as
  // many channels as were in any of the blocks passed to Write().
  void ReadChannel(const float aPerFrameDelays[WEBAUDIO_BLOCK_SIZE],
                   AudioBlock* aOutputChunk, uint32_t aChannel,
                   ChannelInterpretation aChannelInterpretation);

  // Advance the buffer pointer
  void NextBlock() {
    mCurrentChunk = (mCurrentChunk + 1) % mChunks.Length();
#ifdef DEBUG
    MOZ_ASSERT(mHaveWrittenBlock);
    mHaveWrittenBlock = false;
#endif
  }

  void Reset() { mChunks.Clear(); };

  int MaxDelayTicks() const { return mMaxDelayTicks; }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const;

 private:
  void ReadChannels(const float aPerFrameDelays[WEBAUDIO_BLOCK_SIZE],
                    AudioBlock* aOutputChunk, uint32_t aFirstChannel,
                    uint32_t aNumChannelsToRead,
                    ChannelInterpretation aChannelInterpretation);
  bool EnsureBuffer();
  int PositionForDelay(int aDelay);
  int ChunkForPosition(int aPosition);
  int OffsetForPosition(int aPosition);
  int ChunkForDelay(int aDelay);
  void UpdateUpmixChannels(int aNewReadChunk, uint32_t channelCount,
                           ChannelInterpretation aChannelInterpretation);

  // Circular buffer for capturing delayed samples.
  FallibleTArray<AudioChunk> mChunks;
  // Cache upmixed channel arrays.
  AutoTArray<const float*, GUESS_AUDIO_CHANNELS> mUpmixChannels;
  // Maximum delay, in ticks
  int mMaxDelayTicks;
  // The current position in the circular buffer.  The next write will be to
  // this chunk, and the next read may begin before this chunk.
  int mCurrentChunk;
  // The chunk owning the pointers in mUpmixChannels
  int mLastReadChunk;
#ifdef DEBUG
  bool mHaveWrittenBlock;
#endif
};

}  // namespace mozilla

#endif  // DelayBuffer_h_
