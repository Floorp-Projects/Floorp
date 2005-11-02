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

#include "txMozillaTextOutput.h"
#include "nsContentCID.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIDOMElement.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMText.h"
#include "nsIDocumentTransformer.h"
#include "nsNetUtil.h"
#include "nsIDOMNSDocument.h"
#include "nsIParser.h"

static NS_DEFINE_CID(kXMLDocumentCID, NS_XMLDOCUMENT_CID);

txMozillaTextOutput::txMozillaTextOutput(nsIDOMDocument* aSourceDocument,
                                         nsIDOMDocument* aResultDocument,
                                         nsITransformObserver* aObserver)
{
    mObserver = do_GetWeakReference(aObserver);
    createResultDocument(aSourceDocument, aResultDocument);
}

txMozillaTextOutput::txMozillaTextOutput(nsIDOMDocumentFragment* aDest)
{
    nsCOMPtr<nsIDOMDocument> doc;
    aDest->GetOwnerDocument(getter_AddRefs(doc));
    NS_ASSERTION(doc, "unable to get ownerdocument");
    nsCOMPtr<nsIDOMText> textNode;
    nsresult rv = doc->CreateTextNode(nsString(),
                                      getter_AddRefs(textNode));
    if (NS_FAILED(rv)) {
        return;
    }
    nsCOMPtr<nsIDOMNode> dummy;
    rv = aDest->AppendChild(textNode, getter_AddRefs(dummy));
    if (NS_FAILED(rv)) {
        return;
    }

    mTextNode = textNode;
    return;
}

txMozillaTextOutput::~txMozillaTextOutput()
{
}

void txMozillaTextOutput::attribute(const nsAString& aName,
                                    const PRInt32 aNsID,
                                    const nsAString& aValue)
{
}

void txMozillaTextOutput::characters(const nsAString& aData, PRBool aDOE)
{
    if (mTextNode)
        mTextNode->AppendData(aData);
}

void txMozillaTextOutput::comment(const nsAString& aData)
{
}

void txMozillaTextOutput::endDocument()
{
    nsCOMPtr<nsITransformObserver> observer = do_QueryReferent(mObserver);
    if (observer) {
        observer->OnTransformDone(NS_OK, mDocument);
    }
}

void txMozillaTextOutput::endElement(const nsAString& aName,
                                     const PRInt32 aNsID)
{
}

void txMozillaTextOutput::processingInstruction(const nsAString& aTarget,
                                                const nsAString& aData)
{
}

void txMozillaTextOutput::startDocument()
{
}

