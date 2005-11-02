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
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@propagandism.org>
 *   Axel Hecht <axel@pike.org>
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

#include "txXPathTreeWalker.h"
#include "nsPrintfCString.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "XMLUtils.h"

txXPathTreeWalker::txXPathTreeWalker(const txXPathTreeWalker& aOther)
    : mPosition(aOther.mPosition)
{
}

txXPathTreeWalker::txXPathTreeWalker(const txXPathNode& aNode)
    : mPosition(aNode)
{
}

txXPathTreeWalker::~txXPathTreeWalker()
{
}

#define INNER mPosition.mInner

PRBool
txXPathTreeWalker::moveToElementById(const nsAString& aID)
{
    Document* document;
    if (INNER->nodeType == Node::DOCUMENT_NODE) {
        document = NS_STATIC_CAST(Document*, INNER);
    }
    else {
        document = INNER->ownerDocument;
    }

    Element* element =
        document->getElementById(aID);
    if (!element) {
        return PR_FALSE;
    }

    INNER = element;

    return PR_TRUE;
}

PRBool
txXPathTreeWalker::moveToFirstAttribute()
{
    if (INNER->nodeType != Node::ELEMENT_NODE) {
        return PR_FALSE;
    }

    Element* element = NS_STATIC_CAST(Element*, INNER);
    Attr *attribute = element->getFirstAttribute();
    while (attribute) {
        if (attribute->getNamespaceID() != kNameSpaceID_XMLNS) {
            INNER = attribute;

            return PR_TRUE;
        }

        attribute = attribute->getNextAttribute();
    }

    return PR_FALSE;
}

PRBool
txXPathTreeWalker::moveToNextAttribute()
{
    // XXX an assertion should be enough here with the current code
    if (INNER->nodeType != Node::ATTRIBUTE_NODE) {
        return PR_FALSE;
    }

    Element* element = NS_STATIC_CAST(Element*, INNER->getXPathParent());
    Attr *attribute = element->getFirstAttribute();
    while (attribute != INNER) {
        attribute = attribute->getNextAttribute();
    }
    NS_ASSERTION(attribute, "Attr not attribute of it's owner?");

    attribute = attribute->getNextAttribute();

    while (attribute) {
        if (attribute->getNamespaceID() != kNameSpaceID_XMLNS) {
            INNER = attribute;

            return PR_TRUE;
        }

        attribute = attribute->getNextAttribute();
    }

    return PR_FALSE;
}

PRBool
txXPathTreeWalker::moveToFirstChild()
{
    if (!INNER->firstChild) {
        return PR_FALSE;
    }

    INNER = INNER->firstChild;

    return PR_TRUE;
}

PRBool
txXPathTreeWalker::moveToLastChild()
{
    if (!INNER->lastChild) {
        return PR_FALSE;
    }

    INNER = INNER->lastChild;

    return PR_TRUE;
}

PRBool
txXPathTreeWalker::moveToNextSibling()
{
    if (!INNER->nextSibling) {
        return PR_FALSE;
    }

    INNER = INNER->nextSibling;

    return PR_TRUE;
}

PRBool
txXPathTreeWalker::moveToPreviousSibling()
{
    if (!INNER->previousSibling) {
        return PR_FALSE;
    }

    INNER = INNER->previousSibling;

    return PR_TRUE;
}

PRBool
txXPathTreeWalker::moveToParent()
{
    if (INNER->nodeType == Node::ATTRIBUTE_NODE) {
        INNER = NS_STATIC_CAST(NodeDefinition*, INNER->getXPathParent());
        return PR_TRUE;
    }

    if (INNER->nodeType == Node::DOCUMENT_NODE) {
        return PR_FALSE;
    }

    NS_ASSERTION(INNER->parentNode, "orphaned node shouldn't happen");

    INNER = INNER->parentNode;

    return PR_TRUE;
}

txXPathNode::txXPathNode(const txXPathNode& aNode)
    : mInner(aNode.mInner)
{
}

PRBool
txXPathNode::operator==(const txXPathNode& aNode) const
{
    return (mInner == aNode.mInner);
}

/* static */
PRBool
txXPathNodeUtils::getAttr(const txXPathNode& aNode, nsIAtom* aLocalName,
                          PRInt32 aNSID, nsAString& aValue)
{
    if (aNode.mInner->getNodeType() != Node::ELEMENT_NODE) {
        return PR_FALSE;
    }

    Element* elem = NS_STATIC_CAST(Element*, aNode.mInner);
    return elem->getAttr(aLocalName, aNSID, aValue);
}

/* static */
already_AddRefed<nsIAtom>
txXPathNodeUtils::getLocalName(const txXPathNode& aNode)
{
    nsIAtom* localName;
    return aNode.mInner->getLocalName(&localName) ? localName : nsnull;
}

