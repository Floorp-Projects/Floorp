/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIAuthPrompt.h"
#include "mozilla/dom/Document.h"
#include "nsIExpatSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsILoadGroup.h"
#include "nsParser.h"
#include "nsCharsetSource.h"
#include "nsIRequestObserver.h"
#include "nsContentPolicyUtils.h"
#include "nsIStreamConverterService.h"
#include "nsSyncLoadService.h"
#include "nsIHttpChannel.h"
#include "nsIURI.h"
#include "nsIPrincipal.h"
#include "nsIWindowWatcher.h"
#include "nsIXMLContentSink.h"
#include "nsMimeTypes.h"
#include "nsNetUtil.h"
#include "nsGkAtoms.h"
#include "txLog.h"
#include "txMozillaXSLTProcessor.h"
#include "txStylesheetCompiler.h"
#include "txXMLUtils.h"
#include "nsAttrName.h"
#include "nsComponentManagerUtils.h"
#include "nsIScriptError.h"
#include "nsError.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Text.h"
#include "mozilla/Encoding.h"
#include "mozilla/UniquePtr.h"
#include "ReferrerInfo.h"

using namespace mozilla;
using mozilla::dom::Document;
using mozilla::dom::ReferrerPolicy;

static void getSpec(nsIChannel* aChannel, nsAString& aSpec) {
  if (!aChannel) {
    return;
  }

  nsCOMPtr<nsIURI> uri;
  aChannel->GetOriginalURI(getter_AddRefs(uri));
  if (!uri) {
    return;
  }

  nsAutoCString spec;
  uri->GetSpec(spec);
  AppendUTF8toUTF16(spec, aSpec);
}

class txStylesheetSink final : public nsIXMLContentSink,
                               public nsIExpatSink,
                               public nsIStreamListener,
                               public nsIInterfaceRequestor {
 public:
  txStylesheetSink(txStylesheetCompiler* aCompiler, nsParser* aParser);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIEXPATSINK
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSIINTERFACEREQUESTOR

  // nsIContentSink
  NS_IMETHOD WillParse(void) override { return NS_OK; }
  NS_IMETHOD DidBuildModel(bool aTerminated) override;
  NS_IMETHOD WillInterrupt(void) override { return NS_OK; }
  void WillResume() override {}
  NS_IMETHOD SetParser(nsParserBase* aParser) override { return NS_OK; }
  virtual void FlushPendingNotifications(mozilla::FlushType aType) override {}
  virtual void SetDocumentCharset(NotNull<const Encoding*> aEncoding) override {
  }
  virtual nsISupports* GetTarget() override { return nullptr; }

 private:
  RefPtr<txStylesheetCompiler> mCompiler;
  nsCOMPtr<nsIStreamListener> mListener;
  RefPtr<nsParser> mParser;
  bool mCheckedForXML;

 protected:
  ~txStylesheetSink() = default;

  // This exists solely to suppress a warning from nsDerivedSafe
  txStylesheetSink();
};

txStylesheetSink::txStylesheetSink(txStylesheetCompiler* aCompiler,
                                   nsParser* aParser)
    : mCompiler(aCompiler),
      mListener(aParser),
      mParser(aParser),
      mCheckedForXML(false) {}

NS_IMPL_ISUPPORTS(txStylesheetSink, nsIXMLContentSink, nsIContentSink,
                  nsIExpatSink, nsIStreamListener, nsIRequestObserver,
                  nsIInterfaceRequestor)

