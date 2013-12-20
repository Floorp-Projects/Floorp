/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_SliceBudget_h
#define js_SliceBudget_h

#include <stdint.h>

namespace js {

/*
 * This class records how much work has been done in a given collection slice, so that
 * we can return before pausing for too long. Some slices are allowed to run for
 * unlimited time, and others are bounded. To reduce the number of gettimeofday
 * calls, we only check the time every 1000 operations.
 */
struct JS_PUBLIC_API(SliceBudget)
{
    int64_t deadline; /* in microseconds */
    intptr_t counter;

    static const intptr_t CounterReset = 1000;

    static const int64_t Unlimited = 0;
    static int64_t TimeBudget(int64_t millis);
    static int64_t WorkBudget(int64_t work);

    /* Equivalent to SliceBudget(UnlimitedBudget). */
    SliceBudget();

    /* Instantiate as SliceBudget(Time/WorkBudget(n)). */
    SliceBudget(int64_t budget);

    void reset() {
        deadline = INT64_MAX;
        counter = INTPTR_MAX;
    }

    void step(intptr_t amt = 1) {
        counter -= amt;
    }

    bool checkOverBudget();

    bool isOverBudget() {
        if (counter >= 0)
            return false;
        return checkOverBudget();
    }
};

} // namespace js

#endif /* js_SliceBudget_h */
