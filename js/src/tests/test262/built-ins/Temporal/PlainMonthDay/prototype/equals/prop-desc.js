// |reftest| skip-if(!this.hasOwnProperty('Temporal')) -- Temporal is not enabled unconditionally
// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.equals
description: The "equals" property of Temporal.PlainMonthDay.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.PlainMonthDay.prototype.equals,
  "function",
  "`typeof PlainMonthDay.prototype.equals` is `function`"
);

verifyProperty(Temporal.PlainMonthDay.prototype, "equals", {
  writable: true,
  enumerable: false,
  configurable: true,
});

reportCompare(0, 0);
