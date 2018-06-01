/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include <algorithm>
#include <cstdint>

#include "Common.h"
#include "mozilla/Move.h"
#include "nsIInputStream.h"
#include "SourceBuffer.h"
#include "SurfaceCache.h"

using namespace mozilla;
using namespace mozilla::image;

using std::min;

void
ExpectChunkAndByteCount(const SourceBufferIterator& aIterator,
                        uint32_t aChunks,
                        size_t aBytes)
{
  EXPECT_EQ(aChunks, aIterator.ChunkCount());
  EXPECT_EQ(aBytes, aIterator.ByteCount());
}

void
ExpectRemainingBytes(const SourceBufferIterator& aIterator, size_t aBytes)
{
  EXPECT_TRUE(aIterator.RemainingBytesIsNoMoreThan(aBytes));
  EXPECT_TRUE(aIterator.RemainingBytesIsNoMoreThan(aBytes + 1));

  if (aBytes > 0) {
    EXPECT_FALSE(aIterator.RemainingBytesIsNoMoreThan(0));
    EXPECT_FALSE(aIterator.RemainingBytesIsNoMoreThan(aBytes - 1));
  }
}

char
GenerateByte(size_t aIndex)
{
  uint8_t byte = aIndex % 256;
  return *reinterpret_cast<char*>(&byte);
}

void
GenerateData(char* aOutput, size_t aOffset, size_t aLength)
{
  for (size_t i = 0; i < aLength; ++i) {
    aOutput[i] = GenerateByte(aOffset + i);
  }
}

void
GenerateData(char* aOutput, size_t aLength)
{
  GenerateData(aOutput, 0, aLength);
}

void
CheckData(const char* aData, size_t aOffset, size_t aLength)
{
  for (size_t i = 0; i < aLength; ++i) {
    ASSERT_EQ(GenerateByte(aOffset + i), aData[i]);
  }
}

enum class AdvanceMode
{
  eAdvanceAsMuchAsPossible,
  eAdvanceByLengthExactly
};

class ImageSourceBuffer : public ::testing::Test
{
public:
  ImageSourceBuffer()
    : mSourceBuffer(new SourceBuffer)
    , mExpectNoResume(new ExpectNoResume)
    , mCountResumes(new CountResumes)
  {
    GenerateData(mData, sizeof(mData));
    EXPECT_FALSE(mSourceBuffer->IsComplete());
  }

protected:
  void CheckedAppendToBuffer(const char* aData, size_t aLength)
  {
    EXPECT_TRUE(NS_SUCCEEDED(mSourceBuffer->Append(aData, aLength)));
  }

  void CheckedAppendToBufferLastByteForLength(size_t aLength)
  {
    const char lastByte = GenerateByte(aLength);
    CheckedAppendToBuffer(&lastByte, 1);
  }

  void CheckedAppendToBufferInChunks(size_t aChunkLength, size_t aTotalLength)
  {
    char* data = new char[aChunkLength];

    size_t bytesWritten = 0;
    while (bytesWritten < aTotalLength) {
      GenerateData(data, bytesWritten, aChunkLength);
      size_t toWrite = min(aChunkLength, aTotalLength - bytesWritten);
      CheckedAppendToBuffer(data, toWrite);
      bytesWritten += toWrite;
    }

    delete[] data;
  }

  void CheckedCompleteBuffer(nsresult aCompletionStatus = NS_OK)
  {
    mSourceBuffer->Complete(aCompletionStatus);
    EXPECT_TRUE(mSourceBuffer->IsComplete());
  }

  void CheckedCompleteBuffer(SourceBufferIterator& aIterator,
                             size_t aLength,
                             nsresult aCompletionStatus = NS_OK)
  {
    CheckedCompleteBuffer(aCompletionStatus);
    ExpectRemainingBytes(aIterator, aLength);
  }

  void CheckedAdvanceIteratorStateOnly(SourceBufferIterator& aIterator,
                                       size_t aLength,
                                       uint32_t aChunks,
                                       size_t aTotalLength,
                                       AdvanceMode aAdvanceMode
                                         = AdvanceMode::eAdvanceAsMuchAsPossible)
  {
    const size_t advanceBy = aAdvanceMode == AdvanceMode::eAdvanceAsMuchAsPossible
                           ? SIZE_MAX
                           : aLength;

    auto state = aIterator.AdvanceOrScheduleResume(advanceBy, mExpectNoResume);
    ASSERT_EQ(SourceBufferIterator::READY, state);
    EXPECT_TRUE(aIterator.Data());
    EXPECT_EQ(aLength, aIterator.Length());

    ExpectChunkAndByteCount(aIterator, aChunks, aTotalLength);
  }

  void CheckedAdvanceIteratorStateOnly(SourceBufferIterator& aIterator,
                                       size_t aLength)
  {
    CheckedAdvanceIteratorStateOnly(aIterator, aLength, 1, aLength);
  }

