/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ResourceQueue.h"
#include "MediaData.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Logging.h"
#include "mozilla/Sprintf.h"
#include "mozilla/Unused.h"

extern mozilla::LogModule* GetSourceBufferResourceLog();

#define SBR_DEBUG(arg, ...)                                       \
  MOZ_LOG(GetSourceBufferResourceLog(), mozilla::LogLevel::Debug, \
          ("ResourceQueue(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
#define SBR_DEBUGV(arg, ...)                                        \
  MOZ_LOG(GetSourceBufferResourceLog(), mozilla::LogLevel::Verbose, \
          ("ResourceQueue(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

namespace mozilla {

ResourceItem::ResourceItem(const MediaSpan& aData, uint64_t aOffset)
    : mData(aData), mOffset(aOffset) {}

size_t ResourceItem::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this);
}

class ResourceQueueDeallocator : public nsDequeFunctor<ResourceItem> {
  void operator()(ResourceItem* aObject) override { delete aObject; }
};

ResourceQueue::ResourceQueue()
    : nsDeque<ResourceItem>(new ResourceQueueDeallocator()),
      mLogicalLength(0),
      mOffset(0) {}

uint64_t ResourceQueue::GetOffset() { return mOffset; }

uint64_t ResourceQueue::GetLength() { return mLogicalLength; }

const uint8_t* ResourceQueue::GetContiguousAccess(int64_t aOffset,
                                                  size_t aSize) {
  uint32_t offset = 0;
  uint32_t start = GetAtOffset(aOffset, &offset);
  if (start >= GetSize()) {
    return nullptr;
  }
  ResourceItem* item = ResourceAt(start);
  if (offset + aSize > item->mData.Length()) {
    return nullptr;
  }
  return item->mData.Elements() + offset;
}

void ResourceQueue::CopyData(uint64_t aOffset, uint32_t aCount, char* aDest) {
  uint32_t offset = 0;
  uint32_t start = GetAtOffset(aOffset, &offset);
  size_t i = start;
  while (i < uint32_t(GetSize()) && aCount > 0) {
    ResourceItem* item = ResourceAt(i++);
    uint32_t bytes = std::min(aCount, uint32_t(item->mData.Length() - offset));
    if (bytes != 0) {
      memcpy(aDest, item->mData.Elements() + offset, bytes);
      offset = 0;
      aCount -= bytes;
      aDest += bytes;
    }
  }
}

void ResourceQueue::AppendItem(const MediaSpan& aData) {
  uint64_t offset = mLogicalLength;
  mLogicalLength += aData.Length();
  Push(new ResourceItem(aData, offset));
}

uint32_t ResourceQueue::Evict(uint64_t aOffset, uint32_t aSizeToEvict) {
  SBR_DEBUG("Evict(aOffset=%" PRIu64 ", aSizeToEvict=%u)", aOffset,
            aSizeToEvict);
  return EvictBefore(std::min(aOffset, mOffset + (uint64_t)aSizeToEvict));
}

uint32_t ResourceQueue::EvictBefore(uint64_t aOffset) {
  SBR_DEBUG("EvictBefore(%" PRIu64 ")", aOffset);
  uint32_t evicted = 0;
  while (GetSize()) {
    ResourceItem* item = ResourceAt(0);
    SBR_DEBUG("item=%p length=%zu offset=%" PRIu64, item, item->mData.Length(),
              mOffset);
    if (item->mData.Length() + mOffset >= aOffset) {
      if (aOffset <= mOffset) {
        break;
      }
      uint32_t offset = aOffset - mOffset;
      mOffset += offset;
      evicted += offset;
      item->mData.RemoveFront(offset);
      item->mOffset += offset;
      break;
    }
    mOffset += item->mData.Length();
    evicted += item->mData.Length();
    delete PopFront();
  }
  return evicted;
}

uint32_t ResourceQueue::EvictAll() {
  SBR_DEBUG("EvictAll()");
  uint32_t evicted = 0;
  while (GetSize()) {
    ResourceItem* item = ResourceAt(0);
    SBR_DEBUG("item=%p length=%zu offset=%" PRIu64, item, item->mData.Length(),
              mOffset);
    mOffset += item->mData.Length();
    evicted += item->mData.Length();
    delete PopFront();
  }
  return evicted;
}

size_t ResourceQueue::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
  // Calculate the size of the internal deque.
  size_t size = nsDeque<ResourceItem>::SizeOfExcludingThis(aMallocSizeOf);

  // Sum the ResourceItems. The ResourceItems's MediaSpans may share the
  // same underlying MediaByteBuffers, so we need to de-dupe the buffers
  // in order to report an accurate size.
  nsTArray<MediaByteBuffer*> buffers;
  for (uint32_t i = 0; i < uint32_t(GetSize()); ++i) {
    const ResourceItem* item = ResourceAt(i);
    size += item->SizeOfIncludingThis(aMallocSizeOf);
    if (!buffers.Contains(item->mData.Buffer())) {
      buffers.AppendElement(item->mData.Buffer());
    }
  }

  for (MediaByteBuffer* buffer : buffers) {
    size += buffer->ShallowSizeOfExcludingThis(aMallocSizeOf);
  }

  return size;
}

#if defined(DEBUG)
void ResourceQueue::Dump(const char* aPath) {
  for (uint32_t i = 0; i < uint32_t(GetSize()); ++i) {
    ResourceItem* item = ResourceAt(i);

    char buf[255];
    SprintfLiteral(buf, "%s/%08u.bin", aPath, i);
    FILE* fp = fopen(buf, "wb");
    if (!fp) {
      return;
    }
    Unused << fwrite(item->mData.Elements(), item->mData.Length(), 1, fp);
    fclose(fp);
  }
}
#endif

ResourceItem* ResourceQueue::ResourceAt(uint32_t aIndex) const {
  return static_cast<ResourceItem*>(ObjectAt(aIndex));
}

uint32_t ResourceQueue::GetAtOffset(uint64_t aOffset,
                                    uint32_t* aResourceOffset) const {
  MOZ_RELEASE_ASSERT(aOffset >= mOffset);

  size_t hi = GetSize();
  size_t lo = 0;
  while (lo < hi) {
    size_t mid = lo + (hi - lo) / 2;
    const ResourceItem* resource = ResourceAt(mid);
    if (resource->mOffset <= aOffset &&
        aOffset < resource->mOffset + resource->mData.Length()) {
      if (aResourceOffset) {
        *aResourceOffset = aOffset - resource->mOffset;
      }
      return uint32_t(mid);
    }
    if (resource->mOffset + resource->mData.Length() <= aOffset) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }

  return uint32_t(GetSize());
}

ResourceItem* ResourceQueue::PopFront() {
  return nsDeque<ResourceItem>::PopFront();
}

#undef SBR_DEBUG
#undef SBR_DEBUGV

}  // namespace mozilla
