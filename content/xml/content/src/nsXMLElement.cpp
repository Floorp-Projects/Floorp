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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsXMLElement.h"

nsresult
NS_NewXMLElement(nsIContent** aInstancePtrResult, already_AddRefed<nsINodeInfo> aNodeInfo)
{
  nsXMLElement* it = new nsXMLElement(aNodeInfo);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aInstancePtrResult = it);

  return NS_OK;
}

DOMCI_NODE_DATA(Element, nsXMLElement)

// QueryInterface implementation for nsXMLElement
NS_INTERFACE_TABLE_HEAD(nsXMLElement)
  NS_NODE_OFFSET_AND_INTERFACE_TABLE_BEGIN(nsXMLElement)
    NS_INTERFACE_TABLE_ENTRY(nsXMLElement, nsIDOMNode)
    NS_INTERFACE_TABLE_ENTRY(nsXMLElement, nsIDOMElement)
  NS_OFFSET_AND_INTERFACE_TABLE_END
  NS_ELEMENT_INTERFACE_TABLE_TO_MAP_SEGUE
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Element)
NS_ELEMENT_INTERFACE_MAP_END

NS_IMPL_ADDREF_INHERITED(nsXMLElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsXMLElement, nsGenericElement)

NS_IMPL_ELEMENT_CLONE(nsXMLElement)

nsresult
nsXMLElement::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                        PRBool aNotify)
{
  PRBool isId = PR_FALSE;
  if (aAttribute == GetIDAttributeName() &&
      aNameSpaceID == kNameSpaceID_None) {
    // Have to do this before clearing flag. See RemoveFromIdTable
    RemoveFromIdTable();
    isId = PR_TRUE;
  }
  
  nsresult rv = nsGenericElement::UnsetAttr(aNameSpaceID, aAttribute, aNotify);
  
  if (isId) {
    UnsetFlags(NODE_HAS_ID);
  }
  
  return rv;
}

nsIAtom *
nsXMLElement::GetIDAttributeName() const
{
  return mNodeInfo->GetIDAttributeAtom();
}

nsIAtom*
nsXMLElement::DoGetID() const
{
  NS_ASSERTION(HasFlag(NODE_HAS_ID), "Unexpected call");

  nsIAtom* IDName = GetIDAttributeName();
  if (IDName) {
    const nsAttrValue* attrVal = mAttrsAndChildren.GetAttr(IDName);
    if (attrVal) {
      if (attrVal->Type() == nsAttrValue::eAtom) {
        return attrVal->GetAtomValue();
      }
      if (attrVal->IsEmptyString()) {
        return nsnull;
      }
      // Check if the ID has been stored as a string.
      // This would occur if the ID attribute name changed after 
      // the ID was parsed. 
      if (attrVal->Type() == nsAttrValue::eString) {
        nsAutoString idVal(attrVal->GetStringValue());

        // Create an atom from the value and set it into the attribute list. 
        const_cast<nsAttrValue*>(attrVal)->ParseAtom(idVal);
        return attrVal->GetAtomValue();
      }
    }
  }
  return nsnull;
}

PRBool
nsXMLElement::ParseAttribute(PRInt32 aNamespaceID,
                             nsIAtom* aAttribute,
                             const nsAString& aValue,
                             nsAttrValue& aResult)
{
  if (aAttribute == GetIDAttributeName() &&
      aNamespaceID == kNameSpaceID_None) {
    // Store id as an atom.  id="" means that the element has no id,
    // not that it has an emptystring as the id.
    RemoveFromIdTable();
    if (aValue.IsEmpty()) {
      UnsetFlags(NODE_HAS_ID);
      return PR_FALSE;
    }
    aResult.ParseAtom(aValue);
    SetFlags(NODE_HAS_ID);
    AddToIdTable(aResult.GetAtomValue());
    return PR_TRUE;
  }

  return PR_FALSE;
}

nsresult
nsXMLElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                         nsIContent* aBindingParent,
                         PRBool aCompileEventHandlers)
{
  nsresult rv = nsGenericElement::BindToTree(aDocument, aParent,
                                             aBindingParent,
                                             aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDocument && HasFlag(NODE_HAS_ID) && !GetBindingParent()) {
    aDocument->AddToIdTable(this, DoGetID());
  }

  return NS_OK;
}

void
nsXMLElement::UnbindFromTree(PRBool aDeep, PRBool aNullParent)
{
  RemoveFromIdTable();

  return nsGenericElement::UnbindFromTree(aDeep, aNullParent);
}
