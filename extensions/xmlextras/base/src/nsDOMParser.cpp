/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "nsDOMParser.h"
#include "nsIURI.h"
#include "nsIChannel.h"
#include "nsILoadGroup.h"
#include "nsIInputStream.h"
#include "nsNetUtil.h"
#include "nsIByteArrayInputStream.h"
#include "nsIXPConnect.h"
#include "nsIUnicodeEncoder.h"
#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsLayoutCID.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIPrivateDOMImplementation.h"
#include "nsIJSContextStack.h"
#include "nsIScriptSecurityManager.h"
#include "nsICodebasePrincipal.h"
#include "nsIDOMClassInfo.h"

#ifdef IMPLEMENT_SYNC_LOAD
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIEventQueueService.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDOMEventReceiver.h"
#include "jsapi.h"
#include "nsLoadListenerProxy.h"
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
#endif

static const char* kLoadAsData = "loadAsData";

static NS_DEFINE_CID(kIDOMDOMImplementationCID, NS_DOM_IMPLEMENTATION_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

/////////////////////////////////////////////
//
//
/////////////////////////////////////////////

class nsDOMParserChannel : public nsIChannel {
public:
  nsDOMParserChannel(nsIURI* aURI, const char* aContentType);
  virtual ~nsDOMParserChannel();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUEST
  NS_DECL_NSICHANNEL

protected:
  nsCString mContentType;
  nsresult mStatus;
  PRInt32 mContentLength;
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsISupports> mOwner;
  nsCOMPtr<nsILoadGroup> mLoadGroup;
};

nsDOMParserChannel::nsDOMParserChannel(nsIURI* aURI, const char* aContentType)
{
  NS_INIT_ISUPPORTS();
  mURI = aURI;
  mContentType.Assign(aContentType);
  mStatus = NS_OK;
  mContentLength = -1;
}

nsDOMParserChannel::~nsDOMParserChannel()
{
}

NS_IMPL_ISUPPORTS2(nsDOMParserChannel, 
                   nsIChannel,
                   nsIRequest)

/* boolean isPending (); */
NS_IMETHODIMP nsDOMParserChannel::GetName(PRUnichar* *result)
{
    NS_NOTREACHED("nsDOMParserChannel::GetName");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsDOMParserChannel::IsPending(PRBool *_retval)
{
  NS_ENSURE_ARG(_retval);
  *_retval = PR_FALSE;
  return NS_OK;
}

/* readonly attribute nsresult status; */
NS_IMETHODIMP 
nsDOMParserChannel::GetStatus(nsresult *aStatus)
{
  NS_ENSURE_ARG(aStatus);
  *aStatus = mStatus;
  return NS_OK;
}

/* void cancel (in nsresult status); */
NS_IMETHODIMP 
nsDOMParserChannel::Cancel(nsresult status)
{
  mStatus = status;
  return NS_OK;
}

/* void suspend (); */
NS_IMETHODIMP 
nsDOMParserChannel::Suspend()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void resume (); */
NS_IMETHODIMP 
nsDOMParserChannel::Resume()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute nsIURI originalURI; */
NS_IMETHODIMP 
nsDOMParserChannel::GetOriginalURI(nsIURI * *aOriginalURI)
{
  NS_ENSURE_ARG_POINTER(aOriginalURI);
  *aOriginalURI = mURI;
  NS_ADDREF(*aOriginalURI);
  return NS_OK;
}
NS_IMETHODIMP nsDOMParserChannel::SetOriginalURI(nsIURI * aOriginalURI)
{
  mURI = aOriginalURI;
  return NS_OK;
}

/* attribute nsIURI URI; */
NS_IMETHODIMP nsDOMParserChannel::GetURI(nsIURI * *aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);
  *aURI = mURI;
  NS_ADDREF(*aURI);
  return NS_OK;
}

/* attribute string contentType; */
NS_IMETHODIMP nsDOMParserChannel::GetContentType(char * *aContentType)
{
  NS_ENSURE_ARG_POINTER(aContentType);
  *aContentType = mContentType.ToNewCString();
  return NS_OK;
}
NS_IMETHODIMP nsDOMParserChannel::SetContentType(const char * aContentType)
{
  NS_ENSURE_ARG(aContentType);
  mContentType.Assign(aContentType);
  return NS_OK;
}

/* attribute long contentLength; */
NS_IMETHODIMP nsDOMParserChannel::GetContentLength(PRInt32 *aContentLength)
{
  NS_ENSURE_ARG(aContentLength);
  *aContentLength = mContentLength;
  return NS_OK;
}
NS_IMETHODIMP nsDOMParserChannel::SetContentLength(PRInt32 aContentLength)
{
  mContentLength = aContentLength;
  return NS_OK;
}

/* attribute nsISupports owner; */
NS_IMETHODIMP nsDOMParserChannel::GetOwner(nsISupports * *aOwner)
{
  NS_ENSURE_ARG_POINTER(aOwner);
  *aOwner = mOwner;
  NS_IF_ADDREF(*aOwner);
  return NS_OK;
}
NS_IMETHODIMP nsDOMParserChannel::SetOwner(nsISupports * aOwner)
{
  mOwner = aOwner;
  return NS_OK;
}

/* attribute nsLoadFlags loadFlags; */
NS_IMETHODIMP nsDOMParserChannel::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsDOMParserChannel::SetLoadFlags(nsLoadFlags aLoadFlags)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute nsILoadGroup loadGroup; */
NS_IMETHODIMP nsDOMParserChannel::GetLoadGroup(nsILoadGroup * *aLoadGroup)
{
  NS_ENSURE_ARG_POINTER(aLoadGroup);
  *aLoadGroup = mLoadGroup;
  NS_IF_ADDREF(*aLoadGroup);
  return NS_OK;
}
NS_IMETHODIMP nsDOMParserChannel::SetLoadGroup(nsILoadGroup * aLoadGroup)
{
  mLoadGroup = aLoadGroup;
  return NS_OK;
}

/* attribute nsIInterfaceRequestor notificationCallbacks; */
NS_IMETHODIMP nsDOMParserChannel::GetNotificationCallbacks(nsIInterfaceRequestor * *aNotificationCallbacks)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsDOMParserChannel::SetNotificationCallbacks(nsIInterfaceRequestor * aNotificationCallbacks)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsISupports securityInfo; */
NS_IMETHODIMP nsDOMParserChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsDOMParserChannel::Open(nsIInputStream **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsDOMParserChannel::AsyncOpen(nsIStreamListener *listener, nsISupports *ctxt)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/////////////////////////////////////////////
//
//
/////////////////////////////////////////////

#ifdef IMPLEMENT_SYNC_LOAD
// See if we have a modal event loop
static inline PRBool IsModal(nsIWebBrowserChrome *aWindow)
{
  if (aWindow) {
    PRBool isModal;
    if (NS_SUCCEEDED(aWindow->IsWindowModal(&isModal))) {
      return isModal;
    }
  }

  return PR_FALSE;
}

// nsIDOMEventListener
nsresult
nsDOMParser::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

// nsIDOMLoadListener
nsresult
nsDOMParser::Load(nsIDOMEvent* aEvent)
{
  if (IsModal(mChromeWindow)) {
    mChromeWindow->ExitModalEventLoop(NS_OK);
  }

  mChromeWindow = nsnull;

  return NS_OK;
}

nsresult
nsDOMParser::Unload(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

nsresult
nsDOMParser::Abort(nsIDOMEvent* aEvent)
{
  if (IsModal(mChromeWindow)) {
    mChromeWindow->ExitModalEventLoop(NS_OK);
  }

  mChromeWindow = nsnull;

  return NS_OK;
}

nsresult
nsDOMParser::Error(nsIDOMEvent* aEvent)
{
  if (IsModal(mChromeWindow)) {
    mChromeWindow->ExitModalEventLoop(NS_OK);
  }

  mChromeWindow = nsnull;

  return NS_OK;
}
#endif

nsDOMParser::nsDOMParser()
{
  NS_INIT_ISUPPORTS();
}

nsDOMParser::~nsDOMParser()
{
#ifdef IMPLEMENT_SYNC_LOAD
  if (IsModal(mChromeWindow)) {
    mChromeWindow->ExitModalEventLoop(NS_OK);
  }
#endif
}


// QueryInterface implementation for nsDOMParser
NS_INTERFACE_MAP_BEGIN(nsDOMParser)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMParser)
  NS_INTERFACE_MAP_ENTRY(nsIDOMParser)
#ifdef IMPLEMENT_SYNC_LOAD
  NS_INTERFACE_MAP_ENTRY(nsIDOMLoadListener)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
#endif
  NS_INTERFACE_MAP_ENTRY_DOM_CLASSINFO(DOMParser)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsDOMParser)
NS_IMPL_RELEASE(nsDOMParser)

static nsresult
ConvertWStringToStream(const PRUnichar* aStr,
                       PRInt32 aLength,
                       nsIInputStream** aStream,
                       PRInt32* aContentLength)
{
  nsresult rv;
  nsCOMPtr<nsIUnicodeEncoder> encoder;
  nsAutoString charsetStr;
  char* charBuf;

  // We want to encode the string as utf-8, so get the right encoder
  nsCOMPtr<nsICharsetConverterManager> charsetConv = 
           do_GetService(kCharsetConverterManagerCID, &rv);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
  
  charsetStr.AssignWithConversion("UTF-8");
  rv = charsetConv->GetUnicodeEncoder(&charsetStr,
                                      getter_AddRefs(encoder));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
  
  // Convert to utf-8
  PRInt32 charLength;
  const PRUnichar* unicodeBuf = aStr;
  PRInt32 unicodeLength = aLength;
    
  rv = encoder->GetMaxLength(unicodeBuf, unicodeLength, &charLength);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  
  charBuf = (char*)nsMemory::Alloc(charLength + 1);
  if (!charBuf) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  rv = encoder->Convert(unicodeBuf, 
                        &unicodeLength, 
                        charBuf, 
                        &charLength);
  if (NS_FAILED(rv)) {
    nsMemory::Free(charBuf);
    return NS_ERROR_FAILURE;
  }

  // The new stream takes ownership of the buffer
  rv = NS_NewByteArrayInputStream((nsIByteArrayInputStream**)aStream, 
                                  charBuf, 
                                  charLength);
  if (NS_FAILED(rv)) {
    nsMemory::Free(charBuf);
    return NS_ERROR_FAILURE;
  }
  
  *aContentLength = charLength;

  return NS_OK;
}

/* nsIDOMDocument parseFromString (in wstring str, in string contentType); */
NS_IMETHODIMP 
nsDOMParser::ParseFromString(const PRUnichar *str, 
                             const char *contentType, 
                             nsIDOMDocument **_retval)
{
  NS_ENSURE_ARG(str);
  NS_ENSURE_ARG(contentType);
  NS_ENSURE_ARG_POINTER(_retval);

  nsCOMPtr<nsIInputStream> stream;
  PRInt32 contentLength;

  nsresult rv = ConvertWStringToStream(str, nsCRT::strlen(str), getter_AddRefs(stream), &contentLength);
  if (NS_FAILED(rv)) {
    *_retval = nsnull;
    return rv;
  }

  return ParseFromStream(stream, "UTF-8", contentLength, contentType, _retval);
}


/* nsIDOMDocument parseFromStream (in nsIInputStream stream, in string charset, in string contentType); */
NS_IMETHODIMP 
nsDOMParser::ParseFromStream(nsIInputStream *stream, 
                             const char *charset, 
                             PRInt32 contentLength,
                             const char *contentType, 
                             nsIDOMDocument **_retval)
{
  NS_ENSURE_ARG(stream);
  NS_ENSURE_ARG(charset);
  NS_ENSURE_ARG(contentType);
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;

  nsresult rv;
  nsCOMPtr<nsIURI> baseURI;
  nsCOMPtr<nsIPrincipal> principal;

  // For now, we can only create XML documents.
  if (nsCRT::strcmp(contentType, "text/xml") != 0 &&
    nsCRT::strcmp(contentType, "application/xml") != 0 &&
    nsCRT::strcmp(contentType, "application/xhtml+xml") != 0) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // First try to find a base URI for the document we're creating
  nsCOMPtr<nsIXPCNativeCallContext> cc;
  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
  if(NS_SUCCEEDED(rv)) {
    rv = xpc->GetCurrentNativeCallContext(getter_AddRefs(cc));
  }

  if (NS_SUCCEEDED(rv) && cc) {
    JSContext* cx;
    rv = cc->GetJSContext(&cx);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  
    nsCOMPtr<nsIScriptSecurityManager> secMan = 
             do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
      rv = secMan->GetSubjectPrincipal(getter_AddRefs(principal));
      if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsICodebasePrincipal> codebase(do_QueryInterface(principal));
        if (codebase) {
          codebase->GetURI(getter_AddRefs(baseURI));
        }
      }
    }
  }

  if (!baseURI) {
    // No URI from script environment (we are running from command line, for example).
    // Create a dummy one.
    // XXX Is this safe? Could we get the URI from stream or something?
    if (!mBaseURI) {
      rv = NS_NewURI(getter_AddRefs(baseURI),
                     "about:blank" );
      if (NS_FAILED(rv)) return rv;    
    } else {
      baseURI = mBaseURI;
    }
  }

  // Get and initialize a DOMImplementation
  nsCOMPtr<nsIDOMDOMImplementation> implementation(do_CreateInstance(kIDOMDOMImplementationCID, &rv));
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  
  if (baseURI) {
    nsCOMPtr<nsIPrivateDOMImplementation> privImpl(do_QueryInterface(implementation));
    if (privImpl) {
      privImpl->Init(baseURI);
    }
  }

  // Create an empty document from it
  nsCOMPtr<nsIDOMDocument> domDocument;
  nsAutoString emptyStr;
  rv = implementation->CreateDocument(emptyStr, 
                                      emptyStr, 
                                      nsnull, 
                                      getter_AddRefs(domDocument));
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

