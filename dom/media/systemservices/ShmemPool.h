/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ShmemPool_h
#define mozilla_ShmemPool_h

#include "mozilla/ipc/Shmem.h"
#include "mozilla/Mutex.h"

#undef LOG
#undef LOG_ENABLED
extern mozilla::LazyLogModule gCamerasParentLog;
#define LOG(args) MOZ_LOG(gCamerasParentLog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() MOZ_LOG_TEST(gCamerasParentLog, mozilla::LogLevel::Debug)

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

  bool Valid() {
    return mInitialized;
  }

  uint8_t * GetBytes() {
    return mShmem.get<uint8_t>();
  }

  mozilla::ipc::Shmem& Get() {
    return mShmem;
  }

private:
  friend class ShmemPool;

  bool mInitialized;
  mozilla::ipc::Shmem mShmem;
};

class ShmemPool {
public:
  explicit ShmemPool(size_t aPoolSize);
  ~ShmemPool();
  // Get/GetIfAvailable differ in what thread they can run on. GetIfAvailable
  // can run anywhere but won't allocate if the right size isn't available.
  ShmemBuffer GetIfAvailable(size_t aSize);
  void Put(ShmemBuffer&& aShmem);

  // We need to use the allocation/deallocation functions
  // of a specific IPC child/parent instance.
  template <class T>
  void Cleanup(T* aInstance)
  {
    MutexAutoLock lock(mMutex);
    for (size_t i = 0; i < mShmemPool.Length(); i++) {
      if (mShmemPool[i].mInitialized) {
        aInstance->DeallocShmem(mShmemPool[i].Get());
        mShmemPool[i].mInitialized = false;
      }
    }
  }

  template <class T>
  ShmemBuffer Get(T* aInstance, size_t aSize)
  {
    MutexAutoLock lock(mMutex);

    // Pool is empty, don't block caller.
    if (mPoolFree == 0) {
      // This isn't initialized, so will be understood as an error.
      return ShmemBuffer();
    }

    ShmemBuffer& res = mShmemPool[mPoolFree - 1];

    if (!res.mInitialized) {
      LOG(("Initializing new Shmem in pool"));
      if (!aInstance->AllocShmem(aSize, ipc::SharedMemory::TYPE_BASIC, &res.mShmem)) {
        LOG(("Failure allocating new Shmem buffer"));
        return ShmemBuffer();
      }
      res.mInitialized = true;
    }

    MOZ_ASSERT(res.mShmem.IsWritable(), "Shmem in Pool is not writable?");

    // Prepare buffer, increase size if needed (we never shrink as we don't
    // maintain seperate sized pools and we don't want to keep reallocating)
    if (res.mShmem.Size<char>() < aSize) {
      LOG(("Size change/increase in Shmem Pool"));
      aInstance->DeallocShmem(res.mShmem);
      res.mInitialized = false;
      // this may fail; always check return value
      if (!aInstance->AllocShmem(aSize, ipc::SharedMemory::TYPE_BASIC, &res.mShmem)) {
        LOG(("Failure allocating resized Shmem buffer"));
        return ShmemBuffer();
      } else {
        res.mInitialized = true;
      }
    }

    MOZ_ASSERT(res.mShmem.IsWritable(), "Shmem in Pool is not writable post resize?");

    mPoolFree--;
#ifdef DEBUG
    size_t poolUse = mShmemPool.Length() - mPoolFree;
    if (poolUse > mMaxPoolUse) {
      mMaxPoolUse = poolUse;
      LOG(("Maximum ShmemPool use increased: %zu buffers", mMaxPoolUse));
    }
#endif
    return std::move(res);
  }

private:
  Mutex mMutex;
  size_t mPoolFree;
#ifdef DEBUG
  size_t mMaxPoolUse;
#endif
  nsTArray<ShmemBuffer> mShmemPool;
};


} // namespace mozilla

#endif  // mozilla_ShmemPool_h
