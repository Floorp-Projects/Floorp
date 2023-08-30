/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLProgressElement.h"
#include "mozilla/dom/HTMLProgressElementBinding.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Progress)

namespace mozilla::dom {

HTMLProgressElement::HTMLProgressElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : nsGenericHTMLElement(std::move(aNodeInfo)) {
  // We start out indeterminate
  AddStatesSilently(ElementState::INDETERMINATE);
}

HTMLProgressElement::~HTMLProgressElement() = default;

NS_IMPL_ELEMENT_CLONE(HTMLProgressElement)

bool HTMLProgressElement::ParseAttribute(int32_t aNamespaceID,
                                         nsAtom* aAttribute,
                                         const nsAString& aValue,
                                         nsIPrincipal* aMaybeScriptedPrincipal,
                                         nsAttrValue& aResult) {
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::value || aAttribute == nsGkAtoms::max) {
      return aResult.ParseDoubleValue(aValue);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

void HTMLProgressElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                       const nsAttrValue* aValue,
                                       const nsAttrValue* aOldValue,
                                       nsIPrincipal* aSubjectPrincipal,
                                       bool aNotify) {
  if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::value) {
    const bool indeterminate =
        !aValue || aValue->Type() != nsAttrValue::eDoubleValue;
    SetStates(ElementState::INDETERMINATE, indeterminate, aNotify);
  }
  return nsGenericHTMLElement::AfterSetAttr(
      aNameSpaceID, aName, aValue, aOldValue, aSubjectPrincipal, aNotify);
}

double HTMLProgressElement::Value() const {
  const nsAttrValue* attrValue = mAttrs.GetAttr(nsGkAtoms::value);
  if (!attrValue || attrValue->Type() != nsAttrValue::eDoubleValue ||
      attrValue->GetDoubleValue() < 0.0) {
    return 0.0;
  }

  return std::min(attrValue->GetDoubleValue(), Max());
}

double HTMLProgressElement::Max() const {
  const nsAttrValue* attrMax = mAttrs.GetAttr(nsGkAtoms::max);
  if (!attrMax || attrMax->Type() != nsAttrValue::eDoubleValue ||
      attrMax->GetDoubleValue() <= 0.0) {
    return 1.0;
  }

  return attrMax->GetDoubleValue();
}

double HTMLProgressElement::Position() const {
  if (State().HasState(ElementState::INDETERMINATE)) {
    return -1.0;
  }

  return Value() / Max();
}

JSObject* HTMLProgressElement::WrapNode(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return HTMLProgressElement_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
