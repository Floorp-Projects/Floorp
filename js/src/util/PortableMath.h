/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef util_PortableMath_h
#define util_PortableMath_h

#include "mozilla/FloatingPoint.h"

#include <math.h>

#include "vm/JSContext.h"  // for js::AutoUnsafeCallWithABI

/*
 * Use system provided math routines.
 */

/* The right copysign function is not always named the same thing. */
#ifdef __GNUC__
    #define js_copysign __builtin_copysign
#elif defined _WIN32
    #define js_copysign _copysign
#else
    #define js_copysign copysign
#endif

// This macro is should be `one' if current compiler supports builtin functions
// like __builtin_sadd_overflow.
#if __GNUC__ >= 5
    // GCC 5 and above supports these functions.
    #define BUILTIN_CHECKED_ARITHMETIC_SUPPORTED(x) 1
#else
    // For CLANG, we use its own function to check for this.
    #ifdef __has_builtin
        #define BUILTIN_CHECKED_ARITHMETIC_SUPPORTED(x) __has_builtin(x)
    #endif
#endif
#ifndef BUILTIN_CHECKED_ARITHMETIC_SUPPORTED
    #define BUILTIN_CHECKED_ARITHMETIC_SUPPORTED(x) 0
#endif

namespace js {

MOZ_MUST_USE inline bool
SafeAdd(int32_t one, int32_t two, int32_t* res)
{
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

MOZ_MUST_USE inline bool
SafeSub(int32_t one, int32_t two, int32_t* res)
{
#if BUILTIN_CHECKED_ARITHMETIC_SUPPORTED(__builtin_ssub_overflow)
    return !__builtin_ssub_overflow(one, two, res);
#else
    *res = uint32_t(one) - uint32_t(two);
    int64_t ores = (int64_t)one - (int64_t)two;
    return ores == (int64_t)*res;
#endif
}

MOZ_MUST_USE inline bool
SafeMul(int32_t one, int32_t two, int32_t* res)
{
#if BUILTIN_CHECKED_ARITHMETIC_SUPPORTED(__builtin_smul_overflow)
    return !__builtin_smul_overflow(one, two, res);
#else
    *res = uint32_t(one) * uint32_t(two);
    int64_t ores = (int64_t)one * (int64_t)two;
    return ores == (int64_t)*res;
#endif
}

/**
 * ES-compliant floating-point division.
 * <https://tc39.github.io/ecma262/#sec-applying-the-div-operator>
 */
inline double
NumberDiv(double a, double b)
{
    AutoUnsafeCallWithABI unsafe;
    if (b == 0) {
        if (a == 0 || mozilla::IsNaN(a)
#ifdef XP_WIN
            || mozilla::IsNaN(b) /* XXX MSVC miscompiles such that (NaN == 0) */
#endif
        )
            return JS::GenericNaN();

        if (mozilla::IsNegative(a) != mozilla::IsNegative(b))
            return mozilla::NegativeInfinity<double>();
        return mozilla::PositiveInfinity<double>();
    }

    return a / b;
}

/**
 * ES-compliant floating-point remainder operation.
 * <https://tc39.github.io/ecma262/#sec-applying-the-mod-operator>
 */
inline double
NumberMod(double a, double b)
{
    AutoUnsafeCallWithABI unsafe;
    if (b == 0)
        return JS::GenericNaN();

#ifdef XP_WIN
    /*
     * Workaround MS fmod bug where 42 % (1/0) => NaN, not 42.
     * Workaround MS fmod bug where -0 % -N => 0, not -0.
     */
    if ((mozilla::IsFinite(a) && mozilla::IsInfinite(b)) ||
        (a == 0 && mozilla::IsFinite(b)))
    {
        return a;
    }
#endif

    return fmod(a, b);
}

} // namespace js

#endif /* util_PortableMath_h */
