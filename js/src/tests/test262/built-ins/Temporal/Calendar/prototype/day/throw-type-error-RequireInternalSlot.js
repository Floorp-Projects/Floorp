// |reftest| skip -- Temporal is not supported
// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.day
description: Temporal.Calendar.prototype.day throws TypeError on RequireInternalSlot if object has no internal slot.
info: |
  2. Perform ? RequireInternalSlot(calendar, [[InitializedTemporalCalendar]]).
features: [Temporal, arrow-function]
---*/
let cal = new Temporal.Calendar("iso8601");

let badCal = { day: cal.day }
assert.throws(TypeError, () => badCal.day("2021-03-04"),
    "Throw TypeError if there are no internal slot");

reportCompare(0, 0);
