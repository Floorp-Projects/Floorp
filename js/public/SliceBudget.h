/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_SliceBudget_h
#define js_SliceBudget_h

#include "mozilla/Atomics.h"

#include <stdint.h>

namespace js {

struct JS_PUBLIC_API(TimeBudget)
{
    int64_t budget;

    explicit TimeBudget(int64_t milliseconds) { budget = milliseconds; }
};

struct JS_PUBLIC_API(WorkBudget)
{
    int64_t budget;

    explicit WorkBudget(int64_t work) { budget = work; }
};

/*
 * This class records how much work has been done in a given collection slice,
 * so that we can return before pausing for too long. Some slices are allowed
 * to run for unlimited time, and others are bounded. To reduce the number of
 * gettimeofday calls, we only check the time every 1000 operations.
 */
class JS_PUBLIC_API(SliceBudget)
{
    static const int64_t unlimitedDeadline = INT64_MAX;
    static const intptr_t unlimitedStartCounter = INTPTR_MAX;

    bool checkOverBudget(JSContext* maybeCx);

    SliceBudget();

  public:
    // Memory of the originally requested budget. If isUnlimited, neither of
    // these are in use. If deadline==0, then workBudget is valid. Otherwise
    // timeBudget is valid.
    TimeBudget timeBudget;
    WorkBudget workBudget;

    int64_t deadline; /* in microseconds */
    mozilla::Atomic<intptr_t, mozilla::Relaxed> counter;

    static const intptr_t CounterReset = 1000;

    static const int64_t UnlimitedTimeBudget = -1;
    static const int64_t UnlimitedWorkBudget = -1;

    /* Use to create an unlimited budget. */
    static SliceBudget unlimited() { return SliceBudget(); }

    /* Instantiate as SliceBudget(TimeBudget(n)). */
    explicit SliceBudget(TimeBudget time);

    /* Instantiate as SliceBudget(WorkBudget(n)). */
    explicit SliceBudget(WorkBudget work);

    // Need an explicit copy constructor because Atomic fails to provide one.
    SliceBudget(const SliceBudget& other)
        : timeBudget(other.timeBudget),
          workBudget(other.workBudget),
          deadline(other.deadline),
          counter(other.counter)
    {}

    // Need an explicit operator= because Atomic fails to provide one.
    SliceBudget& operator=(const SliceBudget& other) {
        timeBudget = other.timeBudget;
        workBudget = other.workBudget;
        deadline = other.deadline;
        counter = intptr_t(other.counter);
        return *this;
    }

    void makeUnlimited() {
        deadline = unlimitedDeadline;
        counter = unlimitedStartCounter;
    }

    // Request that checkOverBudget be called the next time isOverBudget is
    // called.
    void requestFullCheck() {
        counter = 0;
    }

    void step(intptr_t amt = 1) {
        counter -= amt;
    }

    // Only need to pass maybeCx if the GC interrupt callback should be checked
    // (and possibly invoked).
    bool isOverBudget(JSContext* maybeCx = nullptr) {
        if (counter > 0)
            return false;
        return checkOverBudget(maybeCx);
    }

    bool isWorkBudget() const { return deadline == 0; }
    bool isTimeBudget() const { return deadline > 0 && !isUnlimited(); }
    bool isUnlimited() const { return deadline == unlimitedDeadline; }

    int describe(char* buffer, size_t maxlen) const;
};

} // namespace js

#endif /* js_SliceBudget_h */
