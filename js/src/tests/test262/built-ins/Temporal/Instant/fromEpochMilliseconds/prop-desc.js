// |reftest| skip-if(!this.hasOwnProperty('Temporal')) -- Temporal is not enabled unconditionally
// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.fromepochmilliseconds
description: The "fromEpochMilliseconds" property of Temporal.Instant
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.Instant.fromEpochMilliseconds,
  "function",
  "`typeof Instant.fromEpochMilliseconds` is `function`"
);

verifyProperty(Temporal.Instant, "fromEpochMilliseconds", {
  writable: true,
  enumerable: false,
  configurable: true,
});

reportCompare(0, 0);
