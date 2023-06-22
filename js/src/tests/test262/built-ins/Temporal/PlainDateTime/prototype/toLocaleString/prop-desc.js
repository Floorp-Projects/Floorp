// |reftest| skip-if(!this.hasOwnProperty('Temporal')) -- Temporal is not enabled unconditionally
// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.tolocalestring
description: The "toLocaleString" property of Temporal.PlainDateTime.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.PlainDateTime.prototype.toLocaleString,
  "function",
  "`typeof PlainDateTime.prototype.toLocaleString` is `function`"
);

verifyProperty(Temporal.PlainDateTime.prototype, "toLocaleString", {
  writable: true,
  enumerable: false,
  configurable: true,
});

reportCompare(0, 0);
