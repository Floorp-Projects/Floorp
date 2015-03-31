/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaObject.h"

#include "mozilla/TypeTraits.h"
#include "QuotaManager.h"
#include "Utilities.h"

#ifdef DEBUG
#include "nsComponentManagerUtils.h"
#include "nsIFile.h"
#include "nsXPCOMCID.h"
#endif

USING_QUOTA_NAMESPACE

namespace {

template <typename T, typename U>
void
AssertPositiveIntegers(T aOne, U aTwo)
{
  static_assert(mozilla::IsIntegral<T>::value, "Not an integer!");
  static_assert(mozilla::IsIntegral<U>::value, "Not an integer!");
  MOZ_ASSERT(aOne >= 0);
  MOZ_ASSERT(aTwo >= 0);
}

template <typename T, typename U>
void
AssertNoOverflow(T aOne, U aTwo)
{
  AssertPositiveIntegers(aOne, aTwo);
  AssertNoOverflow(uint64_t(aOne), uint64_t(aTwo));
}

template <>
void
AssertNoOverflow<uint64_t, uint64_t>(uint64_t aOne, uint64_t aTwo)
{
  MOZ_ASSERT(UINT64_MAX - aOne >= aTwo);
}

template <typename T, typename U>
void
AssertNoUnderflow(T aOne, U aTwo)
{
  AssertPositiveIntegers(aOne, aTwo);
  AssertNoUnderflow(uint64_t(aOne), uint64_t(aTwo));
}

template <>
void
AssertNoUnderflow<uint64_t, uint64_t>(uint64_t aOne, uint64_t aTwo)
{
  MOZ_ASSERT(aOne >= aTwo);
}

} // anonymous namespace

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
  MOZ_ASSERT(aSize >= 0);

#ifdef DEBUG
  {
    nsCOMPtr<nsIFile> file = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
    MOZ_ASSERT(file);

    MOZ_ASSERT(NS_SUCCEEDED(file->InitWithPath(mPath)));

    bool exists;
    MOZ_ASSERT(NS_SUCCEEDED(file->Exists(&exists)));

    if (exists) {
      int64_t fileSize;
      MOZ_ASSERT(NS_SUCCEEDED(file->GetFileSize(&fileSize)));

      MOZ_ASSERT(aSize == fileSize);
    } else {
      MOZ_ASSERT(!aSize);
    }
  }
#endif

  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "Shouldn't be null!");

  MutexAutoLock lock(quotaManager->mQuotaMutex);

  if (!mOriginInfo || mSize == aSize) {
    return;
  }

  AssertNoUnderflow(quotaManager->mTemporaryStorageUsage, mSize);
  quotaManager->mTemporaryStorageUsage -= mSize;

  GroupInfo* groupInfo = mOriginInfo->mGroupInfo;

  AssertNoUnderflow(groupInfo->mUsage, mSize);
  groupInfo->mUsage -= mSize;

  AssertNoUnderflow(mOriginInfo->mUsage, mSize);
  mOriginInfo->mUsage -= mSize;

  mSize = aSize;

  AssertNoOverflow(mOriginInfo->mUsage, mSize);
  mOriginInfo->mUsage += mSize;

  AssertNoOverflow(groupInfo->mUsage, mSize);
  groupInfo->mUsage += mSize;

  AssertNoOverflow(quotaManager->mTemporaryStorageUsage, mSize);
  quotaManager->mTemporaryStorageUsage += mSize;
}

