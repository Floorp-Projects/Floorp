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

#include "nsCOMPtr.h"
#include "nsIChannel.h"
#include "nsIDOMLoadListener.h"
#include "nsIHttpEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIScriptContext.h"
#include "nsISyncLoadDOMService.h"
#include "nsString.h"
#include "nsWeakReference.h"
#include "jsapi.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIDOMEventReceiver.h"
#include "nsIEventQueueService.h"
#include "nsIJSContextStack.h"
#include "nsIPrivateDOMImplementation.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptSecurityManager.h"
#include "nsContentCID.h"
#include "nsContentUtils.h"
#include "nsNetUtil.h"
#include "nsIHttpChannel.h"
#include "nsIScriptLoader.h"
#include "nsIScriptLoaderObserver.h"
#include "nsIXMLContentSink.h"
#include "nsIContent.h"
#include "nsAutoPtr.h"

static const char kLoadAsData[] = "loadAsData";

static NS_DEFINE_CID(kXMLDocumentCID, NS_XMLDOCUMENT_CID);

// This is ugly, but nsXBLContentSink.h isn't exported
nsresult
NS_NewXBLContentSink(nsIXMLContentSink** aResult,
                     nsIDocument* aDoc,
                     nsIURI* aURL,
                     nsISupports* aContainer);

class nsSyncLoadService : public nsISyncLoadDOMService
{
public:
    nsSyncLoadService();
    virtual ~nsSyncLoadService();
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSISYNCLOADDOMSERVICE

    static
    nsresult PushSyncStreamToListener(nsIInputStream* aIn,
                                      nsIStreamListener* aListener,
                                      nsIChannel* aChannel);
};

/**
 * This class manages loading a single XML document
 */

class nsSyncLoader : public nsIDOMLoadListener,
                     public nsIHttpEventSink,
                     public nsIInterfaceRequestor,
                     public nsSupportsWeakReference
{
public:
    nsSyncLoader();
    virtual ~nsSyncLoader();

    NS_DECL_ISUPPORTS

    nsresult LoadDocument(nsIChannel* aChannel, nsIURI *aLoaderURI,
                          PRBool aChannelIsSync, PRBool aForceToXML,
                          nsIDOMDocument** aResult);

    // nsIDOMEventListener
    NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

    // nsIDOMLoadListener
    NS_IMETHOD Load(nsIDOMEvent* aEvent);
    NS_IMETHOD BeforeUnload(nsIDOMEvent* aEvent);
    NS_IMETHOD Unload(nsIDOMEvent* aEvent);
    NS_IMETHOD Abort(nsIDOMEvent* aEvent);
    NS_IMETHOD Error(nsIDOMEvent* aEvent);

    NS_DECL_NSIHTTPEVENTSINK

    NS_DECL_NSIINTERFACEREQUESTOR

protected:
    nsresult PushAsyncStream(nsIStreamListener* aListener);
    nsresult PushSyncStream(nsIStreamListener* aListener);

    nsCOMPtr<nsIChannel> mChannel;
    PRPackedBool mLoading;
    PRPackedBool mLoadSuccess;
};


/*
 * This class exists to prevent a circular reference between
 * the loaded document and the nsSyncLoader instance. The
 * request owns the document. While the document is loading, 
 * the request is a load listener, held onto by the document.
 * The proxy class breaks the circularity by filling in as the
 * load listener and holding a weak reference to the request
 * object.
 */

class txLoadListenerProxy : public nsIDOMLoadListener {
public:
    txLoadListenerProxy(nsWeakPtr aParent);
    virtual ~txLoadListenerProxy();

    NS_DECL_ISUPPORTS

    // nsIDOMEventListener
    NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

    // nsIDOMLoadListener
    NS_IMETHOD Load(nsIDOMEvent* aEvent);
    NS_IMETHOD BeforeUnload(nsIDOMEvent* aEvent);
    NS_IMETHOD Unload(nsIDOMEvent* aEvent);
    NS_IMETHOD Abort(nsIDOMEvent* aEvent);
    NS_IMETHOD Error(nsIDOMEvent* aEvent);

protected:
    nsWeakPtr  mParent;
};

txLoadListenerProxy::txLoadListenerProxy(nsWeakPtr aParent)
{
    mParent = aParent;
}

txLoadListenerProxy::~txLoadListenerProxy()
{
}

NS_IMPL_ISUPPORTS1(txLoadListenerProxy, nsIDOMLoadListener)

