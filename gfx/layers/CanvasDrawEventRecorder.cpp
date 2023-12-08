/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasDrawEventRecorder.h"

#include <string.h>

#include "mozilla/layers/SharedSurfacesChild.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace layers {

static const uint32_t kMaxSpinCount = 200;

static const TimeDuration kTimeout = TimeDuration::FromMilliseconds(100);
static const int32_t kTimeoutRetryCount = 50;

static const uint32_t kCacheLineSize = 64;
static const uint32_t kSmallStreamSize = 64 * 1024;
static const uint32_t kLargeStreamSize = 2048 * 1024;

static_assert((static_cast<uint64_t>(UINT32_MAX) + 1) % kSmallStreamSize == 0,
              "kSmallStreamSize must be a power of two.");
static_assert((static_cast<uint64_t>(UINT32_MAX) + 1) % kLargeStreamSize == 0,
              "kLargeStreamSize must be a power of two.");

uint32_t CanvasEventRingBuffer::StreamSize() {
  return mLargeStream ? kLargeStreamSize : kSmallStreamSize;
}

bool CanvasEventRingBuffer::InitBuffer(
    base::ProcessId aOtherPid, ipc::SharedMemoryBasic::Handle* aReadHandle) {
  size_t shmemSize = StreamSize() + (2 * kCacheLineSize);
  mSharedMemory = MakeAndAddRef<ipc::SharedMemoryBasic>();
  if (NS_WARN_IF(!mSharedMemory->Create(shmemSize)) ||
      NS_WARN_IF(!mSharedMemory->Map(shmemSize))) {
    mGood = false;
    return false;
  }

  *aReadHandle = mSharedMemory->TakeHandle();
  if (NS_WARN_IF(!*aReadHandle)) {
    mGood = false;
    return false;
  }

  mBuf = static_cast<char*>(mSharedMemory->memory());
  mBufPos = mBuf;
  mAvailable = StreamSize();

  static_assert(sizeof(ReadFooter) <= kCacheLineSize,
                "ReadFooter must fit in kCacheLineSize.");
  mRead = reinterpret_cast<ReadFooter*>(mBuf + StreamSize());
  mRead->count = 0;
  mRead->returnCount = 0;
  mRead->state = State::Processing;

  static_assert(sizeof(WriteFooter) <= kCacheLineSize,
                "WriteFooter must fit in kCacheLineSize.");
  mWrite = reinterpret_cast<WriteFooter*>(mBuf + StreamSize() + kCacheLineSize);
  mWrite->count = 0;
  mWrite->returnCount = 0;
  mWrite->requiredDifference = 0;
  mWrite->state = State::Processing;
  mOurCount = 0;

  return true;
}

bool CanvasEventRingBuffer::InitWriter(
    base::ProcessId aOtherPid, ipc::SharedMemoryBasic::Handle* aReadHandle,
    CrossProcessSemaphoreHandle* aReaderSem,
    CrossProcessSemaphoreHandle* aWriterSem,
    UniquePtr<WriterServices> aWriterServices) {
  if (!InitBuffer(aOtherPid, aReadHandle)) {
    return false;
  }

  mReaderSemaphore.reset(
      CrossProcessSemaphore::Create("SharedMemoryStreamParent", 0));
  *aReaderSem = mReaderSemaphore->CloneHandle();
  mReaderSemaphore->CloseHandle();
  if (!IsHandleValid(*aReaderSem)) {
    return false;
  }
  mWriterSemaphore.reset(
      CrossProcessSemaphore::Create("SharedMemoryStreamChild", 0));
  *aWriterSem = mWriterSemaphore->CloneHandle();
  mWriterSemaphore->CloseHandle();
  if (!IsHandleValid(*aWriterSem)) {
    return false;
  }

  mWriterServices = std::move(aWriterServices);

  mGood = true;
  return true;
}

