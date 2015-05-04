/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLMeterElement.h"
#include "mozilla/EventStates.h"
#include "mozilla/dom/HTMLMeterElementBinding.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Meter)

namespace mozilla {
namespace dom {

const double HTMLMeterElement::kDefaultValue =  0.0;
const double HTMLMeterElement::kDefaultMin   =  0.0;
const double HTMLMeterElement::kDefaultMax   =  1.0;


HTMLMeterElement::HTMLMeterElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

HTMLMeterElement::~HTMLMeterElement()
{
}

NS_IMPL_ELEMENT_CLONE(HTMLMeterElement)

EventStates
HTMLMeterElement::IntrinsicState() const
{
  EventStates state = nsGenericHTMLElement::IntrinsicState();

  state |= GetOptimumState();

  return state;
}

bool
HTMLMeterElement::ParseAttribute(int32_t aNamespaceID, nsIAtom* aAttribute,
                                 const nsAString& aValue, nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::value || aAttribute == nsGkAtoms::max ||
        aAttribute == nsGkAtoms::min   || aAttribute == nsGkAtoms::low ||
        aAttribute == nsGkAtoms::high  || aAttribute == nsGkAtoms::optimum) {
      return aResult.ParseDoubleValue(aValue);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute,
                                                  aValue, aResult);
}

/*
 * Value getters :
 * const getters used by XPCOM methods and by IntrinsicState
 */

double
HTMLMeterElement::Min() const
{
  /**
   * If the attribute min is defined, the minimum is this value.
   * Otherwise, the minimum is the default value.
   */
  const nsAttrValue* attrMin = mAttrsAndChildren.GetAttr(nsGkAtoms::min);
  if (attrMin && attrMin->Type() == nsAttrValue::eDoubleValue) {
    return attrMin->GetDoubleValue();
  }
  return kDefaultMin;
}

double
HTMLMeterElement::Max() const
{
  /**
   * If the attribute max is defined, the maximum is this value.
   * Otherwise, the maximum is the default value.
   * If the maximum value is less than the minimum value,
   * the maximum value is the same as the minimum value.
   */
  double max;

  const nsAttrValue* attrMax = mAttrsAndChildren.GetAttr(nsGkAtoms::max);
  if (attrMax && attrMax->Type() == nsAttrValue::eDoubleValue) {
    max = attrMax->GetDoubleValue();
  } else {
    max = kDefaultMax;
  }

  return std::max(max, Min());
}

double
HTMLMeterElement::Value() const
{
  /**
   * If the attribute value is defined, the actual value is this value.
   * Otherwise, the actual value is the default value.
   * If the actual value is less than the minimum value,
   * the actual value is the same as the minimum value.
   * If the actual value is greater than the maximum value,
   * the actual value is the same as the maximum value.
   */
  double value;

  const nsAttrValue* attrValue = mAttrsAndChildren.GetAttr(nsGkAtoms::value);
  if (attrValue && attrValue->Type() == nsAttrValue::eDoubleValue) {
    value = attrValue->GetDoubleValue();
  } else {
    value = kDefaultValue;
  }

  double min = Min();

  if (value <= min) {
    return min;
  }

  return std::min(value, Max());
}

double
HTMLMeterElement::Low() const
{
  /**
   * If the low value is defined, the low value is this value.
   * Otherwise, the low value is the minimum value.
   * If the low value is less than the minimum value,
   * the low value is the same as the minimum value.
   * If the low value is greater than the maximum value,
   * the low value is the same as the maximum value.
   */

  double min = Min();

  const nsAttrValue* attrLow = mAttrsAndChildren.GetAttr(nsGkAtoms::low);
  if (!attrLow || attrLow->Type() != nsAttrValue::eDoubleValue) {
    return min;
  }

  double low = attrLow->GetDoubleValue();

  if (low <= min) {
    return min;
  }

  return std::min(low, Max());
}

double
HTMLMeterElement::High() const
{
  /**
   * If the high value is defined, the high value is this value.
   * Otherwise, the high value is the maximum value.
   * If the high value is less than the low value,
   * the high value is the same as the low value.
   * If the high value is greater than the maximum value,
   * the high value is the same as the maximum value.
   */

  double max = Max();

  const nsAttrValue* attrHigh = mAttrsAndChildren.GetAttr(nsGkAtoms::high);
  if (!attrHigh || attrHigh->Type() != nsAttrValue::eDoubleValue) {
    return max;
  }

  double high = attrHigh->GetDoubleValue();

  if (high >= max) {
    return max;
  }

  return std::max(high, Low());
}

double
HTMLMeterElement::Optimum() const
{
  /**
   * If the optimum value is defined, the optimum value is this value.
   * Otherwise, the optimum value is the midpoint between
   * the minimum value and the maximum value :
   * min + (max - min)/2 = (min + max)/2
   * If the optimum value is less than the minimum value,
   * the optimum value is the same as the minimum value.
   * If the optimum value is greater than the maximum value,
   * the optimum value is the same as the maximum value.
   */

  double max = Max();

  double min = Min();

  const nsAttrValue* attrOptimum =
              mAttrsAndChildren.GetAttr(nsGkAtoms::optimum);
  if (!attrOptimum || attrOptimum->Type() != nsAttrValue::eDoubleValue) {
    return (min + max) / 2.0;
  }

  double optimum = attrOptimum->GetDoubleValue();

  if (optimum <= min) {
    return min;
  }

  return std::min(optimum, max);
}

EventStates
HTMLMeterElement::GetOptimumState() const
{
  /*
   * If the optimum value is in [minimum, low[,
   *     return if the value is in optimal, suboptimal or sub-suboptimal region
   *
   * If the optimum value is in [low, high],
   *     return if the value is in optimal or suboptimal region
   *
   * If the optimum value is in ]high, maximum],
   *     return if the value is in optimal, suboptimal or sub-suboptimal region
   */
  double value = Value();
  double low = Low();
  double high = High();
  double optimum = Optimum();

  if (optimum < low) {
    if (value < low) {
      return NS_EVENT_STATE_OPTIMUM;
    }
    if (value <= high) {
      return NS_EVENT_STATE_SUB_OPTIMUM;
    }
    return NS_EVENT_STATE_SUB_SUB_OPTIMUM;
  }
  if (optimum > high) {
    if (value > high) {
      return NS_EVENT_STATE_OPTIMUM;
    }
    if (value >= low) {
      return NS_EVENT_STATE_SUB_OPTIMUM;
    }
    return NS_EVENT_STATE_SUB_SUB_OPTIMUM;
  }
  // optimum in [low, high]
  if (value >= low && value <= high) {
    return NS_EVENT_STATE_OPTIMUM;
  }
  return NS_EVENT_STATE_SUB_OPTIMUM;
}

JSObject*
HTMLMeterElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLMeterElementBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
