/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 * Portions created by Sun Microsystems are Copyright (C) 2002 Sun
 * Microsystems, Inc. All Rights Reserved.
 *
 * Original Author:  Gilbert Fang (gilbert.fang@sun.com)
 *
 * Contributor(s):
 *   Kyle Yuan (kyle.yuan@sun.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIDOMElement.h"
#include "nsXULFormControlAccessibleWrap.h"

NS_IMPL_ISUPPORTS_INHERITED1(nsXULProgressMeterAccessibleWrap, nsXULProgressMeterAccessible, nsIAccessibleValue)

nsXULProgressMeterAccessibleWrap::nsXULProgressMeterAccessibleWrap(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsXULProgressMeterAccessible(aNode, aShell)
{
}

/* readonly attribute double maximumValue; */
NS_IMETHODIMP nsXULProgressMeterAccessibleWrap::GetMaximumValue(double *aMaximumValue)
{
  *aMaximumValue = 1; // 100% = 1;
  return NS_OK;
}

/* readonly attribute double minimumValue; */
NS_IMETHODIMP nsXULProgressMeterAccessibleWrap::GetMinimumValue(double *aMinimumValue)
{
  *aMinimumValue = 0;
  return NS_OK;
}

/* readonly attribute double currentValue; */
NS_IMETHODIMP nsXULProgressMeterAccessibleWrap::GetCurrentValue(double *aCurrentValue)
{
  nsAutoString currentValue;
  GetValue(currentValue);
  PRInt32 error;
  *aCurrentValue = currentValue.ToFloat(&error) / 100;
  return NS_OK;
}

/* boolean setCurrentValue (in double value); */
NS_IMETHODIMP nsXULProgressMeterAccessibleWrap::SetCurrentValue(double aValue, PRBool *_retval)
{
  //Here I do not suppose the min/max are 0/1.00 because I want
  // these part of code to be more extensible.
  *_retval = PR_FALSE;
  double min, max;
  GetMinimumValue(&min);
  GetMaximumValue(&max);
  if (aValue > max || aValue < min)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mDOMNode));
  NS_ASSERTION(element, "No element for DOM node!");
  PRUint32 value = PRUint32(aValue * 100.0 + 0.5);
  nsAutoString valueString;
  valueString.AppendInt(value);
  valueString.AppendLiteral("%");
  if (NS_SUCCEEDED(element->SetAttribute(NS_LITERAL_STRING("value"), valueString))) {
    *_retval = PR_TRUE;
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}
