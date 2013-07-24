/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaObject.h"

#include "QuotaManager.h"

USING_QUOTA_NAMESPACE

void
QuotaObject::AddRef()
{
  QuotaManager* quotaManager = QuotaManager::Get();
  if (!quotaManager) {
    NS_ERROR("Null quota manager, this shouldn't happen, possible leak!");

    ++mRefCnt;

    return;
  }

  MutexAutoLock lock(quotaManager->mQuotaMutex);

  ++mRefCnt;
}

void
QuotaObject::Release()
{
  QuotaManager* quotaManager = QuotaManager::Get();
  if (!quotaManager) {
    NS_ERROR("Null quota manager, this shouldn't happen, possible leak!");

    nsrefcnt count = --mRefCnt;
    if (count == 0) {
      mRefCnt = 1;
      delete this;
    }

    return;
  }

  {
    MutexAutoLock lock(quotaManager->mQuotaMutex);

    --mRefCnt;

    if (mRefCnt > 0) {
      return;
    }

    if (mOriginInfo) {
      mOriginInfo->mQuotaObjects.Remove(mPath);
    }
  }

  delete this;
}

void
QuotaObject::UpdateSize(int64_t aSize)
{
  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "Shouldn't be null!");

  MutexAutoLock lock(quotaManager->mQuotaMutex);

  if (mOriginInfo) {
    mOriginInfo->mUsage -= mSize;
    mSize = aSize;
    mOriginInfo->mUsage += mSize;
  }
}

bool
QuotaObject::MaybeAllocateMoreSpace(int64_t aOffset, int32_t aCount)
{
  int64_t end = aOffset + aCount;

  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "Shouldn't be null!");

  MutexAutoLock lock(quotaManager->mQuotaMutex);

  if (mSize >= end || !mOriginInfo) {
    return true;
  }

  int64_t newUsage = mOriginInfo->mUsage - mSize + end;
  if (newUsage > mOriginInfo->mLimit) {
    // This will block the thread, but it will also drop the mutex while
    // waiting. The mutex will be reacquired again when the waiting is finished.
    if (!quotaManager->LockedQuotaIsLifted()) {
      return false;
    }

    // Threads raced, the origin info removal has been done by some other
    // thread.
    if (!mOriginInfo) {
      // The other thread could allocate more space.
      if (end > mSize) {
        mSize = end;
      }

      return true;
    }

    nsCString origin = mOriginInfo->mOrigin;

    mOriginInfo->LockedClearOriginInfos();
    NS_ASSERTION(!mOriginInfo,
                 "Should have cleared in LockedClearOriginInfos!");

    quotaManager->mOriginInfos.Remove(origin);

    // Some other thread could increase the size without blocking (increasing
    // the origin usage without hitting the limit), but no more than this one.
    NS_ASSERTION(mSize < end, "This shouldn't happen!");

    mSize = end;

    return true;
  }

  mOriginInfo->mUsage = newUsage;
  mSize = end;

  return true;
}

#ifdef DEBUG
void
OriginInfo::LockedClearOriginInfos()
{
  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "Shouldn't be null!");

  quotaManager->mQuotaMutex.AssertCurrentThreadOwns();

  mQuotaObjects.EnumerateRead(ClearOriginInfoCallback, nullptr);
}
#endif

// static
PLDHashOperator
OriginInfo::ClearOriginInfoCallback(const nsAString& aKey,
                                    QuotaObject* aValue,
                                    void* aUserArg)
{
  NS_ASSERTION(!aKey.IsEmpty(), "Empty key!");
  NS_ASSERTION(aValue, "Null pointer!");

  aValue->mOriginInfo = nullptr;

  return PL_DHASH_NEXT;
}
