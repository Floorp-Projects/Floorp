/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "mozilla/dom/XMLDocument.h"
#include "nsParserCIID.h"
#include "nsCharsetSource.h"
#include "nsIXMLContentSink.h"
#include "nsPresContext.h" 
#include "nsIContent.h"
#include "nsIContentViewerContainer.h"
#include "nsIContentViewer.h"
#include "nsIDocShell.h"
#include "nsHTMLParts.h"
#include "nsIComponentManager.h"
#include "nsIDOMElement.h"
#include "nsIBaseWindow.h"
#include "nsIDOMWindow.h"
#include "nsIDOMDocumentType.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIHttpChannelInternal.h"
#include "nsIURI.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"
#include "nsError.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsLayoutCID.h"
#include "mozilla/dom/Attr.h"
#include "nsCExternalHandlerService.h"
#include "nsMimeTypes.h"
#include "mozilla/EventListenerManager.h"
#include "nsContentUtils.h"
#include "nsThreadUtils.h"
#include "nsJSUtils.h"
#include "nsCRT.h"
#include "nsIAuthPrompt.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentPolicyUtils.h"
#include "nsNodeUtils.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsIHTMLDocument.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/XMLDocumentBinding.h"
#include "mozilla/dom/DocumentBinding.h"

using namespace mozilla;
using namespace mozilla::dom;

// ==================================================================
// =
// ==================================================================


