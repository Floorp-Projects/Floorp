/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The MITRE Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

// Tom Kneeland (3/29/99)
//
//  Implementation of the Document Object Model Level 1 Core
//    Implementation of the NodeDefinition Class
//
// Modification History:
// Who  When      What
// TK   03/29/99  Created
//

#include "dom.h"
#include "nsVoidArray.h"
#include "txURIUtils.h"
#include "txAtoms.h"
#include <string.h>

NodeDefinition::NodeDefinition(NodeType type, nsIAtom *aLocalName,
                               const nsAString& value, Document* owner) :
    mLocalName(aLocalName),
    nodeValue(value),
    nodeType(type),
    parentNode(nsnull),
    previousSibling(nsnull),
    nextSibling(nsnull),
    ownerDocument(owner),
    length(0),
    firstChild(nsnull),
    lastChild(nsnull),
    mOrderInfo(nsnull)
{
}

//
// This node is being destroyed, so loop through and destroy all the children.
//
NodeDefinition::~NodeDefinition()
{
  NodeDefinition* pCurrent = firstChild;
  NodeDefinition* pDestroyer;

  while (pCurrent)
    {
      pDestroyer = pCurrent;
      pCurrent = pCurrent->nextSibling;
      delete pDestroyer;
    }

  delete mOrderInfo;
}

nsresult NodeDefinition::getNodeName(nsAString& aName) const
{
  mLocalName->ToString(aName);
  return NS_OK;
}

nsresult NodeDefinition::getNodeValue(nsAString& aValue)
{
  aValue = nodeValue;
  return NS_OK;
}

unsigned short NodeDefinition::getNodeType() const
{
  return nodeType;
}

Node* NodeDefinition::getParentNode() const
{
  return parentNode;
}

Node* NodeDefinition::getFirstChild() const
{
  return firstChild;
}

Node* NodeDefinition::getLastChild() const
{
  return lastChild;
}

Node* NodeDefinition::getPreviousSibling() const
{
  return previousSibling;
}

Node* NodeDefinition::getNextSibling() const
{
  return nextSibling;
}

Document* NodeDefinition::getOwnerDocument() const
{
  return ownerDocument;
}

Node* NodeDefinition::appendChild(Node* newChild)
{
  return nsnull;
}

NodeDefinition* NodeDefinition::implAppendChild(NodeDefinition* newChild)
{
  // The new child should not be a child of any other node
  if (!newChild->previousSibling && !newChild->nextSibling &&
      !newChild->parentNode)
    {
      newChild->previousSibling = lastChild;

      if (lastChild)
        lastChild->nextSibling = newChild;

      lastChild = newChild;

      newChild->parentNode = this;

      if (!newChild->previousSibling)
        firstChild = newChild;

      ++length;

      return newChild;
    }

  return nsnull;
}

NodeDefinition* NodeDefinition::implRemoveChild(NodeDefinition* oldChild)
{
  if (oldChild != firstChild)
    oldChild->previousSibling->nextSibling = oldChild->nextSibling;
  else
    firstChild = oldChild->nextSibling;

  if (oldChild != lastChild)
    oldChild->nextSibling->previousSibling = oldChild->previousSibling;
  else
    lastChild = oldChild->previousSibling;

  oldChild->nextSibling = nsnull;
  oldChild->previousSibling = nsnull;
  oldChild->parentNode = nsnull;

  --length;

  return oldChild;
}

MBool NodeDefinition::hasChildNodes() const
{
  if (firstChild)
    return MB_TRUE;
  else
    return MB_FALSE;
}

MBool NodeDefinition::getLocalName(nsIAtom** aLocalName)
{
  if (!aLocalName)
    return MB_FALSE;
  *aLocalName = 0;
  return MB_TRUE;
}

nsresult NodeDefinition::getNamespaceURI(nsAString& aNSURI)
{
  return txStandaloneNamespaceManager::getNamespaceURI(getNamespaceID(),
                                                       aNSURI);
}

PRInt32 NodeDefinition::getNamespaceID()
{
  return kNameSpaceID_None;
}

Node* NodeDefinition::getXPathParent()
{
  return parentNode;
}

