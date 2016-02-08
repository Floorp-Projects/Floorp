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
#include "nsCORSListenerProxy.h"
#include "nsError.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/EncodingUtils.h"
#include "mozilla/UniquePtr.h"

using namespace mozilla;
using mozilla::dom::EncodingUtils;
using mozilla::net::ReferrerPolicy;

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

class txStylesheetSink final : public nsIXMLContentSink,
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
    NS_IMETHOD WillParse(void) override { return NS_OK; }
    NS_IMETHOD DidBuildModel(bool aTerminated) override;
    NS_IMETHOD WillInterrupt(void) override { return NS_OK; }
    NS_IMETHOD WillResume(void) override { return NS_OK; }
    NS_IMETHOD SetParser(nsParserBase* aParser) override { return NS_OK; }
    virtual void FlushPendingNotifications(mozFlushType aType) override { }
    NS_IMETHOD SetDocumentCharset(nsACString& aCharset) override { return NS_OK; }
    virtual nsISupports *GetTarget() override { return nullptr; }

private:
    RefPtr<txStylesheetCompiler> mCompiler;
    nsCOMPtr<nsIStreamListener>    mListener;
    nsCOMPtr<nsIParser>            mParser;
    bool mCheckedForXML;

protected:
    ~txStylesheetSink() {}

    // This exists solely to suppress a warning from nsDerivedSafe
    txStylesheetSink();
};

txStylesheetSink::txStylesheetSink(txStylesheetCompiler* aCompiler,
                                   nsIParser* aParser)
    : mCompiler(aCompiler)
    , mParser(aParser)
    , mCheckedForXML(false)
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
        nsCOMPtr<nsIDTD> dtd;
        mParser->GetDTD(getter_AddRefs(dtd));
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

    return mListener->OnDataAvailable(aRequest, mParser, aInputStream,
                                      aOffset, aCount);
}

NS_IMETHODIMP
txStylesheetSink::OnStartRequest(nsIRequest *aRequest, nsISupports *aContext)
{
    int32_t charsetSource = kCharsetFromDocTypeDefault;

    nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);

    nsCOMPtr<nsIPrincipal> channelPrincipal;
    nsContentUtils::GetSecurityManager()->GetChannelResultPrincipal(
        channel, getter_AddRefs(channelPrincipal));
    mCompiler->setPrincipal(channelPrincipal);

    nsCOMPtr<nsIURI> baseURI;
    nsresult rv = NS_GetFinalChannelURI(channel, getter_AddRefs(baseURI));
    NS_ENSURE_SUCCESS(rv, rv);
    mCompiler->setBaseURI(baseURI);

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

    mParser->SetDocumentCharset(charset, charsetSource);

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
                                        mParser,
                                        getter_AddRefs(converter));
            if (NS_SUCCEEDED(rv)) {
                mListener = converter;
            }
        }
    }

    return mListener->OnStartRequest(aRequest, mParser);
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
        nsCOMPtr<nsIDTD> dtd;
        mParser->GetDTD(getter_AddRefs(dtd));
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

    nsresult rv = mListener->OnStopRequest(aRequest, mParser, aStatusCode);
    mListener = nullptr;
    mParser = nullptr;
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

class txCompileObserver final : public txACompileObserver
{
public:
    txCompileObserver(txMozillaXSLTProcessor* aProcessor,
                      nsIDocument* aLoaderDocument);

    TX_DECL_ACOMPILEOBSERVER
    NS_INLINE_DECL_REFCOUNTING(txCompileObserver)

    nsresult startLoad(nsIURI* aUri, nsIPrincipal* aSourcePrincipal,
                       txStylesheetCompiler* aCompiler);

private:
    RefPtr<txMozillaXSLTProcessor> mProcessor;
    nsCOMPtr<nsIDocument> mLoaderDocument;

    // This exists solely to suppress a warning from nsDerivedSafe
    txCompileObserver();

    // Private destructor, to discourage deletion outside of Release():
    ~txCompileObserver()
    {
    }
};

txCompileObserver::txCompileObserver(txMozillaXSLTProcessor* aProcessor,
                                     nsIDocument* aLoaderDocument)
    : mProcessor(aProcessor),
      mLoaderDocument(aLoaderDocument)
{
}

