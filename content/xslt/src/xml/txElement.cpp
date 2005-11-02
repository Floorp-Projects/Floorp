/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * (C) Copyright The MITRE Corporation 1999  All rights reserved.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * The program provided "as is" without any warranty express or
 * implied, including the warranty of non-infringement and the implied
 * warranties of merchantibility and fitness for a particular purpose.
 * The Copyright owner will not be liable for any damages suffered by
 * you as a result of using the Program. In no event will the Copyright
 * owner be liable for any special, indirect or consequential damages or
 * lost profits even if the Copyright owner has been advised of the
 * possibility of their occurrence.
 *
 * Please see release.txt distributed with this file for more information.
 *
 */
// Tom Kneeland (3/29/99)
//
//  Implementation of the Document Object Model Level 1 Core
//    Implementation of the Element class
//

#include "dom.h"
#include "txAtoms.h"
#include "XMLUtils.h"

//
//Construct a new element with the specified tagName and Document owner.
//Simply call the constructor for NodeDefinition, and specify the proper node
//type.
//
Element::Element(const nsAString& tagName, Document* owner) :
         NodeDefinition(Node::ELEMENT_NODE, tagName, nsString(), owner)
{
  mAttributes.ownerElement = this;
  mNamespaceID = kNameSpaceID_Unknown;

  int idx = tagName.FindChar(':');
  if (idx == kNotFound) {
    mLocalName = TX_GET_ATOM(tagName);
  }
  else {
    mLocalName = TX_GET_ATOM(Substring(tagName, idx + 1,
                                       tagName.Length() - (idx + 1)));
  }
}

Element::Element(const nsAString& aNamespaceURI,
                 const nsAString& aTagName,
                 Document* aOwner) :
         NodeDefinition(Node::ELEMENT_NODE, aTagName, nsString(), aOwner)
{
  Element(aTagName, aOwner);
  if (aNamespaceURI.IsEmpty())
    mNamespaceID = kNameSpaceID_None;
  else
    mNamespaceID = txNamespaceManager::getNamespaceID(aNamespaceURI);
}

//
// This element is being destroyed, so destroy all attributes stored
// in the mAttributes NamedNodeMap.
//
Element::~Element()
{
  mAttributes.clear();
  TX_IF_RELEASE_ATOM(mLocalName);
}

Node* Element::appendChild(Node* newChild)
{
  switch (newChild->getNodeType())
    {
      case Node::ELEMENT_NODE :
      case Node::TEXT_NODE :
      case Node::COMMENT_NODE :
      case Node::PROCESSING_INSTRUCTION_NODE :
        {
          // Remove the "newChild" if it is already a child of this node
          NodeDefinition* pNewChild = (NodeDefinition*)newChild;
          if (pNewChild->getParentNode() == this)
            pNewChild = implRemoveChild(pNewChild);

          return implAppendChild(pNewChild);
        }

      default:
        break;
    }

  return nsnull;
}

//
//Return the elements local (unprefixed) name.
//
MBool Element::getLocalName(nsIAtom** aLocalName)
{
  if (!aLocalName)
    return MB_FALSE;
  *aLocalName = mLocalName;
  TX_ADDREF_ATOM(*aLocalName);
  return MB_TRUE;
}

//
//Return the namespace the elements belongs to.
//
PRInt32 Element::getNamespaceID()
{
  if (mNamespaceID>=0)
    return mNamespaceID;
  int idx = nodeName.FindChar(':');
  if (idx == kNotFound) {
    Node* node = this;
    while (node && node->getNodeType() == Node::ELEMENT_NODE) {
      nsAutoString nsURI;
      if (((Element*)node)->getAttr(txXMLAtoms::xmlns, kNameSpaceID_XMLNS,
                                    nsURI)) {
        // xmlns = "" sets the default namespace ID to kNameSpaceID_None;
        if (!nsURI.IsEmpty()) {
          mNamespaceID = txNamespaceManager::getNamespaceID(nsURI);
        }
        else {
          mNamespaceID = kNameSpaceID_None;
        }
        return mNamespaceID;
      }
      node = node->getParentNode();
    }
    mNamespaceID = kNameSpaceID_None;
  }
  else {
    nsCOMPtr<nsIAtom> prefix = do_GetAtom(Substring(nodeName, 0, idx));
    mNamespaceID = lookupNamespaceID(prefix);
  }
  return mNamespaceID;
}

