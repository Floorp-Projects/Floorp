/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_COMWrappers_h
#define mozilla_mscom_COMWrappers_h

#include <objbase.h>

// A set of wrapped COM functions, so that we can dynamically link to the
// functions in combase.dll on win8+. This prevents ole32.dll and many other
// DLLs loading, which are not required when we have win32k locked down.
namespace mozilla::mscom::wrapped {

HRESULT CoInitializeEx(LPVOID pvReserved, DWORD dwCoInit);

void CoUninitialize();

HRESULT CoIncrementMTAUsage(CO_MTA_USAGE_COOKIE* pCookie);

HRESULT CoGetApartmentType(APTTYPE* pAptType, APTTYPEQUALIFIER* pAptQualifier);

HRESULT CoInitializeSecurity(PSECURITY_DESCRIPTOR pSecDesc, LONG cAuthSvc,
                             SOLE_AUTHENTICATION_SERVICE* asAuthSvc,
                             void* pReserved1, DWORD dwAuthnLevel,
                             DWORD dwImpLevel, void* pAuthList,
                             DWORD dwCapabilities, void* pReserved3);

HRESULT CoCreateInstance(REFCLSID rclsid, LPUNKNOWN pUnkOuter,
                         DWORD dwClsContext, REFIID riid, LPVOID* ppv);

HRESULT CoCreateGuid(GUID* pguid);

}  // namespace mozilla::mscom::wrapped

#endif  // mozilla_mscom_COMWrappers_h
