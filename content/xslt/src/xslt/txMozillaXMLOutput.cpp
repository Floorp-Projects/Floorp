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
 * The Original Code is the TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@netscape.com>
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

#include "txMozillaXMLOutput.h"

#include "nsIDocument.h"
#include "nsIDocShell.h"
#include "nsIDOMComment.h"
#include "nsIDOMDocumentType.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMProcessingInstruction.h"
#include "nsIDOMText.h"
#include "nsIDOMHTMLTableSectionElem.h"
#include "nsIDOMHTMLScriptElement.h"
#include "nsIDOMNSDocument.h"
#include "nsIParser.h"
#include "nsIRefreshURI.h"
#include "nsIScriptGlobalObject.h"
#include "nsITextContent.h"
#include "nsIDocumentTransformer.h"
#include "nsIXMLContent.h"
#include "nsContentCID.h"
#include "nsNetUtil.h"
#include "nsUnicharUtils.h"
#include "txAtoms.h"
#include "TxLog.h"
#include "nsIConsoleService.h"
#include "nsIDOMDocumentFragment.h"
#include "nsINameSpaceManager.h"
#include "nsICSSStyleSheet.h"

extern nsINameSpaceManager* gTxNameSpaceManager;

static NS_DEFINE_CID(kXMLDocumentCID, NS_XMLDOCUMENT_CID);
static NS_DEFINE_CID(kHTMLDocumentCID, NS_HTMLDOCUMENT_CID);

#define kXHTMLNameSpaceURI "http://www.w3.org/1999/xhtml"

#define TX_ENSURE_CURRENTNODE                           \
    NS_ASSERTION(mCurrentNode, "mCurrentNode is NULL"); \
    if (!mCurrentNode)                                  \
        return

txMozillaXMLOutput::txMozillaXMLOutput(const String& aRootName,
                                       PRInt32 aRootNsID,
                                       txOutputFormat* aFormat,
                                       nsIDOMDocument* aSourceDocument,
                                       nsIDOMDocument* aResultDocument,
                                       nsITransformObserver* aObserver)
    : mBadChildLevel(0),
      mDontAddCurrent(PR_FALSE),
      mHaveTitleElement(PR_FALSE),
      mHaveBaseElement(PR_FALSE),
      mInTransform(PR_FALSE),
      mCreatingNewDocument(PR_TRUE)
{
    NS_INIT_ISUPPORTS();
    mOutputFormat.merge(*aFormat);
    mOutputFormat.setFromDefaults();

    mObserver = do_GetWeakReference(aObserver);

    createResultDocument(aRootName, aRootNsID, aSourceDocument, aResultDocument);
}

txMozillaXMLOutput::txMozillaXMLOutput(txOutputFormat* aFormat,
                                       nsIDOMDocumentFragment* aFragment)
    : mBadChildLevel(0),
      mDontAddCurrent(PR_FALSE),
      mHaveTitleElement(PR_FALSE),
      mHaveBaseElement(PR_FALSE),
      mInTransform(PR_FALSE),
      mCreatingNewDocument(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
    mOutputFormat.merge(*aFormat);
    mOutputFormat.setFromDefaults();

    aFragment->GetOwnerDocument(getter_AddRefs(mDocument));

    mCurrentNode = aFragment;
}

txMozillaXMLOutput::~txMozillaXMLOutput()
{
}

NS_IMPL_ISUPPORTS3(txMozillaXMLOutput,
                   txIOutputXMLEventHandler,
                   nsIScriptLoaderObserver,
                   nsICSSLoaderObserver);

void txMozillaXMLOutput::attribute(const String& aName,
                                   const PRInt32 aNsID,
                                   const String& aValue)
{
    if (!mParentNode)
        // XXX Signal this? (can't add attributes after element closed)
        return;

    if (mBadChildLevel) {
        return;
    }

    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(mCurrentNode);
    NS_ASSERTION(element, "No element to add the attribute to.");
    if (!element)
        // XXX Signal this? (no element to add attributes to)
        return;

    if ((mOutputFormat.mMethod == eHTMLOutput) && (aNsID == kNameSpaceID_None)) {
        // Outputting HTML as XHTML, lowercase attribute names
        nsAutoString lowerName(aName);
        ToLowerCase(lowerName);
        element->SetAttributeNS(NS_LITERAL_STRING(""), lowerName,
                                aValue);
    }
    else {
        nsAutoString nsURI;
        gTxNameSpaceManager->GetNameSpaceURI(aNsID, nsURI);
        element->SetAttributeNS(nsURI, aName, aValue);
    }
}

