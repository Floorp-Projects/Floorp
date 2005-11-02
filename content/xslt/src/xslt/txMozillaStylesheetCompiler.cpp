/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@netscape.com> (original author)
 *
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
#include "nsICharsetAlias.h"
#include "nsIDocument.h"
#include "nsIExpatSink.h"
#include "nsILoadGroup.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "nsINodeInfo.h"
#include "nsIParser.h"
#include "nsISyncLoadDOMService.h"
#include "nsIURI.h"
#include "nsIXMLContentSink.h"
#include "nsNetUtil.h"
#include "nsParserCIID.h"
#include "txAtoms.h"
#include "txMozillaXSLTProcessor.h"
#include "txStylesheetCompiler.h"
#include "XMLUtils.h"

static const char kLoadAsData[] = "loadAsData";

class txStylesheetSink : public nsIXMLContentSink,
                         public nsIExpatSink
{
public:
    txStylesheetSink(txStylesheetCompiler* aCompiler);
    virtual ~txStylesheetSink();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIEXPATSINK

    // nsIContentSink
    NS_IMETHOD WillBuildModel(void) { return NS_OK; }
    NS_IMETHOD DidBuildModel(PRInt32 aQualityLevel);
    NS_IMETHOD WillInterrupt(void) { return NS_OK; }
    NS_IMETHOD WillResume(void) { return NS_OK; }
    NS_IMETHOD SetParser(nsIParser* aParser) { return NS_OK; }
    NS_IMETHOD FlushPendingNotifications() { return NS_OK; }
    NS_IMETHOD SetDocumentCharset(nsAString& aCharset) { return NS_OK; }

private:
    nsRefPtr<txStylesheetCompiler> mCompiler;

protected:
    // This exists soly to supress a warning from nsDerivedSafe
    txStylesheetSink();
};

txStylesheetSink::txStylesheetSink(txStylesheetCompiler* aCompiler)
    : mCompiler(aCompiler)
{
}

txStylesheetSink::~txStylesheetSink()
{
}

NS_IMPL_ISUPPORTS2(txStylesheetSink, nsIXMLContentSink, nsIExpatSink)

