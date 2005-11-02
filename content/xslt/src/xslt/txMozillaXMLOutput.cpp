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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@propagandism.org>
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
#include "nsIScriptLoader.h"
#include "nsIDOMDocument.h"
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
#include "txStringUtils.h"
#include "txURIUtils.h"
#include "nsIHTMLDocument.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsIDocumentTransformer.h"

extern nsINameSpaceManager* gTxNameSpaceManager;

static NS_DEFINE_CID(kXMLDocumentCID, NS_XMLDOCUMENT_CID);
static NS_DEFINE_CID(kHTMLDocumentCID, NS_HTMLDOCUMENT_CID);

#define kXHTMLNameSpaceURI "http://www.w3.org/1999/xhtml"

#define TX_ENSURE_CURRENTNODE                           \
    NS_ASSERTION(mCurrentNode, "mCurrentNode is NULL"); \
    if (!mCurrentNode)                                  \
        return

txMozillaXMLOutput::txMozillaXMLOutput(const nsAString& aRootName,
                                       PRInt32 aRootNsID,
                                       txOutputFormat* aFormat,
                                       nsIDOMDocument* aSourceDocument,
                                       nsIDOMDocument* aResultDocument,
                                       nsITransformObserver* aObserver)
    : mBadChildLevel(0),
      mTableState(NORMAL),
      mDontAddCurrent(PR_FALSE),
      mHaveTitleElement(PR_FALSE),
      mHaveBaseElement(PR_FALSE),
      mCreatingNewDocument(PR_TRUE)
{
    if (aObserver) {
        mNotifier = new txTransformNotifier();
        if (mNotifier) {
            mNotifier->Init(aObserver);
        }
    }

    mOutputFormat.merge(*aFormat);
    mOutputFormat.setFromDefaults();

    createResultDocument(aRootName, aRootNsID, aSourceDocument, aResultDocument);
}

txMozillaXMLOutput::txMozillaXMLOutput(txOutputFormat* aFormat,
                                       nsIDOMDocumentFragment* aFragment)
    : mBadChildLevel(0),
      mTableState(NORMAL),
      mDontAddCurrent(PR_FALSE),
      mHaveTitleElement(PR_FALSE),
      mHaveBaseElement(PR_FALSE),
      mCreatingNewDocument(PR_FALSE)
{
    mOutputFormat.merge(*aFormat);
    mOutputFormat.setFromDefaults();

    aFragment->GetOwnerDocument(getter_AddRefs(mDocument));

    nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDocument);
    mDocumentIsHTML = doc && !doc->IsCaseSensitive();

    mCurrentNode = aFragment;
}

txMozillaXMLOutput::~txMozillaXMLOutput()
{
}

void txMozillaXMLOutput::attribute(const nsAString& aName,
                                   const PRInt32 aNsID,
                                   const nsAString& aValue)
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
        nsAutoString lowerName;
        TX_ToLowerCase(aName, lowerName);
        element->SetAttributeNS(nsString(), lowerName,
                                aValue);
    }
    else {
        nsAutoString nsURI;
        gTxNameSpaceManager->GetNameSpaceURI(aNsID, nsURI);
        element->SetAttributeNS(nsURI, aName, aValue);
    }
}

void txMozillaXMLOutput::characters(const nsAString& aData, PRBool aDOE)
{
    closePrevious(eCloseElement);

    if (mBadChildLevel) {
        return;
    }

    mText.Append(aData);
}

void txMozillaXMLOutput::comment(const nsAString& aData)
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
            domDoc->SetTitle(nsString());
        }
    }

    if (!mRefreshString.IsEmpty()) {
        nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDocument);
        nsIScriptGlobalObject *sgo = doc->GetScriptGlobalObject();
        if (sgo) {
            nsCOMPtr<nsIRefreshURI> refURI =
                do_QueryInterface(sgo->GetDocShell());
            if (refURI) {
                refURI->SetupRefreshURIFromHeader(doc->GetBaseURI(),
                                                  mRefreshString);
            }
        }
    }

    if (mNotifier) {
        mNotifier->OnTransformEnd();
    }
}

