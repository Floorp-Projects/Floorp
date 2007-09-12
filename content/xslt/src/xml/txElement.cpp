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
//    Implementation of the Element class
//

#include "txDOM.h"
#include "txAtoms.h"
#include "txXMLUtils.h"

Element::Element(nsIAtom *aPrefix, nsIAtom *aLocalName, PRInt32 aNamespaceID,
                 Document* aOwner) :
  NodeDefinition(Node::ELEMENT_NODE, aLocalName, EmptyString(), aOwner),
  mPrefix(aPrefix),
  mNamespaceID(aNamespaceID)
{
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

nsresult
Element::getNodeName(nsAString& aName) const
{
  if (mPrefix) {
    mPrefix->ToString(aName);
    aName.Append(PRUnichar(':'));
  }
  else {
    aName.Truncate();
  }

  const char* ASCIIAtom;
  mLocalName->GetUTF8String(&ASCIIAtom);
  AppendUTF8toUTF16(ASCIIAtom, aName);

  return NS_OK;
}

//
//Return the elements local (unprefixed) name.
//
MBool Element::getLocalName(nsIAtom** aLocalName)
{
  if (!aLocalName)
    return MB_FALSE;
  *aLocalName = mLocalName;
  NS_ADDREF(*aLocalName);
  return MB_TRUE;
}

//
//Return the namespace the elements belongs to.
//
PRInt32 Element::getNamespaceID()
{
  return mNamespaceID;
}

nsresult
Element::appendAttributeNS(nsIAtom *aPrefix, nsIAtom *aLocalName,
                           PRInt32 aNamespaceID, const nsAString& aValue)
{
  nsAutoPtr<Attr> newAttribute;
  newAttribute = new Attr(aPrefix, aLocalName, aNamespaceID, this, aValue);
  if (!newAttribute) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (mFirstAttribute) {
    Attr *lastAttribute = mFirstAttribute;
    while (lastAttribute->mNextAttribute) {
      lastAttribute = lastAttribute->mNextAttribute;
    }
    lastAttribute->mNextAttribute = newAttribute;
  }
  else {
    mFirstAttribute = newAttribute;
  }

  return NS_OK;
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
  Attr *attribute = mFirstAttribute;
  while (attribute) {
    if (attribute->equals(aLocalName, aNSID)) {
      attribute->getNodeValue(aValue);

      return PR_TRUE;
    }

    attribute = attribute->mNextAttribute;
  }

  return PR_FALSE;
}

//
// Return true if the attribute specified by localname and nsID
// exists. Return false otherwise.
//
MBool Element::hasAttr(nsIAtom* aLocalName, PRInt32 aNSID)
{
  Attr *attribute = mFirstAttribute;
  while (attribute) {
    if (attribute->equals(aLocalName, aNSID)) {
      return PR_TRUE;
    }

    attribute = attribute->mNextAttribute;
  }

  return PR_FALSE;
}

/**
 * ID accessors. Getter used for id() patterns, private setter for parser
 */
PRBool
Element::getIDValue(nsAString& aValue)
{
  if (mIDValue.IsEmpty()) {
    return PR_FALSE;
  }
  aValue = mIDValue;
  return PR_TRUE;
}

void
Element::setIDValue(const nsAString& aValue)
{
  mIDValue = aValue;
}
