/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLProgressElement.h"
#include "mozilla/dom/HTMLProgressElementBinding.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Progress)
DOMCI_NODE_DATA(HTMLProgressElement, mozilla::dom::HTMLProgressElement)

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
  NS_HTML_CONTENT_INTERFACE_TABLE1(HTMLProgressElement,
                                   nsIDOMHTMLProgressElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(HTMLProgressElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLProgressElement)

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
  const nsAttrValue* attrValue = mAttrsAndChildren.GetAttr(nsGkAtoms::value);
  if (!attrValue || attrValue->Type() != nsAttrValue::eDoubleValue ||
      attrValue->GetDoubleValue() < 0.0) {
    *aValue = kDefaultValue;
    return NS_OK;
  }

  *aValue = attrValue->GetDoubleValue();

  double max;
  GetMax(&max);

  *aValue = std::min(*aValue, max);

  return NS_OK;
}

NS_IMETHODIMP
HTMLProgressElement::SetValue(double aValue)
{
  return SetDoubleAttr(nsGkAtoms::value, aValue);
}

NS_IMETHODIMP
HTMLProgressElement::GetMax(double* aValue)
{
  const nsAttrValue* attrMax = mAttrsAndChildren.GetAttr(nsGkAtoms::max);
  if (attrMax && attrMax->Type() == nsAttrValue::eDoubleValue &&
      attrMax->GetDoubleValue() > 0.0) {
    *aValue = attrMax->GetDoubleValue();
  } else {
    *aValue = kDefaultMax;
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLProgressElement::SetMax(double aValue)
{
  return SetDoubleAttr(nsGkAtoms::max, aValue);
}

NS_IMETHODIMP
HTMLProgressElement::GetPosition(double* aPosition)
{
  if (IsIndeterminate()) {
    *aPosition = kIndeterminatePosition;
    return NS_OK;
  }

  double value;
  double max;
  GetValue(&value);
  GetMax(&max);

  *aPosition = value / max;

  return NS_OK;
}

bool
HTMLProgressElement::IsIndeterminate() const
{
  const nsAttrValue* attrValue = mAttrsAndChildren.GetAttr(nsGkAtoms::value);
  return !attrValue || attrValue->Type() != nsAttrValue::eDoubleValue;
}

JSObject*
HTMLProgressElement::WrapNode(JSContext* aCx, JSObject* aScope,
                              bool* aTriedToWrap)
{
  return HTMLProgressElementBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

} // namespace dom
} // namespace mozilla