void txMozillaXMLOutput::characters(const String& aData)
{
    closePrevious(eCloseElement);

    if (mBadChildLevel) {
        return;
    }

    mText.Append(aData);
}

void txMozillaXMLOutput::comment(const String& aData)
{
    closePrevious(eCloseElement | eFlushText);

    if (mBadChildLevel) {
        return;
    }

    TX_ENSURE_CURRENTNODE;

    nsCOMPtr<nsIDOMComment> comment;
    nsresult rv = mDocument->CreateComment(aData,
                                           getter_AddRefs(comment));
    NS_ASSERTION(NS_SUCCEEDED(rv), "Can't create comment");
    nsCOMPtr<nsIDOMNode> resultNode;
    rv = mCurrentNode->AppendChild(comment, getter_AddRefs(resultNode));
    NS_ASSERTION(NS_SUCCEEDED(rv), "Can't append comment");
}

void txMozillaXMLOutput::endDocument()
{
    closePrevious(eCloseElement | eFlushText);
    // This should really be handled by nsIDocument::Reset
    if (mCreatingNewDocument && !mHaveTitleElement) {
        nsCOMPtr<nsIDOMNSDocument> domDoc = do_QueryInterface(mDocument);
        if (domDoc) {
            domDoc->SetTitle(NS_LITERAL_STRING(""));
        }
    }

    if (!mRefreshString.IsEmpty()) {
        nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDocument);
        nsCOMPtr<nsIScriptGlobalObject> sgo;
        doc->GetScriptGlobalObject(getter_AddRefs(sgo));
        if (sgo) {
            nsCOMPtr<nsIDocShell> docShell;
            sgo->GetDocShell(getter_AddRefs(docShell));
            nsCOMPtr<nsIRefreshURI> refURI = do_QueryInterface(docShell);
            if (refURI) {
                nsCOMPtr<nsIURI> baseURI;
                doc->GetBaseURL(*getter_AddRefs(baseURI));
                refURI->SetupRefreshURIFromHeader(baseURI, mRefreshString);
            }
        }
    }

    mInTransform = PR_FALSE;
    SignalTransformEnd();
}

void txMozillaXMLOutput::endElement(const String& aName, const PRInt32 aNsID)
{
    TX_ENSURE_CURRENTNODE;

    if (mBadChildLevel) {
        --mBadChildLevel;
        PR_LOG(txLog::xslt, PR_LOG_DEBUG,
               ("endElement, mBadChildLevel = %d\n", mBadChildLevel));
        return;
    }
    
#ifdef DEBUG
    nsAutoString nodeName;
    mCurrentNode->GetNodeName(nodeName);
    NS_ASSERTION(nodeName.Equals(aName, nsCaseInsensitiveStringComparator()),
                 "Unbalanced startElement and endElement calls!");
#endif

    closePrevious(eCloseElement | eFlushText);

    // Handle html-elements
    if ((mOutputFormat.mMethod == eHTMLOutput && aNsID == kNameSpaceID_None) ||
        aNsID == kNameSpaceID_XHTML) {
        nsCOMPtr<nsIDOMElement> element = do_QueryInterface(mCurrentNode);
        NS_ASSERTION(element, "endElement'ing non-element");
        endHTMLElement(element, aNsID == kNameSpaceID_XHTML);
    }

    // Add the element to the tree if it wasn't added before and take one step
    // up the tree
    // we can't use GetParentNode to check if mCurrentNode is the
    // "non-added node" since that does strange things when we've called
    // SetDocument manually
    if (mCurrentNode == mNonAddedNode) {
        nsCOMPtr<nsIDocument> document = do_QueryInterface(mNonAddedParent);
        if (document && !mRootContent) {
            mRootContent = do_QueryInterface(mCurrentNode);
            mRootContent->SetDocument(document, PR_FALSE, PR_TRUE);
            document->SetRootContent(mRootContent);
        }
        else {
            nsCOMPtr<nsIDOMNode> resultNode;
            mNonAddedParent->AppendChild(mCurrentNode,
                                         getter_AddRefs(resultNode));
        }
        mCurrentNode = mNonAddedParent;
        mNonAddedParent = nsnull;
        mNonAddedNode = nsnull;
    }
    else {
        nsCOMPtr<nsIDOMNode> parent;
        mCurrentNode->GetParentNode(getter_AddRefs(parent));
        mCurrentNode = parent;
    }
}

