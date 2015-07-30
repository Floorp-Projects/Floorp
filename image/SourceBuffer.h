/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * SourceBuffer is a single producer, multiple consumer data structure used for
 * storing image source (compressed) data.
 */

#ifndef mozilla_image_sourcebuffer_h
#define mozilla_image_sourcebuffer_h

#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Mutex.h"
#include "mozilla/Move.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/nsRefPtr.h"
#include "nsTArray.h"

namespace mozilla {
namespace image {

class SourceBuffer;

/**
 * IResumable is an interface for classes that can schedule themselves to resume
 * their work later. An implementation of IResumable generally should post a
 * runnable to some event target which continues the work of the task.
 */
struct IResumable
{
  MOZ_DECLARE_REFCOUNTED_TYPENAME(IResumable)

  // Subclasses may or may not be XPCOM classes, so we just require that they
  // implement AddRef and Release.
  NS_IMETHOD_(MozExternalRefCountType) AddRef(void) = 0;
  NS_IMETHOD_(MozExternalRefCountType) Release(void) = 0;

  virtual void Resume() = 0;

protected:
  virtual ~IResumable() { }
};

/**
 * SourceBufferIterator is a class that allows consumers of image source data to
 * read the contents of a SourceBuffer sequentially.
 *
 * Consumers can advance through the SourceBuffer by calling
 * AdvanceOrScheduleResume() repeatedly. After every advance, they should call
 * check the return value, which will tell them the iterator's new state.
 *
 * If WAITING is returned, AdvanceOrScheduleResume() has arranged
 * to call the consumer's Resume() method later, so the consumer should save its
 * state if needed and stop running.
 *
 * If the iterator's new state is READY, then the consumer can call Data() and
 * Length() to read new data from the SourceBuffer.
 *
 * Finally, in the COMPLETE state the consumer can call CompletionStatus() to
 * get the status passed to SourceBuffer::Complete().
 */
class SourceBufferIterator final
{
public:
  enum State {
    START,    // The iterator is at the beginning of the buffer.
    READY,    // The iterator is pointing to new data.
    WAITING,  // The iterator is blocked and the caller must yield.
    COMPLETE  // The iterator is pointing to the end of the buffer.
  };

  explicit SourceBufferIterator(SourceBuffer* aOwner)
    : mOwner(aOwner)
    , mState(START)
  {
    MOZ_ASSERT(aOwner);
    mData.mIterating.mChunk = 0;
    mData.mIterating.mData = nullptr;
    mData.mIterating.mOffset = 0;
    mData.mIterating.mLength = 0;
  }

  SourceBufferIterator(SourceBufferIterator&& aOther)
    : mOwner(Move(aOther.mOwner))
    , mState(aOther.mState)
    , mData(aOther.mData)
  { }

  ~SourceBufferIterator();

  SourceBufferIterator& operator=(SourceBufferIterator&& aOther)
  {
    mOwner = Move(aOther.mOwner);
    mState = aOther.mState;
    mData = aOther.mData;
    return *this;
  }

  /**
   * Returns true if there are no more than @aBytes remaining in the
   * SourceBuffer. If the SourceBuffer is not yet complete, returns false.
   */
  bool RemainingBytesIsNoMoreThan(size_t aBytes) const;

  /**
   * Advances the iterator through the SourceBuffer if possible. If not,
   * arranges to call the @aConsumer's Resume() method when more data is
   * available.
   */
  State AdvanceOrScheduleResume(IResumable* aConsumer);

  /// If at the end, returns the status passed to SourceBuffer::Complete().
  nsresult CompletionStatus() const
  {
    MOZ_ASSERT(mState == COMPLETE,
               "Calling CompletionStatus() in the wrong state");
    return mState == COMPLETE ? mData.mAtEnd.mStatus : NS_OK;
  }

