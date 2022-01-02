// Copyright 2019 Google Inc. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-intl.datetimeformat.prototype.resolvedoptions
description: Verifies the property order for the object returned by resolvedOptions().
includes: [arrayContains.js]
features: [Intl.DateTimeFormat-fractionalSecondDigits]
---*/

const options = new Intl.DateTimeFormat([], {
  "fractionalSecondDigits": 3,
  "minute": "numeric",
  "second": "numeric",
}).resolvedOptions();

const expected = [
  "locale",
  "calendar",
  "numberingSystem",
  "timeZone",
  "minute",
  "second",
  "fractionalSecondDigits",
];

let actual = Object.getOwnPropertyNames(options);

// Ensure all expected items are in actual and also allow other properties
// implemented in new proposals.
assert(arrayContains(actual, expected));
for (var i = 1; i < expected.length; i++) {
  // Ensure the order as expected but allow additional new property in between
  assert(actual.indexOf(expected[i-1]) < actual.indexOf(expected[i]));
}

reportCompare(0, 0);
