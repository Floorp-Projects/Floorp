/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLMeterElement_h
#define mozilla_dom_HTMLMeterElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"
#include "nsAttrValue.h"
#include "nsAttrValueInlines.h"
#include "nsAlgorithm.h"
#include <algorithm>

namespace mozilla {
namespace dom {

class HTMLMeterElement final : public nsGenericHTMLElement
{
public:
  explicit HTMLMeterElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);

  virtual EventStates IntrinsicState() const override;

  nsresult Clone(mozilla::dom::NodeInfo* aNodeInfo, nsINode** aResult) const override;

  bool ParseAttribute(int32_t aNamespaceID, nsIAtom* aAttribute,
                      const nsAString& aValue, nsAttrValue& aResult) override;

  // WebIDL

  /* @return the value */
  double Value() const;
  void SetValue(double aValue, ErrorResult& aRv)
  {
    SetDoubleAttr(nsGkAtoms::value, aValue, aRv);
  }

  /* @return the minimum value */
  double Min() const;
  void SetMin(double aValue, ErrorResult& aRv)
  {
    SetDoubleAttr(nsGkAtoms::min, aValue, aRv);
  }

  /* @return the maximum value */
  double Max() const;
  void SetMax(double aValue, ErrorResult& aRv)
  {
    SetDoubleAttr(nsGkAtoms::max, aValue, aRv);
  }

  /* @return the low value */
  double Low() const;
  void SetLow(double aValue, ErrorResult& aRv)
  {
    SetDoubleAttr(nsGkAtoms::low, aValue, aRv);
  }

  /* @return the high value */
  double High() const;
  void SetHigh(double aValue, ErrorResult& aRv)
  {
    SetDoubleAttr(nsGkAtoms::high, aValue, aRv);
  }

  /* @return the optimum value */
  double Optimum() const;
  void SetOptimum(double aValue, ErrorResult& aRv)
  {
    SetDoubleAttr(nsGkAtoms::optimum, aValue, aRv);
  }

protected:
  virtual ~HTMLMeterElement();

  virtual JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

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
  EventStates GetOptimumState() const;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLMeterElement_h
