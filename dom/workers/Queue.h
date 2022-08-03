/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workerinternal_Queue_h
#define mozilla_dom_workerinternal_Queue_h

#include "mozilla/Mutex.h"
#include "nsTArray.h"

namespace mozilla::dom::workerinternals {

template <typename T, int TCount>
struct StorageWithTArray {
  typedef AutoTArray<T, TCount> StorageType;

  static void Reverse(StorageType& aStorage) {
    uint32_t length = aStorage.Length();
    for (uint32_t index = 0; index < length / 2; index++) {
      uint32_t reverseIndex = length - 1 - index;

      T t1 = aStorage.ElementAt(index);
      T t2 = aStorage.ElementAt(reverseIndex);

      aStorage.ReplaceElementsAt(index, 1, t2);
      aStorage.ReplaceElementsAt(reverseIndex, 1, t1);
    }
  }

  static bool IsEmpty(const StorageType& aStorage) {
    return !!aStorage.IsEmpty();
  }

  static void Push(StorageType& aStorage, const T& aEntry) {
    aStorage.AppendElement(aEntry);
  }

  static bool Pop(StorageType& aStorage, T& aEntry) {
    if (IsEmpty(aStorage)) {
      return false;
    }

    aEntry = aStorage.PopLastElement();
    return true;
  }

  static void Clear(StorageType& aStorage) { aStorage.Clear(); }

  static void Compact(StorageType& aStorage) { aStorage.Compact(); }
};

class MOZ_CAPABILITY LockingWithMutex {
  mozilla::Mutex mMutex;

 protected:
  LockingWithMutex() : mMutex("LockingWithMutex::mMutex") {}

  void Lock() MOZ_CAPABILITY_ACQUIRE() { mMutex.Lock(); }

  void Unlock() MOZ_CAPABILITY_RELEASE() { mMutex.Unlock(); }

  class MOZ_SCOPED_CAPABILITY AutoLock {
    LockingWithMutex& mHost;

   public:
    explicit AutoLock(LockingWithMutex& aHost) MOZ_CAPABILITY_ACQUIRE(aHost)
        : mHost(aHost) {
      mHost.Lock();
    }

    ~AutoLock() MOZ_CAPABILITY_RELEASE() { mHost.Unlock(); }
  };

  friend class AutoLock;
};

class NoLocking {
 protected:
  void Lock() {}

  void Unlock() {}

  class AutoLock {
   public:
    explicit AutoLock(NoLocking& aHost) {}

    ~AutoLock() = default;
  };
};

template <typename T, int TCount = 256, class LockingPolicy = NoLocking,
          class StoragePolicy =
              StorageWithTArray<T, TCount % 2 ? TCount / 2 + 1 : TCount / 2> >
class Queue : public LockingPolicy {
  typedef typename StoragePolicy::StorageType StorageType;
  typedef typename LockingPolicy::AutoLock AutoLock;

  StorageType mStorage1;
  StorageType mStorage2;

  StorageType* mFront;
  StorageType* mBack;

 public:
  Queue() : mFront(&mStorage1), mBack(&mStorage2) {}

  bool IsEmpty() {
    AutoLock lock(*this);
    return StoragePolicy::IsEmpty(*mFront) && StoragePolicy::IsEmpty(*mBack);
  }

  void Push(const T& aEntry) {
    AutoLock lock(*this);
    StoragePolicy::Push(*mBack, aEntry);
  }

  bool Pop(T& aEntry) {
    AutoLock lock(*this);
    if (StoragePolicy::IsEmpty(*mFront)) {
      StoragePolicy::Compact(*mFront);
      StoragePolicy::Reverse(*mBack);
      StorageType* tmp = mFront;
      mFront = mBack;
      mBack = tmp;
    }
    return StoragePolicy::Pop(*mFront, aEntry);
  }

  void Clear() {
    AutoLock lock(*this);
    StoragePolicy::Clear(*mFront);
    StoragePolicy::Clear(*mBack);
  }

  // XXX Do we need this?
  void Lock() { LockingPolicy::Lock(); }

  // XXX Do we need this?
  void Unlock() { LockingPolicy::Unlock(); }

 private:
  // Queue is not copyable.
  Queue(const Queue&);
  Queue& operator=(const Queue&);
};

}  // namespace mozilla::dom::workerinternals

#endif /* mozilla_dom_workerinternals_Queue_h*/
