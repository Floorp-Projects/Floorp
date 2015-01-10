/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SourceBufferResource.h"

#include <algorithm>

#include "nsISeekableStream.h"
#include "nsISupports.h"
#include "prlog.h"

#ifdef PR_LOGGING
PRLogModuleInfo* GetSourceBufferResourceLog()
{
  static PRLogModuleInfo* sLogModule;
  if (!sLogModule) {
    sLogModule = PR_NewLogModule("SourceBufferResource");
  }
  return sLogModule;
}

#define SBR_DEBUG(...) PR_LOG(GetSourceBufferResourceLog(), PR_LOG_DEBUG, (__VA_ARGS__))
#define SBR_DEBUGV(...) PR_LOG(GetSourceBufferResourceLog(), PR_LOG_DEBUG+1, (__VA_ARGS__))
#else
#define SBR_DEBUG(...)
#define SBR_DEBUGV(...)
#endif

namespace mozilla {

nsresult
SourceBufferResource::Close()
{
  ReentrantMonitorAutoEnter mon(mMonitor);
  SBR_DEBUG("SourceBufferResource(%p)::Close", this);
  //MOZ_ASSERT(!mClosed);
  mClosed = true;
  mon.NotifyAll();
  return NS_OK;
}

nsresult
SourceBufferResource::Read(char* aBuffer, uint32_t aCount, uint32_t* aBytes)
{
  SBR_DEBUGV("SourceBufferResource(%p)::Read(aBuffer=%p, aCount=%u, aBytes=%p)",
             this, aBytes, aCount, aBytes);
  ReentrantMonitorAutoEnter mon(mMonitor);

  return ReadInternal(aBuffer, aCount, aBytes, /* aMayBlock = */ true);
}

nsresult
SourceBufferResource::ReadInternal(char* aBuffer, uint32_t aCount, uint32_t* aBytes, bool aMayBlock)
{
  mMonitor.AssertCurrentThreadIn();
  MOZ_ASSERT_IF(!aMayBlock, aBytes);

  // Cache the offset for the read in case mOffset changes while waiting on the
  // monitor below. It's basically impossible to implement these API semantics
  // sanely. :-(
  uint64_t readOffset = mOffset;

  while (aMayBlock &&
         !mEnded &&
         readOffset + aCount > static_cast<uint64_t>(GetLength())) {
    SBR_DEBUGV("SourceBufferResource(%p)::ReadInternal waiting for data", this);
    mMonitor.Wait();
  }

  uint32_t available = GetLength() - readOffset;
  uint32_t count = std::min(aCount, available);
  SBR_DEBUGV("SourceBufferResource(%p)::ReadInternal() readOffset=%llu GetLength()=%u available=%u count=%u mEnded=%d",
             this, readOffset, GetLength(), available, count, mEnded);
  if (available == 0) {
    SBR_DEBUGV("SourceBufferResource(%p)::ReadInternal() reached EOF", this);
    *aBytes = 0;
    return NS_OK;
  }

  mInputBuffer.CopyData(readOffset, count, aBuffer);
  *aBytes = count;

  // From IRC:
  // <@cpearce>bholley: *this* is why there should only every be a ReadAt() and
  // no Read() on a Stream abstraction! there's no good answer, they all suck.
  mOffset = readOffset + count;

  return NS_OK;
}

nsresult
SourceBufferResource::ReadAt(int64_t aOffset, char* aBuffer, uint32_t aCount, uint32_t* aBytes)
{
  SBR_DEBUG("SourceBufferResource(%p)::ReadAt(aOffset=%lld, aBuffer=%p, aCount=%u, aBytes=%p)",
            this, aOffset, aBytes, aCount, aBytes);
  ReentrantMonitorAutoEnter mon(mMonitor);
  return ReadAtInternal(aOffset, aBuffer, aCount, aBytes, /* aMayBlock = */ true);
}

nsresult
SourceBufferResource::ReadAtInternal(int64_t aOffset, char* aBuffer, uint32_t aCount, uint32_t* aBytes,
                                     bool aMayBlock)
{
  mMonitor.AssertCurrentThreadIn();
  nsresult rv = SeekInternal(aOffset);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return ReadInternal(aBuffer, aCount, aBytes, aMayBlock);
}

nsresult
SourceBufferResource::Seek(int32_t aWhence, int64_t aOffset)
{
  SBR_DEBUG("SourceBufferResource(%p)::Seek(aWhence=%d, aOffset=%lld)", this, aWhence, aOffset);
  ReentrantMonitorAutoEnter mon(mMonitor);

  int64_t newOffset = mOffset;
  switch (aWhence) {
  case nsISeekableStream::NS_SEEK_END:
    newOffset = GetLength() - aOffset;
    break;
  case nsISeekableStream::NS_SEEK_CUR:
    newOffset += aOffset;
    break;
  case nsISeekableStream::NS_SEEK_SET:
    newOffset = aOffset;
    break;
  }

  SBR_DEBUGV("SourceBufferResource(%p)::Seek() newOffset=%lld GetOffset()=%llu GetLength()=%llu)",
             this, newOffset, mInputBuffer.GetOffset(), GetLength());
  nsresult rv = SeekInternal(newOffset);
  mon.NotifyAll();
  return rv;
}

nsresult
SourceBufferResource::SeekInternal(int64_t aOffset)
{
  mMonitor.AssertCurrentThreadIn();

  if (mClosed ||
      aOffset < 0 ||
      uint64_t(aOffset) < mInputBuffer.GetOffset() ||
      aOffset > GetLength()) {
    return NS_ERROR_FAILURE;
  }

  mOffset = aOffset;
  return NS_OK;
}

nsresult
SourceBufferResource::ReadFromCache(char* aBuffer, int64_t aOffset, uint32_t aCount)
{
  SBR_DEBUG("SourceBufferResource(%p)::ReadFromCache(aBuffer=%p, aOffset=%lld, aCount=%u)",
            this, aBuffer, aOffset, aCount);
  ReentrantMonitorAutoEnter mon(mMonitor);
  uint32_t bytesRead;
  int64_t oldOffset = mOffset;
  nsresult rv = ReadAtInternal(aOffset, aBuffer, aCount, &bytesRead, /* aMayBlock = */ false);
  mOffset = oldOffset; // ReadFromCache isn't supposed to affect the seek position.
  NS_ENSURE_SUCCESS(rv, rv);

  // ReadFromCache return failure if not all the data is cached.
  return bytesRead == aCount ? NS_OK : NS_ERROR_FAILURE;
}

uint32_t
SourceBufferResource::EvictData(uint32_t aThreshold)
{
  SBR_DEBUG("SourceBufferResource(%p)::EvictData(aThreshold=%u)", this, aThreshold);
  ReentrantMonitorAutoEnter mon(mMonitor);
  return mInputBuffer.Evict(mOffset, aThreshold);
}

void
SourceBufferResource::EvictBefore(uint64_t aOffset)
{
  SBR_DEBUG("SourceBufferResource(%p)::EvictBefore(aOffset=%llu)", this, aOffset);
  ReentrantMonitorAutoEnter mon(mMonitor);
  // If aOffset is past the current playback offset we don't evict.
  if (aOffset < mOffset) {
    mInputBuffer.Evict(aOffset, 0);
  }
}

void
SourceBufferResource::AppendData(const uint8_t* aData, uint32_t aLength)
{
  SBR_DEBUG("SourceBufferResource(%p)::AppendData(aData=%p, aLength=%u)", this, aData, aLength);
  ReentrantMonitorAutoEnter mon(mMonitor);
  mInputBuffer.AppendItem(aData, aLength);
  mon.NotifyAll();
}

void
SourceBufferResource::Ended()
{
  SBR_DEBUG("SourceBufferResource(%p)::Ended()", this);
  ReentrantMonitorAutoEnter mon(mMonitor);
  mEnded = true;
  mon.NotifyAll();
}

SourceBufferResource::~SourceBufferResource()
{
  SBR_DEBUG("SourceBufferResource(%p)::~SourceBufferResource()", this);
  MOZ_COUNT_DTOR(SourceBufferResource);
}

SourceBufferResource::SourceBufferResource(const nsACString& aType)
  : mType(aType)
  , mMonitor("mozilla::SourceBufferResource::mMonitor")
  , mOffset(0)
  , mClosed(false)
  , mEnded(false)
{
  SBR_DEBUG("SourceBufferResource(%p)::SourceBufferResource(aType=%s)",
            this, nsCString(aType).get());
  MOZ_COUNT_CTOR(SourceBufferResource);
}

} // namespace mozilla