  void CheckedAdvanceIterator(SourceBufferIterator& aIterator,
                              size_t aLength,
                              uint32_t aChunks,
                              size_t aTotalLength,
                              AdvanceMode aAdvanceMode
                                = AdvanceMode::eAdvanceAsMuchAsPossible)
  {
    // Check that the iterator is in the expected state.
    CheckedAdvanceIteratorStateOnly(aIterator, aLength, aChunks,
                                    aTotalLength, aAdvanceMode);

    // Check that we read the expected data. To do this, we need to compute our
    // offset in the SourceBuffer, but fortunately that's pretty easy: it's the
    // total number of bytes the iterator has advanced through, minus the length
    // of the current chunk.
    const size_t offset = aIterator.ByteCount() - aIterator.Length();
    CheckData(aIterator.Data(), offset, aIterator.Length());
  }

  void CheckedAdvanceIterator(SourceBufferIterator& aIterator, size_t aLength)
  {
    CheckedAdvanceIterator(aIterator, aLength, 1, aLength);
  }

  void CheckIteratorMustWait(SourceBufferIterator& aIterator,
                             IResumable* aOnResume)
  {
    auto state = aIterator.AdvanceOrScheduleResume(1, aOnResume);
    EXPECT_EQ(SourceBufferIterator::WAITING, state);
  }

  void CheckIteratorIsComplete(SourceBufferIterator& aIterator,
                               uint32_t aChunks,
                               size_t aTotalLength,
                               nsresult aCompletionStatus = NS_OK)
  {
    ASSERT_TRUE(mSourceBuffer->IsComplete());
    auto state = aIterator.AdvanceOrScheduleResume(1, mExpectNoResume);
    ASSERT_EQ(SourceBufferIterator::COMPLETE, state);
    EXPECT_EQ(aCompletionStatus, aIterator.CompletionStatus());
    ExpectRemainingBytes(aIterator, 0);
    ExpectChunkAndByteCount(aIterator, aChunks, aTotalLength);
  }

  void CheckIteratorIsComplete(SourceBufferIterator& aIterator,
                               size_t aTotalLength)
  {
    CheckIteratorIsComplete(aIterator, 1, aTotalLength);
  }

  AutoInitializeImageLib mInit;
  char mData[9];
  RefPtr<SourceBuffer> mSourceBuffer;
  RefPtr<ExpectNoResume> mExpectNoResume;
  RefPtr<CountResumes> mCountResumes;
};

TEST_F(ImageSourceBuffer, InitialState)
{
  SourceBufferIterator iterator = mSourceBuffer->Iterator();

  // RemainingBytesIsNoMoreThan() should always return false in the initial
  // state, since we can't know the answer until Complete() has been called.
  EXPECT_FALSE(iterator.RemainingBytesIsNoMoreThan(0));
  EXPECT_FALSE(iterator.RemainingBytesIsNoMoreThan(SIZE_MAX));

  // We haven't advanced our iterator at all, so its counters should be zero.
  ExpectChunkAndByteCount(iterator, 0, 0);

  // Attempt to advance; we should fail, and end up in the WAITING state. We
  // expect no resumes because we don't actually append anything to the
  // SourceBuffer in this test.
  CheckIteratorMustWait(iterator, mExpectNoResume);
}

TEST_F(ImageSourceBuffer, ZeroLengthBufferAlwaysFails)
{
  SourceBufferIterator iterator = mSourceBuffer->Iterator();

  // Complete the buffer without writing to it, providing a successful
  // completion status.
  CheckedCompleteBuffer(iterator, 0);

  // Completing a buffer without writing to it results in an automatic failure;
  // make sure that the actual completion status we get from the iterator
  // reflects this.
  CheckIteratorIsComplete(iterator, 0, 0, NS_ERROR_FAILURE);
}

TEST_F(ImageSourceBuffer, CompleteSuccess)
{
  SourceBufferIterator iterator = mSourceBuffer->Iterator();

  // Write a single byte to the buffer and complete the buffer. (We have to
  // write at least one byte because completing a zero length buffer always
  // fails; see the ZeroLengthBufferAlwaysFails test.)
  CheckedAppendToBuffer(mData, 1);
  CheckedCompleteBuffer(iterator, 1);

  // We should be able to advance once (to read the single byte) and then should
  // reach the COMPLETE state with a successful status.
  CheckedAdvanceIterator(iterator, 1);
  CheckIteratorIsComplete(iterator, 1);
}

