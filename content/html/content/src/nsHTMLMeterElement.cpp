/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIDOMHTMLMeterElement.h"
#include "nsGenericHTMLElement.h"
#include "nsAttrValue.h"
#include "nsAttrValueInlines.h"
#include "nsEventStateManager.h"
#include "nsAlgorithm.h"
#include <algorithm>

using namespace mozilla::dom;

class nsHTMLMeterElement : public nsGenericHTMLElement,
                           public nsIDOMHTMLMeterElement
{
public:
  nsHTMLMeterElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLMeterElement();

  /* nsISupports */
  NS_DECL_ISUPPORTS_INHERITED

  /* nsIDOMNode */
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  /* nsIDOMElement */
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  /* nsIDOMHTMLElement */
  NS_FORWARD_NSIDOMHTMLELEMENT_TO_GENERIC

  /* nsIDOMHTMLMeterElement */
  NS_DECL_NSIDOMHTMLMETERELEMENT

  virtual nsEventStates IntrinsicState() const;

  nsresult Clone(nsINodeInfo* aNodeInfo, nsINode** aResult) const;

  bool ParseAttribute(int32_t aNamespaceID, nsIAtom* aAttribute,
                      const nsAString& aValue, nsAttrValue& aResult);

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }

private:

  static const double kDefaultValue;
  static const double kDefaultMin;
  static const double kDefaultMax;

  /**
   * Returns the optimum state of the element.
   * NS_EVENT_STATE_OPTIMUM if the actual value is in the optimum region.
   * NS_EVENT_STATE_SUB_OPTIMUM if the actual value is in the sub-optimal region.
   * NS_EVENT_STATE_SUB_SUB_OPTIMUM if the actual value is in the sub-sub-optimal region.
   *
   * @return the optimum state of the element.
   */
  nsEventStates GetOptimumState() const;

  /* @return the minimum value */
  double GetMin() const;

  /* @return the maximum value */
  double GetMax() const;

  /* @return the actual value */
  double GetValue() const;

  /* @return the low value */
  double GetLow() const;

  /* @return the high value */
  double GetHigh() const;

  /* @return the optimum value */
  double GetOptimum() const;
};

const double nsHTMLMeterElement::kDefaultValue =  0.0;
const double nsHTMLMeterElement::kDefaultMin   =  0.0;
const double nsHTMLMeterElement::kDefaultMax   =  1.0;

NS_IMPL_NS_NEW_HTML_ELEMENT(Meter)


nsHTMLMeterElement::nsHTMLMeterElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

nsHTMLMeterElement::~nsHTMLMeterElement()
{
}

NS_IMPL_ADDREF_INHERITED(nsHTMLMeterElement, Element)
NS_IMPL_RELEASE_INHERITED(nsHTMLMeterElement, Element)

DOMCI_NODE_DATA(HTMLMeterElement, nsHTMLMeterElement)

NS_INTERFACE_TABLE_HEAD(nsHTMLMeterElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(nsHTMLMeterElement,
                                   nsIDOMHTMLMeterElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLMeterElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLMeterElement)

NS_IMPL_ELEMENT_CLONE(nsHTMLMeterElement)


nsEventStates
nsHTMLMeterElement::IntrinsicState() const
{
  nsEventStates state = nsGenericHTMLElement::IntrinsicState();

  state |= GetOptimumState();

  return state;
}

bool
nsHTMLMeterElement::ParseAttribute(int32_t aNamespaceID, nsIAtom* aAttribute,
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
nsHTMLMeterElement::GetMin() const
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
nsHTMLMeterElement::GetMax() const
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

  return std::max(max, GetMin());
}

double
nsHTMLMeterElement::GetValue() const
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

  double min = GetMin();

  if (value <= min) {
    return min;
  }

  return std::min(value, GetMax());
}

double
nsHTMLMeterElement::GetLow() const
{
  /**
   * If the low value is defined, the low value is this value.
   * Otherwise, the low value is the minimum value.
   * If the low value is less than the minimum value,
   * the low value is the same as the minimum value.
   * If the low value is greater than the maximum value,
   * the low value is the same as the maximum value.
   */

  double min = GetMin();

  const nsAttrValue* attrLow = mAttrsAndChildren.GetAttr(nsGkAtoms::low);
  if (!attrLow || attrLow->Type() != nsAttrValue::eDoubleValue) {
    return min;
  }

  double low = attrLow->GetDoubleValue();

  if (low <= min) {
    return min;
  }

  return std::min(low, GetMax());
}

double
nsHTMLMeterElement::GetHigh() const
{
  /**
   * If the high value is defined, the high value is this value.
   * Otherwise, the high value is the maximum value.
   * If the high value is less than the low value,
   * the high value is the same as the low value.
   * If the high value is greater than the maximum value,
   * the high value is the same as the maximum value.
   */

  double max = GetMax();

  const nsAttrValue* attrHigh = mAttrsAndChildren.GetAttr(nsGkAtoms::high);
  if (!attrHigh || attrHigh->Type() != nsAttrValue::eDoubleValue) {
    return max;
  }

  double high = attrHigh->GetDoubleValue();

  if (high >= max) {
    return max;
  }

  return std::max(high, GetLow());
}

double
nsHTMLMeterElement::GetOptimum() const
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

  double max = GetMax();

  double min = GetMin();

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

/*
 * XPCOM methods
 */

NS_IMETHODIMP
nsHTMLMeterElement::GetMin(double* aValue)
{
  *aValue = GetMin();
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMeterElement::SetMin(double aValue)
{
  return SetDoubleAttr(nsGkAtoms::min, aValue);
}

NS_IMETHODIMP
nsHTMLMeterElement::GetMax(double* aValue)
{
  *aValue = GetMax();
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMeterElement::SetMax(double aValue)
{
  return SetDoubleAttr(nsGkAtoms::max, aValue);
}

NS_IMETHODIMP
nsHTMLMeterElement::GetValue(double* aValue)
{
  *aValue = GetValue();
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMeterElement::SetValue(double aValue)
{
  return SetDoubleAttr(nsGkAtoms::value, aValue);
}

NS_IMETHODIMP
nsHTMLMeterElement::GetLow(double* aValue)
{
  *aValue = GetLow();
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMeterElement::SetLow(double aValue)
{
  return SetDoubleAttr(nsGkAtoms::low, aValue);
}

NS_IMETHODIMP
nsHTMLMeterElement::GetHigh(double* aValue)
{
  *aValue = GetHigh();
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMeterElement::SetHigh(double aValue)
{
  return SetDoubleAttr(nsGkAtoms::high, aValue);
}

NS_IMETHODIMP
nsHTMLMeterElement::GetOptimum(double* aValue)
{
  *aValue = GetOptimum();
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLMeterElement::SetOptimum(double aValue)
{
  return SetDoubleAttr(nsGkAtoms::optimum, aValue);
}

nsEventStates
nsHTMLMeterElement::GetOptimumState() const
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
  double value = GetValue();
  double low = GetLow();
  double high = GetHigh();
  double optimum = GetOptimum();

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

