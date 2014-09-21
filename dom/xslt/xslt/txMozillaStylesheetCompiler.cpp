/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMArray.h"
#include "nsIAuthPrompt.h"
#include "nsIDOMNode.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIExpatSink.h"
#include "nsIChannelEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsILoadGroup.h"
#include "nsIParser.h"
#include "nsCharsetSource.h"
#include "nsIRequestObserver.h"
#include "nsIScriptSecurityManager.h"
#include "nsContentPolicyUtils.h"
#include "nsIStreamConverterService.h"
#include "nsSyncLoadService.h"
#include "nsIURI.h"
#include "nsIPrincipal.h"
#include "nsIWindowWatcher.h"
#include "nsIXMLContentSink.h"
#include "nsMimeTypes.h"
#include "nsNetUtil.h"
#include "nsParserCIID.h"
#include "nsGkAtoms.h"
#include "txLog.h"
#include "txMozillaXSLTProcessor.h"
#include "txStylesheetCompiler.h"
#include "txXMLUtils.h"
#include "nsAttrName.h"
#include "nsIScriptError.h"
#include "nsIURL.h"
#include "nsCrossSiteListenerProxy.h"
#include "nsError.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/EncodingUtils.h"

using namespace mozilla;
using mozilla::dom::EncodingUtils;

static NS_DEFINE_CID(kCParserCID, NS_PARSER_CID);

