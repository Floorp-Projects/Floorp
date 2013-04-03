/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ia2AccessibleValue.h"

#include "AccessibleValue_i.c"

#include "AccessibleWrap.h"
#include "Accessible-inl.h"

using namespace mozilla::a11y;

// IUnknown

STDMETHODIMP
ia2AccessibleValue::QueryInterface(REFIID iid, void** ppv)
{
  *ppv = nullptr;

  if (IID_IAccessibleValue == iid) {
    AccessibleWrap* valueAcc = static_cast<AccessibleWrap*>(this);
    if (valueAcc->HasNumericValue()) {
      *ppv = static_cast<IAccessibleValue*>(this);
      valueAcc->AddRef();
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  return E_NOINTERFACE;
}

// IAccessibleValue

STDMETHODIMP
ia2AccessibleValue::get_currentValue(VARIANT* aCurrentValue)
{
  A11Y_TRYBLOCK_BEGIN

  VariantInit(aCurrentValue);

  AccessibleWrap* valueAcc = static_cast<AccessibleWrap*>(this);
  if (valueAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  double currentValue = 0;
  nsresult rv = valueAcc->GetCurrentValue(&currentValue);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  aCurrentValue->vt = VT_R8;
  aCurrentValue->dblVal = currentValue;
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleValue::setCurrentValue(VARIANT aValue)
{
  A11Y_TRYBLOCK_BEGIN

  AccessibleWrap* valueAcc = static_cast<AccessibleWrap*>(this);
  if (valueAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  if (aValue.vt != VT_R8)
    return E_INVALIDARG;

  nsresult rv = valueAcc->SetCurrentValue(aValue.dblVal);
  return GetHRESULT(rv);

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleValue::get_maximumValue(VARIANT* aMaximumValue)
{
  A11Y_TRYBLOCK_BEGIN

  VariantInit(aMaximumValue);

  AccessibleWrap* valueAcc = static_cast<AccessibleWrap*>(this);
  if (valueAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  double maximumValue = 0;
  nsresult rv = valueAcc->GetMaximumValue(&maximumValue);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  aMaximumValue->vt = VT_R8;
  aMaximumValue->dblVal = maximumValue;
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleValue::get_minimumValue(VARIANT* aMinimumValue)
{
  A11Y_TRYBLOCK_BEGIN

  VariantInit(aMinimumValue);

  AccessibleWrap* valueAcc = static_cast<AccessibleWrap*>(this);
  if (valueAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  double minimumValue = 0;
  nsresult rv = valueAcc->GetMinimumValue(&minimumValue);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  aMinimumValue->vt = VT_R8;
  aMinimumValue->dblVal = minimumValue;
  return S_OK;

  A11Y_TRYBLOCK_END
}

