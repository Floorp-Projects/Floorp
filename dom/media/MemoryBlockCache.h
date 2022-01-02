/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEMORY_BLOCK_CACHE_H_
#define MEMORY_BLOCK_CACHE_H_

#include "MediaBlockCacheBase.h"
#include "mozilla/Mutex.h"

namespace mozilla {

// Manages block management for the media cache. Data comes in over the network
// via callbacks on the main thread, however we don't want to write the
// incoming data to the media cache on the main thread, as this could block
// causing UI jank.
//
// So MediaBlockCacheBase provides an abstraction for a temporary memory buffer
// as an array of blocks, which supports a block move operation, and
// allows synchronous reading and writing from any thread.
//
// Writes and cache block moves (which require reading) may be deferred to
// their own non-main thread. This object also ensures that data which has
// been scheduled to be written, but hasn't actually *been* written, is read
// as if it had, i.e. pending writes are cached in readable memory until
// they're flushed to file.
//
// To improve efficiency, writes can only be done at block granularity,
// whereas reads can be done with byte granularity.
class MemoryBlockCache : public MediaBlockCacheBase {
 public:
  explicit MemoryBlockCache(int64_t aContentLength);

 protected:
  virtual ~MemoryBlockCache();

 public:
  // Allocate initial buffer.
  // If re-initializing, clear buffer.
  virtual nsresult Init() override;

  void Flush() override;

  // Maximum number of blocks allowed in this block cache.
  // Based on initial content length, and minimum usable block cache.
  size_t GetMaxBlocks(size_t) const override { return mMaxBlocks; }

  // Can be called on any thread.
  virtual nsresult WriteBlock(uint32_t aBlockIndex, Span<const uint8_t> aData1,
                              Span<const uint8_t> aData2) override;

  // Synchronously reads data from buffer.
  virtual nsresult Read(int64_t aOffset, uint8_t* aData, int32_t aLength,
                        int32_t* aBytes) override;

  // Moves a block. Can be called on any thread.
  virtual nsresult MoveBlock(int32_t aSourceBlockIndex,
                             int32_t aDestBlockIndex) override;

 private:
  static size_t BlockIndexToOffset(uint32_t aBlockIndex) {
    return static_cast<size_t>(aBlockIndex) * BLOCK_SIZE;
  }

  // Ensure the buffer has at least a multiple of BLOCK_SIZE that can contain
  // aContentLength bytes. Buffer size can only grow.
  // Returns false if allocation failed.
  bool EnsureBufferCanContain(size_t aContentLength);

  // Initial content length.
  const size_t mInitialContentLength;

  // Maximum number of blocks that this MemoryBlockCache expects.
  const size_t mMaxBlocks;

  // Mutex which controls access to all members below.
  Mutex mMutex;

  nsTArray<uint8_t> mBuffer;
  bool mHasGrown = false;
};

}  // End namespace mozilla.

#endif /* MEMORY_BLOCK_CACHE_H_ */
