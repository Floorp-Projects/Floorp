/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@propagandism.org>
 *   Ryan Jones <sciguyryan@gmail.com>
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

#include "nsCOMArray.h"
#include "nsIAuthPrompt.h"
#include "nsICharsetAlias.h"
#include "nsIDOMNode.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIExpatSink.h"
#include "nsIChannelEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsILoadGroup.h"
#include "nsINameSpaceManager.h"
#include "nsINodeInfo.h"
#include "nsIParser.h"
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
#include "txAtoms.h"
#include "txLog.h"
#include "txMozillaXSLTProcessor.h"
#include "txStylesheetCompiler.h"
#include "txXMLUtils.h"
#include "nsAttrName.h"
#include "nsIScriptError.h"
#include "nsIURL.h"
#include "nsCrossSiteListenerProxy.h"
#include "nsDOMError.h"
#include "mozilla/dom/Element.h"

using namespace mozilla;

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

    nsCAutoString spec;
    uri->GetSpec(spec);
    AppendUTF8toUTF16(spec, aSpec);
}

class txStylesheetSink : public nsIXMLContentSink,
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
    NS_IMETHOD DidBuildModel(PRBool aTerminated);
    NS_IMETHOD WillInterrupt(void) { return NS_OK; }
    NS_IMETHOD WillResume(void) { return NS_OK; }
    NS_IMETHOD SetParser(nsIParser* aParser) { return NS_OK; }
    virtual void FlushPendingNotifications(mozFlushType aType) { }
    NS_IMETHOD SetDocumentCharset(nsACString& aCharset) { return NS_OK; }
    virtual nsISupports *GetTarget() { return nsnull; }

private:
    nsRefPtr<txStylesheetCompiler> mCompiler;
    nsCOMPtr<nsIStreamListener> mListener;
    PRPackedBool mCheckedForXML;

protected:
    // This exists solely to suppress a warning from nsDerivedSafe
    txStylesheetSink();
};

txStylesheetSink::txStylesheetSink(txStylesheetCompiler* aCompiler,
                                   nsIParser* aParser)
    : mCompiler(aCompiler),
      mCheckedForXML(PR_FALSE)
{
    mListener = do_QueryInterface(aParser);
}

NS_IMPL_ISUPPORTS6(txStylesheetSink,
                   nsIXMLContentSink,
                   nsIContentSink,
                   nsIExpatSink,
                   nsIStreamListener,
                   nsIRequestObserver,
                   nsIInterfaceRequestor)

NS_IMETHODIMP
txStylesheetSink::HandleStartElement(const PRUnichar *aName,
                                     const PRUnichar **aAtts,
                                     PRUint32 aAttsCount,
                                     PRInt32 aIndex,
                                     PRUint32 aLineNumber)
{
    NS_PRECONDITION(aAttsCount % 2 == 0, "incorrect aAttsCount");

    nsresult rv =
        mCompiler->startElement(aName, aAtts, aAttsCount / 2, aIndex);
    if (NS_FAILED(rv)) {
        mCompiler->cancel(rv);

        return rv;
    }
    
    return NS_OK;
}

NS_IMETHODIMP
txStylesheetSink::HandleEndElement(const PRUnichar *aName)
{
    nsresult rv = mCompiler->endElement();
    if (NS_FAILED(rv)) {
        mCompiler->cancel(rv);

        return rv;
    }

    return NS_OK;
}

NS_IMETHODIMP
txStylesheetSink::HandleComment(const PRUnichar *aName)
{
    return NS_OK;
}

NS_IMETHODIMP
txStylesheetSink::HandleCDataSection(const PRUnichar *aData,
                                     PRUint32 aLength)
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
txStylesheetSink::HandleCharacterData(const PRUnichar *aData,
                                      PRUint32 aLength)
{
    nsresult rv = mCompiler->characters(Substring(aData, aData + aLength));
    if (NS_FAILED(rv)) {
        mCompiler->cancel(rv);
        return rv;
    }

    return NS_OK;
}

NS_IMETHODIMP
txStylesheetSink::HandleProcessingInstruction(const PRUnichar *aTarget,
                                              const PRUnichar *aData)
{
    return NS_OK;
}

NS_IMETHODIMP
txStylesheetSink::HandleXMLDeclaration(const PRUnichar *aVersion,
                                       const PRUnichar *aEncoding,
                                       PRInt32 aStandalone)
{
    return NS_OK;
}

NS_IMETHODIMP
txStylesheetSink::ReportError(const PRUnichar *aErrorText,
                              const PRUnichar *aSourceText,
                              nsIScriptError *aError,
                              PRBool *_retval)
{
    NS_PRECONDITION(aError && aSourceText && aErrorText, "Check arguments!!!");

    // The expat driver should report the error.
    *_retval = PR_TRUE;

    mCompiler->cancel(NS_ERROR_FAILURE, aErrorText, aSourceText);

    return NS_OK;
}

NS_IMETHODIMP 
txStylesheetSink::DidBuildModel(PRBool aTerminated)
{  
    return mCompiler->doneLoading();
}

