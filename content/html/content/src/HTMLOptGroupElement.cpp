/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLOptGroupElement.h"
#include "mozilla/dom/HTMLOptGroupElementBinding.h"
#include "mozilla/dom/HTMLSelectElement.h" // SafeOptionListMutation
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsIFrame.h"
#include "nsIFormControlFrame.h"
#include "nsEventStates.h"

#include "nsEventDispatcher.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(OptGroup)

namespace mozilla {
namespace dom {

/**
 * The implementation of &lt;optgroup&gt;
 */



HTMLOptGroupElement::HTMLOptGroupElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
  // We start off enabled
  AddStatesSilently(NS_EVENT_STATE_ENABLED);
}

HTMLOptGroupElement::~HTMLOptGroupElement()
{
}


NS_IMPL_ISUPPORTS_INHERITED1(HTMLOptGroupElement, nsGenericHTMLElement,
                             nsIDOMHTMLOptGroupElement)

NS_IMPL_ELEMENT_CLONE(HTMLOptGroupElement)


NS_IMPL_BOOL_ATTR(HTMLOptGroupElement, Disabled, disabled)
NS_IMPL_STRING_ATTR(HTMLOptGroupElement, Label, label)


nsresult
HTMLOptGroupElement::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  aVisitor.mCanHandle = false;
  // Do not process any DOM events if the element is disabled
  // XXXsmaug This is not the right thing to do. But what is?
  if (HasAttr(kNameSpaceID_None, nsGkAtoms::disabled)) {
    return NS_OK;
  }

  nsIFrame* frame = GetPrimaryFrame();
  if (frame) {
    const nsStyleUserInterface* uiStyle = frame->StyleUserInterface();
    if (uiStyle->mUserInput == NS_STYLE_USER_INPUT_NONE ||
        uiStyle->mUserInput == NS_STYLE_USER_INPUT_DISABLED) {
      return NS_OK;
    }
  }

  return nsGenericHTMLElement::PreHandleEvent(aVisitor);
}

nsIContent*
HTMLOptGroupElement::GetSelect()
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
HTMLOptGroupElement::InsertChildAt(nsIContent* aKid,
                                   uint32_t aIndex,
                                   bool aNotify)
{
  SafeOptionListMutation safeMutation(GetSelect(), this, aKid, aIndex, aNotify);
  nsresult rv = nsGenericHTMLElement::InsertChildAt(aKid, aIndex, aNotify);
  if (NS_FAILED(rv)) {
    safeMutation.MutationFailed();
  }
  return rv;
}

void
HTMLOptGroupElement::RemoveChildAt(uint32_t aIndex, bool aNotify)
{
  SafeOptionListMutation safeMutation(GetSelect(), this, nullptr, aIndex,
                                      aNotify);
  nsGenericHTMLElement::RemoveChildAt(aIndex, aNotify);
}

nsresult
HTMLOptGroupElement::AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
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
HTMLOptGroupElement::IntrinsicState() const
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

JSObject*
HTMLOptGroupElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return HTMLOptGroupElementBinding::Wrap(aCx, aScope, this);
}

} // namespace dom
} // namespace mozilla
