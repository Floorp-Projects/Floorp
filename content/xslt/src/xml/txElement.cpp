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
Element::Element(const String& tagName, Document* owner) :
         NodeDefinition(Node::ELEMENT_NODE, tagName, NULL_STRING, owner)
{
  mAttributes.ownerElement = this;
  mNamespaceID = kNameSpaceID_Unknown;

  int idx = nodeName.indexOf(':');
  if (idx == kNotFound) {
    mLocalName = TX_GET_ATOM(nodeName);
  }
  else {
    String tmp;
    nodeName.subString(idx+1, tmp);
    mLocalName = TX_GET_ATOM(tmp);
  }
}

Element::Element(const String& aNamespaceURI,
                 const String& aTagName,
                 Document* aOwner) :
         NodeDefinition(Node::ELEMENT_NODE, aTagName, NULL_STRING, aOwner)
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
MBool Element::getLocalName(txAtom** aLocalName)
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
  int idx = nodeName.indexOf(':');
  if (idx == kNotFound) {
    Node* node = this;
    while (node && node->getNodeType() == Node::ELEMENT_NODE) {
      String nsURI;
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
    String prefix;
    nodeName.subString(0, idx, prefix);
    txAtom* prefixAtom = TX_GET_ATOM(prefix);
    mNamespaceID = lookupNamespaceID(prefixAtom);
    TX_IF_RELEASE_ATOM(prefixAtom);
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
void Element::setAttribute(const String& name, const String& value)
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

void Element::setAttributeNS(const String& aNamespaceURI,
                             const String& aName,
                             const String& aValue)
{
  // Check to see if an attribute with this name already exists. If it does
  // overwrite its value, if not, add it.
  PRInt32 namespaceID = txNamespaceManager::getNamespaceID(aNamespaceURI);
  String localPart;
  XMLUtils::getLocalPart(aName, localPart);
  txAtom* localName = TX_GET_ATOM(localPart);

  Attr* foundNode = 0;
  AttrMap::ListItem* item = mAttributes.firstItem;
  while (item) {
    foundNode = (Attr*)item->node;
    txAtom* attrName;
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
  TX_IF_RELEASE_ATOM(localName);

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
Attr* Element::getAttributeNode(const String& name)
{
  return (Attr*)mAttributes.getNamedItem(name);
}

//
// Return true if the attribute specified by localname and nsID
// exists, and sets aValue to the value of the attribute.
// Return false, if the attribute does not exist.
//
MBool Element::getAttr(txAtom* aLocalName, PRInt32 aNSID,
                       String& aValue)
{
  aValue.Truncate();
  AttrMap::ListItem* item = mAttributes.firstItem;
  while (item) {
    Attr* attrNode = (Attr*)item->node;
    txAtom* localName;
    if (attrNode->getLocalName(&localName) &&
        aNSID == attrNode->getNamespaceID() &&
        aLocalName == localName) {
      aValue.Append(attrNode->getValue());
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
MBool Element::hasAttr(txAtom* aLocalName, PRInt32 aNSID)
{
  AttrMap::ListItem* item = mAttributes.firstItem;
  while (item) {
    Attr* attrNode = (Attr*)item->node;
    txAtom* localName;
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
