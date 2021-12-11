/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A service that provides methods for synchronously loading a DOM in various
 * ways.
 */

#include "nsSyncLoadService.h"
#include "nsCOMPtr.h"
#include "nsIChannel.h"
#include "nsIChannelEventSink.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIInterfaceRequestor.h"
#include "nsIStreamListener.h"
#include "nsIURI.h"
#include "nsString.h"
#include "nsWeakReference.h"
#include "mozilla/dom/Document.h"
#include "nsIHttpChannel.h"
#include "nsIPrincipal.h"
#include "nsContentUtils.h"  // for kLoadAsData
#include "nsThreadUtils.h"
#include "nsNetUtil.h"
#include "nsStreamUtils.h"
#include "ReferrerInfo.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::dom;

using mozilla::dom::ReferrerPolicy;

/**
 * This class manages loading a single XML document
 */

class nsSyncLoader : public nsIStreamListener,
                     public nsIChannelEventSink,
                     public nsIInterfaceRequestor,
                     public nsSupportsWeakReference {
 public:
  nsSyncLoader()
      : mLoading(false), mAsyncLoadStatus(NS_ERROR_NOT_INITIALIZED) {}

  NS_DECL_ISUPPORTS

  nsresult LoadDocument(nsIChannel* aChannel, bool aChannelIsSync,
                        bool aForceToXML, ReferrerPolicy aReferrerPolicy,
                        Document** aResult);

  NS_FORWARD_NSISTREAMLISTENER(mListener->)
  NS_DECL_NSIREQUESTOBSERVER

  NS_DECL_NSICHANNELEVENTSINK

  NS_DECL_NSIINTERFACEREQUESTOR

 private:
  virtual ~nsSyncLoader();

  nsresult PushAsyncStream(nsIStreamListener* aListener);
  nsresult PushSyncStream(nsIStreamListener* aListener);

  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsIStreamListener> mListener;
  bool mLoading;
  nsresult mAsyncLoadStatus;
};

class nsForceXMLListener : public nsIStreamListener {
  virtual ~nsForceXMLListener();

 public:
  explicit nsForceXMLListener(nsIStreamListener* aListener);

  NS_DECL_ISUPPORTS
  NS_FORWARD_NSISTREAMLISTENER(mListener->)
  NS_DECL_NSIREQUESTOBSERVER

 private:
  nsCOMPtr<nsIStreamListener> mListener;
};

nsForceXMLListener::nsForceXMLListener(nsIStreamListener* aListener)
    : mListener(aListener) {}

nsForceXMLListener::~nsForceXMLListener() = default;

NS_IMPL_ISUPPORTS(nsForceXMLListener, nsIStreamListener, nsIRequestObserver)

NS_IMETHODIMP
nsForceXMLListener::OnStartRequest(nsIRequest* aRequest) {
  nsresult status;
  aRequest->GetStatus(&status);
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
  if (channel && NS_SUCCEEDED(status)) {
    channel->SetContentType("text/xml"_ns);
  }

  return mListener->OnStartRequest(aRequest);
}

NS_IMETHODIMP
nsForceXMLListener::OnStopRequest(nsIRequest* aRequest, nsresult aStatusCode) {
  return mListener->OnStopRequest(aRequest, aStatusCode);
}

nsSyncLoader::~nsSyncLoader() {
  if (mLoading && mChannel) {
    mChannel->Cancel(NS_BINDING_ABORTED);
  }
}

NS_IMPL_ISUPPORTS(nsSyncLoader, nsIStreamListener, nsIRequestObserver,
                  nsIChannelEventSink, nsIInterfaceRequestor,
                  nsISupportsWeakReference)

