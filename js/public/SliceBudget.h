/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_SliceBudget_h
#define js_SliceBudget_h

#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Variant.h"

#include <stdint.h>

#include "jstypes.h"

namespace js {

struct JS_PUBLIC_API TimeBudget {
  const int64_t budget;
  mozilla::TimeStamp deadline;  // Calculated when SliceBudget is constructed.

  explicit TimeBudget(int64_t milliseconds) : budget(milliseconds) {}
  explicit TimeBudget(mozilla::TimeDuration duration)
      : TimeBudget(duration.ToMilliseconds()) {}
};

struct JS_PUBLIC_API WorkBudget {
  const int64_t budget;

  explicit WorkBudget(int64_t work) : budget(work) {}
};

struct UnlimitedBudget {};

/*
 * This class describes a limit to the amount of work to be performed in a GC
 * slice, so that we can return to the mutator without pausing for too long. The
 * budget may be based on a deadline time or an amount of work to be performed,
 * or may be unlimited.
 *
 * To reduce the number of gettimeofday calls, we only check the time every 1000
 * operations.
 */
class JS_PUBLIC_API SliceBudget {
 public:
  using InterruptRequestFlag = mozilla::Atomic<bool>;

 private:
  static const intptr_t UnlimitedCounter = INTPTR_MAX;

  // Most calls to isOverBudget will only check the counter value. Every N
  // steps, do a more "expensive" check -- look at the current time and/or
  // check the atomic interrupt flag.
  static constexpr intptr_t StepsPerExpensiveCheck = 1000;

  // Configuration

  mozilla::Variant<TimeBudget, WorkBudget, UnlimitedBudget> budget;

  // External flag to request the current slice to be interrupted
  // (and return isOverBudget() early.) Applies only to time-based budgets.
  InterruptRequestFlag* interruptRequested = nullptr;

  // How many steps to count before checking the time and possibly the interrupt
  // flag.
  int64_t counter = StepsPerExpensiveCheck;

  // This SliceBudget is considered interrupted from the time isOverBudget()
  // finds the interrupt flag set, to the next time resetOverBudget() (or
  // checkAndResetOverBudget()) is called.
  bool interrupted = false;

  explicit SliceBudget(InterruptRequestFlag* irqPtr)
      : budget(UnlimitedBudget()),
        interruptRequested(irqPtr),
        counter(irqPtr ? StepsPerExpensiveCheck : UnlimitedCounter) {}

  bool checkOverBudget();

 public:
  // Use to create an unlimited budget.
  static SliceBudget unlimited() { return SliceBudget(nullptr); }

  // Instantiate as SliceBudget(TimeBudget(n)).
  explicit SliceBudget(TimeBudget time,
                       InterruptRequestFlag* interrupt = nullptr);

  explicit SliceBudget(mozilla::TimeDuration duration,
                       InterruptRequestFlag* interrupt = nullptr)
      : SliceBudget(TimeBudget(duration.ToMilliseconds()), interrupt) {}

  // Instantiate as SliceBudget(WorkBudget(n)).
  explicit SliceBudget(WorkBudget work);

  // Register having performed the given number of steps (counted against a
  // work budget, or progress towards the next time or callback check).
  void step(uint64_t steps = 1) {
    MOZ_ASSERT(steps > 0);
    counter -= steps;
  }

  // Do enough steps to force an "expensive" (time and/or callback) check on
  // the next call to isOverBudget. Useful when switching between major phases
  // of an operation like a cycle collection.
  void stepAndForceCheck() {
    if (!isUnlimited()) {
      counter = 0;
    }
  }

  bool isOverBudget() { return counter <= 0 && checkOverBudget(); }

  // Normally not used. Reset the SliceBudget to its initial state.
  // Note that resetting the interrupt request flag could race with
  // anything that is setting it, causing the interrupt to be missed.
  void reset() {
    if (isTimeBudget()) {
      counter = timeBudget();
    } else if (isWorkBudget()) {
      counter = workBudget();
    }
    if (interruptRequested) {
      *interruptRequested = false;
    }
  }

  bool isWorkBudget() const { return budget.is<WorkBudget>(); }
  bool isTimeBudget() const { return budget.is<TimeBudget>(); }
  bool isUnlimited() const { return budget.is<UnlimitedBudget>(); }

  int64_t timeBudget() const { return budget.as<TimeBudget>().budget; }
  int64_t workBudget() const { return budget.as<WorkBudget>().budget; }

  mozilla::TimeStamp deadline() const {
    return budget.as<TimeBudget>().deadline;
  }

  int describe(char* buffer, size_t maxlen) const;
};

}  // namespace js

#endif /* js_SliceBudget_h */
