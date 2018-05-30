/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioPacketizer_h_
#define AudioPacketizer_h_

#include <mozilla/PodOperations.h>
#include <mozilla/Assertions.h>
#include <mozilla/UniquePtr.h>
#include <AudioSampleFormat.h>

// Enable this to warn when `Output` has been called but not enough data was
// buffered.
// #define LOG_PACKETIZER_UNDERRUN

namespace mozilla {
/**
 * This class takes arbitrary input data, and returns packets of a specific
 * size. In the process, it can convert audio samples from 16bit integers to
 * float (or vice-versa).
 *
 * Input and output, as well as length units in the public interface are
 * interleaved frames.
 *
 * Allocations of output buffer can be performed by this class.  Buffers can
 * simply be delete-d.  This is because packets are intended to be sent off to
 * non-gecko code using normal pointers/length pairs
 *
 * Alternatively, consumers can pass in a buffer in which the output is copied.
 * The buffer needs to be large enough to store a packet worth of audio.
 *
 * The implementation uses a circular buffer using absolute virtual indices.
 */
template <typename InputType, typename OutputType>
class AudioPacketizer
{
public:
  AudioPacketizer(uint32_t aPacketSize, uint32_t aChannels)
    : mPacketSize(aPacketSize)
    , mChannels(aChannels)
    , mReadIndex(0)
    , mWriteIndex(0)
    // Start off with a single packet
    , mStorage(new InputType[aPacketSize * aChannels])
    , mLength(aPacketSize * aChannels)
  {
     MOZ_ASSERT(aPacketSize > 0 && aChannels > 0,
       "The packet size and the number of channel should be strictly positive");
  }

  void Input(const InputType* aFrames, uint32_t aFrameCount)
  {
    uint32_t inputSamples = aFrameCount * mChannels;
    // Need to grow the storage. This should rarely happen, if at all, once the
    // array has the right size.
    if (inputSamples > EmptySlots()) {
      // Calls to Input and Output are roughtly interleaved
      // (Input,Output,Input,Output, etc.), or balanced
      // (Input,Input,Input,Output,Output,Output), so we update the buffer to
      // the exact right size in order to not waste space.
      uint32_t newLength = AvailableSamples() + inputSamples;
      uint32_t toCopy = AvailableSamples();
      UniquePtr<InputType[]> oldStorage = std::move(mStorage);
      mStorage = mozilla::MakeUnique<InputType[]>(newLength);
      // Copy the old data at the beginning of the new storage.
      if (WriteIndex() >= ReadIndex()) {
        PodCopy(mStorage.get(),
                oldStorage.get() + ReadIndex(),
                AvailableSamples());
      } else {
        uint32_t firstPartLength = mLength - ReadIndex();
        uint32_t secondPartLength = AvailableSamples() - firstPartLength;
        PodCopy(mStorage.get(),
                oldStorage.get() + ReadIndex(),
                firstPartLength);
        PodCopy(mStorage.get() + firstPartLength,
                oldStorage.get(),
                secondPartLength);
      }
      mWriteIndex = toCopy;
      mReadIndex = 0;
      mLength = newLength;
    }

    if (WriteIndex() + inputSamples <= mLength) {
      PodCopy(mStorage.get() + WriteIndex(), aFrames, aFrameCount * mChannels);
    } else {
      uint32_t firstPartLength = mLength - WriteIndex();
      uint32_t secondPartLength = inputSamples - firstPartLength;
      PodCopy(mStorage.get() + WriteIndex(), aFrames, firstPartLength);
      PodCopy(mStorage.get(), aFrames + firstPartLength, secondPartLength);
    }

    mWriteIndex += inputSamples;
  }

  OutputType* Output()
  {
    uint32_t samplesNeeded = mPacketSize * mChannels;
    OutputType* out = new OutputType[samplesNeeded];

    Output(out);

    return out;
  }

  void Output(OutputType* aOutputBuffer)
  {
    uint32_t samplesNeeded = mPacketSize * mChannels;

    // Under-run. Pad the end of the buffer with silence.
    if (AvailableSamples() < samplesNeeded) {
#ifdef LOG_PACKETIZER_UNDERRUN
      char buf[256];
      snprintf(buf, 256,
               "AudioPacketizer %p underrun: available: %u, needed: %u\n",
               this, AvailableSamples(), samplesNeeded);
      NS_WARNING(buf);
#endif
      uint32_t zeros = samplesNeeded - AvailableSamples();
      PodZero(aOutputBuffer + AvailableSamples(), zeros);
      samplesNeeded -= zeros;
    }
    if (ReadIndex() + samplesNeeded <= mLength) {
      ConvertAudioSamples<InputType,OutputType>(mStorage.get() + ReadIndex(),
                                                aOutputBuffer,
                                                samplesNeeded);
    } else {
      uint32_t firstPartLength = mLength - ReadIndex();
      uint32_t secondPartLength = samplesNeeded - firstPartLength;
      ConvertAudioSamples<InputType, OutputType>(mStorage.get() + ReadIndex(),
                                                 aOutputBuffer,
                                                 firstPartLength);
      ConvertAudioSamples<InputType, OutputType>(mStorage.get(),
                                                 aOutputBuffer + firstPartLength,
                                                 secondPartLength);
    }
    mReadIndex += samplesNeeded;
  }

  uint32_t PacketsAvailable() const {
    return AvailableSamples() / mChannels / mPacketSize;
  }

  bool Empty() const {
   return mWriteIndex == mReadIndex;
  }

  bool Full() const {
    return mWriteIndex - mReadIndex == mLength;
  }

  uint32_t PacketSize() const {
    return mPacketSize;
  }

  uint32_t Channels() const {
    return mChannels;
  }

private:
  uint32_t ReadIndex() const {
    return mReadIndex % mLength;
  }

  uint32_t WriteIndex() const {
    return mWriteIndex % mLength;
  }

  uint32_t AvailableSamples() const {
    return mWriteIndex - mReadIndex;
  }

  uint32_t EmptySlots() const {
    return mLength - AvailableSamples();
  }

  // Size of one packet of audio, in frames
  uint32_t mPacketSize;
  // Number of channels of the stream flowing through this packetizer
  uint32_t mChannels;
  // Two virtual index into the buffer: the read position and the write
  // position.
  uint64_t mReadIndex;
  uint64_t mWriteIndex;
  // Storage for the samples
  mozilla::UniquePtr<InputType[]> mStorage;
  // Length of the buffer, in samples
  uint32_t mLength;
};

} // mozilla

#endif // AudioPacketizer_h_