NS_IMETHODIMP
txStylesheetSink::OnDataAvailable(nsIRequest *aRequest, nsISupports *aContext,
                                  nsIInputStream *aInputStream,
                                  PRUint32 aOffset, PRUint32 aCount)
{
    if (!mCheckedForXML) {
        nsCOMPtr<nsIParser> parser = do_QueryInterface(aContext);
        nsCOMPtr<nsIDTD> dtd;
        parser->GetDTD(getter_AddRefs(dtd));
        if (dtd) {
            mCheckedForXML = PR_TRUE;
            if (!(dtd->GetType() & NS_IPARSER_FLAG_XML)) {
                nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
                nsAutoString spec;
                getSpec(channel, spec);
                mCompiler->cancel(NS_ERROR_XSLT_WRONG_MIME_TYPE, nsnull,
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
    nsCAutoString charset(NS_LITERAL_CSTRING("UTF-8"));
    PRInt32 charsetSource = kCharsetFromDocTypeDefault;

    nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);

    // check channel's charset...
    nsCAutoString charsetVal;
    nsresult rv = channel->GetContentCharset(charsetVal);
    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsICharsetAlias> calias =
            do_GetService(NS_CHARSETALIAS_CONTRACTID);

        if (calias) {
            nsCAutoString preferred;
            rv = calias->GetPreferred(charsetVal,
                                      preferred);
            if (NS_SUCCEEDED(rv)) {            
                charset = preferred;
                charsetSource = kCharsetFromChannel;
             }
        }
    }

    nsCOMPtr<nsIParser> parser = do_QueryInterface(aContext);
    parser->SetDocumentCharset(charset, charsetSource);

    nsCAutoString contentType;
    channel->GetContentType(contentType);

    // Time to sniff! Note: this should go away once file channels do
    // sniffing themselves.
    nsCOMPtr<nsIURI> uri;
    channel->GetURI(getter_AddRefs(uri));
    PRBool sniff;
    if (NS_SUCCEEDED(uri->SchemeIs("file", &sniff)) && sniff &&
        contentType.Equals(UNKNOWN_CONTENT_TYPE)) {
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
    PRBool success = PR_TRUE;

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
        mCompiler->cancel(result, nsnull, spec.get());
    }

    nsresult rv = mListener->OnStopRequest(aRequest, aContext, aStatusCode);
    mListener = nsnull;
    return rv;
}

NS_IMETHODIMP
txStylesheetSink::GetInterface(const nsIID& aIID, void** aResult)
{
    if (aIID.Equals(NS_GET_IID(nsIAuthPrompt))) {
        NS_ENSURE_ARG(aResult);
        *aResult = nsnull;

        nsresult rv;
        nsCOMPtr<nsIWindowWatcher> wwatcher =
            do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIAuthPrompt> prompt;
        rv = wwatcher->GetNewAuthPrompter(nsnull, getter_AddRefs(prompt));
        NS_ENSURE_SUCCESS(rv, rv);

        nsIAuthPrompt* rawPtr = nsnull;
        prompt.swap(rawPtr);
        *aResult = rawPtr;

        return NS_OK;
    }

    return NS_ERROR_NO_INTERFACE;
}

class txCompileObserver : public txACompileObserver
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

protected:
    // This exists solely to suppress a warning from nsDerivedSafe
    txCompileObserver();
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
      GetCodebasePrincipal(referrerUri, getter_AddRefs(referrerPrincipal));
    NS_ENSURE_SUCCESS(rv, rv);

    // Content Policy
    PRInt16 shouldLoad = nsIContentPolicy::ACCEPT;
    rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_STYLESHEET,
                                   uri,
                                   referrerPrincipal,
                                   nsnull,
                                   NS_LITERAL_CSTRING("application/xml"),
                                   nsnull,
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
                                   const PRUnichar *aErrorText,
                                   const PRUnichar *aParam)
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
    nsresult rv = NS_NewChannel(getter_AddRefs(channel), aUri);
    NS_ENSURE_SUCCESS(rv, rv);

    channel->SetLoadGroup(mLoadGroup);

    channel->SetContentType(NS_LITERAL_CSTRING("text/xml"));

    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
    if (httpChannel) {
        httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                      NS_LITERAL_CSTRING("*/*"),
                                      PR_FALSE);

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
    nsCOMPtr<nsIStreamListener> listener =
        new nsCORSListenerProxy(sink, aReferrerPrincipal, channel,
                                PR_FALSE, &rv);
    NS_ENSURE_TRUE(listener, NS_ERROR_OUT_OF_MEMORY);
    NS_ENSURE_SUCCESS(rv, rv);

    return channel->AsyncOpen(listener, parser);
}