void txMozillaXMLOutput::getOutputDocument(nsIDOMDocument** aDocument)
{
    *aDocument = mDocument;
    NS_IF_ADDREF(*aDocument);
}

void txMozillaXMLOutput::processingInstruction(const String& aTarget, const String& aData)
{
    if (mOutputFormat.mMethod == eHTMLOutput)
        return;

    closePrevious(eCloseElement | eFlushText);

    TX_ENSURE_CURRENTNODE;

    nsCOMPtr<nsIDOMProcessingInstruction> pi;
    nsresult rv = mDocument->CreateProcessingInstruction(aTarget, aData,
                                                         getter_AddRefs(pi));
    NS_ASSERTION(NS_SUCCEEDED(rv), "Can't create processing instruction");
    if (NS_FAILED(rv))
        return;

    nsCOMPtr<nsIStyleSheetLinkingElement> ssle;
    if (mCreatingNewDocument) {
        ssle = do_QueryInterface(pi);
        if (ssle) {
            ssle->InitStyleLinkElement(nsnull, PR_FALSE);
            ssle->SetEnableUpdates(PR_FALSE);
        }
    }

    nsCOMPtr<nsIDOMNode> resultNode;
    rv = mCurrentNode->AppendChild(pi, getter_AddRefs(resultNode));
    NS_ASSERTION(NS_SUCCEEDED(rv), "Can't append processing instruction");
    if (NS_FAILED(rv))
        return;

    if (ssle) {
        ssle->SetEnableUpdates(PR_TRUE);
        rv = ssle->UpdateStyleSheet(nsnull, this);
        if (rv == NS_ERROR_HTMLPARSER_BLOCK) {
            nsCOMPtr<nsIStyleSheet> stylesheet;
            ssle->GetStyleSheet(*getter_AddRefs(stylesheet));
            mStylesheets.AppendObject(stylesheet);
        }
    }
}

void txMozillaXMLOutput::startDocument()
{
    mInTransform = PR_TRUE;
}

void txMozillaXMLOutput::startElement(const String& aName,
                                      const PRInt32 aNsID)
{
    TX_ENSURE_CURRENTNODE;

    if (mBadChildLevel) {
        ++mBadChildLevel;
        PR_LOG(txLog::xslt, PR_LOG_DEBUG,
               ("startElement, mBadChildLevel = %d\n", mBadChildLevel));
        return;
    }

    closePrevious(eCloseElement | eFlushText);

    if (mBadChildLevel) {
        // eCloseElement couldn't add the parent, we fail as well
        ++mBadChildLevel;
        PR_LOG(txLog::xslt, PR_LOG_DEBUG,
               ("startElement, mBadChildLevel = %d\n", mBadChildLevel));
        return;
    }

    nsresult rv;

    nsCOMPtr<nsIDOMElement> element;
    mDontAddCurrent = PR_FALSE;

    if ((mOutputFormat.mMethod == eHTMLOutput) && (aNsID == kNameSpaceID_None)) {
        rv = mDocument->CreateElement(aName,
                                      getter_AddRefs(element));
        if (NS_FAILED(rv)) {
            return;
        }

        startHTMLElement(element);
    }
    else {
        nsAutoString nsURI;
        gTxNameSpaceManager->GetNameSpaceURI(aNsID, nsURI);
        rv = mDocument->CreateElementNS(nsURI, aName,
                                        getter_AddRefs(element));
        NS_ASSERTION(NS_SUCCEEDED(rv), "Can't create element");
        if (NS_FAILED(rv)) {
            return;
        }

        if (aNsID == kNameSpaceID_XHTML)
            startHTMLElement(element);
    }

    if (mCreatingNewDocument) {
        nsCOMPtr<nsIContent> cont = do_QueryInterface(element);
        NS_ASSERTION(cont, "element doesn't implement nsIContent");
        nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDocument);
        cont->SetDocument(doc, PR_FALSE, PR_TRUE);
    }
    mParentNode = mCurrentNode;
    mCurrentNode = do_QueryInterface(element);
}

