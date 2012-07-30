/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"
#include "nsDOMParser.h"
#include "nsIURI.h"
#include "nsIChannel.h"
#include "nsILoadGroup.h"
#include "nsIInputStream.h"
#include "nsNetUtil.h"
#include "nsStringStream.h"
#include "nsIDOMDocument.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsDOMClassInfoID.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
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

nsDOMParser::nsDOMParser()
  : mAttemptedInit(false)
{
}

nsDOMParser::~nsDOMParser()
{
}

DOMCI_DATA(DOMParser, nsDOMParser)

// QueryInterface implementation for nsDOMParser
NS_INTERFACE_MAP_BEGIN(nsDOMParser)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMParser)
  NS_INTERFACE_MAP_ENTRY(nsIDOMParser)
  NS_INTERFACE_MAP_ENTRY(nsIDOMParserJS)
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

  nsresult rv;

  if (!nsCRT::strcmp(contentType, "text/html")) {
    nsCOMPtr<nsIDOMDocument> domDocument;
    rv = SetUpDocument(DocumentFlavorHTML, getter_AddRefs(domDocument));
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIDocument> document = do_QueryInterface(domDocument);

    // Keep the XULXBL state, base URL and principal setting in sync with the
    // XML case

    if (nsContentUtils::IsSystemPrincipal(mOriginalPrincipal)) {
      document->ForceEnableXULXBL();
    }

    // Make sure to give this document the right base URI
    document->SetBaseURI(mBaseURI);
    // And the right principal
    document->SetPrincipal(mPrincipal);

    nsDependentString sourceBuffer(str);
    rv = nsContentUtils::ParseDocumentHTML(sourceBuffer, document, false);
    NS_ENSURE_SUCCESS(rv, rv);

    domDocument.forget(aResult);
    return rv;
  }

  NS_ConvertUTF16toUTF8 data(str);

  // The new stream holds a reference to the buffer
  nsCOMPtr<nsIInputStream> stream;
  rv = NS_NewByteInputStream(getter_AddRefs(stream),
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

  return ParseFromStream(stream, nullptr, bufLen, contentType, aResult);
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
  *aResult = nullptr;

  bool svg = nsCRT::strcmp(contentType, "image/svg+xml") == 0;

  // For now, we can only create XML documents.
  //XXXsmaug Should we create an HTMLDocument (in XHTML mode)
  //         for "application/xhtml+xml"?
  if ((nsCRT::strcmp(contentType, "text/xml") != 0) &&
      (nsCRT::strcmp(contentType, "application/xml") != 0) &&
      (nsCRT::strcmp(contentType, "application/xhtml+xml") != 0) &&
      !svg)
    return NS_ERROR_NOT_IMPLEMENTED;

  nsresult rv;

  // Put the nsCOMPtr out here so we hold a ref to the stream as needed
  nsCOMPtr<nsIInputStream> bufferedStream;
  if (!NS_InputStreamIsBuffered(stream)) {
    rv = NS_NewBufferedInputStream(getter_AddRefs(bufferedStream), stream,
                                   4096);
    NS_ENSURE_SUCCESS(rv, rv);

    stream = bufferedStream;
  }

  nsCOMPtr<nsIDOMDocument> domDocument;
  rv = SetUpDocument(svg ? DocumentFlavorSVG : DocumentFlavorLegacyGuess,
                     getter_AddRefs(domDocument));
  NS_ENSURE_SUCCESS(rv, rv);

  // Create a fake channel 
  nsCOMPtr<nsIChannel> parserChannel;
  NS_NewInputStreamChannel(getter_AddRefs(parserChannel), mDocumentURI, nullptr,
                           nsDependentCString(contentType), nullptr);
  NS_ENSURE_STATE(parserChannel);

  // More principal-faking here 
  parserChannel->SetOwner(mOriginalPrincipal);

  if (charset) {
    parserChannel->SetContentCharset(nsDependentCString(charset));
  }

  // Tell the document to start loading
  nsCOMPtr<nsIStreamListener> listener;

  // Have to pass false for reset here, else the reset will remove
  // our event listener.  Should that listener addition move to later
  // than this call?  Then we wouldn't need to mess around with
  // SetPrincipal, etc, probably!
  nsCOMPtr<nsIDocument> document(do_QueryInterface(domDocument));
  if (!document) return NS_ERROR_FAILURE;

  // Keep the XULXBL state, base URL and principal setting in sync with the
  // HTML case

  if (nsContentUtils::IsSystemPrincipal(mOriginalPrincipal)) {
    document->ForceEnableXULXBL();
  }

  rv = document->StartDocumentLoad(kLoadAsData, parserChannel, 
                                   nullptr, nullptr, 
                                   getter_AddRefs(listener),
                                   false);

  // Make sure to give this document the right base URI
  document->SetBaseURI(mBaseURI);

  // And the right principal
  document->SetPrincipal(mPrincipal);

  if (NS_FAILED(rv) || !listener) {
    return NS_ERROR_FAILURE;
  }

  // Now start pumping data to the listener
  nsresult status;

  rv = listener->OnStartRequest(parserChannel, nullptr);
  if (NS_FAILED(rv))
    parserChannel->Cancel(rv);
  parserChannel->GetStatus(&status);

  if (NS_SUCCEEDED(rv) && NS_SUCCEEDED(status)) {
    rv = listener->OnDataAvailable(parserChannel, nullptr, stream, 0,
                                   contentLength);
    if (NS_FAILED(rv))
      parserChannel->Cancel(rv);
    parserChannel->GetStatus(&status);
  }

  rv = listener->OnStopRequest(parserChannel, nullptr, status);
  // Failure returned from OnStopRequest does not affect the final status of
  // the channel, so we do not need to call Cancel(rv) as we do above.

  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }

  domDocument.swap(*aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMParser::Init(nsIPrincipal* principal, nsIURI* documentURI,
                  nsIURI* baseURI, nsIScriptGlobalObject* aScriptObject)
{
  NS_ENSURE_STATE(!mAttemptedInit);
  mAttemptedInit = true;
  
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
      secMan->GetSimpleCodebasePrincipal(mDocumentURI,
                                         getter_AddRefs(mPrincipal));
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
JSvalToInterface(JSContext* cx, JS::Value val, nsIXPConnect* xpc, bool* wasNull)
{
  if (val.isNull()) {
    *wasNull = true;
    return nsQueryInterface(nullptr);
  }
  
  *wasNull = false;
  if (val.isObject()) {
    JSObject* arg = &val.toObject();

    nsCOMPtr<nsIXPConnectWrappedNative> native;
    xpc->GetWrappedNativeOfJSObject(cx, arg, getter_AddRefs(native));

    // do_QueryWrappedNative is not null-safe
    if (native) {
      return do_QueryWrappedNative(native);
    }
  }
  
  return nsQueryInterface(nullptr);
}

static nsresult
GetInitArgs(JSContext *cx, PRUint32 argc, jsval *argv,
            nsIPrincipal** aPrincipal, nsIURI** aDocumentURI,
            nsIURI** aBaseURI)
{
  // Only proceed if the caller has UniversalXPConnect.
  bool haveUniversalXPConnect;
  nsresult rv = nsContentUtils::GetSecurityManager()->
    IsCapabilityEnabled("UniversalXPConnect", &haveUniversalXPConnect);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!haveUniversalXPConnect) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }    
  
  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  
  // First arg is our principal.  If someone passes something that's
  // not a principal and not null, die to prevent privilege escalation.
  bool wasNull;
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

    nsresult rv = secMan->GetSubjectPrincipal(getter_AddRefs(prin));
    NS_ENSURE_SUCCESS(rv, rv);

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

    nsresult rv = secMan->GetSubjectPrincipal(getter_AddRefs(principal));
    NS_ENSURE_SUCCESS(rv, rv);

    // We're called from JS; there better be a subject principal, really.
    NS_ENSURE_TRUE(principal, NS_ERROR_UNEXPECTED);
  }

  return Init(principal, aDocumentURI, aBaseURI,
              scriptContext ? scriptContext->GetGlobalObject() : nullptr);
}

nsresult
nsDOMParser::SetUpDocument(DocumentFlavor aFlavor, nsIDOMDocument** aResult)
{
  nsCOMPtr<nsIScriptGlobalObject> scriptHandlingObject =
    do_QueryReferent(mScriptHandlingObject);
  nsresult rv;
  if (!mPrincipal) {
    NS_ENSURE_TRUE(!mAttemptedInit, NS_ERROR_NOT_INITIALIZED);
    AttemptedInitMarker marker(&mAttemptedInit);

    nsCOMPtr<nsIPrincipal> prin =
      do_CreateInstance("@mozilla.org/nullprincipal;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = Init(prin, nullptr, nullptr, scriptHandlingObject);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_ASSERTION(mPrincipal, "Must have principal by now");
  NS_ASSERTION(mDocumentURI, "Must have document URI by now");

  // Here we have to cheat a little bit...  Setting the base URI won't
  // work if the document has a null principal, so use
  // mOriginalPrincipal when creating the document, then reset the
  // principal.
  return nsContentUtils::CreateDocument(EmptyString(), EmptyString(), nullptr,
                                        mDocumentURI, mBaseURI,
                                        mOriginalPrincipal,
                                        scriptHandlingObject,
                                        aFlavor,
                                        aResult);
}
