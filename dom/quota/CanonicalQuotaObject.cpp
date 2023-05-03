/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanonicalQuotaObject.h"

#include "GroupInfo.h"
#include "GroupInfoPair.h"
#include "mozilla/dom/StorageActivityService.h"
#include "mozilla/dom/quota/AssertionsImpl.h"
#include "mozilla/dom/quota/DirectoryLock.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "OriginInfo.h"

namespace mozilla::dom::quota {

NS_IMETHODIMP_(MozExternalRefCountType) CanonicalQuotaObject::AddRef() {
  QuotaManager* quotaManager = QuotaManager::Get();
  if (!quotaManager) {
    NS_ERROR("Null quota manager, this shouldn't happen, possible leak!");

    return ++mRefCnt;
  }

  MutexAutoLock lock(quotaManager->mQuotaMutex);

  return ++mRefCnt;
}

NS_IMETHODIMP_(MozExternalRefCountType) CanonicalQuotaObject::Release() {
  QuotaManager* quotaManager = QuotaManager::Get();
  if (!quotaManager) {
    NS_ERROR("Null quota manager, this shouldn't happen, possible leak!");

    nsrefcnt count = --mRefCnt;
    if (count == 0) {
      mRefCnt = 1;
      delete this;
      return 0;
    }

    return mRefCnt;
  }

  {
    MutexAutoLock lock(quotaManager->mQuotaMutex);

    --mRefCnt;

    if (mRefCnt > 0) {
      return mRefCnt;
    }

    if (mOriginInfo) {
      mOriginInfo->mCanonicalQuotaObjects.Remove(mPath);
    }
  }

  delete this;
  return 0;
}

bool CanonicalQuotaObject::MaybeUpdateSize(int64_t aSize, bool aTruncate) {
  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  MutexAutoLock lock(quotaManager->mQuotaMutex);

  return LockedMaybeUpdateSize(aSize, aTruncate);
}

bool CanonicalQuotaObject::IncreaseSize(int64_t aDelta) {
  MOZ_ASSERT(aDelta >= 0);

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  MutexAutoLock lock(quotaManager->mQuotaMutex);

  AssertNoOverflow(mSize, aDelta);
  int64_t size = mSize + aDelta;

  return LockedMaybeUpdateSize(size, /* aTruncate */ false);
}

void CanonicalQuotaObject::DisableQuotaCheck() {
  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  MutexAutoLock lock(quotaManager->mQuotaMutex);

  mQuotaCheckDisabled = true;
}

void CanonicalQuotaObject::EnableQuotaCheck() {
  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  MutexAutoLock lock(quotaManager->mQuotaMutex);

  mQuotaCheckDisabled = false;
}

bool CanonicalQuotaObject::LockedMaybeUpdateSize(int64_t aSize,
                                                 bool aTruncate) {
  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  quotaManager->mQuotaMutex.AssertCurrentThreadOwns();

  if (mWritingDone == false && mOriginInfo) {
    mWritingDone = true;
    StorageActivityService::SendActivity(mOriginInfo->mOrigin);
  }

  if (mQuotaCheckDisabled) {
    return true;
  }

  if (mSize == aSize) {
    return true;
  }

  if (!mOriginInfo) {
    mSize = aSize;
    return true;
  }

  GroupInfo* groupInfo = mOriginInfo->mGroupInfo;
  MOZ_ASSERT(groupInfo);

  if (mSize > aSize) {
    if (aTruncate) {
      const int64_t delta = mSize - aSize;

      AssertNoUnderflow(quotaManager->mTemporaryStorageUsage, delta);
      quotaManager->mTemporaryStorageUsage -= delta;

      if (!mOriginInfo->LockedPersisted()) {
        AssertNoUnderflow(groupInfo->mUsage, delta);
        groupInfo->mUsage -= delta;
      }

      AssertNoUnderflow(mOriginInfo->mUsage, delta);
      mOriginInfo->mUsage -= delta;

      MOZ_ASSERT(mOriginInfo->mClientUsages[mClientType].isSome());
      AssertNoUnderflow(mOriginInfo->mClientUsages[mClientType].value(), delta);
      mOriginInfo->mClientUsages[mClientType] =
          Some(mOriginInfo->mClientUsages[mClientType].value() - delta);

      mSize = aSize;
    }
    return true;
  }

  MOZ_ASSERT(mSize < aSize);

  const auto& complementaryPersistenceTypes =
      ComplementaryPersistenceTypes(groupInfo->mPersistenceType);

  uint64_t delta = aSize - mSize;

  AssertNoOverflow(mOriginInfo->mClientUsages[mClientType].valueOr(0), delta);
  uint64_t newClientUsage =
      mOriginInfo->mClientUsages[mClientType].valueOr(0) + delta;

  AssertNoOverflow(mOriginInfo->mUsage, delta);
  uint64_t newUsage = mOriginInfo->mUsage + delta;

  // Temporary storage has no limit for origin usage (there's a group and the
  // global limit though).

  uint64_t newGroupUsage = groupInfo->mUsage;
  if (!mOriginInfo->LockedPersisted()) {
    AssertNoOverflow(groupInfo->mUsage, delta);
    newGroupUsage += delta;

    uint64_t groupUsage = groupInfo->mUsage;
    for (const auto& complementaryPersistenceType :
         complementaryPersistenceTypes) {
      const auto& complementaryGroupInfo =
          groupInfo->mGroupInfoPair->LockedGetGroupInfo(
              complementaryPersistenceType);

      if (complementaryGroupInfo) {
        AssertNoOverflow(groupUsage, complementaryGroupInfo->mUsage);
        groupUsage += complementaryGroupInfo->mUsage;
      }
    }

    // Temporary storage has a hard limit for group usage (20 % of the global
    // limit).
    AssertNoOverflow(groupUsage, delta);
    if (groupUsage + delta > quotaManager->GetGroupLimit()) {
      return false;
    }
  }

  AssertNoOverflow(quotaManager->mTemporaryStorageUsage, delta);
  uint64_t newTemporaryStorageUsage =
      quotaManager->mTemporaryStorageUsage + delta;

  if (newTemporaryStorageUsage > quotaManager->mTemporaryStorageLimit) {
    // This will block the thread without holding the lock while waitting.

    AutoTArray<RefPtr<OriginDirectoryLock>, 10> locks;
    uint64_t sizeToBeFreed;

    if (::mozilla::ipc::IsOnBackgroundThread()) {
      MutexAutoUnlock autoUnlock(quotaManager->mQuotaMutex);

      sizeToBeFreed = quotaManager->CollectOriginsForEviction(delta, locks);
    } else {
      sizeToBeFreed =
          quotaManager->LockedCollectOriginsForEviction(delta, locks);
    }

    if (!sizeToBeFreed) {
      uint64_t usage = quotaManager->mTemporaryStorageUsage;

      MutexAutoUnlock autoUnlock(quotaManager->mQuotaMutex);

      quotaManager->NotifyStoragePressure(usage);

      return false;
    }

    NS_ASSERTION(sizeToBeFreed >= delta, "Huh?");

    {
      MutexAutoUnlock autoUnlock(quotaManager->mQuotaMutex);

      for (const auto& lock : locks) {
        quotaManager->DeleteOriginDirectory(lock->OriginMetadata());
      }
    }

    // Relocked.

    NS_ASSERTION(mOriginInfo, "How come?!");

    for (const auto& lock : locks) {
      MOZ_ASSERT(!(lock->GetPersistenceType() == groupInfo->mPersistenceType &&
                   lock->Origin() == mOriginInfo->mOrigin),
                 "Deleted itself!");

      quotaManager->LockedRemoveQuotaForOrigin(lock->OriginMetadata());
    }

    // We unlocked and relocked several times so we need to recompute all the
    // essential variables and recheck the group limit.

    AssertNoUnderflow(aSize, mSize);
    delta = aSize - mSize;

    AssertNoOverflow(mOriginInfo->mClientUsages[mClientType].valueOr(0), delta);
    newClientUsage = mOriginInfo->mClientUsages[mClientType].valueOr(0) + delta;

    AssertNoOverflow(mOriginInfo->mUsage, delta);
    newUsage = mOriginInfo->mUsage + delta;

    newGroupUsage = groupInfo->mUsage;
    if (!mOriginInfo->LockedPersisted()) {
      AssertNoOverflow(groupInfo->mUsage, delta);
      newGroupUsage += delta;

      uint64_t groupUsage = groupInfo->mUsage;

      for (const auto& complementaryPersistenceType :
           complementaryPersistenceTypes) {
        const auto& complementaryGroupInfo =
            groupInfo->mGroupInfoPair->LockedGetGroupInfo(
                complementaryPersistenceType);

        if (complementaryGroupInfo) {
          AssertNoOverflow(groupUsage, complementaryGroupInfo->mUsage);
          groupUsage += complementaryGroupInfo->mUsage;
        }
      }

      AssertNoOverflow(groupUsage, delta);
      if (groupUsage + delta > quotaManager->GetGroupLimit()) {
        // Unfortunately some other thread increased the group usage in the
        // meantime and we are not below the group limit anymore.

        // However, the origin eviction must be finalized in this case too.
        MutexAutoUnlock autoUnlock(quotaManager->mQuotaMutex);

        quotaManager->FinalizeOriginEviction(std::move(locks));

        return false;
      }
    }

    AssertNoOverflow(quotaManager->mTemporaryStorageUsage, delta);
    newTemporaryStorageUsage = quotaManager->mTemporaryStorageUsage + delta;

    NS_ASSERTION(
        newTemporaryStorageUsage <= quotaManager->mTemporaryStorageLimit,
        "How come?!");

    // Ok, we successfully freed enough space and the operation can continue
    // without throwing the quota error.
    mOriginInfo->mClientUsages[mClientType] = Some(newClientUsage);

    mOriginInfo->mUsage = newUsage;
    if (!mOriginInfo->LockedPersisted()) {
      groupInfo->mUsage = newGroupUsage;
    }
    quotaManager->mTemporaryStorageUsage = newTemporaryStorageUsage;
    ;

    // Some other thread could increase the size in the meantime, but no more
    // than this one.
    MOZ_ASSERT(mSize < aSize);
    mSize = aSize;

    // Finally, release IO thread only objects and allow next synchronized
    // ops for the evicted origins.
    MutexAutoUnlock autoUnlock(quotaManager->mQuotaMutex);

    quotaManager->FinalizeOriginEviction(std::move(locks));

    return true;
  }

  mOriginInfo->mClientUsages[mClientType] = Some(newClientUsage);

  mOriginInfo->mUsage = newUsage;
  if (!mOriginInfo->LockedPersisted()) {
    groupInfo->mUsage = newGroupUsage;
  }
  quotaManager->mTemporaryStorageUsage = newTemporaryStorageUsage;

  mSize = aSize;

  return true;
}

}  // namespace mozilla::dom::quota
