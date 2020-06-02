/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioRingBuffer.h"

#include "MediaData.h"
#include "mozilla/Assertions.h"
#include "mozilla/Maybe.h"
#include "mozilla/PodOperations.h"

namespace mozilla {

/**
 * RingBuffer is used to preallocate a buffer of a specific size in bytes and
 * then to use it for writing and reading values without any re-allocation or
 * memory moving. Please note that the total byte size of the buffer modulo the
 * size of the chosen type must be zero. The RingBuffer has been created with
 * audio sample values types in mind which are integer or float. However, it
 * can be used with any trivial type. It is _not_ thread-safe! The constructor
 * can be called on any thread but the reads and write must happen on the same
 * thread, which can be different than the construction thread.
 */
template <typename T>
class RingBuffer final {
 public:
  explicit RingBuffer(AlignedByteBuffer&& aMemoryBuffer)
      : mStorage(ConvertToSpan(aMemoryBuffer)),
        mMemoryBuffer(std::move(aMemoryBuffer)) {
    MOZ_ASSERT(std::is_trivial<T>::value);
    MOZ_ASSERT(!mStorage.IsEmpty());
  }

  /**
   * Write `aSamples` number of zeros in the buffer.
   */
  int WriteSilence(int aSamples) {
    MOZ_ASSERT(aSamples);
    return Write(Span<T>(), aSamples);
  }

  /**
   * Copy `aBuffer` to the RingBuffer.
   */
  int Write(const Span<const T>& aBuffer) {
    MOZ_ASSERT(!aBuffer.IsEmpty());
    return Write(aBuffer, aBuffer.Length());
  }

 private:
  /**
   * Copy `aSamples` number of elements from `aBuffer` to the RingBuffer. If
   * `aBuffer` is empty append `aSamples` of zeros.
   */
  int Write(const Span<const T>& aBuffer, int aSamples) {
    MOZ_ASSERT(aSamples > 0 &&
               aBuffer.Length() <= static_cast<uint32_t>(aSamples));

    if (IsFull()) {
      return 0;
    }

    int toWrite = std::min(AvailableWrite(), aSamples);
    int part1 = std::min(Capacity() - mWriteIndex, toWrite);
    int part2 = toWrite - part1;

    Span<T> part1Buffer = mStorage.Subspan(mWriteIndex, part1);
    Span<T> part2Buffer = mStorage.To(part2);

    if (!aBuffer.IsEmpty()) {
      Span<const T> fromPart1 = aBuffer.To(part1);
      Span<const T> fromPart2 = aBuffer.Subspan(part1, part2);

      CopySpan(part1Buffer, fromPart1);
      CopySpan(part2Buffer, fromPart2);
    } else {
      // The aBuffer is empty, append zeros.
      PodZero(part1Buffer.Elements(), part1Buffer.Length());
      PodZero(part2Buffer.Elements(), part2Buffer.Length());
    }

    mWriteIndex = NextIndex(mWriteIndex, toWrite);

    return toWrite;
  }

 public:
  /**
   * Copy `aSamples` number of elements from `aBuffer` to the RingBuffer. The
   * `aBuffer` does not change.
   */
  int Write(const RingBuffer& aBuffer, int aSamples) {
    MOZ_ASSERT(aSamples);

    if (IsFull()) {
      return 0;
    }

    int toWriteThis = std::min(AvailableWrite(), aSamples);
    int toReadThat = std::min(aBuffer.AvailableRead(), toWriteThis);
    int part1 = std::min(aBuffer.Capacity() - aBuffer.mReadIndex, toReadThat);
    int part2 = toReadThat - part1;

    Span<T> part1Buffer = aBuffer.mStorage.Subspan(aBuffer.mReadIndex, part1);
    DebugOnly<int> ret = Write(part1Buffer);
    MOZ_ASSERT(ret == part1);
    if (part2) {
      Span<T> part2Buffer = aBuffer.mStorage.To(part2);
      ret = Write(part2Buffer);
      MOZ_ASSERT(ret == part2);
    }

    return toReadThat;
  }

