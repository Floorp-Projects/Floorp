// |reftest| skip -- Temporal is not supported
// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.compare
description: Checking a typical case (nothing undefined, no NaNs, does not throw, etc.)
features: [Temporal]
---*/

const dt1 = new Temporal.PlainDateTime(1976, 11, 18, 15, 23, 30, 123, 456, 789);
const dt2 = new Temporal.PlainDateTime(2019, 10, 29, 10, 46, 38, 271, 986, 102);

assert.sameValue(
  Temporal.PlainDateTime.compare(dt1, dt1),
  0,
  "equal"
);

assert.sameValue(
  Temporal.PlainDateTime.compare(dt1, dt2),
  -1,
  "smaller/larger"
);

assert.sameValue(
  Temporal.PlainDateTime.compare(dt2, dt1),
  1,
  "larger/smaller"
);

reportCompare(0, 0);
