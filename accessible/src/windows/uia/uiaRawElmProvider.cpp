/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "uiaRawElmProvider.h"

#include "AccessibleWrap.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// uiaRawElmProvider
////////////////////////////////////////////////////////////////////////////////

IMPL_IUNKNOWN2(uiaRawElmProvider,
               IAccessibleEx,
               IRawElementProviderSimple)

////////////////////////////////////////////////////////////////////////////////
// IAccessibleEx

STDMETHODIMP
uiaRawElmProvider::GetObjectForChild(long aIdChild,
                                     __RPC__deref_out_opt IAccessibleEx** aAccEx)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aAccEx)
    return E_INVALIDARG;

  *aAccEx = NULL;

  return mAcc->IsDefunct() ? CO_E_OBJNOTCONNECTED : S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
uiaRawElmProvider::GetIAccessiblePair(__RPC__deref_out_opt IAccessible** aAcc,
                                      __RPC__out long* aIdChild)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aAcc || !aIdChild)
    return E_INVALIDARG;

  *aAcc = NULL;
  *aIdChild = 0;

  if (mAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  *aIdChild = CHILDID_SELF;
  *aAcc = mAcc;
  mAcc->AddRef();

  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
uiaRawElmProvider::GetRuntimeId(__RPC__deref_out_opt SAFEARRAY** aRuntimeIds)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aRuntimeIds)
    return E_INVALIDARG;

  int ids[] = { UiaAppendRuntimeId, reinterpret_cast<int>(mAcc->UniqueID()) };
  *aRuntimeIds = SafeArrayCreateVector(VT_I4, 0, 2);
  if (!*aRuntimeIds)
    return E_OUTOFMEMORY;

  for (LONG i = 0; i < ArrayLength(ids); i++)
    SafeArrayPutElement(*aRuntimeIds, &i, (void*)&(ids[i]));

  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
uiaRawElmProvider::ConvertReturnedElement(__RPC__in_opt IRawElementProviderSimple* aRawElmProvider,
                                          __RPC__deref_out_opt IAccessibleEx** aAccEx)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aRawElmProvider || !aAccEx)
    return E_INVALIDARG;

  *aAccEx = NULL;

  void* instancePtr = NULL;
  HRESULT hr = aRawElmProvider->QueryInterface(IID_IAccessibleEx, &instancePtr);
  if (SUCCEEDED(hr))
    *aAccEx = static_cast<IAccessibleEx*>(instancePtr);

  return hr;

  A11Y_TRYBLOCK_END
}

////////////////////////////////////////////////////////////////////////////////
// IRawElementProviderSimple

STDMETHODIMP
uiaRawElmProvider::get_ProviderOptions(__RPC__out enum ProviderOptions* aOptions)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aOptions)
    return E_INVALIDARG;

  // This method is not used with IAccessibleEx implementations.
  *aOptions = ProviderOptions_ServerSideProvider;
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
uiaRawElmProvider::GetPatternProvider(PATTERNID aPatternId,
                                      __RPC__deref_out_opt IUnknown** aPatternProvider)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aPatternProvider)
    return E_INVALIDARG;

  *aPatternProvider = NULL;
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
uiaRawElmProvider::GetPropertyValue(PROPERTYID aPropertyId,
                                    __RPC__out VARIANT* aPropertyValue)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aPropertyValue)
    return E_INVALIDARG;

  // UI Automation will attempt to get the property from the host
  //window provider.
  aPropertyValue->vt = VT_EMPTY;
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
uiaRawElmProvider::get_HostRawElementProvider(__RPC__deref_out_opt IRawElementProviderSimple** aRawElmProvider)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aRawElmProvider)
    return E_INVALIDARG;

  // This method is not used with IAccessibleEx implementations.
  *aRawElmProvider = NULL;
  return S_OK;

  A11Y_TRYBLOCK_END
}
