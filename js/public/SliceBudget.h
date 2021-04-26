/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_SliceBudget_h
#define js_SliceBudget_h

#include "mozilla/Assertions.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Variant.h"

#include <stdint.h>

#include "jstypes.h"

namespace js {

struct JS_PUBLIC_API TimeBudget {
  const int64_t budget;
  mozilla::TimeStamp deadline;  // Calculated when SliceBudget is constructed.

  explicit TimeBudget(int64_t milliseconds) : budget(milliseconds) {}
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
  static const intptr_t UnlimitedCounter = INTPTR_MAX;

  mozilla::Variant<TimeBudget, WorkBudget, UnlimitedBudget> budget;
  int64_t counter;

  SliceBudget() : budget(UnlimitedBudget()), counter(UnlimitedCounter) {}

  bool checkOverBudget();

 public:
  static const intptr_t StepsPerTimeCheck = 1000;

  // Use to create an unlimited budget.
  static SliceBudget unlimited() { return SliceBudget(); }

  // Instantiate as SliceBudget(TimeBudget(n)).
  explicit SliceBudget(TimeBudget time);

  // Instantiate as SliceBudget(WorkBudget(n)).
  explicit SliceBudget(WorkBudget work);

  explicit SliceBudget(mozilla::TimeDuration time)
      : SliceBudget(TimeBudget(time.ToMilliseconds())) {}

  void step(uint64_t steps = 1) {
    MOZ_ASSERT(steps > 0);
    counter -= steps;
  }

  bool isOverBudget() { return counter <= 0 && checkOverBudget(); }

  bool isWorkBudget() const { return budget.is<WorkBudget>(); }
  bool isTimeBudget() const { return budget.is<TimeBudget>(); }
  bool isUnlimited() const { return budget.is<UnlimitedBudget>(); }

  int64_t timeBudget() const { return budget.as<TimeBudget>().budget; }
  int64_t workBudget() const { return budget.as<WorkBudget>().budget; }

  int describe(char* buffer, size_t maxlen) const;
};

}  // namespace js

#endif /* js_SliceBudget_h */
