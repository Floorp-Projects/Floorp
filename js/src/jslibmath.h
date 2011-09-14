/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   IBM Corp.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _LIBMATH_H
#define _LIBMATH_H

#include <math.h>
#include "jsnum.h"

/*
 * Use system provided math routines.
 */

/* The right copysign function is not always named the same thing. */
#if __GNUC__ >= 4 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
#define js_copysign __builtin_copysign
#elif defined _WIN32
#if _MSC_VER < 1400
/* Try to work around apparent _copysign bustage in VC7.x. */
#define js_copysign js_copysign
extern double js_copysign(double, double);
#else
#define js_copysign _copysign
#endif
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
    if ((JSDOUBLE_IS_FINITE(d) && JSDOUBLE_IS_INFINITE(d2)) ||
        (d == 0 && JSDOUBLE_IS_FINITE(d2))) {
        return d;
    }
#endif
    return fmod(d, d2);
}

namespace js {

inline double
NumberDiv(double a, double b) {
    if (b == 0) {
        if (a == 0 || JSDOUBLE_IS_NaN(a) 
#ifdef XP_WIN
            || JSDOUBLE_IS_NaN(a) /* XXX MSVC miscompiles such that (NaN == 0) */
#endif
        )
            return js_NaN;    
        else if (JSDOUBLE_IS_NEG(a) != JSDOUBLE_IS_NEG(b))
            return js_NegativeInfinity;
        else
            return js_PositiveInfinity; 
    }

    return a / b;
}

}

#endif /* _LIBMATH_H */

