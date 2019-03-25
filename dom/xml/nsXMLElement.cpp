/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXMLElement.h"
#include "mozilla/dom/ElementBinding.h"
#include "mozilla/dom/ElementInlines.h"
#include "nsContentUtils.h"  // nsAutoScriptBlocker

using namespace mozilla;
using namespace mozilla::dom;

nsresult NS_NewXMLElement(
    Element** aInstancePtrResult,
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo) {
  RefPtr<nsXMLElement> it = new nsXMLElement(std::move(aNodeInfo));
  it.forget(aInstancePtrResult);
  return NS_OK;
}

JSObject* nsXMLElement::WrapNode(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) {
  return Element_Binding::Wrap(aCx, this, aGivenProto);
}

void nsXMLElement::UnbindFromTree(bool aDeep, bool aNullParent) {
  nsAtom* property;
  switch (GetPseudoElementType()) {
    case PseudoStyleType::marker:
      property = nsGkAtoms::markerPseudoProperty;
      break;
    case PseudoStyleType::before:
      property = nsGkAtoms::beforePseudoProperty;
      break;
    case PseudoStyleType::after:
      property = nsGkAtoms::afterPseudoProperty;
      break;
    default:
      property = nullptr;
  }
  if (property) {
    MOZ_ASSERT(GetParent());
    MOZ_ASSERT(GetParent()->IsElement());
    GetParent()->DeleteProperty(property);
  }
  Element::UnbindFromTree(aDeep, aNullParent);
}

NS_IMPL_ELEMENT_CLONE(nsXMLElement)
