// |reftest| skip -- Temporal is not supported
// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.calendar.prototype.daysInWeek
description: Temporal.Calendar.prototype.daysInWeek throws TypeError
  when the internal lot is not presented.
info: |
  2. Perform ? RequireInternalSlot(calendar, [[InitializedTemporalCalendar]]).
features: [Temporal]
---*/
let cal = new Temporal.Calendar("iso8601");

let badCal = { daysInWeek: cal.daysInWeek }
assert.throws(TypeError, () => badCal.daysInWeek("2021-03-04"),
    "Throw TypeError if no internal slot");

reportCompare(0, 0);
