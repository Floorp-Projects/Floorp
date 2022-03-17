/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FetchLog.h"
#include "nsContentUtils.h"
#include "nsICookieJarSettings.h"
#include "nsILoadGroup.h"
#include "nsILoadInfo.h"
#include "nsIIOService.h"
#include "nsIObserverService.h"
#include "nsIPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/FetchService.h"
#include "mozilla/dom/InternalRequest.h"
#include "mozilla/dom/InternalResponse.h"
#include "mozilla/dom/PerformanceStorage.h"
#include "mozilla/dom/PerformanceTiming.h"
#include "mozilla/ipc/BackgroundUtils.h"

namespace mozilla::dom {

mozilla::LazyLogModule gFetchLog("Fetch");

FetchServiceResponse CreateErrorResponse(nsresult aRv) {
  IPCPerformanceTimingData ipcTimingData;
  return MakeTuple(InternalResponse::NetworkError(aRv), ipcTimingData,
                   EmptyString(), EmptyString());
}

// FetchInstance

FetchService::FetchInstance::FetchInstance(SafeRefPtr<InternalRequest> aRequest)
    : mRequest(std::move(aRequest)) {}

FetchService::FetchInstance::~FetchInstance() = default;

nsresult FetchService::FetchInstance::Initialize(nsIChannel* aChannel) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  // Get needed information for FetchDriver from passed-in channel.
  if (aChannel) {
    FETCH_LOG(("FetchInstance::Initialize [%p] aChannel[%p]", this, aChannel));

    nsresult rv;
    nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
    MOZ_ASSERT(loadInfo);

    nsCOMPtr<nsIURI> channelURI;
    rv = aChannel->GetURI(getter_AddRefs(channelURI));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsIScriptSecurityManager* securityManager =
        nsContentUtils::GetSecurityManager();
    if (securityManager) {
      securityManager->GetChannelResultPrincipal(aChannel,
                                                 getter_AddRefs(mPrincipal));
    }

    if (!mPrincipal) {
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

  nsAutoCString principalSpec;
  MOZ_ALWAYS_SUCCEEDS(mPrincipal->GetAsciiSpec(principalSpec));
  nsAutoCString requestURL;
  mRequest->GetURL(requestURL);
  FETCH_LOG(("FetchInstance::Fetch [%p], mRequest URL: %s mPrincipal: %s", this,
             requestURL.BeginReading(), principalSpec.BeginReading()));

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
    FETCH_LOG(
        ("FetchInstance::Fetch FetchDriver::Fetch failed(0x%X)", (uint32_t)rv));
    return FetchService::NetworkErrorResponse(rv);
  }

  return mResponsePromiseHolder.Ensure(__func__);
}

void FetchService::FetchInstance::Cancel() {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  FETCH_LOG(("FetchInstance::Cancel() [%p]", this));

  if (mFetchDriver) {
    mFetchDriver->RunAbortAlgorithm();
  }

  mResponsePromiseHolder.ResolveIfExists(
      CreateErrorResponse(NS_ERROR_DOM_ABORT_ERR), __func__);
}

void FetchService::FetchInstance::OnResponseEnd(
    FetchDriverObserver::EndReason aReason) {
  FETCH_LOG(("FetchInstance::OnResponseEnd [%p]", this));
  if (aReason == eAborted) {
    FETCH_LOG(("FetchInstance::OnResponseEnd end with eAborted"));
    mResponsePromiseHolder.ResolveIfExists(
        CreateErrorResponse(NS_ERROR_DOM_ABORT_ERR), __func__);
    return;
  }
  if (!mResponsePromiseHolder.IsEmpty()) {
    // Remove the FetchInstance from FetchInstanceTable
    RefPtr<FetchServiceResponsePromise> responsePromise =
        mResponsePromiseHolder.Ensure(__func__);
    RefPtr<FetchService> fetchService = FetchService::GetInstance();
    MOZ_ASSERT(fetchService);
    auto entry = fetchService->mFetchInstanceTable.Lookup(responsePromise);
    MOZ_ASSERT(entry);
    entry.Remove();
    FETCH_LOG(
        ("FetchInstance::OnResponseEnd entry of responsePromise[%p] is removed",
         responsePromise.get()));
  }

  // Get PerformanceTimingData from FetchDriver.
  IPCPerformanceTimingData ipcPerformanceTiming;
  nsString initiatorType;
  nsString entryName;
  UniquePtr<PerformanceTimingData> performanceTiming(
      mFetchDriver->GetPerformanceTimingData(initiatorType, entryName));
  if (performanceTiming != nullptr) {
    ipcPerformanceTiming = performanceTiming->ToIPC();
  }

  initiatorType = u"navigation"_ns;

  FetchServiceResponse response = MakeTuple(
      std::move(mResponse), ipcPerformanceTiming, initiatorType, entryName);

  // Resolve the FetchServiceResponsePromise
  mResponsePromiseHolder.ResolveIfExists(std::move(response), __func__);
}

void FetchService::FetchInstance::OnResponseAvailableInternal(
    SafeRefPtr<InternalResponse> aResponse) {
  FETCH_LOG(("FetchInstance::OnResponseAvailableInternal [%p]", this));
  mResponse = std::move(aResponse);
}

// TODO:
// Following methods would not be used for navigation preload, but would be used
// with PFetch. They will be implemented in bug 1351231.
bool FetchService::FetchInstance::NeedOnDataAvailable() { return false; }
void FetchService::FetchInstance::OnDataAvailable() {}
void FetchService::FetchInstance::FlushConsoleReport() {}

// FetchService

NS_IMPL_ISUPPORTS(FetchService, nsIObserver)

StaticRefPtr<FetchService> gInstance;

/*static*/
already_AddRefed<FetchService> FetchService::GetInstance() {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  if (!gInstance) {
    gInstance = MakeRefPtr<FetchService>();
    nsresult rv = gInstance->RegisterNetworkObserver();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      gInstance = nullptr;
      return nullptr;
    }
    ClearOnShutdown(&gInstance);
  }
  RefPtr<FetchService> service = gInstance;
  return service.forget();
}

