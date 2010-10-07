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
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsIDOMClassInfo.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsLoadListenerProxy.h"
#include "nsStreamUtils.h"
#include "nsThreadUtils.h"
#include "nsNetCID.h"
#include "nsContentUtils.h"
#include "nsDOMJSUtils.h"
#include "nsDOMError.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "mozilla/AutoRestore.h"

using namespace mozilla;

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
  : mLoopingForSyncLoad(PR_FALSE),
    mAttemptedInit(PR_FALSE)
{
}

nsDOMParser::~nsDOMParser()
{
  NS_ABORT_IF_FALSE(!mLoopingForSyncLoad, "we rather crash than hang");
  mLoopingForSyncLoad = PR_FALSE;
}

DOMCI_DATA(DOMParser, nsDOMParser)

// QueryInterface implementation for nsDOMParser
NS_INTERFACE_MAP_BEGIN(nsDOMParser)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMParser)
  NS_INTERFACE_MAP_ENTRY(nsIDOMParser)
  NS_INTERFACE_MAP_ENTRY(nsIDOMParserJS)
  NS_INTERFACE_MAP_ENTRY(nsIDOMLoadListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DOMParser)
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
                                      reinterpret_cast<const char *>(buf),
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

  nsCOMPtr<nsIScriptGlobalObject> scriptHandlingObject =
    do_QueryReferent(mScriptHandlingObject);
  nsresult rv;
  if (!mPrincipal) {
    NS_ENSURE_TRUE(!mAttemptedInit, NS_ERROR_NOT_INITIALIZED);
    AttemptedInitMarker marker(&mAttemptedInit);
    
    nsCOMPtr<nsIPrincipal> prin =
      do_CreateInstance("@mozilla.org/nullprincipal;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    rv = Init(prin, nsnull, nsnull, scriptHandlingObject);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_ASSERTION(mPrincipal, "Must have principal by now");
  NS_ASSERTION(mDocumentURI, "Must have document URI by now");
  
  // Put the nsCOMPtr out here so we hold a ref to the stream as needed
  nsCOMPtr<nsIInputStream> bufferedStream;
  if (!NS_InputStreamIsBuffered(stream)) {
    rv = NS_NewBufferedInputStream(getter_AddRefs(bufferedStream), stream,
                                   4096);
    NS_ENSURE_SUCCESS(rv, rv);

    stream = bufferedStream;
  }

  // Here we have to cheat a little bit...  Setting the base URI won't
  // work if the document has a null principal, so use
  // mOriginalPrincipal when creating the document, then reset the
  // principal.
  nsCOMPtr<nsIDOMDocument> domDocument;
  rv = nsContentUtils::CreateDocument(EmptyString(), EmptyString(), nsnull,
                                      mDocumentURI, mBaseURI,
                                      mOriginalPrincipal,
                                      scriptHandlingObject,
                                      getter_AddRefs(domDocument));
  NS_ENSURE_SUCCESS(rv, rv);

  // Register as a load listener on the document
  nsCOMPtr<nsPIDOMEventTarget> target(do_QueryInterface(domDocument));
  if (target) {
    nsWeakPtr requestWeak(do_GetWeakReference(static_cast<nsIDOMParser*>(this)));
    nsLoadListenerProxy* proxy = new nsLoadListenerProxy(requestWeak);
    if (!proxy) return NS_ERROR_OUT_OF_MEMORY;

    // This will addref the proxy
    rv = target->AddEventListenerByIID(static_cast<nsIDOMEventListener*>(proxy), 
                                       NS_GET_IID(nsIDOMLoadListener));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Create a fake channel 
  nsCOMPtr<nsIChannel> parserChannel;
  NS_NewInputStreamChannel(getter_AddRefs(parserChannel), mDocumentURI, nsnull,
                           nsDependentCString(contentType), nsnull);
  NS_ENSURE_STATE(parserChannel);

  // More principal-faking here 
  parserChannel->SetOwner(mOriginalPrincipal);

  if (charset) {
    parserChannel->SetContentCharset(nsDependentCString(charset));
  }

  // Tell the document to start loading
  nsCOMPtr<nsIStreamListener> listener;

  AutoRestore<PRPackedBool> restoreSyncLoop(mLoopingForSyncLoad);
  mLoopingForSyncLoad = PR_TRUE;

  // Have to pass PR_FALSE for reset here, else the reset will remove
  // our event listener.  Should that listener addition move to later
  // than this call?  Then we wouldn't need to mess around with
  // SetPrincipal, etc, probably!
  nsCOMPtr<nsIDocument> document(do_QueryInterface(domDocument));
  if (!document) return NS_ERROR_FAILURE;

  if (nsContentUtils::IsSystemPrincipal(mOriginalPrincipal)) {
    document->ForceEnableXULXBL();
  }

  rv = document->StartDocumentLoad(kLoadAsData, parserChannel, 
                                   nsnull, nsnull, 
                                   getter_AddRefs(listener),
                                   PR_FALSE);

  // Make sure to give this document the right base URI
  document->SetBaseURI(mBaseURI);

  // And the right principal
  document->SetPrincipal(mPrincipal);

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
nsDOMParser::Init(nsIPrincipal* principal, nsIURI* documentURI,
                  nsIURI* baseURI, nsIScriptGlobalObject* aScriptObject)
{
  NS_ENSURE_STATE(!mAttemptedInit);
  mAttemptedInit = PR_TRUE;
  
  NS_ENSURE_ARG(principal || documentURI);

  mDocumentURI = documentURI;
  
  if (!mDocumentURI) {
    principal->GetURI(getter_AddRefs(mDocumentURI));
    // If we have the system principal, then we'll just use the null principals
    // uri.
    if (!mDocumentURI && !nsContentUtils::IsSystemPrincipal(principal)) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  mScriptHandlingObject = do_GetWeakReference(aScriptObject);
  mPrincipal = principal;
  nsresult rv;
  if (!mPrincipal) {
    nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
    NS_ENSURE_TRUE(secMan, NS_ERROR_NOT_AVAILABLE);
    rv =
      secMan->GetCodebasePrincipal(mDocumentURI, getter_AddRefs(mPrincipal));
    NS_ENSURE_SUCCESS(rv, rv);
    mOriginalPrincipal = mPrincipal;
  } else {
    mOriginalPrincipal = mPrincipal;
    if (nsContentUtils::IsSystemPrincipal(mPrincipal)) {
      // Don't give DOMParsers the system principal.  Use a null
      // principal instead.
      mPrincipal = do_CreateInstance("@mozilla.org/nullprincipal;1", &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      if (!mDocumentURI) {
        rv = mPrincipal->GetURI(getter_AddRefs(mDocumentURI));
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }
  
  mBaseURI = baseURI;
  // Note: if mBaseURI is null, fine.  Leave it like that; that will use the
  // documentURI as the base.  Otherwise for null principals we'll get
  // nsDocument::SetBaseURI giving errors.

  NS_POSTCONDITION(mPrincipal, "Must have principal");
  NS_POSTCONDITION(mOriginalPrincipal, "Must have original principal");
  NS_POSTCONDITION(mDocumentURI, "Must have document URI");
  return NS_OK;
}
  
static nsQueryInterface
JSvalToInterface(JSContext* cx, jsval val, nsIXPConnect* xpc, PRBool* wasNull)
{
  if (val == JSVAL_NULL) {
    *wasNull = PR_TRUE;
    return nsQueryInterface(nsnull);
  }
  
  *wasNull = PR_FALSE;
  if (JSVAL_IS_OBJECT(val)) {
    JSObject* arg = JSVAL_TO_OBJECT(val);

    nsCOMPtr<nsIXPConnectWrappedNative> native;
    xpc->GetWrappedNativeOfJSObject(cx, arg, getter_AddRefs(native));

    // do_QueryWrappedNative is not null-safe
    if (native) {
      return do_QueryWrappedNative(native);
    }
  }
  
  return nsQueryInterface(nsnull);
}

static nsresult
GetInitArgs(JSContext *cx, PRUint32 argc, jsval *argv,
            nsIPrincipal** aPrincipal, nsIURI** aDocumentURI,
            nsIURI** aBaseURI)
{
  // Only proceed if the caller has UniversalXPConnect.
  PRBool haveUniversalXPConnect;
  nsresult rv = nsContentUtils::GetSecurityManager()->
    IsCapabilityEnabled("UniversalXPConnect", &haveUniversalXPConnect);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!haveUniversalXPConnect) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }    
  
  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  
  // First arg is our principal.  If someone passes something that's
  // not a principal and not null, die to prevent privilege escalation.
  PRBool wasNull;
  nsCOMPtr<nsIPrincipal> prin = JSvalToInterface(cx, argv[0], xpc, &wasNull);
  if (!prin && !wasNull) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIURI> documentURI;
  nsCOMPtr<nsIURI> baseURI;
  if (argc > 1) {
    // Grab our document URI too.  Again, if it's something unexpected bail
    // out.
    documentURI = JSvalToInterface(cx, argv[1], xpc, &wasNull);
    if (!documentURI && !wasNull) {
      return NS_ERROR_INVALID_ARG;
    }

    if (argc > 2) {
      // Grab our base URI as well
      baseURI = JSvalToInterface(cx, argv[2], xpc, &wasNull);
      if (!baseURI && !wasNull) {
        return NS_ERROR_INVALID_ARG;
      }
    }
  }

  NS_IF_ADDREF(*aPrincipal = prin);
  NS_IF_ADDREF(*aDocumentURI = documentURI);
  NS_IF_ADDREF(*aBaseURI = baseURI);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMParser::Initialize(nsISupports* aOwner, JSContext* cx, JSObject* obj,
                        PRUint32 argc, jsval *argv)
{
  AttemptedInitMarker marker(&mAttemptedInit);
  nsCOMPtr<nsIPrincipal> prin;
  nsCOMPtr<nsIURI> documentURI;
  nsCOMPtr<nsIURI> baseURI;
  if (argc > 0) {
    nsresult rv = GetInitArgs(cx, argc, argv, getter_AddRefs(prin),
                              getter_AddRefs(documentURI),
                              getter_AddRefs(baseURI));
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // No arguments; use the subject principal
    nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
    NS_ENSURE_TRUE(secMan, NS_ERROR_UNEXPECTED);

    secMan->GetSubjectPrincipal(getter_AddRefs(prin));

    // We're called from JS; there better be a subject principal, really.
    NS_ENSURE_TRUE(prin, NS_ERROR_UNEXPECTED);
  }

  NS_ASSERTION(prin, "Must have principal by now");
  
  if (!documentURI) {
    // No explicit documentURI; grab document and base URIs off the window our
    // constructor was called on. Error out if anything untoward happens.

    // Note that this is a behavior change as far as I can tell -- we're now
    // using the base URI and document URI of the window off of which the
    // DOMParser is created, not the window in which parse*() is called.
    // Does that matter?

    // Also note that |cx| matches what GetDocumentFromContext() would return,
    // while GetDocumentFromCaller() gives us the window that the DOMParser()
    // call was made on.

    nsCOMPtr<nsIDocument> doc;
    nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aOwner);
    if (aOwner) {
      nsCOMPtr<nsIDOMDocument> domdoc = window->GetExtantDocument();
      doc = do_QueryInterface(domdoc);
    }

    if (!doc) {
      return NS_ERROR_UNEXPECTED;
    }

    baseURI = doc->GetDocBaseURI();
    documentURI = doc->GetDocumentURI();
  }

  nsCOMPtr<nsIScriptGlobalObject> scriptglobal = do_QueryInterface(aOwner);
  return Init(prin, documentURI, baseURI, scriptglobal);
}

NS_IMETHODIMP
nsDOMParser::Init(nsIPrincipal *aPrincipal, nsIURI *aDocumentURI,
                  nsIURI *aBaseURI)
{
  AttemptedInitMarker marker(&mAttemptedInit);

  JSContext *cx = nsContentUtils::GetCurrentJSContext();
  NS_ENSURE_TRUE(cx, NS_ERROR_UNEXPECTED);

  nsIScriptContext* scriptContext = GetScriptContextFromJSContext(cx);

  nsCOMPtr<nsIPrincipal> principal = aPrincipal;

  if (!principal && !aDocumentURI) {
    nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
    NS_ENSURE_TRUE(secMan, NS_ERROR_UNEXPECTED);

    secMan->GetSubjectPrincipal(getter_AddRefs(principal));

    // We're called from JS; there better be a subject principal, really.
    NS_ENSURE_TRUE(principal, NS_ERROR_UNEXPECTED);
  }

  return Init(principal, aDocumentURI, aBaseURI,
              scriptContext ? scriptContext->GetGlobalObject() : nsnull);
}
