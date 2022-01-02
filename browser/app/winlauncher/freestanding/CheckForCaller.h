/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_freestanding_CheckForCaller_h
#define mozilla_freestanding_CheckForCaller_h

namespace mozilla {

#if defined(_MSC_VER)
#  include <intrin.h>
#  pragma intrinsic(_ReturnAddress)
#  define RETURN_ADDRESS() _ReturnAddress()
#elif defined(__GNUC__) || defined(__clang__)
#  define RETURN_ADDRESS() \
    __builtin_extract_return_addr(__builtin_return_address(0))
#endif

template <int N>
bool CheckForAddress(void* aReturnAddress, const wchar_t (&aName)[N]) {
  HMODULE callingModule;
  if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(aReturnAddress),
                            &callingModule)) {
    return false;
  }

  return callingModule && callingModule == ::GetModuleHandleW(aName);
}

}  // namespace mozilla

#endif  // mozilla_freestanding_CheckForCaller_h
