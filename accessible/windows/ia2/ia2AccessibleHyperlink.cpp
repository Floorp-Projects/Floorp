/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Accessible2.h"
#include "AccessibleHyperlink.h"
#include "AccessibleHyperlink_i.c"

#include "AccessibleWrap.h"
#include "IUnknownImpl.h"
#include "nsIURI.h"

using namespace mozilla::a11y;

// IUnknown

STDMETHODIMP
ia2AccessibleHyperlink::QueryInterface(REFIID iid, void** ppv)
{
  if (!ppv)
    return E_INVALIDARG;

  *ppv = nullptr;

  if (IID_IAccessibleHyperlink == iid) {
    auto accWrap = static_cast<AccessibleWrap*>(this);
    if (accWrap->IsProxy() ?
        !(accWrap->ProxyInterfaces() & Interfaces::HYPERLINK) :
        !accWrap->IsLink())
      return E_NOINTERFACE;

    *ppv = static_cast<IAccessibleHyperlink*>(this);
    (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
    return S_OK;
  }

  return ia2AccessibleAction::QueryInterface(iid, ppv);
}

// IAccessibleHyperlink

STDMETHODIMP
ia2AccessibleHyperlink::get_anchor(long aIndex, VARIANT* aAnchor)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aAnchor)
    return E_INVALIDARG;

  VariantInit(aAnchor);

  Accessible* thisObj = static_cast<AccessibleWrap*>(this);
  if (thisObj->IsProxy()) {
    ProxyAccessible* anchor = thisObj->Proxy()->AnchorAt(aIndex);
    if (!anchor)
      return S_FALSE;

    IUnknown* tmp = static_cast<IAccessibleHyperlink*>(WrapperFor(anchor));
    tmp->AddRef();
    aAnchor->punkVal = tmp;
    aAnchor->vt = VT_UNKNOWN;
    return S_OK;
  }

  if (thisObj->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  if (aIndex < 0 || aIndex >= static_cast<long>(thisObj->AnchorCount()))
    return E_INVALIDARG;

  if (!thisObj->IsLink())
    return S_FALSE;

  AccessibleWrap* anchor =
    static_cast<AccessibleWrap*>(thisObj->AnchorAt(aIndex));
  if (!anchor)
    return S_FALSE;

  void* instancePtr = nullptr;
  HRESULT result = anchor->QueryInterface(IID_IUnknown, &instancePtr);
  if (FAILED(result))
    return result;

  IUnknown* unknownPtr = static_cast<IUnknown*>(instancePtr);
  aAnchor->ppunkVal = &unknownPtr;
  aAnchor->vt = VT_UNKNOWN;
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleHyperlink::get_anchorTarget(long aIndex, VARIANT* aAnchorTarget)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aAnchorTarget)
    return E_INVALIDARG;

  VariantInit(aAnchorTarget);

  Accessible* thisObj = static_cast<AccessibleWrap*>(this);
  nsAutoCString uriStr;
  if (thisObj->IsProxy()) {
    bool ok;
    thisObj->Proxy()->AnchorURIAt(aIndex, uriStr, &ok);
    if (!ok)
      return S_FALSE;

  } else {
    if (thisObj->IsDefunct())
      return CO_E_OBJNOTCONNECTED;

    if (aIndex < 0 || aIndex >= static_cast<long>(thisObj->AnchorCount()))
      return E_INVALIDARG;

    if (!thisObj->IsLink())
      return S_FALSE;

    nsCOMPtr<nsIURI> uri = thisObj->AnchorURIAt(aIndex);
    if (!uri)
      return S_FALSE;

    nsresult rv = uri->GetSpec(uriStr);
    if (NS_FAILED(rv))
      return GetHRESULT(rv);
  }

  nsAutoString stringURI;
  AppendUTF8toUTF16(uriStr, stringURI);

  aAnchorTarget->vt = VT_BSTR;
  aAnchorTarget->bstrVal = ::SysAllocStringLen(stringURI.get(),
                                               stringURI.Length());
  return aAnchorTarget->bstrVal ? S_OK : E_OUTOFMEMORY;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleHyperlink::get_startIndex(long* aIndex)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aIndex)
    return E_INVALIDARG;

  *aIndex = 0;

  if (ProxyAccessible* proxy = HyperTextProxyFor(this)) {
    bool valid;
    *aIndex = proxy->StartOffset(&valid);
    return valid ? S_OK : S_FALSE;
  }

  Accessible* thisObj = static_cast<AccessibleWrap*>(this);
  if (thisObj->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  if (!thisObj->IsLink())
    return S_FALSE;

  *aIndex = thisObj->StartOffset();
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleHyperlink::get_endIndex(long* aIndex)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aIndex)
    return E_INVALIDARG;

  *aIndex = 0;

  if (ProxyAccessible* proxy = HyperTextProxyFor(this)) {
    bool valid;
    *aIndex = proxy->EndOffset(&valid);
    return valid ? S_OK : S_FALSE;
  }

  Accessible* thisObj = static_cast<AccessibleWrap*>(this);
  if (thisObj->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  if (!thisObj->IsLink())
    return S_FALSE;

  *aIndex = thisObj->EndOffset();
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleHyperlink::get_valid(boolean* aValid)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aValid)
    return E_INVALIDARG;

  *aValid = false;

  if (ProxyAccessible* proxy = HyperTextProxyFor(this)) {
    *aValid = proxy->IsLinkValid();
    return S_OK;
  }

  Accessible* thisObj = static_cast<AccessibleWrap*>(this);
  if (thisObj->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  if (!thisObj->IsLink())
    return S_FALSE;

  *aValid = thisObj->IsLinkValid();
  return S_OK;

  A11Y_TRYBLOCK_END
}

