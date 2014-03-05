/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DelayBuffer.h"

#include "mozilla/PodOperations.h"
#include "AudioChannelFormat.h"
#include "AudioNodeEngine.h"

namespace mozilla {

void
DelayBuffer::Write(const AudioChunk& aInputChunk)
{
  // We must have a reference to the buffer if there are channels
  MOZ_ASSERT(aInputChunk.IsNull() == !aInputChunk.mChannelData.Length());

  if (!EnsureBuffer()) {
    return;
  }

  if (mCurrentChunk == mLastReadChunk) {
    mLastReadChunk = -1; // invalidate cache
  }
  mChunks[mCurrentChunk] = aInputChunk;
}

void
DelayBuffer::Read(const double aPerFrameDelays[WEBAUDIO_BLOCK_SIZE],
                  AudioChunk* aOutputChunk,
                  ChannelInterpretation aChannelInterpretation)
{
  int chunkCount = mChunks.Length();
  if (!chunkCount) {
    aOutputChunk->SetNull(WEBAUDIO_BLOCK_SIZE);
    return;
  }

  // Find the maximum number of contributing channels to determine the output
  // channel count that retains all signal information.  Buffered blocks will
  // be upmixed if necessary.
  //
  // First find the range of "delay" offsets backwards from the current
  // position.  Note that these may be negative for frames that are after the
  // current position (including i).
  double minDelay = aPerFrameDelays[0];
  double maxDelay = minDelay;
  for (unsigned i = 1; i < WEBAUDIO_BLOCK_SIZE; ++i) {
    minDelay = std::min(minDelay, aPerFrameDelays[i] - i);
    maxDelay = std::max(maxDelay, aPerFrameDelays[i] - i);
  }

  // Now find the chunks touched by this range and check their channel counts.
  int oldestChunk = ChunkForDelay(int(maxDelay) + 1);
  int youngestChunk = ChunkForDelay(minDelay);

  uint32_t channelCount = 0;
  for (int i = oldestChunk; true; i = (i + 1) % chunkCount) {
    channelCount = GetAudioChannelsSuperset(channelCount,
                                            mChunks[i].ChannelCount());
    if (i == youngestChunk) {
      break;
    }
  }

  if (channelCount) {
    AllocateAudioBlock(channelCount, aOutputChunk);
    ReadChannels(aPerFrameDelays, aOutputChunk,
                 0, channelCount, aChannelInterpretation);
  } else {
    aOutputChunk->SetNull(WEBAUDIO_BLOCK_SIZE);
  }

  // Remember currentDelayFrames for the next ProcessBlock call
  mCurrentDelay = aPerFrameDelays[WEBAUDIO_BLOCK_SIZE - 1];
}

void
DelayBuffer::ReadChannel(const double aPerFrameDelays[WEBAUDIO_BLOCK_SIZE],
                         const AudioChunk* aOutputChunk, uint32_t aChannel,
                         ChannelInterpretation aChannelInterpretation)
{
  if (!mChunks.Length()) {
    float* outputChannel = static_cast<float*>
      (const_cast<void*>(aOutputChunk->mChannelData[aChannel]));
    PodZero(outputChannel, WEBAUDIO_BLOCK_SIZE);
    return;
  }

  ReadChannels(aPerFrameDelays, aOutputChunk,
               aChannel, 1, aChannelInterpretation);
}

void
DelayBuffer::ReadChannels(const double aPerFrameDelays[WEBAUDIO_BLOCK_SIZE],
                          const AudioChunk* aOutputChunk,
                          uint32_t aFirstChannel, uint32_t aNumChannelsToRead,
                          ChannelInterpretation aChannelInterpretation)
{
  uint32_t totalChannelCount = aOutputChunk->mChannelData.Length();
  uint32_t readChannelsEnd = aFirstChannel + aNumChannelsToRead;
  MOZ_ASSERT(readChannelsEnd <= totalChannelCount);

  if (mUpmixChannels.Length() != totalChannelCount) {
    mLastReadChunk = -1; // invalidate cache
  }

  float* const* outputChannels = reinterpret_cast<float* const*>
    (const_cast<void* const*>(aOutputChunk->mChannelData.Elements()));
  for (uint32_t channel = aFirstChannel;
       channel < readChannelsEnd; ++channel) {
    PodZero(outputChannels[channel], WEBAUDIO_BLOCK_SIZE);
  }

  for (unsigned i = 0; i < WEBAUDIO_BLOCK_SIZE; ++i) {
    double currentDelay = aPerFrameDelays[i];
    MOZ_ASSERT(currentDelay >= 0.0);
    MOZ_ASSERT(currentDelay <= static_cast<double>(mMaxDelayTicks));

    // Interpolate two input frames in case the read position does not match
    // an integer index.
    // Use the larger delay, for the older frame, first, as this is more
    // likely to use the cached upmixed channel arrays.
    int floorDelay = int(currentDelay);
    double interpolationFactor = currentDelay - floorDelay;
    int positions[2];
    positions[1] = PositionForDelay(floorDelay) + i;
    positions[0] = positions[1] - 1;

    for (unsigned tick = 0; tick < ArrayLength(positions); ++tick) {
      int readChunk = ChunkForPosition(positions[tick]);
      // mVolume is not set on default initialized chunks so handle null
      // chunks specially.
      if (!mChunks[readChunk].IsNull()) {
        int readOffset = OffsetForPosition(positions[tick]);
        UpdateUpmixChannels(readChunk, totalChannelCount,
                            aChannelInterpretation);
        double multiplier = interpolationFactor * mChunks[readChunk].mVolume;
        for (uint32_t channel = aFirstChannel;
             channel < readChannelsEnd; ++channel) {
          outputChannels[channel][i] += multiplier *
            static_cast<const float*>(mUpmixChannels[channel])[readOffset];
        }
      }

      interpolationFactor = 1.0 - interpolationFactor;
    }
  }
}

void
DelayBuffer::Read(double aDelayTicks, AudioChunk* aOutputChunk,
                  ChannelInterpretation aChannelInterpretation)
{
  const bool firstTime = mCurrentDelay < 0.0;
  double currentDelay = firstTime ? aDelayTicks : mCurrentDelay;

  double computedDelay[WEBAUDIO_BLOCK_SIZE];

  for (unsigned i = 0; i < WEBAUDIO_BLOCK_SIZE; ++i) {
    // If the value has changed, smoothly approach it
    currentDelay += (aDelayTicks - currentDelay) * mSmoothingRate;
    computedDelay[i] = currentDelay;
  }

  Read(computedDelay, aOutputChunk, aChannelInterpretation);
}

bool
DelayBuffer::EnsureBuffer()
{
  if (mChunks.Length() == 0) {
    // The length of the buffer is at least one block greater than the maximum
    // delay so that writing an input block does not overwrite the block that
    // would subsequently be read at maximum delay.  Also round up to the next
    // block size, so that no block of writes will need to wrap.
    const int chunkCount = (mMaxDelayTicks + 2 * WEBAUDIO_BLOCK_SIZE - 1) >>
                                         WEBAUDIO_BLOCK_SIZE_BITS;
    if (!mChunks.SetLength(chunkCount)) {
      return false;
    }

    mLastReadChunk = -1;
  }
  return true;
}

int
DelayBuffer::PositionForDelay(int aDelay) {
  // Adding mChunks.Length() keeps integers positive for defined and
  // appropriate bitshift, remainder, and bitwise operations.
  return ((mCurrentChunk + mChunks.Length()) * WEBAUDIO_BLOCK_SIZE) - aDelay;
}

int
DelayBuffer::ChunkForPosition(int aPosition)
{
  MOZ_ASSERT(aPosition >= 0);
  return (aPosition >> WEBAUDIO_BLOCK_SIZE_BITS) % mChunks.Length();
}

int
DelayBuffer::OffsetForPosition(int aPosition)
{
  MOZ_ASSERT(aPosition >= 0);
  return aPosition & (WEBAUDIO_BLOCK_SIZE - 1);
}

int
DelayBuffer::ChunkForDelay(int aDelay)
{
  return ChunkForPosition(PositionForDelay(aDelay));
}

void
DelayBuffer::UpdateUpmixChannels(int aNewReadChunk, uint32_t aChannelCount,
                                 ChannelInterpretation aChannelInterpretation)
{
  if (aNewReadChunk == mLastReadChunk) {
    MOZ_ASSERT(mUpmixChannels.Length() == aChannelCount);
    return;
  }

  static const float silenceChannel[WEBAUDIO_BLOCK_SIZE] = {};

  mLastReadChunk = aNewReadChunk;
  // Missing assignment operator is bug 976927
  mUpmixChannels.ReplaceElementsAt(0, mUpmixChannels.Length(),
                                   mChunks[aNewReadChunk].mChannelData);
  MOZ_ASSERT(mUpmixChannels.Length() <= aChannelCount);
  if (mUpmixChannels.Length() < aChannelCount) {
    if (aChannelInterpretation == ChannelInterpretation::Speakers) {
      AudioChannelsUpMix(&mUpmixChannels, aChannelCount, silenceChannel);
      MOZ_ASSERT(mUpmixChannels.Length() == aChannelCount,
                 "We called GetAudioChannelsSuperset to avoid this");
    } else {
      // Fill up the remaining channels with zeros
      for (uint32_t channel = mUpmixChannels.Length();
           channel < aChannelCount; ++channel) {
        mUpmixChannels.AppendElement(silenceChannel);
      }
    }
  }
}

} // mozilla