NS_IMETHODIMP
txLoadListenerProxy::HandleEvent(nsIDOMEvent* aEvent)
{
    nsCOMPtr<nsIDOMLoadListener> listener = do_QueryReferent(mParent);

    if (listener) {
        return listener->HandleEvent(aEvent);
    }

    return NS_OK;
}

NS_IMETHODIMP
txLoadListenerProxy::Load(nsIDOMEvent* aEvent)
{
    nsCOMPtr<nsIDOMLoadListener> listener = do_QueryReferent(mParent);

    if (listener) {
        return listener->Load(aEvent);
    }

    return NS_OK;
}

NS_IMETHODIMP
txLoadListenerProxy::BeforeUnload(nsIDOMEvent* aEvent)
{
    nsCOMPtr<nsIDOMLoadListener> listener = do_QueryReferent(mParent);

    if (listener) {
        return listener->BeforeUnload(aEvent);
    }

    return NS_OK;
}

NS_IMETHODIMP
txLoadListenerProxy::Unload(nsIDOMEvent* aEvent)
{
    nsCOMPtr<nsIDOMLoadListener> listener = do_QueryReferent(mParent);

    if (listener) {
        return listener->Unload(aEvent);
    }

    return NS_OK;
}

NS_IMETHODIMP
txLoadListenerProxy::Abort(nsIDOMEvent* aEvent)
{
    nsCOMPtr<nsIDOMLoadListener> listener = do_QueryReferent(mParent);

    if (listener) {
        return listener->Abort(aEvent);
    }

    return NS_OK;
}

NS_IMETHODIMP
txLoadListenerProxy::Error(nsIDOMEvent* aEvent)
{
    nsCOMPtr<nsIDOMLoadListener> listener = do_QueryReferent(mParent);

    if (listener) {
        return listener->Error(aEvent);
    }

    return NS_OK;
}

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

nsSyncLoader::nsSyncLoader()
{
}

nsSyncLoader::~nsSyncLoader()
{
    if (mLoading && mChannel) {
        mChannel->Cancel(NS_BINDING_ABORTED);
    }
}

NS_IMPL_ISUPPORTS4(nsSyncLoader,
                   nsIDOMLoadListener,
                   nsIHttpEventSink,
                   nsIInterfaceRequestor,
                   nsISupportsWeakReference)

