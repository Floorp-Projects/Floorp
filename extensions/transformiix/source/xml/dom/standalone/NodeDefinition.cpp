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

NodeDefinition::NodeDefinition(NodeType type, const nsAString& name,
                               const nsAString& value, Document* owner)
{
  nodeName = name;
  Init(type, value, owner);
}

NodeDefinition::NodeDefinition(NodeType aType, const nsAString& aValue,
                               Document* aOwner)
{
  switch (aType)
  {
    case CDATA_SECTION_NODE:
    {
      nodeName = NS_LITERAL_STRING("#cdata-section");
      break;
    }
    case COMMENT_NODE:
    {
      nodeName = NS_LITERAL_STRING("#comment");
      break;
    }
    case DOCUMENT_NODE:
    {
      nodeName = NS_LITERAL_STRING("#document");
      break;
    }
    case DOCUMENT_FRAGMENT_NODE:
    {
      nodeName = NS_LITERAL_STRING("#document-fragment");
      break;
    }
    case TEXT_NODE:
    {
      nodeName = NS_LITERAL_STRING("#text");
      break;
    }
    default:
    {
      break;
    }
  }
  Init(aType, aValue, aOwner);
}

//
// This node is being destroyed, so loop through and destroy all the children.
//
NodeDefinition::~NodeDefinition()
{
  DeleteChildren();
  delete mOrderInfo;
}

void
NodeDefinition::Init(NodeType aType, const nsAString& aValue,
                     Document* aOwner)
{
  nodeType = aType;
  nodeValue = aValue;
  ownerDocument = aOwner;

  parentNode = nsnull;
  previousSibling = nsnull;
  nextSibling = nsnull;;
  firstChild = nsnull;
  lastChild = nsnull;

  length = 0;

  mOrderInfo = 0;
}

//
//Remove and delete all children of this node
//
void NodeDefinition::DeleteChildren()
{
  NodeDefinition* pCurrent = firstChild;
  NodeDefinition* pDestroyer;

  while (pCurrent)
    {
      pDestroyer = pCurrent;
      pCurrent = pCurrent->nextSibling;
      delete pDestroyer;
    }

  length = 0;
  firstChild = nsnull;
  lastChild = nsnull;
}

nsresult NodeDefinition::getNodeName(nsAString& aName) const
{
  aName = nodeName;
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

NamedNodeMap* NodeDefinition::getAttributes()
{
  return 0;
}

Document* NodeDefinition::getOwnerDocument() const
{
  return ownerDocument;
}

Node* NodeDefinition::item(PRUint32 index)
{
  PRUint32 selectLoop;
  NodeDefinition* pSelectNode = firstChild;

  if (index < length)
    {
      for (selectLoop=0;selectLoop<index;selectLoop++)
        pSelectNode = pSelectNode->nextSibling;

      return pSelectNode;
    }

  return nsnull;
}

PRUint32 NodeDefinition::getLength()
{
  return length;
}

void NodeDefinition::setNodeValue(const nsAString& newNodeValue)
{
  nodeValue = newNodeValue;
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

//
// Looks up the Namespace associated with a certain prefix in the context of
// this node.
//
// @return namespace associated with prefix
//
PRInt32 NodeDefinition::lookupNamespaceID(nsIAtom* aPrefix)
{
  // this is http://www.w3.org/2000/xmlns/,
  // ID = kNameSpaceID_XMLNS, see txStandaloneNamespaceManager::Init
  if (aPrefix == txXMLAtoms::xmlns)
    return kNameSpaceID_XMLNS; 
  // this is http://www.w3.org/XML/1998/namespace,
  // ID = kNameSpaceID_XML, see txStandaloneNamespaceManager::Init
  if (aPrefix == txXMLAtoms::xml)
    return kNameSpaceID_XML; 

  Node* node = this;
  if (node->getNodeType() != Node::ELEMENT_NODE)
    node = node->getXPathParent();

  nsAutoString name(NS_LITERAL_STRING("xmlns:"));
  if (aPrefix && (aPrefix != txXMLAtoms::_empty)) {
      //  We have a prefix, search for xmlns:prefix attributes.
      nsAutoString prefixString;
      aPrefix->ToString(prefixString);
      name.Append(prefixString);
  }
  else {
      // No prefix, look up the default namespace by searching for xmlns
      // attributes. Remove the trailing :, set length to 5 (xmlns).
      name.Truncate(5);
  }
  Attr* xmlns;
  while (node && node->getNodeType() == Node::ELEMENT_NODE) {
    if ((xmlns = ((Element*)node)->getAttributeNode(name))) {
      /*
       * xmlns:foo = "" makes "" a valid URI, so get that.
       * xmlns = "" resolves to 0 (null Namespace) (caught above)
       * in Element::getNamespaceID()
       */
      nsAutoString nsURI;
      xmlns->getNodeValue(nsURI);
      return txStandaloneNamespaceManager::getNamespaceID(nsURI);
    }
    node = node->getXPathParent();
  }
  if (!aPrefix || (aPrefix == txXMLAtoms::_empty))
      return kNameSpaceID_None;
  return kNameSpaceID_Unknown;
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
      PRUint32 i;
      NamedNodeMap* attrs = elem->getAttributes();
      for (i = 0; i < attrs->getLength(); ++i) {
        if (attrs->item(i) == this) {
          mOrderInfo->mOrder[lastElem] = i + kTxAttrIndexOffset;
          return mOrderInfo;
        }
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
