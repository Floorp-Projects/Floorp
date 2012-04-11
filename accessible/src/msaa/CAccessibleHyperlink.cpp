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

