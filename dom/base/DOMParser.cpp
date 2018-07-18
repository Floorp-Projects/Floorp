/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DOMParser.h"

#include "nsNetUtil.h"
#include "nsDOMString.h"
#include "MainThreadUtils.h"
#include "nsIStreamListener.h"
#include "nsStringStream.h"
#include "nsIScriptError.h"
#include "nsIScriptSecurityManager.h"
#include "nsCRT.h"
#include "nsStreamUtils.h"
#include "nsContentUtils.h"
#include "nsDOMJSUtils.h"
#include "nsError.h"
#include "nsPIDOMWindow.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/ScriptSettings.h"

using namespace mozilla;
using namespace mozilla::dom;

DOMParser::DOMParser(nsIGlobalObject* aOwner, nsIPrincipal* aDocPrincipal,
                     nsIURI* aDocumentURI, nsIURI* aBaseURI)
  : mOwner(aOwner)
  , mPrincipal(aDocPrincipal)
  , mDocumentURI(aDocumentURI)
  , mBaseURI(aBaseURI)
  , mForceEnableXULXBL(false)
{
  MOZ_ASSERT(aDocPrincipal);
  MOZ_ASSERT(aDocumentURI);
}

DOMParser::~DOMParser()
{
}

// QueryInterface implementation for DOMParser
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMParser)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DOMParser, mOwner)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMParser)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMParser)

static const char*
StringFromSupportedType(SupportedType aType)
{
  return SupportedTypeValues::strings[static_cast<int>(aType)].value;
}

