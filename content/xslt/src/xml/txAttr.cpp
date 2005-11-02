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
Attr::Attr(const nsAString& name, Document* owner):
      NodeDefinition(Node::ATTRIBUTE_NODE, name, nsString(), owner)
{
  int idx = nodeName.FindChar(':');
  if (idx == kNotFound) {
    mLocalName = do_GetAtom(nodeName);
    if (mLocalName == txXMLAtoms::xmlns)
      mNamespaceID = kNameSpaceID_XMLNS;
    else
      mNamespaceID = kNameSpaceID_None;
  }
  else {
    mLocalName = do_GetAtom(Substring(nodeName, idx + 1,
                                      nodeName.Length() - (idx + 1)));
    // namespace handling has to be handled late, the attribute must
    // be added to the tree to resolve the prefix, unless it's
    // xmlns or xml, try to do that here
    nsCOMPtr<nsIAtom> prefixAtom = do_GetAtom(Substring(nodeName, 0, idx));
    if (prefixAtom == txXMLAtoms::xmlns)
      mNamespaceID = kNameSpaceID_XMLNS;
    else if (prefixAtom == txXMLAtoms::xml)
      mNamespaceID = kNameSpaceID_XML;
    else
      mNamespaceID = kNameSpaceID_Unknown;
  }
}

Attr::Attr(const nsAString& aNamespaceURI,
           const nsAString& aName,
           Document* aOwner) :
    NodeDefinition(Node::ATTRIBUTE_NODE, aName, nsString(), aOwner)
{
 if (aNamespaceURI.IsEmpty())
    mNamespaceID = kNameSpaceID_None;
  else
    mNamespaceID = txNamespaceManager::getNamespaceID(aNamespaceURI);

  mLocalName = do_GetAtom(XMLUtils::getLocalPart(nodeName));
}

//
//Release the mLocalName
//
Attr::~Attr()
{
}

void Attr::setNodeValue(const nsAString& aValue)
{
  nodeValue = aValue;
}

nsresult Attr::getNodeValue(nsAString& aValue)
{
  aValue = nodeValue;
  return NS_OK;
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
MBool Attr::getLocalName(nsIAtom** aLocalName)
{
  if (!aLocalName)
    return MB_FALSE;
  *aLocalName = mLocalName;
  NS_ADDREF(*aLocalName);
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

  mNamespaceID = kNameSpaceID_None;
  PRInt32 idx = nodeName.FindChar(':');
  if (idx != kNotFound) {
    nsCOMPtr<nsIAtom> prefixAtom = do_GetAtom(Substring(nodeName, 0, idx));
    mNamespaceID = lookupNamespaceID(prefixAtom);
  }
  return mNamespaceID;
}

//
//Return the attributes owning element
//
Node* Attr::getXPathParent()
{
  return ownerElement;
}
