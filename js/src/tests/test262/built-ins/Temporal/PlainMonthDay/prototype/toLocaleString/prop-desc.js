// |reftest| skip-if(!this.hasOwnProperty('Temporal')) -- Temporal is not enabled unconditionally
// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.tolocalestring
description: The "toLocaleString" property of Temporal.PlainMonthDay.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.PlainMonthDay.prototype.toLocaleString,
  "function",
  "`typeof PlainMonthDay.prototype.toLocaleString` is `function`"
);

verifyProperty(Temporal.PlainMonthDay.prototype, "toLocaleString", {
  writable: true,
  enumerable: false,
  configurable: true,
});

reportCompare(0, 0);