bool CanvasEventRingBuffer::InitReader(
    ipc::SharedMemoryBasic::Handle aReadHandle,
    CrossProcessSemaphoreHandle aReaderSem,
    CrossProcessSemaphoreHandle aWriterSem,
    UniquePtr<ReaderServices> aReaderServices) {
  if (!SetNewBuffer(std::move(aReadHandle))) {
    return false;
  }

  mReaderSemaphore.reset(CrossProcessSemaphore::Create(std::move(aReaderSem)));
  mReaderSemaphore->CloseHandle();
  mWriterSemaphore.reset(CrossProcessSemaphore::Create(std::move(aWriterSem)));
  mWriterSemaphore->CloseHandle();

  mReaderServices = std::move(aReaderServices);

  mGood = true;
  return true;
}

bool CanvasEventRingBuffer::SetNewBuffer(
    ipc::SharedMemoryBasic::Handle aReadHandle) {
  MOZ_RELEASE_ASSERT(
      !mSharedMemory,
      "Shared memory should have been dropped before new buffer is sent.");

  size_t shmemSize = StreamSize() + (2 * kCacheLineSize);
  mSharedMemory = MakeAndAddRef<ipc::SharedMemoryBasic>();
  if (NS_WARN_IF(!mSharedMemory->SetHandle(
          std::move(aReadHandle), ipc::SharedMemory::RightsReadWrite)) ||
      NS_WARN_IF(!mSharedMemory->Map(shmemSize))) {
    mGood = false;
    return false;
  }

  mSharedMemory->CloseHandle();

  mBuf = static_cast<char*>(mSharedMemory->memory());
  mRead = reinterpret_cast<ReadFooter*>(mBuf + StreamSize());
  mWrite = reinterpret_cast<WriteFooter*>(mBuf + StreamSize() + kCacheLineSize);
  mOurCount = 0;
  return true;
}

bool CanvasEventRingBuffer::WaitForAndRecalculateAvailableSpace() {
  if (!good()) {
    return false;
  }

  uint32_t bufPos = mOurCount % StreamSize();
  uint32_t maxToWrite = StreamSize() - bufPos;
  mAvailable = std::min(maxToWrite, WaitForBytesToWrite());
  if (!mAvailable) {
    mBufPos = nullptr;
    return false;
  }

  mBufPos = mBuf + bufPos;
  return true;
}

void CanvasEventRingBuffer::write(const char* const aData, const size_t aSize) {
  const char* curDestPtr = aData;
  size_t remainingToWrite = aSize;
  if (remainingToWrite > mAvailable) {
    if (!WaitForAndRecalculateAvailableSpace()) {
      return;
    }
  }

  if (remainingToWrite <= mAvailable) {
    memcpy(mBufPos, curDestPtr, remainingToWrite);
    UpdateWriteTotalsBy(remainingToWrite);
    return;
  }

  do {
    memcpy(mBufPos, curDestPtr, mAvailable);
    IncrementWriteCountBy(mAvailable);
    curDestPtr += mAvailable;
    remainingToWrite -= mAvailable;
    if (!WaitForAndRecalculateAvailableSpace()) {
      return;
    }
  } while (remainingToWrite > mAvailable);

  memcpy(mBufPos, curDestPtr, remainingToWrite);
  UpdateWriteTotalsBy(remainingToWrite);
}

void CanvasEventRingBuffer::IncrementWriteCountBy(uint32_t aCount) {
  mOurCount += aCount;
  mWrite->count = mOurCount;
  if (mRead->state != State::Processing) {
    CheckAndSignalReader();
  }
}

void CanvasEventRingBuffer::UpdateWriteTotalsBy(uint32_t aCount) {
  IncrementWriteCountBy(aCount);
  mBufPos += aCount;
  mAvailable -= aCount;
}

bool CanvasEventRingBuffer::WaitForAndRecalculateAvailableData() {
  if (!good()) {
    return false;
  }

  uint32_t bufPos = mOurCount % StreamSize();
  uint32_t maxToRead = StreamSize() - bufPos;
  mAvailable = std::min(maxToRead, WaitForBytesToRead());
  if (!mAvailable) {
    SetIsBad();
    mBufPos = nullptr;
    return false;
  }

  mBufPos = mBuf + bufPos;
  return true;
}