void txMozillaXMLOutput::endElement(const nsAString& aName, const PRInt32 aNsID)
{
    TX_ENSURE_CURRENTNODE;

    if (mBadChildLevel) {
        --mBadChildLevel;
        PR_LOG(txLog::xslt, PR_LOG_DEBUG,
               ("endElement, mBadChildLevel = %d\n", mBadChildLevel));
        return;
    }
    
#ifdef DEBUG
    if (mTableState != ADDED_TBODY) {
        nsAutoString nodeName;
        mCurrentNode->GetNodeName(nodeName);
        NS_ASSERTION(nodeName.Equals(aName,
                                     nsCaseInsensitiveStringComparator()),
                     "Unbalanced startElement and endElement calls!");
    }
    else {
        nsCOMPtr<nsIDOMNode> parent;
        mCurrentNode->GetParentNode(getter_AddRefs(parent));
        nsAutoString nodeName;
        parent->GetNodeName(nodeName);
        NS_ASSERTION(nodeName.Equals(aName,
                                     nsCaseInsensitiveStringComparator()),
                     "Unbalanced startElement and endElement calls!");
    }
#endif

    closePrevious(eCloseElement | eFlushText);

    // Handle html-elements
    if ((mOutputFormat.mMethod == eHTMLOutput && aNsID == kNameSpaceID_None) ||
        aNsID == kNameSpaceID_XHTML) {
        nsCOMPtr<nsIDOMElement> element = do_QueryInterface(mCurrentNode);
        NS_ASSERTION(element, "endElement'ing non-element");
        endHTMLElement(element);
    }

    if (mCreatingNewDocument) {
        // Handle all sorts of stylesheets
        nsCOMPtr<nsIStyleSheetLinkingElement> ssle =
            do_QueryInterface(mCurrentNode);
        if (ssle) {
            ssle->SetEnableUpdates(PR_TRUE);
            if (ssle->UpdateStyleSheet(nsnull, mNotifier) ==
                NS_ERROR_HTMLPARSER_BLOCK) {
                nsCOMPtr<nsIStyleSheet> stylesheet;
                ssle->GetStyleSheet(*getter_AddRefs(stylesheet));
                if (mNotifier) {
                    mNotifier->AddStyleSheet(stylesheet);
                }
            }
        }
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

    mTableState =
        NS_STATIC_CAST(TableState, NS_PTR_TO_INT32(mTableStateStack.pop()));
}

void txMozillaXMLOutput::getOutputDocument(nsIDOMDocument** aDocument)
{
    *aDocument = mDocument;
    NS_IF_ADDREF(*aDocument);
}

void txMozillaXMLOutput::processingInstruction(const nsAString& aTarget, const nsAString& aData)
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
        rv = ssle->UpdateStyleSheet(nsnull, mNotifier);
        if (rv == NS_ERROR_HTMLPARSER_BLOCK) {
            nsCOMPtr<nsIStyleSheet> stylesheet;
            ssle->GetStyleSheet(*getter_AddRefs(stylesheet));
            if (mNotifier) {
                mNotifier->AddStyleSheet(stylesheet);
            }
        }
    }
}

void txMozillaXMLOutput::startDocument()
{
    if (mNotifier) {
        mNotifier->OnTransformStart();
    }
}

