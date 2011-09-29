/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * A service that provides methods for synchronously loading a DOM in various ways.
 */

#include "nsSyncLoadService.h"
#include "nsCOMPtr.h"
#include "nsIChannel.h"
#include "nsIChannelEventSink.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIInterfaceRequestor.h"
#include "nsString.h"
#include "nsWeakReference.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIScriptSecurityManager.h"
#include "nsContentUtils.h"
#include "nsThreadUtils.h"
#include "nsNetUtil.h"
#include "nsAutoPtr.h"
#include "nsStreamUtils.h"
#include "nsCrossSiteListenerProxy.h"

/**
 * This class manages loading a single XML document
 */

class nsSyncLoader : public nsIStreamListener,
                     public nsIChannelEventSink,
                     public nsIInterfaceRequestor,
                     public nsSupportsWeakReference
{
public:
    nsSyncLoader() : mLoading(PR_FALSE) {}
    virtual ~nsSyncLoader();

    NS_DECL_ISUPPORTS

    nsresult LoadDocument(nsIChannel* aChannel, nsIPrincipal *aLoaderPrincipal,
                          bool aChannelIsSync, bool aForceToXML,
                          nsIDOMDocument** aResult);

    NS_FORWARD_NSISTREAMLISTENER(mListener->)
    NS_DECL_NSIREQUESTOBSERVER

    NS_DECL_NSICHANNELEVENTSINK

    NS_DECL_NSIINTERFACEREQUESTOR

private:
    nsresult PushAsyncStream(nsIStreamListener* aListener);
    nsresult PushSyncStream(nsIStreamListener* aListener);

    nsCOMPtr<nsIChannel> mChannel;
    nsCOMPtr<nsIStreamListener> mListener;
    bool mLoading;
    nsresult mAsyncLoadStatus;
};

class nsForceXMLListener : public nsIStreamListener
{
public:
    nsForceXMLListener(nsIStreamListener* aListener);
    virtual ~nsForceXMLListener();

    NS_DECL_ISUPPORTS
    NS_FORWARD_NSISTREAMLISTENER(mListener->)
    NS_DECL_NSIREQUESTOBSERVER

private:
    nsCOMPtr<nsIStreamListener> mListener;
};

nsForceXMLListener::nsForceXMLListener(nsIStreamListener* aListener)
    : mListener(aListener)
{
}

nsForceXMLListener::~nsForceXMLListener()
{
}

NS_IMPL_ISUPPORTS2(nsForceXMLListener,
                   nsIStreamListener,
                   nsIRequestObserver)

NS_IMETHODIMP
nsForceXMLListener::OnStartRequest(nsIRequest *aRequest, nsISupports *aContext)
{
    nsresult status;
    aRequest->GetStatus(&status);
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
    if (channel && NS_SUCCEEDED(status)) {
      channel->SetContentType(NS_LITERAL_CSTRING("text/xml"));
    }

    return mListener->OnStartRequest(aRequest, aContext);
}

NS_IMETHODIMP
nsForceXMLListener::OnStopRequest(nsIRequest *aRequest, nsISupports *aContext,
                                  nsresult aStatusCode)
{
    return mListener->OnStopRequest(aRequest, aContext, aStatusCode);
}

nsSyncLoader::~nsSyncLoader()
{
    if (mLoading && mChannel) {
        mChannel->Cancel(NS_BINDING_ABORTED);
    }
}

NS_IMPL_ISUPPORTS5(nsSyncLoader,
                   nsIStreamListener,
                   nsIRequestObserver,
                   nsIChannelEventSink,
                   nsIInterfaceRequestor,
                   nsISupportsWeakReference)

