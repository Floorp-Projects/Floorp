/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_RESOURCEQUEUE_H_
#define MOZILLA_RESOURCEQUEUE_H_

#include "nsDeque.h"
#include "MediaData.h"

namespace mozilla {

// A SourceBufferResource has a queue containing the data that is appended
// to it. The queue holds instances of ResourceItem which is an array of the
// bytes. Appending data to the SourceBufferResource pushes this onto the
// queue.

// Data is evicted once it reaches a size threshold. This pops the items off
// the front of the queue and deletes it.  If an eviction happens then the
// MediaSource is notified (done in SourceBuffer::AppendData) which then
// requests all SourceBuffers to evict data up to approximately the same
// timepoint.

struct ResourceItem {
  explicit ResourceItem(MediaLargeByteBuffer* aData);
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;
  nsRefPtr<MediaLargeByteBuffer> mData;
};

class ResourceQueue : private nsDeque {
public:
  ResourceQueue();

  // Returns the logical byte offset of the start of the data.
  uint64_t GetOffset();

  // Returns the length of all items in the queue plus the offset.
  // This is the logical length of the resource.
  uint64_t GetLength();

  // Copies aCount bytes from aOffset in the queue into aDest.
  void CopyData(uint64_t aOffset, uint32_t aCount, char* aDest);

  void AppendItem(MediaLargeByteBuffer* aData);

  // Tries to evict at least aSizeToEvict from the queue up until
  // aOffset. Returns amount evicted.
  uint32_t Evict(uint64_t aOffset, uint32_t aSizeToEvict);

  uint32_t EvictBefore(uint64_t aOffset);

  uint32_t EvictAll();

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const;

#if defined(DEBUG)
  void Dump(const char* aPath);
#endif

private:
  ResourceItem* ResourceAt(uint32_t aIndex) const;

  // Returns the index of the resource that contains the given
  // logical offset. aResourceOffset will contain the offset into
  // the resource at the given index returned if it is not null.  If
  // no such resource exists, returns GetSize() and aOffset is
  // untouched.
  uint32_t GetAtOffset(uint64_t aOffset, uint32_t *aResourceOffset);

  ResourceItem* PopFront();

  // Logical length of the resource.
  uint64_t mLogicalLength;

  // Logical offset into the resource of the first element in the queue.
  uint64_t mOffset;
};

} // namespace mozilla
#endif /* MOZILLA_RESOURCEQUEUE_H_ */
