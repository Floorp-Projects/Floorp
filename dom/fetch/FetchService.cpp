/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/FetchService.h"
#include "mozilla/dom/InternalRequest.h"
#include "mozilla/dom/InternalResponse.h"
#include "nsXULAppAPI.h"

namespace mozilla::dom {

FetchService::FetchInstance::FetchInstance(SafeRefPtr<InternalRequest> aRequest)
    : mRequest(std::move(aRequest)) {}

nsresult FetchService::FetchInstance::Initialize(nsIChannel* aChannel) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  // TODO:
  // In this method, the needed information for instancing a FetchDriver will
  // be created and saved into member variables, which means principal
  // LoadGroup, cookieJarSettings, performanceStorage.
  // This will be implemented in the next patch.

  return NS_ERROR_NOT_IMPLEMENTED;
}

FetchService::FetchInstance::~FetchInstance() = default;

RefPtr<FetchServiceResponsePromise> FetchService::FetchInstance::Fetch() {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  // TODO:
  // After FetchInstance is initialized in proper, this method will instances a
  // FetchDriver and call FetchDriver::Fetch() to start an asynchronous
  // fetching. Once the fetching starts, this method returns a
  // FetchServiceResponsePromise to its caller. This would be implemented in
  // following patches.
  return mResponsePromiseHolder.Ensure(__func__);
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
    // TODO: Need to call FetchDriver::RunAbortAlgorithm();
    entry.Remove();
  }
}

}  // namespace mozilla::dom
