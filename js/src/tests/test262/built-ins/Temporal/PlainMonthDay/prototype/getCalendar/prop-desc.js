// |reftest| skip-if(!this.hasOwnProperty('Temporal')) -- Temporal is not enabled unconditionally
// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.getcalendar
description: The "getCalendar" property of Temporal.PlainMonthDay.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.PlainMonthDay.prototype.getCalendar,
  "function",
  "`typeof PlainMonthDay.prototype.getCalendar` is `function`"
);

verifyProperty(Temporal.PlainMonthDay.prototype, "getCalendar", {
  writable: true,
  enumerable: false,
  configurable: true,
});

reportCompare(0, 0);
