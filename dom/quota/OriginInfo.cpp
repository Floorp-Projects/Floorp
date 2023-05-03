/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OriginInfo.h"

#include "GroupInfo.h"
#include "GroupInfoPair.h"
#include "mozilla/dom/quota/AssertionsImpl.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/dom/quota/UsageInfo.h"

namespace mozilla::dom::quota {

OriginInfo::OriginInfo(GroupInfo* aGroupInfo, const nsACString& aOrigin,
                       const nsACString& aStorageOrigin, bool aIsPrivate,
                       const ClientUsageArray& aClientUsages, uint64_t aUsage,
                       int64_t aAccessTime, bool aPersisted,
                       bool aDirectoryExists)
    : mClientUsages(aClientUsages.Clone()),
      mGroupInfo(aGroupInfo),
      mOrigin(aOrigin),
      mStorageOrigin(aStorageOrigin),
      mUsage(aUsage),
      mAccessTime(aAccessTime),
      mIsPrivate(aIsPrivate),
      mAccessed(false),
      mPersisted(aPersisted),
      mDirectoryExists(aDirectoryExists) {
  MOZ_ASSERT(aGroupInfo);
  MOZ_ASSERT_IF(!aIsPrivate, aOrigin == aStorageOrigin);
  MOZ_ASSERT_IF(aIsPrivate, aOrigin != aStorageOrigin);
  MOZ_ASSERT(aClientUsages.Length() == Client::TypeMax());
  MOZ_ASSERT_IF(aPersisted,
                aGroupInfo->mPersistenceType == PERSISTENCE_TYPE_DEFAULT);

  // This constructor is called from the "QuotaManager IO" thread and so
  // we can't check if the principal has a WebExtensionPolicy instance
  // associated to it, and even besides that if the extension is currently
  // disabled (and so no WebExtensionPolicy instance would actually exist)
  // its stored data shouldn't be cleared until the extension is uninstalled
  // and so here we resort to check the origin scheme instead.
  mIsExtension = StringBeginsWith(mOrigin, "moz-extension://"_ns);

#ifdef DEBUG
  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  uint64_t usage = 0;
  for (Client::Type type : quotaManager->AllClientTypes()) {
    AssertNoOverflow(usage, aClientUsages[type].valueOr(0));
    usage += aClientUsages[type].valueOr(0);
  }
  MOZ_ASSERT(aUsage == usage);
#endif

  MOZ_COUNT_CTOR(OriginInfo);
}

OriginMetadata OriginInfo::FlattenToOriginMetadata() const {
  return {mGroupInfo->mGroupInfoPair->Suffix(),
          mGroupInfo->mGroupInfoPair->Group(),
          mOrigin,
          mStorageOrigin,
          mIsPrivate,
          mGroupInfo->mPersistenceType};
}

FullOriginMetadata OriginInfo::LockedFlattenToFullOriginMetadata() const {
  AssertCurrentThreadOwnsQuotaMutex();

  return {FlattenToOriginMetadata(), mPersisted, mAccessTime};
}

nsresult OriginInfo::LockedBindToStatement(
    mozIStorageStatement* aStatement) const {
  AssertCurrentThreadOwnsQuotaMutex();
  MOZ_ASSERT(mGroupInfo);

  QM_TRY(MOZ_TO_RESULT(aStatement->BindInt32ByName(
      "repository_id"_ns, mGroupInfo->mPersistenceType)));

  QM_TRY(MOZ_TO_RESULT(aStatement->BindUTF8StringByName(
      "suffix"_ns, mGroupInfo->mGroupInfoPair->Suffix())));
  QM_TRY(MOZ_TO_RESULT(aStatement->BindUTF8StringByName(
      "group_"_ns, mGroupInfo->mGroupInfoPair->Group())));
  QM_TRY(MOZ_TO_RESULT(aStatement->BindUTF8StringByName("origin"_ns, mOrigin)));

  MOZ_ASSERT(!mIsPrivate);

  nsCString clientUsagesText;
  mClientUsages.Serialize(clientUsagesText);

  QM_TRY(MOZ_TO_RESULT(
      aStatement->BindUTF8StringByName("client_usages"_ns, clientUsagesText)));
  QM_TRY(MOZ_TO_RESULT(aStatement->BindInt64ByName("usage"_ns, mUsage)));
  QM_TRY(MOZ_TO_RESULT(
      aStatement->BindInt64ByName("last_access_time"_ns, mAccessTime)));
  QM_TRY(MOZ_TO_RESULT(aStatement->BindInt32ByName("accessed"_ns, mAccessed)));
  QM_TRY(
      MOZ_TO_RESULT(aStatement->BindInt32ByName("persisted"_ns, mPersisted)));

  return NS_OK;
}

void OriginInfo::LockedDecreaseUsage(Client::Type aClientType, int64_t aSize) {
  AssertCurrentThreadOwnsQuotaMutex();

  MOZ_ASSERT(mClientUsages[aClientType].isSome());
  AssertNoUnderflow(mClientUsages[aClientType].value(), aSize);
  mClientUsages[aClientType] = Some(mClientUsages[aClientType].value() - aSize);

  AssertNoUnderflow(mUsage, aSize);
  mUsage -= aSize;

  if (!LockedPersisted()) {
    AssertNoUnderflow(mGroupInfo->mUsage, aSize);
    mGroupInfo->mUsage -= aSize;
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  AssertNoUnderflow(quotaManager->mTemporaryStorageUsage, aSize);
  quotaManager->mTemporaryStorageUsage -= aSize;
}

void OriginInfo::LockedResetUsageForClient(Client::Type aClientType) {
  AssertCurrentThreadOwnsQuotaMutex();

  uint64_t size = mClientUsages[aClientType].valueOr(0);

  mClientUsages[aClientType].reset();

  AssertNoUnderflow(mUsage, size);
  mUsage -= size;

  if (!LockedPersisted()) {
    AssertNoUnderflow(mGroupInfo->mUsage, size);
    mGroupInfo->mUsage -= size;
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  AssertNoUnderflow(quotaManager->mTemporaryStorageUsage, size);
  quotaManager->mTemporaryStorageUsage -= size;
}

UsageInfo OriginInfo::LockedGetUsageForClient(Client::Type aClientType) {
  AssertCurrentThreadOwnsQuotaMutex();

  // The current implementation of this method only supports DOMCACHE and LS,
  // which only use DatabaseUsage. If this assertion is lifted, the logic below
  // must be adapted.
  MOZ_ASSERT(aClientType == Client::Type::DOMCACHE ||
             aClientType == Client::Type::LS ||
             aClientType == Client::Type::FILESYSTEM);

  return UsageInfo{DatabaseUsageType{mClientUsages[aClientType]}};
}

void OriginInfo::LockedPersist() {
  AssertCurrentThreadOwnsQuotaMutex();
  MOZ_ASSERT(mGroupInfo->mPersistenceType == PERSISTENCE_TYPE_DEFAULT);
  MOZ_ASSERT(!mPersisted);

  mPersisted = true;

  // Remove Usage from GroupInfo
  AssertNoUnderflow(mGroupInfo->mUsage, mUsage);
  mGroupInfo->mUsage -= mUsage;
}

}  // namespace mozilla::dom::quota