nsresult
nsSyncLoader::LoadDocument(nsIChannel* aChannel,
                           nsIURI *aLoaderURI,
                           PRBool aChannelIsSync,
                           PRBool aForceToXML,
                           nsIDOMDocument **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = nsnull;
    nsresult rv = NS_OK;

    mChannel = aChannel;

    if (aLoaderURI) {
        nsCOMPtr<nsIURI> docURI;
        rv = aChannel->GetOriginalURI(getter_AddRefs(docURI));
        NS_ENSURE_SUCCESS(rv, rv);

        nsIScriptSecurityManager *securityManager =
            nsContentUtils::GetSecurityManager();

        rv = securityManager->CheckLoadURI(aLoaderURI, docURI,
                                           nsIScriptSecurityManager::STANDARD);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = securityManager->CheckSameOriginURI(aLoaderURI, docURI);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    // Get the loadgroup of the channel
    nsCOMPtr<nsILoadGroup> loadGroup;
    rv = aChannel->GetLoadGroup(getter_AddRefs(loadGroup));
    NS_ENSURE_SUCCESS(rv, rv);

    // Create document
    nsCOMPtr<nsIDocument> document = do_CreateInstance(kXMLDocumentCID, &rv);
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

    // Register as a load listener on the document
    nsCOMPtr<nsIDOMEventReceiver> target = do_QueryInterface(document);
    NS_ENSURE_TRUE(target, NS_ERROR_FAILURE);

    nsWeakPtr requestWeak = do_GetWeakReference(NS_STATIC_CAST(nsIDOMLoadListener*, this));
    txLoadListenerProxy* proxy = new txLoadListenerProxy(requestWeak);
    NS_ENSURE_TRUE(proxy, NS_ERROR_OUT_OF_MEMORY);

    // This will addref the proxy
    rv = target->AddEventListenerByIID(NS_STATIC_CAST(nsIDOMEventListener*, 
                                                      proxy), 
                                       NS_GET_IID(nsIDOMLoadListener));
    NS_ENSURE_SUCCESS(rv, rv);
    
    mLoadSuccess = PR_FALSE;
    if (aChannelIsSync) {
        rv = PushSyncStream(listener);
    }
    else {
        rv = PushAsyncStream(listener);
    }

    mChannel = nsnull;

    // This will release the proxy. Don't use the errorvalue from this since
    // we're more interested in the errorvalue from the loading
    target->RemoveEventListenerByIID(NS_STATIC_CAST(nsIDOMEventListener*, 
                                                    proxy), 
                                     NS_GET_IID(nsIDOMLoadListener));

    // check that the load succeeded
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ENSURE_TRUE(mLoadSuccess, NS_ERROR_FAILURE);

    NS_ENSURE_TRUE(document->GetRootContent(), NS_ERROR_FAILURE);

    return CallQueryInterface(document, aResult);
}

nsresult
nsSyncLoader::PushAsyncStream(nsIStreamListener* aListener)
{
    nsresult rv = NS_OK;

    // Set up a new eventqueue
    nsCOMPtr<nsIEventQueueService> service = 
        do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIEventQueue> currentThreadQ;
    rv = service->PushThreadEventQueue(getter_AddRefs(currentThreadQ));
    NS_ENSURE_SUCCESS(rv, rv);

    // Hook us up to listen to redirects and the like
    mChannel->SetNotificationCallbacks(this);

    // Start reading from the channel
    rv = mChannel->AsyncOpen(aListener, nsnull);

    if (NS_SUCCEEDED(rv)) {
        mLoading = PR_TRUE;

        // process events until we're finished.
        PLEvent *event;
        while (mLoading && NS_SUCCEEDED(rv)) {
            rv = currentThreadQ->WaitForEvent(&event);
            NS_ASSERTION(NS_SUCCEEDED(rv), ": currentThreadQ->WaitForEvent failed...\n");
            if (NS_SUCCEEDED(rv)) {
                rv = currentThreadQ->HandleEvent(event);
                NS_ASSERTION(NS_SUCCEEDED(rv), ": currentThreadQ->HandleEvent failed...\n");
            }
        }
    }

    service->PopThreadEventQueue(currentThreadQ);
    
    return rv;
}

nsresult
nsSyncLoader::PushSyncStream(nsIStreamListener* aListener)
{
    nsCOMPtr<nsIInputStream> in;
    nsresult rv = mChannel->Open(getter_AddRefs(in));
    NS_ENSURE_SUCCESS(rv, rv);

    mLoading = PR_TRUE;
    rv = nsSyncLoadService::PushSyncStreamToListener(in, aListener, mChannel);

    return rv;
}

// nsIDOMEventListener
nsresult
nsSyncLoader::HandleEvent(nsIDOMEvent* aEvent)
{
    return NS_OK;
}

// nsIDOMLoadListener
nsresult
nsSyncLoader::Load(nsIDOMEvent* aEvent)
{
    if (mLoading) {
        mLoading = PR_FALSE;
        mLoadSuccess = PR_TRUE;
    }

    return NS_OK;
}

nsresult
nsSyncLoader::BeforeUnload(nsIDOMEvent* aEvent)
{
    // Like, whatever.

    return NS_OK;
}

nsresult
nsSyncLoader::Unload(nsIDOMEvent* aEvent)
{
    return NS_OK;
}

nsresult
nsSyncLoader::Abort(nsIDOMEvent* aEvent)
{
    if (mLoading) {
        mLoading = PR_FALSE;
    }

    return NS_OK;
}

nsresult
nsSyncLoader::Error(nsIDOMEvent* aEvent)
{
    if (mLoading) {
        mLoading = PR_FALSE;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsSyncLoader::OnRedirect(nsIHttpChannel *aHttpChannel,
                         nsIChannel *aNewChannel)
{
    NS_ENSURE_ARG_POINTER(aNewChannel);

    nsCOMPtr<nsIURI> oldURI;
    nsresult rv = aHttpChannel->GetURI(getter_AddRefs(oldURI)); // The original URI
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIURI> newURI;
    rv = aNewChannel->GetURI(getter_AddRefs(newURI)); // The new URI
    NS_ENSURE_SUCCESS(rv, rv);

    rv = nsContentUtils::GetSecurityManager()->CheckSameOriginURI(oldURI, newURI);

    NS_ENSURE_SUCCESS(rv, rv);

    mChannel = aNewChannel;

    return NS_OK;
}

NS_IMETHODIMP
nsSyncLoader::GetInterface(const nsIID & aIID,
                           void **aResult)
{
    return QueryInterface(aIID, aResult);
}

nsSyncLoadService::nsSyncLoadService()
{
}

nsSyncLoadService::~nsSyncLoadService()
{
}

NS_IMPL_ISUPPORTS1(nsSyncLoadService,
                   nsISyncLoadDOMService)

NS_IMETHODIMP
nsSyncLoadService::LoadDocument(nsIChannel* aChannel, nsIURI* aLoaderURI,
                                nsIDOMDocument** aResult)
{
    nsRefPtr<nsSyncLoader> loader = new nsSyncLoader();
    NS_ENSURE_TRUE(loader, NS_ERROR_OUT_OF_MEMORY);

    return loader->LoadDocument(aChannel, aLoaderURI, PR_FALSE, PR_FALSE,
                                aResult);
}

NS_IMETHODIMP
nsSyncLoadService::LoadDocumentAsXML(nsIChannel* aChannel, nsIURI* aLoaderURI,
                                     nsIDOMDocument** aResult)
{
    nsRefPtr<nsSyncLoader> loader = new nsSyncLoader();
    NS_ENSURE_TRUE(loader, NS_ERROR_OUT_OF_MEMORY);

    return loader->LoadDocument(aChannel, aLoaderURI, PR_FALSE, PR_TRUE,
                                aResult);
}

nsresult
nsSyncLoadService::LoadLocalDocument(nsIChannel* aChannel, nsIURI* aLoaderURI,
                                     nsIDOMDocument** _retval)
{
    nsSyncLoader* loader = new nsSyncLoader();
    NS_ENSURE_TRUE(loader, NS_ERROR_OUT_OF_MEMORY);
    NS_ADDREF(loader);
    nsresult rv = loader->LoadDocument(aChannel, aLoaderURI, PR_TRUE, PR_TRUE,
                                       _retval);
    NS_RELEASE(loader);
    return rv;
}

nsresult
nsSyncLoadService::LoadLocalXBLDocument(nsIChannel* aChannel,
                                        nsIDOMDocument** _retval)
{
    *_retval = nsnull;

    nsCOMPtr<nsIInputStream> in;
    nsresult rv = aChannel->Open(getter_AddRefs(in));
    if (NS_FAILED(rv)) {
        return rv;
    }

    // Get uri and loadgroup
    nsCOMPtr<nsIURI> docURI;
    rv = aChannel->GetURI(getter_AddRefs(docURI));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsILoadGroup> loadGroup;
    rv = aChannel->GetLoadGroup(getter_AddRefs(loadGroup));
    NS_ENSURE_SUCCESS(rv, rv);

    // Create document and contentsink and set them up.
    nsCOMPtr<nsIDocument> doc = do_CreateInstance(kXMLDocumentCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIXMLContentSink> xblSink;
    rv = NS_NewXBLContentSink(getter_AddRefs(xblSink), doc, docURI, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIStreamListener> listener;
    rv = doc->StartDocumentLoad("loadAsInteractiveData",
                                aChannel,
                                loadGroup,
                                nsnull,
                                getter_AddRefs(listener),
                                PR_TRUE,
                                xblSink);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = PushSyncStreamToListener(in, listener, aChannel);

    NS_ENSURE_SUCCESS(rv, rv);

    return CallQueryInterface(doc, _retval);
}

// static
nsresult
nsSyncLoadService::PushSyncStreamToListener(nsIInputStream* aIn,
                                            nsIStreamListener* aListener,
                                            nsIChannel* aChannel)
{
    // Set up buffering stream
    nsCOMPtr<nsIInputStream> bufferedStream;
    nsresult rv = NS_NewBufferedInputStream(getter_AddRefs(bufferedStream),
                                            aIn, 4096);
    NS_ENSURE_SUCCESS(rv, rv);

    // Load
    aListener->OnStartRequest(aChannel, nsnull);
    PRUint32 sourceOffset = 0;
    while (1) {
        PRUint32 readCount = 0;
        rv = bufferedStream->Available(&readCount);
        if (NS_FAILED(rv) || !readCount) {
            break;
        }

        rv = aListener->OnDataAvailable(aChannel, nsnull, bufferedStream,
                                        sourceOffset, readCount);
        if (NS_FAILED(rv)) {
            break;
        }

        sourceOffset += readCount;
    }
    aListener->OnStopRequest(aChannel, nsnull, rv);
    
    return rv;
}

nsresult
NS_NewSyncLoadDOMService(nsISyncLoadDOMService** aResult)
{
    *aResult = new nsSyncLoadService();
    NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);
    NS_ADDREF(*aResult);
    return NS_OK;
}
