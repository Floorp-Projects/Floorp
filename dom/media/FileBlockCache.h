/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FILE_BLOCK_CACHE_H_
#define FILE_BLOCK_CACHE_H_

#include "mozilla/Attributes.h"
#include "mozilla/Monitor.h"
#include "mozilla/MozPromise.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/AbstractThread.h"
#include "nsTArray.h"
#include "MediaCache.h"
#include "nsDeque.h"
#include "nsThreadUtils.h"
#include <deque>

struct PRFileDesc;

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
class FileBlockCache : public Runnable {
public:
  enum {
    BLOCK_SIZE = MediaCacheStream::BLOCK_SIZE
  };

  FileBlockCache();

protected:
  ~FileBlockCache();

public:
  nsresult Init();

  // Closes writer, shuts down thread.
  void Close();

  // Can be called on any thread. This defers to a non-main thread.
  nsresult WriteBlock(uint32_t aBlockIndex, const uint8_t* aData);

  // Performs block writes and block moves on its own thread.
  NS_IMETHOD Run() override;

  // Synchronously reads data from file. May read from file or memory
  // depending on whether written blocks have been flushed to file yet.
  // Not recommended to be called from the main thread, as can cause jank.
  nsresult Read(int64_t aOffset,
                uint8_t* aData,
                int32_t aLength,
                int32_t* aBytes);

  // Moves a block asynchronously. Can be called on any thread.
  // This defers file I/O to a non-main thread.
  nsresult MoveBlock(int32_t aSourceBlockIndex, int32_t aDestBlockIndex);

  // Represents a change yet to be made to a block in the file. The change
  // is either a write (and the data to be written is stored in this struct)
  // or a move (and the index of the source block is stored instead).
  struct BlockChange final {

    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BlockChange)

    // This block is waiting in memory to be written.
    // Stores a copy of the block, so we can write it asynchronously.
    explicit BlockChange(const uint8_t* aData)
      : mSourceBlockIndex(-1)
    {
      mData = MakeUnique<uint8_t[]>(BLOCK_SIZE);
      memcpy(mData.get(), aData, BLOCK_SIZE);
    }

    // This block's contents are located in another file
    // block, i.e. this block has been moved.
    explicit BlockChange(int32_t aSourceBlockIndex)
      : mSourceBlockIndex(aSourceBlockIndex) {}

    UniquePtr<uint8_t[]> mData;
    const int32_t mSourceBlockIndex;

    bool IsMove() const {
      return mSourceBlockIndex != -1;
    }
    bool IsWrite() const {
      return mSourceBlockIndex == -1 &&
             mData.get() != nullptr;
    }

  private:
    // Private destructor, to discourage deletion outside of Release():
    ~BlockChange()
    {
    }
  };

private:
  int64_t BlockIndexToOffset(int32_t aBlockIndex) {
    return static_cast<int64_t>(aBlockIndex) * BLOCK_SIZE;
  }

  void SetCacheFile(PRFileDesc* aFD);

  // Monitor which controls access to mFD and mFDCurrentPos. Don't hold
  // mDataMonitor while holding mFileMonitor! mFileMonitor must be owned
  // while accessing any of the following data fields or methods.
  Monitor mFileMonitor;
  // Moves a block already committed to file.
  nsresult MoveBlockInFile(int32_t aSourceBlockIndex,
                           int32_t aDestBlockIndex);
  // Seeks file pointer.
  nsresult Seek(int64_t aOffset);
  // Reads data from file offset.
  nsresult ReadFromFile(int64_t aOffset,
                        uint8_t* aDest,
                        int32_t aBytesToRead,
                        int32_t& aBytesRead);
  nsresult WriteBlockToFile(int32_t aBlockIndex, const uint8_t* aBlockData);
  // File descriptor we're writing to. This is created externally, but
  // shutdown by us.
  PRFileDesc* mFD;
  // The current file offset in the file.
  int64_t mFDCurrentPos;

  // Monitor which controls access to all data in this class, except mFD
  // and mFDCurrentPos. Don't hold mDataMonitor while holding mFileMonitor!
  // mDataMonitor must be owned while accessing any of the following data
  // fields or methods.
  Monitor mDataMonitor;
  // Ensures we either are running the event to preform IO, or an event
  // has been dispatched to preform the IO.
  // mDataMonitor must be owned while calling this.
  void EnsureWriteScheduled();
  // Promise that tracks the request for an anonymous temporary file for the
  // cache to store data into. The file descriptor must be requested from the
  // parent process when the cache is initialized. While this promise is
  // outstanding, the FileBlockCache buffers blocks in memory, and reads
  // against the cache are serviced from the in-memory buffers.
  RefPtr<GenericPromise::Private> mInitPromise;

  // Array of block changes to made. If mBlockChanges[offset/BLOCK_SIZE] == nullptr,
  // then the block has no pending changes to be written, but if
  // mBlockChanges[offset/BLOCK_SIZE] != nullptr, then either there's a block
  // cached in memory waiting to be written, or this block is the target of a
  // block move.
  nsTArray< RefPtr<BlockChange> > mBlockChanges;
  // Thread upon which block writes and block moves are performed. This is
  // created upon open, and shutdown (asynchronously) upon close (on the
  // main thread).
  nsCOMPtr<nsIThread> mThread;
  // Wrapper for mThread.
  RefPtr<AbstractThread> mAbstractThread;
  // Queue of pending block indexes that need to be written or moved.
  std::deque<int32_t> mChangeIndexList;
  // True if we've dispatched an event to commit all pending block changes
  // to file on mThread.
  bool mIsWriteScheduled;
  // True if the writer is ready to write data to file.
  bool mIsOpen;
};

} // End namespace mozilla.

#endif /* FILE_BLOCK_CACHE_H_ */
