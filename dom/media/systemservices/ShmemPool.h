/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ShmemPool_h
#define mozilla_ShmemPool_h

#include "mozilla/Mutex.h"
#include "mozilla/ipc/Shmem.h"
#include "nsTArray.h"

extern mozilla::LazyLogModule sShmemPoolLog;
#define SHMEMPOOL_LOG(args) \
  MOZ_LOG(sShmemPoolLog, mozilla::LogLevel::Debug, args)
#define SHMEMPOOL_LOG_WARN(args) \
  MOZ_LOG(sShmemPoolLog, mozilla::LogLevel::Warning, args)
#define SHMEMPOOL_LOG_ERROR(args) \
  MOZ_LOG(sShmemPoolLog, mozilla::LogLevel::Error, args)

namespace mozilla {

class ShmemPool;

class ShmemBuffer {
 public:
  ShmemBuffer() : mInitialized(false) {}
  explicit ShmemBuffer(mozilla::ipc::Shmem aShmem) {
    mInitialized = true;
    mShmem = aShmem;
  }

  ShmemBuffer(ShmemBuffer&& rhs) {
    mInitialized = rhs.mInitialized;
    mShmem = std::move(rhs.mShmem);
  }

  ShmemBuffer& operator=(ShmemBuffer&& rhs) {
    MOZ_ASSERT(&rhs != this, "self-moves are prohibited");
    mInitialized = rhs.mInitialized;
    mShmem = std::move(rhs.mShmem);
    return *this;
  }

  // No copies allowed
  ShmemBuffer(const ShmemBuffer&) = delete;
  ShmemBuffer& operator=(const ShmemBuffer&) = delete;

  bool Valid() { return mInitialized; }

  uint8_t* GetBytes() { return mShmem.get<uint8_t>(); }

  mozilla::ipc::Shmem& Get() { return mShmem; }

 private:
  friend class ShmemPool;

  bool mInitialized;
  mozilla::ipc::Shmem mShmem;
};

class ShmemPool final {
 public:
  enum class PoolType { StaticPool, DynamicPool };
  explicit ShmemPool(size_t aPoolSize,
                     PoolType aPoolType = PoolType::StaticPool);
  ~ShmemPool();
  // Get/GetIfAvailable differ in what thread they can run on. GetIfAvailable
  // can run anywhere but won't allocate if the right size isn't available.
  ShmemBuffer GetIfAvailable(size_t aSize);
  void Put(ShmemBuffer&& aShmem);

  // We need to use the allocation/deallocation functions
  // of a specific IPC child/parent instance.
  template <class T>
  void Cleanup(T* aInstance) {
    MutexAutoLock lock(mMutex);
    for (size_t i = 0; i < mShmemPool.Length(); i++) {
      if (mShmemPool[i].mInitialized) {
        aInstance->DeallocShmem(mShmemPool[i].Get());
        mShmemPool[i].mInitialized = false;
      }
    }
  }

  enum class AllocationPolicy { Default, Unsafe };

  template <class T>
  ShmemBuffer Get(T* aInstance, size_t aSize,
                  AllocationPolicy aPolicy = AllocationPolicy::Default) {
    MutexAutoLock lock(mMutex);

    // Pool is empty, don't block caller.
    if (mPoolFree == 0 && mPoolType == PoolType::StaticPool) {
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
    if (mPoolFree == 0) {
      MOZ_ASSERT(mPoolType == PoolType::DynamicPool);
      SHMEMPOOL_LOG(("Dynamic ShmemPool empty, allocating extra Shmem buffer"));
      ShmemBuffer newBuffer;
      mShmemPool.InsertElementAt(0, std::move(newBuffer));
      mPoolFree++;
    }

    ShmemBuffer& res = mShmemPool[mPoolFree - 1];

    if (!res.mInitialized) {
      SHMEMPOOL_LOG(("Initializing new Shmem in pool"));
      if (!AllocateShmem(aInstance, aSize, res, aPolicy)) {
        SHMEMPOOL_LOG(("Failure allocating new Shmem buffer"));
        return ShmemBuffer();
      }
      res.mInitialized = true;
    }

    MOZ_DIAGNOSTIC_ASSERT(res.mShmem.IsWritable(),
                          "Shmem in Pool is not writable?");

    // Prepare buffer, increase size if needed (we never shrink as we don't
    // maintain seperate sized pools and we don't want to keep reallocating)
    if (res.mShmem.Size<char>() < aSize) {
      SHMEMPOOL_LOG(("Size change/increase in Shmem Pool"));
      aInstance->DeallocShmem(res.mShmem);
      res.mInitialized = false;
      // this may fail; always check return value
      if (!AllocateShmem(aInstance, aSize, res, aPolicy)) {
        SHMEMPOOL_LOG(("Failure allocating resized Shmem buffer"));
        return ShmemBuffer();
      } else {
        res.mInitialized = true;
      }
    }

    MOZ_ASSERT(res.mShmem.IsWritable(),
               "Shmem in Pool is not writable post resize?");

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

 private:
  template <class T>
  bool AllocateShmem(T* aInstance, size_t aSize, ShmemBuffer& aRes,
                     AllocationPolicy aPolicy) {
    return (aPolicy == AllocationPolicy::Default &&
            aInstance->AllocShmem(aSize, &aRes.mShmem)) ||
           (aPolicy == AllocationPolicy::Unsafe &&
            aInstance->AllocUnsafeShmem(aSize, &aRes.mShmem));
  }
  const PoolType mPoolType;
  Mutex mMutex MOZ_UNANNOTATED;
  size_t mPoolFree;
  bool mErrorLogged;
#ifdef DEBUG
  size_t mMaxPoolUse;
#endif
  nsTArray<ShmemBuffer> mShmemPool;
};

}  // namespace mozilla

#endif  // mozilla_ShmemPool_h