NS_IMETHODIMP
txStylesheetSink::HandleStartElement(const PRUnichar *aName,
                                     const PRUnichar **aAtts,
                                     PRUint32 aAttsCount,
                                     PRUint32 aIndex,
                                     PRUint32 aLineNumber)
{
    nsresult rv = mCompiler->startElement(aName, aAtts, aAttsCount);
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
txStylesheetSink::HandleXMLDeclaration(const PRUnichar *aData,
                                       PRUint32 aLength)
{
    return NS_OK;
}

NS_IMETHODIMP
txStylesheetSink::ReportError(const PRUnichar *aErrorText,
                              const PRUnichar *aSourceText)
{
    mCompiler->cancel(NS_ERROR_FAILURE);
    return NS_OK;
}

NS_IMETHODIMP 
txStylesheetSink::DidBuildModel(PRInt32 aQualityLevel)
{  
    return mCompiler->doneLoading();
}

class txCompileObserver : public txACompileObserver
{
public:
    txCompileObserver(txMozillaXSLTProcessor* aProcessor,
                      nsILoadGroup* aLoadGroup);
    virtual ~txCompileObserver();

    TX_DECL_ACOMPILEOBSERVER;

    nsresult startLoad(nsIURI* aUri, txStylesheetSink* aSink,
                       nsIURI* aReferrerUri);

protected:
    nsAutoRefCnt mRefCnt;

private:
    nsRefPtr<txMozillaXSLTProcessor> mProcessor;
    nsCOMPtr<nsILoadGroup> mLoadGroup;

protected:
    // This exists soly to supress a warning from nsDerivedSafe
    txCompileObserver();
};

txCompileObserver::txCompileObserver(txMozillaXSLTProcessor* aProcessor,
                                     nsILoadGroup* aLoadGroup)
    : mProcessor(aProcessor),
      mLoadGroup(aLoadGroup)
{
}

txCompileObserver::~txCompileObserver()
{
}

nsrefcnt
txCompileObserver::AddRef()
{
    return ++mRefCnt;
}

nsrefcnt
txCompileObserver::Release()
{
    if (--mRefCnt == 0) {
        mRefCnt = 1; //stabilize
        delete this;
        return 0;
    }
    return mRefCnt;
}

nsresult
txCompileObserver::loadURI(const nsAString& aUri,
                           txStylesheetCompiler* aCompiler)
{
    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_NewURI(getter_AddRefs(uri), aUri);
    NS_ENSURE_SUCCESS(rv, rv);

    nsRefPtr<txStylesheetSink> sink = new txStylesheetSink(aCompiler);
    NS_ENSURE_TRUE(sink, NS_ERROR_OUT_OF_MEMORY);

    return startLoad(uri, sink, nsnull);
}

void
txCompileObserver::onDoneCompiling(txStylesheetCompiler* aCompiler,
                                   nsresult aResult)
{
    mProcessor->setStylesheet(aCompiler->getStylesheet());
}

nsresult
txCompileObserver::startLoad(nsIURI* aUri, txStylesheetSink* aSink,
                             nsIURI* aReferrerUri)
{
    nsCOMPtr<nsIChannel> channel;
    nsresult rv = NS_NewChannel(getter_AddRefs(channel), aUri);
    NS_ENSURE_SUCCESS(rv, rv);

    channel->SetLoadGroup(mLoadGroup);

    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
    if (httpChannel) {
        httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                      NS_LITERAL_CSTRING("text/xml,application/xml,application/xhtml+xml,*/*;q=0.1"),
                                      PR_FALSE);

        if (aReferrerUri) {
            httpChannel->SetReferrer(aReferrerUri);
        }
    }

    nsAutoString charset(NS_LITERAL_STRING("UTF-8"));
    PRInt32 charsetSource = kCharsetFromDocTypeDefault;

    // check channel's charset...
    nsCAutoString charsetVal;
    rv = channel->GetContentCharset(charsetVal);
    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsICharsetAlias> calias =
            do_GetService(NS_CHARSETALIAS_CONTRACTID);

        if (calias) {
            nsAutoString preferred;
            rv = calias->GetPreferred(NS_ConvertASCIItoUCS2(charsetVal),
                                      preferred);
            if (NS_SUCCEEDED(rv)) {            
                charset = preferred;
                charsetSource = kCharsetFromChannel;
             }
        }
    }

    static NS_DEFINE_CID(kCParserCID, NS_PARSER_CID);
    nsCOMPtr<nsIParser> parser = do_CreateInstance(kCParserCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // Set the parser as the stream listener for the document loader...
    nsCOMPtr<nsIStreamListener> listener = do_QueryInterface(parser, &rv);
    NS_ENSURE_TRUE(listener, rv);

    parser->SetDocumentCharset(charset, charsetSource);
    parser->SetCommand(kLoadAsData);
    parser->SetContentSink(aSink);
    parser->Parse(aUri);

    return channel->AsyncOpen(listener, nsnull);
}

nsresult
TX_LoadSheet(nsIURI* aUri, txMozillaXSLTProcessor* aProcessor,
             nsILoadGroup* aLoadGroup, nsIURI* aReferrerUri)
{
    nsCAutoString uri;
    aUri->GetSpec(uri);
    PR_LOG(txLog::xslt, PR_LOG_ALWAYS, ("TX_LoadSheet: %s\n", uri.get()));

    nsRefPtr<txCompileObserver> observer =
        new txCompileObserver(aProcessor, aLoadGroup);
    NS_ENSURE_TRUE(observer, NS_ERROR_OUT_OF_MEMORY);

    nsRefPtr<txStylesheetCompiler> compiler =
        new txStylesheetCompiler(NS_ConvertUTF8toUCS2(uri), observer);
    NS_ENSURE_TRUE(compiler, NS_ERROR_OUT_OF_MEMORY);

    nsRefPtr<txStylesheetSink> sink = new txStylesheetSink(compiler);
    NS_ENSURE_TRUE(sink, NS_ERROR_OUT_OF_MEMORY);

    return observer->startLoad(aUri, sink, aReferrerUri);
}