nsresult nsSyncLoader::LoadDocument(nsIChannel* aChannel, bool aChannelIsSync,
                                    bool aForceToXML,
                                    ReferrerPolicy aReferrerPolicy,
                                    Document** aResult) {
  NS_ENSURE_ARG(aChannel);
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nullptr;
  nsresult rv = NS_OK;

  mChannel = aChannel;
  nsCOMPtr<nsIHttpChannel> http = do_QueryInterface(mChannel);
  if (http) {
    rv = http->SetRequestHeader(
        "Accept"_ns,
        nsLiteralCString(
            "text/xml,application/xml,application/xhtml+xml,*/*;q=0.1"),
        false);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
    nsCOMPtr<nsIReferrerInfo> referrerInfo;
    loadInfo->TriggeringPrincipal()->CreateReferrerInfo(
        aReferrerPolicy, getter_AddRefs(referrerInfo));
    if (referrerInfo) {
      rv = http->SetReferrerInfoWithoutClone(referrerInfo);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }

  // Hook us up to listen to redirects and the like.
  // Do this before setting up the cross-site proxy since
  // that installs its own proxies.
  mChannel->SetNotificationCallbacks(this);

  // Get the loadgroup of the channel
  nsCOMPtr<nsILoadGroup> loadGroup;
  rv = aChannel->GetLoadGroup(getter_AddRefs(loadGroup));
  NS_ENSURE_SUCCESS(rv, rv);

  // Create document
  nsCOMPtr<Document> document;
  rv = NS_NewXMLDocument(getter_AddRefs(document));
  NS_ENSURE_SUCCESS(rv, rv);

  // Start the document load. Do this before we attach the load listener
  // since we reset the document which drops all observers.
  nsCOMPtr<nsIStreamListener> listener;
  rv = document->StartDocumentLoad(kLoadAsData, mChannel, loadGroup, nullptr,
                                   getter_AddRefs(listener), true);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aForceToXML) {
    nsCOMPtr<nsIStreamListener> forceListener =
        new nsForceXMLListener(listener);
    listener.swap(forceListener);
  }

  if (aChannelIsSync) {
    rv = PushSyncStream(listener);
  } else {
    rv = PushAsyncStream(listener);
  }

  http = do_QueryInterface(mChannel);
  if (NS_SUCCEEDED(rv) && http) {
    bool succeeded;
    if (NS_FAILED(http->GetRequestSucceeded(&succeeded)) || !succeeded) {
      rv = NS_ERROR_FAILURE;
    }
  }
  mChannel = nullptr;

  // check that the load succeeded
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_TRUE(document->GetRootElement(), NS_ERROR_FAILURE);

  document.forget(aResult);

  return NS_OK;
}

nsresult nsSyncLoader::PushAsyncStream(nsIStreamListener* aListener) {
  mListener = aListener;

  mAsyncLoadStatus = NS_OK;

  // Start reading from the channel
  nsresult rv = mChannel->AsyncOpen(this);

  if (NS_SUCCEEDED(rv)) {
    // process events until we're finished.
    mLoading = true;
    nsIThread* thread = NS_GetCurrentThread();
    while (mLoading && NS_SUCCEEDED(rv)) {
      bool processedEvent;
      rv = thread->ProcessNextEvent(true, &processedEvent);
      if (NS_SUCCEEDED(rv) && !processedEvent) rv = NS_ERROR_UNEXPECTED;
    }
  }

  mListener = nullptr;

  NS_ENSURE_SUCCESS(rv, rv);

  // Note that if AsyncOpen failed that's ok -- the only caller of
  // this method nulls out mChannel immediately after we return.

  return mAsyncLoadStatus;
}

nsresult nsSyncLoader::PushSyncStream(nsIStreamListener* aListener) {
  nsCOMPtr<nsIInputStream> in;
  nsresult rv = mChannel->Open(getter_AddRefs(in));
  NS_ENSURE_SUCCESS(rv, rv);

  mLoading = true;
  rv = nsSyncLoadService::PushSyncStreamToListener(in.forget(), aListener,
                                                   mChannel);
  mLoading = false;

  return rv;
}

NS_IMETHODIMP
nsSyncLoader::OnStartRequest(nsIRequest* aRequest) {
  return mListener->OnStartRequest(aRequest);
}

NS_IMETHODIMP
nsSyncLoader::OnStopRequest(nsIRequest* aRequest, nsresult aStatusCode) {
  if (NS_SUCCEEDED(mAsyncLoadStatus) && NS_FAILED(aStatusCode)) {
    mAsyncLoadStatus = aStatusCode;
  }
  nsresult rv = mListener->OnStopRequest(aRequest, aStatusCode);
  if (NS_SUCCEEDED(mAsyncLoadStatus) && NS_FAILED(rv)) {
    mAsyncLoadStatus = rv;
  }
  mLoading = false;

  return rv;
}

