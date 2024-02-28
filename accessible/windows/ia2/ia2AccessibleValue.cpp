/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ia2AccessibleValue.h"

#include "AccessibleValue_i.c"

#include "AccessibleWrap.h"
#include "LocalAccessible-inl.h"
#include "IUnknownImpl.h"
#include "MsaaAccessible.h"

#include "mozilla/FloatingPoint.h"

using namespace mozilla::a11y;

AccessibleWrap* ia2AccessibleValue::LocalAcc() {
  return static_cast<MsaaAccessible*>(this)->LocalAcc();
}

Accessible* ia2AccessibleValue::Acc() {
  return static_cast<MsaaAccessible*>(this)->Acc();
}

// IUnknown

STDMETHODIMP
ia2AccessibleValue::QueryInterface(REFIID iid, void** ppv) {
  if (!ppv) return E_INVALIDARG;

  *ppv = nullptr;

  if (IID_IAccessibleValue == iid) {
    Accessible* valueAcc = Acc();
    if (valueAcc && valueAcc->HasNumericValue()) {
      RefPtr<IAccessibleValue> result = this;
      result.forget(ppv);
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  return E_NOINTERFACE;
}

// IAccessibleValue

STDMETHODIMP
ia2AccessibleValue::get_currentValue(VARIANT* aCurrentValue) {
  if (!aCurrentValue) return E_INVALIDARG;

  VariantInit(aCurrentValue);

  Accessible* valueAcc = Acc();
  if (!valueAcc) {
    return CO_E_OBJNOTCONNECTED;
  }
  double currentValue;

  currentValue = valueAcc->CurValue();

  if (std::isnan(currentValue)) return S_FALSE;

  aCurrentValue->vt = VT_R8;
  aCurrentValue->dblVal = currentValue;
  return S_OK;
}

STDMETHODIMP
ia2AccessibleValue::setCurrentValue(VARIANT aValue) {
  if (aValue.vt != VT_R8) return E_INVALIDARG;

  Accessible* valueAcc = Acc();
  if (!valueAcc) {
    return CO_E_OBJNOTCONNECTED;
  }

  return valueAcc->SetCurValue(aValue.dblVal) ? S_OK : E_FAIL;
}

STDMETHODIMP
ia2AccessibleValue::get_maximumValue(VARIANT* aMaximumValue) {
  if (!aMaximumValue) return E_INVALIDARG;

  VariantInit(aMaximumValue);

  Accessible* valueAcc = Acc();
  if (!valueAcc) {
    return CO_E_OBJNOTCONNECTED;
  }
  double maximumValue;

  maximumValue = valueAcc->MaxValue();

  if (std::isnan(maximumValue)) return S_FALSE;

  aMaximumValue->vt = VT_R8;
  aMaximumValue->dblVal = maximumValue;
  return S_OK;
}

STDMETHODIMP
ia2AccessibleValue::get_minimumValue(VARIANT* aMinimumValue) {
  if (!aMinimumValue) return E_INVALIDARG;

  VariantInit(aMinimumValue);

  Accessible* valueAcc = Acc();
  if (!valueAcc) {
    return CO_E_OBJNOTCONNECTED;
  }
  double minimumValue;

  minimumValue = valueAcc->MinValue();

  if (std::isnan(minimumValue)) return S_FALSE;

  aMinimumValue->vt = VT_R8;
  aMinimumValue->dblVal = minimumValue;
  return S_OK;
}
