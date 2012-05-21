/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
    nsCOMPtr<nsIAccessibleValue> valueAcc(do_QueryObject(this));
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

  nsCOMPtr<nsIAccessibleValue> valueAcc(do_QueryObject(this));
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
  nsCOMPtr<nsIAccessibleValue> valueAcc(do_QueryObject(this));
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

  nsCOMPtr<nsIAccessibleValue> valueAcc(do_QueryObject(this));
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

  nsCOMPtr<nsIAccessibleValue> valueAcc(do_QueryObject(this));
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

