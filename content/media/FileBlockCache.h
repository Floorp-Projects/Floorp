/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FILE_BLOCK_CACHE_H_
#define FILE_BLOCK_CACHE_H_

#include "mozilla/Monitor.h"
#include "prio.h"
#include "nsTArray.h"
#include "nsMediaCache.h"
#include "nsDeque.h"

namespace mozilla {

// Manages file I/O for the media cache. Data comes in over the network
// via callbacks on the main thread, however we don't want to write the
// incoming data to the media cache on the main thread, as this could block
// causing UI jank. 
//
// So FileBlockCache provides an abstraction for a temporary file accessible
// as an array of blocks, which supports a block move operation, and
// allows synchronous reading and writing from any thread, with writes being
// buffered so as not to block.
//
// Writes and cache block moves (which require reading) are deferred to
// their own non-main thread. This object also ensures that data which has
// been scheduled to be written, but hasn't actually *been* written, is read
// as if it had, i.e. pending writes are cached in readable memory until
// they're flushed to file.
//
// To improve efficiency, writes can only be done at block granularity,
// whereas reads can be done with byte granularity.
//
// Note it's also recommended not to read from the media cache from the main
// thread to prevent jank.
//
// When WriteBlock() or MoveBlock() are called, data about how to complete
// the block change is added to mBlockChanges, indexed by block index, and
// the block index is appended to the mChangeIndexList. This enables
// us to quickly tell if a block has been changed, and ensures we can perform
// the changes in the correct order. An event is dispatched to perform the
// changes listed in mBlockChanges to file. Read() checks mBlockChanges and
// determines the current data to return, reading from file or from
// mBlockChanges as necessary.
class FileBlockCache : public nsRunnable {
public:
  enum {
    BLOCK_SIZE = nsMediaCacheStream::BLOCK_SIZE
  };

  FileBlockCache();

  ~FileBlockCache();

  // Assumes ownership of aFD.
  nsresult Open(PRFileDesc* aFD);

  // Closes writer, shuts down thread.
  void Close();

  // Can be called on any thread. This defers to a non-main thread.
  nsresult WriteBlock(PRUint32 aBlockIndex, const PRUint8* aData);

  // Performs block writes and block moves on its own thread.
  NS_IMETHOD Run();

  // Synchronously reads data from file. May read from file or memory
  // depending on whether written blocks have been flushed to file yet.
  // Not recommended to be called from the main thread, as can cause jank.
  nsresult Read(PRInt64 aOffset,
                PRUint8* aData,
                PRInt32 aLength,
                PRInt32* aBytes);

  // Moves a block asynchronously. Can be called on any thread.
  // This defers file I/O to a non-main thread.
  nsresult MoveBlock(PRInt32 aSourceBlockIndex, PRInt32 aDestBlockIndex);

  // Represents a change yet to be made to a block in the file. The change
  // is either a write (and the data to be written is stored in this struct)
  // or a move (and the index of the source block is stored instead).
  struct BlockChange {

    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BlockChange)

    // This block is waiting in memory to be written.
    // Stores a copy of the block, so we can write it asynchronously.
    BlockChange(const PRUint8* aData)
      : mSourceBlockIndex(-1)
    {
      mData = new PRUint8[BLOCK_SIZE];
      memcpy(mData.get(), aData, BLOCK_SIZE);
    }

    // This block's contents are located in another file
    // block, i.e. this block has been moved.
    BlockChange(PRInt32 aSourceBlockIndex)
      : mSourceBlockIndex(aSourceBlockIndex) {}

    nsAutoArrayPtr<PRUint8> mData;
    const PRInt32 mSourceBlockIndex;

    bool IsMove() const {
      return mSourceBlockIndex != -1;
    }
    bool IsWrite() const {
      return mSourceBlockIndex == -1 &&
             mData.get() != nsnull;
    }
  };

  class Int32Queue : private nsDeque {
  public:
    PRInt32 PopFront() {
      PRInt32 front = ObjectAt(0);
      nsDeque::PopFront();
      return front;
    }

    void PushBack(PRInt32 aValue) {
      nsDeque::Push(reinterpret_cast<void*>(aValue));
    }

    bool Contains(PRInt32 aValue) {
      for (PRInt32 i = 0; i < GetSize(); ++i) {
        if (ObjectAt(i) == aValue) {
          return true;
        }
      }
      return false;
    }

    bool IsEmpty() {
      return nsDeque::GetSize() == 0;
    }

  private:
    PRInt32 ObjectAt(PRInt32 aIndex) {
      void* v = nsDeque::ObjectAt(aIndex);
      // Ugly hack to work around "casting 64bit void* to 32bit int loses precision"
      // error on 64bit Linux.
      return *(reinterpret_cast<PRInt32*>(&v));
    }
  };

private:
  // Monitor which controls access to mFD and mFDCurrentPos. Don't hold
  // mDataMonitor while holding mFileMonitor! mFileMonitor must be owned
  // while accessing any of the following data fields or methods.
  mozilla::Monitor mFileMonitor;
  // Moves a block already committed to file.
  nsresult MoveBlockInFile(PRInt32 aSourceBlockIndex,
                           PRInt32 aDestBlockIndex);
  // Seeks file pointer.
  nsresult Seek(PRInt64 aOffset);
  // Reads data from file offset.
  nsresult ReadFromFile(PRInt32 aOffset,
                        PRUint8* aDest,
                        PRInt32 aBytesToRead,
                        PRInt32& aBytesRead);
  nsresult WriteBlockToFile(PRInt32 aBlockIndex, const PRUint8* aBlockData);
  // File descriptor we're writing to. This is created externally, but
  // shutdown by us.
  PRFileDesc* mFD;
  // The current file offset in the file.
  PRInt64 mFDCurrentPos;

  // Monitor which controls access to all data in this class, except mFD
  // and mFDCurrentPos. Don't hold mDataMonitor while holding mFileMonitor!
  // mDataMonitor must be owned while accessing any of the following data
  // fields or methods.
  mozilla::Monitor mDataMonitor;
  // Ensures we either are running the event to preform IO, or an event
  // has been dispatched to preform the IO.
  // mDataMonitor must be owned while calling this.
  void EnsureWriteScheduled();
  // Array of block changes to made. If mBlockChanges[offset/BLOCK_SIZE] == nsnull,
  // then the block has no pending changes to be written, but if
  // mBlockChanges[offset/BLOCK_SIZE] != nsnull, then either there's a block
  // cached in memory waiting to be written, or this block is the target of a
  // block move.
  nsTArray< nsRefPtr<BlockChange> > mBlockChanges;
  // Thread upon which block writes and block moves are performed. This is
  // created upon open, and shutdown (asynchronously) upon close (on the
  // main thread).
  nsCOMPtr<nsIThread> mThread;
  // Queue of pending block indexes that need to be written or moved.
  //nsAutoTArray<PRInt32, 8> mChangeIndexList;
  Int32Queue mChangeIndexList;
  // True if we've dispatched an event to commit all pending block changes
  // to file on mThread.
  bool mIsWriteScheduled;
  // True if the writer is ready to write data to file.
  bool mIsOpen;
};

} // End namespace mozilla.

#endif /* FILE_BLOCK_CACHE_H_ */
