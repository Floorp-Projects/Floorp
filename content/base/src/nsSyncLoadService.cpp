/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsSyncLoader.h"
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
#include "nsNetUtil.h"
#include "nsIHttpChannel.h"

static const char* kLoadAsData = "loadAsData";

static NS_DEFINE_CID(kIDOMDOMImplementationCID, NS_DOM_IMPLEMENTATION_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

/*
 * This class exists to prevent a circular reference between
 * the loaded document and the nsSyncloader instance. The
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
    NS_IMETHOD Unload(nsIDOMEvent* aEvent);
    NS_IMETHOD Abort(nsIDOMEvent* aEvent);
    NS_IMETHOD Error(nsIDOMEvent* aEvent);

protected:
    nsWeakPtr  mParent;
};

txLoadListenerProxy::txLoadListenerProxy(nsWeakPtr aParent)
{
    NS_INIT_ISUPPORTS();
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

nsSyncLoader::nsSyncLoader()
{
    NS_INIT_ISUPPORTS();
}

nsSyncLoader::~nsSyncLoader()
{
    if (mLoading && mChannel) {
        mChannel->Cancel(NS_BINDING_ABORTED);
    }
}

NS_IMPL_ISUPPORTS5(nsSyncLoader,
                   nsISyncLoader,
                   nsIDOMLoadListener,
                   nsIHttpEventSink,
                   nsIInterfaceRequestor,
                   nsISupportsWeakReference)

NS_IMETHODIMP
nsSyncLoader::LoadDocument(nsIURI* aDocumentURI,
                           nsIDocument *aLoader,
                           nsIDOMDocument **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = nsnull;

    nsCOMPtr<nsIURI> loaderURI;
    aLoader->GetDocumentURL(getter_AddRefs(loaderURI));

    nsresult rv = NS_OK;
    nsCOMPtr<nsIScriptSecurityManager> securityManager = 
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = securityManager->CheckLoadURI(loaderURI, aDocumentURI,
                                       nsIScriptSecurityManager::STANDARD);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = securityManager->CheckSameOriginURI(loaderURI, aDocumentURI);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsILoadGroup> loadGroup;
    rv = aLoader->GetDocumentLoadGroup(getter_AddRefs(loadGroup));
    NS_ENSURE_SUCCESS(rv, rv);

    // Get and initialize a DOMImplementation
    nsCOMPtr<nsIDOMDOMImplementation> implementation = 
        do_CreateInstance(kIDOMDOMImplementationCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPrivateDOMImplementation> privImplementation(do_QueryInterface(implementation, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
    privImplementation->Init(aDocumentURI);

    // Create an empty document from it
    nsString emptyStr;
    nsCOMPtr<nsIDOMDocument> DOMDocument;
    rv = implementation->CreateDocument(emptyStr, 
                                        emptyStr, 
                                        nsnull, 
                                        getter_AddRefs(DOMDocument));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = NS_NewChannel(getter_AddRefs(mChannel), aDocumentURI, nsnull, loadGroup);
    NS_ENSURE_SUCCESS(rv, rv);

    // Make sure we've been opened
    if (!mChannel) {
        return NS_ERROR_NOT_INITIALIZED;
    }

    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mChannel));
    if (httpChannel) {
        httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                      NS_LITERAL_CSTRING(""));
        httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                      NS_LITERAL_CSTRING("text/xml,application/xml,application/xhtml+xml,*/*;q=0.1"));

        // XXX need to set the HTTP referrer
    }

    // Tell the document to start loading
    nsCOMPtr<nsIDocument> document = do_QueryInterface(DOMDocument, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIEventQueueService> service = 
        do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIEventQueue> currentThreadQ;
    rv = service->PushThreadEventQueue(getter_AddRefs(currentThreadQ));
    NS_ENSURE_SUCCESS(rv, rv);

    // Register as a load listener on the document
    nsCOMPtr<nsIDOMEventReceiver> target = do_QueryInterface(DOMDocument);
    NS_ENSURE_TRUE(target, NS_ERROR_FAILURE);

    nsWeakPtr requestWeak = getter_AddRefs(NS_GetWeakReference(NS_STATIC_CAST(nsIDOMLoadListener*, this)));
    txLoadListenerProxy* proxy = new txLoadListenerProxy(requestWeak);
    if (!proxy) {
        service->PopThreadEventQueue(currentThreadQ);
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // This will addref the proxy
    rv = target->AddEventListenerByIID(NS_STATIC_CAST(nsIDOMEventListener*, 
                                                      proxy), 
                                       NS_GET_IID(nsIDOMLoadListener));
    if (NS_FAILED(rv)) {
        service->PopThreadEventQueue(currentThreadQ);
        return rv;
    }
    
    mLoadSuccess = PR_FALSE;

    nsCOMPtr<nsIStreamListener> listener;
    rv = document->StartDocumentLoad(kLoadAsData, mChannel, 
                                     loadGroup, nsnull, 
                                     getter_AddRefs(listener),
                                     PR_FALSE);

    if (NS_SUCCEEDED(rv)) {
        // Hook us up to listen to redirects and the like
        mChannel->SetNotificationCallbacks(this);

        // Start reading from the channel
        rv = mChannel->AsyncOpen(listener, nsnull);

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
    }

    mChannel = 0;

    if (NS_FAILED(rv)) {
        service->PopThreadEventQueue(currentThreadQ);
        // This will release the proxy
        target->RemoveEventListenerByIID(NS_STATIC_CAST(nsIDOMEventListener*, 
                                                        proxy), 
                                         NS_GET_IID(nsIDOMLoadListener));
        return rv;
    }

    // This will release the proxy
    rv = target->RemoveEventListenerByIID(NS_STATIC_CAST(nsIDOMEventListener*, 
                                                         proxy), 
                                          NS_GET_IID(nsIDOMLoadListener));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMElement> documentElement;
    DOMDocument->GetDocumentElement(getter_AddRefs(documentElement));
    if (mLoadSuccess && documentElement) {
        *aResult = DOMDocument;
        NS_ADDREF(*aResult);
    }

    rv = service->PopThreadEventQueue(currentThreadQ);

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

    nsresult rv = NS_ERROR_FAILURE;

    nsCOMPtr<nsIScriptSecurityManager> secMan =
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIURI> oldURI;
    rv = aHttpChannel->GetURI(getter_AddRefs(oldURI)); // The original URI
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIURI> newURI;
    rv = aNewChannel->GetURI(getter_AddRefs(newURI)); // The new URI
    NS_ENSURE_SUCCESS(rv, rv);

    rv = secMan->CheckSameOriginURI(oldURI, newURI);

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
