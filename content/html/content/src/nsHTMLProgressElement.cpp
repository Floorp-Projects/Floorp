/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mounir Lamouri <mounir.lamouri@mozilla.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIDOMHTMLProgressElement.h"
#include "nsGenericHTMLElement.h"
#include "nsAttrValue.h"
#include "nsEventStateManager.h"


class nsHTMLProgressElement : public nsGenericHTMLFormElement,
                              public nsIDOMHTMLProgressElement
{
public:
  nsHTMLProgressElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLProgressElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLFormElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLProgressElement
  NS_DECL_NSIDOMHTMLPROGRESSELEMENT

  // nsIFormControl
  NS_IMETHOD_(PRUint32) GetType() const { return NS_FORM_PROGRESS; }
  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsFormSubmission* aFormSubmission);

  nsEventStates IntrinsicState() const;

  nsresult Clone(nsINodeInfo* aNodeInfo, nsINode** aResult) const;

  bool ParseAttribute(PRInt32 aNamespaceID, nsIAtom* aAttribute,
                        const nsAString& aValue, nsAttrValue& aResult);

  virtual nsXPCClassInfo* GetClassInfo();

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
  : nsGenericHTMLFormElement(aNodeInfo)
{
  // We start out indeterminate
  AddStatesSilently(NS_EVENT_STATE_INDETERMINATE);
}

nsHTMLProgressElement::~nsHTMLProgressElement()
{
}

NS_IMPL_ADDREF_INHERITED(nsHTMLProgressElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLProgressElement, nsGenericElement)

DOMCI_NODE_DATA(HTMLProgressElement, nsHTMLProgressElement)

NS_INTERFACE_TABLE_HEAD(nsHTMLProgressElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(nsHTMLProgressElement,
                                   nsIDOMHTMLProgressElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLProgressElement,
                                               nsGenericHTMLFormElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLProgressElement)

NS_IMPL_ELEMENT_CLONE(nsHTMLProgressElement)


NS_IMETHODIMP
nsHTMLProgressElement::Reset()
{
  // The progress element is not resettable.
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLProgressElement::SubmitNamesValues(nsFormSubmission* aFormSubmission)
{
  // The progress element is not submittable.
  return NS_OK;
}

nsEventStates
nsHTMLProgressElement::IntrinsicState() const
{
  nsEventStates state = nsGenericHTMLFormElement::IntrinsicState();

  if (IsIndeterminate()) {
    state |= NS_EVENT_STATE_INDETERMINATE;
  }

  return state;
}

bool
nsHTMLProgressElement::ParseAttribute(PRInt32 aNamespaceID, nsIAtom* aAttribute,
                                      const nsAString& aValue, nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::value || aAttribute == nsGkAtoms::max) {
      return aResult.ParseDoubleValue(aValue);
    }
  }

  return nsGenericHTMLFormElement::ParseAttribute(aNamespaceID, aAttribute,
                                                  aValue, aResult);
}

NS_IMETHODIMP
nsHTMLProgressElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  return nsGenericHTMLFormElement::GetForm(aForm);
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

  *aValue = NS_MIN(*aValue, max);

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

