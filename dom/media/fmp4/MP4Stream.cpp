/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MP4Stream.h"
#include "MediaResource.h"

namespace mozilla {

MP4Stream::MP4Stream(MediaResource* aResource)
  : mResource(aResource)
  , mPinCount(0)
{
  MOZ_COUNT_CTOR(MP4Stream);
  MOZ_ASSERT(aResource);
}

MP4Stream::~MP4Stream()
{
  MOZ_COUNT_DTOR(MP4Stream);
  MOZ_ASSERT(mPinCount == 0);
}

bool
MP4Stream::BlockingReadIntoCache(int64_t aOffset, size_t aCount, Monitor* aToUnlock)
{
  MOZ_ASSERT(mPinCount > 0);
  CacheBlock block(aOffset, aCount);
  if (!block.Init()) {
    return false;
  }

  uint32_t sum = 0;
  uint32_t bytesRead = 0;
  do {
    uint64_t offset = aOffset + sum;
    char* buffer = block.Buffer() + sum;
    uint32_t toRead = aCount - sum;
    MonitorAutoUnlock unlock(*aToUnlock);
    nsresult rv = mResource->ReadAt(offset, buffer, toRead, &bytesRead);
    if (NS_FAILED(rv)) {
      return false;
    }
    sum += bytesRead;
  } while (sum < aCount && bytesRead > 0);

  MOZ_ASSERT(block.mCount >= sum);
  block.mCount = sum;

  mCache.AppendElement(block);
  return true;
}

// We surreptitiously reimplement the supposedly-blocking ReadAt as a non-
// blocking CachedReadAt, and record when it fails. This allows MP4Reader
// to retry the read as an actual blocking read without holding the lock.
bool
MP4Stream::ReadAt(int64_t aOffset, void* aBuffer, size_t aCount,
                  size_t* aBytesRead)
{
  if (mFailedRead.isSome()) {
    mFailedRead.reset();
  }

  if (!CachedReadAt(aOffset, aBuffer, aCount, aBytesRead)) {
    mFailedRead.emplace(aOffset, aCount);
    return false;
  }

  return true;
}

bool
MP4Stream::CachedReadAt(int64_t aOffset, void* aBuffer, size_t aCount,
                        size_t* aBytesRead)
{
  // First, check our local cache.
  for (size_t i = 0; i < mCache.Length(); ++i) {
    if (mCache[i].mOffset == aOffset && mCache[i].mCount >= aCount) {
      memcpy(aBuffer, mCache[i].Buffer(), aCount);
      *aBytesRead = aCount;
      return true;
    }
  }

  nsresult rv = mResource->ReadFromCache(reinterpret_cast<char*>(aBuffer),
                                         aOffset, aCount);
  if (NS_FAILED(rv)) {
    *aBytesRead = 0;
    return false;
  }
  *aBytesRead = aCount;
  return true;
}

bool
MP4Stream::Length(int64_t* aSize)
{
  if (mResource->GetLength() < 0)
    return false;
  *aSize = mResource->GetLength();
  return true;
}
}
