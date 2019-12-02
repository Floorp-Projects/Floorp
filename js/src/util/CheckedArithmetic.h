/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef util_CheckedArithmetic_h
#define util_CheckedArithmetic_h

#include "mozilla/Attributes.h"
#include "mozilla/Compiler.h"

#include <stdint.h>

// This macro is should be `one' if current compiler supports builtin functions
// like __builtin_sadd_overflow.
#if MOZ_IS_GCC
// GCC supports these functions.
#  define BUILTIN_CHECKED_ARITHMETIC_SUPPORTED(x) 1
#else
// For CLANG, we use its own function to check for this.
#  ifdef __has_builtin
#    define BUILTIN_CHECKED_ARITHMETIC_SUPPORTED(x) __has_builtin(x)
#  endif
#endif
#ifndef BUILTIN_CHECKED_ARITHMETIC_SUPPORTED
#  define BUILTIN_CHECKED_ARITHMETIC_SUPPORTED(x) 0
#endif

namespace js {

MOZ_MUST_USE inline bool SafeAdd(int32_t one, int32_t two, int32_t* res) {
#if BUILTIN_CHECKED_ARITHMETIC_SUPPORTED(__builtin_sadd_overflow)
  // Using compiler's builtin function.
  return !__builtin_sadd_overflow(one, two, res);
#else
  // Use unsigned for the 32-bit operation since signed overflow gets
  // undefined behavior.
  *res = uint32_t(one) + uint32_t(two);
  int64_t ores = (int64_t)one + (int64_t)two;
  return ores == (int64_t)*res;
#endif
}

MOZ_MUST_USE inline bool SafeSub(int32_t one, int32_t two, int32_t* res) {
#if BUILTIN_CHECKED_ARITHMETIC_SUPPORTED(__builtin_ssub_overflow)
  return !__builtin_ssub_overflow(one, two, res);
#else
  *res = uint32_t(one) - uint32_t(two);
  int64_t ores = (int64_t)one - (int64_t)two;
  return ores == (int64_t)*res;
#endif
}

MOZ_MUST_USE inline bool SafeMul(int32_t one, int32_t two, int32_t* res) {
#if BUILTIN_CHECKED_ARITHMETIC_SUPPORTED(__builtin_smul_overflow)
  return !__builtin_smul_overflow(one, two, res);
#else
  *res = uint32_t(one) * uint32_t(two);
  int64_t ores = (int64_t)one * (int64_t)two;
  return ores == (int64_t)*res;
#endif
}

} /* namespace js */

#endif /* util_CheckedArithmetic_h */
