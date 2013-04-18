/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _LIBMATH_H
#define _LIBMATH_H

#include "mozilla/FloatingPoint.h"

#include <math.h>
#include "jsnum.h"

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

#if defined(_M_X64) && defined(_MSC_VER) && _MSC_VER <= 1500
// This is a workaround for fmod bug (http://support.microsoft.com/kb/982107)
extern "C" double js_myfmod(double x, double y);
#define fmod js_myfmod
#endif

/* Consistency wrapper for platform deviations in fmod() */
static inline double
js_fmod(double d, double d2)
{
#ifdef XP_WIN
    /*
     * Workaround MS fmod bug where 42 % (1/0) => NaN, not 42.
     * Workaround MS fmod bug where -0 % -N => 0, not -0.
     */
    if ((MOZ_DOUBLE_IS_FINITE(d) && MOZ_DOUBLE_IS_INFINITE(d2)) ||
        (d == 0 && MOZ_DOUBLE_IS_FINITE(d2))) {
        return d;
    }
#endif
    return fmod(d, d2);
}

namespace js {

inline double
NumberDiv(double a, double b)
{
    if (b == 0) {
        if (a == 0 || MOZ_DOUBLE_IS_NaN(a)
#ifdef XP_WIN
            || MOZ_DOUBLE_IS_NaN(b) /* XXX MSVC miscompiles such that (NaN == 0) */
#endif
        )
            return js_NaN;

        if (MOZ_DOUBLE_IS_NEGATIVE(a) != MOZ_DOUBLE_IS_NEGATIVE(b))
            return js_NegativeInfinity;
        return js_PositiveInfinity;
    }

    return a / b;
}

inline double
NumberMod(double a, double b) {
    if (b == 0) 
        return js_NaN;
    return js_fmod(a, b);
}

}

#endif /* _LIBMATH_H */