TEST_F(ImageSourceBuffer, CompleteFailure)
{
  SourceBufferIterator iterator = mSourceBuffer->Iterator();

  // Write a single byte to the buffer and complete the buffer. (We have to
  // write at least one byte because completing a zero length buffer always
  // fails; see the ZeroLengthBufferAlwaysFails test.)
  CheckedAppendToBuffer(mData, 1);
  CheckedCompleteBuffer(iterator, 1, NS_ERROR_FAILURE);

  // Advance the iterator. Because a failing status is propagated to the
  // iterator as soon as it advances, we won't be able to read the single byte
  // that we wrote above; we go directly into the COMPLETE state.
  CheckIteratorIsComplete(iterator, 0, 0, NS_ERROR_FAILURE);
}

TEST_F(ImageSourceBuffer, Append)
{
  SourceBufferIterator iterator = mSourceBuffer->Iterator();

  // Write test data to the buffer.
  EXPECT_TRUE(NS_SUCCEEDED(mSourceBuffer->ExpectLength(sizeof(mData))));
  CheckedAppendToBuffer(mData, sizeof(mData));
  CheckedCompleteBuffer(iterator, sizeof(mData));

  // Verify that we can read it back via the iterator, and that the final state
  // is what we expect.
  CheckedAdvanceIterator(iterator, sizeof(mData));
  CheckIteratorIsComplete(iterator, sizeof(mData));
}

TEST_F(ImageSourceBuffer, HugeAppendFails)
{
  SourceBufferIterator iterator = mSourceBuffer->Iterator();

  // We should fail to append anything bigger than what the SurfaceCache can
  // hold, so use the SurfaceCache's maximum capacity to calculate what a
  // "massive amount of data" (see below) consists of on this platform.
  ASSERT_LT(SurfaceCache::MaximumCapacity(), SIZE_MAX);
  const size_t hugeSize = SurfaceCache::MaximumCapacity() + 1;

  // Attempt to write a massive amount of data and verify that it fails. (We'd
  // get a buffer overrun during the test if it succeeds, but if it succeeds
  // that's the least of our problems.)
  EXPECT_TRUE(NS_FAILED(mSourceBuffer->Append(mData, hugeSize)));
  EXPECT_TRUE(mSourceBuffer->IsComplete());
  CheckIteratorIsComplete(iterator, 0, 0, NS_ERROR_OUT_OF_MEMORY);
}

TEST_F(ImageSourceBuffer, AppendFromInputStream)
{
  SourceBufferIterator iterator = mSourceBuffer->Iterator();

  // Construct an input stream with some arbitrary data. (We use test data from
  // one of the decoder tests.)
  nsCOMPtr<nsIInputStream> inputStream = LoadFile(GreenPNGTestCase().mPath);
  ASSERT_TRUE(inputStream != nullptr);

  // Figure out how much data we have.
  uint64_t length;
  ASSERT_TRUE(NS_SUCCEEDED(inputStream->Available(&length)));

  // Write test data to the buffer.
  EXPECT_TRUE(NS_SUCCEEDED(mSourceBuffer->AppendFromInputStream(inputStream,
                                                                length)));
  CheckedCompleteBuffer(iterator, length);

  // Verify that the iterator sees the appropriate amount of data.
  CheckedAdvanceIteratorStateOnly(iterator, length);
  CheckIteratorIsComplete(iterator, length);
}

TEST_F(ImageSourceBuffer, AppendAfterComplete)
{
  SourceBufferIterator iterator = mSourceBuffer->Iterator();

  // Write test data to the buffer.
  EXPECT_TRUE(NS_SUCCEEDED(mSourceBuffer->ExpectLength(sizeof(mData))));
  CheckedAppendToBuffer(mData, sizeof(mData));
  CheckedCompleteBuffer(iterator, sizeof(mData));

  // Verify that we can read it back via the iterator, and that the final state
  // is what we expect.
  CheckedAdvanceIterator(iterator, sizeof(mData));
  CheckIteratorIsComplete(iterator, sizeof(mData));

  // Write more data to the completed buffer.
  EXPECT_TRUE(NS_FAILED(mSourceBuffer->Append(mData, sizeof(mData))));

  // Try to read with a new iterator and verify that the new data got ignored.
  SourceBufferIterator iterator2 = mSourceBuffer->Iterator();
  CheckedAdvanceIterator(iterator2, sizeof(mData));
  CheckIteratorIsComplete(iterator2, sizeof(mData));
}

TEST_F(ImageSourceBuffer, MinChunkCapacity)
{
  SourceBufferIterator iterator = mSourceBuffer->Iterator();

  // Write test data to the buffer using many small appends. Since
  // ExpectLength() isn't being called, we should be able to write up to
  // SourceBuffer::MIN_CHUNK_CAPACITY bytes without a second chunk being
  // allocated.
  CheckedAppendToBufferInChunks(10, SourceBuffer::MIN_CHUNK_CAPACITY);

  // Verify that the iterator sees the appropriate amount of data.
  CheckedAdvanceIterator(iterator, SourceBuffer::MIN_CHUNK_CAPACITY);

  // Write one more byte; we expect to see that it triggers an allocation.
  CheckedAppendToBufferLastByteForLength(SourceBuffer::MIN_CHUNK_CAPACITY);
  CheckedCompleteBuffer(iterator, 1);

  // Verify that the iterator sees the new byte and a new chunk has been
  // allocated.
  CheckedAdvanceIterator(iterator, 1, 2, SourceBuffer::MIN_CHUNK_CAPACITY + 1);
  CheckIteratorIsComplete(iterator, 2, SourceBuffer::MIN_CHUNK_CAPACITY + 1);
}

