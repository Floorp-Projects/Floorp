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
#include "nsIDOMComment.h"
#include "nsIDOMDocumentType.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIDOMProcessingInstruction.h"
#include "nsIDOMText.h"
#include "nsIDOMHTMLTableSectionElem.h"
#include "nsIDOMHTMLScriptElement.h"
#include "nsIDOMNSDocument.h"
#include "nsUnicharUtils.h"
#include "txAtoms.h"
#include "nsIParser.h"
#include "nsNetUtil.h"
#include "nsIScriptGlobalObject.h"
#include "nsIRefreshURI.h"
#include "nsIDocShell.h"

#define kXHTMLNameSpaceURI "http://www.w3.org/1999/xhtml"
#define kTXNameSpaceURI "http://www.mozilla.org/TransforMiix"
#define kTXWrapper "transformiix:result"

#define TX_ENSURE_CURRENTNODE                           \
    NS_ASSERTION(mCurrentNode, "mCurrentNode is NULL"); \
    if (!mCurrentNode)                                  \
        return

txMozillaXMLOutput::txMozillaXMLOutput() : mDisableStylesheetLoad(PR_FALSE)
{
}

txMozillaXMLOutput::~txMozillaXMLOutput()
{
}

void txMozillaXMLOutput::attribute(const String& aName,
                                   const PRInt32 aNsID,
                                   const String& aValue)
{
    if (!mParentNode)
        // XXX Signal this? (can't add attributes after element closed)
        return;

    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(mCurrentNode);
    NS_ASSERTION(element, "No element to add the attribute to.");
    if (!element)
        // XXX Signal this? (no element to add attributes to)
        return;

    if ((mOutputFormat.mMethod == eHTMLOutput) && (aNsID == kNameSpaceID_None)) {
        // Outputting HTML as XHTML, lowercase attribute names
        nsAutoString lowerName(aName.getConstNSString());
        ToLowerCase(lowerName);
        element->SetAttributeNS(NS_LITERAL_STRING(""), lowerName,
                                aValue.getConstNSString());
    }
    else {
        nsAutoString nsURI;
        mNameSpaceManager->GetNameSpaceURI(aNsID, nsURI);
        element->SetAttributeNS(nsURI, aName.getConstNSString(),
                                aValue.getConstNSString());
    }
}

void txMozillaXMLOutput::characters(const String& aData)
{
    closePrevious(eCloseElement);

    mText.Append(aData.getConstNSString());
}

void txMozillaXMLOutput::comment(const String& aData)
{
    closePrevious(eCloseElement | eFlushText);

    TX_ENSURE_CURRENTNODE;

    nsCOMPtr<nsIDOMComment> comment;
    nsresult rv = mDocument->CreateComment(aData.getConstNSString(),
                                           getter_AddRefs(comment));
    NS_ASSERTION(NS_SUCCEEDED(rv), "Can't create comment");

    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(comment);
    nsCOMPtr<nsIDOMNode> resultNode;
    rv = mCurrentNode->AppendChild(node, getter_AddRefs(resultNode));
    NS_ASSERTION(NS_SUCCEEDED(rv), "Can't append comment");
}

void txMozillaXMLOutput::disableStylesheetLoad()
{
    mDisableStylesheetLoad = PR_TRUE;
}