void txMozillaXMLOutput::startElement(const nsAString& aName,
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

    nsresult rv = mTableStateStack.push(NS_INT32_TO_PTR(mTableState));
    if (NS_FAILED(rv)) {
        return;
    }
    mTableState = NORMAL;

    nsCOMPtr<nsIDOMElement> element;
    mDontAddCurrent = PR_FALSE;

    if ((mOutputFormat.mMethod == eHTMLOutput) && (aNsID == kNameSpaceID_None)) {
        if (mDocumentIsHTML) {
            rv = mDocument->CreateElement(aName,
                                          getter_AddRefs(element));
        }
        else {
            nsAutoString lcname;
            ToLowerCase(aName, lcname);
            rv = mDocument->CreateElementNS(NS_LITERAL_STRING(kXHTMLNameSpaceURI),
                                            lcname,
                                            getter_AddRefs(element));
        }
        if (NS_FAILED(rv)) {
            return;
        }

        startHTMLElement(element, PR_FALSE);
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
            startHTMLElement(element, PR_TRUE);
    }

    if (mCreatingNewDocument) {
        // Handle all sorts of stylesheets
        nsCOMPtr<nsIStyleSheetLinkingElement> ssle =
            do_QueryInterface(element);
        if (ssle) {
            ssle->InitStyleLinkElement(nsnull, PR_FALSE);
            ssle->SetEnableUpdates(PR_FALSE);
        }

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

            nsCOMPtr<nsIDOMNode> child, resultNode;
            PRUint32 i, childCount = document->GetChildCount();
            for (i = 0; i < childCount; ++i) {
                nsCOMPtr<nsIContent> childContent = document->GetChildAt(0);
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
        rv = mCurrentNode->AppendChild(text, getter_AddRefs(resultNode));
        NS_ASSERTION(NS_SUCCEEDED(rv), "Can't append text node");

        mText.Truncate();
    }
}

void txMozillaXMLOutput::startHTMLElement(nsIDOMElement* aElement, PRBool aXHTML)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
    nsIAtom *atom = content->Tag();

    mDontAddCurrent = (atom == txHTMLAtoms::script);

    if ((atom != txHTMLAtoms::tr || aXHTML) &&
        NS_PTR_TO_INT32(mTableStateStack.peek()) == ADDED_TBODY) {
        nsCOMPtr<nsIDOMNode> parent;
        mCurrentNode->GetParentNode(getter_AddRefs(parent));
        mCurrentNode.swap(parent);
        mTableStateStack.pop();
    }

    if (atom == txHTMLAtoms::table && !aXHTML) {
        mTableState = TABLE;
    }
    else if (atom == txHTMLAtoms::tr && !aXHTML &&
             NS_PTR_TO_INT32(mTableStateStack.peek()) == TABLE) {
        nsCOMPtr<nsIDOMElement> elem;
        rv = createHTMLElement(NS_LITERAL_STRING("tbody"),
                               getter_AddRefs(elem));
        if (NS_FAILED(rv)) {
            return;
        }
        nsCOMPtr<nsIDOMNode> dummy;
        rv = mCurrentNode->AppendChild(elem, getter_AddRefs(dummy));
        if (NS_FAILED(rv)) {
            return;
        }
        rv = mTableStateStack.push(NS_INT32_TO_PTR(ADDED_TBODY));
        if (NS_FAILED(rv)) {
            return;
        }
        mCurrentNode = elem;
    }
    else if (atom == txHTMLAtoms::head &&
             mOutputFormat.mMethod == eHTMLOutput) {
        // Insert META tag, according to spec, 16.2, like
        // <META http-equiv="Content-Type" content="text/html; charset=EUC-JP">
        nsCOMPtr<nsIDOMElement> meta;
        rv = createHTMLElement(NS_LITERAL_STRING("meta"),
                               getter_AddRefs(meta));
        if (NS_FAILED(rv)) {
            return;
        }
        rv = meta->SetAttribute(NS_LITERAL_STRING("http-equiv"),
                                NS_LITERAL_STRING("Content-Type"));
        NS_ASSERTION(NS_SUCCEEDED(rv), "Can't set http-equiv on meta");
        nsAutoString metacontent;
        metacontent.Append(mOutputFormat.mMediaType);
        metacontent.Append(NS_LITERAL_STRING("; charset="));
        metacontent.Append(mOutputFormat.mEncoding);
        rv = meta->SetAttribute(NS_LITERAL_STRING("content"),
                                metacontent);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Can't set content on meta");
        nsCOMPtr<nsIDOMNode> dummy;
        rv = aElement->AppendChild(meta, getter_AddRefs(dummy));
        NS_ASSERTION(NS_SUCCEEDED(rv), "Can't append meta");
    }
}

