/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Peter Van der Beken, peterv@netscape.com
 *    -- original author.
 *
 */

#include "nsSyncLoader.h"
#include "nsNetUtil.h"
#include "nsLayoutCID.h"
#include "nsIEventQueueService.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIDOMEventReceiver.h"
#include "nsIXPConnect.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindowInternal.h"
#include "nsAppShellCIDs.h"
#include "nsIAppShellService.h"

static const char* kLoadAsData = "loadAsData";

static NS_DEFINE_CID(kIDOMDOMImplementationCID, NS_DOM_IMPLEMENTATION_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);

/*
 * This class exists to prevent a circular reference between
 * the loaded document and the nsSyncloader instance. The
 * request owns the document. While the document is loading, 
 * the request is a load listener, held onto by the document.
 * The proxy class breaks the circularity by filling in as the
 * load listener and holding a weak reference to the request
 * object.
 */

class nsLoadListenerProxy : public nsIDOMLoadListener {
public:
    nsLoadListenerProxy(nsWeakPtr aParent);
    virtual ~nsLoadListenerProxy();

    NS_DECL_ISUPPORTS

    // nsIDOMEventListener
    virtual nsresult HandleEvent(nsIDOMEvent* aEvent);

    // nsIDOMLoadListener
    virtual nsresult Load(nsIDOMEvent* aEvent);
    virtual nsresult Unload(nsIDOMEvent* aEvent);
    virtual nsresult Abort(nsIDOMEvent* aEvent);
    virtual nsresult Error(nsIDOMEvent* aEvent);

protected:
    nsWeakPtr  mParent;
};

nsLoadListenerProxy::nsLoadListenerProxy(nsWeakPtr aParent)
{
    NS_INIT_ISUPPORTS();
    mParent = aParent;
}

nsLoadListenerProxy::~nsLoadListenerProxy()
{
}

NS_IMPL_ISUPPORTS1(nsLoadListenerProxy, nsIDOMLoadListener)

nsresult
nsLoadListenerProxy::HandleEvent(nsIDOMEvent* aEvent)
{
    nsCOMPtr<nsIDOMLoadListener> listener = do_QueryReferent(mParent);

    if (listener) {
        return listener->HandleEvent(aEvent);
    }
  
    return NS_OK;
}

nsresult 
nsLoadListenerProxy::Load(nsIDOMEvent* aEvent)
{
    nsCOMPtr<nsIDOMLoadListener> listener = do_QueryReferent(mParent);

    if (listener) {
        return listener->Load(aEvent);
    }

    return NS_OK;
}

nsresult 
nsLoadListenerProxy::Unload(nsIDOMEvent* aEvent)
{
    nsCOMPtr<nsIDOMLoadListener> listener = do_QueryReferent(mParent);

    if (listener) {
        return listener->Unload(aEvent);
    }
  
    return NS_OK;
}

nsresult 
nsLoadListenerProxy::Abort(nsIDOMEvent* aEvent)
{
    nsCOMPtr<nsIDOMLoadListener> listener = do_QueryReferent(mParent);

    if (listener) {
        return listener->Abort(aEvent);
    }
  
    return NS_OK;
}

nsresult 
nsLoadListenerProxy::Error(nsIDOMEvent* aEvent)
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
    //if (XML_HTTP_REQUEST_SENT == mStatus) {
    //    Abort();
    //}
    if (mDocShellTreeOwner) {
        mDocShellTreeOwner->ExitModalLoop(NS_OK);
    }
}

NS_IMPL_ISUPPORTS3(nsSyncLoader, nsISyncLoader, nsIDOMLoadListener, nsISupportsWeakReference)

