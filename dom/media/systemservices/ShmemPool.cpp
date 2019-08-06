/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/Logging.h"
#include "mozilla/ShmemPool.h"
#include "mozilla/Move.h"

mozilla::LazyLogModule sShmemPoolLog("ShmemPool");

#define SHMEMPOOL_LOG_VERBOSE(args) \
  MOZ_LOG(sShmemPoolLog, mozilla::LogLevel::Verbose, args)

namespace mozilla {

ShmemPool::ShmemPool(size_t aPoolSize)
    : mMutex("mozilla::ShmemPool"),
      mPoolFree(aPoolSize),
      mErrorLogged(false)
#ifdef DEBUG
      ,
      mMaxPoolUse(0)
#endif
{
  mShmemPool.SetLength(aPoolSize);
}

mozilla::ShmemBuffer ShmemPool::GetIfAvailable(size_t aSize) {
  MutexAutoLock lock(mMutex);

  // Pool is empty, don't block caller.
  if (mPoolFree == 0) {
    if (!mErrorLogged) {
      // log "out of pool" once as error to avoid log spam
      mErrorLogged = true;
      SHMEMPOOL_LOG_ERROR(
          ("ShmemPool is empty, future occurrences "
           "will be logged as warnings"));
    } else {
      SHMEMPOOL_LOG_WARN(("ShmemPool is empty"));
    }
    // This isn't initialized, so will be understood as an error.
    return ShmemBuffer();
  }

  ShmemBuffer& res = mShmemPool[mPoolFree - 1];

  if (!res.mInitialized) {
    SHMEMPOOL_LOG(("No free preallocated Shmem"));
    return ShmemBuffer();
  }

  MOZ_ASSERT(res.mShmem.IsWritable(), "Pool in Shmem is not writable?");

  if (res.mShmem.Size<uint8_t>() < aSize) {
    SHMEMPOOL_LOG(("Free Shmem but not of the right size"));
    return ShmemBuffer();
  }

  mPoolFree--;
#ifdef DEBUG
  size_t poolUse = mShmemPool.Length() - mPoolFree;
  if (poolUse > mMaxPoolUse) {
    mMaxPoolUse = poolUse;
    SHMEMPOOL_LOG(
        ("Maximum ShmemPool use increased: %zu buffers", mMaxPoolUse));
  }
#endif
  return std::move(res);
}

void ShmemPool::Put(ShmemBuffer&& aShmem) {
  MutexAutoLock lock(mMutex);
  MOZ_ASSERT(mPoolFree < mShmemPool.Length());
  mShmemPool[mPoolFree] = std::move(aShmem);
  mPoolFree++;
#ifdef DEBUG
  size_t poolUse = mShmemPool.Length() - mPoolFree;
  if (poolUse > 0) {
    SHMEMPOOL_LOG_VERBOSE(("ShmemPool usage reduced to %zu buffers", poolUse));
  }
#endif
}

ShmemPool::~ShmemPool() {
#ifdef DEBUG
  for (size_t i = 0; i < mShmemPool.Length(); i++) {
    MOZ_ASSERT(!mShmemPool[i].Valid());
  }
#endif
}

}  // namespace mozilla