  /// If we're ready to read, returns a pointer to the new data.
  const char* Data() const
  {
    MOZ_ASSERT(mState == READY, "Calling Data() in the wrong state");
    return mState == READY ? mData.mIterating.mData + mData.mIterating.mOffset
                           : nullptr;
  }

  /// If we're ready to read, returns the length of the new data.
  size_t Length() const
  {
    MOZ_ASSERT(mState == READY, "Calling Length() in the wrong state");
    return mState == READY ? mData.mIterating.mLength : 0;
  }

private:
  friend class SourceBuffer;

  SourceBufferIterator(const SourceBufferIterator&) = delete;
  SourceBufferIterator& operator=(const SourceBufferIterator&) = delete;

  bool HasMore() const { return mState != COMPLETE; }

  State SetReady(uint32_t aChunk, const char* aData,
                size_t aOffset, size_t aLength)
  {
    MOZ_ASSERT(mState != COMPLETE);
    mData.mIterating.mChunk = aChunk;
    mData.mIterating.mData = aData;
    mData.mIterating.mOffset = aOffset;
    mData.mIterating.mLength = aLength;
    return mState = READY;
  }

  State SetWaiting()
  {
    MOZ_ASSERT(mState != COMPLETE);
    MOZ_ASSERT(mState != WAITING, "Did we get a spurious wakeup somehow?");
    return mState = WAITING;
  }

  State SetComplete(nsresult aStatus)
  {
    mData.mAtEnd.mStatus = aStatus;
    return mState = COMPLETE;
  }

  nsRefPtr<SourceBuffer> mOwner;

  State mState;

  /**
   * This union contains our iteration state if we're still iterating (for
   * states START, READY, and WAITING) and the status the SourceBuffer was
   * completed with if we're in state COMPLETE.
   */
  union {
    struct {
      uint32_t mChunk;
      const char* mData;
      size_t mOffset;
      size_t mLength;
    } mIterating;
    struct {
      nsresult mStatus;
    } mAtEnd;
  } mData;
};

/**
 * SourceBuffer is a parallel data structure used for storing image source
 * (compressed) data.
 *
 * SourceBuffer is a single producer, multiple consumer data structure. The
 * single producer calls Append() to append data to the buffer. In parallel,
 * multiple consumers can call Iterator(), which returns a SourceBufferIterator
 * that they can use to iterate through the buffer. The SourceBufferIterator
 * returns a series of pointers which remain stable for lifetime of the
 * SourceBuffer, and the data they point to is immutable, ensuring that the
 * producer never interferes with the consumers.
 *
 * In order to avoid blocking, SourceBuffer works with SourceBufferIterator to
 * keep a list of consumers which are waiting for new data, and to resume them
 * when the producer appends more. All consumers must implement the IResumable
 * interface to make this possible.
 *
 * XXX(seth): We should add support for compacting a SourceBuffer. To do this,
 * we need to have SourceBuffer keep track of how many live
 * SourceBufferIterator's point to it. When the SourceBuffer is complete and no
 * live SourceBufferIterator's for it remain, we can compact its contents into a
 * single chunk.
 */
class SourceBuffer final
{
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(image::SourceBuffer)
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(image::SourceBuffer)

  SourceBuffer();

  //////////////////////////////////////////////////////////////////////////////
  // Producer methods.
  //////////////////////////////////////////////////////////////////////////////

  /**
   * If the producer knows how long the source data will be, it should call
   * ExpectLength, which enables SourceBuffer to preallocate its buffer.
   */
  nsresult ExpectLength(size_t aExpectedLength);

  /// Append the provided data to the buffer.
  nsresult Append(const char* aData, size_t aLength);

  /**
   * Mark the buffer complete, with a status that will be available to
   * consumers. Further calls to Append() are forbidden after Complete().
   */
  void Complete(nsresult aStatus);

  /// Returns true if the buffer is complete.
  bool IsComplete();

  /// Memory reporting.
  size_t SizeOfIncludingThisWithComputedFallback(MallocSizeOf) const;


