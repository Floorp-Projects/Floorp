/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_RESOURCEQUEUE_H_
#define MOZILLA_RESOURCEQUEUE_H_

#include <algorithm>
#include "nsDeque.h"
#include "nsTArray.h"
#include "prlog.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* GetSourceBufferResourceLog();

#define SBR_DEBUG(...) PR_LOG(GetSourceBufferResourceLog(), PR_LOG_DEBUG, (__VA_ARGS__))
#define SBR_DEBUGV(...) PR_LOG(GetSourceBufferResourceLog(), PR_LOG_DEBUG+1, (__VA_ARGS__))
#else
#define SBR_DEBUG(...)
#define SBR_DEBUGV(...)
#endif

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
  ResourceItem(const uint8_t* aData, uint32_t aSize) {
    mData.AppendElements(aData, aSize);
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
    // size including this
    size_t size = aMallocSizeOf(this);

    // size excluding this
    size += mData.SizeOfExcludingThis(aMallocSizeOf);

    return size;
  }

  nsTArray<uint8_t> mData;
};

class ResourceQueueDeallocator : public nsDequeFunctor {
  virtual void* operator() (void* aObject) {
    delete static_cast<ResourceItem*>(aObject);
    return nullptr;
  }
};

class ResourceQueue : private nsDeque {
public:
  ResourceQueue()
    : nsDeque(new ResourceQueueDeallocator())
    , mLogicalLength(0)
    , mOffset(0)
  {
  }

  // Returns the logical byte offset of the start of the data.
  uint64_t GetOffset() {
    return mOffset;
  }

  // Returns the length of all items in the queue plus the offset.
  // This is the logical length of the resource.
  uint64_t GetLength() {
    return mLogicalLength;
  }

  // Copies aCount bytes from aOffset in the queue into aDest.
  void CopyData(uint64_t aOffset, uint32_t aCount, char* aDest) {
    uint32_t offset = 0;
    uint32_t start = GetAtOffset(aOffset, &offset);
    uint32_t end = std::min(GetAtOffset(aOffset + aCount, nullptr) + 1, uint32_t(GetSize()));
    for (uint32_t i = start; i < end; ++i) {
      ResourceItem* item = ResourceAt(i);
      uint32_t bytes = std::min(aCount, uint32_t(item->mData.Length() - offset));
      if (bytes != 0) {
        memcpy(aDest, &item->mData[offset], bytes);
        offset = 0;
        aCount -= bytes;
        aDest += bytes;
      }
    }
  }

  void AppendItem(const uint8_t* aData, uint32_t aLength) {
    mLogicalLength += aLength;
    Push(new ResourceItem(aData, aLength));
  }

  // Tries to evict at least aSizeToEvict from the queue up until
  // aOffset. Returns amount evicted.
  uint32_t Evict(uint64_t aOffset, uint32_t aSizeToEvict) {
    SBR_DEBUG("ResourceQueue(%p)::Evict(aOffset=%llu, aSizeToEvict=%u)",
              this, aOffset, aSizeToEvict);
    return EvictBefore(std::min(aOffset, (uint64_t)aSizeToEvict));
  }

  uint32_t EvictBefore(uint64_t aOffset) {
    SBR_DEBUG("ResourceQueue(%p)::EvictBefore(%llu)", this, aOffset);
    uint32_t evicted = 0;
    while (ResourceItem* item = ResourceAt(0)) {
      SBR_DEBUG("ResourceQueue(%p)::EvictBefore item=%p length=%d offset=%llu",
                this, item, item->mData.Length(), mOffset);
      if (item->mData.Length() + mOffset >= aOffset) {
        break;
      }
      mOffset += item->mData.Length();
      evicted += item->mData.Length();
      delete PopFront();
    }
    return evicted;
  }

  uint32_t EvictAll() {
    SBR_DEBUG("ResourceQueue(%p)::EvictAll()", this);
    uint32_t evicted = 0;
    while (ResourceItem* item = ResourceAt(0)) {
      SBR_DEBUG("ResourceQueue(%p)::EvictAll item=%p length=%d offset=%llu",
                this, item, item->mData.Length(), mOffset);
      mOffset += item->mData.Length();
      evicted += item->mData.Length();
      delete PopFront();
    }
    return evicted;
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
    // Calculate the size of the internal deque.
    size_t size = nsDeque::SizeOfExcludingThis(aMallocSizeOf);

    // Sum the ResourceItems.
    for (uint32_t i = 0; i < uint32_t(GetSize()); ++i) {
      const ResourceItem* item = ResourceAt(i);
      size += item->SizeOfIncludingThis(aMallocSizeOf);
    }

    return size;
  }

#if defined(DEBUG)
  void Dump(const char* aPath) {
    for (uint32_t i = 0; i < uint32_t(GetSize()); ++i) {
      ResourceItem* item = ResourceAt(i);

      char buf[255];
      PR_snprintf(buf, sizeof(buf), "%s/%08u.bin", aPath, i);
      FILE* fp = fopen(buf, "wb");
      if (!fp) {
        return;
      }
      fwrite(item->mData.Elements(), item->mData.Length(), 1, fp);
      fclose(fp);
    }
  }
#endif

private:
  ResourceItem* ResourceAt(uint32_t aIndex) const {
    return static_cast<ResourceItem*>(ObjectAt(aIndex));
  }

  // Returns the index of the resource that contains the given
  // logical offset. aResourceOffset will contain the offset into
  // the resource at the given index returned if it is not null.  If
  // no such resource exists, returns GetSize() and aOffset is
  // untouched.
  uint32_t GetAtOffset(uint64_t aOffset, uint32_t *aResourceOffset) {
    MOZ_ASSERT(aOffset >= mOffset);
    uint64_t offset = mOffset;
    for (uint32_t i = 0; i < uint32_t(GetSize()); ++i) {
      ResourceItem* item = ResourceAt(i);
      // If the item contains the start of the offset we want to
      // break out of the loop.
      if (item->mData.Length() + offset > aOffset) {
        if (aResourceOffset) {
          *aResourceOffset = aOffset - offset;
        }
        return i;
      }
      offset += item->mData.Length();
    }
    return GetSize();
  }

  ResourceItem* PopFront() {
    return static_cast<ResourceItem*>(nsDeque::PopFront());
  }

  // Logical length of the resource.
  uint64_t mLogicalLength;

  // Logical offset into the resource of the first element in the queue.
  uint64_t mOffset;
};

} // namespace mozilla
#endif /* MOZILLA_RESOURCEQUEUE_H_ */
