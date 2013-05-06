/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ia2AccessibleAction.h"

#include "AccessibleAction_i.c"

#include "AccessibleWrap.h"
#include "IUnknownImpl.h"

using namespace mozilla::a11y;

// IUnknown

STDMETHODIMP
ia2AccessibleAction::QueryInterface(REFIID iid, void** ppv)
{
  *ppv = nullptr;

  if (IID_IAccessibleAction == iid) {
    *ppv = static_cast<IAccessibleAction*>(this);
    (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

// IAccessibleAction

STDMETHODIMP
ia2AccessibleAction::nActions(long* aActionCount)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aActionCount)
    return E_INVALIDARG;

  *aActionCount = 0;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  *aActionCount = acc->ActionCount();
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleAction::doAction(long aActionIndex)
{
  A11Y_TRYBLOCK_BEGIN

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  uint8_t index = static_cast<uint8_t>(aActionIndex);
  nsresult rv = acc->DoAction(index);
  return GetHRESULT(rv);

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleAction::get_description(long aActionIndex, BSTR *aDescription)
{
  A11Y_TRYBLOCK_BEGIN

  *aDescription = nullptr;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsAutoString description;
  uint8_t index = static_cast<uint8_t>(aActionIndex);
  nsresult rv = acc->GetActionDescription(index, description);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  if (description.IsEmpty())
    return S_FALSE;

  *aDescription = ::SysAllocStringLen(description.get(),
                                      description.Length());
  return *aDescription ? S_OK : E_OUTOFMEMORY;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleAction::get_keyBinding(long aActionIndex, long aNumMaxBinding,
                                  BSTR **aKeyBinding,
                                  long *aNumBinding)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aKeyBinding)
    return E_INVALIDARG;
  *aKeyBinding = nullptr;

  if (!aNumBinding)
    return E_INVALIDARG;
  *aNumBinding = 0;

  if (aActionIndex != 0 || aNumMaxBinding < 1)
    return E_INVALIDARG;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

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

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleAction::get_name(long aActionIndex, BSTR *aName)
{
  A11Y_TRYBLOCK_BEGIN

  *aName = nullptr;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsAutoString name;
  uint8_t index = static_cast<uint8_t>(aActionIndex);
  nsresult rv = acc->GetActionName(index, name);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  if (name.IsEmpty())
    return S_FALSE;

  *aName = ::SysAllocStringLen(name.get(), name.Length());
  return *aName ? S_OK : E_OUTOFMEMORY;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleAction::get_localizedName(long aActionIndex, BSTR *aLocalizedName)
{
  A11Y_TRYBLOCK_BEGIN

  *aLocalizedName = nullptr;
  return E_NOTIMPL;

  A11Y_TRYBLOCK_END
}
