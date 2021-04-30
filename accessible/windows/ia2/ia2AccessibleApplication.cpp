/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ia2AccessibleApplication.h"

#include "AccessibleApplication_i.c"
#include "ApplicationAccessibleWrap.h"

using namespace mozilla;
using namespace mozilla::a11y;

ApplicationAccessible* ia2AccessibleApplication::AppAcc() {
  // XXX This first static_cast is a necessary hack until we get rid of the
  // inheritance of ApplicationAccessibleWrap.
  auto wrap = static_cast<ApplicationAccessibleWrap*>(this);
  AccessibleWrap* acc = static_cast<MsaaAccessible*>(wrap)->LocalAcc();
  return static_cast<ApplicationAccessible*>(acc);
}

////////////////////////////////////////////////////////////////////////////////
// IUnknown

STDMETHODIMP
ia2AccessibleApplication::QueryInterface(REFIID iid, void** ppv) {
  if (!ppv) return E_INVALIDARG;

  *ppv = nullptr;

  if (IID_IAccessibleApplication == iid) {
    *ppv = static_cast<IAccessibleApplication*>(this);
    (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

////////////////////////////////////////////////////////////////////////////////
// IAccessibleApplication

STDMETHODIMP
ia2AccessibleApplication::get_appName(BSTR* aName) {
  if (!aName) return E_INVALIDARG;

  *aName = nullptr;

  ApplicationAccessible* appAcc = AppAcc();
  if (!appAcc) return CO_E_OBJNOTCONNECTED;

  nsAutoString name;
  appAcc->AppName(name);
  if (name.IsEmpty()) return S_FALSE;

  *aName = ::SysAllocStringLen(name.get(), name.Length());
  return *aName ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
ia2AccessibleApplication::get_appVersion(BSTR* aVersion) {
  if (!aVersion) return E_INVALIDARG;

  *aVersion = nullptr;

  ApplicationAccessible* appAcc = AppAcc();
  if (!appAcc) return CO_E_OBJNOTCONNECTED;

  nsAutoString version;
  appAcc->AppVersion(version);
  if (version.IsEmpty()) return S_FALSE;

  *aVersion = ::SysAllocStringLen(version.get(), version.Length());
  return *aVersion ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
ia2AccessibleApplication::get_toolkitName(BSTR* aName) {
  if (!aName) return E_INVALIDARG;

  ApplicationAccessible* appAcc = AppAcc();
  if (!appAcc) return CO_E_OBJNOTCONNECTED;

  nsAutoString name;
  appAcc->PlatformName(name);
  if (name.IsEmpty()) return S_FALSE;

  *aName = ::SysAllocStringLen(name.get(), name.Length());
  return *aName ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
ia2AccessibleApplication::get_toolkitVersion(BSTR* aVersion) {
  if (!aVersion) return E_INVALIDARG;

  *aVersion = nullptr;

  ApplicationAccessible* appAcc = AppAcc();
  if (!appAcc) return CO_E_OBJNOTCONNECTED;

  nsAutoString version;
  appAcc->PlatformVersion(version);
  if (version.IsEmpty()) return S_FALSE;

  *aVersion = ::SysAllocStringLen(version.get(), version.Length());
  return *aVersion ? S_OK : E_OUTOFMEMORY;
}
