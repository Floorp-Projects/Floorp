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

namespace mozilla::dom {

class HTMLMeterElement final : public nsGenericHTMLElement {
 public:
  explicit HTMLMeterElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                      const nsAString& aValue,
                      nsIPrincipal* aMaybeScriptedPrincipal,
                      nsAttrValue& aResult) override;
  void AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                    const nsAttrValue* aValue, const nsAttrValue* aOldValue,
                    nsIPrincipal* aSubjectPrincipal, bool aNotify) override;

  // WebIDL

  /* @return the value */
  double Value() const;
  /* Returns the percentage that this element should be filed based on the
   * min/max/value */
  double Position() const;
  void SetValue(double aValue, ErrorResult& aRv) {
    SetDoubleAttr(nsGkAtoms::value, aValue, aRv);
  }

  /* @return the minimum value */
  double Min() const;
  void SetMin(double aValue, ErrorResult& aRv) {
    SetDoubleAttr(nsGkAtoms::min, aValue, aRv);
  }

  /* @return the maximum value */
  double Max() const;
  void SetMax(double aValue, ErrorResult& aRv) {
    SetDoubleAttr(nsGkAtoms::max, aValue, aRv);
  }

  /* @return the low value */
  double Low() const;
  void SetLow(double aValue, ErrorResult& aRv) {
    SetDoubleAttr(nsGkAtoms::low, aValue, aRv);
  }

  /* @return the high value */
  double High() const;
  void SetHigh(double aValue, ErrorResult& aRv) {
    SetDoubleAttr(nsGkAtoms::high, aValue, aRv);
  }

  /* @return the optimum value */
  double Optimum() const;
  void SetOptimum(double aValue, ErrorResult& aRv) {
    SetDoubleAttr(nsGkAtoms::optimum, aValue, aRv);
  }

  NS_IMPL_FROMNODE_HTML_WITH_TAG(HTMLMeterElement, meter);

 protected:
  virtual ~HTMLMeterElement();

  JSObject* WrapNode(JSContext*, JS::Handle<JSObject*> aGivenProto) override;

 private:
  /**
   * Returns the optimum state of the element.
   * ElementState::OPTIMUM if the actual value is in the optimum region.
   * ElementState::SUB_OPTIMUM if the actual value is in the sub-optimal
   *                            region.
   * ElementState::SUB_SUB_OPTIMUM if the actual value is in the
   *                                sub-sub-optimal region.
   *
   * @return the optimum state of the element.
   */
  ElementState GetOptimumState() const;
  void UpdateOptimumState(bool aNotify);
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_HTMLMeterElement_h