nsresult
NS_NewDOMDocument(nsIDOMDocument** aInstancePtrResult,
                  const nsAString& aNamespaceURI, 
                  const nsAString& aQualifiedName, 
                  nsIDOMDocumentType* aDoctype,
                  nsIURI* aDocumentURI,
                  nsIURI* aBaseURI,
                  nsIPrincipal* aPrincipal,
                  bool aLoadedAsData,
                  nsIGlobalObject* aEventObject,
                  DocumentFlavor aFlavor)
{
  // Note: can't require that aDocumentURI/aBaseURI/aPrincipal be non-null,
  // since at least one caller (XMLHttpRequest) doesn't have decent args to
  // pass in.
  
  nsresult rv;

  *aInstancePtrResult = nullptr;

  nsCOMPtr<nsIDocument> d;
  bool isHTML = false;
  bool isXHTML = false;
  if (aFlavor == DocumentFlavorSVG) {
    rv = NS_NewSVGDocument(getter_AddRefs(d));
  } else if (aFlavor == DocumentFlavorHTML) {
    rv = NS_NewHTMLDocument(getter_AddRefs(d));
    isHTML = true;
  } else if (aFlavor == DocumentFlavorPlain) {
    rv = NS_NewXMLDocument(getter_AddRefs(d), aLoadedAsData, true);
  } else if (aDoctype) {
    MOZ_ASSERT(aFlavor == DocumentFlavorLegacyGuess);
    nsAutoString publicId, name;
    aDoctype->GetPublicId(publicId);
    if (publicId.IsEmpty()) {
      aDoctype->GetName(name);
    }
    if (name.EqualsLiteral("html") ||
        publicId.EqualsLiteral("-//W3C//DTD HTML 4.01//EN") ||
        publicId.EqualsLiteral("-//W3C//DTD HTML 4.01 Frameset//EN") ||
        publicId.EqualsLiteral("-//W3C//DTD HTML 4.01 Transitional//EN") ||
        publicId.EqualsLiteral("-//W3C//DTD HTML 4.0//EN") ||
        publicId.EqualsLiteral("-//W3C//DTD HTML 4.0 Frameset//EN") ||
        publicId.EqualsLiteral("-//W3C//DTD HTML 4.0 Transitional//EN")) {
      rv = NS_NewHTMLDocument(getter_AddRefs(d));
      isHTML = true;
    } else if (publicId.EqualsLiteral("-//W3C//DTD XHTML 1.0 Strict//EN") ||
               publicId.EqualsLiteral("-//W3C//DTD XHTML 1.0 Transitional//EN") ||
               publicId.EqualsLiteral("-//W3C//DTD XHTML 1.0 Frameset//EN")) {
      rv = NS_NewHTMLDocument(getter_AddRefs(d));
      isHTML = true;
      isXHTML = true;
    }
    else if (publicId.EqualsLiteral("-//W3C//DTD SVG 1.1//EN")) {
      rv = NS_NewSVGDocument(getter_AddRefs(d));
    }
    // XXX Add support for XUL documents.
    else {
      rv = NS_NewXMLDocument(getter_AddRefs(d));
    }
  } else {
    MOZ_ASSERT(aFlavor == DocumentFlavorLegacyGuess);
    rv = NS_NewXMLDocument(getter_AddRefs(d));
  }

  if (NS_FAILED(rv)) {
    return rv;
  }

  if (isHTML) {
    nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(d);
    NS_ASSERTION(htmlDoc, "HTML Document doesn't implement nsIHTMLDocument?");
    htmlDoc->SetCompatibilityMode(eCompatibility_FullStandards);
    htmlDoc->SetIsXHTML(isXHTML);
  }
  nsDocument* doc = static_cast<nsDocument*>(d.get());
  doc->SetLoadedAsData(aLoadedAsData);
  doc->nsDocument::SetDocumentURI(aDocumentURI);
  // Must set the principal first, since SetBaseURI checks it.
  doc->SetPrincipal(aPrincipal);
  doc->SetBaseURI(aBaseURI);

  // We need to set the script handling object after we set the principal such
  // that the doc group is assigned correctly.
  if (nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(aEventObject)) {
    d->SetScriptHandlingObject(sgo);
  } else if (aEventObject){
    d->SetScopeObject(aEventObject);
  }

  // XMLDocuments and documents "created in memory" get to be UTF-8 by default,
  // unlike the legacy HTML mess
  doc->SetDocumentCharacterSet(NS_LITERAL_CSTRING("UTF-8"));
  
  if (aDoctype) {
    nsCOMPtr<nsIDOMNode> tmpNode;
    rv = doc->AppendChild(aDoctype, getter_AddRefs(tmpNode));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  
  if (!aQualifiedName.IsEmpty()) {
    nsCOMPtr<nsIDOMElement> root;
    rv = doc->CreateElementNS(aNamespaceURI, aQualifiedName,
                              getter_AddRefs(root));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMNode> tmpNode;

    rv = doc->AppendChild(root, getter_AddRefs(tmpNode));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *aInstancePtrResult = doc;
  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}

nsresult
NS_NewXMLDocument(nsIDocument** aInstancePtrResult, bool aLoadedAsData,
                  bool aIsPlainDocument)
{
  RefPtr<XMLDocument> doc = new XMLDocument();

  nsresult rv = doc->Init();

  if (NS_FAILED(rv)) {
    *aInstancePtrResult = nullptr;
    return rv;
  }

  doc->SetLoadedAsData(aLoadedAsData);
  doc->mIsPlainDocument = aIsPlainDocument;
  doc.forget(aInstancePtrResult);

  return NS_OK;
}

nsresult
NS_NewXBLDocument(nsIDOMDocument** aInstancePtrResult,
                  nsIURI* aDocumentURI,
                  nsIURI* aBaseURI,
                  nsIPrincipal* aPrincipal)
{
  nsresult rv = NS_NewDOMDocument(aInstancePtrResult,
                                  NS_LITERAL_STRING("http://www.mozilla.org/xbl"),
                                  NS_LITERAL_STRING("bindings"), nullptr,
                                  aDocumentURI, aBaseURI, aPrincipal, false,
                                  nullptr, DocumentFlavorLegacyGuess);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDocument> idoc = do_QueryInterface(*aInstancePtrResult);
  nsDocument* doc = static_cast<nsDocument*>(idoc.get());
  doc->SetLoadedAsInteractiveData(true);
  doc->SetReadyStateInternal(nsIDocument::READYSTATE_COMPLETE);

  return NS_OK;
}

namespace mozilla {
namespace dom {

XMLDocument::XMLDocument(const char* aContentType)
  : nsDocument(aContentType),
    mAsync(true)
{
  // NOTE! nsDocument::operator new() zeroes out all members, so don't
  // bother initializing members to 0.

  mType = eGenericXML;
}

XMLDocument::~XMLDocument()
{
  // XXX We rather crash than hang
  mLoopingForSyncLoad = false;
}

// QueryInterface implementation for XMLDocument
NS_IMPL_ISUPPORTS_INHERITED(XMLDocument, nsDocument, nsIDOMXMLDocument)

nsresult
XMLDocument::Init()
{
  nsresult rv = nsDocument::Init();
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

void
XMLDocument::Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup)
{
  nsDocument::Reset(aChannel, aLoadGroup);
}

void
XMLDocument::ResetToURI(nsIURI *aURI, nsILoadGroup *aLoadGroup,
                        nsIPrincipal* aPrincipal)
{
  if (mChannelIsPending) {
    StopDocumentLoad();
    mChannel->Cancel(NS_BINDING_ABORTED);
    mChannelIsPending = false;
  }

  nsDocument::ResetToURI(aURI, aLoadGroup, aPrincipal);
}

bool
XMLDocument::Load(const nsAString& aUrl, ErrorResult& aRv)
{
  bool hasHadScriptObject = true;
  nsIScriptGlobalObject* scriptObject =
    GetScriptHandlingObject(hasHadScriptObject);
  if (!scriptObject && hasHadScriptObject) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return false;
  }

  nsCOMPtr<nsIDocument> callingDoc = GetEntryDocument();
  nsCOMPtr<nsIPrincipal> principal = NodePrincipal();

  // The callingDoc's Principal and doc's Principal should be the same
  if (callingDoc && (callingDoc->NodePrincipal() != principal)) {
    nsContentUtils::ReportToConsole(nsIScriptError::errorFlag,
                                    NS_LITERAL_CSTRING("DOM"),
                                    callingDoc,
                                    nsContentUtils::eDOM_PROPERTIES,
                                    "XMLDocumentLoadPrincipalMismatch");
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return false;
  }

  // Reporting a warning on ourselves is rather pointless, because we probably
  // have no window id (and hence the warning won't show up in any web console)
  // and probably aren't considered a "content document" because we're not
  // loaded in a docshell, so won't accumulate telemetry for use counters.  Try
  // warning on our entry document, if any, since that should have things like
  // window ids and associated docshells.
  nsIDocument* docForWarning = callingDoc ? callingDoc.get() : this;
  if (nsContentUtils::IsCallerChrome()) {
    docForWarning->WarnOnceAbout(nsIDocument::eChromeUseOfDOM3LoadMethod);
  } else {
    docForWarning->WarnOnceAbout(nsIDocument::eUseOfDOM3LoadMethod);
  } 

  nsIURI *baseURI = mDocumentURI;
  nsAutoCString charset;

  if (callingDoc) {
    baseURI = callingDoc->GetDocBaseURI();
    charset = callingDoc->GetDocumentCharacterSet();
  }

  // Create a new URI
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aUrl, charset.get(), baseURI);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return false;
  }

  if (nsContentUtils::IsSystemPrincipal(principal)) {
    // We're called from chrome, check to make sure the URI we're
    // about to load is also chrome.

    bool isChrome = false;
    if (NS_FAILED(uri->SchemeIs("chrome", &isChrome)) || !isChrome) {
      nsAutoCString spec;
      if (mDocumentURI)
        mDocumentURI->GetSpec(spec);

      nsAutoString error;
      error.AssignLiteral("Cross site loading using document.load is no "
                          "longer supported. Use XMLHttpRequest instead.");
      nsCOMPtr<nsIScriptError> errorObject =
          do_CreateInstance(NS_SCRIPTERROR_CONTRACTID, &rv);
      if (NS_FAILED(rv)) {
        aRv.Throw(rv);
        return false;
      }

      rv = errorObject->InitWithWindowID(error,
                                         NS_ConvertUTF8toUTF16(spec),
                                         EmptyString(),
                                         0, 0, nsIScriptError::warningFlag,
                                         "DOM",
                                         callingDoc ?
                                           callingDoc->InnerWindowID() :
                                           this->InnerWindowID());

      if (NS_FAILED(rv)) {
        aRv.Throw(rv);
        return false;
      }

      nsCOMPtr<nsIConsoleService> consoleService =
        do_GetService(NS_CONSOLESERVICE_CONTRACTID);
      if (consoleService) {
        consoleService->LogMessage(errorObject);
      }

      aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
      return false;
    }
  }

  // Partial Reset, need to restore principal for security reasons and
  // event listener manager so that load listeners etc. will
  // remain. This should be done before the security check is done to
  // ensure that the document is reset even if the new document can't
  // be loaded.  Note that we need to hold a strong ref to |principal|
  // here, because ResetToURI will null out our node principal before
  // setting the new one.
  RefPtr<EventListenerManager> elm(mListenerManager);
  mListenerManager = nullptr;

  // When we are called from JS we can find the load group for the page,
  // and add ourselves to it. This way any pending requests
  // will be automatically aborted if the user leaves the page.

  nsCOMPtr<nsILoadGroup> loadGroup;
  if (callingDoc) {
    loadGroup = callingDoc->GetDocumentLoadGroup();
  }

  ResetToURI(uri, loadGroup, principal);

  mListenerManager = elm;

  // Create a channel
  nsCOMPtr<nsIInterfaceRequestor> req = nsContentUtils::SameOriginChecker();

  nsCOMPtr<nsIChannel> channel;
  // nsIRequest::LOAD_BACKGROUND prevents throbber from becoming active,
  // which in turn keeps STOP button from becoming active  
  rv = NS_NewChannel(getter_AddRefs(channel),
                     uri,
                     callingDoc ? callingDoc.get() :
                                  static_cast<nsIDocument*>(this),
                     nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED,
                     nsIContentPolicy::TYPE_INTERNAL_XMLHTTPREQUEST,
                     loadGroup,
                     req,
                     nsIRequest::LOAD_BACKGROUND);

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return false;
  }

  // TODO Bug 1189945: Remove nsIChannel CorsMode flag and set Request.mode
  // based on nsILoadInfo securityFlags instead. This block will be removed
  // when Request.mode set correctly.
  nsCOMPtr<nsIHttpChannelInternal> httpChannel = do_QueryInterface(channel);
  if (httpChannel) {
    httpChannel->SetCorsMode(nsIHttpChannelInternal::CORS_MODE_SAME_ORIGIN);
  }

  // StartDocumentLoad asserts that readyState is uninitialized, so
  // uninitialize it. SetReadyStateInternal make this transition invisible to
  // Web content. But before doing that, assert that the current readyState
  // is complete as it should be after the call to ResetToURI() above.
  MOZ_ASSERT(GetReadyStateEnum() == nsIDocument::READYSTATE_COMPLETE,
             "Bad readyState");
  SetReadyStateInternal(nsIDocument::READYSTATE_UNINITIALIZED);

  // Prepare for loading the XML document "into oneself"
  nsCOMPtr<nsIStreamListener> listener;
  if (NS_FAILED(rv = StartDocumentLoad(kLoadAsData, channel, 
                                       loadGroup, nullptr, 
                                       getter_AddRefs(listener),
                                       false))) {
    NS_ERROR("XMLDocument::Load: Failed to start the document load.");
    aRv.Throw(rv);
    return false;
  }

  // After this point, if we error out of this method we should clear
  // mChannelIsPending.

  // Start an asynchronous read of the XML document
  rv = channel->AsyncOpen2(listener);
  if (NS_FAILED(rv)) {
    mChannelIsPending = false;
    aRv.Throw(rv);
    return false;
  }

  if (!mAsync) {
    nsCOMPtr<nsIThread> thread = do_GetCurrentThread();

    nsAutoSyncOperation sync(this);
    mLoopingForSyncLoad = true;
    while (mLoopingForSyncLoad) {
      if (!NS_ProcessNextEvent(thread))
        break;
    }

    // We set return to true unless there was a parsing error
    Element* rootElement = GetRootElement();
    if (!rootElement) {
      return false;
    }

    if (rootElement->LocalName().EqualsLiteral("parsererror")) {
      nsAutoString ns;
      rootElement->GetNamespaceURI(ns);
      if (ns.EqualsLiteral("http://www.mozilla.org/newlayout/xml/parsererror.xml")) {
        return false;
      }
    }
  }

  return true;
}

