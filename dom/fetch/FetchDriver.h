/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FetchDriver_h
#define mozilla_dom_FetchDriver_h

#include "nsAutoPtr.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIChannelEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIStreamListener.h"
#include "nsRefPtr.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/net/ReferrerPolicy.h"

class nsIDocument;
class nsIOutputStream;
class nsILoadGroup;
class nsIPrincipal;

namespace mozilla {
namespace dom {

class InternalRequest;
class InternalResponse;

class FetchDriverObserver
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FetchDriverObserver);
  virtual void OnResponseAvailable(InternalResponse* aResponse) = 0;
  virtual void OnResponseEnd()
  { };

protected:
  virtual ~FetchDriverObserver()
  { };
};

class FetchDriver final : public nsIStreamListener,
                          public nsIChannelEventSink,
                          public nsIInterfaceRequestor,
                          public nsIAsyncVerifyRedirectCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIASYNCVERIFYREDIRECTCALLBACK

  explicit FetchDriver(InternalRequest* aRequest, nsIPrincipal* aPrincipal,
                       nsILoadGroup* aLoadGroup);
  NS_IMETHOD Fetch(FetchDriverObserver* aObserver);

  void
  SetDocument(nsIDocument* aDocument);

private:
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsILoadGroup> mLoadGroup;
  nsRefPtr<InternalRequest> mRequest;
  nsRefPtr<InternalResponse> mResponse;
  nsCOMPtr<nsIOutputStream> mPipeOutputStream;
  nsRefPtr<FetchDriverObserver> mObserver;
  nsCOMPtr<nsIInterfaceRequestor> mNotificationCallbacks;
  nsCOMPtr<nsIAsyncVerifyRedirectCallback> mRedirectCallback;
  nsCOMPtr<nsIChannel> mOldRedirectChannel;
  nsCOMPtr<nsIChannel> mNewRedirectChannel;
  nsCOMPtr<nsIDocument> mDocument;
  uint32_t mFetchRecursionCount;

  DebugOnly<bool> mResponseAvailableCalled;

  FetchDriver() = delete;
  FetchDriver(const FetchDriver&) = delete;
  FetchDriver& operator=(const FetchDriver&) = delete;
  ~FetchDriver();

  nsresult Fetch(bool aCORSFlag);
  nsresult ContinueFetch(bool aCORSFlag);
  nsresult BasicFetch();
  nsresult HttpFetch(bool aCORSFlag = false, bool aCORSPreflightFlag = false, bool aAuthenticationFlag = false);
  nsresult ContinueHttpFetchAfterNetworkFetch();
  // Returns the filtered response sent to the observer.
  already_AddRefed<InternalResponse>
  BeginAndGetFilteredResponse(InternalResponse* aResponse);
  // Utility since not all cases need to do any post processing of the filtered
  // response.
  void BeginResponse(InternalResponse* aResponse);
  nsresult FailWithNetworkError();
  nsresult SucceedWithResponse();
  nsresult DoesNotRequirePreflight(nsIChannel* aChannel);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FetchDriver_h
