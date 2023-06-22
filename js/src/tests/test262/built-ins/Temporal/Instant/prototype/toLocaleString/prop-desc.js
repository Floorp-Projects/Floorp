// |reftest| skip-if(!this.hasOwnProperty('Temporal')) -- Temporal is not enabled unconditionally
// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.tolocalestring
description: The "toLocaleString" property of Temporal.Instant.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.Instant.prototype.toLocaleString,
  "function",
  "`typeof Instant.prototype.toLocaleString` is `function`"
);

verifyProperty(Temporal.Instant.prototype, "toLocaleString", {
  writable: true,
  enumerable: false,
  configurable: true,
});

reportCompare(0, 0);