void txMozillaTextOutput::createResultDocument(nsIDOMDocument* aSourceDocument,
                                               nsIDOMDocument* aResultDocument)
{
    nsresult rv = NS_OK;
    
    /*
     * Create an XHTML document to hold the text.
     *
     * <html>
     *   <head />
     *   <body>
     *     <pre> * The text comes here * </pre>
     *   <body>
     * </html>
     *
     * Except if we are transforming into a non-displayed document we create
     * the following DOM
     *
     * <transformiix:result> * The text comes here * </transformiix:result>
     */
     
    nsCOMPtr<nsIDocument> doc;
    if (!aResultDocument) {
        // Create the document
        doc = do_CreateInstance(kXMLDocumentCID, &rv);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't create document");
        mDocument = do_QueryInterface(doc);
    }
    else {
        mDocument = aResultDocument;
        doc = do_QueryInterface(aResultDocument);
        NS_ASSERTION(doc, "Couldn't QI to nsIDocument");
    }

    if (!doc) {
        return;
    }

    NS_ASSERTION(mDocument, "Need document");

    nsCOMPtr<nsIDOMNSDocument> nsDoc = do_QueryInterface(mDocument);
    if (nsDoc) {
        nsDoc->SetTitle(nsString());
    }

    // Reset and set up document
    nsCOMPtr<nsIChannel> channel;
    nsCOMPtr<nsIDocument> sourceDoc = do_QueryInterface(aSourceDocument);
    nsCOMPtr<nsILoadGroup> loadGroup = sourceDoc->GetDocumentLoadGroup();
    nsCOMPtr<nsIIOService> serv = do_GetService(NS_IOSERVICE_CONTRACTID);
    if (serv) {
        // Create a temporary channel to get nsIDocument->Reset to
        // do the right thing. We want the output document to get
        // much of the input document's characteristics.
        serv->NewChannelFromURI(sourceDoc->GetDocumentURI(),
                                getter_AddRefs(channel));
    }
    doc->Reset(channel, loadGroup);
    doc->SetBaseURI(sourceDoc->GetBaseURI());

    // Set the charset
    if (!mOutputFormat.mEncoding.IsEmpty()) {
        doc->SetDocumentCharacterSet(
            NS_LossyConvertUTF16toASCII(mOutputFormat.mEncoding));
        doc->SetDocumentCharacterSetSource(kCharsetFromOtherComponent);
    }
    else {
        doc->SetDocumentCharacterSet(sourceDoc->GetDocumentCharacterSet());
        doc->SetDocumentCharacterSetSource(
            sourceDoc->GetDocumentCharacterSetSource());
    }

    // Notify the contentsink that the document is created
    nsCOMPtr<nsITransformObserver> observer = do_QueryReferent(mObserver);
    if (observer) {
        observer->OnDocumentCreated(mDocument);
    }

    // Create the content

    // When transforming into a non-displayed document (i.e. when there is no
    // observer) we only create a transformiix:result root element.
    // Don't do this when called through nsIXSLTProcessorObsolete (i.e. when
    // aResultDocument is set) for compability reasons
    nsCOMPtr<nsIDOMNode> textContainer;
    if (!aResultDocument && !observer) {
        nsCOMPtr<nsIDOMElement> docElement;
        mDocument->CreateElementNS(NS_LITERAL_STRING(kTXNameSpaceURI),
                                   NS_LITERAL_STRING(kTXWrapper),
                                   getter_AddRefs(docElement));
        NS_ASSERTION(docElement, "Failed to create wrapper element");
        if (!docElement) {
            return;
        }

        rv = mDocument->AppendChild(docElement, getter_AddRefs(textContainer));
        NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to append the wrapper element");
        if (NS_FAILED(rv)) {
            return;
        }
    }
    else {
        nsCOMPtr<nsIDOMElement> element, docElement;
        nsCOMPtr<nsIDOMNode> parent, pre;

        NS_NAMED_LITERAL_STRING(XHTML_NSURI, "http://www.w3.org/1999/xhtml");

        mDocument->CreateElementNS(XHTML_NSURI,
                                   NS_LITERAL_STRING("html"),
                                   getter_AddRefs(docElement));
        nsCOMPtr<nsIContent> rootContent = do_QueryInterface(docElement);
        NS_ASSERTION(rootContent, "Need root element");
        if (!rootContent) {
            return;
        }

        rootContent->SetDocument(doc, PR_FALSE, PR_TRUE);

        doc->SetRootContent(rootContent);

        mDocument->CreateElementNS(XHTML_NSURI,
                                   NS_LITERAL_STRING("head"),
                                   getter_AddRefs(element));
        NS_ASSERTION(element, "Failed to create head element");
        if (!element) {
            return;
        }

        rv = docElement->AppendChild(element, getter_AddRefs(parent));
        NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to append the head element");
        if (NS_FAILED(rv)) {
            return;
        }

        mDocument->CreateElementNS(XHTML_NSURI,
                                   NS_LITERAL_STRING("body"),
                                   getter_AddRefs(element));
        NS_ASSERTION(element, "Failed to create body element");
        if (!element) {
            return;
        }

        rv = docElement->AppendChild(element, getter_AddRefs(parent));
        NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to append the body element");
        if (NS_FAILED(rv)) {
            return;
        }

        mDocument->CreateElementNS(XHTML_NSURI,
                                   NS_LITERAL_STRING("pre"),
                                   getter_AddRefs(element));
        NS_ASSERTION(element, "Failed to create pre element");
        if (!element) {
            return;
        }

        rv = parent->AppendChild(element, getter_AddRefs(pre));
        NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to append the pre element");
        if (NS_FAILED(rv)) {
            return;
        }

        nsCOMPtr<nsIDOMHTMLElement> htmlElement = do_QueryInterface(pre);
        htmlElement->SetId(NS_LITERAL_STRING("transformiixResult"));
        NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to append the id");
        
        textContainer = pre;
    }

    nsCOMPtr<nsIDOMText> textNode;
    mDocument->CreateTextNode(nsString(),
                              getter_AddRefs(textNode));
    NS_ASSERTION(textNode, "Failed to create the text node");
    if (!textNode) {
        return;
    }

    nsCOMPtr<nsIDOMNode> dummy;
    rv = textContainer->AppendChild(textNode, getter_AddRefs(dummy));
    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to append the text node");
    if (NS_FAILED(rv)) {
        return;
    }

    mTextNode = textNode;
}

void txMozillaTextOutput::startElement(const nsAString& aName,
                                       const PRInt32 aNsID)
{
}

void txMozillaTextOutput::getOutputDocument(nsIDOMDocument** aDocument)
{
    *aDocument = mDocument;
    NS_IF_ADDREF(*aDocument);
}
