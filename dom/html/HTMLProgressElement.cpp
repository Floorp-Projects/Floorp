/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EventStates.h"
#include "mozilla/dom/HTMLProgressElement.h"
#include "mozilla/dom/HTMLProgressElementBinding.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Progress)

namespace mozilla {
namespace dom {

const double HTMLProgressElement::kIndeterminatePosition = -1.0;
const double HTMLProgressElement::kDefaultValue          =  0.0;
const double HTMLProgressElement::kDefaultMax            =  1.0;


HTMLProgressElement::HTMLProgressElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
  // We start out indeterminate
  AddStatesSilently(NS_EVENT_STATE_INDETERMINATE);
}

HTMLProgressElement::~HTMLProgressElement()
{
}

NS_IMPL_ELEMENT_CLONE(HTMLProgressElement)


EventStates
HTMLProgressElement::IntrinsicState() const
{
  EventStates state = nsGenericHTMLElement::IntrinsicState();

  if (IsIndeterminate()) {
    state |= NS_EVENT_STATE_INDETERMINATE;
  }

  return state;
}

bool
HTMLProgressElement::ParseAttribute(int32_t aNamespaceID,
                                    nsAtom* aAttribute,
                                    const nsAString& aValue,
                                    nsIPrincipal* aMaybeScriptedPrincipal,
                                    nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::value || aAttribute == nsGkAtoms::max) {
      return aResult.ParseDoubleValue(aValue);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute,
                                              aValue,
                                              aMaybeScriptedPrincipal,
                                              aResult);
}

double
HTMLProgressElement::Value() const
{
  const nsAttrValue* attrValue = mAttrsAndChildren.GetAttr(nsGkAtoms::value);
  if (!attrValue || attrValue->Type() != nsAttrValue::eDoubleValue ||
      attrValue->GetDoubleValue() < 0.0) {
    return kDefaultValue;
  }

  return std::min(attrValue->GetDoubleValue(), Max());
}

double
HTMLProgressElement::Max() const
{
  const nsAttrValue* attrMax = mAttrsAndChildren.GetAttr(nsGkAtoms::max);
  if (!attrMax || attrMax->Type() != nsAttrValue::eDoubleValue ||
      attrMax->GetDoubleValue() <= 0.0) {
    return kDefaultMax;
  }

  return attrMax->GetDoubleValue();
}

double
HTMLProgressElement::Position() const
{
  if (IsIndeterminate()) {
    return kIndeterminatePosition;
  }

  return Value() / Max();
}

bool
HTMLProgressElement::IsIndeterminate() const
{
  const nsAttrValue* attrValue = mAttrsAndChildren.GetAttr(nsGkAtoms::value);
  return !attrValue || attrValue->Type() != nsAttrValue::eDoubleValue;
}

JSObject*
HTMLProgressElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLProgressElement_Binding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
