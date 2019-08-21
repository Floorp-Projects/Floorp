/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FetchDriver_h
#define mozilla_dom_FetchDriver_h

#include "nsIChannelEventSink.h"
#include "nsICacheInfoChannel.h"
#include "nsIInterfaceRequestor.h"
#include "nsIStreamListener.h"
#include "nsIThreadRetargetableStreamListener.h"
#include "mozilla/ConsoleReportCollector.h"
#include "mozilla/dom/AbortSignal.h"
#include "mozilla/dom/SerializedStackHolder.h"
#include "mozilla/dom/SRIMetadata.h"
#include "mozilla/RefPtr.h"

#include "mozilla/DebugOnly.h"

class nsIConsoleReportCollector;
class nsICookieSettings;
class nsICSPEventListener;
class nsIEventTarget;
class nsIOutputStream;
class nsILoadGroup;
class nsIPrincipal;

namespace mozilla {
namespace dom {

class Document;
class InternalRequest;
class InternalResponse;
class PerformanceStorage;

/**
 * Provides callbacks to be called when response is available or on error.
 * Implemenations usually resolve or reject the promise returned from fetch().
 * The callbacks can be called synchronously or asynchronously from
 * FetchDriver::Fetch.
 */
class FetchDriverObserver {
 public:
  FetchDriverObserver()
      : mReporter(new ConsoleReportCollector()), mGotResponseAvailable(false) {}

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FetchDriverObserver);
  void OnResponseAvailable(InternalResponse* aResponse) {
    MOZ_ASSERT(!mGotResponseAvailable);
    mGotResponseAvailable = true;
    OnResponseAvailableInternal(aResponse);
  }

  enum EndReason {
    eAborted,
    eByNetworking,
  };

  virtual void OnResponseEnd(EndReason aReason){};

  nsIConsoleReportCollector* GetReporter() const { return mReporter; }

  virtual void FlushConsoleReport() = 0;

  // Called in OnStartRequest() to determine if the OnDataAvailable() method
  // needs to be called.  Invoking that method may generate additional main
  // thread runnables.
  virtual bool NeedOnDataAvailable() = 0;

  // Called once when the first byte of data is received iff
  // NeedOnDataAvailable() returned true when called in OnStartRequest().
  virtual void OnDataAvailable() = 0;

 protected:
  virtual ~FetchDriverObserver(){};

  virtual void OnResponseAvailableInternal(InternalResponse* aResponse) = 0;

  nsCOMPtr<nsIConsoleReportCollector> mReporter;

 private:
  bool mGotResponseAvailable;
};

class AlternativeDataStreamListener;

class FetchDriver final : public nsIStreamListener,
                          public nsIChannelEventSink,
                          public nsIInterfaceRequestor,
                          public nsIThreadRetargetableStreamListener,
                          public AbortFollower {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSITHREADRETARGETABLESTREAMLISTENER

  FetchDriver(InternalRequest* aRequest, nsIPrincipal* aPrincipal,
              nsILoadGroup* aLoadGroup, nsIEventTarget* aMainThreadEventTarget,
              nsICookieSettings* aCookieSettings,
              PerformanceStorage* aPerformanceStorage, bool aIsTrackingFetch);

  nsresult Fetch(AbortSignalImpl* aSignalImpl, FetchDriverObserver* aObserver);

  void SetDocument(Document* aDocument);

  void SetCSPEventListener(nsICSPEventListener* aCSPEventListener);

  void SetClientInfo(const ClientInfo& aClientInfo);

  void SetController(const Maybe<ServiceWorkerDescriptor>& aController);

  void SetWorkerScript(const nsACString& aWorkerScript) {
    MOZ_ASSERT(!aWorkerScript.IsEmpty());
    mWorkerScript = aWorkerScript;
  }

  void SetOriginStack(UniquePtr<SerializedStackHolder>&& aOriginStack) {
    mOriginStack = std::move(aOriginStack);
  }

  // AbortFollower
  void Abort() override;

 private:
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsILoadGroup> mLoadGroup;
  RefPtr<InternalRequest> mRequest;
  RefPtr<InternalResponse> mResponse;
  nsCOMPtr<nsIOutputStream> mPipeOutputStream;
  RefPtr<FetchDriverObserver> mObserver;
  RefPtr<Document> mDocument;
  nsCOMPtr<nsICSPEventListener> mCSPEventListener;
  Maybe<ClientInfo> mClientInfo;
  Maybe<ServiceWorkerDescriptor> mController;
  nsCOMPtr<nsIChannel> mChannel;
  nsAutoPtr<SRICheckDataVerifier> mSRIDataVerifier;
  nsCOMPtr<nsIEventTarget> mMainThreadEventTarget;

  nsCOMPtr<nsICookieSettings> mCookieSettings;

  // This is set only when Fetch is used in workers.
  RefPtr<PerformanceStorage> mPerformanceStorage;

  SRIMetadata mSRIMetadata;
  nsCString mWorkerScript;
  UniquePtr<SerializedStackHolder> mOriginStack;

  // This is written once in OnStartRequest on the main thread and then
  // written/read in OnDataAvailable() on any thread.  Necko guarantees
  // that these do not overlap.
  bool mNeedToObserveOnDataAvailable;

  bool mIsTrackingFetch;

  RefPtr<AlternativeDataStreamListener> mAltDataListener;
  bool mOnStopRequestCalled;

#ifdef DEBUG
  bool mResponseAvailableCalled;
  bool mFetchCalled;
#endif

  friend class AlternativeDataStreamListener;

  FetchDriver() = delete;
  FetchDriver(const FetchDriver&) = delete;
  FetchDriver& operator=(const FetchDriver&) = delete;
  ~FetchDriver();

  nsresult HttpFetch(
      const nsACString& aPreferredAlternativeDataType = EmptyCString());
  // Returns the filtered response sent to the observer.
  already_AddRefed<InternalResponse> BeginAndGetFilteredResponse(
      InternalResponse* aResponse, bool aFoundOpaqueRedirect);
  // Utility since not all cases need to do any post processing of the filtered
  // response.
  void FailWithNetworkError(nsresult rv);

  void SetRequestHeaders(nsIHttpChannel* aChannel) const;

  nsresult FinishOnStopRequest(AlternativeDataStreamListener* aAltDataListener);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_FetchDriver_h
