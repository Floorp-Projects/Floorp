/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileBlockCache.h"
#include "mozilla/SharedThreadPool.h"
#include "VideoUtils.h"
#include "prio.h"
#include <algorithm>
#include "nsAnonymousTemporaryFile.h"
#include "mozilla/dom/ContentChild.h"
#include "nsXULAppAPI.h"

namespace mozilla {

#undef LOG
LazyLogModule gFileBlockCacheLog("FileBlockCache");
#define LOG(x, ...) MOZ_LOG(gFileBlockCacheLog, LogLevel::Debug, \
  ("%p " x, this, ##__VA_ARGS__))

static void
CloseFD(PRFileDesc* aFD)
{
  PRStatus prrc;
  prrc = PR_Close(aFD);
  if (prrc != PR_SUCCESS) {
    NS_WARNING("PR_Close() failed.");
  }
}

void
FileBlockCache::SetCacheFile(PRFileDesc* aFD)
{
  LOG("SetFD(aFD=%p) mThread=%p", aFD, mThread.get());

  if (!aFD) {
    // Failed to get a temporary file. Shutdown.
    Close();
    return;
  }
  {
    MutexAutoLock lock(mFileMutex);
    mFD = aFD;
  }
  {
    MutexAutoLock lock(mDataMutex);
    if (mThread) {
      // Still open, complete the initialization.
      mInitialized = true;
      if (mIsWriteScheduled) {
        // A write was scheduled while waiting for FD. We need to run/dispatch a
        // task to service the request.
        nsCOMPtr<nsIRunnable> event = mozilla::NewRunnableMethod(
          "FileBlockCache::SetCacheFile -> PerformBlockIOs",
          this,
          &FileBlockCache::PerformBlockIOs);
        mThread->Dispatch(event.forget(), NS_DISPATCH_NORMAL);
      }
      return;
    }
  }
  // We've been closed while waiting for the file descriptor.
  // Close the file descriptor we've just received, if still there.
  MutexAutoLock lock(mFileMutex);
  if (mFD) {
    CloseFD(mFD);
    mFD = nullptr;
  }
}

nsresult
FileBlockCache::Init()
{
  MutexAutoLock mon(mDataMutex);
  if (mThread) {
    LOG("Init() again");
    // Just discard pending changes, assume MediaCache won't read from
    // blocks it hasn't written to.
    mChangeIndexList.clear();
    mBlockChanges.Clear();
    return NS_OK;
  }

  LOG("Init()");

  nsresult rv = NS_NewNamedThread("FileBlockCache",
                                  getter_AddRefs(mThread),
                                  nullptr,
                                  SharedThreadPool::kStackSize);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (XRE_IsParentProcess()) {
    RefPtr<FileBlockCache> self = this;
    rv = mThread->Dispatch(
      NS_NewRunnableFunction("FileBlockCache::Init",
                             [self] {
                               PRFileDesc* fd = nullptr;
                               nsresult rv = NS_OpenAnonymousTemporaryFile(&fd);
                               if (NS_SUCCEEDED(rv)) {
                                 self->SetCacheFile(fd);
                               } else {
                                 self->Close();
                               }
                             }),
      NS_DISPATCH_NORMAL);
  } else {
    // We must request a temporary file descriptor from the parent process.
    RefPtr<FileBlockCache> self = this;
    rv = dom::ContentChild::GetSingleton()->AsyncOpenAnonymousTemporaryFile(
      [self](PRFileDesc* aFD) { self->SetCacheFile(aFD); });
  }

  if (NS_FAILED(rv)) {
    Close();
  }

  return rv;
}

FileBlockCache::FileBlockCache()
  : mFileMutex("MediaCache.Writer.IO.Mutex")
  , mFD(nullptr)
  , mFDCurrentPos(0)
  , mDataMutex("MediaCache.Writer.Data.Mutex")
  , mIsWriteScheduled(false)
  , mIsReading(false)
{
}

FileBlockCache::~FileBlockCache()
{
  Close();
}

void
FileBlockCache::Close()
{
  LOG("Close()");

  nsCOMPtr<nsIThread> thread;
  {
    MutexAutoLock mon(mDataMutex);
    if (!mThread) {
      return;
    }
    thread.swap(mThread);
  }

  PRFileDesc* fd;
  {
    MutexAutoLock lock(mFileMutex);
    fd = mFD;
    mFD = nullptr;
  }

  // Let the thread close the FD, and then trigger its own shutdown.
  // Note that mThread is now empty, so no other task will be posted there.
  // Also mThread and mFD are empty and therefore can be reused immediately.
  nsresult rv = thread->Dispatch(
    NS_NewRunnableFunction("FileBlockCache::Close",
                           [thread, fd] {
                             if (fd) {
                               CloseFD(fd);
                             }
                             // We must shut down the thread in another
                             // runnable. This is called
                             // while we're shutting down the media cache, and
                             // nsIThread::Shutdown()
                             // can cause events to run before it completes,
                             // which could end up
                             // opening more streams, while the media cache is
                             // shutting down and
                             // releasing memory etc!
                             nsCOMPtr<nsIRunnable> event =
                               new ShutdownThreadEvent(thread);
                             SystemGroup::Dispatch("ShutdownThreadEvent",
                                                   TaskCategory::Other,
                                                   event.forget());
                           }),
    NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS_VOID(rv);
}

template<typename Container, typename Value>
bool
ContainerContains(const Container& aContainer, const Value& value)
{
  return std::find(aContainer.begin(), aContainer.end(), value)
         != aContainer.end();
}

nsresult
FileBlockCache::WriteBlock(uint32_t aBlockIndex,
  Span<const uint8_t> aData1, Span<const uint8_t> aData2)
{
  MutexAutoLock mon(mDataMutex);

  if (!mThread) {
    return NS_ERROR_FAILURE;
  }

  // Check if we've already got a pending write scheduled for this block.
  mBlockChanges.EnsureLengthAtLeast(aBlockIndex + 1);
  bool blockAlreadyHadPendingChange = mBlockChanges[aBlockIndex] != nullptr;
  mBlockChanges[aBlockIndex] = new BlockChange(aData1, aData2);

  if (!blockAlreadyHadPendingChange || !ContainerContains(mChangeIndexList, aBlockIndex)) {
    // We either didn't already have a pending change for this block, or we
    // did but we didn't have an entry for it in mChangeIndexList (we're in the process
    // of writing it and have removed the block's index out of mChangeIndexList
    // in Run() but not finished writing the block to file yet). Add the blocks
    // index to the end of mChangeIndexList to ensure the block is written as
    // as soon as possible.
    mChangeIndexList.push_back(aBlockIndex);
  }
  NS_ASSERTION(ContainerContains(mChangeIndexList, aBlockIndex), "Must have entry for new block");

  EnsureWriteScheduled();

  return NS_OK;
}

void FileBlockCache::EnsureWriteScheduled()
{
  mDataMutex.AssertCurrentThreadOwns();
  MOZ_ASSERT(mThread);

  if (mIsWriteScheduled || mIsReading) {
    return;
  }
  mIsWriteScheduled = true;
  if (!mInitialized) {
    // We're still waiting on a file descriptor. When it arrives,
    // the write will be scheduled.
    return;
  }
  nsCOMPtr<nsIRunnable> event = mozilla::NewRunnableMethod(
    "FileBlockCache::EnsureWriteScheduled -> PerformBlockIOs",
    this,
    &FileBlockCache::PerformBlockIOs);
  mThread->Dispatch(event.forget(), NS_DISPATCH_NORMAL);
}

nsresult FileBlockCache::Seek(int64_t aOffset)
{
  mFileMutex.AssertCurrentThreadOwns();

  if (mFDCurrentPos != aOffset) {
    MOZ_ASSERT(mFD);
    int64_t result = PR_Seek64(mFD, aOffset, PR_SEEK_SET);
    if (result != aOffset) {
      NS_WARNING("Failed to seek media cache file");
      return NS_ERROR_FAILURE;
    }
    mFDCurrentPos = result;
  }
  return NS_OK;
}

nsresult FileBlockCache::ReadFromFile(int64_t aOffset,
                                      uint8_t* aDest,
                                      int32_t aBytesToRead,
                                      int32_t& aBytesRead)
{
  LOG("ReadFromFile(offset=%" PRIu64 ", len=%u)", aOffset, aBytesToRead);
  mFileMutex.AssertCurrentThreadOwns();
  MOZ_ASSERT(mFD);

  nsresult res = Seek(aOffset);
  if (NS_FAILED(res)) return res;

  aBytesRead = PR_Read(mFD, aDest, aBytesToRead);
  if (aBytesRead <= 0)
    return NS_ERROR_FAILURE;
  mFDCurrentPos += aBytesRead;

  return NS_OK;
}

nsresult FileBlockCache::WriteBlockToFile(int32_t aBlockIndex,
                                          const uint8_t* aBlockData)
{
  LOG("WriteBlockToFile(index=%u)", aBlockIndex);

  mFileMutex.AssertCurrentThreadOwns();
  MOZ_ASSERT(mFD);

  nsresult rv = Seek(BlockIndexToOffset(aBlockIndex));
  if (NS_FAILED(rv)) return rv;

  int32_t amount = PR_Write(mFD, aBlockData, BLOCK_SIZE);
  if (amount < BLOCK_SIZE) {
    NS_WARNING("Failed to write media cache block!");
    return NS_ERROR_FAILURE;
  }
  mFDCurrentPos += BLOCK_SIZE;

  return NS_OK;
}

nsresult FileBlockCache::MoveBlockInFile(int32_t aSourceBlockIndex,
                                         int32_t aDestBlockIndex)
{
  LOG("MoveBlockInFile(src=%u, dest=%u)", aSourceBlockIndex, aDestBlockIndex);

  mFileMutex.AssertCurrentThreadOwns();

  uint8_t buf[BLOCK_SIZE];
  int32_t bytesRead = 0;
  if (NS_FAILED(ReadFromFile(BlockIndexToOffset(aSourceBlockIndex),
                             buf,
                             BLOCK_SIZE,
                             bytesRead))) {
    return NS_ERROR_FAILURE;
  }
  return WriteBlockToFile(aDestBlockIndex, buf);
}

void
FileBlockCache::PerformBlockIOs()
{
  NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");
  MutexAutoLock mon(mDataMutex);
  NS_ASSERTION(mIsWriteScheduled, "Should report write running or scheduled.");

  LOG("Run() mFD=%p mThread=%p", mFD, mThread.get());

  while (!mChangeIndexList.empty()) {
    if (!mThread) {
      // We've been closed, abort, discarding unwritten changes.
      mIsWriteScheduled = false;
      return;
    }

    if (mIsReading) {
      // We're trying to read; postpone all writes. (Reader will resume writes.)
      mIsWriteScheduled = false;
      return;
    }

    // Process each pending change. We pop the index out of the change
    // list, but leave the BlockChange in mBlockChanges until the change
    // is written to file. This is so that any read which happens while
    // we drop mDataMutex to write will refer to the data's source in
    // memory, rather than the not-yet up to date data written to file.
    // This also ensures we will insert a new index into mChangeIndexList
    // when this happens.

    // Hold a reference to the change, in case another change
    // overwrites the mBlockChanges entry for this block while we drop
    // mDataMutex to take mFileMutex.
    int32_t blockIndex = mChangeIndexList.front();
    RefPtr<BlockChange> change = mBlockChanges[blockIndex];
    MOZ_ASSERT(change,
               "Change index list should only contain entries for blocks "
               "with changes");
    {
      MutexAutoUnlock unlock(mDataMutex);
      MutexAutoLock lock(mFileMutex);
      if (!mFD) {
        // We may be here if mFD has been reset because we're closing, so we
        // don't care anymore about writes.
        return;
      }
      if (change->IsWrite()) {
        WriteBlockToFile(blockIndex, change->mData.get());
      } else if (change->IsMove()) {
        MoveBlockInFile(change->mSourceBlockIndex, blockIndex);
      }
    }
    mChangeIndexList.pop_front();
    // If a new change has not been made to the block while we dropped
    // mDataMutex, clear reference to the old change. Otherwise, the old
    // reference has been cleared already.
    if (mBlockChanges[blockIndex] == change) {
      mBlockChanges[blockIndex] = nullptr;
    }
  }

  mIsWriteScheduled = false;
}

nsresult FileBlockCache::Read(int64_t aOffset,
                              uint8_t* aData,
                              int32_t aLength,
                              int32_t* aBytes)
{
  MutexAutoLock mon(mDataMutex);

  if (!mThread || (aOffset / BLOCK_SIZE) > INT32_MAX) {
    return NS_ERROR_FAILURE;
  }

  mIsReading = true;
  auto exitRead = MakeScopeExit([&] {
    mIsReading = false;
    if (!mChangeIndexList.empty()) {
      // mReading has stopped or prevented pending writes, resume them.
      EnsureWriteScheduled();
    }
  });

  int32_t bytesToRead = aLength;
  int64_t offset = aOffset;
  uint8_t* dst = aData;
  while (bytesToRead > 0) {
    int32_t blockIndex = static_cast<int32_t>(offset / BLOCK_SIZE);
    int32_t start = offset % BLOCK_SIZE;
    int32_t amount = std::min(BLOCK_SIZE - start, bytesToRead);

    // If the block is not yet written to file, we can just read from
    // the memory buffer, otherwise we need to read from file.
    int32_t bytesRead = 0;
    RefPtr<BlockChange> change = mBlockChanges[blockIndex];
    if (change && change->IsWrite()) {
      // Block isn't yet written to file. Read from memory buffer.
      const uint8_t* blockData = change->mData.get();
      memcpy(dst, blockData + start, amount);
      bytesRead = amount;
    } else {
      if (change && change->IsMove()) {
        // The target block is the destination of a not-yet-completed move
        // action, so read from the move's source block from file. Note we
        // *don't* follow a chain of moves here, as a move's source index
        // is resolved when MoveBlock() is called, and the move's source's
        // block could be have itself been subject to a move (or write)
        // which happened *after* this move was recorded.
        blockIndex = mBlockChanges[blockIndex]->mSourceBlockIndex;
      }
      // Block has been written to file, either as the source block of a move,
      // or as a stable (all changes made) block. Read the data directly
      // from file.
      nsresult res;
      {
        MutexAutoUnlock unlock(mDataMutex);
        MutexAutoLock lock(mFileMutex);
        if (!mFD) {
          // Not initialized yet, or closed.
          return NS_ERROR_FAILURE;
        }
        res = ReadFromFile(BlockIndexToOffset(blockIndex) + start,
                           dst,
                           amount,
                           bytesRead);
      }
      NS_ENSURE_SUCCESS(res,res);
    }
    dst += bytesRead;
    offset += bytesRead;
    bytesToRead -= bytesRead;
  }
  *aBytes = aLength - bytesToRead;
  return NS_OK;
}

nsresult FileBlockCache::MoveBlock(int32_t aSourceBlockIndex, int32_t aDestBlockIndex)
{
  MutexAutoLock mon(mDataMutex);

  if (!mThread) {
    return NS_ERROR_FAILURE;
  }

  mBlockChanges.EnsureLengthAtLeast(std::max(aSourceBlockIndex, aDestBlockIndex) + 1);

  // The source block's contents may be the destination of another pending
  // move, which in turn can be the destination of another pending move,
  // etc. Resolve the final source block, so that if one of the blocks in
  // the chain of moves is overwritten, we don't lose the reference to the
  // contents of the destination block.
  int32_t sourceIndex = aSourceBlockIndex;
  BlockChange* sourceBlock = nullptr;
  while ((sourceBlock = mBlockChanges[sourceIndex]) &&
          sourceBlock->IsMove()) {
    sourceIndex = sourceBlock->mSourceBlockIndex;
  }

  if (mBlockChanges[aDestBlockIndex] == nullptr ||
      !ContainerContains(mChangeIndexList, aDestBlockIndex)) {
    // Only add another entry to the change index list if we don't already
    // have one for this block. We won't have an entry when either there's
    // no pending change for this block, or if there is a pending change for
    // this block and we're in the process of writing it (we've popped the
    // block's index out of mChangeIndexList in Run() but not finished writing
    // the block to file yet.
    mChangeIndexList.push_back(aDestBlockIndex);
  }

  // If the source block hasn't yet been written to file then the dest block
  // simply contains that same write. Resolve this as a write instead.
  if (sourceBlock && sourceBlock->IsWrite()) {
    mBlockChanges[aDestBlockIndex] = new BlockChange(sourceBlock->mData.get());
  } else {
    mBlockChanges[aDestBlockIndex] = new BlockChange(sourceIndex);
  }

  EnsureWriteScheduled();

  NS_ASSERTION(ContainerContains(mChangeIndexList, aDestBlockIndex),
    "Should have scheduled block for change");

  return NS_OK;
}

} // End namespace mozilla.

// avoid redefined macro in unified build
#undef LOG