#ifdef IMPLEMENT_SYNC_LOAD
  // Register as a load listener on the document
  nsCOMPtr<nsIDOMEventReceiver> target(do_QueryInterface(domDocument));
  if (target) {
    nsWeakPtr requestWeak(getter_AddRefs(NS_GetWeakReference(NS_STATIC_CAST(nsIDOMParser*, this))));
    nsLoadListenerProxy* proxy = new nsLoadListenerProxy(requestWeak);
    if (!proxy) return NS_ERROR_OUT_OF_MEMORY;

    // This will addref the proxy
    rv = target->AddEventListenerByIID(NS_STATIC_CAST(nsIDOMEventListener*, 
                                                      proxy), 
                                       NS_GET_IID(nsIDOMLoadListener));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  }
#endif

  // Create a fake channel 
  nsDOMParserChannel* parserChannel = new nsDOMParserChannel(baseURI, contentType);
  if (!parserChannel) return NS_ERROR_OUT_OF_MEMORY;

  // Hold a reference to it in this method
  nsCOMPtr<nsIChannel> channel = NS_STATIC_CAST(nsIChannel*, parserChannel);
  if (principal) {
    channel->SetOwner(principal);
  }
  nsCOMPtr<nsIRequest> request = NS_STATIC_CAST(nsIRequest*, parserChannel);

  // Tell the document to start loading
  nsCOMPtr<nsIStreamListener> listener;
  nsCOMPtr<nsIDocument> document(do_QueryInterface(domDocument));
  if (!document) return NS_ERROR_FAILURE;

