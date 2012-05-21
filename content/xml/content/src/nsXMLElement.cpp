/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXMLElement.h"
#include "nsContentUtils.h" // nsAutoScriptBlocker

nsresult
NS_NewXMLElement(nsIContent** aInstancePtrResult, already_AddRefed<nsINodeInfo> aNodeInfo)
{
  nsXMLElement* it = new nsXMLElement(aNodeInfo);
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
                        bool aNotify)
{
  nsAutoScriptBlocker scriptBlocker;
  bool isId = false;
  if (aAttribute == GetIDAttributeName() &&
      aNameSpaceID == kNameSpaceID_None) {
    // Have to do this before clearing flag. See RemoveFromIdTable
    RemoveFromIdTable();
    isId = true;
  }

  nsMutationGuard guard;

  nsresult rv = nsGenericElement::UnsetAttr(aNameSpaceID, aAttribute, aNotify);

  if (isId &&
      (!guard.Mutated(0) ||
       !mNodeInfo->GetIDAttributeAtom() ||
       !HasAttr(kNameSpaceID_None, GetIDAttributeName()))) {
    ClearHasID();
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
  NS_ASSERTION(HasID(), "Unexpected call");

  const nsAttrValue* attrVal = mAttrsAndChildren.GetAttr(GetIDAttributeName());
  return attrVal ? attrVal->GetAtomValue() : nsnull;
}

void
nsXMLElement::NodeInfoChanged(nsINodeInfo* aOldNodeInfo)
{
  NS_ASSERTION(!IsInDoc() ||
               aOldNodeInfo->GetDocument() == mNodeInfo->GetDocument(),
               "Can only change document if we're not inside one");
  nsIDocument* doc = GetCurrentDoc();

  if (HasID() && doc) {
    const nsAttrValue* attrVal =
      mAttrsAndChildren.GetAttr(aOldNodeInfo->GetIDAttributeAtom());
    if (attrVal) {
      doc->RemoveFromIdTable(this, attrVal->GetAtomValue());
    }
  }
  
  ClearHasID();

  nsIAtom* IDName = GetIDAttributeName();
  if (IDName) {
    const nsAttrValue* attrVal = mAttrsAndChildren.GetAttr(IDName);
    if (attrVal) {
      SetHasID();
      if (attrVal->Type() == nsAttrValue::eString) {
        nsString idVal(attrVal->GetStringValue());

        // Create an atom from the value and set it into the attribute list. 
        const_cast<nsAttrValue*>(attrVal)->ParseAtom(idVal);
      }
      NS_ASSERTION(attrVal->Type() == nsAttrValue::eAtom,
                   "Should be atom by now");
      if (doc) {
        doc->AddToIdTable(this, attrVal->GetAtomValue());
      }
    }
  }
}

bool
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
      ClearHasID();
      return false;
    }
    aResult.ParseAtom(aValue);
    SetHasID();
    AddToIdTable(aResult.GetAtomValue());
    return true;
  }

  return false;
}

nsresult
nsXMLElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                         nsIContent* aBindingParent,
                         bool aCompileEventHandlers)
{
  nsresult rv = nsGenericElement::BindToTree(aDocument, aParent,
                                             aBindingParent,
                                             aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDocument && HasID() && !GetBindingParent()) {
    aDocument->AddToIdTable(this, DoGetID());
  }

  return NS_OK;
}

void
nsXMLElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  RemoveFromIdTable();

  return nsGenericElement::UnbindFromTree(aDeep, aNullParent);
}