void txMozillaXMLOutput::closePrevious(PRInt8 aAction)
{
    TX_ENSURE_CURRENTNODE;

    nsresult rv;
    if ((aAction & eCloseElement) && mParentNode) {
        nsCOMPtr<nsIDocument> document = do_QueryInterface(mParentNode);
        nsCOMPtr<nsIDOMElement> currentElement = do_QueryInterface(mCurrentNode);

        if (document && currentElement && mRootContent) {
            // We already have a document element, but the XSLT spec allows this.
            // As a workaround, create a wrapper object and use that as the
            // document element.
            nsCOMPtr<nsIDOMElement> wrapper;

            rv = mDocument->CreateElementNS(NS_LITERAL_STRING(kTXNameSpaceURI),
                                            NS_LITERAL_STRING(kTXWrapper),
                                            getter_AddRefs(wrapper));
            NS_ASSERTION(NS_SUCCEEDED(rv), "Can't create wrapper element");

            nsCOMPtr<nsIContent> childContent;
            nsCOMPtr<nsIDOMNode> child, resultNode;
            PRInt32 childCount, i;
            document->GetChildCount(childCount);
            for (i = 0; i < childCount; ++i) {
                document->ChildAt(0, *getter_AddRefs(childContent));
                if (childContent == mRootContent) {
                    document->SetRootContent(nsnull);
                }
                child = do_QueryInterface(childContent);
                wrapper->AppendChild(child, getter_AddRefs(resultNode));
            }

            mParentNode = wrapper;
            mRootContent = do_QueryInterface(wrapper);
            mRootContent->SetDocument(document, PR_FALSE, PR_TRUE);
            document->SetRootContent(mRootContent);
        }

        if (mDontAddCurrent && !mNonAddedParent) {
            mNonAddedParent = mParentNode;
            mNonAddedNode = mCurrentNode;
        }
        else {
            if (document && currentElement && !mRootContent) {
                mRootContent = do_QueryInterface(mCurrentNode);
                mRootContent->SetDocument(document, PR_FALSE, PR_TRUE);
                document->SetRootContent(mRootContent);
            }
            else {
                nsCOMPtr<nsIDOMNode> resultNode;

                rv = mParentNode->AppendChild(mCurrentNode, getter_AddRefs(resultNode));
                if (NS_FAILED(rv)) {
                    mBadChildLevel = 1;
                    mCurrentNode = mParentNode;
                    PR_LOG(txLog::xslt, PR_LOG_DEBUG,
                           ("closePrevious, mBadChildLevel = %d\n",
                            mBadChildLevel));
                    // warning to the console
                    nsCOMPtr<nsIConsoleService> consoleSvc = 
                        do_GetService("@mozilla.org/consoleservice;1", &rv);
                    if (consoleSvc) {
                        consoleSvc->LogStringMessage(
                            NS_LITERAL_STRING("failed to create XSLT content").get());
                    }
                }
            }
        }
        mParentNode = nsnull;
    }
    else if ((aAction & eFlushText) && !mText.IsEmpty()) {
        nsCOMPtr<nsIDOMText> text;
        rv = mDocument->CreateTextNode(mText, getter_AddRefs(text));
        NS_ASSERTION(NS_SUCCEEDED(rv), "Can't create text node");

        nsCOMPtr<nsIDOMNode> resultNode;
        mCurrentNode->AppendChild(text, getter_AddRefs(resultNode));
        NS_ASSERTION(NS_SUCCEEDED(rv), "Can't append text node");

        mText.Truncate();
    }
}

