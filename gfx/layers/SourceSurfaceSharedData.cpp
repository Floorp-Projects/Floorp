/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SourceSurfaceSharedData.h"

#include "mozilla/Likely.h"
#include "mozilla/StaticPrefs_image.h"
#include "mozilla/Types.h"  // for decltype
#include "mozilla/layers/SharedSurfacesChild.h"
#include "mozilla/layers/SharedSurfacesParent.h"
#include "nsDebug.h"  // for NS_ABORT_OOM

#include "base/process_util.h"

#ifdef DEBUG
/**
 * If defined, this makes SourceSurfaceSharedData::Finalize memory protect the
 * underlying shared buffer in the producing process (the content or UI
 * process). Given flushing the page table is expensive, and its utility is
 * predominantly diagnostic (in case of overrun), turn it off by default.
 */
#  define SHARED_SURFACE_PROTECT_FINALIZED
#endif

using namespace mozilla::layers;

namespace mozilla {
namespace gfx {

void SourceSurfaceSharedDataWrapper::Init(
    const IntSize& aSize, int32_t aStride, SurfaceFormat aFormat,
    const SharedMemoryBasic::Handle& aHandle, base::ProcessId aCreatorPid) {
  MOZ_ASSERT(!mBuf);
  mSize = aSize;
  mStride = aStride;
  mFormat = aFormat;
  mCreatorPid = aCreatorPid;

  size_t len = GetAlignedDataLength();
  mBuf = MakeAndAddRef<SharedMemoryBasic>();
  if (!mBuf->SetHandle(aHandle, ipc::SharedMemory::RightsReadOnly)) {
    MOZ_CRASH("Invalid shared memory handle!");
  }

  bool mapped = EnsureMapped(len);
  if ((sizeof(uintptr_t) <= 4 ||
       StaticPrefs::image_mem_shared_unmap_force_enabled_AtStartup()) &&
      len / 1024 >
          StaticPrefs::image_mem_shared_unmap_min_threshold_kb_AtStartup()) {
    mHandleLock.emplace("SourceSurfaceSharedDataWrapper::mHandleLock");

    if (mapped) {
      // Tracking at the initial mapping, and not just after the first use of
      // the surface means we might get unmapped again before the next frame
      // gets rendered if a low virtual memory condition persists.
      SharedSurfacesParent::AddTracking(this);
    }
  } else if (!mapped) {
    // We don't support unmapping for this surface, and we failed to map it.
    NS_ABORT_OOM(len);
  } else {
    mBuf->CloseHandle();
  }
}

void SourceSurfaceSharedDataWrapper::Init(SourceSurfaceSharedData* aSurface) {
  MOZ_ASSERT(!mBuf);
  MOZ_ASSERT(aSurface);
  mSize = aSurface->mSize;
  mStride = aSurface->mStride;
  mFormat = aSurface->mFormat;
  mCreatorPid = base::GetCurrentProcId();
  mBuf = aSurface->mBuf;
}

bool SourceSurfaceSharedDataWrapper::EnsureMapped(size_t aLength) {
  MOZ_ASSERT(!GetData());

  while (!mBuf->Map(aLength)) {
    nsTArray<RefPtr<SourceSurfaceSharedDataWrapper>> expired;
    if (!SharedSurfacesParent::AgeOneGeneration(expired)) {
      return false;
    }
    MOZ_ASSERT(!expired.Contains(this));
    SharedSurfacesParent::ExpireMap(expired);
  }

  return true;
}

bool SourceSurfaceSharedDataWrapper::Map(MapType,
                                         MappedSurface* aMappedSurface) {
  uint8_t* dataPtr;

  if (mHandleLock) {
    MutexAutoLock lock(*mHandleLock);
    dataPtr = GetData();
    if (mMapCount == 0) {
      SharedSurfacesParent::RemoveTracking(this);
      if (!dataPtr) {
        size_t len = GetAlignedDataLength();
        if (!EnsureMapped(len)) {
          NS_ABORT_OOM(len);
        }
        dataPtr = GetData();
      }
    }
    ++mMapCount;
  } else {
    dataPtr = GetData();
    ++mMapCount;
  }

  MOZ_ASSERT(dataPtr);
  aMappedSurface->mData = dataPtr;
  aMappedSurface->mStride = mStride;
  return true;
}

void SourceSurfaceSharedDataWrapper::Unmap() {
  if (mHandleLock) {
    MutexAutoLock lock(*mHandleLock);
    if (--mMapCount == 0) {
      SharedSurfacesParent::AddTracking(this);
    }
  } else {
    --mMapCount;
  }
  MOZ_ASSERT(mMapCount >= 0);
}

void SourceSurfaceSharedDataWrapper::ExpireMap() {
  MutexAutoLock lock(*mHandleLock);
  if (mMapCount == 0) {
    mBuf->Unmap();
  }
}

bool SourceSurfaceSharedData::Init(const IntSize& aSize, int32_t aStride,
                                   SurfaceFormat aFormat,
                                   bool aShare /* = true */) {
  mSize = aSize;
  mStride = aStride;
  mFormat = aFormat;

  size_t len = GetAlignedDataLength();
  mBuf = new SharedMemoryBasic();
  if (NS_WARN_IF(!mBuf->Create(len)) || NS_WARN_IF(!mBuf->Map(len))) {
    mBuf = nullptr;
    return false;
  }

  if (aShare) {
    layers::SharedSurfacesChild::Share(this);
  }

  return true;
}

void SourceSurfaceSharedData::GuaranteePersistance() {
  // Shared memory is not unmapped until we release SourceSurfaceSharedData.
}

void SourceSurfaceSharedData::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                                                  SizeOfInfo& aInfo) const {
  MutexAutoLock lock(mMutex);
  aInfo.AddType(SurfaceType::DATA_SHARED);
  if (mBuf) {
    aInfo.mNonHeapBytes = GetAlignedDataLength();
  }
  if (!mClosed) {
    aInfo.mExternalHandles = 1;
  }
  Maybe<wr::ExternalImageId> extId = SharedSurfacesChild::GetExternalId(this);
  if (extId) {
    aInfo.mExternalId = wr::AsUint64(extId.ref());
  }
}

uint8_t* SourceSurfaceSharedData::GetDataInternal() const {
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

nsresult SourceSurfaceSharedData::ShareToProcess(
    base::ProcessId aPid, SharedMemoryBasic::Handle& aHandle) {
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

void SourceSurfaceSharedData::CloseHandleInternal() {
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

bool SourceSurfaceSharedData::ReallocHandle() {
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
  if (NS_WARN_IF(!buf->Create(len)) || NS_WARN_IF(!buf->Map(len))) {
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

void SourceSurfaceSharedData::Finalize() {
  MutexAutoLock lock(mMutex);
  MOZ_ASSERT(!mFinalized);

#ifdef SHARED_SURFACE_PROTECT_FINALIZED
  size_t len = GetAlignedDataLength();
  mBuf->Protect(static_cast<char*>(mBuf->memory()), len, RightsRead);
#endif

  mFinalized = true;
}

}  // namespace gfx
}  // namespace mozilla
