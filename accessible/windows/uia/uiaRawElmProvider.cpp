/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "uiaRawElmProvider.h"

#include "AccAttributes.h"
#include "AccessibleWrap.h"
#include "ARIAMap.h"
#include "LocalAccessible-inl.h"
#include "MsaaAccessible.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// uiaRawElmProvider
////////////////////////////////////////////////////////////////////////////////

AccessibleWrap* uiaRawElmProvider::LocalAcc() {
  return static_cast<MsaaAccessible*>(this)->LocalAcc();
}

// IUnknown

// Because uiaRawElmProvider inherits multiple COM interfaces (and thus multiple
// IUnknowns), we need to explicitly implement AddRef and Release to make
// our QueryInterface implementation (IMPL_IUNKNOWN2) happy.
ULONG STDMETHODCALLTYPE uiaRawElmProvider::AddRef() {
  return static_cast<MsaaAccessible*>(this)->AddRef();
}

ULONG STDMETHODCALLTYPE uiaRawElmProvider::Release() {
  return static_cast<MsaaAccessible*>(this)->Release();
}

IMPL_IUNKNOWN2(uiaRawElmProvider, IAccessibleEx, IRawElementProviderSimple)

////////////////////////////////////////////////////////////////////////////////
// IAccessibleEx

STDMETHODIMP
uiaRawElmProvider::GetObjectForChild(
    long aIdChild, __RPC__deref_out_opt IAccessibleEx** aAccEx) {
  if (!aAccEx) return E_INVALIDARG;

  *aAccEx = nullptr;

  return LocalAcc() ? S_OK : CO_E_OBJNOTCONNECTED;
}

STDMETHODIMP
uiaRawElmProvider::GetIAccessiblePair(__RPC__deref_out_opt IAccessible** aAcc,
                                      __RPC__out long* aIdChild) {
  if (!aAcc || !aIdChild) return E_INVALIDARG;

  *aAcc = nullptr;
  *aIdChild = 0;

  if (!LocalAcc()) {
    return CO_E_OBJNOTCONNECTED;
  }

  *aIdChild = CHILDID_SELF;
  RefPtr<IAccessible> copy = static_cast<MsaaAccessible*>(this);
  copy.forget(aAcc);

  return S_OK;
}

STDMETHODIMP
uiaRawElmProvider::GetRuntimeId(__RPC__deref_out_opt SAFEARRAY** aRuntimeIds) {
  if (!aRuntimeIds) return E_INVALIDARG;
  AccessibleWrap* acc = LocalAcc();
  if (!acc) {
    return CO_E_OBJNOTCONNECTED;
  }

  int ids[] = {UiaAppendRuntimeId,
               static_cast<int>(reinterpret_cast<intptr_t>(acc->UniqueID()))};
  *aRuntimeIds = SafeArrayCreateVector(VT_I4, 0, 2);
  if (!*aRuntimeIds) return E_OUTOFMEMORY;

  for (LONG i = 0; i < (LONG)ArrayLength(ids); i++)
    SafeArrayPutElement(*aRuntimeIds, &i, (void*)&(ids[i]));

  return S_OK;
}

STDMETHODIMP
uiaRawElmProvider::ConvertReturnedElement(
    __RPC__in_opt IRawElementProviderSimple* aRawElmProvider,
    __RPC__deref_out_opt IAccessibleEx** aAccEx) {
  if (!aRawElmProvider || !aAccEx) return E_INVALIDARG;

  *aAccEx = nullptr;

  void* instancePtr = nullptr;
  HRESULT hr = aRawElmProvider->QueryInterface(IID_IAccessibleEx, &instancePtr);
  if (SUCCEEDED(hr)) *aAccEx = static_cast<IAccessibleEx*>(instancePtr);

  return hr;
}

////////////////////////////////////////////////////////////////////////////////
// IRawElementProviderSimple

STDMETHODIMP
uiaRawElmProvider::get_ProviderOptions(
    __RPC__out enum ProviderOptions* aOptions) {
  if (!aOptions) return E_INVALIDARG;

  // This method is not used with IAccessibleEx implementations.
  *aOptions = ProviderOptions_ServerSideProvider;
  return S_OK;
}

STDMETHODIMP
uiaRawElmProvider::GetPatternProvider(
    PATTERNID aPatternId, __RPC__deref_out_opt IUnknown** aPatternProvider) {
  if (!aPatternProvider) return E_INVALIDARG;

  *aPatternProvider = nullptr;
  return S_OK;
}

STDMETHODIMP
uiaRawElmProvider::GetPropertyValue(PROPERTYID aPropertyId,
                                    __RPC__out VARIANT* aPropertyValue) {
  if (!aPropertyValue) return E_INVALIDARG;

  AccessibleWrap* acc = LocalAcc();
  if (!acc) {
    return CO_E_OBJNOTCONNECTED;
  }

  aPropertyValue->vt = VT_EMPTY;

  switch (aPropertyId) {
    // Accelerator Key / shortcut.
    case UIA_AcceleratorKeyPropertyId: {
      nsAutoString keyString;

      acc->KeyboardShortcut().ToString(keyString);

      if (!keyString.IsEmpty()) {
        aPropertyValue->vt = VT_BSTR;
        aPropertyValue->bstrVal = ::SysAllocString(keyString.get());
        return S_OK;
      }

      break;
    }

    // Access Key / mneumonic.
    case UIA_AccessKeyPropertyId: {
      nsAutoString keyString;

      acc->AccessKey().ToString(keyString);

      if (!keyString.IsEmpty()) {
        aPropertyValue->vt = VT_BSTR;
        aPropertyValue->bstrVal = ::SysAllocString(keyString.get());
        return S_OK;
      }

      break;
    }

    // ARIA Role / shortcut
    case UIA_AriaRolePropertyId: {
      nsAutoString xmlRoles;

      RefPtr<AccAttributes> attributes = acc->Attributes();
      attributes->GetAttribute(nsGkAtoms::xmlroles, xmlRoles);

      if (!xmlRoles.IsEmpty()) {
        aPropertyValue->vt = VT_BSTR;
        aPropertyValue->bstrVal = ::SysAllocString(xmlRoles.get());
        return S_OK;
      }

      break;
    }

    // ARIA Properties
    case UIA_AriaPropertiesPropertyId: {
      nsAutoString ariaProperties;

      aria::AttrIterator attribIter(acc->GetContent());
      while (attribIter.Next()) {
        nsAutoString attribName, attribValue;
        nsAutoString value;
        attribIter.AttrName()->ToString(attribName);
        attribIter.AttrValue(attribValue);
        if (StringBeginsWith(attribName, u"aria-"_ns)) {
          // Found 'aria-'
          attribName.ReplaceLiteral(0, 5, u"");
        }

        ariaProperties.Append(attribName);
        ariaProperties.Append('=');
        ariaProperties.Append(attribValue);
        ariaProperties.Append(';');
      }

      if (!ariaProperties.IsEmpty()) {
        // remove last delimiter:
        ariaProperties.Truncate(ariaProperties.Length() - 1);
        aPropertyValue->vt = VT_BSTR;
        aPropertyValue->bstrVal = ::SysAllocString(ariaProperties.get());
        return S_OK;
      }

      break;
    }
  }

  return S_OK;
}

STDMETHODIMP
uiaRawElmProvider::get_HostRawElementProvider(
    __RPC__deref_out_opt IRawElementProviderSimple** aRawElmProvider) {
  if (!aRawElmProvider) return E_INVALIDARG;

  // This method is not used with IAccessibleEx implementations.
  *aRawElmProvider = nullptr;
  return S_OK;
}
