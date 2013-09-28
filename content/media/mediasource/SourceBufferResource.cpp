/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SourceBufferResource.h"

#include <string.h>
#include <algorithm>

#include "nsISeekableStream.h"
#include "nsTraceRefcnt.h"
#include "prenv.h"
#include "prlog.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gMediaSourceLog;
#define LOG(type, msg) PR_LOG(gMediaSourceLog, type, msg)
#else
#define LOG(type, msg)
#endif

namespace mozilla {

namespace dom {

class SourceBuffer;

}  // namespace dom

nsresult
SourceBufferResource::Close()
{
  ReentrantMonitorAutoEnter mon(mMonitor);
  LOG(PR_LOG_DEBUG, ("%p SBR::Close", this));
  //MOZ_ASSERT(!mClosed);
  mClosed = true;
  mon.NotifyAll();
  return NS_OK;
}

nsresult
SourceBufferResource::Read(char* aBuffer, uint32_t aCount, uint32_t* aBytes)
{
  ReentrantMonitorAutoEnter mon(mMonitor);
  bool blockingRead = !!aBytes;

  while (blockingRead && !mEnded && mOffset + aCount > GetLength()) {
    LOG(PR_LOG_DEBUG, ("%p SBR::Read waiting for data", this));
    mon.Wait();
  }

  uint32_t available = GetLength() - mOffset;
  uint32_t count = std::min(aCount, available);
  if (!PR_GetEnv("MOZ_QUIET")) {
    LOG(PR_LOG_DEBUG, ("%p SBR::Read aCount=%u length=%u offset=%u "
                       "available=%u count=%u, blocking=%d bufComplete=%d",
                       this, aCount, GetLength(), mOffset, available, count,
                       blockingRead, mEnded));
  }
  if (available == 0) {
    LOG(PR_LOG_DEBUG, ("%p SBR::Read EOF", this));
    *aBytes = 0;
    return NS_OK;
  }

  memcpy(aBuffer, &mInputBuffer[mOffset], count);
  *aBytes = count;
  mOffset += count;
  return NS_OK;
}

nsresult
SourceBufferResource::ReadAt(int64_t aOffset, char* aBuffer, uint32_t aCount, uint32_t* aBytes)
{
  ReentrantMonitorAutoEnter mon(mMonitor);
  nsresult rv = Seek(nsISeekableStream::NS_SEEK_SET, aOffset);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return Read(aBuffer, aCount, aBytes);
}

nsresult
SourceBufferResource::Seek(int32_t aWhence, int64_t aOffset)
{
  ReentrantMonitorAutoEnter mon(mMonitor);
  if (mClosed) {
    return NS_ERROR_FAILURE;
  }

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

  if (newOffset < 0 || newOffset > GetLength()) {
    return NS_ERROR_FAILURE;
  }

  mOffset = newOffset;
  mon.NotifyAll();

  return NS_OK;
}

nsresult
SourceBufferResource::ReadFromCache(char* aBuffer, int64_t aOffset, uint32_t aCount)
{
  ReentrantMonitorAutoEnter mon(mMonitor);
  nsresult rv = Seek(nsISeekableStream::NS_SEEK_SET, aOffset);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return Read(aBuffer, aCount, nullptr);
}

void
SourceBufferResource::AppendData(const uint8_t* aData, uint32_t aLength)
{
  ReentrantMonitorAutoEnter mon(mMonitor);
  mInputBuffer.AppendElements(aData, aLength);
  mon.NotifyAll();
}

void
SourceBufferResource::Ended()
{
  ReentrantMonitorAutoEnter mon(mMonitor);
  mEnded = true;
  mon.NotifyAll();
}

SourceBufferResource::~SourceBufferResource()
{
  MOZ_COUNT_DTOR(SourceBufferResource);
  LOG(PR_LOG_DEBUG, ("%p SBR::~SBR", this));
}

SourceBufferResource::SourceBufferResource(nsIPrincipal* aPrincipal,
                                           const nsACString& aType)
  : mPrincipal(aPrincipal)
  , mType(aType)
  , mMonitor("mozilla::SourceBufferResource::mMonitor")
  , mOffset(0)
  , mClosed(false)
  , mEnded(false)
{
  MOZ_COUNT_CTOR(SourceBufferResource);
  LOG(PR_LOG_DEBUG, ("%p SBR::SBR()", this));
}

} // namespace mozilla
