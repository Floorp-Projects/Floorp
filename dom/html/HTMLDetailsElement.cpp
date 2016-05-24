/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLDetailsElement.h"

#include "mozilla/dom/HTMLDetailsElementBinding.h"
#include "mozilla/dom/HTMLUnknownElement.h"
#include "mozilla/Preferences.h"

// Expand NS_IMPL_NS_NEW_HTML_ELEMENT(Details) to add pref check.
nsGenericHTMLElement*
NS_NewHTMLDetailsElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                         mozilla::dom::FromParser aFromParser)
{
  if (!mozilla::dom::HTMLDetailsElement::IsDetailsEnabled()) {
    return new mozilla::dom::HTMLUnknownElement(aNodeInfo);
  }

  return new mozilla::dom::HTMLDetailsElement(aNodeInfo);
}

namespace mozilla {
namespace dom {

/* static */ bool
HTMLDetailsElement::IsDetailsEnabled()
{
  static bool isDetailsEnabled = false;
  static bool added = false;

  if (!added) {
    Preferences::AddBoolVarCache(&isDetailsEnabled,
                                 "dom.details_element.enabled");
    added = true;
  }

  return isDetailsEnabled;
}

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
      mToggleEventDispatcher = new ToggleEventDispatcher(this);
      mToggleEventDispatcher->PostDOMEvent();
    }
  }

  return nsGenericHTMLElement::BeforeSetAttr(aNameSpaceID, aName, aValue,
                                             aNotify);
}

JSObject*
HTMLDetailsElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLDetailsElementBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
