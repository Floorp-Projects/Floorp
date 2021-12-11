/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIA_BLOCK_CACHE_BASE_H_
#define MEDIA_BLOCK_CACHE_BASE_H_

#include "MediaCache.h"
#include "mozilla/Span.h"

namespace mozilla {

// Manages block management for the media cache. Data comes in over the network
// via callbacks on the main thread, however we don't want to write the
// incoming data to the media cache on the main thread, as this could block
// causing UI jank.
//
// So MediaBlockCacheBase provides an abstraction for a temporary memory buffer
// or file accessible as an array of blocks, which supports a block move
// operation, and allows synchronous reading and writing from any thread, with
// writes being buffered as needed so as not to block.
//
// Writes and cache block moves (which require reading) may be deferred to
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
class MediaBlockCacheBase {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaBlockCacheBase)

  static_assert(
      MediaCacheStream::BLOCK_SIZE <
          static_cast<
              std::remove_const<decltype(MediaCacheStream::BLOCK_SIZE)>::type>(
              INT32_MAX),
      "MediaCacheStream::BLOCK_SIZE should fit in 31 bits");
  static const int32_t BLOCK_SIZE = MediaCacheStream::BLOCK_SIZE;

 protected:
  virtual ~MediaBlockCacheBase() = default;

 public:
  // Initialize this cache.
  virtual nsresult Init() = 0;

  // Erase data and discard pending changes to reset the cache to its pristine
  // state as it was after Init().
  virtual void Flush() = 0;

  // Maximum number of blocks expected in this block cache. (But allow overflow
  // to accomodate incoming traffic before MediaCache can handle it.)
  virtual size_t GetMaxBlocks(size_t aCacheSizeInKiB) const = 0;

  // Can be called on any thread. This defers to a non-main thread.
  virtual nsresult WriteBlock(uint32_t aBlockIndex, Span<const uint8_t> aData1,
                              Span<const uint8_t> aData2) = 0;

  // Synchronously reads data from file. May read from file or memory
  // depending on whether written blocks have been flushed to file yet.
  // Not recommended to be called from the main thread, as can cause jank.
  virtual nsresult Read(int64_t aOffset, uint8_t* aData, int32_t aLength,
                        int32_t* aBytes) = 0;

  // Moves a block asynchronously. Can be called on any thread.
  // This defers file I/O to a non-main thread.
  virtual nsresult MoveBlock(int32_t aSourceBlockIndex,
                             int32_t aDestBlockIndex) = 0;
};

}  // End namespace mozilla.

#endif /* MEDIA_BLOCK_CACHE_BASE_H_ */