static void
getSpec(nsIChannel* aChannel, nsAString& aSpec)
{
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

class txStylesheetSink MOZ_FINAL : public nsIXMLContentSink,
                                   public nsIExpatSink,
                                   public nsIStreamListener,
                                   public nsIInterfaceRequestor
{
public:
    txStylesheetSink(txStylesheetCompiler* aCompiler, nsIParser* aParser);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIEXPATSINK
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSIINTERFACEREQUESTOR

    // nsIContentSink
    NS_IMETHOD WillParse(void) { return NS_OK; }
    NS_IMETHOD DidBuildModel(bool aTerminated);
    NS_IMETHOD WillInterrupt(void) { return NS_OK; }
    NS_IMETHOD WillResume(void) { return NS_OK; }
    NS_IMETHOD SetParser(nsParserBase* aParser) { return NS_OK; }
    virtual void FlushPendingNotifications(mozFlushType aType) { }
    NS_IMETHOD SetDocumentCharset(nsACString& aCharset) { return NS_OK; }
    virtual nsISupports *GetTarget() { return nullptr; }

private:
    nsRefPtr<txStylesheetCompiler> mCompiler;
    nsCOMPtr<nsIStreamListener> mListener;
    bool mCheckedForXML;

protected:
    ~txStylesheetSink() {}

    // This exists solely to suppress a warning from nsDerivedSafe
    txStylesheetSink();
};

txStylesheetSink::txStylesheetSink(txStylesheetCompiler* aCompiler,
                                   nsIParser* aParser)
    : mCompiler(aCompiler),
      mCheckedForXML(false)
{
    mListener = do_QueryInterface(aParser);
}

NS_IMPL_ISUPPORTS(txStylesheetSink,
                  nsIXMLContentSink,
                  nsIContentSink,
                  nsIExpatSink,
                  nsIStreamListener,
                  nsIRequestObserver,
                  nsIInterfaceRequestor)

NS_IMETHODIMP
txStylesheetSink::HandleStartElement(const char16_t *aName,
                                     const char16_t **aAtts,
                                     uint32_t aAttsCount,
                                     uint32_t aLineNumber)
{
    NS_PRECONDITION(aAttsCount % 2 == 0, "incorrect aAttsCount");

    nsresult rv =
        mCompiler->startElement(aName, aAtts, aAttsCount / 2);
    if (NS_FAILED(rv)) {
        mCompiler->cancel(rv);

        return rv;
    }
    
    return NS_OK;
}

NS_IMETHODIMP
txStylesheetSink::HandleEndElement(const char16_t *aName)
{
    nsresult rv = mCompiler->endElement();
    if (NS_FAILED(rv)) {
        mCompiler->cancel(rv);

        return rv;
    }

    return NS_OK;
}

NS_IMETHODIMP
txStylesheetSink::HandleComment(const char16_t *aName)
{
    return NS_OK;
}

NS_IMETHODIMP
txStylesheetSink::HandleCDataSection(const char16_t *aData,
                                     uint32_t aLength)
{
    return HandleCharacterData(aData, aLength);
}

NS_IMETHODIMP
txStylesheetSink::HandleDoctypeDecl(const nsAString & aSubset,
                                    const nsAString & aName,
                                    const nsAString & aSystemId,
                                    const nsAString & aPublicId,
                                    nsISupports *aCatalogData)
{
    return NS_OK;
}

NS_IMETHODIMP
txStylesheetSink::HandleCharacterData(const char16_t *aData,
                                      uint32_t aLength)
{
    nsresult rv = mCompiler->characters(Substring(aData, aData + aLength));
    if (NS_FAILED(rv)) {
        mCompiler->cancel(rv);
        return rv;
    }

    return NS_OK;
}

NS_IMETHODIMP
txStylesheetSink::HandleProcessingInstruction(const char16_t *aTarget,
                                              const char16_t *aData)
{
    return NS_OK;
}

NS_IMETHODIMP
txStylesheetSink::HandleXMLDeclaration(const char16_t *aVersion,
                                       const char16_t *aEncoding,
                                       int32_t aStandalone)
{
    return NS_OK;
}

NS_IMETHODIMP
txStylesheetSink::ReportError(const char16_t *aErrorText,
                              const char16_t *aSourceText,
                              nsIScriptError *aError,
                              bool *_retval)
{
    NS_PRECONDITION(aError && aSourceText && aErrorText, "Check arguments!!!");

    // The expat driver should report the error.
    *_retval = true;

    mCompiler->cancel(NS_ERROR_FAILURE, aErrorText, aSourceText);

    return NS_OK;
}

NS_IMETHODIMP 
txStylesheetSink::DidBuildModel(bool aTerminated)
{  
    return mCompiler->doneLoading();
}

NS_IMETHODIMP
txStylesheetSink::OnDataAvailable(nsIRequest *aRequest, nsISupports *aContext,
                                  nsIInputStream *aInputStream,
                                  uint64_t aOffset, uint32_t aCount)
{
    if (!mCheckedForXML) {
        nsCOMPtr<nsIParser> parser = do_QueryInterface(aContext);
        nsCOMPtr<nsIDTD> dtd;
        parser->GetDTD(getter_AddRefs(dtd));
        if (dtd) {
            mCheckedForXML = true;
            if (!(dtd->GetType() & NS_IPARSER_FLAG_XML)) {
                nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
                nsAutoString spec;
                getSpec(channel, spec);
                mCompiler->cancel(NS_ERROR_XSLT_WRONG_MIME_TYPE, nullptr,
                                  spec.get());

                return NS_ERROR_XSLT_WRONG_MIME_TYPE;
            }
        }
    }

    return mListener->OnDataAvailable(aRequest, aContext, aInputStream,
                                      aOffset, aCount);
}

NS_IMETHODIMP
txStylesheetSink::OnStartRequest(nsIRequest *aRequest, nsISupports *aContext)
{
    int32_t charsetSource = kCharsetFromDocTypeDefault;

    nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);

    // check channel's charset...
    nsAutoCString charsetVal;
    nsAutoCString charset;
    if (NS_SUCCEEDED(channel->GetContentCharset(charsetVal))) {
        if (EncodingUtils::FindEncodingForLabel(charsetVal, charset)) {
            charsetSource = kCharsetFromChannel;
        }
    }

    if (charset.IsEmpty()) {
      charset.AssignLiteral("UTF-8");
    }

    nsCOMPtr<nsIParser> parser = do_QueryInterface(aContext);
    parser->SetDocumentCharset(charset, charsetSource);

    nsAutoCString contentType;
    channel->GetContentType(contentType);

    // Time to sniff! Note: this should go away once file channels do
    // sniffing themselves.
    nsCOMPtr<nsIURI> uri;
    channel->GetURI(getter_AddRefs(uri));
    bool sniff;
    if (NS_SUCCEEDED(uri->SchemeIs("file", &sniff)) && sniff &&
        contentType.Equals(UNKNOWN_CONTENT_TYPE)) {
        nsresult rv;
        nsCOMPtr<nsIStreamConverterService> serv =
            do_GetService("@mozilla.org/streamConverters;1", &rv);
        if (NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsIStreamListener> converter;
            rv = serv->AsyncConvertData(UNKNOWN_CONTENT_TYPE,
                                        "*/*",
                                        mListener,
                                        aContext,
                                        getter_AddRefs(converter));
            if (NS_SUCCEEDED(rv)) {
                mListener = converter;
            }
        }
    }

    return mListener->OnStartRequest(aRequest, aContext);
}

