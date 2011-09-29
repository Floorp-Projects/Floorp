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
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Gaunt (jgaunt@netscape.com)
 *   Aaron Leventhal (aaronl@netscape.com)
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

// NOTE: alphabetically ordered
#include "nsFormControlAccessible.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMXULControlElement.h"

////////////////////////////////////////////////////////////////////////////////
// ProgressMeterAccessible
////////////////////////////////////////////////////////////////////////////////

template class ProgressMeterAccessible<1>;
template class ProgressMeterAccessible<100>;

////////////////////////////////////////////////////////////////////////////////
// nsISupports

template<int Max>
NS_IMPL_ADDREF_INHERITED(ProgressMeterAccessible<Max>, nsFormControlAccessible)

template<int Max>
NS_IMPL_RELEASE_INHERITED(ProgressMeterAccessible<Max>, nsFormControlAccessible)

template<int Max>
NS_IMPL_QUERY_INTERFACE_INHERITED1(ProgressMeterAccessible<Max>,
                                   nsFormControlAccessible,
                                   nsIAccessibleValue)

////////////////////////////////////////////////////////////////////////////////
// nsAccessible

template<int Max>
PRUint32
ProgressMeterAccessible<Max>::NativeRole()
{
  return nsIAccessibleRole::ROLE_PROGRESSBAR;
}

////////////////////////////////////////////////////////////////////////////////
// ProgressMeterAccessible<Max>: Widgets

template<int Max>
bool
ProgressMeterAccessible<Max>::IsWidget() const
{
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibleValue

template<int Max>
NS_IMETHODIMP
ProgressMeterAccessible<Max>::GetValue(nsAString& aValue)
{
  nsresult rv = nsFormControlAccessible::GetValue(aValue);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aValue.IsEmpty())
    return NS_OK;

  double maxValue = 0;
  rv = GetMaximumValue(&maxValue);
  NS_ENSURE_SUCCESS(rv, rv);

  double curValue = 0;
  rv = GetCurrentValue(&curValue);
  NS_ENSURE_SUCCESS(rv, rv);

  // Treat the current value bigger than maximum as 100%.
  double percentValue = (curValue < maxValue) ?
    (curValue / maxValue) * 100 : 100;

  nsAutoString value;
  value.AppendFloat(percentValue); // AppendFloat isn't available on nsAString
  value.AppendLiteral("%");
  aValue = value;
  return NS_OK;
}

template<int Max>
NS_IMETHODIMP
ProgressMeterAccessible<Max>::GetMaximumValue(double* aMaximumValue)
{
  nsresult rv = nsFormControlAccessible::GetMaximumValue(aMaximumValue);
  if (rv != NS_OK_NO_ARIA_VALUE)
    return rv;

  nsAutoString value;
  if (mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::max, value)) {
    PRInt32 result = NS_OK;
    *aMaximumValue = value.ToDouble(&result);
    return result;
  }

  *aMaximumValue = Max;
  return NS_OK;
}

template<int Max>
NS_IMETHODIMP
ProgressMeterAccessible<Max>::GetMinimumValue(double* aMinimumValue)
{
  nsresult rv = nsFormControlAccessible::GetMinimumValue(aMinimumValue);
  if (rv != NS_OK_NO_ARIA_VALUE)
    return rv;

  *aMinimumValue = 0;
  return NS_OK;
}

template<int Max>
NS_IMETHODIMP
ProgressMeterAccessible<Max>::GetMinimumIncrement(double* aMinimumIncrement)
{
  nsresult rv = nsFormControlAccessible::GetMinimumIncrement(aMinimumIncrement);
  if (rv != NS_OK_NO_ARIA_VALUE)
    return rv;

  *aMinimumIncrement = 0;
  return NS_OK;
}

template<int Max>
NS_IMETHODIMP
ProgressMeterAccessible<Max>::GetCurrentValue(double* aCurrentValue)
{
  nsresult rv = nsFormControlAccessible::GetCurrentValue(aCurrentValue);
  if (rv != NS_OK_NO_ARIA_VALUE)
    return rv;

  nsAutoString attrValue;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::value, attrValue);

  // Return zero value if there is no attribute or its value is empty.
  if (attrValue.IsEmpty())
    return NS_OK;

  PRInt32 error = NS_OK;
  double value = attrValue.ToDouble(&error);
  if (NS_FAILED(error))
    return NS_OK; // Zero value because of wrong markup.

  *aCurrentValue = value;
  return NS_OK;
}

template<int Max>
NS_IMETHODIMP
ProgressMeterAccessible<Max>::SetCurrentValue(double aValue)
{
  return NS_ERROR_FAILURE; // Progress meters are readonly.
}

////////////////////////////////////////////////////////////////////////////////
// nsRadioButtonAccessible
////////////////////////////////////////////////////////////////////////////////

nsRadioButtonAccessible::
  nsRadioButtonAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsFormControlAccessible(aContent, aShell)
{
}

PRUint8
nsRadioButtonAccessible::ActionCount()
{
  return 1;
}

NS_IMETHODIMP nsRadioButtonAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex == eAction_Click) {
    aName.AssignLiteral("select"); 
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsRadioButtonAccessible::DoAction(PRUint8 aIndex)
{
  if (aIndex != eAction_Click)
    return NS_ERROR_INVALID_ARG;

  DoCommand();
  return NS_OK;
}

PRUint32
nsRadioButtonAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_RADIOBUTTON;
}

////////////////////////////////////////////////////////////////////////////////
// nsRadioButtonAccessible: Widgets

bool
nsRadioButtonAccessible::IsWidget() const
{
  return true;
}
