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

#include "txRtfHandler.h"

txRtfHandler::txRtfHandler(Document* aDocument,
                           txResultTreeFragment* aResultTreeFragment) :
                               mDocument(aDocument),
                               mResultTreeFragment(aResultTreeFragment)
{
    NS_ASSERTION(mDocument, "We need a valid document");
    if (!mDocument)
        return;

    NS_ASSERTION(mResultTreeFragment, "We need a valid result tree fragment");
    if (!mResultTreeFragment)
        return;

    DocumentFragment* fragment = mDocument->createDocumentFragment();
    NS_ASSERTION(fragment, "Out of memory creating a document fragmen");
    // XXX ErrorReport: Out of memory
    mResultTreeFragment->add(fragment);
    mCurrentNode = fragment;
}

txRtfHandler::~txRtfHandler()
{
}

void txRtfHandler::attribute(const String& aName,
                             const PRInt32 aNsID,
                             const String& aValue)
{
    Element* element = (Element*)mCurrentNode;
    NS_ASSERTION(element, "We need an element");
    if (!element)
        // XXX ErrorReport: Can't add attributes without element
        return;

    if (element->hasChildNodes())
        // XXX ErrorReport: Can't add attributes after adding children
        return;

    String nsURI;
    mDocument->namespaceIDToURI(aNsID, nsURI);
    element->setAttributeNS(nsURI, aName, aValue);
}

void txRtfHandler::characters(const String& aData)
{
    NS_ASSERTION(mCurrentNode, "We need a node");
    if (!mCurrentNode)
        return;

    Text* text = mDocument->createTextNode(aData);
    mCurrentNode->appendChild(text);
}

void txRtfHandler::comment(const String& aData)
{
    NS_ASSERTION(mCurrentNode, "We need a node");
    if (!mCurrentNode)
        return;

    Comment* comment = mDocument->createComment(aData);
    mCurrentNode->appendChild(comment);
}

void txRtfHandler::endDocument()
{
}

void txRtfHandler::endElement(const String& aName,
                              const PRInt32 aNsID)
{
    NS_ASSERTION(mCurrentNode, "We need a node");
    if (!mCurrentNode)
        return;

    mCurrentNode = mCurrentNode->getParentNode();
}

void txRtfHandler::processingInstruction(const String& aTarget,
                                         const String& aData)
{
    NS_ASSERTION(mCurrentNode, "We need a node");
    if (!mCurrentNode)
        return;

    ProcessingInstruction* pi;
    pi = mDocument->createProcessingInstruction(aTarget, aData);
    mCurrentNode->appendChild(pi);
}

void txRtfHandler::startDocument()
{
}

void txRtfHandler::startElement(const String& aName,
                                const PRInt32 aNsID)
{
    NS_ASSERTION(mCurrentNode, "We need a node");
    if (!mCurrentNode)
        return;

    String nsURI;
    mDocument->namespaceIDToURI(aNsID, nsURI);
    Element* element = mDocument->createElementNS(nsURI, aName);
    mCurrentNode->appendChild(element);
    mCurrentNode = element;
}