NS_IMETHODIMP
txStylesheetSink::OnStopRequest(nsIRequest *aRequest, nsISupports *aContext,
                                nsresult aStatusCode)
{
    bool success = true;

    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aRequest);
    if (httpChannel) {
        httpChannel->GetRequestSucceeded(&success);
    }

    nsresult result = aStatusCode;
    if (!success) {
        // XXX We sometimes want to use aStatusCode here, but the parser resets
        //     it to NS_ERROR_NOINTERFACE because we don't implement
        //     nsIHTMLContentSink.
        result = NS_ERROR_XSLT_NETWORK_ERROR;
    }
    else if (!mCheckedForXML) {
        nsCOMPtr<nsIParser> parser = do_QueryInterface(aContext);
        nsCOMPtr<nsIDTD> dtd;
        parser->GetDTD(getter_AddRefs(dtd));
        if (dtd && !(dtd->GetType() & NS_IPARSER_FLAG_XML)) {
            result = NS_ERROR_XSLT_WRONG_MIME_TYPE;
        }
    }

    if (NS_FAILED(result)) {
        nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
        nsAutoString spec;
        getSpec(channel, spec);
        mCompiler->cancel(result, nullptr, spec.get());
    }

    nsresult rv = mListener->OnStopRequest(aRequest, aContext, aStatusCode);
    mListener = nullptr;
    return rv;
}

NS_IMETHODIMP
txStylesheetSink::GetInterface(const nsIID& aIID, void** aResult)
{
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

        nsIAuthPrompt* rawPtr = nullptr;
        prompt.swap(rawPtr);
        *aResult = rawPtr;

        return NS_OK;
    }

    return NS_ERROR_NO_INTERFACE;
}

class txCompileObserver MOZ_FINAL : public txACompileObserver
{
public:
    txCompileObserver(txMozillaXSLTProcessor* aProcessor,
                      nsILoadGroup* aLoadGroup);

    TX_DECL_ACOMPILEOBSERVER
    NS_INLINE_DECL_REFCOUNTING(txCompileObserver)

    nsresult startLoad(nsIURI* aUri, txStylesheetCompiler* aCompiler,
                       nsIPrincipal* aSourcePrincipal);

private:
    nsRefPtr<txMozillaXSLTProcessor> mProcessor;
    nsCOMPtr<nsILoadGroup> mLoadGroup;
    nsCOMPtr<nsIPrincipal> mCallerPrincipal;

    // This exists solely to suppress a warning from nsDerivedSafe
    txCompileObserver();

    // Private destructor, to discourage deletion outside of Release():
    ~txCompileObserver()
    {
    }
};

txCompileObserver::txCompileObserver(txMozillaXSLTProcessor* aProcessor,
                                     nsILoadGroup* aLoadGroup)
    : mProcessor(aProcessor),
      mLoadGroup(aLoadGroup)
{
}

