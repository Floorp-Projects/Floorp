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
#include "txAtom.h"

const String XMLNS_ATTR = "xmlns";

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
  if (idx == NOT_FOUND) {
    mLocalName = TX_GET_ATOM(nodeName);
  }
  else {
    String tmp;
    nodeName.subString(idx+1, tmp);
    mLocalName = TX_GET_ATOM(tmp);
  }
}

//
// This element is being destroyed, so destroy all attributes stored
// in the mAttributes NamedNodeMap.
//
Element::~Element()
{
  mAttributes.clear();
  TX_RELEASE_IF_ATOM(mLocalName);
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
  if (idx == NOT_FOUND) {
    Node* node = this;
    while (node && node->getNodeType() == Node::ELEMENT_NODE) {
      String nsURI;
      if (((Element*)node)->getAttr(txXMLAtoms::XMLNSPrefix,
                                    kNameSpaceID_XMLNS, nsURI)) {
        // xmlns = "" sets the default namespace ID to kNameSpaceID_None;
        if (!nsURI.isEmpty()) {
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
    TX_RELEASE_IF_ATOM(prefixAtom);
  }
  return mNamespaceID;
}

//
//First check to see if the new node is an allowable child for an Element.  If
//it is, call NodeDefinition's implementation of Insert Before.  If not, return
//null as an error
//
Node* Element::insertBefore(Node* newChild, Node* refChild)
{
  Node* returnVal = NULL;

  switch (newChild->getNodeType())
    {
      case Node::ELEMENT_NODE :
      case Node::TEXT_NODE :
      case Node::COMMENT_NODE :
      case Node::PROCESSING_INSTRUCTION_NODE :
      case Node::CDATA_SECTION_NODE :
      case Node::DOCUMENT_FRAGMENT_NODE : //-- added 19990813 (kvisco)
      case Node::ENTITY_REFERENCE_NODE:
        returnVal = NodeDefinition::insertBefore(newChild, refChild);
        break;
      default:
        returnVal = NULL;
    }

  return returnVal;
}

//
//Return the tagName for this element.  This is simply the nodeName.
//
const String& Element::getTagName()
{
  return nodeName;
}

NamedNodeMap* Element::getAttributes()
{
  return &mAttributes;
}

//
//Retreive an attribute's value by name.  If the attribute does not exist,
//return a reference to the pre-created, constatnt "NULL STRING".
//
const String& Element::getAttribute(const String& name)
{
  Node* tempNode = mAttributes.getNamedItem(name);

  if (tempNode)
    return mAttributes.getNamedItem(name)->getNodeValue();
  else
    return NULL_STRING;
}


//
//Add an attribute to this Element.  Create a new Attr object using the
//name and value specified.  Then add the Attr to the the Element's
//mAttributes NamedNodeMap.
//
void Element::setAttribute(const String& name, const String& value)
{
  Attr* tempAttribute;

  //Check to see if an attribute with this name already exists.  If it does
  //over write its value, if not, add it.
  tempAttribute = getAttributeNode(name);
  if (tempAttribute)
      tempAttribute->setNodeValue(value);
  else
    {
      tempAttribute = getOwnerDocument()->createAttribute(name);
      tempAttribute->setNodeValue(value);
      mAttributes.setNamedItem(tempAttribute);
    }
}

//
//Remove an attribute from the mAttributes NamedNodeMap, and free its memory.
//   NOTE:  How do default values enter into this picture
//
void Element::removeAttribute(const String& name)
{
  delete mAttributes.removeNamedItem(name);
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
  aValue.clear();
  AttrMap::ListItem* item = mAttributes.firstItem;
  while (item) {
    Attr* attrNode = (Attr*)item->node;
    txAtom* localName;
    if (attrNode->getLocalName(&localName) &&
        aNSID == attrNode->getNamespaceID() &&
        aLocalName == localName) {
      aValue.append(attrNode->getValue());
      TX_RELEASE_IF_ATOM(localName);
      return MB_TRUE;
    }
    TX_RELEASE_IF_ATOM(localName);
    item = item->next;
  }
  return MB_FALSE;
}

//
//Set a new attribute specifed by the newAttr node.  If an attribute with that
//name already exists, the existing Attr is removed from the list and return to
//the caller, else NULL is returned.
//
Attr* Element::setAttributeNode(Attr* newAttr)
{
  Attr* pOldAttr = (Attr*)mAttributes.removeNamedItem(newAttr->getNodeName());

  mAttributes.setNamedItem(newAttr);
  return pOldAttr;
}

//
//Remove the Attribute from the attributes list and return to the caller.  If
//the node is not found, return NULL.
//
Attr* Element::removeAttributeNode(Attr* oldAttr)
{
  return (Attr*)mAttributes.removeNamedItem(oldAttr->getNodeName());
}

NodeList* Element::getElementsByTagName(const String& name)
{
    return 0;
}

void Element::normalize()
{
}