/**
 * handling DOM->txStylesheet
 * Observer needs to do synchronous loads.
 */
static nsresult
handleNode(nsIDOMNode* aNode, txStylesheetCompiler* aCompiler)
{
    nsresult rv = NS_OK;
    PRUint16 nodetype;
    aNode->GetNodeType(&nodetype);
    switch (nodetype) {
        case nsIDOMNode::ELEMENT_NODE:
        {
            nsCOMPtr<nsIContent> element = do_QueryInterface(aNode);

            nsCOMPtr<nsINodeInfo> ni;
            element->GetNodeInfo(*getter_AddRefs(ni));

            PRInt32 namespaceID;
            nsCOMPtr<nsIAtom> prefix, localname;
            ni->GetNamespaceID(namespaceID);
            ni->GetNameAtom(*getter_AddRefs(localname));
            ni->GetPrefixAtom(*getter_AddRefs(prefix));

            PRInt32 attsCount;
            element->GetAttrCount(attsCount);
            nsAutoArrayPtr<txStylesheetAttr> atts;
            if (attsCount > 0) {
                atts = new txStylesheetAttr[attsCount];
                NS_ENSURE_TRUE(atts, NS_ERROR_OUT_OF_MEMORY);

                PRInt32 counter;
                for (counter = 0; counter < attsCount; ++counter) {
                    txStylesheetAttr& att = atts[counter];
                    element->GetAttrNameAt(counter, att.mNamespaceID,
                                           *getter_AddRefs(att.mLocalName),
                                           *getter_AddRefs(att.mPrefix));
                    element->GetAttr(att.mNamespaceID, att.mLocalName, att.mValue);
                }
            }

            rv = aCompiler->startElement(namespaceID, localname, prefix, atts,
                                         attsCount);
            NS_ENSURE_SUCCESS(rv, rv);

            // explicitly destroy the attrs here since we no longer need it
            atts = nsnull;

            PRInt32 childCount;
            element->ChildCount(childCount);
            if (childCount > 0) {
                PRInt32 counter = 0;
                nsCOMPtr<nsIContent> child;
                while (NS_SUCCEEDED(element->ChildAt(counter++, *getter_AddRefs(child))) && child) {
                    nsCOMPtr<nsIDOMNode> childNode = do_QueryInterface(child);
                    rv = handleNode(childNode, aCompiler);
                    NS_ENSURE_SUCCESS(rv, rv);
                }
            }

            rv = aCompiler->endElement();
            NS_ENSURE_SUCCESS(rv, rv);

            break;
        }
        case nsIDOMNode::CDATA_SECTION_NODE:
        case nsIDOMNode::TEXT_NODE:
        {
            nsAutoString chars;
            aNode->GetNodeValue(chars);
            rv = aCompiler->characters(chars);
            NS_ENSURE_SUCCESS(rv, rv);

            break;
        }
        case nsIDOMNode::DOCUMENT_NODE:
        {
            nsCOMPtr<nsIDocument> document = do_QueryInterface(aNode);
            PRInt32 childCount;
            document->GetChildCount(childCount);
            if (childCount > 0) {
                PRInt32 counter = 0;
                nsCOMPtr<nsIContent> child;
                while (NS_SUCCEEDED(document->ChildAt(counter++, *getter_AddRefs(child))) && child) {
                    nsCOMPtr<nsIDOMNode> childNode = do_QueryInterface(child);
                    rv = handleNode(childNode, aCompiler);
                    NS_ENSURE_SUCCESS(rv, rv);
                }
            }
            break;
        }
    }
    return NS_OK;
}

