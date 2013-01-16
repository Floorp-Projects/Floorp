/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIDOMHTMLProgressElement.h"
#include "nsGenericHTMLElement.h"
#include "nsAttrValue.h"
#include "nsAttrValueInlines.h"
#include "nsEventStateManager.h"
#include <algorithm>

using namespace mozilla::dom;

class nsHTMLProgressElement : public nsGenericHTMLElement,
                              public nsIDOMHTMLProgressElement
{
public:
  nsHTMLProgressElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLProgressElement();

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

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }

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

const double nsHTMLProgressElement::kIndeterminatePosition = -1.0;
const double nsHTMLProgressElement::kDefaultValue          =  0.0;
const double nsHTMLProgressElement::kDefaultMax            =  1.0;

NS_IMPL_NS_NEW_HTML_ELEMENT(Progress)


nsHTMLProgressElement::nsHTMLProgressElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
  // We start out indeterminate
  AddStatesSilently(NS_EVENT_STATE_INDETERMINATE);
}

nsHTMLProgressElement::~nsHTMLProgressElement()
{
}

NS_IMPL_ADDREF_INHERITED(nsHTMLProgressElement, Element)
NS_IMPL_RELEASE_INHERITED(nsHTMLProgressElement, Element)

DOMCI_NODE_DATA(HTMLProgressElement, nsHTMLProgressElement)

NS_INTERFACE_TABLE_HEAD(nsHTMLProgressElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(nsHTMLProgressElement,
                                   nsIDOMHTMLProgressElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLProgressElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLProgressElement)

NS_IMPL_ELEMENT_CLONE(nsHTMLProgressElement)


nsEventStates
nsHTMLProgressElement::IntrinsicState() const
{
  nsEventStates state = nsGenericHTMLElement::IntrinsicState();

  if (IsIndeterminate()) {
    state |= NS_EVENT_STATE_INDETERMINATE;
  }

  return state;
}

bool
nsHTMLProgressElement::ParseAttribute(int32_t aNamespaceID, nsIAtom* aAttribute,
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
nsHTMLProgressElement::GetValue(double* aValue)
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
nsHTMLProgressElement::SetValue(double aValue)
{
  return SetDoubleAttr(nsGkAtoms::value, aValue);
}

NS_IMETHODIMP
nsHTMLProgressElement::GetMax(double* aValue)
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
nsHTMLProgressElement::SetMax(double aValue)
{
  return SetDoubleAttr(nsGkAtoms::max, aValue);
}

NS_IMETHODIMP
nsHTMLProgressElement::GetPosition(double* aPosition)
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
nsHTMLProgressElement::IsIndeterminate() const
{
  const nsAttrValue* attrValue = mAttrsAndChildren.GetAttr(nsGkAtoms::value);
  return !attrValue || attrValue->Type() != nsAttrValue::eDoubleValue;
}