/*static*/
RefPtr<FetchServiceResponsePromise> FetchService::NetworkErrorResponse(
    nsresult aRv) {
  return FetchServiceResponsePromise::CreateAndResolve(CreateErrorResponse(aRv),
                                                       __func__);
}

FetchService::FetchService() {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
}

FetchService::~FetchService() {
  MOZ_ALWAYS_SUCCEEDS(UnregisterNetworkObserver());
}

nsresult FetchService::RegisterNetworkObserver() {
  AssertIsOnMainThread();
  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  if (!observerService) {
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIIOService> ioService = services::GetIOService();
  if (!ioService) {
    return NS_ERROR_UNEXPECTED;
  }

  nsresult rv = observerService->AddObserver(
      this, NS_IOSERVICE_OFFLINE_STATUS_TOPIC, false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = observerService->AddObserver(this, "xpcom-shutdown", false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ioService->GetOffline(&mOffline);
  NS_ENSURE_SUCCESS(rv, rv);
  mObservingNetwork = true;

  return NS_OK;
}

nsresult FetchService::UnregisterNetworkObserver() {
  AssertIsOnMainThread();
  nsresult rv;
  if (mObservingNetwork) {
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (observerService) {
      rv = observerService->RemoveObserver(this,
                                           NS_IOSERVICE_OFFLINE_STATUS_TOPIC);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = observerService->RemoveObserver(this, "xpcom-shutdown");
      NS_ENSURE_SUCCESS(rv, rv);
    }
    mObservingNetwork = false;
  }
  return NS_OK;
}

NS_IMETHODIMP FetchService::Observe(nsISupports* aSubject, const char* aTopic,
                                    const char16_t* aData) {
  FETCH_LOG(("FetchService::Observe topic: %s", aTopic));
  AssertIsOnMainThread();
  MOZ_ASSERT(!strcmp(aTopic, NS_IOSERVICE_OFFLINE_STATUS_TOPIC) ||
             !strcmp(aTopic, "xpcom-shutdown"));

  if (!strcmp(aTopic, "xpcom-shutdown")) {
    // Going to shutdown, unregister the network status observer to avoid
    // receiving
    nsresult rv = UnregisterNetworkObserver();
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
  }

  if (nsDependentString(aData).EqualsLiteral(NS_IOSERVICE_ONLINE)) {
    mOffline = false;
  } else {
    mOffline = true;
    // Network is offline, cancel running fetchs.
    for (auto it = mFetchInstanceTable.begin(), end = mFetchInstanceTable.end();
         it != end; ++it) {
      it->GetData()->Cancel();
    }
    mFetchInstanceTable.Clear();
  }
  return NS_OK;
}

RefPtr<FetchServiceResponsePromise> FetchService::Fetch(
    SafeRefPtr<InternalRequest> aRequest, nsIChannel* aChannel) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  FETCH_LOG(("FetchService::Fetch aRequest[%p], aChannel[%p], mOffline: %s",
             aRequest.unsafeGetRawPtr(), aChannel,
             mOffline ? "true" : "false"));

  if (mOffline) {
    FETCH_LOG(("FetchService::Fetch network offline"));
    return NetworkErrorResponse(NS_ERROR_OFFLINE);
  }

  // Create FetchInstance
  RefPtr<FetchInstance> fetch = MakeRefPtr<FetchInstance>(aRequest.clonePtr());

  // Call FetchInstance::Initialize() to get needed information for FetchDriver,
  nsresult rv = fetch->Initialize(aChannel);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NetworkErrorResponse(rv);
  }

  // Call FetchInstance::Fetch() to start an asynchronous fetching.
  RefPtr<FetchServiceResponsePromise> responsePromise = fetch->Fetch();

  if (!responsePromise->IsResolved()) {
    // Insert the created FetchInstance into FetchInstanceTable.
    if (!mFetchInstanceTable.WithEntryHandle(responsePromise,
                                             [&](auto&& entry) {
                                               if (entry.HasEntry()) {
                                                 return false;
                                               }
                                               entry.Insert(fetch);
                                               return true;
                                             })) {
      FETCH_LOG(("FetchService::Fetch entry of responsePromise[%p] exists",
                 responsePromise.get()));
      return NetworkErrorResponse(NS_ERROR_UNEXPECTED);
    }
    FETCH_LOG(("FetchService::Fetch responsePromise[%p], fetchInstance[%p]",
               responsePromise.get(), fetch.get()));
  }
  return responsePromise;
}

void FetchService::CancelFetch(
    RefPtr<FetchServiceResponsePromise>&& aResponsePromise) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aResponsePromise);
  FETCH_LOG(("FetchService::CancelFetch aResponsePromise[%p]",
             aResponsePromise.get()));

  auto entry = mFetchInstanceTable.Lookup(aResponsePromise);
  if (entry) {
    // Notice any modifications here before entry.Remove() probably should be
    // reflected to Observe() offline case.
    entry.Data()->Cancel();

    entry.Remove();
    FETCH_LOG(("FetchService::CancelFetch aResponsePromise[%p] is removed",
               aResponsePromise.get()));
  }
}

}  // namespace mozilla::dom