bool
QuotaObject::MaybeAllocateMoreSpace(int64_t aOffset, int32_t aCount)
{
  AssertNoOverflow(aOffset, aCount);
  int64_t end = aOffset + aCount;

  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "Shouldn't be null!");

  MutexAutoLock lock(quotaManager->mQuotaMutex);

  if (mSize >= end || !mOriginInfo) {
    return true;
  }

  GroupInfo* groupInfo = mOriginInfo->mGroupInfo;

  nsRefPtr<GroupInfo> complementaryGroupInfo =
    groupInfo->mGroupInfoPair->LockedGetGroupInfo(
      ComplementaryPersistenceType(groupInfo->mPersistenceType));

  AssertNoUnderflow(end, mSize);
  uint64_t delta = end - mSize;

  AssertNoOverflow(mOriginInfo->mUsage, delta);
  uint64_t newUsage = mOriginInfo->mUsage + delta;

  // Temporary storage has no limit for origin usage (there's a group and the
  // global limit though).

  AssertNoOverflow(groupInfo->mUsage, delta);
  uint64_t newGroupUsage = groupInfo->mUsage + delta;

  uint64_t groupUsage = groupInfo->mUsage;
  if (complementaryGroupInfo) {
    AssertNoOverflow(groupUsage, complementaryGroupInfo->mUsage);
    groupUsage += complementaryGroupInfo->mUsage;
  }

  // Temporary storage has a hard limit for group usage (20 % of the global
  // limit).
  AssertNoOverflow(groupUsage, delta);
  if (groupUsage + delta > quotaManager->GetGroupLimit()) {
    return false;
  }

  AssertNoOverflow(quotaManager->mTemporaryStorageUsage, delta);
  uint64_t newTemporaryStorageUsage = quotaManager->mTemporaryStorageUsage +
                                      delta;

  if (newTemporaryStorageUsage > quotaManager->mTemporaryStorageLimit) {
    // This will block the thread without holding the lock while waitting.

    nsAutoTArray<OriginInfo*, 10> originInfos;
    uint64_t sizeToBeFreed =
      quotaManager->LockedCollectOriginsForEviction(delta, originInfos);

    if (!sizeToBeFreed) {
      return false;
    }

    NS_ASSERTION(sizeToBeFreed >= delta, "Huh?");

    {
      MutexAutoUnlock autoUnlock(quotaManager->mQuotaMutex);

      for (uint32_t i = 0; i < originInfos.Length(); i++) {
        OriginInfo* originInfo = originInfos[i];

        quotaManager->DeleteFilesForOrigin(
                                       originInfo->mGroupInfo->mPersistenceType,
                                       originInfo->mOrigin);
      }
    }

    // Relocked.

    NS_ASSERTION(mOriginInfo, "How come?!");

    nsTArray<OriginParams> origins;
    for (uint32_t i = 0; i < originInfos.Length(); i++) {
      OriginInfo* originInfo = originInfos[i];

      NS_ASSERTION(originInfo != mOriginInfo, "Deleted itself!");

      PersistenceType persistenceType =
        originInfo->mGroupInfo->mPersistenceType;
      nsCString group = originInfo->mGroupInfo->mGroup;
      nsCString origin = originInfo->mOrigin;
      bool isApp = originInfo->mIsApp;
      quotaManager->LockedRemoveQuotaForOrigin(persistenceType, group, origin);

#ifdef DEBUG
      originInfos[i] = nullptr;
#endif

      origins.AppendElement(OriginParams(persistenceType, origin, isApp));
    }

    // We unlocked and relocked several times so we need to recompute all the
    // essential variables and recheck the group limit.

    AssertNoUnderflow(end, mSize);
    delta = end - mSize;

    AssertNoOverflow(mOriginInfo->mUsage, delta);
    newUsage = mOriginInfo->mUsage + delta;

    AssertNoOverflow(groupInfo->mUsage, delta);
    newGroupUsage = groupInfo->mUsage + delta;

    groupUsage = groupInfo->mUsage;
    if (complementaryGroupInfo) {
      AssertNoOverflow(groupUsage, complementaryGroupInfo->mUsage);
      groupUsage += complementaryGroupInfo->mUsage;
    }

    AssertNoOverflow(groupUsage, delta);
    if (groupUsage + delta > quotaManager->GetGroupLimit()) {
      // Unfortunately some other thread increased the group usage in the
      // meantime and we are not below the group limit anymore.

      // However, the origin eviction must be finalized in this case too.
      MutexAutoUnlock autoUnlock(quotaManager->mQuotaMutex);

      quotaManager->FinalizeOriginEviction(origins);

      return false;
    }

    AssertNoOverflow(quotaManager->mTemporaryStorageUsage, delta);
    newTemporaryStorageUsage = quotaManager->mTemporaryStorageUsage + delta;

    NS_ASSERTION(newTemporaryStorageUsage <=
                 quotaManager->mTemporaryStorageLimit, "How come?!");

    // Ok, we successfully freed enough space and the operation can continue
    // without throwing the quota error.
    mOriginInfo->mUsage = newUsage;
    groupInfo->mUsage = newGroupUsage;
    quotaManager->mTemporaryStorageUsage = newTemporaryStorageUsage;;

    // Some other thread could increase the size in the meantime, but no more
    // than this one.
    MOZ_ASSERT(mSize < end);
    mSize = end;

    // Finally, release IO thread only objects and allow next synchronized
    // ops for the evicted origins.
    MutexAutoUnlock autoUnlock(quotaManager->mQuotaMutex);

    quotaManager->FinalizeOriginEviction(origins);

    return true;
  }

  mOriginInfo->mUsage = newUsage;
  groupInfo->mUsage = newGroupUsage;
  quotaManager->mTemporaryStorageUsage = newTemporaryStorageUsage;

  mSize = end;

  return true;
}

