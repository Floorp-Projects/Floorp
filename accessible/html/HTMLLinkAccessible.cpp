/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLLinkAccessible.h"

#include "CacheConstants.h"
#include "nsCoreUtils.h"
#include "mozilla/a11y/Role.h"
#include "States.h"

#include "nsContentUtils.h"
#include "mozilla/a11y/DocAccessible.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/MutationEventBinding.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// HTMLLinkAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLLinkAccessible::HTMLLinkAccessible(nsIContent* aContent,
                                       DocAccessible* aDoc)
    : HyperTextAccessible(aContent, aDoc) {
  mType = eHTMLLinkType;
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessible

role HTMLLinkAccessible::NativeRole() const { return roles::LINK; }

uint64_t HTMLLinkAccessible::NativeState() const {
  return HyperTextAccessible::NativeState() & ~states::READONLY;
}

uint64_t HTMLLinkAccessible::NativeLinkState() const {
  dom::ElementState state = mContent->AsElement()->State();
  if (state.HasState(dom::ElementState::UNVISITED)) {
    return states::LINKED;
  }

  if (state.HasState(dom::ElementState::VISITED)) {
    return states::LINKED | states::TRAVERSED;
  }

  // This is a either named anchor (a link with also a name attribute) or
  // it doesn't have any attributes. Check if 'click' event handler is
  // registered, otherwise bail out.
  return nsCoreUtils::HasClickListener(mContent) ? states::LINKED : 0;
}

uint64_t HTMLLinkAccessible::NativeInteractiveState() const {
  uint64_t state = HyperTextAccessible::NativeInteractiveState();

  // This is how we indicate it is a named anchor. In other words, this anchor
  // can be selected as a location :) There is no other better state to use to
  // indicate this.
  if (mContent->AsElement()->HasAttr(nsGkAtoms::name)) {
    state |= states::SELECTABLE;
  }

  return state;
}

void HTMLLinkAccessible::Value(nsString& aValue) const {
  aValue.Truncate();

  HyperTextAccessible::Value(aValue);
  if (aValue.IsEmpty()) {
    nsContentUtils::GetLinkLocation(mContent->AsElement(), aValue);
  }
}

bool HTMLLinkAccessible::HasPrimaryAction() const {
  return IsLinked() || HyperTextAccessible::HasPrimaryAction();
  ;
}

void HTMLLinkAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName) {
  aName.Truncate();

  if (!IsLinked()) {
    HyperTextAccessible::ActionNameAt(aIndex, aName);
    return;
  }

  // Action 0 (default action): Jump to link
  if (aIndex == eAction_Jump) aName.AssignLiteral("jump");
}

bool HTMLLinkAccessible::AttributeChangesState(nsAtom* aAttribute) {
  return aAttribute == nsGkAtoms::href ||
         HyperTextAccessible::AttributeChangesState(aAttribute);
}

void HTMLLinkAccessible::DOMAttributeChanged(int32_t aNameSpaceID,
                                             nsAtom* aAttribute,
                                             int32_t aModType,
                                             const nsAttrValue* aOldValue,
                                             uint64_t aOldState) {
  HyperTextAccessible::DOMAttributeChanged(aNameSpaceID, aAttribute, aModType,
                                           aOldValue, aOldState);

  if (aAttribute == nsGkAtoms::href &&
      (aModType == dom::MutationEvent_Binding::ADDITION ||
       aModType == dom::MutationEvent_Binding::REMOVAL)) {
    mDoc->QueueCacheUpdate(this, CacheDomain::Actions);
  }
}

////////////////////////////////////////////////////////////////////////////////
// HyperLinkAccessible

bool HTMLLinkAccessible::IsLink() const {
  // Expose HyperLinkAccessible unconditionally.
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLLinkAccessible

bool HTMLLinkAccessible::IsLinked() const {
  dom::ElementState state = mContent->AsElement()->State();
  return state.HasAtLeastOneOfStates(dom::ElementState::VISITED_OR_UNVISITED);
}
