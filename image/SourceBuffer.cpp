/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SourceBuffer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include "mozilla/Likely.h"
#include "nsIInputStream.h"
#include "MainThreadUtils.h"
#include "SurfaceCache.h"

using std::max;
using std::min;

namespace mozilla {
namespace image {

//////////////////////////////////////////////////////////////////////////////
// SourceBufferIterator implementation.
//////////////////////////////////////////////////////////////////////////////

SourceBufferIterator::~SourceBufferIterator()
{
  if (mOwner) {
    mOwner->OnIteratorRelease();
  }
}

SourceBufferIterator&
SourceBufferIterator::operator=(SourceBufferIterator&& aOther)
{
  if (mOwner) {
    mOwner->OnIteratorRelease();
  }

  mOwner = std::move(aOther.mOwner);
  mState = aOther.mState;
  mData = aOther.mData;
  mChunkCount = aOther.mChunkCount;
  mByteCount = aOther.mByteCount;
  mRemainderToRead = aOther.mRemainderToRead;

  return *this;
}

SourceBufferIterator::State
SourceBufferIterator::AdvanceOrScheduleResume(size_t aRequestedBytes,
                                              IResumable* aConsumer)
{
  MOZ_ASSERT(mOwner);

  if (MOZ_UNLIKELY(!HasMore())) {
    MOZ_ASSERT_UNREACHABLE("Should not advance a completed iterator");
    return COMPLETE;
  }

  // The range of data [mOffset, mOffset + mNextReadLength) has just been read
  // by the caller (or at least they don't have any interest in it), so consume
  // that data.
  MOZ_ASSERT(mData.mIterating.mNextReadLength <= mData.mIterating.mAvailableLength);
  mData.mIterating.mOffset += mData.mIterating.mNextReadLength;
  mData.mIterating.mAvailableLength -= mData.mIterating.mNextReadLength;

  // An iterator can have a limit imposed on it to read only a subset of a
  // source buffer. If it is present, we need to mimic the same behaviour as
  // the owning SourceBuffer.
  if (MOZ_UNLIKELY(mRemainderToRead != SIZE_MAX)) {
    MOZ_ASSERT(mData.mIterating.mNextReadLength <= mRemainderToRead);
    mRemainderToRead -= mData.mIterating.mNextReadLength;

    if (MOZ_UNLIKELY(mRemainderToRead == 0)) {
      mData.mIterating.mNextReadLength = 0;
      SetComplete(NS_OK);
      return COMPLETE;
    }

    if (MOZ_UNLIKELY(aRequestedBytes > mRemainderToRead)) {
      aRequestedBytes = mRemainderToRead;
    }
  }

  mData.mIterating.mNextReadLength = 0;

  if (MOZ_LIKELY(mState == READY)) {
    // If the caller wants zero bytes of data, that's easy enough; we just
    // configured ourselves for a zero-byte read above!  In theory we could do
    // this even in the START state, but it's not important for performance and
    // breaking the ability of callers to assert that the pointer returned by
    // Data() is non-null doesn't seem worth it.
    if (aRequestedBytes == 0) {
      MOZ_ASSERT(mData.mIterating.mNextReadLength == 0);
      return READY;
    }

    // Try to satisfy the request out of our local buffer. This is potentially
    // much faster than requesting data from our owning SourceBuffer because we
    // don't have to take the lock. Note that if we have anything at all in our
    // local buffer, we use it to satisfy the request; @aRequestedBytes is just
    // the *maximum* number of bytes we can return.
    if (mData.mIterating.mAvailableLength > 0) {
      return AdvanceFromLocalBuffer(aRequestedBytes);
    }
  }

  // Our local buffer is empty, so we'll have to request data from our owning
  // SourceBuffer.
  return mOwner->AdvanceIteratorOrScheduleResume(*this,
                                                 aRequestedBytes,
                                                 aConsumer);
}

bool
SourceBufferIterator::RemainingBytesIsNoMoreThan(size_t aBytes) const
{
  MOZ_ASSERT(mOwner);
  return mOwner->RemainingBytesIsNoMoreThan(*this, aBytes);
}


//////////////////////////////////////////////////////////////////////////////
// SourceBuffer implementation.
//////////////////////////////////////////////////////////////////////////////

const size_t SourceBuffer::MIN_CHUNK_CAPACITY;
const size_t SourceBuffer::MAX_CHUNK_CAPACITY;

SourceBuffer::SourceBuffer()
  : mMutex("image::SourceBuffer")
  , mConsumerCount(0)
  , mCompacted(false)
{ }

SourceBuffer::~SourceBuffer()
{
  MOZ_ASSERT(mConsumerCount == 0,
             "SourceBuffer destroyed with active consumers");
}

nsresult
SourceBuffer::AppendChunk(Maybe<Chunk>&& aChunk)
{
  mMutex.AssertCurrentThreadOwns();

#ifdef DEBUG
  if (mChunks.Length() > 0) {
    NS_WARNING("Appending an extra chunk for SourceBuffer");
  }
#endif

  if (MOZ_UNLIKELY(!aChunk)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (MOZ_UNLIKELY(aChunk->AllocationFailed())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (MOZ_UNLIKELY(!mChunks.AppendElement(std::move(*aChunk), fallible))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

Maybe<SourceBuffer::Chunk>
SourceBuffer::CreateChunk(size_t aCapacity,
                          size_t aExistingCapacity /* = 0 */,
                          bool aRoundUp /* = true */)
{
  if (MOZ_UNLIKELY(aCapacity == 0)) {
    MOZ_ASSERT_UNREACHABLE("Appending a chunk of zero size?");
    return Nothing();
  }

  // Round up if requested.
  size_t finalCapacity = aRoundUp ? RoundedUpCapacity(aCapacity)
                                  : aCapacity;

  // Use the size of the SurfaceCache as an additional heuristic to avoid
  // allocating huge buffers. Generally images do not get smaller when decoded,
  // so if we could store the source data in the SurfaceCache, we assume that
  // there's no way we'll be able to store the decoded version.
  if (MOZ_UNLIKELY(!SurfaceCache::CanHold(finalCapacity + aExistingCapacity))) {
    NS_WARNING("SourceBuffer refused to create chunk too large for SurfaceCache");
    return Nothing();
  }

  return Some(Chunk(finalCapacity));
}

nsresult
SourceBuffer::Compact()
{
  mMutex.AssertCurrentThreadOwns();

  MOZ_ASSERT(mConsumerCount == 0, "Should have no consumers here");
  MOZ_ASSERT(mWaitingConsumers.Length() == 0, "Shouldn't have waiters");
  MOZ_ASSERT(mStatus, "Should be complete here");

  // If we've tried to compact once, don't attempt again.
  if (mCompacted) {
    return NS_OK;
  }

  mCompacted = true;

  // Compact our waiting consumers list, since we're complete and no future
  // consumer will ever have to wait.
  mWaitingConsumers.Compact();

  // If we have no chunks, then there's nothing to compact.
  if (mChunks.Length() < 1) {
    return NS_OK;
  }

  // If we have one chunk, then we can compact if it has excess capacity.
  if (mChunks.Length() == 1 && mChunks[0].Length() == mChunks[0].Capacity()) {
    return NS_OK;
  }

  // If the last chunk has the maximum capacity, then we know the total size
  // will be quite large and not worth consolidating. We can likely/cheapily
  // trim the last chunk if it is too big however.
  size_t capacity = mChunks.LastElement().Capacity();
  if (capacity == MAX_CHUNK_CAPACITY) {
    size_t lastLength = mChunks.LastElement().Length();
    if (lastLength != capacity) {
      mChunks.LastElement().SetCapacity(lastLength);
    }
    return NS_OK;
  }

  // We can compact our buffer. Determine the total length.
  size_t length = 0;
  for (uint32_t i = 0 ; i < mChunks.Length() ; ++i) {
    length += mChunks[i].Length();
  }

  // If our total length is zero (which means ExpectLength() got called, but no
  // data ever actually got written) then just empty our chunk list.
  if (MOZ_UNLIKELY(length == 0)) {
    mChunks.Clear();
    return NS_OK;
  }

  Chunk& mergeChunk = mChunks[0];
  if (MOZ_UNLIKELY(!mergeChunk.SetCapacity(length))) {
    NS_WARNING("Failed to reallocate chunk for SourceBuffer compacting - OOM?");
    return NS_OK;
  }

  // Copy our old chunks into the newly reallocated first chunk.
  for (uint32_t i = 1 ; i < mChunks.Length() ; ++i) {
    size_t offset = mergeChunk.Length();
    MOZ_ASSERT(offset < mergeChunk.Capacity());
    MOZ_ASSERT(offset + mChunks[i].Length() <= mergeChunk.Capacity());

    memcpy(mergeChunk.Data() + offset, mChunks[i].Data(), mChunks[i].Length());
    mergeChunk.AddLength(mChunks[i].Length());
  }

  MOZ_ASSERT(mergeChunk.Length() == mergeChunk.Capacity(),
             "Compacted chunk has slack space");

  // Remove the redundant chunks.
  mChunks.RemoveElementsAt(1, mChunks.Length() - 1);
  mChunks.Compact();

  return NS_OK;
}

/* static */ size_t
SourceBuffer::RoundedUpCapacity(size_t aCapacity)
{
  // Protect against overflow.
  if (MOZ_UNLIKELY(SIZE_MAX - aCapacity < MIN_CHUNK_CAPACITY)) {
    return aCapacity;
  }

  // Round up to the next multiple of MIN_CHUNK_CAPACITY (which should be the
  // size of a page).
  size_t roundedCapacity =
    (aCapacity + MIN_CHUNK_CAPACITY - 1) & ~(MIN_CHUNK_CAPACITY - 1);
  MOZ_ASSERT(roundedCapacity >= aCapacity, "Bad math?");
  MOZ_ASSERT(roundedCapacity - aCapacity < MIN_CHUNK_CAPACITY, "Bad math?");

  return roundedCapacity;
}

size_t
SourceBuffer::FibonacciCapacityWithMinimum(size_t aMinCapacity)
{
  mMutex.AssertCurrentThreadOwns();

  // We grow the source buffer using a Fibonacci growth rate. It will be capped
  // at MAX_CHUNK_CAPACITY, unless the available data exceeds that.

  size_t length = mChunks.Length();

  if (length == 0 || aMinCapacity > MAX_CHUNK_CAPACITY) {
    return aMinCapacity;
  }

  if (length == 1) {
    return min(max(2 * mChunks[0].Capacity(), aMinCapacity),
               MAX_CHUNK_CAPACITY);
  }

  return min(max(mChunks[length - 1].Capacity() +
                 mChunks[length - 2].Capacity(),
                 aMinCapacity), MAX_CHUNK_CAPACITY);
}

void
SourceBuffer::AddWaitingConsumer(IResumable* aConsumer)
{
  mMutex.AssertCurrentThreadOwns();

  MOZ_ASSERT(!mStatus, "Waiting when we're complete?");

  if (aConsumer) {
    mWaitingConsumers.AppendElement(aConsumer);
  }
}

void
SourceBuffer::ResumeWaitingConsumers()
{
  mMutex.AssertCurrentThreadOwns();

  if (mWaitingConsumers.Length() == 0) {
    return;
  }

  for (uint32_t i = 0 ; i < mWaitingConsumers.Length() ; ++i) {
    mWaitingConsumers[i]->Resume();
  }

  mWaitingConsumers.Clear();
}

nsresult
SourceBuffer::ExpectLength(size_t aExpectedLength)
{
  MOZ_ASSERT(aExpectedLength > 0, "Zero expected size?");

  MutexAutoLock lock(mMutex);

  if (MOZ_UNLIKELY(mStatus)) {
    MOZ_ASSERT_UNREACHABLE("ExpectLength after SourceBuffer is complete");
    return NS_OK;
  }

  if (MOZ_UNLIKELY(mChunks.Length() > 0)) {
    MOZ_ASSERT_UNREACHABLE("Duplicate or post-Append call to ExpectLength");
    return NS_OK;
  }

  if (MOZ_UNLIKELY(!SurfaceCache::CanHold(aExpectedLength))) {
    NS_WARNING("SourceBuffer refused to store too large buffer");
    return HandleError(NS_ERROR_INVALID_ARG);
  }

  size_t length = min(aExpectedLength, MAX_CHUNK_CAPACITY);
  if (MOZ_UNLIKELY(NS_FAILED(AppendChunk(CreateChunk(length,
                                                     /* aExistingCapacity */ 0,
                                                     /* aRoundUp */ false))))) {
    return HandleError(NS_ERROR_OUT_OF_MEMORY);
  }

  return NS_OK;
}

nsresult
SourceBuffer::Append(const char* aData, size_t aLength)
{
  MOZ_ASSERT(aData, "Should have a buffer");
  MOZ_ASSERT(aLength > 0, "Writing a zero-sized chunk");

  size_t currentChunkCapacity = 0;
  size_t currentChunkLength = 0;
  char* currentChunkData = nullptr;
  size_t currentChunkRemaining = 0;
  size_t forCurrentChunk = 0;
  size_t forNextChunk = 0;
  size_t nextChunkCapacity = 0;
  size_t totalCapacity = 0;

  {
    MutexAutoLock lock(mMutex);

    if (MOZ_UNLIKELY(mStatus)) {
      // This SourceBuffer is already complete; ignore further data.
      return NS_ERROR_FAILURE;
    }

    if (MOZ_UNLIKELY(mChunks.Length() == 0)) {
      if (MOZ_UNLIKELY(NS_FAILED(AppendChunk(CreateChunk(aLength))))) {
        return HandleError(NS_ERROR_OUT_OF_MEMORY);
      }
    }

    // Copy out the current chunk's information so we can release the lock.
    // Note that this wouldn't be safe if multiple producers were allowed!
    Chunk& currentChunk = mChunks.LastElement();
    currentChunkCapacity = currentChunk.Capacity();
    currentChunkLength = currentChunk.Length();
    currentChunkData = currentChunk.Data();

    // Partition this data between the current chunk and the next chunk.
    // (Because we always allocate a chunk big enough to fit everything passed
    // to Append, we'll never need more than those two chunks to store
    // everything.)
    currentChunkRemaining = currentChunkCapacity - currentChunkLength;
    forCurrentChunk = min(aLength, currentChunkRemaining);
    forNextChunk = aLength - forCurrentChunk;

    // If we'll need another chunk, determine what its capacity should be while
    // we still hold the lock.
    nextChunkCapacity = forNextChunk > 0
                      ? FibonacciCapacityWithMinimum(forNextChunk)
                      : 0;

    for (uint32_t i = 0 ; i < mChunks.Length() ; ++i) {
      totalCapacity += mChunks[i].Capacity();
    }
  }

  // Write everything we can fit into the current chunk.
  MOZ_ASSERT(currentChunkLength + forCurrentChunk <= currentChunkCapacity);
  memcpy(currentChunkData + currentChunkLength, aData, forCurrentChunk);

  // If there's something left, create a new chunk and write it there.
  Maybe<Chunk> nextChunk;
  if (forNextChunk > 0) {
    MOZ_ASSERT(nextChunkCapacity >= forNextChunk, "Next chunk too small?");
    nextChunk = CreateChunk(nextChunkCapacity, totalCapacity);
    if (MOZ_LIKELY(nextChunk && !nextChunk->AllocationFailed())) {
      memcpy(nextChunk->Data(), aData + forCurrentChunk, forNextChunk);
      nextChunk->AddLength(forNextChunk);
    }
  }

  // Update shared data structures.
  {
    MutexAutoLock lock(mMutex);

    // Update the length of the current chunk.
    Chunk& currentChunk = mChunks.LastElement();
    MOZ_ASSERT(currentChunk.Data() == currentChunkData, "Multiple producers?");
    MOZ_ASSERT(currentChunk.Length() == currentChunkLength,
               "Multiple producers?");

    currentChunk.AddLength(forCurrentChunk);

    // If we created a new chunk, add it to the series.
    if (forNextChunk > 0) {
      if (MOZ_UNLIKELY(!nextChunk)) {
        return HandleError(NS_ERROR_OUT_OF_MEMORY);
      }

      if (MOZ_UNLIKELY(NS_FAILED(AppendChunk(std::move(nextChunk))))) {
        return HandleError(NS_ERROR_OUT_OF_MEMORY);
      }
    }

    // Resume any waiting readers now that there's new data.
    ResumeWaitingConsumers();
  }

  return NS_OK;
}

static nsresult
AppendToSourceBuffer(nsIInputStream*,
                     void* aClosure,
                     const char* aFromRawSegment,
                     uint32_t,
                     uint32_t aCount,
                     uint32_t* aWriteCount)
{
  SourceBuffer* sourceBuffer = static_cast<SourceBuffer*>(aClosure);

  // Copy the source data. Unless we hit OOM, we squelch the return value here,
  // because returning an error means that ReadSegments stops reading data, and
  // we want to ensure that we read everything we get. If we hit OOM then we
  // return a failed status to the caller.
  nsresult rv = sourceBuffer->Append(aFromRawSegment, aCount);
  if (rv == NS_ERROR_OUT_OF_MEMORY) {
    return rv;
  }

  // Report that we wrote everything we got.
  *aWriteCount = aCount;

  return NS_OK;
}

nsresult
SourceBuffer::AppendFromInputStream(nsIInputStream* aInputStream,
                                    uint32_t aCount)
{
  uint32_t bytesRead;
  nsresult rv = aInputStream->ReadSegments(AppendToSourceBuffer, this,
                                           aCount, &bytesRead);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (bytesRead == 0) {
    // The loading of the image has been canceled.
    return NS_ERROR_FAILURE;
  }

  if (bytesRead != aCount) {
    // Only some of the given data was read. We may have failed in
    // SourceBuffer::Append but ReadSegments swallowed the error. Otherwise the
    // stream itself failed to yield the data.
    MutexAutoLock lock(mMutex);
    if (mStatus) {
      MOZ_ASSERT(NS_FAILED(*mStatus));
      return *mStatus;
    }

    MOZ_ASSERT_UNREACHABLE("AppendToSourceBuffer should consume everything");
  }

  return rv;
}

void
SourceBuffer::Complete(nsresult aStatus)
{
  MutexAutoLock lock(mMutex);

  // When an error occurs internally (e.g. due to an OOM), we save the status.
  // This will indirectly trigger a failure higher up and that will call
  // SourceBuffer::Complete. Since it doesn't necessarily know we are already
  // complete, it is safe to ignore.
  if (mStatus && (MOZ_UNLIKELY(NS_SUCCEEDED(*mStatus) ||
                               aStatus != NS_IMAGELIB_ERROR_FAILURE))) {
    MOZ_ASSERT_UNREACHABLE("Called Complete more than once");
    return;
  }

  if (MOZ_UNLIKELY(NS_SUCCEEDED(aStatus) && IsEmpty())) {
    // It's illegal to succeed without writing anything.
    aStatus = NS_ERROR_FAILURE;
  }

  mStatus = Some(aStatus);

  // Resume any waiting consumers now that we're complete.
  ResumeWaitingConsumers();

  // If we still have active consumers, just return.
  if (mConsumerCount > 0) {
    return;
  }

  // Attempt to compact our buffer down to a single chunk.
  Compact();
}

bool
SourceBuffer::IsComplete()
{
  MutexAutoLock lock(mMutex);
  return bool(mStatus);
}

size_t
SourceBuffer::SizeOfIncludingThisWithComputedFallback(MallocSizeOf
                                                        aMallocSizeOf) const
{
  MutexAutoLock lock(mMutex);

  size_t n = aMallocSizeOf(this);
  n += mChunks.ShallowSizeOfExcludingThis(aMallocSizeOf);

  for (uint32_t i = 0 ; i < mChunks.Length() ; ++i) {
    size_t chunkSize = aMallocSizeOf(mChunks[i].Data());

    if (chunkSize == 0) {
      // We're on a platform where moz_malloc_size_of always returns 0.
      chunkSize = mChunks[i].Capacity();
    }

    n += chunkSize;
  }

  return n;
}

SourceBufferIterator
SourceBuffer::Iterator(size_t aReadLength)
{
  {
    MutexAutoLock lock(mMutex);
    mConsumerCount++;
  }

  return SourceBufferIterator(this, aReadLength);
}

void
SourceBuffer::OnIteratorRelease()
{
  MutexAutoLock lock(mMutex);

  MOZ_ASSERT(mConsumerCount > 0, "Consumer count doesn't add up");
  mConsumerCount--;

  // If we still have active consumers, or we're not complete yet, then return.
  if (mConsumerCount > 0 || !mStatus) {
    return;
  }

  // Attempt to compact our buffer down to a single chunk.
  Compact();
}

bool
SourceBuffer::RemainingBytesIsNoMoreThan(const SourceBufferIterator& aIterator,
                                         size_t aBytes) const
{
  MutexAutoLock lock(mMutex);

  // If we're not complete, we always say no.
  if (!mStatus) {
    return false;
  }

  // If the iterator's at the end, the answer is trivial.
  if (!aIterator.HasMore()) {
    return true;
  }

  uint32_t iteratorChunk = aIterator.mData.mIterating.mChunk;
  size_t iteratorOffset = aIterator.mData.mIterating.mOffset;
  size_t iteratorLength = aIterator.mData.mIterating.mAvailableLength;

  // Include the bytes the iterator is currently pointing to in the limit, so
  // that the current chunk doesn't have to be a special case.
  size_t bytes = aBytes + iteratorOffset + iteratorLength;

  // Count the length over all of our chunks, starting with the one that the
  // iterator is currently pointing to. (This is O(N), but N is expected to be
  // ~1, so it doesn't seem worth caching the length separately.)
  size_t lengthSoFar = 0;
  for (uint32_t i = iteratorChunk ; i < mChunks.Length() ; ++i) {
    lengthSoFar += mChunks[i].Length();
    if (lengthSoFar > bytes) {
      return false;
    }
  }

  return true;
}

SourceBufferIterator::State
SourceBuffer::AdvanceIteratorOrScheduleResume(SourceBufferIterator& aIterator,
                                              size_t aRequestedBytes,
                                              IResumable* aConsumer)
{
  MutexAutoLock lock(mMutex);

  MOZ_ASSERT(aIterator.HasMore(), "Advancing a completed iterator and "
                                  "AdvanceOrScheduleResume didn't catch it");

  if (MOZ_UNLIKELY(mStatus && NS_FAILED(*mStatus))) {
    // This SourceBuffer is complete due to an error; all reads fail.
    return aIterator.SetComplete(*mStatus);
  }

  if (MOZ_UNLIKELY(mChunks.Length() == 0)) {
    // We haven't gotten an initial chunk yet.
    AddWaitingConsumer(aConsumer);
    return aIterator.SetWaiting(!!aConsumer);
  }

  uint32_t iteratorChunkIdx = aIterator.mData.mIterating.mChunk;
  MOZ_ASSERT(iteratorChunkIdx < mChunks.Length());

  const Chunk& currentChunk = mChunks[iteratorChunkIdx];
  size_t iteratorEnd = aIterator.mData.mIterating.mOffset +
                       aIterator.mData.mIterating.mAvailableLength;
  MOZ_ASSERT(iteratorEnd <= currentChunk.Length());
  MOZ_ASSERT(iteratorEnd <= currentChunk.Capacity());

  if (iteratorEnd < currentChunk.Length()) {
    // There's more data in the current chunk.
    return aIterator.SetReady(iteratorChunkIdx, currentChunk.Data(),
                              iteratorEnd, currentChunk.Length() - iteratorEnd,
                              aRequestedBytes);
  }

  if (iteratorEnd == currentChunk.Capacity() &&
      !IsLastChunk(iteratorChunkIdx)) {
    // Advance to the next chunk.
    const Chunk& nextChunk = mChunks[iteratorChunkIdx + 1];
    return aIterator.SetReady(iteratorChunkIdx + 1, nextChunk.Data(), 0,
                              nextChunk.Length(), aRequestedBytes);
  }

  MOZ_ASSERT(IsLastChunk(iteratorChunkIdx), "Should've advanced");

  if (mStatus) {
    // There's no more data and this SourceBuffer completed successfully.
    MOZ_ASSERT(NS_SUCCEEDED(*mStatus), "Handled failures earlier");
    return aIterator.SetComplete(*mStatus);
  }

  // We're not complete, but there's no more data right now. Arrange to wake up
  // the consumer when we get more data.
  AddWaitingConsumer(aConsumer);
  return aIterator.SetWaiting(!!aConsumer);
}

nsresult
SourceBuffer::HandleError(nsresult aError)
{
  MOZ_ASSERT(NS_FAILED(aError), "Should have an error here");
  MOZ_ASSERT(aError == NS_ERROR_OUT_OF_MEMORY ||
             aError == NS_ERROR_INVALID_ARG,
             "Unexpected error; may want to notify waiting readers, which "
             "HandleError currently doesn't do");

  mMutex.AssertCurrentThreadOwns();

  NS_WARNING("SourceBuffer encountered an unrecoverable error");

  // Record the error.
  mStatus = Some(aError);

  // Drop our references to waiting readers.
  mWaitingConsumers.Clear();

  return *mStatus;
}

bool
SourceBuffer::IsEmpty()
{
  mMutex.AssertCurrentThreadOwns();
  return mChunks.Length() == 0 ||
         mChunks[0].Length() == 0;
}

bool
SourceBuffer::IsLastChunk(uint32_t aChunk)
{
  mMutex.AssertCurrentThreadOwns();
  return aChunk + 1 == mChunks.Length();
}

} // namespace image
} // namespace mozilla
