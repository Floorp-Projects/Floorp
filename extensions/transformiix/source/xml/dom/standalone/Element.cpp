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

#include "dom.h"
#include "txAtoms.h"
#include "XMLUtils.h"

//
//Construct a new element with the specified tagName and Document owner.
//Simply call the constructor for NodeDefinition, and specify the proper node
//type.
//
Element::Element(const nsAString& tagName, Document* owner) :
         NodeDefinition(Node::ELEMENT_NODE, tagName, EmptyString(), owner)
{
  mAttributes.ownerElement = this;
  mNamespaceID = kNameSpaceID_Unknown;

  int idx = tagName.FindChar(':');
  if (idx == kNotFound) {
    mLocalName = do_GetAtom(tagName);
  }
  else {
    mLocalName = do_GetAtom(Substring(tagName, idx + 1,
                                      tagName.Length() - (idx + 1)));
  }
}

Element::Element(const nsAString& aNamespaceURI,
                 const nsAString& aTagName,
                 Document* aOwner) :
         NodeDefinition(Node::ELEMENT_NODE, aTagName, EmptyString(), aOwner)
{
  Element(aTagName, aOwner);
  if (aNamespaceURI.IsEmpty())
    mNamespaceID = kNameSpaceID_None;
  else
    mNamespaceID = txStandaloneNamespaceManager::getNamespaceID(aNamespaceURI);
}

//
// This element is being destroyed, so destroy all attributes stored
// in the mAttributes NamedNodeMap.
//
Element::~Element()
{
  mAttributes.clear();
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
  NS_ADDREF(*aLocalName);
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
          mNamespaceID = txStandaloneNamespaceManager::getNamespaceID(nsURI);
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
  PRInt32 namespaceID =
      txStandaloneNamespaceManager::getNamespaceID(aNamespaceURI);
  nsCOMPtr<nsIAtom> localName = do_GetAtom(XMLUtils::getLocalPart(aName));

  Attr* foundNode = 0;
  AttrMap::ListItem* item = mAttributes.firstItem;
  while (item) {
    foundNode = (Attr*)item->node;
    nsCOMPtr<nsIAtom> attrName;
    if (foundNode->getLocalName(getter_AddRefs(attrName)) &&
        namespaceID == foundNode->getNamespaceID() &&
        localName == attrName) {
      break;
    }
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
    nsCOMPtr<nsIAtom> localName;
    if (attrNode->getLocalName(getter_AddRefs(localName)) &&
        aNSID == attrNode->getNamespaceID() &&
        aLocalName == localName) {
      attrNode->getNodeValue(aValue);
      return MB_TRUE;
    }
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
    nsCOMPtr<nsIAtom> localName;
    if (attrNode->getLocalName(getter_AddRefs(localName)) &&
        aNSID == attrNode->getNamespaceID() &&
        aLocalName == localName) {
      return MB_TRUE;
    }
    item = item->next;
  }
  return MB_FALSE;
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
