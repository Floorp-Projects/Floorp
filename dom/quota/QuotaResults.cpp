/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaResults.h"

#include "ErrorList.h"
#include "mozilla/Assertions.h"
#include "mozilla/MacroForEach.h"
#include "nscore.h"

namespace mozilla::dom::quota {

FullOriginMetadataResult::FullOriginMetadataResult(
    const FullOriginMetadata& aFullOriginMetadata)
    : mFullOriginMetadata(aFullOriginMetadata) {}

NS_IMPL_ISUPPORTS(FullOriginMetadataResult, nsIQuotaFullOriginMetadataResult)

NS_IMETHODIMP
FullOriginMetadataResult::GetSuffix(nsACString& aSuffix) {
  aSuffix = mFullOriginMetadata.mSuffix;
  return NS_OK;
}

NS_IMETHODIMP
FullOriginMetadataResult::GetGroup(nsACString& aGroup) {
  aGroup = mFullOriginMetadata.mGroup;
  return NS_OK;
}

NS_IMETHODIMP
FullOriginMetadataResult::GetOrigin(nsACString& aOrigin) {
  aOrigin = mFullOriginMetadata.mOrigin;
  return NS_OK;
}

NS_IMETHODIMP
FullOriginMetadataResult::GetStorageOrigin(nsACString& aStorageOrigin) {
  aStorageOrigin = mFullOriginMetadata.mStorageOrigin;
  return NS_OK;
}

NS_IMETHODIMP
FullOriginMetadataResult::GetPersistenceType(nsACString& aPersistenceType) {
  aPersistenceType =
      PersistenceTypeToString(mFullOriginMetadata.mPersistenceType);
  return NS_OK;
}

NS_IMETHODIMP
FullOriginMetadataResult::GetPersisted(bool* aPersisted) {
  MOZ_ASSERT(aPersisted);

  *aPersisted = mFullOriginMetadata.mPersisted;
  return NS_OK;
}

NS_IMETHODIMP
FullOriginMetadataResult::GetLastAccessTime(int64_t* aLastAccessTime) {
  MOZ_ASSERT(aLastAccessTime);

  *aLastAccessTime = mFullOriginMetadata.mLastAccessTime;
  return NS_OK;
}

UsageResult::UsageResult(const nsACString& aOrigin, bool aPersisted,
                         uint64_t aUsage, uint64_t aLastAccessed)
    : mOrigin(aOrigin),
      mUsage(aUsage),
      mPersisted(aPersisted),
      mLastAccessed(aLastAccessed) {}

NS_IMPL_ISUPPORTS(UsageResult, nsIQuotaUsageResult)

NS_IMETHODIMP
UsageResult::GetOrigin(nsACString& aOrigin) {
  aOrigin = mOrigin;
  return NS_OK;
}

NS_IMETHODIMP
UsageResult::GetPersisted(bool* aPersisted) {
  MOZ_ASSERT(aPersisted);

  *aPersisted = mPersisted;
  return NS_OK;
}

NS_IMETHODIMP
UsageResult::GetUsage(uint64_t* aUsage) {
  MOZ_ASSERT(aUsage);

  *aUsage = mUsage;
  return NS_OK;
}

NS_IMETHODIMP
UsageResult::GetLastAccessed(uint64_t* aLastAccessed) {
  MOZ_ASSERT(aLastAccessed);

  *aLastAccessed = mLastAccessed;
  return NS_OK;
}

OriginUsageResult::OriginUsageResult(UsageInfo aUsageInfo)
    : mUsageInfo(aUsageInfo) {}

NS_IMPL_ISUPPORTS(OriginUsageResult, nsIQuotaOriginUsageResult)

NS_IMETHODIMP
OriginUsageResult::GetDatabaseUsage(uint64_t* aDatabaseUsage) {
  MOZ_ASSERT(aDatabaseUsage);

  *aDatabaseUsage = mUsageInfo.DatabaseUsage().valueOr(0);
  return NS_OK;
}

NS_IMETHODIMP
OriginUsageResult::GetFileUsage(uint64_t* aFileUsage) {
  MOZ_ASSERT(aFileUsage);

  *aFileUsage = mUsageInfo.FileUsage().valueOr(0);
  return NS_OK;
}

NS_IMETHODIMP
OriginUsageResult::GetUsage(uint64_t* aUsage) {
  MOZ_ASSERT(aUsage);

  *aUsage = mUsageInfo.TotalUsage().valueOr(0);
  return NS_OK;
}

NS_IMETHODIMP
OriginUsageResult::GetDatabaseUsageIsExplicit(bool* aDatabaseUsageIsExplicit) {
  MOZ_ASSERT(aDatabaseUsageIsExplicit);

  *aDatabaseUsageIsExplicit = mUsageInfo.DatabaseUsage().isSome();
  return NS_OK;
}

NS_IMETHODIMP
OriginUsageResult::GetFileUsageIsExplicit(bool* aFileUsageIsExplicit) {
  MOZ_ASSERT(aFileUsageIsExplicit);

  *aFileUsageIsExplicit = mUsageInfo.FileUsage().isSome();
  return NS_OK;
}

NS_IMETHODIMP
OriginUsageResult::GetTotalUsageIsExplicit(bool* aTotalUsageIsExplicit) {
  MOZ_ASSERT(aTotalUsageIsExplicit);

  *aTotalUsageIsExplicit = mUsageInfo.TotalUsage().isSome();
  return NS_OK;
}

UsageInfo& OriginUsageResult::GetUsageInfo() { return mUsageInfo; }

EstimateResult::EstimateResult(uint64_t aUsage, uint64_t aLimit)
    : mUsage(aUsage), mLimit(aLimit) {}

NS_IMPL_ISUPPORTS(EstimateResult, nsIQuotaEstimateResult)

NS_IMETHODIMP
EstimateResult::GetUsage(uint64_t* aUsage) {
  MOZ_ASSERT(aUsage);

  *aUsage = mUsage;
  return NS_OK;
}

NS_IMETHODIMP
EstimateResult::GetLimit(uint64_t* aLimit) {
  MOZ_ASSERT(aLimit);

  *aLimit = mLimit;
  return NS_OK;
}

}  // namespace mozilla::dom::quota
