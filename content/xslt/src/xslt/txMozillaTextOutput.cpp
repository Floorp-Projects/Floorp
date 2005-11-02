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

#include "txMozillaTextOutput.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMText.h"
#include "TxString.h"

txMozillaTextOutput::txMozillaTextOutput()
{
}

txMozillaTextOutput::~txMozillaTextOutput()
{
}

void txMozillaTextOutput::attribute(const String& aName,
                                    const PRInt32 aNsID,
                                    const String& aValue)
{
}

void txMozillaTextOutput::characters(const String& aData)
{
    if (mTextNode)
        mTextNode->AppendData(aData.getConstNSString());
}

void txMozillaTextOutput::comment(const String& aData)
{
}

void txMozillaTextOutput::disableStylesheetLoad()
{
}

void txMozillaTextOutput::endDocument()
{
}

void txMozillaTextOutput::endElement(const String& aName,
                                     const PRInt32 aNsID)
{
}

nsresult txMozillaTextOutput::getRootContent(nsIContent** aReturn)
{
    NS_ASSERTION(aReturn, "NULL pointer passed to getRootContent");

    *aReturn = mRootContent;
    NS_IF_ADDREF(*aReturn);
    return NS_OK;
}

PRBool txMozillaTextOutput::isDone()
{
    return PR_TRUE;
}

void txMozillaTextOutput::processingInstruction(const String& aTarget,
                                                const String& aData)
{
}

void txMozillaTextOutput::removeScriptElement(nsIDOMHTMLScriptElement *aElement)
{
    NS_NOTREACHED("No script elements in text output, so this is weird!");
}

void txMozillaTextOutput::setOutputDocument(nsIDOMDocument* aDocument)
{
    NS_ASSERTION(aDocument, "Document can't be NULL!");
    if (!aDocument)
        return;

    /*
     * Create an XHTML document to hold the text.
     *
     * <html>
     *   <head />
     *   <body>
     *     <pre> * The text comes here * </pre>
     *   <body>
     * </html>
     */

    nsCOMPtr<nsIDOMElement> element, docElement;
    nsCOMPtr<nsIDOMNode> parent, pre;
    nsCOMPtr<nsIDOMText> textNode;

    NS_NAMED_LITERAL_STRING(XHTML_NSURI, "http://www.w3.org/1999/xhtml");

    aDocument->CreateElementNS(XHTML_NSURI,
                               NS_LITERAL_STRING("html"),
                               getter_AddRefs(docElement));
    mRootContent = do_QueryInterface(docElement);
    NS_ASSERTION(mRootContent, "Need root element");
    if (!mRootContent)
        return;

    nsCOMPtr<nsIDocument> document = do_QueryInterface(aDocument);
    NS_ASSERTION(document, "Need document");

    nsresult rv = mRootContent->SetDocument(document, PR_FALSE, PR_TRUE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to set the document");
    if (NS_FAILED(rv))
        return;

    rv = document->SetRootContent(mRootContent);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to set the root content");
    if (NS_FAILED(rv))
        return;

    aDocument->CreateElementNS(XHTML_NSURI,
                               NS_LITERAL_STRING("head"),
                               getter_AddRefs(element));
    NS_ASSERTION(element, "Failed to create head element");
    if (!element)
        return;

    rv = docElement->AppendChild(element, getter_AddRefs(parent));
    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to append the head element");
    if (NS_FAILED(rv))
        return;

    aDocument->CreateElementNS(XHTML_NSURI,
                               NS_LITERAL_STRING("body"),
                               getter_AddRefs(element));
    NS_ASSERTION(element, "Failed to create body element");
    if (!mRootContent)
        return;

    rv = docElement->AppendChild(element, getter_AddRefs(parent));
    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to append the body element");
    if (NS_FAILED(rv))
        return;

    aDocument->CreateElementNS(XHTML_NSURI,
                               NS_LITERAL_STRING("pre"),
                               getter_AddRefs(element));
    NS_ASSERTION(element, "Failed to create pre element");
    if (!element)
        return;

    rv = parent->AppendChild(element, getter_AddRefs(pre));
    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to append the pre element");
    if (NS_FAILED(rv))
        return;

    nsCOMPtr<nsIDOMHTMLElement> htmlElement = do_QueryInterface(pre);
    htmlElement->SetId(NS_LITERAL_STRING("transformiixResult"));
    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to append the id");

    aDocument->CreateTextNode(NS_LITERAL_STRING(""),
                              getter_AddRefs(textNode));
    NS_ASSERTION(textNode, "Failed to create the text node");
    if (!textNode)
        return;

    rv = pre->AppendChild(textNode, getter_AddRefs(parent));
    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to append the text node");
    if (NS_FAILED(rv))
        return;

    mTextNode = textNode;
}

void txMozillaTextOutput::setOutputFormat(txOutputFormat* aOutputFormat)
{
    mOutputFormat.reset();
    mOutputFormat.merge(*aOutputFormat);
    mOutputFormat.setFromDefaults();
}

void txMozillaTextOutput::startDocument()
{
}

void txMozillaTextOutput::startElement(const String& aName,
                                       const PRInt32 aNsID)
{
}
