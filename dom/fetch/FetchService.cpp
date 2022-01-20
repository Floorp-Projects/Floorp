/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsILoadGroup.h"
#include "nsILoadInfo.h"
#include "nsIPrincipal.h"
#include "nsICookieJarSettings.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/dom/FetchService.h"
#include "mozilla/dom/InternalRequest.h"
#include "mozilla/dom/InternalResponse.h"
#include "mozilla/dom/PerformanceStorage.h"
#include "mozilla/ipc/BackgroundUtils.h"

namespace mozilla::dom {

// FetchInstance

FetchService::FetchInstance::FetchInstance(SafeRefPtr<InternalRequest> aRequest)
    : mRequest(std::move(aRequest)) {}

FetchService::FetchInstance::~FetchInstance() = default;

nsresult FetchService::FetchInstance::Initialize(nsIChannel* aChannel) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  // Setup principal form InternalRequest
  const mozilla::UniquePtr<mozilla::ipc::PrincipalInfo>& principalInfo =
      mRequest->GetPrincipalInfo();
  MOZ_ASSERT(principalInfo);
  auto principalOrErr = PrincipalInfoToPrincipal(*principalInfo);
  if (NS_WARN_IF(principalOrErr.isErr())) {
    return principalOrErr.unwrapErr();
  }
  mPrincipal = principalOrErr.unwrap();

  // Get needed information for FetchDriver from passed-in channel.
  if (aChannel) {
    nsresult rv;
    nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
    MOZ_ASSERT(loadInfo);

    // Principal in the InternalRequest should be the same with the triggering
    // principal in the LoadInfo
    nsCOMPtr<nsIPrincipal> triggeringPrincipal;
    rv = loadInfo->GetTriggeringPrincipal(getter_AddRefs(triggeringPrincipal));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    if (!mPrincipal->Equals(triggeringPrincipal)) {
      return NS_ERROR_UNEXPECTED;
    }

    // Get loadGroup from channel
    rv = aChannel->GetLoadGroup(getter_AddRefs(mLoadGroup));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    if (!mLoadGroup) {
      rv = NS_NewLoadGroup(getter_AddRefs(mLoadGroup), mPrincipal);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    // Get CookieJarSettings from channel
    rv = loadInfo->GetCookieJarSettings(getter_AddRefs(mCookieJarSettings));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // Get PerformanceStorage from channel
    mPerformanceStorage = loadInfo->GetPerformanceStorage();
  } else {
    // TODO:
    // Get information from InternalRequest and PFetch IPC parameters.
    // This will be implemented in bug 1351231.
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  return NS_OK;
}

RefPtr<FetchServiceResponsePromise> FetchService::FetchInstance::Fetch() {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(mPrincipal);
  MOZ_ASSERT(mLoadGroup);

  nsresult rv;

  // Create a FetchDriver instance
  mFetchDriver = MakeRefPtr<FetchDriver>(
      mRequest.clonePtr(),         // Fetch Request
      mPrincipal,                  // Principal
      mLoadGroup,                  // LoadGroup
      GetMainThreadEventTarget(),  // MainThreadEventTarget
      mCookieJarSettings,          // CookieJarSettings
      mPerformanceStorage,         // PerformanceStorage
      false                        // IsTrackingFetch
  );

  // Call FetchDriver::Fetch to start fetching.
  // Pass AbortSignalImpl as nullptr since we no need support AbortSignalImpl
  // with FetchService. AbortSignalImpl related information should be passed
  // through PFetch or InterceptedHttpChannel, then call
  // FetchService::CancelFetch() to abort the running fetch.
  rv = mFetchDriver->Fetch(nullptr, this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return FetchServiceResponsePromise::CreateAndResolve(
        InternalResponse::NetworkError(rv), __func__);
  }

  return mResponsePromiseHolder.Ensure(__func__);
}

void FetchService::FetchInstance::Cancel() {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  if (mFetchDriver) {
    mFetchDriver->RunAbortAlgorithm();
  }
}

void FetchService::FetchInstance::OnResponseEnd(
    FetchDriverObserver::EndReason aReason) {
  if (aReason == eAborted) {
    mResponsePromiseHolder.ResolveIfExists(
        InternalResponse::NetworkError(NS_ERROR_DOM_ABORT_ERR), __func__);
  }
}

void FetchService::FetchInstance::OnResponseAvailableInternal(
    SafeRefPtr<InternalResponse> aResponse) {
  // Remove the FetchInstance from FetchInstanceTable
  RefPtr<FetchServiceResponsePromise> responsePromise =
      mResponsePromiseHolder.Ensure(__func__);
  RefPtr<FetchService> fetchService = FetchService::GetInstance();
  MOZ_ASSERT(fetchService);
  auto entry = fetchService->mFetchInstanceTable.Lookup(responsePromise);
  MOZ_ASSERT(entry);
  entry.Remove();

  // Resolve the FetchServiceResponsePromise
  mResponsePromiseHolder.ResolveIfExists(std::move(aResponse), __func__);
}

// TODO:
// Following methods would not be used for navigation preload, but would be used
// with PFetch. They will be implemented in bug 1351231.
bool FetchService::FetchInstance::NeedOnDataAvailable() { return false; }
void FetchService::FetchInstance::OnDataAvailable() {}
void FetchService::FetchInstance::FlushConsoleReport() {}

// FetchService

StaticRefPtr<FetchService> gInstance;

/*static*/
already_AddRefed<FetchService> FetchService::GetInstance() {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  if (!gInstance) {
    gInstance = MakeRefPtr<FetchService>();
    ClearOnShutdown(&gInstance);
  }
  RefPtr<FetchService> service = gInstance;
  return service.forget();
}

/*static*/
RefPtr<FetchServiceResponsePromise> FetchService::NetworkErrorResponse(
    nsresult aRv) {
  return FetchServiceResponsePromise::CreateAndResolve(
      InternalResponse::NetworkError(aRv), __func__);
}

FetchService::FetchService() {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
}

FetchService::~FetchService() = default;

RefPtr<FetchServiceResponsePromise> FetchService::Fetch(
    SafeRefPtr<InternalRequest> aRequest, nsIChannel* aChannel) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  // Create FetchInstance
  RefPtr<FetchInstance> fetch = MakeRefPtr<FetchInstance>(aRequest.clonePtr());

  // Call FetchInstance::Initialize() to get needed information for FetchDriver,
  nsresult rv = fetch->Initialize(aChannel);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NetworkErrorResponse(rv);
  }

  // Call FetchInstance::Fetch() to start an asynchronous fetching.
  RefPtr<FetchServiceResponsePromise> responsePromise = fetch->Fetch();

  // Insert the created FetchInstance into FetchInstanceTable.
  // TODO: the FetchInstance should be removed from FetchInstanceTable, once the
  //       fetching finishes or be aborted. This should be implemented in
  //       following patches when implementing FetchDriverObserver on
  //       FetchInstance
  if (!mFetchInstanceTable.WithEntryHandle(responsePromise, [&](auto&& entry) {
        if (entry.HasEntry()) {
          return false;
        }
        entry.Insert(fetch);
        return true;
      })) {
    return NetworkErrorResponse(NS_ERROR_UNEXPECTED);
  }
  return responsePromise;
}

void FetchService::CancelFetch(
    RefPtr<FetchServiceResponsePromise>&& aResponsePromise) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aResponsePromise);

  auto entry = mFetchInstanceTable.Lookup(aResponsePromise);
  if (entry) {
    entry.Data()->Cancel();
    entry.Remove();
  }
}

}  // namespace mozilla::dom