NS_IMETHODIMP
nsSyncLoader::LoadDocument(nsIURI* documentURI, nsIDOMDocument **_retval)
{
    nsresult rv = NS_OK;

    nsCOMPtr<nsILoadGroup> loadGroup;

    // If we have a base document, use it for the base URL and loadgroup
    //if (mBaseDocument) {
    //    rv = mBaseDocument->GetDocumentLoadGroup(getter_AddRefs(loadGroup));
    //    if (NS_FAILED(rv)) return rv;
    //}

    //rv = NS_NewURI(getter_AddRefs(uri), url, mBaseURI);
    //if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIChannel> channel;
    rv = NS_OpenURI(getter_AddRefs(channel), documentURI, nsnull, loadGroup);
    if (NS_FAILED(rv)) return rv;
  
    nsCOMPtr<nsIInputStream> postDataStream;

    // Make sure we've been opened
    if (!channel) {
        return NS_ERROR_NOT_INITIALIZED;
    }

    // Get and initialize a DOMImplementation
    nsCOMPtr<nsIDOMDOMImplementation> implementation = do_CreateInstance(kIDOMDOMImplementationCID, &rv);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  
    //if (mBaseURI) {
    //    nsCOMPtr<nsIPrivateDOMImplementation> privImpl = do_QueryInterface(implementation);
    //    if (privImpl) {
    //        privImpl->Init(mBaseURI);
    //    }
    //}

    // Create an empty document from it
    nsAutoString emptyStr;
    nsCOMPtr<nsIDOMDocument> DOMDocument;
    rv = implementation->CreateDocument(emptyStr, 
                                        emptyStr, 
                                        nsnull, 
                                        getter_AddRefs(DOMDocument));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    // Register as a load listener on the document
    nsCOMPtr<nsIDOMEventReceiver> target = do_QueryInterface(DOMDocument);
    if (target) {
        nsWeakPtr requestWeak = getter_AddRefs(NS_GetWeakReference(NS_STATIC_CAST(nsIDOMLoadListener*, this)));
        nsLoadListenerProxy* proxy = new nsLoadListenerProxy(requestWeak);
        if (!proxy) return NS_ERROR_OUT_OF_MEMORY;

        // This will addref the proxy
        rv = target->AddEventListenerByIID(NS_STATIC_CAST(nsIDOMEventListener*, 
                                                          proxy), 
                                                          NS_GET_IID(nsIDOMLoadListener));
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    }

    // Tell the document to start loading
    nsCOMPtr<nsIStreamListener> listener;
    nsCOMPtr<nsIDocument> document = do_QueryInterface(DOMDocument);
    if (!document) {
        return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIEventQueue> modalEventQueue;
    nsCOMPtr<nsIEventQueueService> eventQService;
  
    nsCOMPtr<nsIXPCNativeCallContext> cc;
    NS_WITH_SERVICE(nsIXPConnect, xpc, nsIXPConnect::GetCID(), &rv);
    if(NS_SUCCEEDED(rv)) {
        rv = xpc->GetCurrentNativeCallContext(getter_AddRefs(cc));
    }

    JSContext* cx;
    if (NS_SUCCEEDED(rv) && cc) {
        rv = cc->GetJSContext(&cx);
        if (NS_FAILED(rv)) NS_ERROR_FAILURE;
    }
    else {
        NS_WITH_SERVICE(nsIAppShellService, appshellSvc, kAppShellServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;
        nsCOMPtr<nsIDOMWindowInternal> junk;
        rv = appshellSvc->GetHiddenWindowAndJSContext(getter_AddRefs(junk), &cx);
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    }
    if (NS_SUCCEEDED(rv)) {
        nsIScriptContext* scriptCX;

        // We can only do this if we're called from a DOM script context
        scriptCX = (nsIScriptContext*)JS_GetContextPrivate(cx);
        if (!scriptCX) return NS_OK;

        // Get the nsIDocShellTreeOwner associated with the window
        // containing this script context
        // XXX Need to find a better way to do this rather than
        // chaining through a bunch of getters and QIs
        nsCOMPtr<nsIScriptGlobalObject> global;
        global = dont_AddRef(scriptCX->GetGlobalObject());
        if (!global) return NS_ERROR_FAILURE;

        nsCOMPtr<nsIDocShell> docshell;
        rv = global->GetDocShell(getter_AddRefs(docshell));
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
      
        nsCOMPtr<nsIDocShellTreeItem> item = do_QueryInterface(docshell);
        if (!item) return NS_ERROR_FAILURE;

        rv = item->GetTreeOwner(getter_AddRefs(mDocShellTreeOwner));
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

        eventQService = do_GetService(kEventQueueServiceCID);
        if(!eventQService || 
           NS_FAILED(eventQService->PushThreadEventQueue(getter_AddRefs(modalEventQueue)))) {
            return NS_ERROR_FAILURE;
        }
    }

    rv = document->StartDocumentLoad(kLoadAsData, channel, 
                                     nsnull, nsnull, 
                                     getter_AddRefs(listener),
                                     PR_FALSE);

    if (NS_FAILED(rv)) {
        if (modalEventQueue) {
            eventQService->PopThreadEventQueue(modalEventQueue);
        }
        return NS_ERROR_FAILURE;
    }

    // Start reading from the channel
    rv = channel->AsyncOpen(listener, nsnull);

    if (NS_FAILED(rv)) {
        if (modalEventQueue) {
            eventQService->PopThreadEventQueue(modalEventQueue);
        }
        return NS_ERROR_FAILURE;
    }  

    // Spin an event loop here and wait
    if (mDocShellTreeOwner) {
        rv = mDocShellTreeOwner->ShowModal();
    
        eventQService->PopThreadEventQueue(modalEventQueue);
    
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;      
    }

    *_retval = DOMDocument;
    NS_ADDREF(*_retval);
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
    if (mDocShellTreeOwner) {
        mDocShellTreeOwner->ExitModalLoop(NS_OK);
        mDocShellTreeOwner = 0;
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
    if (mDocShellTreeOwner) {
        mDocShellTreeOwner->ExitModalLoop(NS_OK);
        mDocShellTreeOwner = 0;
    }

    return NS_OK;
}

nsresult
nsSyncLoader::Error(nsIDOMEvent* aEvent)
{
    if (mDocShellTreeOwner) {
        mDocShellTreeOwner->ExitModalLoop(NS_OK);
        mDocShellTreeOwner = 0;
    }

    return NS_OK;
}
