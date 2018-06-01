/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SourceSurfaceSharedData.h"

#include "mozilla/Likely.h"
#include "mozilla/Types.h" // for decltype
#include "mozilla/layers/SharedSurfacesChild.h"

#ifdef DEBUG
/**
 * If defined, this makes SourceSurfaceSharedData::Finalize memory protect the
 * underlying shared buffer in the producing process (the content or UI
 * process). Given flushing the page table is expensive, and its utility is
 * predominantly diagnostic (in case of overrun), turn it off by default.
 */
#define SHARED_SURFACE_PROTECT_FINALIZED
#endif

namespace mozilla {
namespace gfx {

bool
SourceSurfaceSharedDataWrapper::Init(const IntSize& aSize,
                                     int32_t aStride,
                                     SurfaceFormat aFormat,
                                     const SharedMemoryBasic::Handle& aHandle,
                                     base::ProcessId aCreatorPid)
{
  MOZ_ASSERT(!mBuf);
  mSize = aSize;
  mStride = aStride;
  mFormat = aFormat;
  mCreatorPid = aCreatorPid;

  size_t len = GetAlignedDataLength();
  mBuf = MakeAndAddRef<SharedMemoryBasic>();
  if (NS_WARN_IF(!mBuf->SetHandle(aHandle, ipc::SharedMemory::RightsReadOnly)) ||
      NS_WARN_IF(!mBuf->Map(len))) {
    mBuf = nullptr;
    return false;
  }

  mBuf->CloseHandle();
  return true;
}

void
SourceSurfaceSharedDataWrapper::Init(SourceSurfaceSharedData* aSurface)
{
  MOZ_ASSERT(!mBuf);
  MOZ_ASSERT(aSurface);
  mSize = aSurface->mSize;
  mStride = aSurface->mStride;
  mFormat = aSurface->mFormat;
  mCreatorPid = base::GetCurrentProcId();
  mBuf = aSurface->mBuf;
}

bool
SourceSurfaceSharedData::Init(const IntSize &aSize,
                              int32_t aStride,
                              SurfaceFormat aFormat,
                              bool aShare /* = true */)
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

  if (aShare) {
    layers::SharedSurfacesChild::Share(this);
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
                                                size_t& aNonHeapSizeOut,
                                                size_t& aExtHandlesOut) const
{
  MutexAutoLock lock(mMutex);
  if (mBuf) {
    aNonHeapSizeOut += GetAlignedDataLength();
  }
  if (!mClosed) {
    ++aExtHandlesOut;
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
  MOZ_ASSERT(mHandleCount > 0);

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
    MOZ_ASSERT(mHandleCount == 0);
    MOZ_ASSERT(mShared);
    return;
  }

  if (mShared) {
    mBuf->CloseHandle();
    mClosed = true;
  }
}

bool
SourceSurfaceSharedData::ReallocHandle()
{
  MutexAutoLock lock(mMutex);
  MOZ_ASSERT(mHandleCount > 0);
  MOZ_ASSERT(mClosed);

  if (NS_WARN_IF(!mFinalized)) {
    // We haven't finished populating the surface data yet, which means we are
    // out of luck, as we have no means of synchronizing with the producer to
    // write new data to a new buffer. This should be fairly rare, caused by a
    // crash in the GPU process, while we were decoding an image.
    return false;
  }

  size_t len = GetAlignedDataLength();
  RefPtr<SharedMemoryBasic> buf = new SharedMemoryBasic();
  if (NS_WARN_IF(!buf->Create(len)) ||
      NS_WARN_IF(!buf->Map(len))) {
    return false;
  }

  size_t copyLen = GetDataLength();
  memcpy(buf->memory(), mBuf->memory(), copyLen);
#ifdef SHARED_SURFACE_PROTECT_FINALIZED
  buf->Protect(static_cast<char*>(buf->memory()), len, RightsRead);
#endif

  if (mMapCount > 0 && !mOldBuf) {
    mOldBuf = std::move(mBuf);
  }
  mBuf = std::move(buf);
  mClosed = false;
  mShared = false;
  return true;
}

void
SourceSurfaceSharedData::Finalize()
{
  MutexAutoLock lock(mMutex);
  MOZ_ASSERT(!mFinalized);

#ifdef SHARED_SURFACE_PROTECT_FINALIZED
  size_t len = GetAlignedDataLength();
  mBuf->Protect(static_cast<char*>(mBuf->memory()), len, RightsRead);
#endif

  mFinalized = true;
}

} // namespace gfx
} // namespace mozilla
