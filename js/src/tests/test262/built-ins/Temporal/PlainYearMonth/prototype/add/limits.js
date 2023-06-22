// |reftest| skip-if(!this.hasOwnProperty('Temporal')) -- Temporal is not enabled unconditionally
// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.add
description: RangeError thrown when going out of range
features: [Temporal]
---*/

const max = Temporal.PlainYearMonth.from("+275760-09");
for (const overflow of ["reject", "constrain"]) {
  assert.throws(RangeError, () => max.add({ months: 1 }, { overflow }), overflow);
}

reportCompare(0, 0);
