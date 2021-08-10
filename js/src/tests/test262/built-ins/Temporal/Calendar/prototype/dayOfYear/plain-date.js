// |reftest| skip -- Temporal is not supported
// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.calendar.prototype.dayofyear
description: Temporal.Calendar.prototype.dayOfYear will take PlainDate object and
  return the day of year.
info: |
  5. Return 𝔽(! ToISODayOfYear(temporalDate.[[ISOYear]], temporalDate.[[ISOMonth]], temporalDate.[[ISODay]])).
features: [Temporal]
---*/
let cal = new Temporal.Calendar("iso8601");

let d = new Temporal.PlainDate(1970, 1, 1);
assert.sameValue(1, cal.dayOfYear(d));
d = new Temporal.PlainDate(2000, 1, 1);
assert.sameValue(1, cal.dayOfYear(d));

d = new Temporal.PlainDate(2021, 1, 15);
assert.sameValue(15, cal.dayOfYear(d));

d = new Temporal.PlainDate(2020, 2, 15);
assert.sameValue(46, cal.dayOfYear(d));

d = new Temporal.PlainDate(2020, 3, 15);
assert.sameValue(75, cal.dayOfYear(d));

d = new Temporal.PlainDate(2000, 3, 15);
assert.sameValue(75, cal.dayOfYear(d));

d = new Temporal.PlainDate(2001, 3, 15);
assert.sameValue(74, cal.dayOfYear(d));

d = new Temporal.PlainDate(2000, 12, 31);
assert.sameValue(366, cal.dayOfYear(d));

d = new Temporal.PlainDate(2001, 12, 31);
assert.sameValue(365, cal.dayOfYear(d));

reportCompare(0, 0);