  //////////////////////////////////////////////////////////////////////////////
  // Consumer methods.
  //////////////////////////////////////////////////////////////////////////////

  /// Returns an iterator to this SourceBuffer.
  SourceBufferIterator Iterator();


private:
  friend class SourceBufferIterator;

  ~SourceBuffer();

  //////////////////////////////////////////////////////////////////////////////
  // Chunk type and chunk-related methods.
  //////////////////////////////////////////////////////////////////////////////

  class Chunk
  {
  public:
    explicit Chunk(size_t aCapacity)
      : mCapacity(aCapacity)
      , mLength(0)
    {
      MOZ_ASSERT(aCapacity > 0, "Creating zero-capacity chunk");
      mData = new (fallible) char[mCapacity];
    }

    ~Chunk() { delete[] mData; }

    Chunk(Chunk&& aOther)
      : mCapacity(aOther.mCapacity)
      , mLength(aOther.mLength)
      , mData(aOther.mData)
    {
      aOther.mCapacity = aOther.mLength = 0;
      aOther.mData = nullptr;
    }

    Chunk& operator=(Chunk&& aOther)
    {
      mCapacity = aOther.mCapacity;
      mLength = aOther.mLength;
      mData = aOther.mData;
      aOther.mCapacity = aOther.mLength = 0;
      aOther.mData = nullptr;
      return *this;
    }

    bool AllocationFailed() const { return !mData; }
    size_t Capacity() const { return mCapacity; }
    size_t Length() const { return mLength; }

    char* Data() const
    {
      MOZ_ASSERT(mData, "Allocation failed but nobody checked for it");
      return mData;
    }

    void AddLength(size_t aAdditionalLength)
    {
      MOZ_ASSERT(mLength + aAdditionalLength <= mCapacity);
      mLength += aAdditionalLength;
    }

  private:
    Chunk(const Chunk&) = delete;
    Chunk& operator=(const Chunk&) = delete;

    size_t mCapacity;
    size_t mLength;
    char* mData;
  };

  nsresult AppendChunk(Maybe<Chunk>&& aChunk);
  Maybe<Chunk> CreateChunk(size_t aCapacity, bool aRoundUp = true);
  nsresult Compact();
  static size_t RoundedUpCapacity(size_t aCapacity);
  size_t FibonacciCapacityWithMinimum(size_t aMinCapacity);


  //////////////////////////////////////////////////////////////////////////////
  // Iterator / consumer methods.
  //////////////////////////////////////////////////////////////////////////////

  void AddWaitingConsumer(IResumable* aConsumer);
  void ResumeWaitingConsumers();

  typedef SourceBufferIterator::State State;

  State AdvanceIteratorOrScheduleResume(SourceBufferIterator& aIterator,
                                        IResumable* aConsumer);
  bool RemainingBytesIsNoMoreThan(const SourceBufferIterator& aIterator,
                                  size_t aBytes) const;

  void OnIteratorRelease();

  //////////////////////////////////////////////////////////////////////////////
  // Helper methods.
  //////////////////////////////////////////////////////////////////////////////

  nsresult HandleError(nsresult aError);
  bool IsEmpty();
  bool IsLastChunk(uint32_t aChunk);


  //////////////////////////////////////////////////////////////////////////////
  // Member variables.
  //////////////////////////////////////////////////////////////////////////////

  static const size_t MIN_CHUNK_CAPACITY = 4096;

  /// All private members are protected by mMutex.
  mutable Mutex mMutex;

  /// The data in this SourceBuffer, stored as a series of Chunks.
  FallibleTArray<Chunk> mChunks;

  /// Consumers which are waiting to be notified when new data is available.
  nsTArray<nsRefPtr<IResumable>> mWaitingConsumers;

  /// If present, marks this SourceBuffer complete with the given final status.
  Maybe<nsresult> mStatus;

  /// Count of active consumers.
  uint32_t mConsumerCount;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_sourcebuffer_h