already_AddRefed<nsIDocument>
DOMParser::ParseFromString(const nsAString& aStr, SupportedType aType,
                           ErrorResult& aRv)
{
  if (aType == SupportedType::Text_html) {
    nsCOMPtr<nsIDocument> document = SetUpDocument(DocumentFlavorHTML, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    // Keep the XULXBL state in sync with the XML case.
    if (mForceEnableXULXBL) {
      document->ForceEnableXULXBL();
    }

    nsresult rv = nsContentUtils::ParseDocumentHTML(aStr, document, false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRv.Throw(rv);
      return nullptr;
    }

    return document.forget();
  }

  nsAutoCString utf8str;
  // Convert from UTF16 to UTF8 using fallible allocations
  if (!AppendUTF16toUTF8(aStr, utf8str, mozilla::fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  // The new stream holds a reference to the buffer
  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = NS_NewByteInputStream(getter_AddRefs(stream),
                                      utf8str.get(), utf8str.Length(),
                                      NS_ASSIGNMENT_DEPEND);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return nullptr;
  }

  return ParseFromStream(stream,  NS_LITERAL_STRING("UTF-8"),
                         utf8str.Length(), aType, aRv);
}

already_AddRefed<nsIDocument>
DOMParser::ParseFromBuffer(const Uint8Array& aBuf, SupportedType aType,
                           ErrorResult& aRv)
{
  aBuf.ComputeLengthAndData();
  return ParseFromBuffer(MakeSpan(aBuf.Data(), aBuf.Length()), aType, aRv);
}

already_AddRefed<nsIDocument>
DOMParser::ParseFromBuffer(Span<const uint8_t> aBuf, SupportedType aType,
                           ErrorResult& aRv)
{
  // The new stream holds a reference to the buffer
  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = NS_NewByteInputStream(getter_AddRefs(stream),
                                      reinterpret_cast<const char *>(aBuf.Elements()),
                                      aBuf.Length(), NS_ASSIGNMENT_DEPEND);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return ParseFromStream(stream, VoidString(), aBuf.Length(), aType, aRv);
}


already_AddRefed<nsIDocument>
DOMParser::ParseFromStream(nsIInputStream* aStream,
                           const nsAString& aCharset,
                           int32_t aContentLength,
                           SupportedType aType,
                           ErrorResult& aRv)
{
  bool svg = (aType == SupportedType::Image_svg_xml);

  // For now, we can only create XML documents.
  //XXXsmaug Should we create an HTMLDocument (in XHTML mode)
  //         for "application/xhtml+xml"?
  if (aType != SupportedType::Text_xml &&
      aType != SupportedType::Application_xml &&
      aType != SupportedType::Application_xhtml_xml &&
      !svg) {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return nullptr;
  }

  // Put the nsCOMPtr out here so we hold a ref to the stream as needed
  nsCOMPtr<nsIInputStream> stream = aStream;
  if (!NS_InputStreamIsBuffered(stream)) {
    nsCOMPtr<nsIInputStream> bufferedStream;
    nsresult rv = NS_NewBufferedInputStream(getter_AddRefs(bufferedStream),
                                   stream.forget(), 4096);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRv.Throw(rv);
      return nullptr;
    }

    stream = bufferedStream;
  }

  nsCOMPtr<nsIDocument> document =
    SetUpDocument(svg ? DocumentFlavorSVG : DocumentFlavorLegacyGuess, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Create a fake channel
  nsCOMPtr<nsIChannel> parserChannel;
  NS_NewInputStreamChannel(getter_AddRefs(parserChannel),
                           mDocumentURI,
                           nullptr, // aStream
                           mPrincipal,
                           nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL,
                           nsIContentPolicy::TYPE_OTHER,
                           nsDependentCString(StringFromSupportedType(aType)));
  if (NS_WARN_IF(!parserChannel)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  if (!DOMStringIsNull(aCharset)) {
    parserChannel->SetContentCharset(NS_ConvertUTF16toUTF8(aCharset));
  }

  // Tell the document to start loading
  nsCOMPtr<nsIStreamListener> listener;

  // Keep the XULXBL state in sync with the HTML case
  if (mForceEnableXULXBL) {
    document->ForceEnableXULXBL();
  }

  // Have to pass false for reset here, else the reset will remove
  // our event listener.  Should that listener addition move to later
  // than this call?
  nsresult rv = document->StartDocumentLoad(kLoadAsData, parserChannel,
                                            nullptr, nullptr,
                                            getter_AddRefs(listener),
                                            false);

  if (NS_FAILED(rv) || !listener) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // Now start pumping data to the listener
  nsresult status;

  rv = listener->OnStartRequest(parserChannel, nullptr);
  if (NS_FAILED(rv))
    parserChannel->Cancel(rv);
  parserChannel->GetStatus(&status);

  if (NS_SUCCEEDED(rv) && NS_SUCCEEDED(status)) {
    rv = listener->OnDataAvailable(parserChannel, nullptr, stream, 0,
                                   aContentLength);
    if (NS_FAILED(rv))
      parserChannel->Cancel(rv);
    parserChannel->GetStatus(&status);
  }

  rv = listener->OnStopRequest(parserChannel, nullptr, status);
  // Failure returned from OnStopRequest does not affect the final status of
  // the channel, so we do not need to call Cancel(rv) as we do above.

  if (NS_FAILED(rv)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return document.forget();
}

/*static */already_AddRefed<DOMParser>
DOMParser::Constructor(const GlobalObject& aOwner,
                       ErrorResult& rv)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIPrincipal> docPrincipal = aOwner.GetSubjectPrincipal();
  nsCOMPtr<nsIURI> documentURI;
  nsIURI* baseURI = nullptr;
  if (nsContentUtils::IsSystemPrincipal(docPrincipal)) {
    docPrincipal = NullPrincipal::CreateWithoutOriginAttributes();
    docPrincipal->GetURI(getter_AddRefs(documentURI));
  } else {
    // Grab document and base URIs off the window our constructor was
    // called on. Error out if anything untoward happens.
    nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aOwner.GetAsSupports());
    if (!window) {
      rv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    baseURI = window->GetDocBaseURI();
    documentURI = window->GetDocumentURI();
  }

  if (!documentURI) {
    rv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aOwner.GetAsSupports());
  MOZ_ASSERT(global);
  RefPtr<DOMParser> domParser = new DOMParser(global, docPrincipal,
                                              documentURI, baseURI);
  return domParser.forget();
}

// static
already_AddRefed<DOMParser>
DOMParser::CreateWithoutGlobal(ErrorResult& aRv)
{
  nsCOMPtr<nsIPrincipal> docPrincipal =
    NullPrincipal::CreateWithoutOriginAttributes();
  nsCOMPtr<nsIURI> documentURI;
  docPrincipal->GetURI(getter_AddRefs(documentURI));

  if (!documentURI) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  RefPtr<DOMParser> domParser = new DOMParser(nullptr, docPrincipal,
                                              documentURI, nullptr);
  return domParser.forget();
}

already_AddRefed<nsIDocument>
DOMParser::SetUpDocument(DocumentFlavor aFlavor, ErrorResult& aRv)
{
  // We should really just use mOwner here, but nsDocument gets confused
  // if we pass it a scriptHandlingObject that doesn't QI to
  // nsIScriptGlobalObject, and test_isequalnode.js (an xpcshell test without
  // a window global) breaks. The correct solution is just to wean nsDocument
  // off of nsIScriptGlobalObject, but that's a yak to shave another day.
  nsCOMPtr<nsIScriptGlobalObject> scriptHandlingObject =
    do_QueryInterface(mOwner);

  // Try to inherit a style backend.
  NS_ASSERTION(mPrincipal, "Must have principal by now");
  NS_ASSERTION(mDocumentURI, "Must have document URI by now");

  nsCOMPtr<nsIDocument> doc;
  nsresult rv = NS_NewDOMDocument(getter_AddRefs(doc), EmptyString(), EmptyString(),
                                  nullptr, mDocumentURI, mBaseURI, mPrincipal,
                                  true, scriptHandlingObject, aFlavor);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return nullptr;
  }

  return doc.forget();
}
