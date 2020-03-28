/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_freestanding_SafeThreadLocal_h
#define mozilla_freestanding_SafeThreadLocal_h

#include <type_traits>

#include "mozilla/NativeNt.h"
#include "mozilla/ThreadLocal.h"

namespace mozilla {
namespace freestanding {

// We cannot fall back to the Tls* APIs because kernel32 might not have been
// loaded yet.
#if defined(__MINGW32__) && !defined(HAVE_THREAD_TLS_KEYWORD)
#  error "This code requires the compiler to have native TLS support"
#endif  // defined(__MINGW32__) && !defined(HAVE_THREAD_TLS_KEYWORD)

/**
 * This class holds data as a thread-local variable, or as a global variable
 * if the thread local storage is not initialized yet.  It should be safe
 * because in that early stage we assume there is no more than a single thread.
 */
template <typename T>
class SafeThreadLocal final {
  static MOZ_THREAD_LOCAL(T) sThreadLocal;
  static T sGlobal;
  static bool sIsTlsUsed;

  // In normal cases, TLS is always available and the class uses sThreadLocal
  // without changing sMainThreadId.  So sMainThreadId is likely to be 0.
  //
  // If TLS is not available, we use sGlobal instead and update sMainThreadId
  // so that that thread keeps using sGlobal even after TLS is initialized
  // later.
  static DWORD sMainThreadId;

  // Need non-inline accessors to prevent the compiler from generating code
  // accessing sThreadLocal before checking a condition.
  MOZ_NEVER_INLINE static void SetGlobalValue(T aValue) { sGlobal = aValue; }
  MOZ_NEVER_INLINE static T GetGlobalValue() { return sGlobal; }

 public:
  static void set(T aValue) {
    static_assert(std::is_pointer_v<T>,
                  "SafeThreadLocal must be used with a pointer");

    if (sMainThreadId == mozilla::nt::RtlGetCurrentThreadId()) {
      SetGlobalValue(aValue);
    } else if (sIsTlsUsed) {
      MOZ_ASSERT(mozilla::nt::RtlGetThreadLocalStoragePointer(),
                 "Once TLS is used, TLS should be available till the end.");
      sThreadLocal.set(aValue);
    } else if (mozilla::nt::RtlGetThreadLocalStoragePointer()) {
      sIsTlsUsed = true;
      sThreadLocal.set(aValue);
    } else {
      MOZ_ASSERT(sMainThreadId == 0,
                 "A second thread cannot be created before TLS is available.");
      sMainThreadId = mozilla::nt::RtlGetCurrentThreadId();
      SetGlobalValue(aValue);
    }
  }

  static T get() {
    if (sMainThreadId == mozilla::nt::RtlGetCurrentThreadId()) {
      return GetGlobalValue();
    } else if (sIsTlsUsed) {
      return sThreadLocal.get();
    }
    return GetGlobalValue();
  }
};

template <typename T>
MOZ_THREAD_LOCAL(T)
SafeThreadLocal<T>::sThreadLocal;

template <typename T>
T SafeThreadLocal<T>::sGlobal = nullptr;

template <typename T>
bool SafeThreadLocal<T>::sIsTlsUsed = false;

template <typename T>
DWORD SafeThreadLocal<T>::sMainThreadId = 0;

}  // namespace freestanding
}  // namespace mozilla

#endif  // mozilla_freestanding_SafeThreadLocal_h
