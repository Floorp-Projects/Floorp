// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The [[Value]] property of the newly constructed object
    with supplied "undefined" argument should be NaN
esid: sec-date-year-month-date-hours-minutes-seconds-ms
description: 2 arguments, (year, month)
---*/

function DateValue(year, month, date, hours, minutes, seconds, ms) {
  return new Date(year, month, date, hours, minutes, seconds, ms).valueOf();
}

var x;
x = DateValue(1899, 11);
assert.sameValue(x, NaN, "(1899, 11)");

x = DateValue(1899, 12);
assert.sameValue(x, NaN, "(1899, 12)");

x = DateValue(1900, 0);
assert.sameValue(x, NaN, "(1900, 0)");

x = DateValue(1969, 11);
assert.sameValue(x, NaN, "(1969, 11)");

x = DateValue(1969, 12);
assert.sameValue(x, NaN, "(1969, 12)");

x = DateValue(1970, 0);
assert.sameValue(x, NaN, "(1970, 0)");

x = DateValue(1999, 11);
assert.sameValue(x, NaN, "(1999, 11)");

x = DateValue(1999, 12);
assert.sameValue(x, NaN, "(1999, 12)");

x = DateValue(2000, 0);
assert.sameValue(x, NaN, "(2000, 0)");

x = DateValue(2099, 11);
assert.sameValue(x, NaN, "(2099, 11)");

x = DateValue(2099, 12);
assert.sameValue(x, NaN, "(2099, 12)");

x = DateValue(2100, 0);
assert.sameValue(x, NaN, "(2100, 0)");

reportCompare(0, 0);
