/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Element.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsCOMPtr.h"
#include "nsChangeHint.h"
#include "nsIContent.h"
#include "nsPresContext.h"
#include "mozilla/dom/MutationEventBinding.h"
#include "mozilla/dom/XULTextElement.h"
#include "mozilla/dom/XULTextElementBinding.h"

namespace mozilla::dom {

nsChangeHint XULTextElement::GetAttributeChangeHint(const nsAtom* aAttribute,
                                                    int32_t aModType) const {
  const bool reframe = [&] {
    if (aAttribute == nsGkAtoms::value) {
      // If we have an accesskey we need to recompute our -moz-label-content.
      // Otherwise this is handled by either the attribute text node, or
      // nsTextBoxFrame for crop="center".
      return aModType == MutationEvent_Binding::ADDITION ||
             aModType == MutationEvent_Binding::REMOVAL ||
             HasAttr(nsGkAtoms::accesskey);
    }
    if (aAttribute == nsGkAtoms::crop || aAttribute == nsGkAtoms::accesskey) {
      // value attr + crop="center" still uses nsTextBoxFrame. accesskey
      // requires reframing as per the above comment.
      return HasAttr(nsGkAtoms::value);
    }
    return false;
  }();
  if (reframe) {
    return nsChangeHint_ReconstructFrame;
  }
  return nsXULElement::GetAttributeChangeHint(aAttribute, aModType);
}

JSObject* XULTextElement::WrapNode(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return XULTextElement_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