TEST_F(ImageSourceBuffer, ExpectLengthAllocatesRequestedCapacity)
{
  SourceBufferIterator iterator = mSourceBuffer->Iterator();

  // Write SourceBuffer::MIN_CHUNK_CAPACITY bytes of test data to the buffer,
  // but call ExpectLength() first to make SourceBuffer expect only a single
  // byte. We expect this to still result in two chunks, because we trust the
  // initial guess of ExpectLength() but after that it will only allocate chunks
  // of at least MIN_CHUNK_CAPACITY bytes.
  EXPECT_TRUE(NS_SUCCEEDED(mSourceBuffer->ExpectLength(1)));
  CheckedAppendToBufferInChunks(10, SourceBuffer::MIN_CHUNK_CAPACITY);
  CheckedCompleteBuffer(iterator, SourceBuffer::MIN_CHUNK_CAPACITY);

  // Verify that the iterator sees a first chunk with 1 byte, and a second chunk
  // with the remaining data.
  CheckedAdvanceIterator(iterator, 1, 1, 1);
  CheckedAdvanceIterator(iterator, SourceBuffer::MIN_CHUNK_CAPACITY - 1, 2,
		                   SourceBuffer::MIN_CHUNK_CAPACITY);
  CheckIteratorIsComplete(iterator, 2, SourceBuffer::MIN_CHUNK_CAPACITY);
}

TEST_F(ImageSourceBuffer, ExpectLengthGrowsAboveMinCapacity)
{
  SourceBufferIterator iterator = mSourceBuffer->Iterator();

  // Write two times SourceBuffer::MIN_CHUNK_CAPACITY bytes of test data to the
  // buffer, calling ExpectLength() with the correct length first. We expect
  // this to result in only one chunk, because ExpectLength() allows us to
  // allocate a larger first chunk than MIN_CHUNK_CAPACITY bytes.
  const size_t length = 2 * SourceBuffer::MIN_CHUNK_CAPACITY;
  EXPECT_TRUE(NS_SUCCEEDED(mSourceBuffer->ExpectLength(length)));
  CheckedAppendToBufferInChunks(10, length);

  // Verify that the iterator sees a single chunk.
  CheckedAdvanceIterator(iterator, length);

  // Write one more byte; we expect to see that it triggers an allocation.
  CheckedAppendToBufferLastByteForLength(length);
  CheckedCompleteBuffer(iterator, 1);

  // Verify that the iterator sees the new byte and a new chunk has been
  // allocated.
  CheckedAdvanceIterator(iterator, 1, 2, length + 1);
  CheckIteratorIsComplete(iterator, 2, length + 1);
}

TEST_F(ImageSourceBuffer, HugeExpectLengthFails)
{
  SourceBufferIterator iterator = mSourceBuffer->Iterator();

  // ExpectLength() should fail if the length is bigger than what the
  // SurfaceCache can hold, so use the SurfaceCache's maximum capacity to
  // calculate what a "massive amount of data" (see below) consists of on this
  // platform.
  ASSERT_LT(SurfaceCache::MaximumCapacity(), SIZE_MAX);
  const size_t hugeSize = SurfaceCache::MaximumCapacity() + 1;

  // Attempt to write a massive amount of data and verify that it fails. (We'd
  // get a buffer overrun during the test if it succeeds, but if it succeeds
  // that's the least of our problems.)
  EXPECT_TRUE(NS_FAILED(mSourceBuffer->ExpectLength(hugeSize)));
  EXPECT_TRUE(mSourceBuffer->IsComplete());
  CheckIteratorIsComplete(iterator, 0, 0, NS_ERROR_INVALID_ARG);
}

TEST_F(ImageSourceBuffer, LargeAppendsAllocateOnlyOneChunk)
{
  SourceBufferIterator iterator = mSourceBuffer->Iterator();

  // Write two times SourceBuffer::MIN_CHUNK_CAPACITY bytes of test data to the
  // buffer in a single Append() call. We expect this to result in only one
  // chunk even though ExpectLength() wasn't called, because we should always
  // allocate a new chunk large enough to store the data we have at hand.
  constexpr size_t length = 2 * SourceBuffer::MIN_CHUNK_CAPACITY;
  char data[length];
  GenerateData(data, sizeof(data));
  CheckedAppendToBuffer(data, length);

  // Verify that the iterator sees a single chunk.
  CheckedAdvanceIterator(iterator, length);

  // Write one more byte; we expect to see that it triggers an allocation.
  CheckedAppendToBufferLastByteForLength(length);
  CheckedCompleteBuffer(iterator, 1);

  // Verify that the iterator sees the new byte and a new chunk has been
  // allocated.
  CheckedAdvanceIterator(iterator, 1, 2, length + 1);
  CheckIteratorIsComplete(iterator, 2, length + 1);
}

