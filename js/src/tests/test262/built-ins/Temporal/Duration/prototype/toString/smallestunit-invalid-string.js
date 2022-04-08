// |reftest| skip -- Temporal is not supported
// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.tostring
description: RangeError thrown when smallestUnit option not one of the allowed string values
features: [Temporal]
---*/

const duration = new Temporal.Duration(0, 0, 0, 0, 12, 34, 56, 123, 987, 500);
const values = ["eras", "years", "months", "weeks", "days", "hours", "minutes", "nonsense", "other string", "mill\u0131seconds", "SECONDS"];
for (const smallestUnit of values) {
  assert.throws(RangeError, () => duration.toString({ smallestUnit }));
}

reportCompare(0, 0);
