/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/SliceBudget.h"
#include "jsapi-tests/tests.h"

using namespace js;

BEGIN_TEST(testSliceBudgetUnlimited) {
  SliceBudget budget = SliceBudget::unlimited();
  CHECK(budget.isUnlimited());
  CHECK(!budget.isTimeBudget());
  CHECK(!budget.isWorkBudget());

  CHECK(!budget.isOverBudget());

  budget.step(1000000);
  CHECK(!budget.isOverBudget());

  return true;
}
END_TEST(testSliceBudgetUnlimited)

BEGIN_TEST(testSliceBudgetWork) {
  SliceBudget budget = SliceBudget(WorkBudget(10000));
  CHECK(!budget.isUnlimited());
  CHECK(budget.isWorkBudget());
  CHECK(!budget.isTimeBudget());

  CHECK(budget.workBudget() == 10000);

  CHECK(!budget.isOverBudget());

  budget.step(5000);
  CHECK(!budget.isOverBudget());

  budget.step(5000);
  CHECK(budget.isOverBudget());

  return true;
}
END_TEST(testSliceBudgetWork)

BEGIN_TEST(testSliceBudgetTime) {
  SliceBudget budget = SliceBudget(TimeBudget(10000));
  CHECK(!budget.isUnlimited());
  CHECK(!budget.isWorkBudget());
  CHECK(budget.isTimeBudget());

  CHECK(budget.timeBudget() == 10000);

  CHECK(!budget.isOverBudget());

  budget.step(5000);
  budget.step(5000);
  CHECK(!budget.isOverBudget());

  // This doesn't test the deadline is correct as that would require waiting.

  return true;
}
END_TEST(testSliceBudgetTime)

BEGIN_TEST(testSliceBudgetTimeZero) {
  SliceBudget budget = SliceBudget(TimeBudget(0));
  budget.step(1000);
  CHECK(budget.isOverBudget());

  return true;
}
END_TEST(testSliceBudgetTimeZero)

BEGIN_TEST(testSliceBudgetInterruptibleTime) {
  SliceBudget::InterruptRequestFlag wantInterrupt(false);

  // Interruptible 100 second budget. This test will finish in well under that
  // time.
  static constexpr int64_t LONG_TIME = 100000;
  SliceBudget budget = SliceBudget(TimeBudget(LONG_TIME), &wantInterrupt);
  CHECK(!budget.isUnlimited());
  CHECK(!budget.isWorkBudget());
  CHECK(budget.isTimeBudget());

  CHECK(budget.timeBudget() == LONG_TIME);

  CHECK(!budget.isOverBudget());

  // We do a little work, very small amount of time passes.
  budget.step(500);

  // Not enough work to check interrupt, and no interrupt anyway.
  CHECK(!budget.isOverBudget());

  // External signal: interrupt requested.
  wantInterrupt = true;

  // Interrupt requested, but not enough work has been done to check for it.
  CHECK(!budget.isOverBudget());

  // Do enough work for an expensive check.
  budget.step(1000);

  // Interrupt requested!
  CHECK(budget.isOverBudget());

  // The external flag is not reset, but the budget will internally remember
  // that an interrupt was requested.
  CHECK(wantInterrupt);
  wantInterrupt = false;
  CHECK(budget.isOverBudget());

  // This doesn't test the deadline is correct as that would require waiting.

  return true;
}
END_TEST(testSliceBudgetInterruptibleTime)