void txMozillaXMLOutput::startHTMLElement(nsIDOMElement* aElement)
{
    nsCOMPtr<nsIAtom> atom;
    nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
    content->GetTag(*getter_AddRefs(atom));

    mDontAddCurrent = (atom == txHTMLAtoms::script);

    if (mCreatingNewDocument) {
        nsCOMPtr<nsIStyleSheetLinkingElement> ssle =
            do_QueryInterface(aElement);
        if (ssle) {
            // XXX Trick nsCSSLoader into blocking/notifying us?
            //     We would need to implement nsIParser and
            //     pass ourselves as first parameter to
            //     InitStyleLinkElement. We would then be notified
            //     of stylesheet loads/load failures.
            ssle->InitStyleLinkElement(nsnull, PR_FALSE);
            ssle->SetEnableUpdates(PR_FALSE);
        }
    }
}

void txMozillaXMLOutput::endHTMLElement(nsIDOMElement* aElement,
                                        PRBool aXHTML)
{
    nsresult rv;
    nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
    NS_ASSERTION(content, "Can't QI to nsIContent");

    nsCOMPtr<nsIAtom> atom;
    content->GetTag(*getter_AddRefs(atom));

    // add <tbody> to tables if there is none (not in xhtml)
    if (atom == txHTMLAtoms::table && !aXHTML) {
        // Check if we have any table section.
        nsCOMPtr<nsIDOMHTMLTableSectionElement> section;
        nsCOMPtr<nsIContent> childContent;
        PRInt32 count, i = 0;

        content->ChildCount(count);
        while (!section && (i < count)) {
            rv = content->ChildAt(i, *getter_AddRefs(childContent));
            NS_ASSERTION(NS_SUCCEEDED(rv), "Something went wrong while getting a child");
            section = do_QueryInterface(childContent);
            ++i;
        }

        if (!section && (count > 0)) {
            // If no section, wrap table's children in a tbody.
            nsCOMPtr<nsIDOMElement> wrapper;

            rv = mDocument->CreateElementNS(NS_LITERAL_STRING(kXHTMLNameSpaceURI),
                                            NS_LITERAL_STRING("tbody"),
                                            getter_AddRefs(wrapper));
            NS_ASSERTION(NS_SUCCEEDED(rv), "Can't create tbody element");

            if (wrapper) {
                nsCOMPtr<nsIDOMNode> resultNode;

                wrapChildren(mCurrentNode, wrapper);
                rv = mCurrentNode->AppendChild(wrapper, getter_AddRefs(resultNode));
                NS_ASSERTION(NS_SUCCEEDED(rv), "Can't append tbody element");
            }
        }
    }
    // Load scripts
    else if (mCreatingNewDocument &&
             atom == txHTMLAtoms::script) {
        // Add this script element to the array of loading script elements.
        nsCOMPtr<nsIDOMHTMLScriptElement> scriptElement =
            do_QueryInterface(mCurrentNode);
        NS_ASSERTION(scriptElement, "Need script element");
        mScriptElements.AppendObject(scriptElement);
    }
    // Set document title
    else if (mCreatingNewDocument &&
             atom == txHTMLAtoms::title && !mHaveTitleElement) {
        // The first title wins
        mHaveTitleElement = PR_TRUE;
        nsCOMPtr<nsIDOMNSDocument> domDoc = do_QueryInterface(mDocument);
        nsCOMPtr<nsIDOMNode> textNode;
        aElement->GetFirstChild(getter_AddRefs(textNode));
        if (domDoc && textNode) {
            nsAutoString text;
            textNode->GetNodeValue(text);
            text.CompressWhitespace();
            domDoc->SetTitle(text);
        }
    }
    else if (mCreatingNewDocument &&
             atom == txHTMLAtoms::base && !mHaveBaseElement) {
        // The first base wins
        mHaveBaseElement = PR_TRUE;

        nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDocument);
        NS_ASSERTION(doc, "document doesn't implement nsIDocument");
        nsAutoString value;
        content->GetAttr(kNameSpaceID_None, txHTMLAtoms::target, value);
        doc->SetBaseTarget(value);

        content->GetAttr(kNameSpaceID_None, txHTMLAtoms::href, value);
        nsCOMPtr<nsIURI> baseURI;
        rv = NS_NewURI(getter_AddRefs(baseURI), value, nsnull);
        if (NS_FAILED(rv))
            return;
        doc->SetBaseURL(baseURI); // The document checks if it is legal to set this base
    }
    else if (mCreatingNewDocument &&
             atom == txHTMLAtoms::meta) {
        // handle HTTP-EQUIV data
        nsAutoString httpEquiv;
        content->GetAttr(kNameSpaceID_None, txHTMLAtoms::httpEquiv, httpEquiv);
        if (httpEquiv.IsEmpty())
            return;

        nsAutoString value;
        content->GetAttr(kNameSpaceID_None, txHTMLAtoms::content, value);
        if (value.IsEmpty())
            return;
        
        ToLowerCase(httpEquiv);
        nsCOMPtr<nsIAtom> header = dont_AddRef(NS_NewAtom(httpEquiv));
        processHTTPEquiv(header, value);
    }

    // Handle all sorts of stylesheets
    if (mCreatingNewDocument) {
        nsCOMPtr<nsIStyleSheetLinkingElement> ssle =
            do_QueryInterface(aElement);
        if (ssle) {
            ssle->SetEnableUpdates(PR_TRUE);
            ssle->UpdateStyleSheet(nsnull, this);
            if (rv == NS_ERROR_HTMLPARSER_BLOCK) {
                nsCOMPtr<nsIStyleSheet> stylesheet;
                ssle->GetStyleSheet(*getter_AddRefs(stylesheet));
                mStylesheets.AppendObject(stylesheet);
            }
        }
    }
}