/* static */
void
txXPathNodeUtils::getLocalName(const txXPathNode& aNode, nsAString& aLocalName)
{
    nsCOMPtr<nsIAtom> localName;
    PRBool hasName = aNode.mInner->getLocalName(getter_AddRefs(localName));
    if (hasName && localName) {
        localName->ToString(aLocalName);
    }
}

/* static */
void
txXPathNodeUtils::getNodeName(const txXPathNode& aNode, nsAString& aName)
{
    aNode.mInner->getNodeName(aName);
}

/* static */
PRInt32
txXPathNodeUtils::getNamespaceID(const txXPathNode& aNode)
{
    return aNode.mInner->getNamespaceID();
}

/* static */
void
txXPathNodeUtils::getNamespaceURI(const txXPathNode& aNode, nsAString& aURI)
{
    aNode.mInner->getNamespaceURI(aURI);
}

/* static */
PRUint16
txXPathNodeUtils::getNodeType(const txXPathNode& aNode)
{
    return aNode.mInner->getNodeType();
}

/* static */
void
txXPathNodeUtils::appendNodeValueHelper(NodeDefinition* aNode,
                                        nsAString& aResult)
{

    NodeDefinition* child = NS_STATIC_CAST(NodeDefinition*,
                                           aNode->getFirstChild());
    while (child) {
        switch (child->getNodeType()) {
            case Node::TEXT_NODE:
            {
                aResult.Append(child->nodeValue);
            }
            case Node::ELEMENT_NODE:
            {
                appendNodeValueHelper(child, aResult);
            }
        }
        child = NS_STATIC_CAST(NodeDefinition*, child->getNextSibling());
    }
}

/* static */
void
txXPathNodeUtils::appendNodeValue(const txXPathNode& aNode, nsAString& aResult)
{
    unsigned short type = aNode.mInner->getNodeType();
    if (type == Node::ATTRIBUTE_NODE ||
        type == Node::COMMENT_NODE ||
        type == Node::PROCESSING_INSTRUCTION_NODE ||
        type == Node::TEXT_NODE) {
        aResult.Append(aNode.mInner->nodeValue);

        return;
    }

    NS_ASSERTION(type == Node::ELEMENT_NODE || type == Node::DOCUMENT_NODE,
                 "Element or Document expected");

    appendNodeValueHelper(aNode.mInner, aResult);
}

/* static */
PRBool
txXPathNodeUtils::isWhitespace(const txXPathNode& aNode)
{
    NS_ASSERTION(aNode.mInner->nodeType == Node::TEXT_NODE, "Wrong type!");

    return XMLUtils::isWhitespace(aNode.mInner->nodeValue);
}

/* static */
txXPathNode*
txXPathNodeUtils::getDocument(const txXPathNode& aNode)
{
    if (aNode.mInner->nodeType == Node::DOCUMENT_NODE) {
        return new txXPathNode(aNode);
    }

    return new txXPathNode(aNode.mInner->ownerDocument);
}

/* static */
txXPathNode*
txXPathNodeUtils::getOwnerDocument(const txXPathNode& aNode)
{
    return getDocument(aNode);
}

#ifndef HAVE_64BIT_OS
#define kFmtSize 13
const char gPrintfFmt[] = "id0x%08p";
#else
#define kFmtSize 21
const char gPrintfFmt[] = "id0x%016p";
#endif

/* static */
nsresult
txXPathNodeUtils::getXSLTId(const txXPathNode& aNode,
                            nsAString& aResult)
{
    CopyASCIItoUCS2(nsPrintfCString(kFmtSize, gPrintfFmt, aNode.mInner),
                    aResult);

    return NS_OK;
}

/* static */
void
txXPathNodeUtils::getBaseURI(const txXPathNode& aNode, nsAString& aURI)
{
    aNode.mInner->getBaseURI(aURI);
}

/* static */
PRIntn
txXPathNodeUtils::comparePosition(const txXPathNode& aNode,
                                  const txXPathNode& aOtherNode)
{
    // First check for equal nodes.
    if (aNode == aOtherNode) {
        return 0;
    }
    return aNode.mInner->compareDocumentPosition(aOtherNode.mInner);
}

/* static */
txXPathNode*
txXPathNativeNode::createXPathNode(Node* aNode)
{
    if (aNode != nsnull) {
        return new txXPathNode(NS_STATIC_CAST(NodeDefinition*, aNode));
    }
    return nsnull;
}

/* static */
nsresult
txXPathNativeNode::getElement(const txXPathNode& aNode, Element** aResult)
{
    if (aNode.mInner->getNodeType() != Node::ELEMENT_NODE) {
        return NS_ERROR_FAILURE;
    }

    *aResult = NS_STATIC_CAST(Element*, aNode.mInner);

    return NS_OK;

}

/* static */
nsresult
txXPathNativeNode::getDocument(const txXPathNode& aNode, Document** aResult)
{
    if (aNode.mInner->getNodeType() != Node::DOCUMENT_NODE) {
        return NS_ERROR_FAILURE;
    }

    *aResult = NS_STATIC_CAST(Document*, aNode.mInner);

    return NS_OK;
}
