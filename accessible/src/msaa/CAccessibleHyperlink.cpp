/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CAccessibleHyperlink.h"

#include "Accessible2.h"
#include "AccessibleHyperlink.h"
#include "AccessibleHyperlink_i.c"

#include "nsAccessibleWrap.h"
#include "nsIWinAccessNode.h"

// IUnknown

STDMETHODIMP
CAccessibleHyperlink::QueryInterface(REFIID iid, void** ppv)
{
  *ppv = NULL;

  if (IID_IAccessibleHyperlink == iid) {
    nsRefPtr<nsAccessible> thisObj = do_QueryObject(this);
    if (!thisObj->IsLink())
      return E_NOINTERFACE;

    *ppv = static_cast<IAccessibleHyperlink*>(this);
    (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
    return S_OK;
  }

  return ia2AccessibleAction::QueryInterface(iid, ppv);
}

// IAccessibleHyperlink

STDMETHODIMP
CAccessibleHyperlink::get_anchor(long aIndex, VARIANT *aAnchor)
{
__try {
  VariantInit(aAnchor);

  nsRefPtr<nsAccessible> thisObj = do_QueryObject(this);
  if (thisObj->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  if (aIndex < 0 || aIndex >= static_cast<long>(thisObj->AnchorCount()))
    return E_INVALIDARG;

  if (!thisObj->IsLink())
    return S_FALSE;

  nsAccessibleWrap* anchor =
    static_cast<nsAccessibleWrap*>(thisObj->AnchorAt(aIndex));
  if (!anchor)
    return S_FALSE;

  void* instancePtr = NULL;
  HRESULT result = anchor->QueryInterface(IID_IUnknown, &instancePtr);
  if (FAILED(result))
    return result;

  IUnknown* unknownPtr = static_cast<IUnknown*>(instancePtr);
  aAnchor->ppunkVal = &unknownPtr;
  aAnchor->vt = VT_UNKNOWN;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleHyperlink::get_anchorTarget(long aIndex, VARIANT *aAnchorTarget)
{
__try {
  VariantInit(aAnchorTarget);

  nsRefPtr<nsAccessible> thisObj = do_QueryObject(this);
  if (thisObj->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  if (aIndex < 0 || aIndex >= static_cast<long>(thisObj->AnchorCount()))
    return E_INVALIDARG;

  if (!thisObj->IsLink())
    return S_FALSE;

  nsCOMPtr<nsIURI> uri = thisObj->AnchorURIAt(aIndex);
  if (!uri)
    return S_FALSE;

  nsCAutoString prePath;
  nsresult rv = uri->GetPrePath(prePath);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  nsCAutoString path;
  rv = uri->GetPath(path);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  nsAutoString stringURI;
  AppendUTF8toUTF16(prePath, stringURI);
  AppendUTF8toUTF16(path, stringURI);

  aAnchorTarget->vt = VT_BSTR;
  aAnchorTarget->bstrVal = ::SysAllocStringLen(stringURI.get(),
                                               stringURI.Length());
  return aAnchorTarget->bstrVal ? S_OK : E_OUTOFMEMORY;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleHyperlink::get_startIndex(long *aIndex)
{
__try {
  *aIndex = 0;

  nsRefPtr<nsAccessible> thisObj = do_QueryObject(this);
  if (thisObj->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  if (!thisObj->IsLink())
    return S_FALSE;

  *aIndex = thisObj->StartOffset();
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleHyperlink::get_endIndex(long *aIndex)
{
__try {
  *aIndex = 0;

  nsRefPtr<nsAccessible> thisObj = do_QueryObject(this);
  if (thisObj->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  if (!thisObj->IsLink())
    return S_FALSE;

  *aIndex = thisObj->EndOffset();
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleHyperlink::get_valid(boolean *aValid)
{
__try {
  *aValid = false;

  nsRefPtr<nsAccessible> thisObj = do_QueryObject(this);
  if (thisObj->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  if (!thisObj->IsLink())
    return S_FALSE;

  *aValid = thisObj->IsLinkValid();
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

