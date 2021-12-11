/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mscom/COMWrappers.h"

#include <objbase.h>

#include "mozilla/Assertions.h"
#include "mozilla/DynamicallyLinkedFunctionPtr.h"

namespace mozilla::mscom::wrapped {

HRESULT CoInitializeEx(LPVOID pvReserved, DWORD dwCoInit) {
  static const StaticDynamicallyLinkedFunctionPtr<decltype(&::CoInitializeEx)>
      pCoInitializeEx(L"combase.dll", "CoInitializeEx");
  if (!pCoInitializeEx) {
    return ::CoInitializeEx(pvReserved, dwCoInit);
  }

  return pCoInitializeEx(pvReserved, dwCoInit);
}

void CoUninitialize() {
  static const StaticDynamicallyLinkedFunctionPtr<decltype(&::CoUninitialize)>
      pCoUninitialize(L"combase.dll", "CoUninitialize");
  if (!pCoUninitialize) {
    return ::CoUninitialize();
  }

  return pCoUninitialize();
}

HRESULT CoIncrementMTAUsage(CO_MTA_USAGE_COOKIE* pCookie) {
  static const StaticDynamicallyLinkedFunctionPtr<
      decltype(&::CoIncrementMTAUsage)>
      pCoIncrementMTAUsage(L"combase.dll", "CoIncrementMTAUsage");
  // This API is only available beginning with Windows 8.
  if (!pCoIncrementMTAUsage) {
    return E_NOTIMPL;
  }

  HRESULT hr = pCoIncrementMTAUsage(pCookie);
  MOZ_ASSERT(SUCCEEDED(hr));
  return hr;
}

HRESULT CoGetApartmentType(APTTYPE* pAptType, APTTYPEQUALIFIER* pAptQualifier) {
  static const StaticDynamicallyLinkedFunctionPtr<
      decltype(&::CoGetApartmentType)>
      pCoGetApartmentType(L"combase.dll", "CoGetApartmentType");
  if (!pCoGetApartmentType) {
    return ::CoGetApartmentType(pAptType, pAptQualifier);
  }

  return pCoGetApartmentType(pAptType, pAptQualifier);
}

HRESULT CoInitializeSecurity(PSECURITY_DESCRIPTOR pSecDesc, LONG cAuthSvc,
                             SOLE_AUTHENTICATION_SERVICE* asAuthSvc,
                             void* pReserved1, DWORD dwAuthnLevel,
                             DWORD dwImpLevel, void* pAuthList,
                             DWORD dwCapabilities, void* pReserved3) {
  static const StaticDynamicallyLinkedFunctionPtr<
      decltype(&::CoInitializeSecurity)>
      pCoInitializeSecurity(L"combase.dll", "CoInitializeSecurity");
  if (!pCoInitializeSecurity) {
    return ::CoInitializeSecurity(pSecDesc, cAuthSvc, asAuthSvc, pReserved1,
                                  dwAuthnLevel, dwImpLevel, pAuthList,
                                  dwCapabilities, pReserved3);
  }

  return pCoInitializeSecurity(pSecDesc, cAuthSvc, asAuthSvc, pReserved1,
                               dwAuthnLevel, dwImpLevel, pAuthList,
                               dwCapabilities, pReserved3);
}

HRESULT CoCreateInstance(REFCLSID rclsid, LPUNKNOWN pUnkOuter,
                         DWORD dwClsContext, REFIID riid, LPVOID* ppv) {
  static const StaticDynamicallyLinkedFunctionPtr<decltype(&::CoCreateInstance)>
      pCoCreateInstance(L"combase.dll", "CoCreateInstance");
  if (!pCoCreateInstance) {
    return ::CoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv);
  }

  return pCoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv);
}

HRESULT CoCreateGuid(GUID* pguid) {
  static const StaticDynamicallyLinkedFunctionPtr<decltype(&::CoCreateGuid)>
      pCoCreateGuid(L"combase.dll", "CoCreateGuid");
  if (!pCoCreateGuid) {
    return ::CoCreateGuid(pguid);
  }

  return pCoCreateGuid(pguid);
}

}  // namespace mozilla::mscom::wrapped