TEST_F(ImageSourceBuffer, LargeAppendsAllocateAtMostOneChunk)
{
  SourceBufferIterator iterator = mSourceBuffer->Iterator();

  // Allocate some data we'll use below.
  constexpr size_t firstWriteLength = SourceBuffer::MIN_CHUNK_CAPACITY / 2;
  constexpr size_t secondWriteLength = 3 * SourceBuffer::MIN_CHUNK_CAPACITY;
  constexpr size_t totalLength = firstWriteLength + secondWriteLength;
  char data[totalLength];
  GenerateData(data, sizeof(data));

  // Write half of SourceBuffer::MIN_CHUNK_CAPACITY bytes of test data to the
  // buffer in a single Append() call. This should fill half of the first chunk.
  CheckedAppendToBuffer(data, firstWriteLength);

  // Write three times SourceBuffer::MIN_CHUNK_CAPACITY bytes of test data to the
  // buffer in a single Append() call. We expect this to result in the first of
  // the first chunk being filled and a new chunk being allocated for the
  // remainder.
  CheckedAppendToBuffer(data + firstWriteLength, secondWriteLength);

  // Verify that the iterator sees a MIN_CHUNK_CAPACITY-length chunk.
  CheckedAdvanceIterator(iterator, SourceBuffer::MIN_CHUNK_CAPACITY);

  // Verify that the iterator sees a second chunk of the length we expect.
  const size_t expectedSecondChunkLength =
    totalLength - SourceBuffer::MIN_CHUNK_CAPACITY;
  CheckedAdvanceIterator(iterator, expectedSecondChunkLength, 2, totalLength);

  // Write one more byte; we expect to see that it triggers an allocation.
  CheckedAppendToBufferLastByteForLength(totalLength);
  CheckedCompleteBuffer(iterator, 1);

  // Verify that the iterator sees the new byte and a new chunk has been
  // allocated.
  CheckedAdvanceIterator(iterator, 1, 3, totalLength + 1);
  CheckIteratorIsComplete(iterator, 3, totalLength + 1);
}

TEST_F(ImageSourceBuffer, OversizedAppendsAllocateAtMostOneChunk)
{
  SourceBufferIterator iterator = mSourceBuffer->Iterator();

  // Allocate some data we'll use below.
  constexpr size_t writeLength = SourceBuffer::MAX_CHUNK_CAPACITY + 1;

  // Write SourceBuffer::MAX_CHUNK_CAPACITY + 1 bytes of test data to the
  // buffer in a single Append() call. This should cause one chunk to be
  // allocated because we wrote it as a single block.
  CheckedAppendToBufferInChunks(writeLength, writeLength);

  // Verify that the iterator sees a MAX_CHUNK_CAPACITY+1-length chunk.
  CheckedAdvanceIterator(iterator, writeLength);

  CheckedCompleteBuffer(NS_OK);
  CheckIteratorIsComplete(iterator, 1, writeLength);
}

TEST_F(ImageSourceBuffer, CompactionHappensWhenBufferIsComplete)
{
  constexpr size_t chunkLength = SourceBuffer::MIN_CHUNK_CAPACITY;
  constexpr size_t totalLength = 2 * chunkLength;

  // Write enough data to create two chunks.
  CheckedAppendToBufferInChunks(chunkLength, totalLength);

  {
    SourceBufferIterator iterator = mSourceBuffer->Iterator();

    // Verify that the iterator sees two chunks.
    CheckedAdvanceIterator(iterator, chunkLength);
    CheckedAdvanceIterator(iterator, chunkLength, 2, totalLength);
  }

  // Complete the buffer, which should trigger compaction implicitly.
  CheckedCompleteBuffer();

  {
    SourceBufferIterator iterator = mSourceBuffer->Iterator();

    // Verify that compaction happened and there's now only one chunk.
    CheckedAdvanceIterator(iterator, totalLength);
    CheckIteratorIsComplete(iterator, 1, totalLength);
  }
}