void txMozillaXMLOutput::processHTTPEquiv(nsIAtom* aHeader, const nsAString& aValue)
{
    // For now we only handle "refresh". There's a longer list in
    // HTMLContentSink::ProcessHeaderData
    if (aHeader == txHTMLAtoms::refresh)
        CopyUCS2toASCII(aValue, mRefreshString);
}

void txMozillaXMLOutput::wrapChildren(nsIDOMNode* aCurrentNode,
                                      nsIDOMElement* aWrapper)
{
    nsCOMPtr<nsIDOMNodeList> children;
    nsresult rv = aCurrentNode->GetChildNodes(getter_AddRefs(children));
    if (NS_FAILED(rv)) {
        NS_ASSERTION(0, "Can't get children!");
        return;
    }

    nsCOMPtr<nsIDOMNode> child, resultNode;
    PRUint32 count, i;
    children->GetLength(&count);
    for (i = 0; i < count; ++i) {
        rv = children->Item(0, getter_AddRefs(child));
        if (NS_SUCCEEDED(rv)) {
            aWrapper->AppendChild(child, getter_AddRefs(resultNode));
        }
    }
}

nsresult
txMozillaXMLOutput::createResultDocument(const String& aName, PRInt32 aNsID,
                                         nsIDOMDocument* aSourceDocument,
                                         nsIDOMDocument* aResultDocument)
{
    nsresult rv;

    nsCOMPtr<nsIDocument> doc;
    if (!aResultDocument) {
        // Create the document
        if (mOutputFormat.mMethod == eHTMLOutput) {
            doc = do_CreateInstance(kHTMLDocumentCID, &rv);
            NS_ENSURE_SUCCESS(rv, rv);
        }
        else {
            // We should check the root name/namespace here and create the
            // appropriate document
            doc = do_CreateInstance(kXMLDocumentCID, &rv);
            NS_ENSURE_SUCCESS(rv, rv);
        }
        mDocument = do_QueryInterface(doc);
    }
    else {
        mDocument = aResultDocument;
        doc = do_QueryInterface(aResultDocument);
    }
    mCurrentNode = mDocument;

    // Reset and set up the document
    nsCOMPtr<nsILoadGroup> loadGroup;
    nsCOMPtr<nsIChannel> channel;
    nsCOMPtr<nsIDocument> sourceDoc = do_QueryInterface(aSourceDocument);
    sourceDoc->GetDocumentLoadGroup(getter_AddRefs(loadGroup));
    nsCOMPtr<nsIIOService> serv = do_GetService(NS_IOSERVICE_CONTRACTID);
    if (serv) {
        // Create a temporary channel to get nsIDocument->Reset to
        // do the right thing. We want the output document to get
        // much of the input document's characteristics.
        nsCOMPtr<nsIURI> docURL;
        sourceDoc->GetDocumentURL(getter_AddRefs(docURL));
        serv->NewChannelFromURI(docURL, getter_AddRefs(channel));
    }
    doc->Reset(channel, loadGroup);
    nsCOMPtr<nsIURI> baseURL;
    sourceDoc->GetBaseURL(*getter_AddRefs(baseURL));
    doc->SetBaseURL(baseURL);
    // XXX We might want to call SetDefaultStylesheets here

    // Set up script loader of the result document.
    nsCOMPtr<nsIScriptLoader> loader;
    doc->GetScriptLoader(getter_AddRefs(loader));
    nsCOMPtr<nsITransformObserver> observer = do_QueryReferent(mObserver);
    if (loader) {
        if (observer) {
            loader->AddObserver(this);
        }
        else {
            // Don't load scripts, we can't notify the caller when they're loaded.
            loader->SetEnabled(PR_FALSE);
        }
    }

    // Notify the contentsink that the document is created
    if (observer) {
        observer->OnDocumentCreated(mDocument);
    }

    // Add a doc-type if requested
    if (!mOutputFormat.mSystemId.IsEmpty()) {
        nsCOMPtr<nsIDOMDOMImplementation> implementation;
        rv = aSourceDocument->GetImplementation(getter_AddRefs(implementation));
        NS_ENSURE_SUCCESS(rv, rv);
        nsAutoString qName;
        if (mOutputFormat.mMethod == eHTMLOutput) {
            qName.Assign(NS_LITERAL_STRING("html"));
        }
        else {
            qName.Assign(aName.getConstNSString());
        }
        nsCOMPtr<nsIDOMDocumentType> documentType;
        rv = implementation->CreateDocumentType(qName,
                                                mOutputFormat.mPublicId,
                                                mOutputFormat.mSystemId,
                                                getter_AddRefs(documentType));
        NS_ASSERTION(NS_SUCCEEDED(rv), "Can't create doctype");
        nsCOMPtr<nsIDOMNode> tmp;
        mDocument->AppendChild(documentType, getter_AddRefs(tmp));
    }

    return NS_OK;
}