nsresult
nsSyncLoader::LoadDocument(nsIChannel* aChannel,
                           nsIPrincipal *aLoaderPrincipal,
                           bool aChannelIsSync,
                           bool aForceToXML,
                           nsIDOMDocument **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = nsnull;
    nsresult rv = NS_OK;

    nsCOMPtr<nsIURI> loaderUri;
    if (aLoaderPrincipal) {
        aLoaderPrincipal->GetURI(getter_AddRefs(loaderUri));
    }

    mChannel = aChannel;
    nsCOMPtr<nsIHttpChannel> http = do_QueryInterface(mChannel);
    if (http) {
        http->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),     
                               NS_LITERAL_CSTRING("text/xml,application/xml,application/xhtml+xml,*/*;q=0.1"),
                               PR_FALSE);
        if (loaderUri) {
            http->SetReferrer(loaderUri);
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
    nsCOMPtr<nsIDocument> document;
    rv = NS_NewXMLDocument(getter_AddRefs(document));
    NS_ENSURE_SUCCESS(rv, rv);

    // Start the document load. Do this before we attach the load listener
    // since we reset the document which drops all observers.
    nsCOMPtr<nsIStreamListener> listener;
    rv = document->StartDocumentLoad(kLoadAsData, mChannel, 
                                     loadGroup, nsnull, 
                                     getter_AddRefs(listener),
                                     PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    if (aForceToXML) {
        nsCOMPtr<nsIStreamListener> forceListener =
            new nsForceXMLListener(listener);
        listener.swap(forceListener);
    }

    if (aLoaderPrincipal) {
        listener = new nsCORSListenerProxy(listener, aLoaderPrincipal,
                                           mChannel, PR_FALSE, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    if (aChannelIsSync) {
        rv = PushSyncStream(listener);
    }
    else {
        rv = PushAsyncStream(listener);
    }

    http = do_QueryInterface(mChannel);
    if (NS_SUCCEEDED(rv) && http) {
        bool succeeded;
        if (NS_FAILED(http->GetRequestSucceeded(&succeeded)) || !succeeded) {
            rv = NS_ERROR_FAILURE;
        }
    }
    mChannel = nsnull;

    // check that the load succeeded
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ENSURE_TRUE(document->GetRootElement(), NS_ERROR_FAILURE);

    return CallQueryInterface(document, aResult);
}

nsresult
nsSyncLoader::PushAsyncStream(nsIStreamListener* aListener)
{
    mListener = aListener;

    mAsyncLoadStatus = NS_OK;

    // Start reading from the channel
    nsresult rv = mChannel->AsyncOpen(this, nsnull);

    if (NS_SUCCEEDED(rv)) {
        // process events until we're finished.
        mLoading = PR_TRUE;
        nsIThread *thread = NS_GetCurrentThread();
        while (mLoading && NS_SUCCEEDED(rv)) {
            bool processedEvent; 
            rv = thread->ProcessNextEvent(PR_TRUE, &processedEvent);
            if (NS_SUCCEEDED(rv) && !processedEvent)
                rv = NS_ERROR_UNEXPECTED;
        }
    }

    mListener = nsnull;

    NS_ENSURE_SUCCESS(rv, rv);

    // Note that if AsyncOpen failed that's ok -- the only caller of
    // this method nulls out mChannel immediately after we return.

    return mAsyncLoadStatus;
}

nsresult
nsSyncLoader::PushSyncStream(nsIStreamListener* aListener)
{
    nsCOMPtr<nsIInputStream> in;
    nsresult rv = mChannel->Open(getter_AddRefs(in));
    NS_ENSURE_SUCCESS(rv, rv);

    mLoading = PR_TRUE;
    rv = nsSyncLoadService::PushSyncStreamToListener(in, aListener, mChannel);
    mLoading = PR_FALSE;
    
    return rv;
}

NS_IMETHODIMP
nsSyncLoader::OnStartRequest(nsIRequest *aRequest, nsISupports *aContext)
{
    return mListener->OnStartRequest(aRequest, aContext);
}

NS_IMETHODIMP
nsSyncLoader::OnStopRequest(nsIRequest *aRequest, nsISupports *aContext,
                            nsresult aStatusCode)
{
    if (NS_SUCCEEDED(mAsyncLoadStatus) && NS_FAILED(aStatusCode)) {
        mAsyncLoadStatus = aStatusCode;
    }
    nsresult rv = mListener->OnStopRequest(aRequest, aContext, aStatusCode);
    if (NS_SUCCEEDED(mAsyncLoadStatus) && NS_FAILED(rv)) {
        mAsyncLoadStatus = rv;
    }
    mLoading = PR_FALSE;

    return rv;
}

NS_IMETHODIMP
nsSyncLoader::AsyncOnChannelRedirect(nsIChannel *aOldChannel,
                                     nsIChannel *aNewChannel,
                                     PRUint32 aFlags,
                                     nsIAsyncVerifyRedirectCallback *callback)
{
    NS_PRECONDITION(aNewChannel, "Redirecting to null channel?");

    mChannel = aNewChannel;

    callback->OnRedirectVerifyCallback(NS_OK);
    return NS_OK;
}

NS_IMETHODIMP
nsSyncLoader::GetInterface(const nsIID & aIID,
                           void **aResult)
{
    return QueryInterface(aIID, aResult);
}

/* static */
nsresult
nsSyncLoadService::LoadDocument(nsIURI *aURI, nsIPrincipal *aLoaderPrincipal,
                                nsILoadGroup *aLoadGroup, bool aForceToXML,
                                nsIDOMDocument** aResult)
{
    nsCOMPtr<nsIChannel> channel;
    nsresult rv = NS_NewChannel(getter_AddRefs(channel), aURI, nsnull,
                                aLoadGroup);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!aForceToXML) {
        channel->SetContentType(NS_LITERAL_CSTRING("text/xml"));
    }

    bool isChrome = false, isResource = false;
    bool isSync = (NS_SUCCEEDED(aURI->SchemeIs("chrome", &isChrome)) &&
                     isChrome) ||
                    (NS_SUCCEEDED(aURI->SchemeIs("resource", &isResource)) &&
                     isResource);

    nsRefPtr<nsSyncLoader> loader = new nsSyncLoader();
    return loader->LoadDocument(channel, aLoaderPrincipal, isSync,
                                aForceToXML, aResult);

}

/* static */
nsresult
nsSyncLoadService::PushSyncStreamToListener(nsIInputStream* aIn,
                                            nsIStreamListener* aListener,
                                            nsIChannel* aChannel)
{
    // Set up buffering stream
    nsresult rv;
    nsCOMPtr<nsIInputStream> bufferedStream;
    if (!NS_InputStreamIsBuffered(aIn)) {
        PRInt32 chunkSize;
        rv = aChannel->GetContentLength(&chunkSize);
        if (NS_FAILED(rv)) {
            chunkSize = 4096;
        }
        chunkSize = NS_MIN(PRInt32(PR_UINT16_MAX), chunkSize);

        rv = NS_NewBufferedInputStream(getter_AddRefs(bufferedStream), aIn,
                                       chunkSize);
        NS_ENSURE_SUCCESS(rv, rv);

        aIn = bufferedStream;
    }

    // Load
    rv = aListener->OnStartRequest(aChannel, nsnull);
    if (NS_SUCCEEDED(rv)) {
        PRUint32 sourceOffset = 0;
        while (1) {
            PRUint32 readCount = 0;
            rv = aIn->Available(&readCount);
            if (NS_FAILED(rv) || !readCount) {
                if (rv == NS_BASE_STREAM_CLOSED) {
                    // End of file, but not an error
                    rv = NS_OK;
                }
                break;
            }

            rv = aListener->OnDataAvailable(aChannel, nsnull, aIn,
                                            sourceOffset, readCount);
            if (NS_FAILED(rv)) {
                break;
            }
            sourceOffset += readCount;
        }
    }
    if (NS_FAILED(rv)) {
        aChannel->Cancel(rv);
    }
    aListener->OnStopRequest(aChannel, nsnull, rv);

    return rv;
}