#ifdef IMPLEMENT_SYNC_LOAD
  nsCOMPtr<nsIEventQueue> modalEventQueue;
  nsCOMPtr<nsIEventQueueService> eventQService;
  
  if (cc) {
    JSContext* cx;
    rv = cc->GetJSContext(&cx);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    // We can only do this if we're called from a DOM script context
    nsIScriptContext* scriptCX = (nsIScriptContext*)JS_GetContextPrivate(cx);
    if (!scriptCX) return NS_OK;

    // Get the nsIDocShellTreeOwner associated with the window
    // containing this script context
    // XXX Need to find a better way to do this rather than
    // chaining through a bunch of getters and QIs
    nsCOMPtr<nsIScriptGlobalObject> global;
    scriptCX->GetGlobalObject(getter_AddRefs(global));
    if (!global) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDocShell> docshell;
    rv = global->GetDocShell(getter_AddRefs(docshell));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    
    nsCOMPtr<nsIDocShellTreeItem> item = do_QueryInterface(docshell);
    if (!item) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
    rv = item->GetTreeOwner(getter_AddRefs(treeOwner));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIInterfaceRequestor> treeRequestor(do_GetInterface(treeOwner));
    if (!treeRequestor) return NS_ERROR_FAILURE;

    treeRequestor->GetInterface(NS_GET_IID(nsIWebBrowserChrome), getter_AddRefs(mChromeWindow));
    if (!mChromeWindow) return NS_ERROR_FAILURE;

    eventQService = do_GetService(kEventQueueServiceCID);
    if(!eventQService || 
       NS_FAILED(eventQService->PushThreadEventQueue(getter_AddRefs(modalEventQueue)))) {
      return NS_ERROR_FAILURE;
    }
  }