TEST_F(ImageSourceBuffer, CompactionIsDelayedWhileIteratorsExist)
{
  constexpr size_t chunkLength = SourceBuffer::MIN_CHUNK_CAPACITY;
  constexpr size_t totalLength = 2 * chunkLength;

  {
    SourceBufferIterator outerIterator = mSourceBuffer->Iterator();

    {
      SourceBufferIterator iterator = mSourceBuffer->Iterator();

      // Write enough data to create two chunks.
      CheckedAppendToBufferInChunks(chunkLength, totalLength);
      CheckedCompleteBuffer(iterator, totalLength);

      // Verify that the iterator sees two chunks. Since there are live
      // iterators, compaction shouldn't have happened when we completed the
      // buffer.
      CheckedAdvanceIterator(iterator, chunkLength);
      CheckedAdvanceIterator(iterator, chunkLength, 2, totalLength);
      CheckIteratorIsComplete(iterator, 2, totalLength);
    }

    // Now |iterator| has been destroyed, but |outerIterator| still exists, so
    // we expect no compaction to have occurred at this point.
    CheckedAdvanceIterator(outerIterator, chunkLength);
    CheckedAdvanceIterator(outerIterator, chunkLength, 2, totalLength);
    CheckIteratorIsComplete(outerIterator, 2, totalLength);
  }

  // Now all iterators have been destroyed. Since the buffer was already
  // complete, we expect compaction to happen implicitly here.

  {
    SourceBufferIterator iterator = mSourceBuffer->Iterator();

    // Verify that compaction happened and there's now only one chunk.
    CheckedAdvanceIterator(iterator, totalLength);
    CheckIteratorIsComplete(iterator, 1, totalLength);
  }
}

TEST_F(ImageSourceBuffer, SourceBufferIteratorsCanBeMoved)
{
  constexpr size_t chunkLength = SourceBuffer::MIN_CHUNK_CAPACITY;
  constexpr size_t totalLength = 2 * chunkLength;

  // Write enough data to create two chunks. We create an iterator here to make
  // sure that compaction doesn't happen during the test.
  SourceBufferIterator iterator = mSourceBuffer->Iterator();
  CheckedAppendToBufferInChunks(chunkLength, totalLength);
  CheckedCompleteBuffer(iterator, totalLength);

  auto GetIterator = [&]{
    SourceBufferIterator lambdaIterator = mSourceBuffer->Iterator();
    CheckedAdvanceIterator(lambdaIterator, chunkLength);
    return lambdaIterator;
  };

  // Move-construct |movedIterator| from the iterator returned from
  // GetIterator() and check that its state is as we expect.
  SourceBufferIterator tmpIterator = GetIterator();
  SourceBufferIterator movedIterator(std::move(tmpIterator));
  EXPECT_TRUE(movedIterator.Data());
  EXPECT_EQ(chunkLength, movedIterator.Length());
  ExpectChunkAndByteCount(movedIterator, 1, chunkLength);

  // Make sure that we can advance the iterator.
  CheckedAdvanceIterator(movedIterator, chunkLength, 2, totalLength);

  // Make sure that the iterator handles completion properly.
  CheckIteratorIsComplete(movedIterator, 2, totalLength);

  // Move-assign |movedIterator| from the iterator returned from
  // GetIterator() and check that its state is as we expect.
  tmpIterator = GetIterator();
  movedIterator = std::move(tmpIterator);
  EXPECT_TRUE(movedIterator.Data());
  EXPECT_EQ(chunkLength, movedIterator.Length());
  ExpectChunkAndByteCount(movedIterator, 1, chunkLength);

  // Make sure that we can advance the iterator.
  CheckedAdvanceIterator(movedIterator, chunkLength, 2, totalLength);

  // Make sure that the iterator handles completion properly.
  CheckIteratorIsComplete(movedIterator, 2, totalLength);
}

TEST_F(ImageSourceBuffer, SubchunkAdvance)
{
  constexpr size_t chunkLength = SourceBuffer::MIN_CHUNK_CAPACITY;
  constexpr size_t totalLength = 2 * chunkLength;

  // Write enough data to create two chunks. We create our iterator here to make
  // sure that compaction doesn't happen during the test.
  SourceBufferIterator iterator = mSourceBuffer->Iterator();
  CheckedAppendToBufferInChunks(chunkLength, totalLength);
  CheckedCompleteBuffer(iterator, totalLength);

  // Advance through the first chunk. The chunk count should not increase.
  // We check that by always passing 1 for the |aChunks| parameter of
  // CheckedAdvanceIteratorStateOnly(). We have to call CheckData() manually
  // because the offset calculation in CheckedAdvanceIterator() assumes that
  // we're advancing a chunk at a time.
  size_t offset = 0;
  while (offset < chunkLength) {
    CheckedAdvanceIteratorStateOnly(iterator, 1, 1, chunkLength,
                                    AdvanceMode::eAdvanceByLengthExactly);
    CheckData(iterator.Data(), offset++, iterator.Length());
  }

  // Read the first byte of the second chunk. This is the point at which we
  // can't advance within the same chunk, so the chunk count should increase. We
  // check that by passing 2 for the |aChunks| parameter of
  // CheckedAdvanceIteratorStateOnly().
  CheckedAdvanceIteratorStateOnly(iterator, 1, 2, totalLength,
                                  AdvanceMode::eAdvanceByLengthExactly);
  CheckData(iterator.Data(), offset++, iterator.Length());

  // Read the rest of the second chunk. The chunk count should not increase.
  while (offset < totalLength) {
    CheckedAdvanceIteratorStateOnly(iterator, 1, 2, totalLength,
                                    AdvanceMode::eAdvanceByLengthExactly);
    CheckData(iterator.Data(), offset++, iterator.Length());
  }

  // Make sure we reached the end.
  CheckIteratorIsComplete(iterator, 2, totalLength);
}

