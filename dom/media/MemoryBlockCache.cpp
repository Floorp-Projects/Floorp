/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MemoryBlockCache.h"

#include "mozilla/Logging.h"

namespace mozilla {

#undef LOG
LazyLogModule gMemoryBlockCacheLog("MemoryBlockCache");
#define LOG(x, ...)                                                            \
  MOZ_LOG(gMemoryBlockCacheLog, LogLevel::Debug, ("%p " x, this, ##__VA_ARGS__))

MemoryBlockCache::MemoryBlockCache(int64_t aContentLength)
  // Buffer whole blocks.
  : mInitialContentLength((aContentLength >= 0) ? size_t(aContentLength) : 0)
  , mMutex("MemoryBlockCache")
{
}

MemoryBlockCache::~MemoryBlockCache()
{
  MOZ_ASSERT(mBuffer.IsEmpty());
}

bool
MemoryBlockCache::EnsureBufferCanContain(size_t aContentLength)
{
  mMutex.AssertCurrentThreadOwns();
  if (aContentLength == 0) {
    return true;
  }
  size_t desiredLength = ((aContentLength - 1) / BLOCK_SIZE + 1) * BLOCK_SIZE;
  if (mBuffer.Length() >= desiredLength) {
    // Already large enough.
    return true;
  }
  // Need larger buffer, attempt to re-allocate.
  return mBuffer.SetLength(desiredLength, mozilla::fallible);
}

nsresult
MemoryBlockCache::Init()
{
  LOG("@%p Init()", this);
  MutexAutoLock lock(mMutex);
  // Attempt to pre-allocate buffer for expected content length.
  return EnsureBufferCanContain(mInitialContentLength) ? NS_OK
                                                       : NS_ERROR_FAILURE;
}

void
MemoryBlockCache::Close()
{
  LOG("@%p Close()", this);
  MutexAutoLock lock(mMutex);
  mBuffer.SetLength(0);
}

nsresult
MemoryBlockCache::WriteBlock(uint32_t aBlockIndex,
                             Span<const uint8_t> aData1,
                             Span<const uint8_t> aData2)
{
  MutexAutoLock lock(mMutex);

  size_t offset = BlockIndexToOffset(aBlockIndex);
  if (!EnsureBufferCanContain(offset + aData1.Length() + aData2.Length())) {
    return NS_ERROR_FAILURE;
  }

  memcpy(mBuffer.Elements() + offset, aData1.Elements(), aData1.Length());
  if (aData2.Length() > 0) {
    memcpy(mBuffer.Elements() + offset + aData1.Length(),
           aData2.Elements(),
           aData2.Length());
  }

  return NS_OK;
}

nsresult
MemoryBlockCache::Read(int64_t aOffset,
                       uint8_t* aData,
                       int32_t aLength,
                       int32_t* aBytes)
{
  MutexAutoLock lock(mMutex);

  MOZ_ASSERT(aOffset >= 0);
  if (aOffset + aLength > int64_t(mBuffer.Length())) {
    return NS_ERROR_FAILURE;
  }

  memcpy(aData, mBuffer.Elements() + aOffset, aLength);
  *aBytes = aLength;

  return NS_OK;
}

nsresult
MemoryBlockCache::MoveBlock(int32_t aSourceBlockIndex, int32_t aDestBlockIndex)
{
  MutexAutoLock lock(mMutex);

  size_t sourceOffset = BlockIndexToOffset(aSourceBlockIndex);
  size_t destOffset = BlockIndexToOffset(aDestBlockIndex);
  if (sourceOffset + BLOCK_SIZE > mBuffer.Length() ||
      !EnsureBufferCanContain(destOffset + BLOCK_SIZE)) {
    return NS_ERROR_FAILURE;
  }

  memcpy(mBuffer.Elements() + destOffset,
         mBuffer.Elements() + sourceOffset,
         BLOCK_SIZE);

  return NS_OK;
}

} // End namespace mozilla.

// avoid redefined macro in unified build
#undef LOG