void CanvasEventRingBuffer::read(char* const aOut, const size_t aSize) {
  char* curSrcPtr = aOut;
  size_t remainingToRead = aSize;
  if (remainingToRead > mAvailable) {
    if (!WaitForAndRecalculateAvailableData()) {
      return;
    }
  }

  if (remainingToRead <= mAvailable) {
    memcpy(curSrcPtr, mBufPos, remainingToRead);
    UpdateReadTotalsBy(remainingToRead);
    return;
  }

  do {
    memcpy(curSrcPtr, mBufPos, mAvailable);
    IncrementReadCountBy(mAvailable);
    curSrcPtr += mAvailable;
    remainingToRead -= mAvailable;
    if (!WaitForAndRecalculateAvailableData()) {
      return;
    }
  } while (remainingToRead > mAvailable);

  memcpy(curSrcPtr, mBufPos, remainingToRead);
  UpdateReadTotalsBy(remainingToRead);
}

void CanvasEventRingBuffer::IncrementReadCountBy(uint32_t aCount) {
  mOurCount += aCount;
  mRead->count = mOurCount;
  if (mWrite->state != State::Processing) {
    CheckAndSignalWriter();
  }
}

void CanvasEventRingBuffer::UpdateReadTotalsBy(uint32_t aCount) {
  IncrementReadCountBy(aCount);
  mBufPos += aCount;
  mAvailable -= aCount;
}

void CanvasEventRingBuffer::CheckAndSignalReader() {
  do {
    switch (mRead->state) {
      case State::Processing:
      case State::Failed:
        return;
      case State::AboutToWait:
        // The reader is making a decision about whether to wait. So, we must
        // wait until it has decided to avoid races. Check if the reader is
        // closed to avoid hangs.
        if (mWriterServices->ReaderClosed()) {
          return;
        }
        continue;
      case State::Waiting:
        if (mRead->count != mOurCount) {
          // We have to use compareExchange here because the reader can change
          // from Waiting to Stopped.
          if (mRead->state.compareExchange(State::Waiting, State::Processing)) {
            mReaderSemaphore->Signal();
            return;
          }

          MOZ_ASSERT(mRead->state == State::Stopped);
          continue;
        }
        return;
      case State::Stopped:
        if (mRead->count != mOurCount) {
          mRead->state = State::Processing;
          mWriterServices->ResumeReader();
        }
        return;
      default:
        MOZ_ASSERT_UNREACHABLE("Invalid waiting state.");
        return;
    }
  } while (true);
}

bool CanvasEventRingBuffer::HasDataToRead() {
  return (mWrite->count != mOurCount);
}

bool CanvasEventRingBuffer::StopIfEmpty() {
  // Double-check that the writer isn't waiting.
  CheckAndSignalWriter();
  mRead->state = State::AboutToWait;
  if (HasDataToRead()) {
    mRead->state = State::Processing;
    return false;
  }

  mRead->state = State::Stopped;
  return true;
}

bool CanvasEventRingBuffer::WaitForDataToRead(TimeDuration aTimeout,
                                              int32_t aRetryCount) {
  uint32_t spinCount = kMaxSpinCount;
  do {
    if (HasDataToRead()) {
      return true;
    }
  } while (--spinCount != 0);

  // Double-check that the writer isn't waiting.
  CheckAndSignalWriter();
  mRead->state = State::AboutToWait;
  if (HasDataToRead()) {
    mRead->state = State::Processing;
    return true;
  }

  mRead->state = State::Waiting;
  do {
    if (mReaderSemaphore->Wait(Some(aTimeout))) {
      MOZ_RELEASE_ASSERT(HasDataToRead());
      return true;
    }

    if (mReaderServices->WriterClosed()) {
      // Something has gone wrong on the writing side, just return false so
      // that we can hopefully recover.
      return false;
    }
  } while (aRetryCount-- > 0);

  // We have to use compareExchange here because the writer can change our
  // state if we are waiting. signaled
  if (!mRead->state.compareExchange(State::Waiting, State::Stopped)) {
    MOZ_RELEASE_ASSERT(HasDataToRead());
    MOZ_RELEASE_ASSERT(mRead->state == State::Processing);
    // The writer has just signaled us, so consume it before returning
    MOZ_ALWAYS_TRUE(mReaderSemaphore->Wait());
    return true;
  }

  return false;
}

