// |reftest| skip -- Temporal is not supported
// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.from
description: Number argument is converted to string
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const result = Temporal.PlainTime.from(1523);
TemporalHelpers.assertPlainTime(result, 15, 23, 0, 0, 0, 0);

reportCompare(0, 0);
