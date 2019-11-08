/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * PR assertion checker.
 */

#ifndef jsutil_h
#define jsutil_h

#include "mozilla/Assertions.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/MemoryChecking.h"
#include "mozilla/PodOperations.h"

#include <limits.h>

#include "js/Initialization.h"
#include "js/Utility.h"
#include "js/Value.h"
#include "util/BitArray.h"
#include "util/Poison.h"

/* Crash diagnostics by default in debug and on nightly channel. */
#if defined(DEBUG) || defined(NIGHTLY_BUILD)
#  define JS_CRASH_DIAGNOSTICS 1
#endif

#if defined(JS_DEBUG)
#  define JS_DIAGNOSTICS_ASSERT(expr) MOZ_ASSERT(expr)
#elif defined(JS_CRASH_DIAGNOSTICS)
#  define JS_DIAGNOSTICS_ASSERT(expr)         \
    do {                                      \
      if (MOZ_UNLIKELY(!(expr))) MOZ_CRASH(); \
    } while (0)
#else
#  define JS_DIAGNOSTICS_ASSERT(expr) ((void)0)
#endif

static MOZ_ALWAYS_INLINE void* js_memcpy(void* dst_, const void* src_,
                                         size_t len) {
  char* dst = (char*)dst_;
  const char* src = (const char*)src_;
  MOZ_ASSERT_IF(dst >= src, (size_t)(dst - src) >= len);
  MOZ_ASSERT_IF(src >= dst, (size_t)(src - dst) >= len);

  return memcpy(dst, src, len);
}

namespace js {

// An internal version of JS_IsInitialized() that returns whether SpiderMonkey
// is currently initialized or is in the process of being initialized.
inline bool IsInitialized() {
  using namespace JS::detail;
  return libraryInitState == InitState::Initializing ||
         libraryInitState == InitState::Running;
}

template <class T>
static constexpr inline T Min(T t1, T t2) {
  return t1 < t2 ? t1 : t2;
}

template <class T>
static constexpr inline T Max(T t1, T t2) {
  return t1 > t2 ? t1 : t2;
}

template <typename T, typename U>
static constexpr U ComputeByteAlignment(T bytes, U alignment) {
  static_assert(mozilla::IsUnsigned<U>::value,
                "alignment amount must be unsigned");

  return (alignment - (bytes % alignment)) % alignment;
}

template <typename T, typename U>
static constexpr T AlignBytes(T bytes, U alignment) {
  static_assert(mozilla::IsUnsigned<U>::value,
                "alignment amount must be unsigned");

  return bytes + ComputeByteAlignment(bytes, alignment);
}

/*****************************************************************************/

// Placement-new elements of an array. This should optimize away for types with
// trivial default initiation.
template <typename T>
static void DefaultInitializeElements(void* arrayPtr, size_t length) {
  uintptr_t elem = reinterpret_cast<uintptr_t>(arrayPtr);
  MOZ_ASSERT(elem % alignof(T) == 0);

  for (size_t i = 0; i < length; ++i) {
    new (reinterpret_cast<void*>(elem)) T;
    elem += sizeof(T);
  }
}

} /* namespace js */

#endif /* jsutil_h */
