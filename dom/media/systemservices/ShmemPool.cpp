/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/Logging.h"
#include "mozilla/ShmemPool.h"
#include "mozilla/Move.h"

#undef LOG
#undef LOG_ENABLED
extern PRLogModuleInfo *gCamerasParentLog;
#define LOG(args) MOZ_LOG(gCamerasParentLog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() MOZ_LOG_TEST(gCamerasParentLog, mozilla::LogLevel::Debug)

namespace mozilla {

ShmemPool::ShmemPool(size_t aPoolSize)
  : mMutex("mozilla::ShmemPool"),
    mPoolFree(aPoolSize)
#ifdef DEBUG
    ,mMaxPoolUse(0)
#endif
{
  mShmemPool.SetLength(aPoolSize);
}

mozilla::ShmemBuffer ShmemPool::GetIfAvailable(size_t aSize)
{
  MutexAutoLock lock(mMutex);

  // Pool is empty, don't block caller.
  if (mPoolFree == 0) {
    // This isn't initialized, so will be understood as an error.
    return ShmemBuffer();
  }

  ShmemBuffer& res = mShmemPool[mPoolFree - 1];

  if (!res.mInitialized) {
    return ShmemBuffer();
  }

  MOZ_ASSERT(res.mShmem.IsWritable(), "Pool in Shmem is not writable?");

  if (res.mShmem.Size<char>() < aSize) {
    return ShmemBuffer();
  }

  mPoolFree--;
#ifdef DEBUG
  size_t poolUse = mShmemPool.Length() - mPoolFree;
  if (poolUse > mMaxPoolUse) {
    mMaxPoolUse = poolUse;
    LOG(("Maximum ShmemPool use increased: %d buffers", mMaxPoolUse));
  }
#endif
  return Move(res);
}

template <class T>
mozilla::ShmemBuffer ShmemPool::Get(T* aInstance, size_t aSize)
{
  MutexAutoLock lock(mMutex);

  // Pool is empty, don't block caller.
  if (mPoolFree == 0) {
    // This isn't initialized, so will be understood as an error.
    return ShmemBuffer();
  }

  ShmemBuffer res = Move(mShmemPool[mPoolFree - 1]);

  if (!res.mInitialized) {
    LOG(("Initiaizing new Shmem in pool"));
    aInstance->AllocShmem(aSize, SharedMemory::TYPE_BASIC, &res.mShmem);
    res.mInitialized = true;
  }

  MOZ_ASSERT(res.mShmem.IsWritable(), "Pool in Shmem is not writable?");

  // Prepare buffer, increase size if needed (we never shrink as we don't
  // maintain seperate sized pools and we don't want to keep reallocating)
  if (res.mShmem.Size<char>() < aSize) {
    LOG(("Size change/increase in Shmem Pool"));
    aInstance->DeallocShmem(res.mShmem);
    // this may fail; always check return value
    if (!aInstance->AllocShmem(aSize, SharedMemory::TYPE_BASIC, &res.mShmem)) {
      LOG(("Failure allocating new size Shmem buffer"));
      return ShmemBuffer();
    }
  }

  mPoolFree--;
  return res;
}

void ShmemPool::Put(ShmemBuffer&& aShmem)
{
  MutexAutoLock lock(mMutex);
  MOZ_ASSERT(mPoolFree < mShmemPool.Length());
  mShmemPool[mPoolFree] = Move(aShmem);
  mPoolFree++;
}

template <class T>
void ShmemPool::Cleanup(T* aInstance)
{
  MutexAutoLock lock(mMutex);
  for (size_t i = 0; i < mShmemPool.Length(); i++) {
    if (mShmemPool[i].mInitialized) {
      aInstance->DeallocShmem(mShmemPool[i].Get());
      mShmemPool[i].mInitialized = false;
    }
  }
}

ShmemPool::~ShmemPool()
{
#ifdef DEBUG
  for (size_t i = 0; i < mShmemPool.Length(); i++) {
    MOZ_ASSERT(!mShmemPool[i].Valid());
  }
#endif
}

} // namespace mozilla
