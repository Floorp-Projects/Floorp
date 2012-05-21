/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_queue_h__
#define mozilla_dom_workers_queue_h__

#include "Workers.h"

#include "mozilla/Mutex.h"
#include "nsTArray.h"

BEGIN_WORKERS_NAMESPACE

template <typename T, int TCount>
struct StorageWithTArray
{
  typedef nsAutoTArray<T, TCount> StorageType;

  static void Reverse(StorageType& aStorage)
  {
    PRUint32 length = aStorage.Length();
    for (PRUint32 index = 0; index < length / 2; index++) {
      PRUint32 reverseIndex = length - 1 - index;

      T t1 = aStorage.ElementAt(index);
      T t2 = aStorage.ElementAt(reverseIndex);

      aStorage.ReplaceElementsAt(index, 1, t2);
      aStorage.ReplaceElementsAt(reverseIndex, 1, t1);
    }
  }

  static bool IsEmpty(const StorageType& aStorage)
  {
    return !!aStorage.IsEmpty();
  }

  static bool Push(StorageType& aStorage, const T& aEntry)
  {
    return !!aStorage.AppendElement(aEntry);
  }

  static bool Pop(StorageType& aStorage, T& aEntry)
  {
    if (IsEmpty(aStorage)) {
      return false;
    }

    PRUint32 index = aStorage.Length() - 1;
    aEntry = aStorage.ElementAt(index);
    aStorage.RemoveElementAt(index);
    return true;
  }

  static void Clear(StorageType& aStorage)
  {
    aStorage.Clear();
  }

  static void Compact(StorageType& aStorage)
  {
    aStorage.Compact();
  }
};

class LockingWithMutex
{
  mozilla::Mutex mMutex;

protected:
  LockingWithMutex()
  : mMutex("LockingWithMutex::mMutex")
  { }

  void Lock()
  {
    mMutex.Lock();
  }

  void Unlock()
  {
    mMutex.Unlock();
  }

  class AutoLock
  {
    LockingWithMutex& mHost;

  public:
    AutoLock(LockingWithMutex& aHost)
    : mHost(aHost)
    {
      mHost.Lock();
    }

    ~AutoLock()
    {
      mHost.Unlock();
    }
  };

  friend class AutoLock;
};

class NoLocking
{
protected:
  void Lock()
  { }

  void Unlock()
  { }

  class AutoLock
  {
  public:
    AutoLock(NoLocking& aHost)
    { }

    ~AutoLock()
    { }
  };
};

template <typename T,
          int TCount = 256,
          class LockingPolicy = NoLocking,
          class StoragePolicy = StorageWithTArray<T, TCount % 2 ?
                                                     TCount / 2 + 1 :
                                                     TCount / 2> >
class Queue : public LockingPolicy
{
  typedef typename StoragePolicy::StorageType StorageType;
  typedef typename LockingPolicy::AutoLock AutoLock;

  StorageType mStorage1;
  StorageType mStorage2;

  StorageType* mFront;
  StorageType* mBack;

public:
  Queue()
  : mFront(&mStorage1), mBack(&mStorage2)
  { }

  bool IsEmpty()
  {
    AutoLock lock(*this);
    return StoragePolicy::IsEmpty(*mFront) &&
           StoragePolicy::IsEmpty(*mBack);
  }

  bool Push(const T& aEntry)
  {
    AutoLock lock(*this);
    return StoragePolicy::Push(*mBack, aEntry);
  }

  bool Pop(T& aEntry)
  {
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

  void Clear()
  {
    AutoLock lock(*this);
    StoragePolicy::Clear(*mFront);
    StoragePolicy::Clear(*mBack);
  }

  // XXX Do we need this?
  void Lock()
  {
    LockingPolicy::Lock();
  }

  // XXX Do we need this?
  void Unlock()
  {
    LockingPolicy::Unlock();
  }

private:
  // Queue is not copyable.
  Queue(const Queue&);
  Queue & operator=(const Queue&);
};

END_WORKERS_NAMESPACE

#endif /* mozilla_dom_workers_queue_h__ */
