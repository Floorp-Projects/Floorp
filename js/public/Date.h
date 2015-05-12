/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_Date_h
#define js_Date_h

#include "mozilla/FloatingPoint.h"
#include "mozilla/MathAlgorithms.h"

#include "jstypes.h"

#include "js/Conversions.h"
#include "js/Value.h"

namespace JS {

class ClippedTime
{
    double t;

    /* ES5 15.9.1.14. */
    double timeClip(double time) {
        /* Steps 1-2. */
        const double MaxTimeMagnitude = 8.64e15;
        if (!mozilla::IsFinite(time) || mozilla::Abs(time) > MaxTimeMagnitude)
            return JS::GenericNaN();

        /* Step 3. */
        return JS::ToInteger(time) + (+0.0);
    }

  public:
    ClippedTime() : t(JS::GenericNaN()) {}
    explicit ClippedTime(double time) : t(timeClip(time)) {}

    static ClippedTime NaN() { return ClippedTime(); }

    double value() const { return t; }
};

inline ClippedTime
TimeClip(double d)
{
    return ClippedTime(d);
}

// Year is a year, month is 0-11, day is 1-based.  The return value is
// a number of milliseconds since the epoch.  Can return NaN.
JS_PUBLIC_API(double)
MakeDate(double year, unsigned month, unsigned day);

// Takes an integer number of milliseconds since the epoch and returns the
// year.  Can return NaN, and will do so if NaN is passed in.
JS_PUBLIC_API(double)
YearFromTime(double time);

// Takes an integer number of milliseconds since the epoch and returns the
// month (0-11).  Can return NaN, and will do so if NaN is passed in.
JS_PUBLIC_API(double)
MonthFromTime(double time);

// Takes an integer number of milliseconds since the epoch and returns the
// day (1-based).  Can return NaN, and will do so if NaN is passed in.
JS_PUBLIC_API(double)
DayFromTime(double time);

} // namespace JS

#endif /* js_Date_h */
