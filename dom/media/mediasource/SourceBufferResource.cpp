/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SourceBufferResource.h"

#include "mozilla/Logging.h"
#include "mozilla/TaskQueue.h"
#include "MediaData.h"

mozilla::LogModule* GetSourceBufferResourceLog() {
  static mozilla::LazyLogModule sLogModule("SourceBufferResource");
  return sLogModule;
}

#define SBR_DEBUG(arg, ...)                                         \
  DDMOZ_LOG(GetSourceBufferResourceLog(), mozilla::LogLevel::Debug, \
            "::%s: " arg, __func__, ##__VA_ARGS__)
#define SBR_DEBUGV(arg, ...)                                          \
  DDMOZ_LOG(GetSourceBufferResourceLog(), mozilla::LogLevel::Verbose, \
            "::%s: " arg, __func__, ##__VA_ARGS__)

namespace mozilla {

RefPtr<GenericPromise> SourceBufferResource::Close() {
  MOZ_ASSERT(OnThread());
  SBR_DEBUG("Close");
  mClosed = true;
  return GenericPromise::CreateAndResolve(true, __func__);
}

nsresult SourceBufferResource::ReadAt(int64_t aOffset, char* aBuffer,
                                      uint32_t aCount, uint32_t* aBytes) {
  SBR_DEBUG("ReadAt(aOffset=%" PRId64 ", aBuffer=%p, aCount=%u, aBytes=%p)",
            aOffset, aBytes, aCount, aBytes);
  return ReadAtInternal(aOffset, aBuffer, aCount, aBytes);
}

nsresult SourceBufferResource::ReadAtInternal(int64_t aOffset, char* aBuffer,
                                              uint32_t aCount,
                                              uint32_t* aBytes) {
  MOZ_ASSERT(OnThread());

  if (mClosed || aOffset < 0 || uint64_t(aOffset) < mInputBuffer.GetOffset() ||
      aOffset > GetLength()) {
    return NS_ERROR_FAILURE;
  }

  uint32_t available = GetLength() - aOffset;
  uint32_t count = std::min(aCount, available);

  SBR_DEBUGV("offset=%" PRId64 " GetLength()=%" PRId64
             " available=%u count=%u mEnded=%d",
             aOffset, GetLength(), available, count, mEnded);
  if (available == 0) {
    SBR_DEBUGV("reached EOF");
    *aBytes = 0;
    return NS_OK;
  }

  mInputBuffer.CopyData(aOffset, count, aBuffer);
  *aBytes = count;

  return NS_OK;
}

nsresult SourceBufferResource::ReadFromCache(char* aBuffer, int64_t aOffset,
                                             uint32_t aCount) {
  SBR_DEBUG("ReadFromCache(aBuffer=%p, aOffset=%" PRId64 ", aCount=%u)",
            aBuffer, aOffset, aCount);
  uint32_t bytesRead;
  nsresult rv = ReadAtInternal(aOffset, aBuffer, aCount, &bytesRead);
  NS_ENSURE_SUCCESS(rv, rv);

  // ReadFromCache return failure if not all the data is cached.
  return bytesRead == aCount ? NS_OK : NS_ERROR_FAILURE;
}

uint32_t SourceBufferResource::EvictData(uint64_t aPlaybackOffset,
                                         int64_t aThreshold) {
  MOZ_ASSERT(OnThread());
  SBR_DEBUG("EvictData(aPlaybackOffset=%" PRIu64
            ","
            "aThreshold=%" PRId64 ")",
            aPlaybackOffset, aThreshold);
  uint32_t result = mInputBuffer.Evict(aPlaybackOffset, aThreshold);
  return result;
}

void SourceBufferResource::EvictBefore(uint64_t aOffset) {
  MOZ_ASSERT(OnThread());
  SBR_DEBUG("EvictBefore(aOffset=%" PRIu64 ")", aOffset);

  mInputBuffer.EvictBefore(aOffset);
}

uint32_t SourceBufferResource::EvictAll() {
  MOZ_ASSERT(OnThread());
  SBR_DEBUG("EvictAll()");
  return mInputBuffer.EvictAll();
}

void SourceBufferResource::AppendData(MediaByteBuffer* aData) {
  AppendData(MediaSpan(aData));
}

void SourceBufferResource::AppendData(const MediaSpan& aData) {
  MOZ_ASSERT(OnThread());
  SBR_DEBUG("AppendData(aData=%p, aLength=%zu)", aData.Elements(),
            aData.Length());
  mInputBuffer.AppendItem(aData);
  mEnded = false;
}

void SourceBufferResource::Ended() {
  MOZ_ASSERT(OnThread());
  SBR_DEBUG("");
  mEnded = true;
}

SourceBufferResource::~SourceBufferResource() { SBR_DEBUG(""); }

SourceBufferResource::SourceBufferResource()
#if defined(DEBUG)
    : mThread(AbstractThread::GetCurrent())
#endif
{
  SBR_DEBUG("");
}

#if defined(DEBUG)
const AbstractThread* SourceBufferResource::GetThread() const {
  return mThread;
}
bool SourceBufferResource::OnThread() const {
  return !GetThread() || GetThread()->IsCurrentThreadIn();
}
#endif

#undef SBR_DEBUG
#undef SBR_DEBUGV
}  // namespace mozilla
