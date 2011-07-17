/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Web Workers.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