TEST_F(ImageSourceBuffer, SubchunkZeroByteAdvance)
{
  constexpr size_t chunkLength = SourceBuffer::MIN_CHUNK_CAPACITY;
  constexpr size_t totalLength = 2 * chunkLength;

  // Write enough data to create two chunks. We create our iterator here to make
  // sure that compaction doesn't happen during the test.
  SourceBufferIterator iterator = mSourceBuffer->Iterator();
  CheckedAppendToBufferInChunks(chunkLength, totalLength);
  CheckedCompleteBuffer(iterator, totalLength);

  // Make an initial zero-length advance. Although a zero-length advance
  // normally won't cause us to read a chunk from the SourceBuffer, we'll do so
  // if the iterator is in the initial state to keep the invariant that
  // SourceBufferIterator in the READY state always returns a non-null pointer
  // from Data().
  CheckedAdvanceIteratorStateOnly(iterator, 0, 1, chunkLength,
                                  AdvanceMode::eAdvanceByLengthExactly);

  // Advance through the first chunk. As in the |SubchunkAdvance| test, the
  // chunk count should not increase. We do a zero-length advance after each
  // normal advance to ensure that zero-length advances do not change the
  // iterator's position or cause a new chunk to be read.
  size_t offset = 0;
  while (offset < chunkLength) {
    CheckedAdvanceIteratorStateOnly(iterator, 1, 1, chunkLength,
                                    AdvanceMode::eAdvanceByLengthExactly);
    CheckData(iterator.Data(), offset++, iterator.Length());
    CheckedAdvanceIteratorStateOnly(iterator, 0, 1, chunkLength,
                                    AdvanceMode::eAdvanceByLengthExactly);
  }

  // Read the first byte of the second chunk. This is the point at which we
  // can't advance within the same chunk, so the chunk count should increase. As
  // before, we do a zero-length advance afterward.
  CheckedAdvanceIteratorStateOnly(iterator, 1, 2, totalLength,
                                  AdvanceMode::eAdvanceByLengthExactly);
  CheckData(iterator.Data(), offset++, iterator.Length());
  CheckedAdvanceIteratorStateOnly(iterator, 0, 2, totalLength,
                                  AdvanceMode::eAdvanceByLengthExactly);

  // Read the rest of the second chunk. The chunk count should not increase. As
  // before, we do a zero-length advance after each normal advance.
  while (offset < totalLength) {
    CheckedAdvanceIteratorStateOnly(iterator, 1, 2, totalLength,
                                    AdvanceMode::eAdvanceByLengthExactly);
    CheckData(iterator.Data(), offset++, iterator.Length());
    CheckedAdvanceIteratorStateOnly(iterator, 0, 2, totalLength,
                                    AdvanceMode::eAdvanceByLengthExactly);
  }

  // Make sure we reached the end.
  CheckIteratorIsComplete(iterator, 2, totalLength);
}

TEST_F(ImageSourceBuffer, SubchunkZeroByteAdvanceWithNoData)
{
  SourceBufferIterator iterator = mSourceBuffer->Iterator();

  // Check that advancing by zero bytes still makes us enter the WAITING state.
  // This is because if we entered the READY state before reading any data at
  // all, we'd break the invariant that SourceBufferIterator::Data() always
  // returns a non-null pointer in the READY state.
  auto state = iterator.AdvanceOrScheduleResume(0, mCountResumes);
  EXPECT_EQ(SourceBufferIterator::WAITING, state);

  // Call Complete(). This should trigger a resume.
  CheckedCompleteBuffer();
  EXPECT_EQ(1u, mCountResumes->Count());
}

TEST_F(ImageSourceBuffer, NullIResumable)
{
  SourceBufferIterator iterator = mSourceBuffer->Iterator();

  // Check that we can't advance.
  CheckIteratorMustWait(iterator, nullptr);

  // Append to the buffer, which would cause a resume if we had passed a
  // non-null IResumable.
  CheckedAppendToBuffer(mData, sizeof(mData));
  CheckedCompleteBuffer(iterator, sizeof(mData));
}

TEST_F(ImageSourceBuffer, AppendTriggersResume)
{
  SourceBufferIterator iterator = mSourceBuffer->Iterator();

  // Check that we can't advance.
  CheckIteratorMustWait(iterator, mCountResumes);

  // Call Append(). This should trigger a resume.
  mSourceBuffer->Append(mData, sizeof(mData));
  EXPECT_EQ(1u, mCountResumes->Count());
}