NamedNodeMap* Element::getAttributes()
{
  return &mAttributes;
}

//
//Add an attribute to this Element.  Create a new Attr object using the
//name and value specified.  Then add the Attr to the the Element's
//mAttributes NamedNodeMap.
//
void Element::setAttribute(const nsAString& name, const nsAString& value)
{
  // Check to see if an attribute with this name already exists. If it does
  // overwrite its value, if not, add it.
  Attr* tempAttribute = getAttributeNode(name);
  if (tempAttribute) {
    tempAttribute->setNodeValue(value);
  }
  else {
    tempAttribute = getOwnerDocument()->createAttribute(name);
    tempAttribute->setNodeValue(value);
    tempAttribute->ownerElement = this;
    mAttributes.append(tempAttribute);
  }
}

void Element::setAttributeNS(const nsAString& aNamespaceURI,
                             const nsAString& aName,
                             const nsAString& aValue)
{
  // Check to see if an attribute with this name already exists. If it does
  // overwrite its value, if not, add it.
  PRInt32 namespaceID = txNamespaceManager::getNamespaceID(aNamespaceURI);
  nsCOMPtr<nsIAtom> localName;
  XMLUtils::getLocalPart(aName, getter_AddRefs(localName));

  Attr* foundNode = 0;
  AttrMap::ListItem* item = mAttributes.firstItem;
  while (item) {
    foundNode = (Attr*)item->node;
    nsIAtom* attrName;
    if (foundNode->getLocalName(&attrName) &&
        namespaceID == foundNode->getNamespaceID() &&
        localName == attrName) {
      TX_IF_RELEASE_ATOM(attrName);
      break;
    }
    TX_IF_RELEASE_ATOM(attrName);
    foundNode = 0;
    item = item->next;
  }

  if (foundNode) {
    foundNode->setNodeValue(aValue);
  }
  else {
    Attr* temp = getOwnerDocument()->createAttributeNS(aNamespaceURI,
                                                       aName);
    temp->setNodeValue(aValue);
    temp->ownerElement = this;
    mAttributes.append(temp);
  }
}

//
//Return the attribute specified by name
//
Attr* Element::getAttributeNode(const nsAString& name)
{
  return (Attr*)mAttributes.getNamedItem(name);
}

//
// Return true if the attribute specified by localname and nsID
// exists, and sets aValue to the value of the attribute.
// Return false, if the attribute does not exist.
//
MBool Element::getAttr(nsIAtom* aLocalName, PRInt32 aNSID,
                       nsAString& aValue)
{
  aValue.Truncate();
  AttrMap::ListItem* item = mAttributes.firstItem;
  while (item) {
    Attr* attrNode = (Attr*)item->node;
    nsIAtom* localName;
    if (attrNode->getLocalName(&localName) &&
        aNSID == attrNode->getNamespaceID() &&
        aLocalName == localName) {
      attrNode->getNodeValue(aValue);
      TX_IF_RELEASE_ATOM(localName);
      return MB_TRUE;
    }
    TX_IF_RELEASE_ATOM(localName);
    item = item->next;
  }
  return MB_FALSE;
}

//
// Return true if the attribute specified by localname and nsID
// exists. Return false otherwise.
//
MBool Element::hasAttr(nsIAtom* aLocalName, PRInt32 aNSID)
{
  AttrMap::ListItem* item = mAttributes.firstItem;
  while (item) {
    Attr* attrNode = (Attr*)item->node;
    nsIAtom* localName;
    if (attrNode->getLocalName(&localName) &&
        aNSID == attrNode->getNamespaceID() &&
        aLocalName == localName) {
      TX_IF_RELEASE_ATOM(localName);
      return MB_TRUE;
    }
    TX_IF_RELEASE_ATOM(localName);
    item = item->next;
  }
  return MB_FALSE;
}
