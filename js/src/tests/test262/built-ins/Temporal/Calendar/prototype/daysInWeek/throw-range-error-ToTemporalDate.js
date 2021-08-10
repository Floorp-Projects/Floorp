// |reftest| skip -- Temporal is not supported
// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.calendar.prototype.daysInWeek
description: Temporal.Calendar.prototype.daysInWeek throws RangeError on
  ToTemporalDate when temporalDateLike is invalid string.
info: |
  4. Let temporalDate be ? ToTemporalDate(temporalDateLike).
features: [Temporal]
---*/
let cal = new Temporal.Calendar("iso8601");

assert.throws(RangeError, () => cal.daysInWeek("invalid string"),
    "Throw RangeError if temporalDateLike is invalid");

reportCompare(0, 0);
