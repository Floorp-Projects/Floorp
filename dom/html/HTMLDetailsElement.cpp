/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLDetailsElement.h"

#include "mozilla/dom/HTMLDetailsElementBinding.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Details)

namespace mozilla {
namespace dom {

HTMLDetailsElement::~HTMLDetailsElement()
{
}

NS_IMPL_ELEMENT_CLONE(HTMLDetailsElement)

nsIContent*
HTMLDetailsElement::GetFirstSummary() const
{
  // XXX: Bug 1245032: Might want to cache the first summary element.
  for (nsIContent* child = nsINode::GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (child->IsHTMLElement(nsGkAtoms::summary)) {
      return child;
    }
  }
  return nullptr;
}

nsChangeHint
HTMLDetailsElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                           int32_t aModType) const
{
  nsChangeHint hint =
    nsGenericHTMLElement::GetAttributeChangeHint(aAttribute, aModType);
  if (aAttribute == nsGkAtoms::open) {
    hint |= nsChangeHint_ReconstructFrame;
  }
  return hint;
}

nsresult
HTMLDetailsElement::BeforeSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                  nsAttrValueOrString* aValue, bool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::open) {
    bool setOpen = aValue != nullptr;
    if (Open() != setOpen) {
      if (mToggleEventDispatcher) {
        mToggleEventDispatcher->Cancel();
      }
      // According to the html spec, a 'toggle' event is a simple event which
      // does not bubble.
      mToggleEventDispatcher =
        new AsyncEventDispatcher(this, NS_LITERAL_STRING("toggle"), false);
      mToggleEventDispatcher->PostDOMEvent();
    }
  }

  return nsGenericHTMLElement::BeforeSetAttr(aNameSpaceID, aName, aValue,
                                             aNotify);
}

void
HTMLDetailsElement::AsyncEventRunning(AsyncEventDispatcher* aEvent)
{
  if (mToggleEventDispatcher == aEvent) {
    mToggleEventDispatcher = nullptr;
  }
}

JSObject*
HTMLDetailsElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLDetailsElementBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
