/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLProgressElement_h
#define mozilla_dom_HTMLProgressElement_h

#include "nsIDOMHTMLProgressElement.h"
#include "nsGenericHTMLElement.h"
#include "nsAttrValue.h"
#include "nsAttrValueInlines.h"
#include "nsEventStateManager.h"
#include <algorithm>

namespace mozilla {
namespace dom {

class HTMLProgressElement : public nsGenericHTMLElement,
                            public nsIDOMHTMLProgressElement
{
public:
  HTMLProgressElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~HTMLProgressElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_TO_GENERIC

  // nsIDOMHTMLProgressElement
  NS_DECL_NSIDOMHTMLPROGRESSELEMENT

  nsEventStates IntrinsicState() const;

  nsresult Clone(nsINodeInfo* aNodeInfo, nsINode** aResult) const;

  bool ParseAttribute(int32_t aNamespaceID, nsIAtom* aAttribute,
                        const nsAString& aValue, nsAttrValue& aResult);

  virtual nsIDOMNode* AsDOMNode() { return this; }

  // WebIDL
  double Value() const;
  void SetValue(double aValue, ErrorResult& aRv)
  {
    aRv = SetDoubleAttr(nsGkAtoms::value, aValue);
  }
  double Max() const;
  void SetMax(double aValue, ErrorResult& aRv)
  {
    aRv = SetDoubleAttr(nsGkAtoms::max, aValue);
  }
  double Position() const;

protected:
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

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