  /**
   * Copy `aBuffer.Length()` number of elements from RingBuffer to `aBuffer`.
   */
  int Read(const Span<T>& aBuffer) {
    MOZ_ASSERT(!aBuffer.IsEmpty());
    MOZ_ASSERT(aBuffer.size() <= std::numeric_limits<int>::max());

    if (IsEmpty()) {
      return 0;
    }

    int toRead = std::min(AvailableRead(), static_cast<int>(aBuffer.Length()));
    int part1 = std::min(Capacity() - mReadIndex, toRead);
    int part2 = toRead - part1;

    Span<T> part1Buffer = mStorage.Subspan(mReadIndex, part1);
    Span<T> part2Buffer = mStorage.To(part2);

    Span<T> toPart1 = aBuffer.To(part1);
    Span<T> toPart2 = aBuffer.Subspan(part1, part2);

    CopySpan(toPart1, part1Buffer);
    CopySpan(toPart2, part2Buffer);

    mReadIndex = NextIndex(mReadIndex, toRead);

    return toRead;
  }

  /**
   * Provide `aCallable` that will be called with the internal linear read
   * buffers and the number of samples available for reading. The `aCallable`
   * will be called at most 2 times. The `aCallable` must return the number of
   * samples that have been actually read. If that number is smaller than the
   * available number of samples, provided in the argument, the `aCallable` will
   * not be called again. The RingBuffer's available read samples will be
   * decreased by the number returned from the `aCallable`.
   *
   * The important aspects of this method are that first, it makes it possible
   * to avoid extra copies to an intermediates buffer, and second, each buffer
   * provided to `aCallable is a linear piece of memory which can be used
   * directly to a resampler for example.
   *
   * In general, the problem with ring buffers is that they cannot provide one
   * linear chunk of memory so extra copies, to a linear buffer, are often
   * needed. This method bridge that gap by breaking the ring buffer's
   * internal read memory into linear pieces and making it available through
   * the `aCallable`. In the body of the `aCallable` those buffers can be used
   * directly without any copy or intermediate steps.
   */
  int ReadNoCopy(std::function<int(const Span<const T>&)>&& aCallable) {
    if (IsEmpty()) {
      return 0;
    }

    int part1 = std::min(Capacity() - mReadIndex, AvailableRead());
    int part2 = AvailableRead() - part1;

    Span<T> part1Buffer = mStorage.Subspan(mReadIndex, part1);
    int toRead = aCallable(part1Buffer);
    MOZ_ASSERT(toRead <= part1);

    if (toRead == part1 && part2) {
      Span<T> part2Buffer = mStorage.To(part2);
      toRead += aCallable(part2Buffer);
      MOZ_ASSERT(toRead <= part1 + part2);
    }

    mReadIndex = NextIndex(mReadIndex, toRead);

    return toRead;
  }

  /**
   * Remove the next `aSamples` number of samples from the ring buffer.
   */
  int Discard(int aSamples) {
    MOZ_ASSERT(aSamples);

    if (IsEmpty()) {
      return 0;
    }

    int toDiscard = std::min(AvailableRead(), aSamples);
    mReadIndex = NextIndex(mReadIndex, toDiscard);

    return toDiscard;
  }

  /**
   * Empty the ring buffer.
   */
  int Clear() {
    if (IsEmpty()) {
      return 0;
    }

    int toDiscard = AvailableRead();
    mReadIndex = NextIndex(mReadIndex, toDiscard);

    return toDiscard;
  }

  /**
   * Returns true if the full capacity of the ring buffer is being used. When
   * full any attempt to write more samples to the ring buffer will fail.
   */
  bool IsFull() const { return (mWriteIndex + 1) % Capacity() == mReadIndex; }

  /**
   * Returns true if the ring buffer is empty. When empty any attempt to read
   * more samples from the ring buffer will fail.
   */
  bool IsEmpty() const { return mWriteIndex == mReadIndex; }

  /**
   * The number of samples available for writing.
   */
  int AvailableWrite() const {
    /* We subtract one element here to always keep at least one sample
     * free in the buffer, to distinguish between full and empty array. */
    int rv = mReadIndex - mWriteIndex - 1;
    if (mWriteIndex >= mReadIndex) {
      rv += Capacity();
    }
    return rv;
  }