uint8_t CanvasEventRingBuffer::ReadNextEvent() {
  uint8_t nextEvent;
  ReadElement(*this, nextEvent);
  while (nextEvent == kCheckpointEventType && good()) {
    ReadElement(*this, nextEvent);
  }

  if (nextEvent == kDropBufferEventType) {
    // Writer is switching to a different sized buffer.
    mBuf = nullptr;
    mBufPos = nullptr;
    mRead = nullptr;
    mWrite = nullptr;
    mAvailable = 0;
    mSharedMemory = nullptr;
    // We always toggle between smaller and larger stream sizes.
    mLargeStream = !mLargeStream;
  }

  return nextEvent;
}

uint32_t CanvasEventRingBuffer::CreateCheckpoint() {
  WriteElement(*this, kCheckpointEventType);
  return mOurCount;
}

bool CanvasEventRingBuffer::WaitForCheckpoint(uint32_t aCheckpoint) {
  return WaitForReadCount(aCheckpoint, kTimeout);
}

bool CanvasEventRingBuffer::SwitchBuffer(
    base::ProcessId aOtherPid, ipc::SharedMemoryBasic::Handle* aReadHandle) {
  WriteElement(*this, kDropBufferEventType);

  // Make sure the drop buffer event has been read before continuing. We can't
  // write an actual checkpoint because there will be no buffer to read from.
  if (!WaitForCheckpoint(mOurCount)) {
    return false;
  }

  mBuf = nullptr;
  mBufPos = nullptr;
  mRead = nullptr;
  mWrite = nullptr;
  mAvailable = 0;
  mSharedMemory = nullptr;
  // We always toggle between smaller and larger stream sizes.
  mLargeStream = !mLargeStream;
  return InitBuffer(aOtherPid, aReadHandle);
}

void CanvasEventRingBuffer::CheckAndSignalWriter() {
  do {
    switch (mWrite->state) {
      case State::Processing:
        return;
      case State::AboutToWait:
        // The writer is making a decision about whether to wait. So, we must
        // wait until it has decided to avoid races. Check if the writer is
        // closed to avoid hangs.
        if (mReaderServices->WriterClosed()) {
          return;
        }
        continue;
      case State::Waiting:
        if (mWrite->count - mOurCount <= mWrite->requiredDifference) {
          mWrite->state = State::Processing;
          mWriterSemaphore->Signal();
        }
        return;
      default:
        MOZ_ASSERT_UNREACHABLE("Invalid waiting state.");
        return;
    }
  } while (true);
}

bool CanvasEventRingBuffer::WaitForReadCount(uint32_t aReadCount,
                                             TimeDuration aTimeout) {
  uint32_t requiredDifference = mOurCount - aReadCount;
  uint32_t spinCount = kMaxSpinCount;
  do {
    if (mOurCount - mRead->count <= requiredDifference) {
      return true;
    }
  } while (--spinCount != 0);

  // Double-check that the reader isn't waiting.
  CheckAndSignalReader();
  mWrite->state = State::AboutToWait;
  if (mOurCount - mRead->count <= requiredDifference) {
    mWrite->state = State::Processing;
    return true;
  }

  mWrite->requiredDifference = requiredDifference;
  mWrite->state = State::Waiting;

  // Wait unless we detect the reading side has closed.
  while (!mWriterServices->ReaderClosed() && mRead->state != State::Failed) {
    if (mWriterSemaphore->Wait(Some(aTimeout))) {
      MOZ_ASSERT(mOurCount - mRead->count <= requiredDifference);
      return true;
    }
  }

  // Either the reader has failed or we're stopping writing for some other
  // reason (e.g. shutdown), so mark us as failed so the reader is aware.
  mWrite->state = State::Failed;
  mGood = false;
  return false;
}