#endif

  rv = document->StartDocumentLoad(kLoadAsData, channel, 
                                   nsnull, nsnull, 
                                   getter_AddRefs(listener),
                                   PR_FALSE);

#ifdef IMPLEMENT_SYNC_LOAD
  if (NS_FAILED(rv) || !listener) {
    if (modalEventQueue) {
      eventQService->PopThreadEventQueue(modalEventQueue);
    }
    return NS_ERROR_FAILURE;
  }
#else
  if (NS_FAILED(rv) || !listener) return NS_ERROR_FAILURE;
#endif

  // Now start pumping data to the listener
  nsresult status;

  rv = listener->OnStartRequest(request, nsnull);
  request->GetStatus(&status);

  if (NS_SUCCEEDED(rv) && NS_SUCCEEDED(status)) {
    rv = listener->OnDataAvailable(request, nsnull, stream, 0, contentLength);
    request->GetStatus(&status);
  }

  rv = listener->OnStopRequest(request, nsnull, status);
#ifdef IMPLEMENT_SYNC_LOAD
  if (NS_FAILED(rv)) {
    if (modalEventQueue) {
      eventQService->PopThreadEventQueue(modalEventQueue);
    }
    return NS_ERROR_FAILURE;
  }
#else
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
#endif

#ifdef IMPLEMENT_SYNC_LOAD
  // Spin an event loop here and wait
  if (mChromeWindow) {
    rv = mChromeWindow->ShowAsModal();
    
    eventQService->PopThreadEventQueue(modalEventQueue);
    
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;      
  } else if (modalEventQueue) {
    eventQService->PopThreadEventQueue(modalEventQueue);
  }
#endif

  *_retval = domDocument;
  NS_ADDREF(*_retval);

  return NS_OK;
}

NS_IMETHODIMP 
nsDOMParser::GetBaseURI(nsIURI **aBaseURI)
{
  NS_ENSURE_ARG_POINTER(aBaseURI);
  *aBaseURI = mBaseURI;
  NS_IF_ADDREF(*aBaseURI);
  return NS_OK;
}

NS_IMETHODIMP 
nsDOMParser::SetBaseURI(nsIURI *aBaseURI)
{
  mBaseURI = aBaseURI;
  return NS_OK;
}
