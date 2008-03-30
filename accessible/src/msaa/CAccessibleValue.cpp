/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "CAccessibleValue.h"

#include "AccessibleValue_i.c"

#include "nsIAccessibleValue.h"

#include "nsCOMPtr.h"

#include "nsAccessNodeWrap.h"

// IUnknown

STDMETHODIMP
CAccessibleValue::QueryInterface(REFIID iid, void** ppv)
{
  *ppv = NULL;

  if (IID_IAccessibleValue == iid) {
    nsCOMPtr<nsIAccessibleValue> valueAcc(do_QueryInterface(this));
    if (!valueAcc)
      return E_NOINTERFACE;

    *ppv = static_cast<IAccessibleValue*>(this);
    (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

// IAccessibleValue

STDMETHODIMP
CAccessibleValue::get_currentValue(VARIANT *aCurrentValue)
{
__try {
  VariantInit(aCurrentValue);

  nsCOMPtr<nsIAccessibleValue> valueAcc(do_QueryInterface(this));
  if (!valueAcc)
    return E_FAIL;

  double currentValue = 0;
  nsresult rv = valueAcc->GetCurrentValue(&currentValue);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  aCurrentValue->vt = VT_R8;
  aCurrentValue->dblVal = currentValue;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleValue::setCurrentValue(VARIANT aValue)
{
__try {
  nsCOMPtr<nsIAccessibleValue> valueAcc(do_QueryInterface(this));
  if (!valueAcc)
    return E_FAIL;

  if (aValue.vt != VT_R8)
    return E_INVALIDARG;

  nsresult rv = valueAcc->SetCurrentValue(aValue.dblVal);
  return GetHRESULT(rv);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleValue::get_maximumValue(VARIANT *aMaximumValue)
{
__try {
  VariantInit(aMaximumValue);

  nsCOMPtr<nsIAccessibleValue> valueAcc(do_QueryInterface(this));
  if (!valueAcc)
    return E_FAIL;

  double maximumValue = 0;
  nsresult rv = valueAcc->GetMaximumValue(&maximumValue);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  aMaximumValue->vt = VT_R8;
  aMaximumValue->dblVal = maximumValue;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleValue::get_minimumValue(VARIANT *aMinimumValue)
{
__try {
  VariantInit(aMinimumValue);

  nsCOMPtr<nsIAccessibleValue> valueAcc(do_QueryInterface(this));
  if (!valueAcc)
    return E_FAIL;

  double minimumValue = 0;
  nsresult rv = valueAcc->GetMinimumValue(&minimumValue);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  aMinimumValue->vt = VT_R8;
  aMinimumValue->dblVal = minimumValue;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

