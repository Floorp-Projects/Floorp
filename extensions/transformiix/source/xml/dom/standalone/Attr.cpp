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
//    Implementation of the Attr class
//

#include "dom.h"
#include "txAtoms.h"
#include "XMLUtils.h"

//
//Construct an Attribute object using the specified name and document owner
//
Attr::Attr(const String& name, Document* owner):
      NodeDefinition(Node::ATTRIBUTE_NODE, name, NULL_STRING, owner)
{
  int idx = nodeName.indexOf(':');
  if (idx == kNotFound) {
    mLocalName = TX_GET_ATOM(nodeName);
    if (mLocalName == txXMLAtoms::xmlns)
      mNamespaceID = kNameSpaceID_XMLNS;
    else
      mNamespaceID = kNameSpaceID_None;
  }
  else {
    String tmp;
    nodeName.subString(idx + 1, tmp);
    mLocalName = TX_GET_ATOM(tmp);
    // namespace handling has to be handled late, the attribute must
    // be added to the tree to resolve the prefix, unless it's
    // xmlns or xml, try to do that here
    String prefix;
    nodeName.subString(0, idx, prefix);
    txAtom* prefixAtom = TX_GET_ATOM(prefix);
    if (prefixAtom == txXMLAtoms::xmlns)
      mNamespaceID = kNameSpaceID_XMLNS;
    else if (prefixAtom == txXMLAtoms::xml)
      mNamespaceID = kNameSpaceID_XML;
    else
      mNamespaceID = kNameSpaceID_Unknown;
    TX_IF_RELEASE_ATOM(prefixAtom);
  }
}

Attr::Attr(const String& aNamespaceURI,
           const String& aName,
           Document* aOwner) :
    NodeDefinition(Node::ATTRIBUTE_NODE, aName, NULL_STRING, aOwner)
{
 if (aNamespaceURI.IsEmpty())
    mNamespaceID = kNameSpaceID_None;
  else
    mNamespaceID = txNamespaceManager::getNamespaceID(aNamespaceURI);

  String localPart;
  XMLUtils::getLocalPart(nodeName, localPart);
  mLocalName = TX_GET_ATOM(localPart);
}

//
//Release the mLocalName
//
Attr::~Attr()
{
  TX_IF_RELEASE_ATOM(mLocalName);
}

//
//Retrieve the value of the attribute.
//
const String& Attr::getValue()
{
  return nodeValue;
}

//
//Set the nodevalue to the given value.
//
void Attr::setValue(const String& newValue)
{
  nodeValue = newValue;
}


//
//Override the set node value member function to create a new TEXT node with
//the String and to add it as the Attribute's child.
//    NOTE:  Not currently impemented, just execute the default setNodeValue
//
void Attr::setNodeValue(const String& nodeValue)
{
  setValue(nodeValue);
}

//
//Return a String represening the value of this node.  If the value is an
//Entity Reference then return the value of the reference.  Otherwise, it is a
//simple conversion of the text value.
//    NOTE: Not currently implemented, just execute the default getNodeValue
//
const String& Attr::getNodeValue()
{
  return nodeValue;
}

//
//Not implemented anymore, return null as an error.
//
Node* Attr::appendChild(Node* newChild)
{
  NS_ASSERTION(0, "not implemented");
  return nsnull;
}

//
//Return the attributes local (unprefixed) name atom.
//
MBool Attr::getLocalName(txAtom** aLocalName)
{
  if (!aLocalName)
    return MB_FALSE;
  *aLocalName = mLocalName;
  TX_ADDREF_ATOM(*aLocalName);
  return MB_TRUE;
}

//
//Return the namespace the attribute belongs to. If the attribute doesn't
//have a prefix it doesn't belong to any namespace per the namespace spec,
//and is handled in the constructor.
//
PRInt32 Attr::getNamespaceID()
{
  if (mNamespaceID >= 0)
    return mNamespaceID;

  int idx = nodeName.indexOf(':');
  String prefix;
  nodeName.subString(0, idx, prefix);
  txAtom* prefixAtom = TX_GET_ATOM(prefix);
  mNamespaceID = lookupNamespaceID(prefixAtom);
  TX_IF_RELEASE_ATOM(prefixAtom);
  return mNamespaceID;
}

//
//Return the attributes owning element
//
Node* Attr::getXPathParent()
{
  return ownerElement;
}