nsresult
TX_LoadSheet(nsIURI* aUri, txMozillaXSLTProcessor* aProcessor,
             nsILoadGroup* aLoadGroup, nsIPrincipal* aCallerPrincipal)
{
    nsCAutoString spec;
    aUri->GetSpec(spec);
    PR_LOG(txLog::xslt, PR_LOG_ALWAYS, ("TX_LoadSheet: %s\n", spec.get()));

    // Content Policy
    PRInt16 shouldLoad = nsIContentPolicy::ACCEPT;
    nsresult rv =
        NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_STYLESHEET,
                                  aUri,
                                  aCallerPrincipal,
                                  aProcessor->GetSourceContentModel(),
                                  NS_LITERAL_CSTRING("application/xml"),
                                  nsnull,
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

        PRUint32 attsCount = element->GetAttrCount();
        nsAutoArrayPtr<txStylesheetAttr> atts;
        if (attsCount > 0) {
            atts = new txStylesheetAttr[attsCount];
            NS_ENSURE_TRUE(atts, NS_ERROR_OUT_OF_MEMORY);

            PRUint32 counter;
            for (counter = 0; counter < attsCount; ++counter) {
                txStylesheetAttr& att = atts[counter];
                const nsAttrName* name = element->GetAttrNameAt(counter);
                att.mNamespaceID = name->NamespaceID();
                att.mLocalName = name->LocalName();
                att.mPrefix = name->GetPrefix();
                element->GetAttr(att.mNamespaceID, att.mLocalName, att.mValue);
            }
        }

        nsINodeInfo *ni = element->NodeInfo();

        rv = aCompiler->startElement(ni->NamespaceID(),
                                     ni->NameAtom(),
                                     ni->GetPrefixAtom(), atts,
                                     attsCount);
        NS_ENSURE_SUCCESS(rv, rv);

        // explicitly destroy the attrs here since we no longer need it
        atts = nsnull;

        PRUint32 childCount = element->GetChildCount();
        if (childCount > 0) {
            PRUint32 counter = 0;
            nsIContent *child;
            while ((child = element->GetChildAt(counter++))) {
                rv = handleNode(child, aCompiler);
                NS_ENSURE_SUCCESS(rv, rv);
            }
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
        nsIDocument* document = static_cast<nsIDocument*>(aNode);

        PRUint32 counter = 0;
        nsIContent *child;
        while ((child = document->GetChildAt(counter++))) {
            rv = handleNode(child, aCompiler);
            NS_ENSURE_SUCCESS(rv, rv);
        }
    }

    return NS_OK;
}

class txSyncCompileObserver : public txACompileObserver
{
public:
    txSyncCompileObserver(txMozillaXSLTProcessor* aProcessor);

    TX_DECL_ACOMPILEOBSERVER
    NS_INLINE_DECL_REFCOUNTING(txSyncCompileObserver)

protected:
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
      GetCodebasePrincipal(referrerUri, getter_AddRefs(referrerPrincipal));
    NS_ENSURE_SUCCESS(rv, rv);

    // Content Policy
    PRInt16 shouldLoad = nsIContentPolicy::ACCEPT;
    rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_STYLESHEET,
                                   uri,
                                   referrerPrincipal,
                                   nsnull,
                                   NS_LITERAL_CSTRING("application/xml"),
                                   nsnull,
                                   &shouldLoad);
    NS_ENSURE_SUCCESS(rv, rv);
    if (NS_CP_REJECTED(shouldLoad)) {
        return NS_ERROR_DOM_BAD_URI;
    }

    // This is probably called by js, a loadGroup for the channel doesn't
    // make sense.
    nsCOMPtr<nsIDOMDocument> document;
    rv = nsSyncLoadService::LoadDocument(uri, referrerPrincipal, nsnull,
                                         PR_FALSE, getter_AddRefs(document));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDocument> doc = do_QueryInterface(document);
    rv = handleNode(doc, aCompiler);
    if (NS_FAILED(rv)) {
        nsCAutoString spec;
        uri->GetSpec(spec);
        aCompiler->cancel(rv, nsnull, NS_ConvertUTF8toUTF16(spec).get());
        return rv;
    }

    rv = aCompiler->doneLoading();
    return rv;
}

void txSyncCompileObserver::onDoneCompiling(txStylesheetCompiler* aCompiler,
                                            nsresult aResult,
                                            const PRUnichar *aErrorText,
                                            const PRUnichar *aParam)
{
}

nsresult
TX_CompileStylesheet(nsINode* aNode, txMozillaXSLTProcessor* aProcessor,
                     nsIPrincipal* aCallerPrincipal,
                     txStylesheet** aStylesheet)
{
    // If we move GetBaseURI to nsINode this can be simplified.
    nsCOMPtr<nsIDocument> doc = aNode->GetOwnerDoc();
    NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

    nsCOMPtr<nsIURI> uri;
    if (aNode->IsNodeOfType(nsINode::eCONTENT)) {
      uri = static_cast<nsIContent*>(aNode)->GetBaseURI();
    }
    else { 
      NS_ASSERTION(aNode->IsNodeOfType(nsINode::eDOCUMENT), "not a doc");
      uri = static_cast<nsIDocument*>(aNode)->GetBaseURI();
    }
    NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);
    
    nsCAutoString spec;
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
