// |reftest| skip -- Temporal is not supported
// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.until
description: Largest unit is respected
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const feb20 = new Temporal.PlainDateTime(2020, 2, 1, 0, 0);

const later = feb20.add({
  days: 1,
  milliseconds: 250,
  microseconds: 250,
  nanoseconds: 250
});

TemporalHelpers.assertDuration(
  feb20.until(later, { largestUnit: "milliseconds" }),
  0, 0, 0, 0, 0, 0, 0, 86400250, 250, 250,
  "can return subseconds (millisecond precision)"
);

TemporalHelpers.assertDuration(
  feb20.until(later, { largestUnit: "microseconds" }),
  0, 0, 0, 0, 0, 0, 0, 0, 86400250250, 250,
  "can return subseconds (microsecond precision)"
);

TemporalHelpers.assertDuration(
  feb20.until(later, { largestUnit: "nanoseconds" }),
  0, 0, 0, 0, 0, 0, 0, 0, 0, 86400250250250,
  "can return subseconds (nanosecond precision)"
);

reportCompare(0, 0);
