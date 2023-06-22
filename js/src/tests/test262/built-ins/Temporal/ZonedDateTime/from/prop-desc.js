// |reftest| skip-if(!this.hasOwnProperty('Temporal')) -- Temporal is not enabled unconditionally
// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.from
description: The "from" property of Temporal.ZonedDateTime
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.ZonedDateTime.from,
  "function",
  "`typeof ZonedDateTime.from` is `function`"
);

verifyProperty(Temporal.ZonedDateTime, "from", {
  writable: true,
  enumerable: false,
  configurable: true,
});

reportCompare(0, 0);
