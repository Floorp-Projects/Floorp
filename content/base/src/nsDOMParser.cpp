/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "jsapi.h"
#include "nsDOMParser.h"
#include "nsIURI.h"
#include "nsIChannel.h"
#include "nsILoadGroup.h"
#include "nsIInputStream.h"
#include "nsNetUtil.h"
#include "nsStringStream.h"
#include "nsIXPConnect.h"
#include "nsIServiceManager.h"
#include "nsLayoutCID.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIDOMWindow.h"
#include "nsIPrivateDOMImplementation.h"
#include "nsIJSContextStack.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMClassInfo.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsIDOMEventReceiver.h"
#include "nsLoadListenerProxy.h"
#include "nsStreamUtils.h"
#include "nsThreadUtils.h"
#include "nsNetCID.h"
#include "nsContentUtils.h"
#include "nsDOMJSUtils.h"

static const char* kLoadAsData = "loadAsData";

static NS_DEFINE_CID(kIDOMDOMImplementationCID, NS_DOM_IMPLEMENTATION_CID);

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
  mLoopingForSyncLoad = PR_FALSE;

  return NS_OK;
}

nsresult
nsDOMParser::BeforeUnload(nsIDOMEvent* aEvent)
{
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
  mLoopingForSyncLoad = PR_FALSE;

  return NS_OK;
}

nsresult
nsDOMParser::Error(nsIDOMEvent* aEvent)
{
  mLoopingForSyncLoad = PR_FALSE;

  return NS_OK;
}

nsDOMParser::nsDOMParser()
  : mLoopingForSyncLoad(PR_FALSE)
{
}

nsDOMParser::~nsDOMParser()
{
  NS_ABORT_IF_FALSE(!mLoopingForSyncLoad, "we rather crash than hang");
  mLoopingForSyncLoad = PR_FALSE;
}


// QueryInterface implementation for nsDOMParser
NS_INTERFACE_MAP_BEGIN(nsDOMParser)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMParser)
  NS_INTERFACE_MAP_ENTRY(nsIDOMParser)
  NS_INTERFACE_MAP_ENTRY(nsIDOMLoadListener)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(DOMParser)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsDOMParser)
NS_IMPL_RELEASE(nsDOMParser)

NS_IMETHODIMP 
nsDOMParser::ParseFromString(const PRUnichar *str, 
                             const char *contentType,
                             nsIDOMDocument **aResult)
{
  NS_ENSURE_ARG(str);
  NS_ENSURE_ARG_POINTER(aResult);

  NS_ConvertUTF16toUTF8 data(str);

  // The new stream holds a reference to the buffer
  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = NS_NewByteInputStream(getter_AddRefs(stream),
                                      data.get(), data.Length(),
                                      NS_ASSIGNMENT_DEPEND);
  if (NS_FAILED(rv))
    return rv;

  return ParseFromStream(stream, "UTF-8", data.Length(), contentType, aResult);
}

NS_IMETHODIMP 
nsDOMParser::ParseFromBuffer(const PRUint8 *buf,
                             PRUint32 bufLen,
                             const char *contentType,
                             nsIDOMDocument **aResult)
{
  NS_ENSURE_ARG_POINTER(buf);
  NS_ENSURE_ARG_POINTER(aResult);

  // The new stream holds a reference to the buffer
  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = NS_NewByteInputStream(getter_AddRefs(stream),
                                      NS_REINTERPRET_CAST(const char *, buf),
                                      bufLen, NS_ASSIGNMENT_DEPEND);
  if (NS_FAILED(rv))
    return rv;

  return ParseFromStream(stream, nsnull, bufLen, contentType, aResult);
}


