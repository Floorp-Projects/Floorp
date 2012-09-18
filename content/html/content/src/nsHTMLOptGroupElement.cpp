/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsHTMLOptGroupElement.h"
#include "nsIDOMEventTarget.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsIFrame.h"
#include "nsIFormControlFrame.h"
#include "nsEventStates.h"

#include "nsEventDispatcher.h"
#include "nsHTMLSelectElement.h"

/**
 * The implementation of &lt;optgroup&gt;
 */

NS_IMPL_NS_NEW_HTML_ELEMENT(OptGroup)


nsHTMLOptGroupElement::nsHTMLOptGroupElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
  // We start off enabled
  AddStatesSilently(NS_EVENT_STATE_ENABLED);
}

nsHTMLOptGroupElement::~nsHTMLOptGroupElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLOptGroupElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLOptGroupElement, nsGenericElement)


DOMCI_NODE_DATA(HTMLOptGroupElement, nsHTMLOptGroupElement)

// QueryInterface implementation for nsHTMLOptGroupElement
NS_INTERFACE_TABLE_HEAD(nsHTMLOptGroupElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(nsHTMLOptGroupElement,
                                   nsIDOMHTMLOptGroupElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLOptGroupElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLOptGroupElement)


NS_IMPL_ELEMENT_CLONE(nsHTMLOptGroupElement)


NS_IMPL_BOOL_ATTR(nsHTMLOptGroupElement, Disabled, disabled)
NS_IMPL_STRING_ATTR(nsHTMLOptGroupElement, Label, label)


nsresult
nsHTMLOptGroupElement::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  aVisitor.mCanHandle = false;
  // Do not process any DOM events if the element is disabled
  // XXXsmaug This is not the right thing to do. But what is?
  if (HasAttr(kNameSpaceID_None, nsGkAtoms::disabled)) {
    return NS_OK;
  }

  nsIFrame* frame = GetPrimaryFrame();
  if (frame) {
    const nsStyleUserInterface* uiStyle = frame->GetStyleUserInterface();
    if (uiStyle->mUserInput == NS_STYLE_USER_INPUT_NONE ||
        uiStyle->mUserInput == NS_STYLE_USER_INPUT_DISABLED) {
      return NS_OK;
    }
  }

  return nsGenericHTMLElement::PreHandleEvent(aVisitor);
}

nsIContent*
nsHTMLOptGroupElement::GetSelect()
{
  nsIContent* parent = this;
  while ((parent = parent->GetParent()) && parent->IsHTML()) {
    if (parent->Tag() == nsGkAtoms::select) {
      return parent;
    }
    if (parent->Tag() != nsGkAtoms::optgroup) {
      break;
    }
  }
  
  return nullptr;
}

nsresult
nsHTMLOptGroupElement::InsertChildAt(nsIContent* aKid,
                                     uint32_t aIndex,
                                     bool aNotify)
{
  nsSafeOptionListMutation safeMutation(GetSelect(), this, aKid, aIndex, aNotify);
  nsresult rv = nsGenericHTMLElement::InsertChildAt(aKid, aIndex, aNotify);
  if (NS_FAILED(rv)) {
    safeMutation.MutationFailed();
  }
  return rv;
}

void
nsHTMLOptGroupElement::RemoveChildAt(uint32_t aIndex, bool aNotify)
{
  nsSafeOptionListMutation safeMutation(GetSelect(), this, nullptr, aIndex,
                                        aNotify);
  nsGenericHTMLElement::RemoveChildAt(aIndex, aNotify);
}

nsresult
nsHTMLOptGroupElement::AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                    const nsAttrValue* aValue, bool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::disabled) {
    // All our children <option> have their :disabled state depending on our
    // disabled attribute. We should make sure their state is updated.
    for (nsIContent* child = nsINode::GetFirstChild(); child;
         child = child->GetNextSibling()) {
      if (child->IsHTML(nsGkAtoms::option)) {
        // No need to call |IsElement()| because it's an HTML element.
        child->AsElement()->UpdateState(true);
      }
    }
  }

  return nsGenericHTMLElement::AfterSetAttr(aNameSpaceID, aName, aValue,
                                            aNotify);
}

nsEventStates
nsHTMLOptGroupElement::IntrinsicState() const
{
  nsEventStates state = nsGenericHTMLElement::IntrinsicState();

  if (HasAttr(kNameSpaceID_None, nsGkAtoms::disabled)) {
    state |= NS_EVENT_STATE_DISABLED;
    state &= ~NS_EVENT_STATE_ENABLED;
  } else {
    state &= ~NS_EVENT_STATE_DISABLED;
    state |= NS_EVENT_STATE_ENABLED;
  }

  return state;
}
