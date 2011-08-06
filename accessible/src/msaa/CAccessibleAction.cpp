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

#include "CAccessibleAction.h"

#include "AccessibleAction_i.c"

#include "nsAccessible.h"

// IUnknown

STDMETHODIMP
CAccessibleAction::QueryInterface(REFIID iid, void** ppv)
{
  *ppv = NULL;

  if (IID_IAccessibleAction == iid) {
    *ppv = static_cast<IAccessibleAction*>(this);
    (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

// IAccessibleAction

STDMETHODIMP
CAccessibleAction::nActions(long* aActionCount)
{
__try {
  if (!aActionCount)
    return E_INVALIDARG;

  *aActionCount = 0;

  nsRefPtr<nsAccessible> acc(do_QueryObject(this));
  if (!acc || acc->IsDefunct())
    return E_FAIL;

  *aActionCount = acc->ActionCount();
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleAction::doAction(long aActionIndex)
{
__try {
  nsCOMPtr<nsIAccessible> acc(do_QueryObject(this));
  if (!acc)
    return E_FAIL;

  PRUint8 index = static_cast<PRUint8>(aActionIndex);
  nsresult rv = acc->DoAction(index);
  return GetHRESULT(rv);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleAction::get_description(long aActionIndex, BSTR *aDescription)
{
__try {
  *aDescription = NULL;

  nsCOMPtr<nsIAccessible> acc(do_QueryObject(this));
  if (!acc)
    return E_FAIL;

  nsAutoString description;
  PRUint8 index = static_cast<PRUint8>(aActionIndex);
  nsresult rv = acc->GetActionDescription(index, description);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  if (description.IsEmpty())
    return S_FALSE;

  *aDescription = ::SysAllocStringLen(description.get(),
                                      description.Length());
  return *aDescription ? S_OK : E_OUTOFMEMORY;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleAction::get_keyBinding(long aActionIndex, long aNumMaxBinding,
                                  BSTR **aKeyBinding,
                                  long *aNumBinding)
{
__try {
  if (!aKeyBinding)
    return E_INVALIDARG;
  *aKeyBinding = NULL;

  if (!aNumBinding)
    return E_INVALIDARG;
  *aNumBinding = 0;

  if (aActionIndex != 0 || aNumMaxBinding < 1)
    return E_INVALIDARG;

  nsRefPtr<nsAccessible> acc(do_QueryObject(this));
  if (!acc || acc->IsDefunct())
    return E_FAIL;

  // Expose keyboard shortcut if it's not exposed via MSAA keyboard shortcut.
  KeyBinding keyBinding = acc->AccessKey();
  if (keyBinding.IsEmpty())
    return S_FALSE;

  keyBinding = acc->KeyboardShortcut();
  if (keyBinding.IsEmpty())
    return S_FALSE;

  nsAutoString keyStr;
  keyBinding.ToString(keyStr);

  *aKeyBinding = static_cast<BSTR*>(::CoTaskMemAlloc(sizeof(BSTR*)));
  if (!*aKeyBinding)
    return E_OUTOFMEMORY;

  *(aKeyBinding[0]) = ::SysAllocStringLen(keyStr.get(), keyStr.Length());
  if (!*(aKeyBinding[0])) {
    ::CoTaskMemFree(*aKeyBinding);
    return E_OUTOFMEMORY;
  }

  *aNumBinding = 1;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleAction::get_name(long aActionIndex, BSTR *aName)
{
__try {
  *aName = NULL;

  nsCOMPtr<nsIAccessible> acc(do_QueryObject(this));
  if (!acc)
    return E_FAIL;

  nsAutoString name;
  PRUint8 index = static_cast<PRUint8>(aActionIndex);
  nsresult rv = acc->GetActionName(index, name);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  if (name.IsEmpty())
    return S_FALSE;

  *aName = ::SysAllocStringLen(name.get(), name.Length());
  return *aName ? S_OK : E_OUTOFMEMORY;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleAction::get_localizedName(long aActionIndex, BSTR *aLocalizedName)
{
__try {
  *aLocalizedName = NULL;
} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }

  return E_NOTIMPL;
}

