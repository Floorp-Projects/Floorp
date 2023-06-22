// |reftest| skip-if(!this.hasOwnProperty('Temporal')) -- Temporal is not enabled unconditionally
// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth
description: The "PlainYearMonth" property of Temporal
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.PlainYearMonth,
  "function",
  "`typeof PlainYearMonth` is `function`"
);

verifyProperty(Temporal, "PlainYearMonth", {
  writable: true,
  enumerable: false,
  configurable: true,
});

reportCompare(0, 0);
