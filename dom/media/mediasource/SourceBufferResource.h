/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SOURCEBUFFERRESOURCE_H_
#define MOZILLA_SOURCEBUFFERRESOURCE_H_

#include "mozilla/AbstractThread.h"
#include "mozilla/Logging.h"
#include "MediaResource.h"
#include "ResourceQueue.h"

#define UNIMPLEMENTED()                               \
  { /* Logging this is too spammy to do by default */ \
  }

namespace mozilla {

class MediaByteBuffer;
class AbstractThread;

namespace dom {

class SourceBuffer;

}  // namespace dom

DDLoggedTypeDeclNameAndBase(SourceBufferResource, MediaResource);

// SourceBufferResource is not thread safe.
class SourceBufferResource final
    : public MediaResource,
      public DecoderDoctorLifeLogger<SourceBufferResource> {
 public:
  SourceBufferResource();
  RefPtr<GenericPromise> Close() override;
  nsresult ReadAt(int64_t aOffset, char* aBuffer, uint32_t aCount,
                  uint32_t* aBytes) override;
  // Memory-based and no locks, caching discouraged.
  bool ShouldCacheReads() override { return false; }
  void Pin() override { UNIMPLEMENTED(); }
  void Unpin() override { UNIMPLEMENTED(); }
  int64_t GetLength() override { return mInputBuffer.GetLength(); }
  int64_t GetNextCachedData(int64_t aOffset) override {
    MOZ_ASSERT(OnThread());
    MOZ_ASSERT(aOffset >= 0);
    if (uint64_t(aOffset) < mInputBuffer.GetOffset()) {
      return mInputBuffer.GetOffset();
    } else if (aOffset == GetLength()) {
      return -1;
    }
    return aOffset;
  }
  int64_t GetCachedDataEnd(int64_t aOffset) override {
    MOZ_ASSERT(OnThread());
    MOZ_ASSERT(aOffset >= 0);
    if (uint64_t(aOffset) < mInputBuffer.GetOffset() ||
        aOffset >= GetLength()) {
      // aOffset is outside of the buffered range.
      return aOffset;
    }
    return GetLength();
  }
  bool IsDataCachedToEndOfResource(int64_t aOffset) override { return false; }
  nsresult ReadFromCache(char* aBuffer, int64_t aOffset,
                         uint32_t aCount) override;

  nsresult GetCachedRanges(MediaByteRangeSet& aRanges) override {
    MOZ_ASSERT(OnThread());
    if (mInputBuffer.GetLength()) {
      aRanges +=
          MediaByteRange(mInputBuffer.GetOffset(), mInputBuffer.GetLength());
    }
    return NS_OK;
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
    MOZ_ASSERT(OnThread());
    return mInputBuffer.SizeOfExcludingThis(aMallocSizeOf);
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  // Used by SourceBuffer.
  void AppendData(MediaByteBuffer* aData);
  void AppendData(const MediaSpan& aData);
  void Ended();
  bool IsEnded() {
    MOZ_ASSERT(OnThread());
    return mEnded;
  }
  // Remove data from resource if it holds more than the threshold reduced by
  // the given number of bytes. Returns amount evicted.
  uint32_t EvictData(uint64_t aPlaybackOffset, int64_t aThresholdReduct);

  // Remove data from resource before the given offset.
  void EvictBefore(uint64_t aOffset);

  // Remove all data from the resource
  uint32_t EvictAll();

  // Returns the amount of data currently retained by this resource.
  int64_t GetSize() {
    MOZ_ASSERT(OnThread());
    return mInputBuffer.GetLength() - mInputBuffer.GetOffset();
  }

  const uint8_t* GetContiguousAccess(int64_t aOffset, size_t aSize) {
    return mInputBuffer.GetContiguousAccess(aOffset, aSize);
  }

#if defined(DEBUG)
  void Dump(const char* aPath) { mInputBuffer.Dump(aPath); }
#endif

 private:
  virtual ~SourceBufferResource();
  nsresult ReadAtInternal(int64_t aOffset, char* aBuffer, uint32_t aCount,
                          uint32_t* aBytes);

#if defined(DEBUG)
  const RefPtr<AbstractThread> mThread;
  // TaskQueue methods and objects.
  const AbstractThread* GetThread() const;
  bool OnThread() const;
#endif

  // The buffer holding resource data.
  ResourceQueue mInputBuffer;

  bool mClosed = false;
  bool mEnded = false;
};

}  // namespace mozilla

#undef UNIMPLEMENTED

#endif /* MOZILLA_SOURCEBUFFERRESOURCE_H_ */