TEST_F(ImageSourceBuffer, OnlyOneResumeTriggeredPerAppend)
{
  SourceBufferIterator iterator = mSourceBuffer->Iterator();

  // Check that we can't advance.
  CheckIteratorMustWait(iterator, mCountResumes);

  // Allocate some data we'll use below.
  constexpr size_t firstWriteLength = SourceBuffer::MIN_CHUNK_CAPACITY / 2;
  constexpr size_t secondWriteLength = 3 * SourceBuffer::MIN_CHUNK_CAPACITY;
  constexpr size_t totalLength = firstWriteLength + secondWriteLength;
  char data[totalLength];
  GenerateData(data, sizeof(data));

  // Write half of SourceBuffer::MIN_CHUNK_CAPACITY bytes of test data to the
  // buffer in a single Append() call. This should fill half of the first chunk.
  // This should trigger a resume.
  CheckedAppendToBuffer(data, firstWriteLength);
  EXPECT_EQ(1u, mCountResumes->Count());

  // Advance past the new data and wait again.
  CheckedAdvanceIterator(iterator, firstWriteLength);
  CheckIteratorMustWait(iterator, mCountResumes);

  // Write three times SourceBuffer::MIN_CHUNK_CAPACITY bytes of test data to the
  // buffer in a single Append() call. We expect this to result in the first of
  // the first chunk being filled and a new chunk being allocated for the
  // remainder. Even though two chunks are getting written to here, only *one*
  // resume should get triggered, for a total of two in this test.
  CheckedAppendToBuffer(data + firstWriteLength, secondWriteLength);
  EXPECT_EQ(2u, mCountResumes->Count());
}

TEST_F(ImageSourceBuffer, CompleteTriggersResume)
{
  SourceBufferIterator iterator = mSourceBuffer->Iterator();

  // Check that we can't advance.
  CheckIteratorMustWait(iterator, mCountResumes);

  // Call Complete(). This should trigger a resume.
  CheckedCompleteBuffer();
  EXPECT_EQ(1u, mCountResumes->Count());
}

TEST_F(ImageSourceBuffer, ExpectLengthDoesNotTriggerResume)
{
  SourceBufferIterator iterator = mSourceBuffer->Iterator();

  // Check that we can't advance.
  CheckIteratorMustWait(iterator, mExpectNoResume);

  // Call ExpectLength(). If this triggers a resume, |mExpectNoResume| will
  // ensure that the test fails.
  mSourceBuffer->ExpectLength(1000);
}

TEST_F(ImageSourceBuffer, CompleteSuccessWithSameReadLength)
{
  SourceBufferIterator iterator = mSourceBuffer->Iterator(1);

  // Write a single byte to the buffer and complete the buffer. (We have to
  // write at least one byte because completing a zero length buffer always
  // fails; see the ZeroLengthBufferAlwaysFails test.)
  CheckedAppendToBuffer(mData, 1);
  CheckedCompleteBuffer(iterator, 1);

  // We should be able to advance once (to read the single byte) and then should
  // reach the COMPLETE state with a successful status.
  CheckedAdvanceIterator(iterator, 1);
  CheckIteratorIsComplete(iterator, 1);
}

TEST_F(ImageSourceBuffer, CompleteSuccessWithSmallerReadLength)
{
  // Create an iterator limited to one byte.
  SourceBufferIterator iterator = mSourceBuffer->Iterator(1);

  // Write two bytes to the buffer and complete the buffer. (We have to
  // write at least one byte because completing a zero length buffer always
  // fails; see the ZeroLengthBufferAlwaysFails test.)
  CheckedAppendToBuffer(mData, 2);
  CheckedCompleteBuffer(iterator, 2);

  // We should be able to advance once (to read the single byte) and then should
  // reach the COMPLETE state with a successful status, because our iterator is
  // limited to a single byte, rather than the full length.
  CheckedAdvanceIterator(iterator, 1);
  CheckIteratorIsComplete(iterator, 1);
}

TEST_F(ImageSourceBuffer, CompleteSuccessWithGreaterReadLength)
{
  // Create an iterator limited to one byte.
  SourceBufferIterator iterator = mSourceBuffer->Iterator(2);

  // Write a single byte to the buffer and complete the buffer. (We have to
  // write at least one byte because completing a zero length buffer always
  // fails; see the ZeroLengthBufferAlwaysFails test.)
  CheckedAppendToBuffer(mData, 1);
  CheckedCompleteBuffer(iterator, 1);

  // We should be able to advance once (to read the single byte) and then should
  // reach the COMPLETE state with a successful status. Our iterator lets us
  // read more but the underlying buffer has been completed.
  CheckedAdvanceIterator(iterator, 1);
  CheckIteratorIsComplete(iterator, 1);
}
