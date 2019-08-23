/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaResults.h"

namespace mozilla {
namespace dom {
namespace quota {

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

OriginUsageResult::OriginUsageResult(uint64_t aUsage, uint64_t aFileUsage)
    : mUsage(aUsage), mFileUsage(aFileUsage) {}

NS_IMPL_ISUPPORTS(OriginUsageResult, nsIQuotaOriginUsageResult)

NS_IMETHODIMP
OriginUsageResult::GetUsage(uint64_t* aUsage) {
  MOZ_ASSERT(aUsage);

  *aUsage = mUsage;
  return NS_OK;
}

NS_IMETHODIMP
OriginUsageResult::GetFileUsage(uint64_t* aFileUsage) {
  MOZ_ASSERT(aFileUsage);

  *aFileUsage = mFileUsage;
  return NS_OK;
}

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

InitializedOriginsResult::InitializedOriginsResult(const nsACString& aOrigin)
    : mOrigin(aOrigin) {}

NS_IMPL_ISUPPORTS(InitializedOriginsResult, nsIQuotaInitializedOriginsResult)

NS_IMETHODIMP
InitializedOriginsResult::GetOrigin(nsACString& aOrigin) {
  aOrigin = mOrigin;
  return NS_OK;
}

}  // namespace quota
}  // namespace dom
}  // namespace mozilla