NS_IMETHODIMP 
nsDOMParser::ParseFromStream(nsIInputStream *stream, 
                             const char *charset, 
                             PRInt32 contentLength,
                             const char *contentType,
                             nsIDOMDocument **aResult)
{
  NS_ENSURE_ARG(stream);
  NS_ENSURE_ARG(contentType);
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nsnull;

  // For now, we can only create XML documents.
  if ((nsCRT::strcmp(contentType, "text/xml") != 0) &&
      (nsCRT::strcmp(contentType, "application/xml") != 0) &&
      (nsCRT::strcmp(contentType, "application/xhtml+xml") != 0))
    return NS_ERROR_NOT_IMPLEMENTED;

  // Put the nsCOMPtr out here so we hold a ref to the stream as needed
  nsresult rv;
  nsCOMPtr<nsIBufferedInputStream> bufferedStream;
  if (!NS_InputStreamIsBuffered(stream)) {
    bufferedStream = do_CreateInstance(NS_BUFFEREDINPUTSTREAM_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = bufferedStream->Init(stream, 4096);
    NS_ENSURE_SUCCESS(rv, rv);
    stream = bufferedStream;
  }
  
  nsCOMPtr<nsIPrincipal> principal;
  nsCOMPtr<nsIScriptSecurityManager> secMan = 
    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    secMan->GetSubjectPrincipal(getter_AddRefs(principal));
  }

  // Try to find a base URI for the document we're creating.
  nsCOMPtr<nsIURI> baseURI;

  nsCOMPtr<nsIXPCNativeCallContext> cc;
  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
  if(NS_SUCCEEDED(rv)) {
    rv = xpc->GetCurrentNativeCallContext(getter_AddRefs(cc));
  }

  if (NS_SUCCEEDED(rv) && cc) {
    JSContext* cx;
    rv = cc->GetJSContext(&cx);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    nsIScriptContext *scriptContext = GetScriptContextFromJSContext(cx);
    if (scriptContext) {
      nsCOMPtr<nsIDOMWindow> window =
        do_QueryInterface(scriptContext->GetGlobalObject());

      if (window) {
        nsCOMPtr<nsIDOMDocument> domdoc;
        window->GetDocument(getter_AddRefs(domdoc));

        nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);
        if (doc) {
          baseURI = doc->GetBaseURI();
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
      // XXXbz Is this really right?  Why are we setting the documentURI to
      // baseURI?  But note that's what the StartDocumentLoad() below would do
      // if we let it reset.  In any case, this is odd, since the caller can
      // set baseURI to anything it feels like, pretty much.
      privImpl->Init(baseURI, baseURI, principal);
    }
  }

  // Create an empty document from it
  nsCOMPtr<nsIDOMDocument> domDocument;
  rv = implementation->CreateDocument(EmptyString(),
                                      EmptyString(),
                                      nsnull,
                                      getter_AddRefs(domDocument));
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  // Register as a load listener on the document
  nsCOMPtr<nsIDOMEventReceiver> target(do_QueryInterface(domDocument));
  if (target) {
    nsWeakPtr requestWeak(do_GetWeakReference(NS_STATIC_CAST(nsIDOMParser*, this)));
    nsLoadListenerProxy* proxy = new nsLoadListenerProxy(requestWeak);
    if (!proxy) return NS_ERROR_OUT_OF_MEMORY;

    // This will addref the proxy
    rv = target->AddEventListenerByIID(NS_STATIC_CAST(nsIDOMEventListener*, 
                                                      proxy), 
                                       NS_GET_IID(nsIDOMLoadListener));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  }

  // Create a fake channel 
  nsCOMPtr<nsIChannel> parserChannel;
  NS_NewInputStreamChannel(getter_AddRefs(parserChannel), baseURI, nsnull,
                           nsDependentCString(contentType), nsnull);
  NS_ENSURE_STATE(parserChannel);

  // Hold a reference to it in this method
  if (principal) {
    parserChannel->SetOwner(principal);
  }

  if (charset) {
    parserChannel->SetContentCharset(nsDependentCString(charset));
  }

  // Tell the document to start loading
  nsCOMPtr<nsIStreamListener> listener;
  nsCOMPtr<nsIDocument> document(do_QueryInterface(domDocument));
  if (!document) return NS_ERROR_FAILURE;

  mLoopingForSyncLoad = PR_TRUE;

  // Have to pass PR_FALSE for reset here, else the reset will remove
  // our event listener.  Should that listener addition move to later
  // than this call?
  rv = document->StartDocumentLoad(kLoadAsData, parserChannel, 
                                   nsnull, nsnull, 
                                   getter_AddRefs(listener),
                                   PR_FALSE);

  if (principal) {
    // Make sure to give this document the right principal
    document->SetPrincipal(principal);
  }

  if (NS_FAILED(rv) || !listener) {
    return NS_ERROR_FAILURE;
  }

  // Now start pumping data to the listener
  nsresult status;

  rv = listener->OnStartRequest(parserChannel, nsnull);
  if (NS_FAILED(rv))
    parserChannel->Cancel(rv);
  parserChannel->GetStatus(&status);

  if (NS_SUCCEEDED(rv) && NS_SUCCEEDED(status)) {
    rv = listener->OnDataAvailable(parserChannel, nsnull, stream, 0,
                                   contentLength);
    if (NS_FAILED(rv))
      parserChannel->Cancel(rv);
    parserChannel->GetStatus(&status);
  }

  rv = listener->OnStopRequest(parserChannel, nsnull, status);
  // Failure returned from OnStopRequest does not affect the final status of
  // the channel, so we do not need to call Cancel(rv) as we do above.

  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }

  // Process events until we receive a load, abort, or error event for the
  // document object.  That event may have already fired.

  nsIThread *thread = NS_GetCurrentThread();
  while (mLoopingForSyncLoad) {
    if (!NS_ProcessNextEvent(thread))
      break;
  }

  domDocument.swap(*aResult);

  return NS_OK;
}

NS_IMETHODIMP 
nsDOMParser::GetBaseURI(nsIURI **aBaseURI)
{
  NS_ENSURE_ARG_POINTER(aBaseURI);

  NS_IF_ADDREF(*aBaseURI = mBaseURI);

  return NS_OK;
}

NS_IMETHODIMP 
nsDOMParser::SetBaseURI(nsIURI *aBaseURI)
{
  mBaseURI = aBaseURI;

  return NS_OK;
}