NS_IMETHODIMP
txStylesheetSink::HandleStartElement(const char16_t* aName,
                                     const char16_t** aAtts,
                                     uint32_t aAttsCount, uint32_t aLineNumber,
                                     uint32_t aColumnNumber) {
  MOZ_ASSERT(aAttsCount % 2 == 0, "incorrect aAttsCount");

  nsresult rv = mCompiler->startElement(aName, aAtts, aAttsCount / 2);
  if (NS_FAILED(rv)) {
    mCompiler->cancel(rv);

    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
txStylesheetSink::HandleEndElement(const char16_t* aName) {
  nsresult rv = mCompiler->endElement();
  if (NS_FAILED(rv)) {
    mCompiler->cancel(rv);

    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
txStylesheetSink::HandleComment(const char16_t* aName) { return NS_OK; }

NS_IMETHODIMP
txStylesheetSink::HandleCDataSection(const char16_t* aData, uint32_t aLength) {
  return HandleCharacterData(aData, aLength);
}

NS_IMETHODIMP
txStylesheetSink::HandleDoctypeDecl(const nsAString& aSubset,
                                    const nsAString& aName,
                                    const nsAString& aSystemId,
                                    const nsAString& aPublicId,
                                    nsISupports* aCatalogData) {
  return NS_OK;
}

NS_IMETHODIMP
txStylesheetSink::HandleCharacterData(const char16_t* aData, uint32_t aLength) {
  nsresult rv = mCompiler->characters(Substring(aData, aData + aLength));
  if (NS_FAILED(rv)) {
    mCompiler->cancel(rv);
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
txStylesheetSink::HandleProcessingInstruction(const char16_t* aTarget,
                                              const char16_t* aData) {
  return NS_OK;
}

NS_IMETHODIMP
txStylesheetSink::HandleXMLDeclaration(const char16_t* aVersion,
                                       const char16_t* aEncoding,
                                       int32_t aStandalone) {
  return NS_OK;
}

NS_IMETHODIMP
txStylesheetSink::ReportError(const char16_t* aErrorText,
                              const char16_t* aSourceText,
                              nsIScriptError* aError, bool* _retval) {
  MOZ_ASSERT(aError && aSourceText && aErrorText, "Check arguments!!!");

  // The expat driver should report the error.
  *_retval = true;

  mCompiler->cancel(NS_ERROR_FAILURE, aErrorText, aSourceText);

  return NS_OK;
}

NS_IMETHODIMP
txStylesheetSink::DidBuildModel(bool aTerminated) {
  return mCompiler->doneLoading();
}

NS_IMETHODIMP
txStylesheetSink::OnDataAvailable(nsIRequest* aRequest,
                                  nsIInputStream* aInputStream,
                                  uint64_t aOffset, uint32_t aCount) {
  if (!mCheckedForXML) {
    Maybe<bool> isForXML = mParser->IsForParsingXML();
    mCheckedForXML = isForXML.isSome();
    if (mCheckedForXML && !isForXML.value()) {
      nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
      nsAutoString spec;
      getSpec(channel, spec);
      mCompiler->cancel(NS_ERROR_XSLT_WRONG_MIME_TYPE, nullptr, spec.get());

      return NS_ERROR_XSLT_WRONG_MIME_TYPE;
    }
  }

  return mListener->OnDataAvailable(aRequest, aInputStream, aOffset, aCount);
}

NS_IMETHODIMP
txStylesheetSink::OnStartRequest(nsIRequest* aRequest) {
  int32_t charsetSource = kCharsetFromDocTypeDefault;

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);

  // check channel's charset...
  const Encoding* encoding = nullptr;
  nsAutoCString charsetVal;
  if (NS_SUCCEEDED(channel->GetContentCharset(charsetVal))) {
    encoding = Encoding::ForLabel(charsetVal);
    if (encoding) {
      charsetSource = kCharsetFromChannel;
    }
  }

  if (!encoding) {
    encoding = UTF_8_ENCODING;
  }

  mParser->SetDocumentCharset(WrapNotNull(encoding), charsetSource, false);

  nsAutoCString contentType;
  channel->GetContentType(contentType);

  // Time to sniff! Note: this should go away once file channels do
  // sniffing themselves.
  nsCOMPtr<nsIURI> uri;
  channel->GetURI(getter_AddRefs(uri));
  if (uri->SchemeIs("file") &&
      contentType.EqualsLiteral(UNKNOWN_CONTENT_TYPE)) {
    nsresult rv;
    nsCOMPtr<nsIStreamConverterService> serv =
        do_GetService("@mozilla.org/streamConverters;1", &rv);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIStreamListener> converter;
      rv = serv->AsyncConvertData(UNKNOWN_CONTENT_TYPE, "*/*", mListener,
                                  NS_ISUPPORTS_CAST(nsIParser*, mParser),
                                  getter_AddRefs(converter));
      if (NS_SUCCEEDED(rv)) {
        mListener = converter;
      }
    }
  }

  return mListener->OnStartRequest(aRequest);
}

NS_IMETHODIMP
txStylesheetSink::OnStopRequest(nsIRequest* aRequest, nsresult aStatusCode) {
  bool success = true;

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aRequest);
  if (httpChannel) {
    Unused << httpChannel->GetRequestSucceeded(&success);
  }

  nsresult result = aStatusCode;
  if (!success) {
    // XXX We sometimes want to use aStatusCode here, but the parser resets
    //     it to NS_ERROR_NOINTERFACE because we don't implement
    //     nsIHTMLContentSink.
    result = NS_ERROR_XSLT_NETWORK_ERROR;
  } else if (!mCheckedForXML) {
    Maybe<bool> isForXML = mParser->IsForParsingXML();
    if (isForXML.isSome() && !isForXML.value()) {
      result = NS_ERROR_XSLT_WRONG_MIME_TYPE;
    }
  }

  if (NS_FAILED(result)) {
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
    nsAutoString spec;
    getSpec(channel, spec);
    mCompiler->cancel(result, nullptr, spec.get());
  }

  nsresult rv = mListener->OnStopRequest(aRequest, aStatusCode);
  mListener = nullptr;
  mParser = nullptr;
  return rv;
}

NS_IMETHODIMP
txStylesheetSink::GetInterface(const nsIID& aIID, void** aResult) {
  if (aIID.Equals(NS_GET_IID(nsIAuthPrompt))) {
    NS_ENSURE_ARG(aResult);
    *aResult = nullptr;

    nsresult rv;
    nsCOMPtr<nsIWindowWatcher> wwatcher =
        do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIAuthPrompt> prompt;
    rv = wwatcher->GetNewAuthPrompter(nullptr, getter_AddRefs(prompt));
    NS_ENSURE_SUCCESS(rv, rv);

    prompt.forget(aResult);

    return NS_OK;
  }

  return NS_ERROR_NO_INTERFACE;
}

class txCompileObserver final : public txACompileObserver {
 public:
  txCompileObserver(txMozillaXSLTProcessor* aProcessor,
                    Document* aLoaderDocument);

  TX_DECL_ACOMPILEOBSERVER
  NS_INLINE_DECL_REFCOUNTING(txCompileObserver, override)

  nsresult startLoad(nsIURI* aUri, txStylesheetCompiler* aCompiler,
                     nsIPrincipal* aSourcePrincipal,
                     ReferrerPolicy aReferrerPolicy);

 private:
  RefPtr<txMozillaXSLTProcessor> mProcessor;
  nsCOMPtr<Document> mLoaderDocument;

  // This exists solely to suppress a warning from nsDerivedSafe
  txCompileObserver();

  // Private destructor, to discourage deletion outside of Release():
  ~txCompileObserver() = default;
};

txCompileObserver::txCompileObserver(txMozillaXSLTProcessor* aProcessor,
                                     Document* aLoaderDocument)
    : mProcessor(aProcessor), mLoaderDocument(aLoaderDocument) {}

nsresult txCompileObserver::loadURI(const nsAString& aUri,
                                    const nsAString& aReferrerUri,
                                    ReferrerPolicy aReferrerPolicy,
                                    txStylesheetCompiler* aCompiler) {
  if (mProcessor->IsLoadDisabled()) {
    return NS_ERROR_XSLT_LOAD_BLOCKED_ERROR;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aUri);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> referrerUri;
  rv = NS_NewURI(getter_AddRefs(referrerUri), aReferrerUri);
  NS_ENSURE_SUCCESS(rv, rv);

  OriginAttributes attrs;
  nsCOMPtr<nsIPrincipal> referrerPrincipal =
      BasePrincipal::CreateContentPrincipal(referrerUri, attrs);
  NS_ENSURE_TRUE(referrerPrincipal, NS_ERROR_FAILURE);

  return startLoad(uri, aCompiler, referrerPrincipal, aReferrerPolicy);
}

void txCompileObserver::onDoneCompiling(txStylesheetCompiler* aCompiler,
                                        nsresult aResult,
                                        const char16_t* aErrorText,
                                        const char16_t* aParam) {
  if (NS_SUCCEEDED(aResult)) {
    mProcessor->setStylesheet(aCompiler->getStylesheet());
  } else {
    mProcessor->reportError(aResult, aErrorText, aParam);
  }
}

nsresult txCompileObserver::startLoad(nsIURI* aUri,
                                      txStylesheetCompiler* aCompiler,
                                      nsIPrincipal* aReferrerPrincipal,
                                      ReferrerPolicy aReferrerPolicy) {
  nsCOMPtr<nsILoadGroup> loadGroup = mLoaderDocument->GetDocumentLoadGroup();
  if (!loadGroup) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIChannel> channel;
  nsresult rv = NS_NewChannelWithTriggeringPrincipal(
      getter_AddRefs(channel), aUri, mLoaderDocument,
      aReferrerPrincipal,  // triggeringPrincipal
      nsILoadInfo::SEC_REQUIRE_CORS_INHERITS_SEC_CONTEXT,
      nsIContentPolicy::TYPE_XSLT,
      nullptr,  // aPerformanceStorage
      loadGroup);

  NS_ENSURE_SUCCESS(rv, rv);

  channel->SetContentType("text/xml"_ns);

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
  if (httpChannel) {
    nsCOMPtr<nsIReferrerInfo> referrerInfo;
    nsresult rv = aReferrerPrincipal->CreateReferrerInfo(
        aReferrerPolicy, getter_AddRefs(referrerInfo));
    if (NS_SUCCEEDED(rv)) {
      rv = httpChannel->SetReferrerInfoWithoutClone(referrerInfo);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }

  RefPtr<nsParser> parser = new nsParser();
  RefPtr<txStylesheetSink> sink = new txStylesheetSink(aCompiler, parser);

  channel->SetNotificationCallbacks(sink);

  parser->SetCommand(kLoadAsData);
  parser->SetContentSink(sink);
  parser->Parse(aUri);

  return channel->AsyncOpen(sink);
}

nsresult TX_LoadSheet(nsIURI* aUri, txMozillaXSLTProcessor* aProcessor,
                      Document* aLoaderDocument,
                      ReferrerPolicy aReferrerPolicy) {
  nsIPrincipal* principal = aLoaderDocument->NodePrincipal();

  nsAutoCString spec;
  aUri->GetSpec(spec);
  MOZ_LOG(txLog::xslt, LogLevel::Info, ("TX_LoadSheet: %s\n", spec.get()));

  RefPtr<txCompileObserver> observer =
      new txCompileObserver(aProcessor, aLoaderDocument);

  RefPtr<txStylesheetCompiler> compiler = new txStylesheetCompiler(
      NS_ConvertUTF8toUTF16(spec), aReferrerPolicy, observer);

  return observer->startLoad(aUri, compiler, principal, aReferrerPolicy);
}

/**
 * handling DOM->txStylesheet
 * Observer needs to do synchronous loads.
 */
static nsresult handleNode(nsINode* aNode, txStylesheetCompiler* aCompiler) {
  nsresult rv = NS_OK;

  if (aNode->IsElement()) {
    dom::Element* element = aNode->AsElement();

    uint32_t attsCount = element->GetAttrCount();
    UniquePtr<txStylesheetAttr[]> atts;
    if (attsCount > 0) {
      atts = MakeUnique<txStylesheetAttr[]>(attsCount);
      uint32_t counter;
      for (counter = 0; counter < attsCount; ++counter) {
        txStylesheetAttr& att = atts[counter];
        const nsAttrName* name = element->GetAttrNameAt(counter);
        att.mNamespaceID = name->NamespaceID();
        att.mLocalName = name->LocalName();
        att.mPrefix = name->GetPrefix();
        element->GetAttr(att.mNamespaceID, att.mLocalName, att.mValue);
      }
    }

    mozilla::dom::NodeInfo* ni = element->NodeInfo();

    rv = aCompiler->startElement(ni->NamespaceID(), ni->NameAtom(),
                                 ni->GetPrefixAtom(), atts.get(), attsCount);
    NS_ENSURE_SUCCESS(rv, rv);

    // explicitly destroy the attrs here since we no longer need it
    atts = nullptr;

    for (nsIContent* child = element->GetFirstChild(); child;
         child = child->GetNextSibling()) {
      rv = handleNode(child, aCompiler);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    rv = aCompiler->endElement();
    NS_ENSURE_SUCCESS(rv, rv);
  } else if (dom::Text* text = aNode->GetAsText()) {
    nsAutoString chars;
    text->AppendTextTo(chars);
    rv = aCompiler->characters(chars);
    NS_ENSURE_SUCCESS(rv, rv);
  } else if (aNode->IsDocument()) {
    for (nsIContent* child = aNode->GetFirstChild(); child;
         child = child->GetNextSibling()) {
      rv = handleNode(child, aCompiler);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

class txSyncCompileObserver final : public txACompileObserver {
 public:
  explicit txSyncCompileObserver(txMozillaXSLTProcessor* aProcessor);

  TX_DECL_ACOMPILEOBSERVER
  NS_INLINE_DECL_REFCOUNTING(txSyncCompileObserver, override)

 private:
  // Private destructor, to discourage deletion outside of Release():
  ~txSyncCompileObserver() = default;

  RefPtr<txMozillaXSLTProcessor> mProcessor;
};

txSyncCompileObserver::txSyncCompileObserver(txMozillaXSLTProcessor* aProcessor)
    : mProcessor(aProcessor) {}

nsresult txSyncCompileObserver::loadURI(const nsAString& aUri,
                                        const nsAString& aReferrerUri,
                                        ReferrerPolicy aReferrerPolicy,
                                        txStylesheetCompiler* aCompiler) {
  if (mProcessor->IsLoadDisabled()) {
    return NS_ERROR_XSLT_LOAD_BLOCKED_ERROR;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aUri);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> referrerUri;
  rv = NS_NewURI(getter_AddRefs(referrerUri), aReferrerUri);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrincipal> referrerPrincipal =
      BasePrincipal::CreateContentPrincipal(referrerUri, OriginAttributes());
  NS_ENSURE_TRUE(referrerPrincipal, NS_ERROR_FAILURE);

  // This is probably called by js, a loadGroup for the channel doesn't
  // make sense.
  nsCOMPtr<nsINode> source;
  if (mProcessor) {
    source = mProcessor->GetSourceContentModel();
  }
  dom::nsAutoSyncOperation sync(source ? source->OwnerDoc() : nullptr,
                                dom::SyncOperationBehavior::eSuspendInput);
  nsCOMPtr<Document> document;

  rv = nsSyncLoadService::LoadDocument(
      uri, nsIContentPolicy::TYPE_XSLT, referrerPrincipal,
      nsILoadInfo::SEC_REQUIRE_CORS_INHERITS_SEC_CONTEXT, nullptr,
      source ? source->OwnerDoc()->CookieJarSettings() : nullptr, false,
      aReferrerPolicy, getter_AddRefs(document));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = handleNode(document, aCompiler);
  if (NS_FAILED(rv)) {
    nsAutoCString spec;
    uri->GetSpec(spec);
    aCompiler->cancel(rv, nullptr, NS_ConvertUTF8toUTF16(spec).get());
    return rv;
  }

  rv = aCompiler->doneLoading();
  return rv;
}

void txSyncCompileObserver::onDoneCompiling(txStylesheetCompiler* aCompiler,
                                            nsresult aResult,
                                            const char16_t* aErrorText,
                                            const char16_t* aParam) {}

nsresult TX_CompileStylesheet(nsINode* aNode,
                              txMozillaXSLTProcessor* aProcessor,
                              txStylesheet** aStylesheet) {
  // If we move GetBaseURI to nsINode this can be simplified.
  nsCOMPtr<Document> doc = aNode->OwnerDoc();

  nsIURI* nodeBaseURI = aNode->GetBaseURI();
  NS_ENSURE_TRUE(nodeBaseURI, NS_ERROR_FAILURE);

  nsAutoCString spec;
  nodeBaseURI->GetSpec(spec);
  NS_ConvertUTF8toUTF16 baseURI(spec);

  nsIURI* docUri = doc->GetDocumentURI();
  NS_ENSURE_TRUE(docUri, NS_ERROR_FAILURE);

  // We need to remove the ref, a URI with a ref would mean that we have an
  // embedded stylesheet.
  nsCOMPtr<nsIURI> uri;
  NS_GetURIWithoutRef(docUri, getter_AddRefs(uri));
  NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);

  uri->GetSpec(spec);
  NS_ConvertUTF8toUTF16 stylesheetURI(spec);

  RefPtr<txSyncCompileObserver> obs = new txSyncCompileObserver(aProcessor);

  RefPtr<txStylesheetCompiler> compiler =
      new txStylesheetCompiler(stylesheetURI, doc->GetReferrerPolicy(), obs);

  compiler->setBaseURI(baseURI);

  nsresult rv = handleNode(aNode, compiler);
  if (NS_FAILED(rv)) {
    compiler->cancel(rv);
    return rv;
  }

  rv = compiler->doneLoading();
  NS_ENSURE_SUCCESS(rv, rv);

  *aStylesheet = compiler->getStylesheet();
  NS_ADDREF(*aStylesheet);

  return NS_OK;
}