void
XMLDocument::SetSuppressParserErrorElement(bool aSuppress)
{
  mSuppressParserErrorElement = aSuppress;
}

bool
XMLDocument::SuppressParserErrorElement()
{
  return mSuppressParserErrorElement;
}

void
XMLDocument::SetSuppressParserErrorConsoleMessages(bool aSuppress)
{
  mSuppressParserErrorConsoleMessages = aSuppress;
}

bool
XMLDocument::SuppressParserErrorConsoleMessages()
{
  return mSuppressParserErrorConsoleMessages;
}

nsresult
XMLDocument::StartDocumentLoad(const char* aCommand,
                               nsIChannel* aChannel,
                               nsILoadGroup* aLoadGroup,
                               nsISupports* aContainer,
                               nsIStreamListener **aDocListener,
                               bool aReset,
                               nsIContentSink* aSink)
{
  nsresult rv = nsDocument::StartDocumentLoad(aCommand,
                                              aChannel, aLoadGroup,
                                              aContainer, 
                                              aDocListener, aReset, aSink);
  if (NS_FAILED(rv)) return rv;

  if (nsCRT::strcmp("loadAsInteractiveData", aCommand) == 0) {
    mLoadedAsInteractiveData = true;
    aCommand = kLoadAsData; // XBL, for example, needs scripts and styles
  }


  int32_t charsetSource = kCharsetFromDocTypeDefault;
  nsAutoCString charset(NS_LITERAL_CSTRING("UTF-8"));
  TryChannelCharset(aChannel, charsetSource, charset, nullptr);

  nsCOMPtr<nsIURI> aUrl;
  rv = aChannel->GetURI(getter_AddRefs(aUrl));
  if (NS_FAILED(rv)) return rv;

  static NS_DEFINE_CID(kCParserCID, NS_PARSER_CID);

  mParser = do_CreateInstance(kCParserCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIXMLContentSink> sink;
    
  if (aSink) {
    sink = do_QueryInterface(aSink);
  }
  else {
    nsCOMPtr<nsIDocShell> docShell;
    if (aContainer) {
      docShell = do_QueryInterface(aContainer);
      NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);
    }
    rv = NS_NewXMLContentSink(getter_AddRefs(sink), this, aUrl, docShell,
                              aChannel);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Set the parser as the stream listener for the document loader...
  rv = CallQueryInterface(mParser, aDocListener);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(mChannel, "How can we not have a channel here?");
  mChannelIsPending = true;
  
  SetDocumentCharacterSet(charset);
  mParser->SetDocumentCharset(charset, charsetSource);
  mParser->SetCommand(aCommand);
  mParser->SetContentSink(sink);
  mParser->Parse(aUrl, nullptr, (void *)this);

  return NS_OK;
}

void
XMLDocument::EndLoad()
{
  mChannelIsPending = false;
  mLoopingForSyncLoad = false;

  mSynchronousDOMContentLoaded = (mLoadedAsData || mLoadedAsInteractiveData);
  nsDocument::EndLoad();
  if (mSynchronousDOMContentLoaded) {
    mSynchronousDOMContentLoaded = false;
    nsDocument::SetReadyStateInternal(nsIDocument::READYSTATE_COMPLETE);
    // Generate a document load event for the case when an XML
    // document was loaded as pure data without any presentation
    // attached to it.
    WidgetEvent event(true, eLoad);
    EventDispatcher::Dispatch(static_cast<nsIDocument*>(this), nullptr, &event);
  }
}

/* virtual */ void
XMLDocument::DocAddSizeOfExcludingThis(nsWindowSizes* aWindowSizes) const
{
  nsDocument::DocAddSizeOfExcludingThis(aWindowSizes);
}

// nsIDOMDocument interface

nsresult
XMLDocument::Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const
{
  NS_ASSERTION(aNodeInfo->NodeInfoManager() == mNodeInfoManager,
               "Can't import this document into another document!");

  RefPtr<XMLDocument> clone = new XMLDocument();
  nsresult rv = CloneDocHelper(clone);
  NS_ENSURE_SUCCESS(rv, rv);

  // State from XMLDocument
  clone->mAsync = mAsync;
  clone->mIsPlainDocument = mIsPlainDocument;

  return CallQueryInterface(clone.get(), aResult);
}

JSObject*
XMLDocument::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  if (mIsPlainDocument) {
    return DocumentBinding::Wrap(aCx, this, aGivenProto);
  }

  return XMLDocumentBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
