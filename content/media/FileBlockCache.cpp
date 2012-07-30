/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/XPCOM.h"
#include "FileBlockCache.h"
#include "VideoUtils.h"

namespace mozilla {

nsresult FileBlockCache::Open(PRFileDesc* aFD)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  NS_ENSURE_TRUE(aFD != nullptr, NS_ERROR_FAILURE);
  {
    MonitorAutoLock mon(mFileMonitor);
    mFD = aFD;
  }
  {
    MonitorAutoLock mon(mDataMonitor);
    nsresult res = NS_NewThread(getter_AddRefs(mThread),
                                nullptr,
                                MEDIA_THREAD_STACK_SIZE);
    mIsOpen = NS_SUCCEEDED(res);
    return res;
  }
}

FileBlockCache::FileBlockCache()
  : mFileMonitor("nsMediaCache.Writer.IO.Monitor"),
    mFD(nullptr),
    mFDCurrentPos(0),
    mDataMonitor("nsMediaCache.Writer.Data.Monitor"),
    mIsWriteScheduled(false),
    mIsOpen(false)
{
  MOZ_COUNT_CTOR(FileBlockCache);
}

FileBlockCache::~FileBlockCache()
{
  NS_ASSERTION(!mIsOpen, "Should Close() FileBlockCache before destroying");
  {
    // Note, mThread will be shutdown by the time this runs, so we won't
    // block while taking mFileMonitor.
    MonitorAutoLock mon(mFileMonitor);
    if (mFD) {
      PR_Close(mFD);
      mFD = nullptr;
    }
  }
  MOZ_COUNT_DTOR(FileBlockCache);
}


void FileBlockCache::Close()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  MonitorAutoLock mon(mDataMonitor);

  mIsOpen = false;

  if (mThread) {
    // We must shut down the thread in another runnable. This is called
    // while we're shutting down the media cache, and nsIThread::Shutdown()
    // can cause events to run before it completes, which could end up
    // opening more streams, while the media cache is shutting down and
    // releasing memory etc! Also note we close mFD in the destructor so
    // as to not disturb any IO that's currently running.
    nsCOMPtr<nsIRunnable> event = new ShutdownThreadEvent(mThread);
    mThread = nullptr;
    NS_DispatchToMainThread(event);
  }
}

nsresult FileBlockCache::WriteBlock(PRUint32 aBlockIndex, const PRUint8* aData)
{
  MonitorAutoLock mon(mDataMonitor);

  if (!mIsOpen)
    return NS_ERROR_FAILURE;

  // Check if we've already got a pending write scheduled for this block.
  mBlockChanges.EnsureLengthAtLeast(aBlockIndex + 1);
  bool blockAlreadyHadPendingChange = mBlockChanges[aBlockIndex] != nullptr;
  mBlockChanges[aBlockIndex] = new BlockChange(aData);

  if (!blockAlreadyHadPendingChange || !mChangeIndexList.Contains(aBlockIndex)) {
    // We either didn't already have a pending change for this block, or we
    // did but we didn't have an entry for it in mChangeIndexList (we're in the process
    // of writing it and have removed the block's index out of mChangeIndexList
    // in Run() but not finished writing the block to file yet). Add the blocks
    // index to the end of mChangeIndexList to ensure the block is written as
    // as soon as possible.
    mChangeIndexList.PushBack(aBlockIndex);
  }
  NS_ASSERTION(mChangeIndexList.Contains(aBlockIndex), "Must have entry for new block");

  EnsureWriteScheduled();

  return NS_OK;
}

void FileBlockCache::EnsureWriteScheduled()
{
  mDataMonitor.AssertCurrentThreadOwns();

  if (!mIsWriteScheduled) {
    mThread->Dispatch(this, NS_DISPATCH_NORMAL);
    mIsWriteScheduled = true;
  }
}

