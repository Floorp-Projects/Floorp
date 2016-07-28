/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// We need Windows 7 headers
#ifdef NTDDI_VERSION
#undef NTDDI_VERSION
#endif
#define NTDDI_VERSION 0x06010000
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0601

#include "DynamicallyLinkedFunctionPtr.h"
#include "mozilla/mscom/Utils.h"
#include "mozilla/RefPtr.h"

#include <objidl.h>

namespace mozilla {
namespace mscom {

bool
IsCurrentThreadMTA()
{
  static DynamicallyLinkedFunctionPtr<decltype(&::CoGetApartmentType)>
    pCoGetApartmentType(L"ole32.dll", "CoGetApartmentType");

  // There isn't really a thread-safe way to query this on Windows XP. In that
  // case, we'll just return false since that assumption does no harm.
  if (!pCoGetApartmentType) {
    return false;
  }

  APTTYPE aptType;
  APTTYPEQUALIFIER aptTypeQualifier;
  HRESULT hr = pCoGetApartmentType(&aptType, &aptTypeQualifier);
  if (FAILED(hr)) {
    return false;
  }

  return aptType == APTTYPE_MTA;
}

bool
IsProxy(IUnknown* aUnknown)
{
  if (!aUnknown) {
    return false;
  }

  // Only proxies implement this interface, so if it is present then we must
  // be dealing with a proxied object.
  RefPtr<IClientSecurity> clientSecurity;
  HRESULT hr = aUnknown->QueryInterface(IID_IClientSecurity,
                                        (void**)getter_AddRefs(clientSecurity));
  if (SUCCEEDED(hr) || hr == RPC_E_WRONG_THREAD) {
    return true;
  }
  return false;
}

} // namespace mscom
} // namespace mozilla
