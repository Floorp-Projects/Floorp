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
#include "IUnknownImpl.h"

#include "mozilla/FloatingPoint.h"

using namespace mozilla::a11y;

// IUnknown

STDMETHODIMP
ia2AccessibleValue::QueryInterface(REFIID iid, void** ppv)
{
  if (!ppv)
    return E_INVALIDARG;

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
  if (!aCurrentValue)
    return E_INVALIDARG;

  VariantInit(aCurrentValue);

  AccessibleWrap* valueAcc = static_cast<AccessibleWrap*>(this);
  double currentValue;
  MOZ_ASSERT(!valueAcc->IsProxy());
  if (valueAcc->IsDefunct()) {
    return CO_E_OBJNOTCONNECTED;
  }

  currentValue = valueAcc->CurValue();

  if (IsNaN(currentValue))
    return S_FALSE;

  aCurrentValue->vt = VT_R8;
  aCurrentValue->dblVal = currentValue;
  return S_OK;
}

STDMETHODIMP
ia2AccessibleValue::setCurrentValue(VARIANT aValue)
{
  if (aValue.vt != VT_R8)
    return E_INVALIDARG;

  AccessibleWrap* valueAcc = static_cast<AccessibleWrap*>(this);
  MOZ_ASSERT(!valueAcc->IsProxy());

  if (valueAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  return valueAcc->SetCurValue(aValue.dblVal) ? S_OK : E_FAIL;
}

STDMETHODIMP
ia2AccessibleValue::get_maximumValue(VARIANT* aMaximumValue)
{
  if (!aMaximumValue)
    return E_INVALIDARG;

  VariantInit(aMaximumValue);

  AccessibleWrap* valueAcc = static_cast<AccessibleWrap*>(this);
  double maximumValue;
  MOZ_ASSERT(!valueAcc->IsProxy());
  if (valueAcc->IsDefunct()) {
    return CO_E_OBJNOTCONNECTED;
  }

  maximumValue = valueAcc->MaxValue();

  if (IsNaN(maximumValue))
    return S_FALSE;

  aMaximumValue->vt = VT_R8;
  aMaximumValue->dblVal = maximumValue;
  return S_OK;
}

STDMETHODIMP
ia2AccessibleValue::get_minimumValue(VARIANT* aMinimumValue)
{
  if (!aMinimumValue)
    return E_INVALIDARG;

  VariantInit(aMinimumValue);

  AccessibleWrap* valueAcc = static_cast<AccessibleWrap*>(this);
  double minimumValue;
  MOZ_ASSERT(!valueAcc->IsProxy());
  if (valueAcc->IsDefunct()) {
    return CO_E_OBJNOTCONNECTED;
  }

  minimumValue = valueAcc->MinValue();

  if (IsNaN(minimumValue))
    return S_FALSE;

  aMinimumValue->vt = VT_R8;
  aMinimumValue->dblVal = minimumValue;
  return S_OK;
}

