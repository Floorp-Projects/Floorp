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
//    Implementation of the Attr class
//

#include "dom.h"
#include "txAtoms.h"
#include "XMLUtils.h"

//
//Construct an Attribute object using the specified name and document owner
//
Attr::Attr(const nsAString& name, Document* owner):
      NodeDefinition(Node::ATTRIBUTE_NODE, name, EmptyString(), owner)
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
    NodeDefinition(Node::ATTRIBUTE_NODE, aName, EmptyString(), aOwner)
{
 if (aNamespaceURI.IsEmpty())
    mNamespaceID = kNameSpaceID_None;
  else
    mNamespaceID = txStandaloneNamespaceManager::getNamespaceID(aNamespaceURI);

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