class txSyncCompileObserver : public txACompileObserver
{
public:
    txSyncCompileObserver(nsIURI* aReferrerURI);
    virtual ~txSyncCompileObserver();

    TX_DECL_ACOMPILEOBSERVER;

protected:
    nsAutoRefCnt mRefCnt;

private:
    nsCOMPtr<nsISyncLoadDOMService> mLoadService;
    nsCOMPtr<nsIURI> mReferrer;

protected:
    // This exists soly to supress a warning from nsDerivedSafe
    txSyncCompileObserver();
};

txSyncCompileObserver::txSyncCompileObserver(nsIURI* aReferrer)
    : mReferrer(aReferrer)
{
}

txSyncCompileObserver::~txSyncCompileObserver()
{
}

nsrefcnt
txSyncCompileObserver::AddRef()
{
    return ++mRefCnt;
}

nsrefcnt
txSyncCompileObserver::Release()
{
    if (--mRefCnt == 0) {
        mRefCnt = 1; //stabilize
        delete this;
        return 0;
    }
    return mRefCnt;
}

nsresult
txSyncCompileObserver::loadURI(const nsAString& aUri,
                               txStylesheetCompiler* aCompiler)
{
    if (!mLoadService) {
        mLoadService =
            do_GetService("@mozilla.org/content/syncload-dom-service;1");
        NS_ENSURE_TRUE(mLoadService, NS_ERROR_OUT_OF_MEMORY);
    }
    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_NewURI(getter_AddRefs(uri), aUri);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIChannel> channel;
    rv = NS_NewChannel(getter_AddRefs(channel), uri);
    NS_ENSURE_SUCCESS(rv, rv);
    // This is probably called by js, a loadGroup for the channeldoesn't
    // make sense.

    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
    if (httpChannel) {
        httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                      NS_LITERAL_CSTRING("text/xml,application/xml,application/xhtml+xml,*/*;q=0.1"),
                                      PR_FALSE);

        if (mReferrer) {
            httpChannel->SetReferrer(mReferrer);
        }
    }
    nsCOMPtr<nsIDOMDocument> document;
    rv = mLoadService->LoadDocument(channel, mReferrer,
                                    getter_AddRefs(document));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = handleNode(document, aCompiler);
    if (NS_FAILED(rv)) {
        aCompiler->cancel(rv);
        return rv;
    }

    rv = aCompiler->doneLoading();
    return rv;
}

void txSyncCompileObserver::onDoneCompiling(txStylesheetCompiler* aCompiler,
                                            nsresult aResult)
{
}

nsresult
TX_CompileStylesheet(nsIDOMNode* aNode, txStylesheet** aStylesheet)
{
    nsCOMPtr<nsIDOMDocument> document;
    aNode->GetOwnerDocument(getter_AddRefs(document));
    if (!document) {
        document = do_QueryInterface(aNode);
    }

    nsCOMPtr<nsIDocument> doc = do_QueryInterface(document);
    nsCOMPtr<nsIURI> uri;
    doc->GetBaseURL(*getter_AddRefs(uri));
    nsCAutoString baseURI;
    uri->GetSpec(baseURI);

    nsRefPtr<txSyncCompileObserver> obs = new txSyncCompileObserver(uri);
    NS_ENSURE_TRUE(obs, NS_ERROR_OUT_OF_MEMORY);
    NS_ConvertUTF8toUCS2 base(baseURI);
    nsRefPtr<txStylesheetCompiler> compiler =
        new txStylesheetCompiler(base, obs);
    NS_ENSURE_TRUE(compiler, NS_ERROR_OUT_OF_MEMORY);

    nsresult rv = handleNode(document, compiler);
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
