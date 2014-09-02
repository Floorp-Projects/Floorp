/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RWLOCK_AUTO_ENTER_H
#define RWLOCK_AUTO_ENTER_H

#include "prrwlock.h"
#include "mozilla/Assertions.h"

class RwLockAutoEnterRead
{
public:
  explicit RwLockAutoEnterRead(PRRWLock* aRwLock)
    : mRwLock(aRwLock)
  {
    MOZ_ASSERT(mRwLock);
    PR_RWLock_Rlock(mRwLock);
  }

  ~RwLockAutoEnterRead()
  {
    PR_RWLock_Unlock(mRwLock);
  }

protected:
  PRRWLock* mRwLock;
};

class RwLockAutoEnterWrite
{
public:
  explicit RwLockAutoEnterWrite(PRRWLock* aRwLock)
    : mRwLock(aRwLock)
  {
    MOZ_ASSERT(mRwLock);
    PR_RWLock_Wlock(mRwLock);
  }

  ~RwLockAutoEnterWrite()
  {
    PR_RWLock_Unlock(mRwLock);
  }

protected:
  PRRWLock* mRwLock;
};

#endif // RWLOCK_AUTO_ENTER_H
