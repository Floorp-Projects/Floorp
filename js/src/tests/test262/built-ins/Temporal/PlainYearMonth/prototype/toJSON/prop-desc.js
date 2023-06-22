// |reftest| skip-if(!this.hasOwnProperty('Temporal')) -- Temporal is not enabled unconditionally
// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.tojson
description: The "toJSON" property of Temporal.PlainYearMonth.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.PlainYearMonth.prototype.toJSON,
  "function",
  "`typeof PlainYearMonth.prototype.toJSON` is `function`"
);

verifyProperty(Temporal.PlainYearMonth.prototype, "toJSON", {
  writable: true,
  enumerable: false,
  configurable: true,
});

reportCompare(0, 0);
