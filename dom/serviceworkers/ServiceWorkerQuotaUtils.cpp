/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MainThreadUtils.h"
#include "ServiceWorkerQuotaUtils.h"

#include "mozilla/ScopeExit.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/dom/quota/QuotaManagerService.h"
#include "nsIClearDataService.h"
#include "nsID.h"
#include "nsIPrincipal.h"
#include "nsIQuotaCallbacks.h"
#include "nsIQuotaRequests.h"
#include "nsIQuotaResults.h"
#include "nsISupports.h"
#include "nsIVariant.h"
#include "nsServiceManagerUtils.h"

using mozilla::dom::quota::QuotaManagerService;

namespace mozilla::dom {

/*
 * QuotaUsageChecker implements the quota usage checking algorithm.
 *
 * 1. Getting the given origin/group usage through QuotaManagerService.
 *    QuotaUsageCheck::Start() implements this step.
 * 2. Checking if the group usage headroom is satisfied.
 *    It could be following three situations.
 *    a. Group headroom is satisfied without any usage mitigation.
 *    b. Group headroom is satisfied after origin usage mitigation.
 *       This invokes nsIClearDataService::DeleteDataFromPrincipal().
 *    c. Group headroom is satisfied after group usage mitigation.
 *       This invokes nsIClearDataService::DeleteDataFromBaseDomain().
 *    QuotaUsageChecker::CheckQuotaHeadroom() implements this step.
 *
 * If the algorithm is done or error out, the QuotaUsageCheck::mCallback will
 * be called with a bool result for external handling.
 */
class QuotaUsageChecker final : public nsIQuotaCallback,
                                public nsIQuotaUsageCallback,
                                public nsIClearDataCallback {
 public:
  NS_DECL_ISUPPORTS
  // For QuotaManagerService::Estimate()
  NS_DECL_NSIQUOTACALLBACK

  // For QuotaManagerService::GetUsageForPrincipal()
  NS_DECL_NSIQUOTAUSAGECALLBACK

  // For nsIClearDataService::DeleteDataFromPrincipal() and
  // nsIClearDataService::DeleteDataFromBaseDomain()
  NS_DECL_NSICLEARDATACALLBACK

  QuotaUsageChecker(nsIPrincipal* aPrincipal,
                    ServiceWorkerQuotaMitigationCallback&& aCallback);

  void Start();

  void RunCallback(bool aResult);

 private:
  ~QuotaUsageChecker() = default;

  // This is a general help method to get nsIQuotaResult/nsIQuotaUsageResult
  // from nsIQuotaRequest/nsIQuotaUsageRequest
  template <typename T, typename U>
  nsresult GetResult(T* aRequest, U&);

  void CheckQuotaHeadroom();

  nsCOMPtr<nsIPrincipal> mPrincipal;

  // The external callback. Calling RunCallback(bool) instead of calling it
  // directly, RunCallback(bool) handles the internal status.
  ServiceWorkerQuotaMitigationCallback mCallback;
  bool mGettingOriginUsageDone;
  bool mGettingGroupUsageDone;
  bool mIsChecking;
  uint64_t mOriginUsage;
  uint64_t mGroupUsage;
  uint64_t mGroupLimit;
};

NS_IMPL_ISUPPORTS(QuotaUsageChecker, nsIQuotaCallback, nsIQuotaUsageCallback,
                  nsIClearDataCallback)

QuotaUsageChecker::QuotaUsageChecker(
    nsIPrincipal* aPrincipal, ServiceWorkerQuotaMitigationCallback&& aCallback)
    : mPrincipal(aPrincipal),
      mCallback(std::move(aCallback)),
      mGettingOriginUsageDone(false),
      mGettingGroupUsageDone(false),
      mIsChecking(false),
      mOriginUsage(0),
      mGroupUsage(0),
      mGroupLimit(0) {}

void QuotaUsageChecker::Start() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mIsChecking) {
    return;
  }
  mIsChecking = true;

  RefPtr<QuotaUsageChecker> self = this;
  auto scopeExit = MakeScopeExit([self]() { self->RunCallback(false); });

  RefPtr<QuotaManagerService> qms = QuotaManagerService::GetOrCreate();
  MOZ_ASSERT(qms);

  // Asynchronious getting quota usage for principal
  nsCOMPtr<nsIQuotaUsageRequest> usageRequest;
  if (NS_WARN_IF(NS_FAILED(qms->GetUsageForPrincipal(
          mPrincipal, this, false, getter_AddRefs(usageRequest))))) {
    return;
  }

  // Asynchronious getting group usage and limit
  nsCOMPtr<nsIQuotaRequest> request;
  if (NS_WARN_IF(
          NS_FAILED(qms->Estimate(mPrincipal, getter_AddRefs(request))))) {
    return;
  }
  request->SetCallback(this);

  scopeExit.release();
}

void QuotaUsageChecker::RunCallback(bool aResult) {
  MOZ_ASSERT(mIsChecking && mCallback);
  if (!mIsChecking) {
    return;
  }
  mIsChecking = false;
  mGettingOriginUsageDone = false;
  mGettingGroupUsageDone = false;

  mCallback(aResult);
  mCallback = nullptr;
}