nsresult FileBlockCache::Seek(PRInt64 aOffset)
{
  mFileMonitor.AssertCurrentThreadOwns();

  if (mFDCurrentPos != aOffset) {
    PROffset64 result = PR_Seek64(mFD, aOffset, PR_SEEK_SET);
    if (result != aOffset) {
      NS_WARNING("Failed to seek media cache file");
      return NS_ERROR_FAILURE;
    }
    mFDCurrentPos = result;
  }
  return NS_OK;
}

nsresult FileBlockCache::ReadFromFile(PRInt32 aOffset,
                                      PRUint8* aDest,
                                      PRInt32 aBytesToRead,
                                      PRInt32& aBytesRead)
{
  mFileMonitor.AssertCurrentThreadOwns();

  nsresult res = Seek(aOffset);
  if (NS_FAILED(res)) return res;
  
  aBytesRead = PR_Read(mFD, aDest, aBytesToRead);
  if (aBytesRead <= 0)
    return NS_ERROR_FAILURE;
  mFDCurrentPos += aBytesRead;
  
  return NS_OK;
}

nsresult FileBlockCache::WriteBlockToFile(PRInt32 aBlockIndex,
                                          const PRUint8* aBlockData)
{
  mFileMonitor.AssertCurrentThreadOwns();

  nsresult rv = Seek(aBlockIndex * BLOCK_SIZE);
  if (NS_FAILED(rv)) return rv;

  PRInt32 amount = PR_Write(mFD, aBlockData, BLOCK_SIZE);
  if (amount < BLOCK_SIZE) {
    NS_WARNING("Failed to write media cache block!");
    return NS_ERROR_FAILURE;
  }
  mFDCurrentPos += BLOCK_SIZE;

  return NS_OK;
}

nsresult FileBlockCache::MoveBlockInFile(PRInt32 aSourceBlockIndex,
                                         PRInt32 aDestBlockIndex)
{
  mFileMonitor.AssertCurrentThreadOwns();

  PRUint8 buf[BLOCK_SIZE];
  PRInt32 bytesRead = 0;
  if (NS_FAILED(ReadFromFile(aSourceBlockIndex * BLOCK_SIZE,
                             buf,
                             BLOCK_SIZE,
                             bytesRead))) {
    return NS_ERROR_FAILURE;
  }
  return WriteBlockToFile(aDestBlockIndex, buf);
}

nsresult FileBlockCache::Run()
{
  MonitorAutoLock mon(mDataMonitor);
  NS_ASSERTION(!NS_IsMainThread(), "Don't call on main thread");
  NS_ASSERTION(!mChangeIndexList.IsEmpty(), "Only dispatch when there's work to do");
  NS_ASSERTION(mIsWriteScheduled, "Should report write running or scheduled.");

  while (!mChangeIndexList.IsEmpty()) {
    if (!mIsOpen) {
      // We've been closed, abort, discarding unwritten changes.
      mIsWriteScheduled = false;
      return NS_ERROR_FAILURE;
    }

    // Process each pending change. We pop the index out of the change
    // list, but leave the BlockChange in mBlockChanges until the change
    // is written to file. This is so that any read which happens while
    // we drop mDataMonitor to write will refer to the data's source in
    // memory, rather than the not-yet up to date data written to file.
    // This also ensures we will insert a new index into mChangeIndexList
    // when this happens.

    // Hold a reference to the change, in case another change
    // overwrites the mBlockChanges entry for this block while we drop
    // mDataMonitor to take mFileMonitor.
    PRInt32 blockIndex = mChangeIndexList.PopFront();
    nsRefPtr<BlockChange> change = mBlockChanges[blockIndex];
    NS_ABORT_IF_FALSE(change,
      "Change index list should only contain entries for blocks with changes");
    {
      MonitorAutoUnlock unlock(mDataMonitor);
      MonitorAutoLock lock(mFileMonitor);
      if (change->IsWrite()) {
        WriteBlockToFile(blockIndex, change->mData.get());
      } else if (change->IsMove()) {
        MoveBlockInFile(change->mSourceBlockIndex, blockIndex);
      }
    }
    // If a new change has not been made to the block while we dropped
    // mDataMonitor, clear reference to the old change. Otherwise, the old
    // reference has been cleared already.
    if (mBlockChanges[blockIndex] == change) {
      mBlockChanges[blockIndex] = nullptr;
    }
  }

  mIsWriteScheduled = false;

  return NS_OK;
}

