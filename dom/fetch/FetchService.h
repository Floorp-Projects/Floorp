/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_FetchService_h
#define _mozilla_dom_FetchService_h

#include "nsIChannel.h"
#include "nsTHashMap.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/FetchDriver.h"
#include "mozilla/dom/PerformanceTimingTypes.h"
#include "mozilla/dom/SafeRefPtr.h"

class nsILoadGroup;
class nsIPrincipal;
class nsICookieJarSettings;
class PerformanceStorage;

namespace mozilla::dom {

class InternalRequest;
class InternalResponse;

using FetchServiceResponse =
    Tuple<SafeRefPtr<InternalResponse>, IPCPerformanceTimingData, nsString,
          nsString>;

using FetchServiceResponsePromise =
    MozPromise<FetchServiceResponse, CopyableErrorResult, true>;

/**
 * FetchService is a singleton object which designed to be used in parent
 * process main thread only. It is used to handle the special fetch requests
 * from ServiceWorkers(by Navigation Preload) and PFetch.
 *
 * FetchService creates FetchInstance internally to represent each Fetch
 * request. It supports an asynchronous fetching, FetchServiceResponsePromise is
 * created when a Fetch starts, once the response is ready or any error happens,
 * the FetchServiceResponsePromise would be resolved or rejected. The promise
 * consumers can set callbacks to handle the Fetch result.
 */
class FetchService final {
 public:
  NS_INLINE_DECL_REFCOUNTING(FetchService)

  static already_AddRefed<FetchService> GetInstance();

  static RefPtr<FetchServiceResponsePromise> NetworkErrorResponse(nsresult aRv);

  FetchService();

  // This method creates a FetchInstance to trigger fetch.
  // The created FetchInstance is saved in mFetchInstanceTable
  RefPtr<FetchServiceResponsePromise> Fetch(
      SafeRefPtr<InternalRequest> aRequest, nsIChannel* aChannel = nullptr);

  void CancelFetch(RefPtr<FetchServiceResponsePromise>&& aResponsePromise);

 private:
  /**
   * FetchInstance is an internal representation for each Fetch created by
   * FetchService.
   * FetchInstance is also a FetchDriverObserver which has responsibility to
   * resolve/reject the FetchServiceResponsePromise.
   * FetchInstance triggers fetch by instancing a FetchDriver with proper
   * initialization. The general usage flow of FetchInstance is as follows
   *
   * RefPtr<FetchInstance> fetch = MakeRefPtr<FetchInstance>(aResquest);
   * fetch->Initialize();
   * RefPtr<FetchServiceResponsePromise> fetch->Fetch();
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

    RefPtr<FetchServiceResponsePromise> Fetch();

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

    MozPromiseHolder<FetchServiceResponsePromise> mResponsePromiseHolder;
  };

  ~FetchService();

  // This is a container to manage the generated fetches.
  nsTHashMap<nsRefPtrHashKey<FetchServiceResponsePromise>,
             RefPtr<FetchInstance> >
      mFetchInstanceTable;
};

}  // namespace mozilla::dom

#endif  // _mozilla_dom_FetchService_h
