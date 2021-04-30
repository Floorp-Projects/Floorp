/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleHyperlink.h"
#include "AccessibleHyperlink_i.c"

#include "AccessibleWrap.h"
#include "IUnknownImpl.h"
#include "nsIURI.h"

using namespace mozilla::a11y;

AccessibleWrap* ia2AccessibleHyperlink::LocalAcc() {
  return static_cast<MsaaAccessible*>(this)->LocalAcc();
}

// IUnknown

STDMETHODIMP
ia2AccessibleHyperlink::QueryInterface(REFIID iid, void** ppv) {
  if (!ppv) return E_INVALIDARG;

  *ppv = nullptr;

  if (IID_IAccessibleHyperlink == iid) {
    auto accWrap = LocalAcc();
    if (!accWrap || (accWrap->IsProxy() ? !accWrap->Proxy()->IsLink()
                                        : !accWrap->IsLink())) {
      return E_NOINTERFACE;
    }

    *ppv = static_cast<IAccessibleHyperlink*>(this);
    (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
    return S_OK;
  }

  return ia2AccessibleAction::QueryInterface(iid, ppv);
}

// IAccessibleHyperlink

STDMETHODIMP
ia2AccessibleHyperlink::get_anchor(long aIndex, VARIANT* aAnchor) {
  if (!aAnchor) return E_INVALIDARG;

  VariantInit(aAnchor);

  LocalAccessible* thisObj = LocalAcc();
  if (!thisObj) {
    return CO_E_OBJNOTCONNECTED;
  }
  MOZ_ASSERT(!thisObj->IsProxy());

  if (aIndex < 0 || aIndex >= static_cast<long>(thisObj->AnchorCount()))
    return E_INVALIDARG;

  if (!thisObj->IsLink()) return S_FALSE;

  AccessibleWrap* anchor =
      static_cast<AccessibleWrap*>(thisObj->AnchorAt(aIndex));
  if (!anchor) return S_FALSE;

  RefPtr<IAccessible> result;
  anchor->GetNativeInterface(getter_AddRefs(result));
  result.forget(&aAnchor->punkVal);
  aAnchor->vt = VT_UNKNOWN;
  return S_OK;
}

STDMETHODIMP
ia2AccessibleHyperlink::get_anchorTarget(long aIndex, VARIANT* aAnchorTarget) {
  if (!aAnchorTarget) {
    return E_INVALIDARG;
  }

  VariantInit(aAnchorTarget);

  LocalAccessible* thisObj = LocalAcc();
  if (!thisObj) {
    return CO_E_OBJNOTCONNECTED;
  }
  nsAutoCString uriStr;
  MOZ_ASSERT(!thisObj->IsProxy());

  if (aIndex < 0 || aIndex >= static_cast<long>(thisObj->AnchorCount())) {
    return E_INVALIDARG;
  }

  if (!thisObj->IsLink()) {
    return S_FALSE;
  }

  nsCOMPtr<nsIURI> uri = thisObj->AnchorURIAt(aIndex);
  if (!uri) {
    return S_FALSE;
  }

  nsresult rv = uri->GetSpec(uriStr);
  if (NS_FAILED(rv)) {
    return GetHRESULT(rv);
  }

  nsAutoString stringURI;
  AppendUTF8toUTF16(uriStr, stringURI);

  aAnchorTarget->vt = VT_BSTR;
  aAnchorTarget->bstrVal =
      ::SysAllocStringLen(stringURI.get(), stringURI.Length());
  return aAnchorTarget->bstrVal ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
ia2AccessibleHyperlink::get_startIndex(long* aIndex) {
  if (!aIndex) return E_INVALIDARG;

  *aIndex = 0;

  LocalAccessible* thisObj = LocalAcc();
  if (!thisObj) {
    return CO_E_OBJNOTCONNECTED;
  }
  MOZ_ASSERT(!thisObj->IsProxy());

  if (!thisObj->IsLink()) return S_FALSE;

  *aIndex = thisObj->StartOffset();
  return S_OK;
}

STDMETHODIMP
ia2AccessibleHyperlink::get_endIndex(long* aIndex) {
  if (!aIndex) return E_INVALIDARG;

  *aIndex = 0;

  LocalAccessible* thisObj = LocalAcc();
  if (!thisObj) {
    return CO_E_OBJNOTCONNECTED;
  }
  MOZ_ASSERT(!thisObj->IsProxy());

  if (!thisObj->IsLink()) return S_FALSE;

  *aIndex = thisObj->EndOffset();
  return S_OK;
}

STDMETHODIMP
ia2AccessibleHyperlink::get_valid(boolean* aValid) {
  if (!aValid) return E_INVALIDARG;

  *aValid = false;

  LocalAccessible* thisObj = LocalAcc();
  if (!thisObj) {
    return CO_E_OBJNOTCONNECTED;
  }
  MOZ_ASSERT(!thisObj->IsProxy());

  if (!thisObj->IsLink()) return S_FALSE;

  *aValid = thisObj->IsLinkValid();
  return S_OK;
}
