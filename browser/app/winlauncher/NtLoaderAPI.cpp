/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "mozilla/LoaderAPIInterfaces.h"

#include "freestanding/LoaderPrivateAPI.h"

#if defined(_MSC_VER)
#  include <intrin.h>
#  pragma intrinsic(_ReturnAddress)
#  define RETURN_ADDRESS() _ReturnAddress()
#elif defined(__GNUC__) || defined(__clang__)
#  define RETURN_ADDRESS() \
    __builtin_extract_return_addr(__builtin_return_address(0))
#endif

static bool CheckForMozglue(void* aReturnAddress) {
  HMODULE callingModule;
  if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(aReturnAddress),
                            &callingModule)) {
    return false;
  }

  return callingModule && callingModule == ::GetModuleHandleW(L"mozglue.dll");
}

namespace mozilla {

extern "C" MOZ_EXPORT nt::LoaderAPI* GetNtLoaderAPI(
    nt::LoaderObserver* aNewObserver) {
  const bool isCallerMozglue = CheckForMozglue(RETURN_ADDRESS());
  MOZ_ASSERT(isCallerMozglue);
  if (!isCallerMozglue) {
    return nullptr;
  }

  freestanding::EnsureInitialized();
  freestanding::LoaderPrivateAPI& api = freestanding::gLoaderPrivateAPI;
  api.SetObserver(aNewObserver);

  return &api;
}

}  // namespace mozilla