nsresult
txCompileObserver::loadURI(const nsAString& aUri,
                           const nsAString& aReferrerUri,
                           txStylesheetCompiler* aCompiler)
{
    if (mProcessor->IsLoadDisabled()) {
        return NS_ERROR_XSLT_LOAD_BLOCKED_ERROR;
    }

    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_NewURI(getter_AddRefs(uri), aUri);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIURI> referrerUri;
    rv = NS_NewURI(getter_AddRefs(referrerUri), aReferrerUri);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPrincipal> referrerPrincipal;
    rv = nsContentUtils::GetSecurityManager()->
      GetSimpleCodebasePrincipal(referrerUri,
                                 getter_AddRefs(referrerPrincipal));
    NS_ENSURE_SUCCESS(rv, rv);

    // Content Policy
    int16_t shouldLoad = nsIContentPolicy::ACCEPT;
    rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_STYLESHEET,
                                   uri,
                                   referrerPrincipal,
                                   nullptr,
                                   NS_LITERAL_CSTRING("application/xml"),
                                   nullptr,
                                   &shouldLoad);
    NS_ENSURE_SUCCESS(rv, rv);
    if (NS_CP_REJECTED(shouldLoad)) {
        return NS_ERROR_DOM_BAD_URI;
    }

    return startLoad(uri, aCompiler, referrerPrincipal);
}

void
txCompileObserver::onDoneCompiling(txStylesheetCompiler* aCompiler,
                                   nsresult aResult,
                                   const char16_t *aErrorText,
                                   const char16_t *aParam)
{
    if (NS_SUCCEEDED(aResult)) {
        mProcessor->setStylesheet(aCompiler->getStylesheet());
    }
    else {
        mProcessor->reportError(aResult, aErrorText, aParam);
    }
}

nsresult
txCompileObserver::startLoad(nsIURI* aUri, txStylesheetCompiler* aCompiler,
                             nsIPrincipal* aReferrerPrincipal)
{
    nsCOMPtr<nsIChannel> channel;
    nsresult rv = NS_NewChannel(getter_AddRefs(channel),
                                aUri,
                                aReferrerPrincipal,
                                nsILoadInfo::SEC_NORMAL,
                                nsIContentPolicy::TYPE_STYLESHEET);

    NS_ENSURE_SUCCESS(rv, rv);

    channel->SetLoadGroup(mLoadGroup);

    channel->SetContentType(NS_LITERAL_CSTRING("text/xml"));

    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
    if (httpChannel) {
        httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                      NS_LITERAL_CSTRING("*/*"),
                                      false);

        nsCOMPtr<nsIURI> referrerURI;
        aReferrerPrincipal->GetURI(getter_AddRefs(referrerURI));
        if (referrerURI) {
            httpChannel->SetReferrer(referrerURI);
        }
    }

    nsCOMPtr<nsIParser> parser = do_CreateInstance(kCParserCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsRefPtr<txStylesheetSink> sink = new txStylesheetSink(aCompiler, parser);
    NS_ENSURE_TRUE(sink, NS_ERROR_OUT_OF_MEMORY);

    channel->SetNotificationCallbacks(sink);

    parser->SetCommand(kLoadAsData);
    parser->SetContentSink(sink);
    parser->Parse(aUri);

    // Always install in case of redirects
    nsRefPtr<nsCORSListenerProxy> listener =
        new nsCORSListenerProxy(sink, aReferrerPrincipal, false);
    rv = listener->Init(channel);
    NS_ENSURE_SUCCESS(rv, rv);

    return channel->AsyncOpen(listener, parser);
}

