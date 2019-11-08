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
#include "util/Memory.h"
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

} /* namespace js */

#endif /* jsutil_h */