nsresult
txCompileObserver::loadURI(nsIURI* aUri,
                           nsIPrincipal* aReferrerPrincipal,
                           txStylesheetCompiler* aCompiler)
{
    if (mProcessor->IsLoadDisabled() || !mLoaderDocument) {
        return NS_ERROR_XSLT_LOAD_BLOCKED_ERROR;
    }

    return startLoad(aUri, aReferrerPrincipal, aCompiler);
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
txCompileObserver::startLoad(nsIURI* aUri, nsIPrincipal* aReferrerPrincipal,
                             txStylesheetCompiler* aCompiler)
{
    MOZ_ASSERT(aReferrerPrincipal);

    nsCOMPtr<nsILoadGroup> loadGroup = mLoaderDocument->GetDocumentLoadGroup();
    if (!loadGroup) {
        return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIChannel> channel;
    nsresult rv = NS_NewChannelWithTriggeringPrincipal(
                    getter_AddRefs(channel),
                    aUri,
                    mLoaderDocument,
                    aReferrerPrincipal, // triggeringPrincipal
                    nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS,
                    nsIContentPolicy::TYPE_XSLT,
                    loadGroup);

    NS_ENSURE_SUCCESS(rv, rv);

    channel->SetContentType(NS_LITERAL_CSTRING("text/xml"));

    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
    if (httpChannel) {
        httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                      NS_LITERAL_CSTRING("*/*"),
                                      false);

        nsCOMPtr<nsIURI> referrerURI;
        aReferrerPrincipal->GetURI(getter_AddRefs(referrerURI));
        if (referrerURI) {
            httpChannel->SetReferrerWithPolicy(referrerURI,
                mLoaderDocument->GetReferrerPolicy());
        }
    }

    nsCOMPtr<nsIParser> parser = do_CreateInstance(kCParserCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    RefPtr<txStylesheetSink> sink = new txStylesheetSink(aCompiler, parser);

    channel->SetNotificationCallbacks(sink);

    parser->SetCommand(kLoadAsData);
    parser->SetContentSink(sink);
    parser->Parse(aUri);

    return channel->AsyncOpen2(sink);
}

nsresult
TX_LoadSheet(nsIURI* aUri, txMozillaXSLTProcessor* aProcessor,
             nsIDocument* aLoaderDocument)
{
    if (MOZ_LOG_TEST(txLog::xslt, LogLevel::Info)) {
        nsAutoCString spec;
        aUri->GetSpec(spec);
        MOZ_LOG(txLog::xslt, LogLevel::Info,
                ("TX_LoadSheet: %s\n", spec.get()));
    }

    RefPtr<txCompileObserver> observer =
        new txCompileObserver(aProcessor, aLoaderDocument);

    nsAutoCString fragment;
    aUri->GetRef(fragment);

    RefPtr<txStylesheetCompiler> compiler =
        new txStylesheetCompiler(NS_ConvertUTF8toUTF16(fragment), observer);

    return observer->startLoad(aUri, aLoaderDocument->NodePrincipal(),
                               compiler);
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

        mozilla::dom::NodeInfo *ni = element->NodeInfo();

        rv = aCompiler->startElement(ni->NamespaceID(),
                                     ni->NameAtom(),
                                     ni->GetPrefixAtom(), atts.get(),
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

class txSyncCompileObserver final : public txACompileObserver
{
public:
    explicit txSyncCompileObserver(txMozillaXSLTProcessor* aProcessor,
                                   nsIDocument* aLoaderDocument)
        : mProcessor(aProcessor),
          mLoaderDocument(aLoaderDocument)
    {}

    TX_DECL_ACOMPILEOBSERVER
    NS_INLINE_DECL_REFCOUNTING(txSyncCompileObserver)

private:
    // Private destructor, to discourage deletion outside of Release():
    ~txSyncCompileObserver()
    {
    }

    RefPtr<txMozillaXSLTProcessor> mProcessor;
    nsCOMPtr<nsIDocument> mLoaderDocument;
};

nsresult
txSyncCompileObserver::loadURI(nsIURI* aUri,
                               nsIPrincipal* aReferrerPrincipal,
                               txStylesheetCompiler* aCompiler)
{
    MOZ_ASSERT(aReferrerPrincipal);

    if (mProcessor->IsLoadDisabled() || !mLoaderDocument) {
        return NS_ERROR_XSLT_LOAD_BLOCKED_ERROR;
    }

    nsAutoSyncOperation sync(mLoaderDocument);
    nsCOMPtr<nsIDOMDocument> document;

    nsCOMPtr<nsILoadGroup> loadGroup = mLoaderDocument->GetDocumentLoadGroup();
    nsresult rv =
        nsSyncLoadService::LoadDocument(aUri,
                                        nsIContentPolicy::TYPE_XSLT,
                                        aReferrerPrincipal,
                                        nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS,
                                        loadGroup,
                                        false,
                                        mLoaderDocument->GetReferrerPolicy(),
                                        getter_AddRefs(document));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDocument> doc = do_QueryInterface(document);
    nsCOMPtr<nsIURI> baseURI = doc->GetBaseURI();
    aCompiler->setBaseURI(baseURI);
    aCompiler->setPrincipal(doc->NodePrincipal());
    rv = handleNode(doc, aCompiler);
    if (NS_FAILED(rv)) {
        nsAutoCString spec;
        aUri->GetSpec(spec);
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
TX_CompileStylesheet(nsINode* aNode,
                     nsIDocument* aLoaderDocument,
                     txMozillaXSLTProcessor* aProcessor,
                     txStylesheet** aStylesheet)
{
    RefPtr<txSyncCompileObserver> obs =
        new txSyncCompileObserver(aProcessor, aLoaderDocument);

    RefPtr<txStylesheetCompiler> compiler =
        new txStylesheetCompiler(EmptyString(), obs);

    nsCOMPtr<nsIURI> baseURI = aNode->GetBaseURI();
    compiler->setBaseURI(baseURI);
    compiler->setPrincipal(aNode->NodePrincipal());

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
