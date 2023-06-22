// |reftest| skip-if(!this.hasOwnProperty('Temporal')) -- Temporal is not enabled unconditionally
// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.compare
description: The "compare" property of Temporal.PlainYearMonth
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.PlainYearMonth.compare,
  "function",
  "`typeof PlainYearMonth.compare` is `function`"
);

verifyProperty(Temporal.PlainYearMonth, "compare", {
  writable: true,
  enumerable: false,
  configurable: true,
});

reportCompare(0, 0);
