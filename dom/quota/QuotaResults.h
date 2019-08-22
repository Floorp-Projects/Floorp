/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_QuotaResults_h
#define mozilla_dom_quota_QuotaResults_h

#include "nsIQuotaResults.h"

namespace mozilla {
namespace dom {
namespace quota {

class UsageResult : public nsIQuotaUsageResult {
  nsCString mOrigin;
  uint64_t mUsage;
  bool mPersisted;
  uint64_t mLastAccessed;

 public:
  UsageResult(const nsACString& aOrigin, bool aPersisted, uint64_t aUsage,
              uint64_t aLastAccessed);

 private:
  virtual ~UsageResult() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIQUOTAUSAGERESULT
};

class OriginUsageResult : public nsIQuotaOriginUsageResult {
  uint64_t mUsage;
  uint64_t mFileUsage;

 public:
  OriginUsageResult(uint64_t aUsage, uint64_t aFileUsage);

 private:
  virtual ~OriginUsageResult() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIQUOTAORIGINUSAGERESULT
};

class EstimateResult : public nsIQuotaEstimateResult {
  uint64_t mUsage;
  uint64_t mLimit;

 public:
  EstimateResult(uint64_t aUsage, uint64_t aLimit);

 private:
  virtual ~EstimateResult() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIQUOTAESTIMATERESULT
};

class InitializedOriginsResult : public nsIQuotaInitializedOriginsResult {
  nsCString mOrigin;

 public:
  explicit InitializedOriginsResult(const nsACString& aOrigin);

 private:
  virtual ~InitializedOriginsResult() = default;

  NS_DECL_ISUPPORTS
  NS_DECL_NSIQUOTAINITIALIZEDORIGINSRESULT
};

}  // namespace quota
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_quota_QuotaResults_h
