/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDeque.h"
#include "MediaData.h"
#include "mozilla/Logging.h"

extern PRLogModuleInfo* GetSourceBufferResourceLog();

/* Polyfill __func__ on MSVC to pass to the log. */
#ifdef _MSC_VER
#define __func__ __FUNCTION__
#endif

#define SBR_DEBUG(arg, ...) MOZ_LOG(GetSourceBufferResourceLog(), mozilla::LogLevel::Debug, ("ResourceQueue(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
#define SBR_DEBUGV(arg, ...) MOZ_LOG(GetSourceBufferResourceLog(), mozilla::LogLevel::Verbose, ("ResourceQueue(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

namespace mozilla {

ResourceItem::ResourceItem(MediaLargeByteBuffer* aData)
  : mData(aData)
{
}

size_t
ResourceItem::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  // size including this
  size_t size = aMallocSizeOf(this);

  // size excluding this
  size += mData->SizeOfExcludingThis(aMallocSizeOf);

  return size;
}

class ResourceQueueDeallocator : public nsDequeFunctor {
  virtual void* operator() (void* aObject) {
    delete static_cast<ResourceItem*>(aObject);
    return nullptr;
  }
};

ResourceQueue::ResourceQueue()
  : nsDeque(new ResourceQueueDeallocator())
  , mLogicalLength(0)
  , mOffset(0)
{
}

uint64_t
ResourceQueue::GetOffset()
{
  return mOffset;
}

uint64_t
ResourceQueue::GetLength()
{
  return mLogicalLength;
}

void
ResourceQueue::CopyData(uint64_t aOffset, uint32_t aCount, char* aDest)
{
  uint32_t offset = 0;
  uint32_t start = GetAtOffset(aOffset, &offset);
  uint32_t end = std::min(GetAtOffset(aOffset + aCount, nullptr) + 1, uint32_t(GetSize()));
  for (uint32_t i = start; i < end; ++i) {
    ResourceItem* item = ResourceAt(i);
    uint32_t bytes = std::min(aCount, uint32_t(item->mData->Length() - offset));
    if (bytes != 0) {
      memcpy(aDest, &(*item->mData)[offset], bytes);
      offset = 0;
      aCount -= bytes;
      aDest += bytes;
    }
  }
}

void
ResourceQueue::AppendItem(MediaLargeByteBuffer* aData)
{
  mLogicalLength += aData->Length();
  Push(new ResourceItem(aData));
}

uint32_t
ResourceQueue::Evict(uint64_t aOffset, uint32_t aSizeToEvict,
                     ErrorResult& aRv)
{
  SBR_DEBUG("Evict(aOffset=%llu, aSizeToEvict=%u)",
            aOffset, aSizeToEvict);
  return EvictBefore(std::min(aOffset, mOffset + (uint64_t)aSizeToEvict), aRv);
}

uint32_t ResourceQueue::EvictBefore(uint64_t aOffset, ErrorResult& aRv)
{
  SBR_DEBUG("EvictBefore(%llu)", aOffset);
  uint32_t evicted = 0;
  while (ResourceItem* item = ResourceAt(0)) {
    SBR_DEBUG("item=%p length=%d offset=%llu",
              item, item->mData->Length(), mOffset);
    if (item->mData->Length() + mOffset >= aOffset) {
      if (aOffset <= mOffset) {
        break;
      }
      uint32_t offset = aOffset - mOffset;
      mOffset += offset;
      evicted += offset;
      nsRefPtr<MediaLargeByteBuffer> data = new MediaLargeByteBuffer;
      if (!data->AppendElements(item->mData->Elements() + offset,
                                item->mData->Length() - offset,
                                fallible)) {
        aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
        return 0;
      }

      item->mData = data;
      break;
    }
    mOffset += item->mData->Length();
    evicted += item->mData->Length();
    delete PopFront();
  }
  return evicted;
}

uint32_t
ResourceQueue::EvictAll()
{
  SBR_DEBUG("EvictAll()");
  uint32_t evicted = 0;
  while (ResourceItem* item = ResourceAt(0)) {
    SBR_DEBUG("item=%p length=%d offset=%llu",
              item, item->mData->Length(), mOffset);
    mOffset += item->mData->Length();
    evicted += item->mData->Length();
    delete PopFront();
  }
  return evicted;
}

size_t
ResourceQueue::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
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
void
ResourceQueue::Dump(const char* aPath)
{
  for (uint32_t i = 0; i < uint32_t(GetSize()); ++i) {
    ResourceItem* item = ResourceAt(i);

    char buf[255];
    PR_snprintf(buf, sizeof(buf), "%s/%08u.bin", aPath, i);
    FILE* fp = fopen(buf, "wb");
    if (!fp) {
      return;
    }
    fwrite(item->mData->Elements(), item->mData->Length(), 1, fp);
    fclose(fp);
  }
}
#endif

ResourceItem*
ResourceQueue::ResourceAt(uint32_t aIndex) const
{
  return static_cast<ResourceItem*>(ObjectAt(aIndex));
}

uint32_t
ResourceQueue::GetAtOffset(uint64_t aOffset, uint32_t *aResourceOffset)
{
  MOZ_RELEASE_ASSERT(aOffset >= mOffset);
  uint64_t offset = mOffset;
  for (uint32_t i = 0; i < uint32_t(GetSize()); ++i) {
    ResourceItem* item = ResourceAt(i);
    // If the item contains the start of the offset we want to
    // break out of the loop.
    if (item->mData->Length() + offset > aOffset) {
      if (aResourceOffset) {
        *aResourceOffset = aOffset - offset;
      }
      return i;
    }
    offset += item->mData->Length();
  }
  return GetSize();
}

ResourceItem*
ResourceQueue::PopFront()
{
  return static_cast<ResourceItem*>(nsDeque::PopFront());
}

#undef SBR_DEBUG
#undef SBR_DEBUGV

} // namespace mozilla
