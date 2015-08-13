/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MP4_STREAM_H_
#define MP4_STREAM_H_

#include "mp4_demuxer/mp4_demuxer.h"

#include "MediaResource.h"

#include "mozilla/Maybe.h"
#include "mozilla/Monitor.h"

namespace mozilla {

class Monitor;

class MP4Stream : public mp4_demuxer::Stream {
public:
  explicit MP4Stream(MediaResource* aResource);
  virtual ~MP4Stream();
  bool BlockingReadIntoCache(int64_t aOffset, size_t aCount, Monitor* aToUnlock);
  virtual bool ReadAt(int64_t aOffset, void* aBuffer, size_t aCount,
                      size_t* aBytesRead) override;
  virtual bool CachedReadAt(int64_t aOffset, void* aBuffer, size_t aCount,
                            size_t* aBytesRead) override;
  virtual bool Length(int64_t* aSize) override;

  struct ReadRecord {
    ReadRecord(int64_t aOffset, size_t aCount) : mOffset(aOffset), mCount(aCount) {}
    bool operator==(const ReadRecord& aOther) { return mOffset == aOther.mOffset && mCount == aOther.mCount; }
    int64_t mOffset;
    size_t mCount;
  };
  bool LastReadFailed(ReadRecord* aOut)
  {
    if (mFailedRead.isSome()) {
      *aOut = mFailedRead.ref();
      return true;
    }

    return false;
  }

  void ClearFailedRead() { mFailedRead.reset(); }

  void Pin()
  {
    mResource.GetResource()->Pin();
    ++mPinCount;
  }

  void Unpin()
  {
    mResource.GetResource()->Unpin();
    MOZ_ASSERT(mPinCount);
    --mPinCount;
    if (mPinCount == 0) {
      mCache.Clear();
    }
  }

private:
  MediaResourceIndex mResource;
  Maybe<ReadRecord> mFailedRead;
  uint32_t mPinCount;

  struct CacheBlock {
    CacheBlock(int64_t aOffset, size_t aCount)
      : mOffset(aOffset), mCount(aCount), mBuffer(nullptr) {}
    int64_t mOffset;
    size_t mCount;

    bool Init()
    {
      mBuffer = new (fallible) char[mCount];
      return !!mBuffer;
    }

    char* Buffer()
    {
      MOZ_ASSERT(mBuffer.get());
      return mBuffer.get();
    }

  private:
    nsAutoArrayPtr<char> mBuffer;
  };
  nsTArray<CacheBlock> mCache;
};

}

#endif
