/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLProgressElement.h"
#include "mozilla/dom/HTMLProgressElementBinding.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Progress)

namespace mozilla {
namespace dom {

const double HTMLProgressElement::kIndeterminatePosition = -1.0;
const double HTMLProgressElement::kDefaultValue          =  0.0;
const double HTMLProgressElement::kDefaultMax            =  1.0;


HTMLProgressElement::HTMLProgressElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
  SetIsDOMBinding();

  // We start out indeterminate
  AddStatesSilently(NS_EVENT_STATE_INDETERMINATE);
}

HTMLProgressElement::~HTMLProgressElement()
{
}

NS_IMPL_ADDREF_INHERITED(HTMLProgressElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLProgressElement, Element)


NS_INTERFACE_TABLE_HEAD(HTMLProgressElement)
  NS_HTML_CONTENT_INTERFACES(nsGenericHTMLElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(HTMLProgressElement,
                                   nsIDOMHTMLProgressElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(HTMLProgressElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_MAP_END

NS_IMPL_ELEMENT_CLONE(HTMLProgressElement)


nsEventStates
HTMLProgressElement::IntrinsicState() const
{
  nsEventStates state = nsGenericHTMLElement::IntrinsicState();

  if (IsIndeterminate()) {
    state |= NS_EVENT_STATE_INDETERMINATE;
  }

  return state;
}

bool
HTMLProgressElement::ParseAttribute(int32_t aNamespaceID, nsIAtom* aAttribute,
                                    const nsAString& aValue, nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::value || aAttribute == nsGkAtoms::max) {
      return aResult.ParseDoubleValue(aValue);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute,
                                              aValue, aResult);
}

NS_IMETHODIMP
HTMLProgressElement::GetValue(double* aValue)
{
  *aValue = Value();
  return NS_OK;
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

NS_IMETHODIMP
HTMLProgressElement::SetValue(double aValue)
{
  ErrorResult rv;
  SetValue(aValue, rv);
  return rv.ErrorCode();
}

NS_IMETHODIMP
HTMLProgressElement::GetMax(double* aValue)
{
  *aValue = Max();
  return NS_OK;
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

NS_IMETHODIMP
HTMLProgressElement::SetMax(double aValue)
{
  ErrorResult rv;
  SetMax(aValue, rv);
  return rv.ErrorCode();
}

NS_IMETHODIMP
HTMLProgressElement::GetPosition(double* aPosition)
{
  *aPosition = Position();
  return NS_OK;
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
HTMLProgressElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return HTMLProgressElementBinding::Wrap(aCx, aScope, this);
}

} // namespace dom
} // namespace mozilla