template <typename T, typename U>
nsresult QuotaUsageChecker::GetResult(T* aRequest, U& aResult) {
  nsCOMPtr<nsIVariant> result;
  nsresult rv = aRequest->GetResult(getter_AddRefs(result));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsID* iid;
  nsCOMPtr<nsISupports> supports;
  rv = result->GetAsInterface(&iid, getter_AddRefs(supports));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  free(iid);

  aResult = do_QueryInterface(supports);
  return NS_OK;
}

void QuotaUsageChecker::CheckQuotaHeadroom() {
  MOZ_ASSERT(NS_IsMainThread());

  const uint64_t groupHeadroom =
      static_cast<uint64_t>(
          StaticPrefs::
              dom_serviceWorkers_mitigations_group_usage_headroom_kb()) *
      1024;
  const uint64_t groupAvailable = mGroupLimit - mGroupUsage;

  // Group usage head room is satisfied, does not need the usage mitigation.
  if (groupAvailable > groupHeadroom) {
    RunCallback(true);
    return;
  }

  RefPtr<QuotaUsageChecker> self = this;
  auto scopeExit = MakeScopeExit([self]() { self->RunCallback(false); });

  nsCOMPtr<nsIClearDataService> csd =
      do_GetService("@mozilla.org/clear-data-service;1");
  MOZ_ASSERT(csd);

  // Group usage headroom is not satisfied even removing the origin usage,
  // clear all group usage.
  if ((groupAvailable + mOriginUsage) < groupHeadroom) {
    nsAutoCString host;
    nsresult rv = mPrincipal->GetHost(host);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    rv = csd->DeleteDataFromBaseDomain(
        host, false, nsIClearDataService::CLEAR_DOM_QUOTA, this);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    // clear the origin usage since it makes group usage headroom be satisifed.
  } else {
    nsresult rv = csd->DeleteDataFromPrincipal(
        mPrincipal, false, nsIClearDataService::CLEAR_DOM_QUOTA, this);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  }

  scopeExit.release();
}

// nsIQuotaUsageCallback implementation

NS_IMETHODIMP QuotaUsageChecker::OnUsageResult(
    nsIQuotaUsageRequest* aUsageRequest) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aUsageRequest);

  RefPtr<QuotaUsageChecker> self = this;
  auto scopeExit = MakeScopeExit([self]() { self->RunCallback(false); });

  nsresult resultCode;
  nsresult rv = aUsageRequest->GetResultCode(&resultCode);
  if (NS_WARN_IF(NS_FAILED(rv) || NS_FAILED(resultCode))) {
    return rv;
  }

  nsCOMPtr<nsIQuotaOriginUsageResult> usageResult;
  rv = GetResult(aUsageRequest, usageResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  MOZ_ASSERT(usageResult);

  rv = usageResult->GetUsage(&mOriginUsage);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(!mGettingOriginUsageDone);
  mGettingOriginUsageDone = true;

  scopeExit.release();

  // Call CheckQuotaHeadroom() when both
  // QuotaManagerService::GetUsageForPrincipal() and
  // QuotaManagerService::Estimate() are done.
  if (mGettingOriginUsageDone && mGettingGroupUsageDone) {
    CheckQuotaHeadroom();
  }
  return NS_OK;
}

// nsIQuotaCallback implementation

NS_IMETHODIMP QuotaUsageChecker::OnComplete(nsIQuotaRequest* aRequest) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRequest);

  RefPtr<QuotaUsageChecker> self = this;
  auto scopeExit = MakeScopeExit([self]() { self->RunCallback(false); });

  nsresult resultCode;
  nsresult rv = aRequest->GetResultCode(&resultCode);
  if (NS_WARN_IF(NS_FAILED(rv) || NS_FAILED(resultCode))) {
    return rv;
  }

  nsCOMPtr<nsIQuotaEstimateResult> estimateResult;
  rv = GetResult(aRequest, estimateResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  MOZ_ASSERT(estimateResult);

  rv = estimateResult->GetUsage(&mGroupUsage);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = estimateResult->GetLimit(&mGroupLimit);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(!mGettingGroupUsageDone);
  mGettingGroupUsageDone = true;

  scopeExit.release();

  // Call CheckQuotaHeadroom() when both
  // QuotaManagerService::GetUsageForPrincipal() and
  // QuotaManagerService::Estimate() are done.
  if (mGettingOriginUsageDone && mGettingGroupUsageDone) {
    CheckQuotaHeadroom();
  }
  return NS_OK;
}

// nsIClearDataCallback implementation

NS_IMETHODIMP QuotaUsageChecker::OnDataDeleted(uint32_t aFailedFlags) {
  RunCallback(true);
  return NS_OK;
}

// Helper methods in ServiceWorkerQuotaUtils.h

void ClearQuotaUsageIfNeeded(nsIPrincipal* aPrincipal,
                             ServiceWorkerQuotaMitigationCallback&& aCallback) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  RefPtr<QuotaUsageChecker> checker =
      MakeRefPtr<QuotaUsageChecker>(aPrincipal, std::move(aCallback));
  checker->Start();
}

}  // namespace mozilla::dom