nsresult
TX_LoadSheet(nsIURI* aUri, txMozillaXSLTProcessor* aProcessor,
             nsILoadGroup* aLoadGroup, nsIPrincipal* aCallerPrincipal)
{
    nsAutoCString spec;
    aUri->GetSpec(spec);
    PR_LOG(txLog::xslt, PR_LOG_ALWAYS, ("TX_LoadSheet: %s\n", spec.get()));

    // Content Policy
    int16_t shouldLoad = nsIContentPolicy::ACCEPT;
    nsresult rv =
        NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_STYLESHEET,
                                  aUri,
                                  aCallerPrincipal,
                                  aProcessor->GetSourceContentModel(),
                                  NS_LITERAL_CSTRING("application/xml"),
                                  nullptr,
                                  &shouldLoad);
    NS_ENSURE_SUCCESS(rv, rv);
    if (NS_CP_REJECTED(shouldLoad)) {
        return NS_ERROR_DOM_BAD_URI;
    }

    nsRefPtr<txCompileObserver> observer =
        new txCompileObserver(aProcessor, aLoadGroup);
    NS_ENSURE_TRUE(observer, NS_ERROR_OUT_OF_MEMORY);

    nsRefPtr<txStylesheetCompiler> compiler =
        new txStylesheetCompiler(NS_ConvertUTF8toUTF16(spec), observer);
    NS_ENSURE_TRUE(compiler, NS_ERROR_OUT_OF_MEMORY);

    return observer->startLoad(aUri, compiler, aCallerPrincipal);
}

/**
 * handling DOM->txStylesheet
 * Observer needs to do synchronous loads.
 */
static nsresult
handleNode(nsINode* aNode, txStylesheetCompiler* aCompiler)
{
    nsresult rv = NS_OK;
    
    if (aNode->IsElement()) {
        dom::Element* element = aNode->AsElement();

        uint32_t attsCount = element->GetAttrCount();
        nsAutoArrayPtr<txStylesheetAttr> atts;
        if (attsCount > 0) {
            atts = new txStylesheetAttr[attsCount];
            NS_ENSURE_TRUE(atts, NS_ERROR_OUT_OF_MEMORY);

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

        mozilla::dom::NodeInfo *ni = element->NodeInfo();

        rv = aCompiler->startElement(ni->NamespaceID(),
                                     ni->NameAtom(),
                                     ni->GetPrefixAtom(), atts,
                                     attsCount);
        NS_ENSURE_SUCCESS(rv, rv);

        // explicitly destroy the attrs here since we no longer need it
        atts = nullptr;

        for (nsIContent* child = element->GetFirstChild();
             child;
             child = child->GetNextSibling()) {
             
            rv = handleNode(child, aCompiler);
            NS_ENSURE_SUCCESS(rv, rv);
        }

        rv = aCompiler->endElement();
        NS_ENSURE_SUCCESS(rv, rv);
    }
    else if (aNode->IsNodeOfType(nsINode::eTEXT)) {
        nsAutoString chars;
        static_cast<nsIContent*>(aNode)->AppendTextTo(chars);
        rv = aCompiler->characters(chars);
        NS_ENSURE_SUCCESS(rv, rv);
    }
    else if (aNode->IsNodeOfType(nsINode::eDOCUMENT)) {
        for (nsIContent* child = aNode->GetFirstChild();
             child;
             child = child->GetNextSibling()) {
             
            rv = handleNode(child, aCompiler);
            NS_ENSURE_SUCCESS(rv, rv);
        }
    }

    return NS_OK;
}

class txSyncCompileObserver MOZ_FINAL : public txACompileObserver
{
public:
    explicit txSyncCompileObserver(txMozillaXSLTProcessor* aProcessor);

    TX_DECL_ACOMPILEOBSERVER
    NS_INLINE_DECL_REFCOUNTING(txSyncCompileObserver)

private:
    // Private destructor, to discourage deletion outside of Release():
    ~txSyncCompileObserver()
    {
    }

    nsRefPtr<txMozillaXSLTProcessor> mProcessor;
};

txSyncCompileObserver::txSyncCompileObserver(txMozillaXSLTProcessor* aProcessor)
  : mProcessor(aProcessor)
{
}

nsresult
txSyncCompileObserver::loadURI(const nsAString& aUri,
                               const nsAString& aReferrerUri,
                               txStylesheetCompiler* aCompiler)
{
    if (mProcessor->IsLoadDisabled()) {
        return NS_ERROR_XSLT_LOAD_BLOCKED_ERROR;
    }

    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_NewURI(getter_AddRefs(uri), aUri);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIURI> referrerUri;
    rv = NS_NewURI(getter_AddRefs(referrerUri), aReferrerUri);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPrincipal> referrerPrincipal;
    rv = nsContentUtils::GetSecurityManager()->
      GetSimpleCodebasePrincipal(referrerUri,
                                 getter_AddRefs(referrerPrincipal));
    NS_ENSURE_SUCCESS(rv, rv);

    // Content Policy
    int16_t shouldLoad = nsIContentPolicy::ACCEPT;
    rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_STYLESHEET,
                                   uri,
                                   referrerPrincipal,
                                   nullptr,
                                   NS_LITERAL_CSTRING("application/xml"),
                                   nullptr,
                                   &shouldLoad);
    NS_ENSURE_SUCCESS(rv, rv);
    if (NS_CP_REJECTED(shouldLoad)) {
        return NS_ERROR_DOM_BAD_URI;
    }

    // This is probably called by js, a loadGroup for the channel doesn't
    // make sense.
    nsCOMPtr<nsINode> source;
    if (mProcessor) {
      source =
        do_QueryInterface(mProcessor->GetSourceContentModel());
    }
    nsAutoSyncOperation sync(source ? source->OwnerDoc() : nullptr);
    nsCOMPtr<nsIDOMDocument> document;
    rv = nsSyncLoadService::LoadDocument(uri, referrerPrincipal, nullptr,
                                         false, getter_AddRefs(document));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDocument> doc = do_QueryInterface(document);
    rv = handleNode(doc, aCompiler);
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
                                            const char16_t *aErrorText,
                                            const char16_t *aParam)
{
}

