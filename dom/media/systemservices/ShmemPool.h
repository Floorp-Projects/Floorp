/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ShmemPool_h
#define mozilla_ShmemPool_h

#include "mozilla/ipc/Shmem.h"
#include "mozilla/Mutex.h"

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
    mShmem = Move(rhs.mShmem);
  }

  ShmemBuffer& operator=(ShmemBuffer&& rhs) {
    MOZ_ASSERT(&rhs != this, "self-moves are prohibited");
    mInitialized = rhs.mInitialized;
    mShmem = Move(rhs.mShmem);
    return *this;
  }

  // No copies allowed
  ShmemBuffer(const ShmemBuffer&) = delete;
  ShmemBuffer& operator=(const ShmemBuffer&) = delete;

  bool Valid() {
    return mInitialized;
  }

  char* GetBytes() {
    return mShmem.get<char>();
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
  // We need to use the allocation/deallocation functions
  // of a specific IPC child/parent instance.
  template <class T> void Cleanup(T* aInstance);
  // These 2 differ in what thread they can run on. GetIfAvailable
  // can run anywhere but won't allocate if the right size isn't available.
  ShmemBuffer GetIfAvailable(size_t aSize);
  template <class T> ShmemBuffer Get(T* aInstance, size_t aSize);
  void Put(ShmemBuffer&& aShmem);

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
