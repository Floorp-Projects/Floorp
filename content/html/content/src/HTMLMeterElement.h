/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLMeterElement_h
#define mozilla_dom_HTMLMeterElement_h

#include "nsIDOMHTMLMeterElement.h"
#include "nsGenericHTMLElement.h"
#include "nsAttrValue.h"
#include "nsAttrValueInlines.h"
#include "nsEventStateManager.h"
#include "nsAlgorithm.h"
#include <algorithm>

namespace mozilla {
namespace dom {

class HTMLMeterElement : public nsGenericHTMLElement,
                         public nsIDOMHTMLMeterElement
{
public:
  HTMLMeterElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~HTMLMeterElement();

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

  virtual nsIDOMNode* AsDOMNode() { return this; }

  // WebIDL

  /* @return the value */
  double Value() const;
  void SetValue(double aValue, ErrorResult& aRv)
  {
    aRv = SetValue(aValue);
  }

  /* @return the minimum value */
  double Min() const;
  void SetMin(double aValue, ErrorResult& aRv)
  {
    aRv = SetMin(aValue);
  }

  /* @return the maximum value */
  double Max() const;
  void SetMax(double aValue, ErrorResult& aRv)
  {
    aRv = SetMax(aValue);
  }

  /* @return the low value */
  double Low() const;
  void SetLow(double aValue, ErrorResult& aRv)
  {
    aRv = SetLow(aValue);
  }

  /* @return the high value */
  double High() const;
  void SetHigh(double aValue, ErrorResult& aRv)
  {
    aRv = SetHigh(aValue);
  }

  /* @return the optimum value */
  double Optimum() const;
  void SetOptimum(double aValue, ErrorResult& aRv)
  {
    aRv = SetOptimum(aValue);
  }

protected:
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

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
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLMeterElement_h