nsresult
TX_CompileStylesheet(nsINode* aNode, txMozillaXSLTProcessor* aProcessor,
                     nsIPrincipal* aCallerPrincipal,
                     txStylesheet** aStylesheet)
{
    // If we move GetBaseURI to nsINode this can be simplified.
    nsCOMPtr<nsIDocument> doc = aNode->OwnerDoc();

    nsCOMPtr<nsIURI> uri;
    if (aNode->IsNodeOfType(nsINode::eCONTENT)) {
      uri = static_cast<nsIContent*>(aNode)->GetBaseURI();
    }
    else { 
      NS_ASSERTION(aNode->IsNodeOfType(nsINode::eDOCUMENT), "not a doc");
      uri = static_cast<nsIDocument*>(aNode)->GetBaseURI();
    }
    NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);
    
    nsAutoCString spec;
    uri->GetSpec(spec);
    NS_ConvertUTF8toUTF16 baseURI(spec);

    nsIURI* docUri = doc->GetDocumentURI();
    NS_ENSURE_TRUE(docUri, NS_ERROR_FAILURE);

    // We need to remove the ref, a URI with a ref would mean that we have an
    // embedded stylesheet.
    docUri->CloneIgnoringRef(getter_AddRefs(uri));
    NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);

    uri->GetSpec(spec);
    NS_ConvertUTF8toUTF16 stylesheetURI(spec);

    nsRefPtr<txSyncCompileObserver> obs =
        new txSyncCompileObserver(aProcessor);
    NS_ENSURE_TRUE(obs, NS_ERROR_OUT_OF_MEMORY);

    nsRefPtr<txStylesheetCompiler> compiler =
        new txStylesheetCompiler(stylesheetURI, obs);
    NS_ENSURE_TRUE(compiler, NS_ERROR_OUT_OF_MEMORY);

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