uint32_t CanvasEventRingBuffer::WaitForBytesToWrite() {
  uint32_t streamFullReadCount = mOurCount - StreamSize();
  if (!WaitForReadCount(streamFullReadCount + 1, kTimeout)) {
    return 0;
  }

  return mRead->count - streamFullReadCount;
}

uint32_t CanvasEventRingBuffer::WaitForBytesToRead() {
  if (!WaitForDataToRead(kTimeout, kTimeoutRetryCount)) {
    return 0;
  }

  return mWrite->count - mOurCount;
}

void CanvasEventRingBuffer::ReturnWrite(const char* aData, size_t aSize) {
  uint32_t writeCount = mRead->returnCount;
  uint32_t bufPos = writeCount % StreamSize();
  uint32_t bufRemaining = StreamSize() - bufPos;
  uint32_t availableToWrite =
      std::min(bufRemaining, (mWrite->returnCount + StreamSize() - writeCount));
  while (availableToWrite < aSize) {
    if (availableToWrite) {
      memcpy(mBuf + bufPos, aData, availableToWrite);
      writeCount += availableToWrite;
      mRead->returnCount = writeCount;
      bufPos = writeCount % StreamSize();
      bufRemaining = StreamSize() - bufPos;
      aData += availableToWrite;
      aSize -= availableToWrite;
    } else if (mReaderServices->WriterClosed()) {
      return;
    }

    availableToWrite = std::min(
        bufRemaining, (mWrite->returnCount + StreamSize() - writeCount));
  }

  memcpy(mBuf + bufPos, aData, aSize);
  writeCount += aSize;
  mRead->returnCount = writeCount;
}

void CanvasEventRingBuffer::ReturnRead(char* aOut, size_t aSize) {
  // First wait for the event returning the data to be read.
  WaitForCheckpoint(mOurCount);
  uint32_t readCount = mWrite->returnCount;

  // If the event sending back data fails to play then it will ReturnWrite
  // nothing. So, wait until something has been written or the reader has
  // stopped processing.
  while (readCount == mRead->returnCount) {
    // We recheck the count, because the other side can write all the data and
    // started waiting in between these two lines.
    if (mRead->state != State::Processing && readCount == mRead->returnCount) {
      return;
    }
  }

  uint32_t bufPos = readCount % StreamSize();
  uint32_t bufRemaining = StreamSize() - bufPos;
  uint32_t availableToRead =
      std::min(bufRemaining, (mRead->returnCount - readCount));
  while (availableToRead < aSize) {
    if (availableToRead) {
      memcpy(aOut, mBuf + bufPos, availableToRead);
      readCount += availableToRead;
      mWrite->returnCount = readCount;
      bufPos = readCount % StreamSize();
      bufRemaining = StreamSize() - bufPos;
      aOut += availableToRead;
      aSize -= availableToRead;
    } else if (mWriterServices->ReaderClosed()) {
      return;
    }

    availableToRead = std::min(bufRemaining, (mRead->returnCount - readCount));
  }

  memcpy(aOut, mBuf + bufPos, aSize);
  readCount += aSize;
  mWrite->returnCount = readCount;
}

void CanvasDrawEventRecorder::StoreSourceSurfaceRecording(
    gfx::SourceSurface* aSurface, const char* aReason) {
  wr::ExternalImageId extId{};
  nsresult rv = layers::SharedSurfacesChild::Share(aSurface, extId);
  if (NS_FAILED(rv)) {
    DrawEventRecorderPrivate::StoreSourceSurfaceRecording(aSurface, aReason);
    return;
  }

  StoreExternalSurfaceRecording(aSurface, wr::AsUint64(extId));
}

}  // namespace layers
}  // namespace mozilla
