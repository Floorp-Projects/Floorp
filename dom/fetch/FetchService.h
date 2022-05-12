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

class nsILoadGroup;
class nsIPrincipal;
class nsICookieJarSettings;
class PerformanceStorage;

namespace mozilla::dom {

class InternalRequest;
class InternalResponse;

using FetchServiceResponse = SafeRefPtr<InternalResponse>;
using FetchServiceResponseAvailablePromise =
    MozPromise<FetchServiceResponse, CopyableErrorResult, true>;
using FetchServiceResponseEndPromise =
    MozPromise<ResponseEndArgs, CopyableErrorResult, true>;

class FetchServicePromises final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FetchServicePromises);

 public:
  FetchServicePromises();

  RefPtr<FetchServiceResponseAvailablePromise> GetResponseAvailablePromise();
  RefPtr<FetchServiceResponseEndPromise> GetResponseEndPromise();

  void ResolveResponseAvailablePromise(FetchServiceResponse&& aResponse,
                                       const char* aMethodName);
  void RejectResponseAvailablePromise(const CopyableErrorResult&& aError,
                                      const char* aMethodName);
  void ResolveResponseEndPromise(ResponseEndArgs&& aArgs,
                                 const char* aMethodName);
  void RejectResponseEndPromise(const CopyableErrorResult&& aError,
                                const char* aMethodName);

 private:
  ~FetchServicePromises() = default;

  RefPtr<FetchServiceResponseAvailablePromise::Private> mAvailablePromise;
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
  NS_DECL_ISUPPORTS;
  NS_DECL_NSIOBSERVER;

  static already_AddRefed<FetchService> GetInstance();

  static RefPtr<FetchServicePromises> NetworkErrorResponse(nsresult aRv);

  FetchService();

  // This method creates a FetchInstance to trigger fetch.
  // The created FetchInstance is saved in mFetchInstanceTable
  RefPtr<FetchServicePromises> Fetch(SafeRefPtr<InternalRequest> aRequest,
                                     nsIChannel* aChannel = nullptr);

  void CancelFetch(RefPtr<FetchServicePromises>&& aPromises);

 private:
  /**
   * FetchInstance is an internal representation for each Fetch created by
   * FetchService.
   * FetchInstance is also a FetchDriverObserver which has responsibility to
   * resolve/reject the FetchServicePromises.
   * FetchInstance triggers fetch by instancing a FetchDriver with proper
   * initialization. The general usage flow of FetchInstance is as follows
   *
   * RefPtr<FetchInstance> fetch = MakeRefPtr<FetchInstance>(aResquest);
   * fetch->Initialize();
   * RefPtr<FetchServicePromises> fetch->Fetch();
   */
  class FetchInstance final : public FetchDriverObserver {
   public:
    explicit FetchInstance(SafeRefPtr<InternalRequest> aRequest);

    // This method is used for initialize the fetch.
    // It accepts a nsIChannel for initialization, this is for the navigation
    // preload case since there has already been an intercepted channel for
    // dispatching fetch event, and needed information can be gotten from the
    // intercepted channel.
    // For other case, the fetch needed information be created according to the
    // mRequest
    nsresult Initialize(nsIChannel* aChannel = nullptr);

    RefPtr<FetchServicePromises> Fetch();

    void Cancel();

    /* FetchDriverObserver interface */
    void OnResponseEnd(FetchDriverObserver::EndReason aReason) override;
    void OnResponseAvailableInternal(
        SafeRefPtr<InternalResponse> aResponse) override;
    bool NeedOnDataAvailable() override;
    void OnDataAvailable() override;
    void FlushConsoleReport() override;

   private:
    ~FetchInstance();

    SafeRefPtr<InternalRequest> mRequest;
    nsCOMPtr<nsIPrincipal> mPrincipal;
    nsCOMPtr<nsILoadGroup> mLoadGroup;
    nsCOMPtr<nsICookieJarSettings> mCookieJarSettings;
    RefPtr<PerformanceStorage> mPerformanceStorage;
    RefPtr<FetchDriver> mFetchDriver;
    SafeRefPtr<InternalResponse> mResponse;

    RefPtr<FetchServicePromises> mPromises;
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