nsresult FileBlockCache::Read(PRInt64 aOffset,
                              PRUint8* aData,
                              PRInt32 aLength,
                              PRInt32* aBytes)
{
  MonitorAutoLock mon(mDataMonitor);

  if (!mFD || (aOffset / BLOCK_SIZE) > PR_INT32_MAX)
    return NS_ERROR_FAILURE;

  PRInt32 bytesToRead = aLength;
  PRInt64 offset = aOffset;
  PRUint8* dst = aData;
  while (bytesToRead > 0) {
    PRInt32 blockIndex = static_cast<PRInt32>(offset / BLOCK_SIZE);
    PRInt32 start = offset % BLOCK_SIZE;
    PRInt32 amount = NS_MIN(BLOCK_SIZE - start, bytesToRead);

    // If the block is not yet written to file, we can just read from
    // the memory buffer, otherwise we need to read from file.
    PRInt32 bytesRead = 0;
    nsRefPtr<BlockChange> change = mBlockChanges[blockIndex];
    if (change && change->IsWrite()) {
      // Block isn't yet written to file. Read from memory buffer.
      const PRUint8* blockData = change->mData.get();
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
        MonitorAutoUnlock unlock(mDataMonitor);
        MonitorAutoLock lock(mFileMonitor);
        res = ReadFromFile(blockIndex * BLOCK_SIZE + start,
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

nsresult FileBlockCache::MoveBlock(PRInt32 aSourceBlockIndex, PRInt32 aDestBlockIndex)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  MonitorAutoLock mon(mDataMonitor);

  if (!mIsOpen)
    return NS_ERROR_FAILURE;

  mBlockChanges.EnsureLengthAtLeast(NS_MAX(aSourceBlockIndex, aDestBlockIndex) + 1);

  // The source block's contents may be the destination of another pending
  // move, which in turn can be the destination of another pending move,
  // etc. Resolve the final source block, so that if one of the blocks in
  // the chain of moves is overwritten, we don't lose the reference to the
  // contents of the destination block.
  PRInt32 sourceIndex = aSourceBlockIndex;
  BlockChange* sourceBlock = nullptr;
  while ((sourceBlock = mBlockChanges[sourceIndex]) &&
          sourceBlock->IsMove()) {
    sourceIndex = sourceBlock->mSourceBlockIndex;
  }

  if (mBlockChanges[aDestBlockIndex] == nullptr ||
      !mChangeIndexList.Contains(aDestBlockIndex)) {
    // Only add another entry to the change index list if we don't already
    // have one for this block. We won't have an entry when either there's
    // no pending change for this block, or if there is a pending change for
    // this block and we're in the process of writing it (we've popped the
    // block's index out of mChangeIndexList in Run() but not finished writing
    // the block to file yet.
    mChangeIndexList.PushBack(aDestBlockIndex);
  }
  
  // If the source block hasn't yet been written to file then the dest block
  // simply contains that same write. Resolve this as a write instead.
  if (sourceBlock && sourceBlock->IsWrite()) {
    mBlockChanges[aDestBlockIndex] = new BlockChange(sourceBlock->mData.get());
  } else {
    mBlockChanges[aDestBlockIndex] = new BlockChange(sourceIndex);
  }

  EnsureWriteScheduled();

  NS_ASSERTION(mChangeIndexList.Contains(aDestBlockIndex),
    "Should have scheduled block for change");

  return NS_OK;
}

} // End namespace mozilla.
