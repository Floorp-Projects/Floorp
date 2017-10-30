/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SourceSurfaceSharedData.h"

#include "mozilla/Likely.h"
#include "mozilla/Types.h" // for decltype

namespace mozilla {
namespace gfx {

bool
SourceSurfaceSharedData::Init(const IntSize &aSize,
                              int32_t aStride,
                              SurfaceFormat aFormat)
{
  mSize = aSize;
  mStride = aStride;
  mFormat = aFormat;

  size_t len = GetAlignedDataLength();
  mBuf = new SharedMemoryBasic();
  if (NS_WARN_IF(!mBuf->Create(len)) ||
      NS_WARN_IF(!mBuf->Map(len))) {
    mBuf = nullptr;
    return false;
  }

  return true;
}

void
SourceSurfaceSharedData::GuaranteePersistance()
{
  // Shared memory is not unmapped until we release SourceSurfaceSharedData.
}

void
SourceSurfaceSharedData::AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                                                size_t& aHeapSizeOut,
                                                size_t& aNonHeapSizeOut) const
{
  if (mBuf) {
    aNonHeapSizeOut += GetAlignedDataLength();
  }
}

uint8_t*
SourceSurfaceSharedData::GetDataInternal() const
{
  mMutex.AssertCurrentThreadOwns();

  // If we have an old buffer lingering, it is because we get reallocated to
  // get a new handle to share, but there were still active mappings.
  if (MOZ_UNLIKELY(mOldBuf)) {
    MOZ_ASSERT(mMapCount > 0);
    MOZ_ASSERT(mFinalized);
    return static_cast<uint8_t*>(mOldBuf->memory());
  }
  return static_cast<uint8_t*>(mBuf->memory());
}

nsresult
SourceSurfaceSharedData::ShareToProcess(base::ProcessId aPid,
                                        SharedMemoryBasic::Handle& aHandle)
{
  MutexAutoLock lock(mMutex);

  if (mClosed) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  bool shared = mBuf->ShareToProcess(aPid, &aHandle);
  if (MOZ_UNLIKELY(!shared)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

void
SourceSurfaceSharedData::CloseHandleInternal()
{
  mMutex.AssertCurrentThreadOwns();

  if (mClosed) {
    return;
  }

  if (mFinalized && mShared) {
    mBuf->CloseHandle();
    mClosed = true;
  }
}

bool
SourceSurfaceSharedData::ReallocHandle()
{
  MutexAutoLock lock(mMutex);
  MOZ_ASSERT(mClosed);
  MOZ_ASSERT(mFinalized);

  size_t len = GetAlignedDataLength();
  RefPtr<SharedMemoryBasic> buf = new SharedMemoryBasic();
  if (NS_WARN_IF(!buf->Create(len)) ||
      NS_WARN_IF(!buf->Map(len))) {
    return false;
  }

  size_t copyLen = GetDataLength();
  memcpy(buf->memory(), mBuf->memory(), copyLen);
  buf->Protect(static_cast<char*>(buf->memory()), len, RightsRead);

  if (mMapCount > 0 && !mOldBuf) {
    mOldBuf = Move(mBuf);
  }
  mBuf = Move(buf);
  mClosed = false;
  mShared = false;
  return true;
}

void
SourceSurfaceSharedData::Finalize()
{
  MutexAutoLock lock(mMutex);
  MOZ_ASSERT(!mClosed);
  MOZ_ASSERT(!mFinalized);

  size_t len = GetAlignedDataLength();
  mBuf->Protect(static_cast<char*>(mBuf->memory()), len, RightsRead);

  mFinalized = true;
  CloseHandleInternal();
}

} // namespace gfx
} // namespace mozilla
