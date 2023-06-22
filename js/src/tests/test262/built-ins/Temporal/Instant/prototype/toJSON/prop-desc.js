// |reftest| skip-if(!this.hasOwnProperty('Temporal')) -- Temporal is not enabled unconditionally
// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.tojson
description: The "toJSON" property of Temporal.Instant.prototype
includes: [propertyHelper.js]
features: [Temporal]
---*/

assert.sameValue(
  typeof Temporal.Instant.prototype.toJSON,
  "function",
  "`typeof Instant.prototype.toJSON` is `function`"
);

verifyProperty(Temporal.Instant.prototype, "toJSON", {
  writable: true,
  enumerable: false,
  configurable: true,
});

reportCompare(0, 0);