  /**
   * The number of samples available for reading.
   */
  int AvailableRead() const {
    if (mWriteIndex >= mReadIndex) {
      return mWriteIndex - mReadIndex;
    }
    return mWriteIndex + Capacity() - mReadIndex;
  }

 private:
  int NextIndex(int aIndex, int aStep) const {
    MOZ_ASSERT(aStep >= 0);
    MOZ_ASSERT(aStep < Capacity());
    MOZ_ASSERT(aIndex < Capacity());
    return (aIndex + aStep) % Capacity();
  }

  int32_t Capacity() const { return mStorage.Length(); }

  Span<T> ConvertToSpan(const AlignedByteBuffer& aOther) const {
    MOZ_ASSERT(aOther.Length() >= sizeof(T));
    return Span<T>(reinterpret_cast<T*>(aOther.Data()),
                   aOther.Length() / sizeof(T));
  }

  void CopySpan(Span<T>& aTo, const Span<const T>& aFrom) {
    MOZ_ASSERT(aTo.Length() == aFrom.Length());
    std::copy(aFrom.cbegin(), aFrom.cend(), aTo.begin());
  }

 private:
  int mReadIndex = 0;
  int mWriteIndex = 0;
  /* Points to the mMemoryBuffer. */
  const Span<T> mStorage;
  /* The actual allocated memory set from outside. It is set in the ctor and it
   * is not used again. It is here to control the lifetime of the memory. The
   * memory is accessed through the mStorage. The idea is that the memory used
   * from the RingBuffer can be pre-allocated. */
  const AlignedByteBuffer mMemoryBuffer;
};

/** AudioRingBuffer **/

/* The private members of AudioRingBuffer. */
class AudioRingBuffer::AudioRingBufferPrivate {
 public:
  AudioSampleFormat mSampleFormat = AUDIO_FORMAT_SILENCE;
  Maybe<RingBuffer<float>> mFloatRingBuffer;
  Maybe<RingBuffer<int16_t>> mIntRingBuffer;
  Maybe<AlignedByteBuffer> mBackingBuffer;
};

AudioRingBuffer::AudioRingBuffer(int aSizeInBytes)
    : mPtr(MakeUnique<AudioRingBufferPrivate>()) {
  MOZ_ASSERT(aSizeInBytes > 0);
  MOZ_ASSERT(aSizeInBytes < std::numeric_limits<int>::max());
  mPtr->mBackingBuffer.emplace(aSizeInBytes);
  MOZ_ASSERT(mPtr->mBackingBuffer);
}

AudioRingBuffer::~AudioRingBuffer() = default;

void AudioRingBuffer::SetSampleFormat(AudioSampleFormat aFormat) {
  MOZ_ASSERT(mPtr->mSampleFormat == AUDIO_FORMAT_SILENCE);
  MOZ_ASSERT(aFormat == AUDIO_FORMAT_S16 || aFormat == AUDIO_FORMAT_FLOAT32);
  MOZ_ASSERT(!mPtr->mIntRingBuffer);
  MOZ_ASSERT(!mPtr->mFloatRingBuffer);
  MOZ_ASSERT(mPtr->mBackingBuffer);

  mPtr->mSampleFormat = aFormat;
  if (mPtr->mSampleFormat == AUDIO_FORMAT_S16) {
    mPtr->mIntRingBuffer.emplace(mPtr->mBackingBuffer.extract());
    MOZ_ASSERT(!mPtr->mBackingBuffer);
    return;
  }
  mPtr->mFloatRingBuffer.emplace(mPtr->mBackingBuffer.extract());
  MOZ_ASSERT(!mPtr->mBackingBuffer);
}

int AudioRingBuffer::Write(const Span<const float>& aBuffer) {
  MOZ_ASSERT(mPtr->mSampleFormat == AUDIO_FORMAT_FLOAT32);
  MOZ_ASSERT(!mPtr->mIntRingBuffer);
  MOZ_ASSERT(!mPtr->mBackingBuffer);
  return mPtr->mFloatRingBuffer->Write(aBuffer);
}

int AudioRingBuffer::Write(const Span<const int16_t>& aBuffer) {
  MOZ_ASSERT(mPtr->mSampleFormat == AUDIO_FORMAT_S16);
  MOZ_ASSERT(!mPtr->mFloatRingBuffer);
  MOZ_ASSERT(!mPtr->mBackingBuffer);
  return mPtr->mIntRingBuffer->Write(aBuffer);
}

int AudioRingBuffer::Write(const AudioRingBuffer& aBuffer, int aSamples) {
  MOZ_ASSERT(mPtr->mSampleFormat == AUDIO_FORMAT_S16 ||
             mPtr->mSampleFormat == AUDIO_FORMAT_FLOAT32);
  MOZ_ASSERT(!mPtr->mBackingBuffer);
  if (mPtr->mSampleFormat == AUDIO_FORMAT_S16) {
    MOZ_ASSERT(!mPtr->mFloatRingBuffer);
    return mPtr->mIntRingBuffer->Write(aBuffer.mPtr->mIntRingBuffer.ref(),
                                       aSamples);
  }
  MOZ_ASSERT(!mPtr->mIntRingBuffer);
  return mPtr->mFloatRingBuffer->Write(aBuffer.mPtr->mFloatRingBuffer.ref(),
                                       aSamples);
}

int AudioRingBuffer::WriteSilence(int aSamples) {
  MOZ_ASSERT(mPtr->mSampleFormat == AUDIO_FORMAT_S16 ||
             mPtr->mSampleFormat == AUDIO_FORMAT_FLOAT32);
  MOZ_ASSERT(!mPtr->mBackingBuffer);
  if (mPtr->mSampleFormat == AUDIO_FORMAT_S16) {
    MOZ_ASSERT(!mPtr->mFloatRingBuffer);
    return mPtr->mIntRingBuffer->WriteSilence(aSamples);
  }
  MOZ_ASSERT(!mPtr->mIntRingBuffer);
  return mPtr->mFloatRingBuffer->WriteSilence(aSamples);
}

int AudioRingBuffer::Read(const Span<float>& aBuffer) {
  MOZ_ASSERT(mPtr->mSampleFormat == AUDIO_FORMAT_FLOAT32);
  MOZ_ASSERT(!mPtr->mIntRingBuffer);
  MOZ_ASSERT(!mPtr->mBackingBuffer);
  return mPtr->mFloatRingBuffer->Read(aBuffer);
}

int AudioRingBuffer::Read(const Span<int16_t>& aBuffer) {
  MOZ_ASSERT(mPtr->mSampleFormat == AUDIO_FORMAT_S16);
  MOZ_ASSERT(!mPtr->mFloatRingBuffer);
  MOZ_ASSERT(!mPtr->mBackingBuffer);
  return mPtr->mIntRingBuffer->Read(aBuffer);
}

int AudioRingBuffer::ReadNoCopy(
    std::function<int(const Span<const float>&)>&& aCallable) {
  MOZ_ASSERT(mPtr->mSampleFormat == AUDIO_FORMAT_FLOAT32);
  MOZ_ASSERT(!mPtr->mIntRingBuffer);
  MOZ_ASSERT(!mPtr->mBackingBuffer);
  return mPtr->mFloatRingBuffer->ReadNoCopy(std::move(aCallable));
}

int AudioRingBuffer::ReadNoCopy(
    std::function<int(const Span<const int16_t>&)>&& aCallable) {
  MOZ_ASSERT(mPtr->mSampleFormat == AUDIO_FORMAT_S16);
  MOZ_ASSERT(!mPtr->mFloatRingBuffer);
  MOZ_ASSERT(!mPtr->mBackingBuffer);
  return mPtr->mIntRingBuffer->ReadNoCopy(std::move(aCallable));
}

int AudioRingBuffer::Discard(int aSamples) {
  MOZ_ASSERT(mPtr->mSampleFormat == AUDIO_FORMAT_S16 ||
             mPtr->mSampleFormat == AUDIO_FORMAT_FLOAT32);
  MOZ_ASSERT(!mPtr->mBackingBuffer);
  if (mPtr->mSampleFormat == AUDIO_FORMAT_S16) {
    MOZ_ASSERT(!mPtr->mFloatRingBuffer);
    return mPtr->mIntRingBuffer->Discard(aSamples);
  }
  MOZ_ASSERT(!mPtr->mIntRingBuffer);
  return mPtr->mFloatRingBuffer->Discard(aSamples);
}

int AudioRingBuffer::Clear() {
  MOZ_ASSERT(mPtr->mSampleFormat == AUDIO_FORMAT_S16 ||
             mPtr->mSampleFormat == AUDIO_FORMAT_FLOAT32);
  MOZ_ASSERT(!mPtr->mBackingBuffer);
  if (mPtr->mSampleFormat == AUDIO_FORMAT_S16) {
    MOZ_ASSERT(!mPtr->mFloatRingBuffer);
    MOZ_ASSERT(mPtr->mIntRingBuffer);
    return mPtr->mIntRingBuffer->Clear();
  }
  MOZ_ASSERT(!mPtr->mIntRingBuffer);
  MOZ_ASSERT(mPtr->mFloatRingBuffer);
  return mPtr->mFloatRingBuffer->Clear();
}

bool AudioRingBuffer::IsFull() const {
  MOZ_ASSERT(mPtr->mSampleFormat == AUDIO_FORMAT_S16 ||
             mPtr->mSampleFormat == AUDIO_FORMAT_FLOAT32);
  MOZ_ASSERT(!mPtr->mBackingBuffer);
  if (mPtr->mSampleFormat == AUDIO_FORMAT_S16) {
    MOZ_ASSERT(!mPtr->mFloatRingBuffer);
    return mPtr->mIntRingBuffer->IsFull();
  }
  MOZ_ASSERT(!mPtr->mIntRingBuffer);
  return mPtr->mFloatRingBuffer->IsFull();
}

bool AudioRingBuffer::IsEmpty() const {
  MOZ_ASSERT(mPtr->mSampleFormat == AUDIO_FORMAT_S16 ||
             mPtr->mSampleFormat == AUDIO_FORMAT_FLOAT32);
  MOZ_ASSERT(!mPtr->mBackingBuffer);
  if (mPtr->mSampleFormat == AUDIO_FORMAT_S16) {
    MOZ_ASSERT(!mPtr->mFloatRingBuffer);
    return mPtr->mIntRingBuffer->IsEmpty();
  }
  MOZ_ASSERT(!mPtr->mIntRingBuffer);
  return mPtr->mFloatRingBuffer->IsEmpty();
}

int AudioRingBuffer::AvailableWrite() const {
  MOZ_ASSERT(mPtr->mSampleFormat == AUDIO_FORMAT_S16 ||
             mPtr->mSampleFormat == AUDIO_FORMAT_FLOAT32);
  MOZ_ASSERT(!mPtr->mBackingBuffer);
  if (mPtr->mSampleFormat == AUDIO_FORMAT_S16) {
    MOZ_ASSERT(!mPtr->mFloatRingBuffer);
    return mPtr->mIntRingBuffer->AvailableWrite();
  }
  MOZ_ASSERT(!mPtr->mIntRingBuffer);
  return mPtr->mFloatRingBuffer->AvailableWrite();
}

int AudioRingBuffer::AvailableRead() const {
  MOZ_ASSERT(mPtr->mSampleFormat == AUDIO_FORMAT_S16 ||
             mPtr->mSampleFormat == AUDIO_FORMAT_FLOAT32);
  MOZ_ASSERT(!mPtr->mBackingBuffer);
  if (mPtr->mSampleFormat == AUDIO_FORMAT_S16) {
    MOZ_ASSERT(!mPtr->mFloatRingBuffer);
    return mPtr->mIntRingBuffer->AvailableRead();
  }
  MOZ_ASSERT(!mPtr->mIntRingBuffer);
  return mPtr->mFloatRingBuffer->AvailableRead();
}

}  // namespace mozilla