void txMozillaXMLOutput::endHTMLElement(nsIDOMElement* aElement)
{
    nsresult rv;
    nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
    NS_ASSERTION(content, "Can't QI to nsIContent");

    nsIAtom *atom = content->Tag();

    if (mTableState == ADDED_TBODY) {
        NS_ASSERTION(atom == txHTMLAtoms::tbody,
                     "Element flagged as added tbody isn't a tbody");
        nsCOMPtr<nsIDOMNode> parent;
        mCurrentNode->GetParentNode(getter_AddRefs(parent));
        mCurrentNode = parent;
        mTableState = NS_STATIC_CAST(TableState,
                                     NS_PTR_TO_INT32(mTableStateStack.pop()));

        return;
    }

    // Load scripts
    if (mNotifier && atom == txHTMLAtoms::script) {
        // Add this script element to the array of loading script elements.
        nsCOMPtr<nsIDOMHTMLScriptElement> scriptElement =
            do_QueryInterface(mCurrentNode);
        NS_ASSERTION(scriptElement, "Need script element");
        mNotifier->AddScriptElement(scriptElement);
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
    else if (mCreatingNewDocument && atom == txHTMLAtoms::base &&
             !mHaveBaseElement) {
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
        doc->SetBaseURI(baseURI); // The document checks if it is legal to set this base
    }
    else if (mCreatingNewDocument && atom == txHTMLAtoms::meta) {
        // handle HTTP-EQUIV data
        nsAutoString httpEquiv;
        content->GetAttr(kNameSpaceID_None, txHTMLAtoms::httpEquiv, httpEquiv);
        if (httpEquiv.IsEmpty())
            return;

        nsAutoString value;
        content->GetAttr(kNameSpaceID_None, txHTMLAtoms::content, value);
        if (value.IsEmpty())
            return;
        
        TX_ToLowerCase(httpEquiv);
        nsCOMPtr<nsIAtom> header = do_GetAtom(httpEquiv);
        processHTTPEquiv(header, value);
    }
}

void txMozillaXMLOutput::processHTTPEquiv(nsIAtom* aHeader, const nsAString& aValue)
{
    // For now we only handle "refresh". There's a longer list in
    // HTMLContentSink::ProcessHeaderData
    if (aHeader == txHTMLAtoms::refresh)
        CopyUCS2toASCII(aValue, mRefreshString);
}

nsresult
txMozillaXMLOutput::createResultDocument(const nsAString& aName, PRInt32 aNsID,
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

            mDocumentIsHTML = PR_TRUE;
        }
        else {
            // We should check the root name/namespace here and create the
            // appropriate document
            doc = do_CreateInstance(kXMLDocumentCID, &rv);
            NS_ENSURE_SUCCESS(rv, rv);

            mDocumentIsHTML = PR_FALSE;
        }
        mDocument = do_QueryInterface(doc);
    }
    else {
        mDocument = aResultDocument;
        doc = do_QueryInterface(aResultDocument);
        
        nsCOMPtr<nsIDocument> doc = do_QueryInterface(aResultDocument);
        mDocumentIsHTML = doc && !doc->IsCaseSensitive();
    }

    mCurrentNode = mDocument;

    // Reset and set up the document
    URIUtils::ResetWithSource(doc, aSourceDocument);

    // Set the charset
    if (!mOutputFormat.mEncoding.IsEmpty()) {
        doc->SetDocumentCharacterSet(
            NS_LossyConvertUTF16toASCII(mOutputFormat.mEncoding));
        doc->SetDocumentCharacterSetSource(kCharsetFromOtherComponent);
    }

    // Set the mime-type
    if (!mOutputFormat.mMediaType.IsEmpty()) {
        doc->SetContentType(mOutputFormat.mMediaType);
    }
    else if (mOutputFormat.mMethod == eHTMLOutput) {
        doc->SetContentType(NS_LITERAL_STRING("text/html"));
    }
    else {
        doc->SetContentType(NS_LITERAL_STRING("text/xml"));
    }

    // Set up script loader of the result document.
    nsIScriptLoader *loader = doc->GetScriptLoader();
    if (loader) {
        if (mNotifier) {
            loader->AddObserver(mNotifier);
        }
        else {
            // Don't load scripts, we can't notify the caller when they're loaded.
            loader->SetEnabled(PR_FALSE);
        }
    }

    if (mNotifier) {
        mNotifier->SetOutputDocument(mDocument);
    }

    // Do this after calling OnDocumentCreated to ensure that the
    // PresShell/PresContext has been hooked up and get notified.
    nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(doc);
    if (htmlDoc) {
        htmlDoc->SetCompatibilityMode(eCompatibility_FullStandards);
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
            qName.Assign(aName);
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

nsresult
txMozillaXMLOutput::createHTMLElement(const nsAString& aName,
                                      nsIDOMElement** aResult)
{
    if (mDocumentIsHTML) {
        return mDocument->CreateElement(aName, aResult);
    }

    return mDocument->CreateElementNS(NS_LITERAL_STRING(kXHTMLNameSpaceURI),
                                      aName, aResult);
}

txTransformNotifier::txTransformNotifier()
    : mInTransform(PR_FALSE)
      
{
}

txTransformNotifier::~txTransformNotifier()
{
}

NS_IMPL_ISUPPORTS2(txTransformNotifier,
                   nsIScriptLoaderObserver,
                   nsICSSLoaderObserver)

NS_IMETHODIMP
txTransformNotifier::ScriptAvailable(nsresult aResult, 
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
txTransformNotifier::ScriptEvaluated(nsresult aResult, 
                                     nsIDOMHTMLScriptElement *aElement,
                                     PRBool aIsInline,
                                     PRBool aWasPending)
{
    mScriptElements.RemoveObject(aElement);
    SignalTransformEnd();
    return NS_OK;
}

NS_IMETHODIMP 
txTransformNotifier::StyleSheetLoaded(nsICSSStyleSheet* aSheet, PRBool aNotify)
{
    // Check that the stylesheet was in the mStylesheets array, if not it is an
    // alternate and we don't want to call SignalTransformEnd since we don't
    // wait on alternates before calling OnTransformDone and so the load of the
    // alternate could finish after we called OnTransformDone already.
    // See http://bugzilla.mozilla.org/show_bug.cgi?id=215465.
    if (mStylesheets.RemoveObject(aSheet)) {
        SignalTransformEnd();
    }

    return NS_OK;
}

void
txTransformNotifier::Init(nsITransformObserver* aObserver)
{
    mObserver = aObserver;
}

void
txTransformNotifier::AddScriptElement(nsIDOMHTMLScriptElement* aElement)
{
    mScriptElements.AppendObject(aElement);
}

void
txTransformNotifier::AddStyleSheet(nsIStyleSheet* aStyleSheet)
{
    mStylesheets.AppendObject(aStyleSheet);
}

void
txTransformNotifier::OnTransformEnd()
{
    mInTransform = PR_FALSE;
    SignalTransformEnd();
}

void
txTransformNotifier::OnTransformStart()
{
    mInTransform = PR_TRUE;
}

void
txTransformNotifier::SetOutputDocument(nsIDOMDocument* aDocument)
{
    mDocument = aDocument;

    // Notify the contentsink that the document is created
    mObserver->OnDocumentCreated(mDocument);
}

void
txTransformNotifier::SignalTransformEnd()
{
    if (mInTransform || mScriptElements.Count() > 0 ||
        mStylesheets.Count() > 0) {
        return;
    }

    // Make sure that we don't get deleted while this function is executed and
    // we remove ourselfs from the scriptloader
    nsCOMPtr<nsIScriptLoaderObserver> kungFuDeathGrip(this);

    // XXX Need a better way to determine transform success/failure
    if (mDocument) {
        nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDocument);
        nsIScriptLoader *loader = doc->GetScriptLoader();
        if (loader) {
            loader->RemoveObserver(this);
        }

        mObserver->OnTransformDone(NS_OK, mDocument);
    }
    else {
        // XXX Need better error message and code.
        mObserver->OnTransformDone(NS_ERROR_FAILURE, nsnull);
    }
}