void
OriginInfo::LockedDecreaseUsage(int64_t aSize)
{
  AssertCurrentThreadOwnsQuotaMutex();

  AssertNoUnderflow(mUsage, aSize);
  mUsage -= aSize;

  AssertNoUnderflow(mGroupInfo->mUsage, aSize);
  mGroupInfo->mUsage -= aSize;

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  AssertNoUnderflow(quotaManager->mTemporaryStorageUsage, aSize);
  quotaManager->mTemporaryStorageUsage -= aSize;
}

already_AddRefed<OriginInfo>
GroupInfo::LockedGetOriginInfo(const nsACString& aOrigin)
{
  AssertCurrentThreadOwnsQuotaMutex();

  for (uint32_t index = 0; index < mOriginInfos.Length(); index++) {
    nsRefPtr<OriginInfo>& originInfo = mOriginInfos[index];

    if (originInfo->mOrigin == aOrigin) {
      nsRefPtr<OriginInfo> result = originInfo;
      return result.forget();
    }
  }

  return nullptr;
}

void
GroupInfo::LockedAddOriginInfo(OriginInfo* aOriginInfo)
{
  AssertCurrentThreadOwnsQuotaMutex();

  NS_ASSERTION(!mOriginInfos.Contains(aOriginInfo),
               "Replacing an existing entry!");
  mOriginInfos.AppendElement(aOriginInfo);

  AssertNoOverflow(mUsage, aOriginInfo->mUsage);
  mUsage += aOriginInfo->mUsage;

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  AssertNoOverflow(quotaManager->mTemporaryStorageUsage, aOriginInfo->mUsage);
  quotaManager->mTemporaryStorageUsage += aOriginInfo->mUsage;
}

void
GroupInfo::LockedRemoveOriginInfo(const nsACString& aOrigin)
{
  AssertCurrentThreadOwnsQuotaMutex();

  for (uint32_t index = 0; index < mOriginInfos.Length(); index++) {
    if (mOriginInfos[index]->mOrigin == aOrigin) {
      AssertNoUnderflow(mUsage, mOriginInfos[index]->mUsage);
      mUsage -= mOriginInfos[index]->mUsage;

      QuotaManager* quotaManager = QuotaManager::Get();
      MOZ_ASSERT(quotaManager);

      AssertNoUnderflow(quotaManager->mTemporaryStorageUsage,
                        mOriginInfos[index]->mUsage);
      quotaManager->mTemporaryStorageUsage -= mOriginInfos[index]->mUsage;

      mOriginInfos.RemoveElementAt(index);

      return;
    }
  }
}

void
GroupInfo::LockedRemoveOriginInfos()
{
  AssertCurrentThreadOwnsQuotaMutex();

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  for (uint32_t index = mOriginInfos.Length(); index > 0; index--) {
    OriginInfo* originInfo = mOriginInfos[index - 1];

    AssertNoUnderflow(mUsage, originInfo->mUsage);
    mUsage -= originInfo->mUsage;

    AssertNoUnderflow(quotaManager->mTemporaryStorageUsage, originInfo->mUsage);
    quotaManager->mTemporaryStorageUsage -= originInfo->mUsage;

    mOriginInfos.RemoveElementAt(index - 1);
  }
}

nsRefPtr<GroupInfo>&
GroupInfoPair::GetGroupInfoForPersistenceType(PersistenceType aPersistenceType)
{
  switch (aPersistenceType) {
    case PERSISTENCE_TYPE_TEMPORARY:
      return mTemporaryStorageGroupInfo;
    case PERSISTENCE_TYPE_DEFAULT:
      return mDefaultStorageGroupInfo;

    case PERSISTENCE_TYPE_PERSISTENT:
    case PERSISTENCE_TYPE_INVALID:
    default:
      MOZ_CRASH("Bad persistence type value!");
  }
}
