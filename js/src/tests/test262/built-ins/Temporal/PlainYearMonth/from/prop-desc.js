// |reftest| skip-if(!this.hasOwnProperty('Temporal')) -- Temporal is not enabled unconditionally
// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.from
description: The "from" property of Temporal.PlainYearMonth
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.PlainYearMonth.from,
  "function",
  "`typeof PlainYearMonth.from` is `function`"
);

verifyProperty(Temporal.PlainYearMonth, "from", {
  writable: true,
  enumerable: false,
  configurable: true,
});

reportCompare(0, 0);
