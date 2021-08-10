// |reftest| skip -- Temporal is not supported
// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.day
description: Temporal.Calendar.prototype.day will take PlainDate and return
  the value of the day.
info: |
  5. Return ! ISODay(temporalDateLike).
features: [Temporal]
---*/
let cal = new Temporal.Calendar("iso8601");

let date = new Temporal.PlainDate(2021, 7, 15);
assert.sameValue(15, cal.day(date));

reportCompare(0, 0);
