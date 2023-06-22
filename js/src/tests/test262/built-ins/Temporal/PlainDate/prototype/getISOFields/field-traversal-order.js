// |reftest| skip-if(!this.hasOwnProperty('Temporal')) -- Temporal is not enabled unconditionally
// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.getisofields
description: Properties added in correct order to object returned from getISOFields
includes: [compareArray.js]
features: [Temporal]
---*/

const expected = [
  "calendar",
  "isoDay",
  "isoMonth",
  "isoYear",
];

const date = new Temporal.PlainDate(2000, 5, 2);
const result = date.getISOFields();

assert.compareArray(Object.keys(result), expected);

reportCompare(0, 0);