NS_IMETHODIMP
nsSyncLoader::AsyncOnChannelRedirect(nsIChannel* aOldChannel,
                                     nsIChannel* aNewChannel, uint32_t aFlags,
                                     nsIAsyncVerifyRedirectCallback* callback) {
  MOZ_ASSERT(aNewChannel, "Redirecting to null channel?");

  mChannel = aNewChannel;

  callback->OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}

NS_IMETHODIMP
nsSyncLoader::GetInterface(const nsIID& aIID, void** aResult) {
  return QueryInterface(aIID, aResult);
}

/* static */
nsresult nsSyncLoadService::LoadDocument(
    nsIURI* aURI, nsContentPolicyType aContentPolicyType,
    nsIPrincipal* aLoaderPrincipal, nsSecurityFlags aSecurityFlags,
    nsILoadGroup* aLoadGroup, nsICookieJarSettings* aCookieJarSettings,
    bool aForceToXML, ReferrerPolicy aReferrerPolicy, Document** aResult) {
  nsCOMPtr<nsIChannel> channel;
  nsresult rv =
      NS_NewChannel(getter_AddRefs(channel), aURI, aLoaderPrincipal,
                    aSecurityFlags, aContentPolicyType, aCookieJarSettings,
                    nullptr,  // PerformanceStorage
                    aLoadGroup);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aForceToXML) {
    channel->SetContentType("text/xml"_ns);
  }

  // if the load needs to enforce CORS, then force the load to be async
  bool isSync =
      !(aSecurityFlags & nsILoadInfo::SEC_REQUIRE_CORS_INHERITS_SEC_CONTEXT) &&
      (aURI->SchemeIs("chrome") || aURI->SchemeIs("resource"));
  RefPtr<nsSyncLoader> loader = new nsSyncLoader();
  return loader->LoadDocument(channel, isSync, aForceToXML, aReferrerPolicy,
                              aResult);
}

/* static */
nsresult nsSyncLoadService::PushSyncStreamToListener(
    already_AddRefed<nsIInputStream> aIn, nsIStreamListener* aListener,
    nsIChannel* aChannel) {
  nsCOMPtr<nsIInputStream> in = std::move(aIn);

  // Set up buffering stream
  nsresult rv;
  nsCOMPtr<nsIInputStream> bufferedStream;
  if (!NS_InputStreamIsBuffered(in)) {
    int64_t chunkSize;
    rv = aChannel->GetContentLength(&chunkSize);
    if (NS_FAILED(rv) || chunkSize < 1) {
      chunkSize = 4096;
    }
    chunkSize = std::min(int64_t(UINT16_MAX), chunkSize);

    rv = NS_NewBufferedInputStream(getter_AddRefs(bufferedStream), in.forget(),
                                   chunkSize);
    NS_ENSURE_SUCCESS(rv, rv);

    in = bufferedStream;
  }

  // Load
  rv = aListener->OnStartRequest(aChannel);
  if (NS_SUCCEEDED(rv)) {
    uint64_t sourceOffset = 0;
    while (1) {
      uint64_t readCount = 0;
      rv = in->Available(&readCount);
      if (NS_FAILED(rv) || !readCount) {
        if (rv == NS_BASE_STREAM_CLOSED) {
          // End of file, but not an error
          rv = NS_OK;
        }
        break;
      }

      if (readCount > UINT32_MAX) readCount = UINT32_MAX;

      rv = aListener->OnDataAvailable(
          aChannel, in, (uint32_t)std::min(sourceOffset, (uint64_t)UINT32_MAX),
          (uint32_t)readCount);
      if (NS_FAILED(rv)) {
        break;
      }
      sourceOffset += readCount;
    }
  }
  if (NS_FAILED(rv)) {
    aChannel->Cancel(rv);
  }
  aListener->OnStopRequest(aChannel, rv);

  return rv;
}