void txMozillaXMLOutput::endDocument()
{
    closePrevious(eCloseElement | eFlushText);
    if (!mHaveTitleElement) {
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
}

void txMozillaXMLOutput::endElement(const String& aName, const PRInt32 aNsID)
{
#ifdef DEBUG
    nsAutoString nodeName;
    mCurrentNode->GetNodeName(nodeName);
    NS_ASSERTION(nodeName.Equals(aName.getConstNSString(), nsCaseInsensitiveStringComparator()),
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

nsresult txMozillaXMLOutput::getRootContent(nsIContent** aReturn)
{
    NS_ASSERTION(aReturn, "NULL pointer passed to getRootContent");

    *aReturn = mRootContent;
    NS_IF_ADDREF(*aReturn);
    return NS_OK;
}

PRBool txMozillaXMLOutput::isDone()
{
    PRUint32 scriptCount = 0;
    if (mScriptElements)
        mScriptElements->Count(&scriptCount);
    return (scriptCount == 0);
}

void txMozillaXMLOutput::processingInstruction(const String& aTarget, const String& aData)
{
    if (mOutputFormat.mMethod == eHTMLOutput)
        return;

    closePrevious(eCloseElement | eFlushText);

    TX_ENSURE_CURRENTNODE;

    nsCOMPtr<nsIDOMProcessingInstruction> pi;
    nsresult rv = mDocument->CreateProcessingInstruction(aTarget.getConstNSString(),
                                                         aData.getConstNSString(),
                                                         getter_AddRefs(pi));
    NS_ASSERTION(NS_SUCCEEDED(rv), "Can't create entity reference");

    nsCOMPtr<nsIStyleSheetLinkingElement> ssle = do_QueryInterface(pi);
    if (ssle) {
      ssle->InitStyleLinkElement(nsnull, PR_FALSE);
      ssle->SetEnableUpdates(PR_FALSE);
    }

    nsCOMPtr<nsIDOMNode> resultNode;
    mCurrentNode->AppendChild(pi, getter_AddRefs(resultNode));
    NS_ASSERTION(NS_SUCCEEDED(rv), "Can't append entity reference");
    if (NS_FAILED(rv))
        return;

    if (ssle) {
      ssle->SetEnableUpdates(PR_TRUE);
      rv = ssle->UpdateStyleSheet(PR_TRUE, nsnull, mStyleSheetCount);
      if (NS_SUCCEEDED(rv) || (rv == NS_ERROR_HTMLPARSER_BLOCK))
        mStyleSheetCount++;
    }
}

void txMozillaXMLOutput::removeScriptElement(nsIDOMHTMLScriptElement *aElement)
{
    if (mScriptElements) {
        PRInt32 index = mScriptElements->IndexOf(aElement);
        if (index > -1)
            mScriptElements->RemoveElementAt(index);
    }
}

void txMozillaXMLOutput::setOutputDocument(nsIDOMDocument* aDocument)
{
    NS_ASSERTION(aDocument, "Document can't be NULL!");
    if (!aDocument)
        return;

    mDocument = aDocument;
    mCurrentNode = mDocument;
    mStyleSheetCount = 0;
    mHaveTitleElement = PR_FALSE;
    mHaveBaseElement = PR_FALSE;
    mNonAddedParent = nsnull;
    mNonAddedNode = nsnull;
    mRefreshString.Truncate();

    nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDocument);
    doc->GetNameSpaceManager(*getter_AddRefs(mNameSpaceManager));
    NS_ASSERTION(mNameSpaceManager, "Can't get namespace manager.");
}

void txMozillaXMLOutput::setOutputFormat(txOutputFormat* aOutputFormat)
{
    mOutputFormat.reset();
    mOutputFormat.merge(*aOutputFormat);
    mOutputFormat.setFromDefaults();
}

void txMozillaXMLOutput::startDocument()
{
    NS_ASSERTION(mDocument, "Document can't be NULL!");
}

void txMozillaXMLOutput::startElement(const String& aName,
                                      const PRInt32 aNsID)
{
    closePrevious(eCloseElement | eFlushText);

    nsresult rv;

    if (!mRootContent && !mOutputFormat.mSystemId.isEmpty()) {
        // No root element yet, so add the doctype if necesary.
        nsCOMPtr<nsIDOMDOMImplementation> implementation;
        rv = mDocument->GetImplementation(getter_AddRefs(implementation));
        NS_ASSERTION(NS_SUCCEEDED(rv), "Can't get DOMImplementation");
        if (NS_SUCCEEDED(rv)) {
            nsAutoString qName;
            nsCOMPtr<nsIDOMDocumentType> documentType;
            nsCOMPtr<nsIDOMNode> firstNode, node;
            if (mOutputFormat.mMethod == eHTMLOutput)
                qName.Assign(NS_LITERAL_STRING("html"));
            else
                qName.Assign(aName.getConstNSString());
            rv = implementation->CreateDocumentType(qName,
                                                    mOutputFormat.mPublicId.getConstNSString(),
                                                    mOutputFormat.mSystemId.getConstNSString(),
                                                    getter_AddRefs(documentType));
            NS_ASSERTION(NS_SUCCEEDED(rv), "Can't create doctype");

            mDocument->GetFirstChild(getter_AddRefs(firstNode));
            rv = mDocument->InsertBefore(documentType, firstNode, getter_AddRefs(node));
            NS_ASSERTION(NS_SUCCEEDED(rv), "Can't insert doctype");
        }
    }

    nsCOMPtr<nsIDOMElement> element;
    mDontAddCurrent = PR_FALSE;

    if ((mOutputFormat.mMethod == eHTMLOutput) && (aNsID == kNameSpaceID_None)) {
        // Outputting HTML as XHTML, lowercase element names
        nsAutoString lowerName(aName.getConstNSString());
        ToLowerCase(lowerName);
        rv = mDocument->CreateElementNS(NS_LITERAL_STRING(kXHTMLNameSpaceURI), lowerName,
                                        getter_AddRefs(element));
        NS_ASSERTION(NS_SUCCEEDED(rv), "Can't create element");

        startHTMLElement(element);
    }
    else {
        nsAutoString nsURI;
        mNameSpaceManager->GetNameSpaceURI(aNsID, nsURI);
        rv = mDocument->CreateElementNS(nsURI, aName.getConstNSString(),
                                        getter_AddRefs(element));
        NS_ASSERTION(NS_SUCCEEDED(rv), "Can't create element");

        if (aNsID == kNameSpaceID_XHTML)
            startHTMLElement(element);
    }

    if (element) {
        nsCOMPtr<nsIContent> cont = do_QueryInterface(element);
        if (cont) {
            nsCOMPtr<nsIDocument> doc = do_QueryInterface(mDocument);
            cont->SetDocument(doc, PR_FALSE, PR_TRUE);
        }
        mParentNode = mCurrentNode;
        mCurrentNode = do_QueryInterface(element);
    }
}

void txMozillaXMLOutput::closePrevious(PRInt8 aAction)
{
    TX_ENSURE_CURRENTNODE;

    nsresult rv;
    PRInt32 namespaceID = kNameSpaceID_None;
    nsCOMPtr<nsIContent> currentContent = do_QueryInterface(mCurrentNode);
    if (currentContent)
        currentContent->GetNameSpaceID(namespaceID);

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

            wrapChildren(mParentNode, wrapper);
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
            if (document && !mRootContent) {
                mRootContent = do_QueryInterface(mCurrentNode);
                mRootContent->SetDocument(document, PR_FALSE, PR_TRUE);
                document->SetRootContent(mRootContent);
            }
            else {
                nsCOMPtr<nsIDOMNode> resultNode;

                rv = mParentNode->AppendChild(mCurrentNode, getter_AddRefs(resultNode));
                NS_ASSERTION(NS_SUCCEEDED(rv), "Can't append node");
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
    else if (atom == txHTMLAtoms::script) {
        // Add this script element to the array of loading script elements.
        nsCOMPtr<nsIDOMHTMLScriptElement> scriptElement =
            do_QueryInterface(mCurrentNode);
        NS_ASSERTION(scriptElement, "Need script element");
        if (!mScriptElements)
            NS_NewISupportsArray(getter_AddRefs(mScriptElements));
        NS_ASSERTION(mScriptElements, "Can't create array");
        if (mScriptElements)
            mScriptElements->AppendElement(scriptElement);
    }
    // Set document title
    else if (atom == txHTMLAtoms::title && !mHaveTitleElement) {
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
    else if (atom == txHTMLAtoms::base && !mHaveBaseElement) {
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
    else if (atom == txHTMLAtoms::meta) {
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
    if (!mDisableStylesheetLoad) {
        nsCOMPtr<nsIStyleSheetLinkingElement> ssle =
            do_QueryInterface(aElement);
        if (ssle) {
            ssle->SetEnableUpdates(PR_TRUE);
            rv = ssle->UpdateStyleSheet(PR_TRUE, nsnull, mStyleSheetCount);
            if (NS_SUCCEEDED(rv) || (rv == NS_ERROR_HTMLPARSER_BLOCK))
              mStyleSheetCount++;
        }
    }
}

void txMozillaXMLOutput::processHTTPEquiv(nsIAtom* aHeader, nsAReadableString& aValue)
{
    // For now we only handle "refresh". There's a longer list in
    // HTMLContentSink::ProcessHeaderData
    if (aHeader == txHTMLAtoms::refresh)
        mRefreshString.Assign(aValue);
}

void txMozillaXMLOutput::wrapChildren(nsIDOMNode* aCurrentNode,
                                      nsIDOMElement* aWrapper)
{
    nsresult rv;
    nsCOMPtr<nsIContent> currentContent;

    currentContent = do_QueryInterface(mCurrentNode, &rv);

    NS_ASSERTION(NS_SUCCEEDED(rv), "Can't QI to nsIContent");
    if (!currentContent)
        return;

    PRInt32 count, i = 0;
    nsCOMPtr<nsIDOMNode> child, resultNode;
    nsCOMPtr<nsIContent> childContent;

    currentContent->ChildCount(count);
    for (i = 0; i < count; i++) {
        rv = currentContent->ChildAt(0, *getter_AddRefs(childContent));
        if (NS_SUCCEEDED(rv)) {
            child = do_QueryInterface(childContent);
            aCurrentNode->RemoveChild(child, getter_AddRefs(resultNode));
            aWrapper->AppendChild(resultNode, getter_AddRefs(child));
        }
    }
}