NS_IMETHODIMP
txMozillaXMLOutput::ScriptAvailable(nsresult aResult, 
                                    nsIDOMHTMLScriptElement *aElement, 
                                    PRBool aIsInline,
                                    PRBool aWasPending,
                                    nsIURI *aURI, 
                                    PRInt32 aLineNo,
                                    const nsAString& aScript)
{
    if (NS_FAILED(aResult)) {
        mScriptElements.RemoveObject(aElement);
        SignalTransformEnd();
    }

    return NS_OK;
}

NS_IMETHODIMP 
txMozillaXMLOutput::ScriptEvaluated(nsresult aResult, 
                                    nsIDOMHTMLScriptElement *aElement,
                                    PRBool aIsInline,
                                    PRBool aWasPending)
{
    mScriptElements.RemoveObject(aElement);
    SignalTransformEnd();
    return NS_OK;
}

NS_IMETHODIMP 
txMozillaXMLOutput::StyleSheetLoaded(nsICSSStyleSheet* aSheet, PRBool aNotify)
{
    // aSheet might not be in our list if the load was done synchronously
    mStylesheets.RemoveObject(aSheet);
    SignalTransformEnd();
    return NS_OK;
}

void
txMozillaXMLOutput::SignalTransformEnd()
{
    if (mInTransform) {
        return;
    }

    nsCOMPtr<nsITransformObserver> observer = do_QueryReferent(mObserver);
    if (!observer) {
        return;
    }

    if (mScriptElements.Count() > 0 || mStylesheets.Count() > 0) {
        return;
    }

    // Make sure that we don't get deleted while this function is executed and
    // we remove ourselfs from the scriptloader
    nsCOMPtr<nsIScriptLoaderObserver> kungFuDeathGrip(this);

    mObserver = nsnull;

    // XXX Need a better way to determine transform success/failure
    if (mDocument) {
        nsCOMPtr<nsIScriptLoader> loader;
        nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDocument);
        doc->GetScriptLoader(getter_AddRefs(loader));
        if (loader) {
            loader->RemoveObserver(this);
        }

        observer->OnTransformDone(NS_OK, mDocument);
    }
    else {
        // XXX Need better error message and code.
        observer->OnTransformDone(NS_ERROR_FAILURE, nsnull);
    }
}