//
// Returns the base URI of the node. Acccounts for xml:base
// attributes.
//
// @return base URI for the node
//
nsresult NodeDefinition::getBaseURI(nsAString& aURI)
{
  Node* node = this;
  nsStringArray baseUrls;
  nsAutoString url;

  while (node) {
    switch (node->getNodeType()) {
      case Node::ELEMENT_NODE :
        if (((Element*)node)->getAttr(txXMLAtoms::base, kNameSpaceID_XML,
                                      url))
          baseUrls.AppendString(url);
        break;

      case Node::DOCUMENT_NODE :
        node->getBaseURI(url);
        baseUrls.AppendString(url);
        break;
    
      default:
        break;
    }
    node = node->getXPathParent();
  }

  PRInt32 count = baseUrls.Count();
  if (count) {
    baseUrls.StringAt(--count, aURI);

    while (count > 0) {
      nsAutoString dest;
      URIUtils::resolveHref(*baseUrls[--count], aURI, dest);
      aURI = dest;
    }
  }
  
  return NS_OK;
} // getBaseURI

/*
 * Compares document position of this node relative to another node
 */
PRInt32 NodeDefinition::compareDocumentPosition(Node* aOther)
{
  OrderInfo* myOrder = getOrderInfo();
  OrderInfo* otherOrder = ((NodeDefinition*)aOther)->getOrderInfo();
  if (!myOrder || !otherOrder)
      return -1;

  if (myOrder->mRoot == otherOrder->mRoot) {
    int c = 0;
    while (c < myOrder->mSize && c < otherOrder->mSize) {
      if (myOrder->mOrder[c] < otherOrder->mOrder[c])
        return -1;
      if (myOrder->mOrder[c] > otherOrder->mOrder[c])
        return 1;
      ++c;
    }
    if (c < myOrder->mSize)
      return 1;
    if (c < otherOrder->mSize)
      return -1;
    return 0;
  }

  if (myOrder->mRoot < otherOrder->mRoot)
    return -1;

  return 1;
}

/*
 * Get order information for node
 */
NodeDefinition::OrderInfo* NodeDefinition::getOrderInfo()
{
  if (mOrderInfo)
    return mOrderInfo;

  mOrderInfo = new OrderInfo;
  if (!mOrderInfo)
    return 0;

  Node* parent = getXPathParent();
  if (!parent) {
    mOrderInfo->mOrder = 0;
    mOrderInfo->mSize = 0;
    mOrderInfo->mRoot = this;
    return mOrderInfo;
  }

  OrderInfo* parentOrder = ((NodeDefinition*)parent)->getOrderInfo();
  mOrderInfo->mSize = parentOrder->mSize + 1;
  mOrderInfo->mRoot = parentOrder->mRoot;
  mOrderInfo->mOrder = new PRUint32[mOrderInfo->mSize];
  if (!mOrderInfo->mOrder) {
    delete mOrderInfo;
    mOrderInfo = 0;
    return 0;
  }
  memcpy(mOrderInfo->mOrder,
         parentOrder->mOrder,
         parentOrder->mSize * sizeof(PRUint32*));

  // Get childnumber of this node
  int lastElem = parentOrder->mSize;
  switch (getNodeType()) {
    case Node::ATTRIBUTE_NODE:
    {
      NS_ASSERTION(parent->getNodeType() == Node::ELEMENT_NODE,
                   "parent to attribute is not an element");

      Element* elem = (Element*)parent;
      Attr *attribute = elem->getFirstAttribute();
      PRUint32 i = 0;
      while (attribute) {
        if (attribute == this) {
          mOrderInfo->mOrder[lastElem] = i + kTxAttrIndexOffset;
          return mOrderInfo;
        }
        attribute = attribute->getNextAttribute();
        ++i;
      }
      break;
    }
    // XXX Namespace: need to take care of namespace nodes here
    default:
    {
      PRUint32 i = 0;
      Node * child = parent->getFirstChild();
      while (child) {
        if (child == this) {
          mOrderInfo->mOrder[lastElem] = i + kTxChildIndexOffset;
          return mOrderInfo;
        }
        ++i;
        child = child->getNextSibling();
      }
      break;
    }
  }

  NS_ASSERTION(0, "unable to get childnumber");
  mOrderInfo->mOrder[lastElem] = 0;
  return mOrderInfo;
}

/*
 * OrderInfo destructor
 */
NodeDefinition::OrderInfo::~OrderInfo()
{
    delete [] mOrder;
}
