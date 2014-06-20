/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLProgressElement_h
#define mozilla_dom_HTMLProgressElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"
#include "nsAttrValue.h"
#include "nsAttrValueInlines.h"
#include <algorithm>

namespace mozilla {
namespace dom {

class HTMLProgressElement MOZ_FINAL : public nsGenericHTMLElement
{
public:
  HTMLProgressElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  virtual ~HTMLProgressElement();

  EventStates IntrinsicState() const MOZ_OVERRIDE;

  nsresult Clone(mozilla::dom::NodeInfo* aNodeInfo, nsINode** aResult) const MOZ_OVERRIDE;

  bool ParseAttribute(int32_t aNamespaceID, nsIAtom* aAttribute,
                        const nsAString& aValue, nsAttrValue& aResult) MOZ_OVERRIDE;

  // WebIDL
  double Value() const;
  void SetValue(double aValue, ErrorResult& aRv)
  {
    SetDoubleAttr(nsGkAtoms::value, aValue, aRv);
  }
  double Max() const;
  void SetMax(double aValue, ErrorResult& aRv)
  {
    SetDoubleAttr(nsGkAtoms::max, aValue, aRv);
  }
  double Position() const;

protected:
  virtual JSObject* WrapNode(JSContext* aCx) MOZ_OVERRIDE;

protected:
  /**
   * Returns whethem the progress element is in the indeterminate state.
   * A progress element is in the indeterminate state if its value is ommited
   * or is not a floating point number..
   *
   * @return whether the progress element is in the indeterminate state.
   */
  bool IsIndeterminate() const;

  static const double kIndeterminatePosition;
  static const double kDefaultValue;
  static const double kDefaultMax;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLProgressElement_h
