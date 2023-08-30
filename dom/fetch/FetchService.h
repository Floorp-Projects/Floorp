/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_FetchService_h
#define _mozilla_dom_FetchService_h

#include "nsIChannel.h"
#include "nsIObserver.h"
#include "nsTHashMap.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/FetchDriver.h"
#include "mozilla/dom/FetchTypes.h"
#include "mozilla/dom/PerformanceTimingTypes.h"
#include "mozilla/dom/SafeRefPtr.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/net/NeckoChannelParams.h"

class nsILoadGroup;
class nsIPrincipal;
class nsICookieJarSettings;
class PerformanceStorage;

namespace mozilla::dom {

class InternalRequest;
class InternalResponse;
class ClientInfo;
class ServiceWorkerDescriptor;

using FetchServiceResponse = SafeRefPtr<InternalResponse>;
using FetchServiceResponseAvailablePromise =
    MozPromise<FetchServiceResponse, CopyableErrorResult, true>;
using FetchServiceResponseTimingPromise =
    MozPromise<ResponseTiming, CopyableErrorResult, true>;
using FetchServiceResponseEndPromise =
    MozPromise<ResponseEndArgs, CopyableErrorResult, true>;

class FetchServicePromises final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FetchServicePromises);

 public:
  FetchServicePromises();

  RefPtr<FetchServiceResponseAvailablePromise> GetResponseAvailablePromise();
  RefPtr<FetchServiceResponseTimingPromise> GetResponseTimingPromise();
  RefPtr<FetchServiceResponseEndPromise> GetResponseEndPromise();

  void ResolveResponseAvailablePromise(FetchServiceResponse&& aResponse,
                                       const char* aMethodName);
  void RejectResponseAvailablePromise(const CopyableErrorResult&& aError,
                                      const char* aMethodName);
  void ResolveResponseTimingPromise(ResponseTiming&& aTiming,
                                    const char* aMethodName);
  void RejectResponseTimingPromise(const CopyableErrorResult&& aError,
                                   const char* aMethodName);
  void ResolveResponseEndPromise(ResponseEndArgs&& aArgs,
                                 const char* aMethodName);
  void RejectResponseEndPromise(const CopyableErrorResult&& aError,
                                const char* aMethodName);

 private:
  ~FetchServicePromises() = default;

  RefPtr<FetchServiceResponseAvailablePromise::Private> mAvailablePromise;
  RefPtr<FetchServiceResponseTimingPromise::Private> mTimingPromise;
  RefPtr<FetchServiceResponseEndPromise::Private> mEndPromise;
};

/**
 * FetchService is a singleton object which designed to be used in parent
 * process main thread only. It is used to handle the special fetch requests
 * from ServiceWorkers(by Navigation Preload) and PFetch.
 *
 * FetchService creates FetchInstance internally to represent each Fetch
 * request. It supports an asynchronous fetching, FetchServicePromises is
 * created when a Fetch starts, once the response is ready or any error happens,
 * the FetchServicePromises would be resolved or rejected. The promises
 * consumers can set callbacks to handle the Fetch result.
 */
class FetchService final : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  struct NavigationPreloadArgs {
    SafeRefPtr<InternalRequest> mRequest;
    nsCOMPtr<nsIChannel> mChannel;
  };

  struct WorkerFetchArgs {
    SafeRefPtr<InternalRequest> mRequest;
    mozilla::ipc::PrincipalInfo mPrincipalInfo;
    nsCString mWorkerScript;
    Maybe<ClientInfo> mClientInfo;
    Maybe<ServiceWorkerDescriptor> mController;
    Maybe<net::CookieJarSettingsArgs> mCookieJarSettings;
    bool mNeedOnDataAvailable;
    nsCOMPtr<nsICSPEventListener> mCSPEventListener;
    uint64_t mAssociatedBrowsingContextID;
    nsCOMPtr<nsISerialEventTarget> mEventTarget;
    nsID mActorID;
  };

  struct UnknownArgs {};

  using FetchArgs =
      Variant<NavigationPreloadArgs, WorkerFetchArgs, UnknownArgs>;

  static already_AddRefed<FetchService> GetInstance();

  static RefPtr<FetchServicePromises> NetworkErrorResponse(
      nsresult aRv, const FetchArgs& aArgs = AsVariant(UnknownArgs{}));

  FetchService();

  // This method creates a FetchInstance to trigger fetch.
  // The created FetchInstance is saved in mFetchInstanceTable
  RefPtr<FetchServicePromises> Fetch(FetchArgs&& aArgs);

  void CancelFetch(const RefPtr<FetchServicePromises>&& aPromises);

 private:
  /**
   * FetchInstance is an internal representation for each Fetch created by
   * FetchService.
   * FetchInstance is also a FetchDriverObserver which has responsibility to
   * resolve/reject the FetchServicePromises.
   * FetchInstance triggers fetch by instancing a FetchDriver with proper
   * initialization. The general usage flow of FetchInstance is as follows
   *
   * RefPtr<FetchInstance> fetch = MakeRefPtr<FetchInstance>();
   * fetch->Initialize(FetchArgs args);
   * RefPtr<FetchServicePromises> fetch->Fetch();
   */
  class FetchInstance final : public FetchDriverObserver {
   public:
    FetchInstance() = default;

    nsresult Initialize(FetchArgs&& aArgs);

    const FetchArgs& Args() { return mArgs; }

    RefPtr<FetchServicePromises> Fetch();

    void Cancel();

    /* FetchDriverObserver interface */
    void OnResponseEnd(FetchDriverObserver::EndReason aReason,
                       JS::Handle<JS::Value> aReasonDetails) override;
    void OnResponseAvailableInternal(
        SafeRefPtr<InternalResponse> aResponse) override;
    bool NeedOnDataAvailable() override;
    void OnDataAvailable() override;
    void FlushConsoleReport() override;
    void OnReportPerformanceTiming() override;
    void OnNotifyNetworkMonitorAlternateStack(uint64_t aChannelID) override;

   private:
    ~FetchInstance() = default;

    SafeRefPtr<InternalRequest> mRequest;
    nsCOMPtr<nsIPrincipal> mPrincipal;
    nsCOMPtr<nsILoadGroup> mLoadGroup;
    nsCOMPtr<nsICookieJarSettings> mCookieJarSettings;
    RefPtr<PerformanceStorage> mPerformanceStorage;
    FetchArgs mArgs{AsVariant(FetchService::UnknownArgs())};
    RefPtr<FetchDriver> mFetchDriver;
    SafeRefPtr<InternalResponse> mResponse;
    RefPtr<FetchServicePromises> mPromises;
    bool mIsWorkerFetch{false};
  };

  ~FetchService();

  nsresult RegisterNetworkObserver();
  nsresult UnregisterNetworkObserver();

  // This is a container to manage the generated fetches.
  nsTHashMap<nsRefPtrHashKey<FetchServicePromises>, RefPtr<FetchInstance> >
      mFetchInstanceTable;
  bool mObservingNetwork{false};
  bool mOffline{false};
};

}  // namespace mozilla::dom

#endif  // _mozilla_dom_FetchService_h
