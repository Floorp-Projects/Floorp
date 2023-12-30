/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const accepted = {
  "0": "2000-01-01T00:00:00",
  "1": "2001-01-01T00:00:00",
  "12": "2001-12-01T00:00:00",
  "32": "2032-01-01T00:00:00",
  "49": "2049-01-01T00:00:00",
  "50": "1950-01-01T00:00:00",
  "99": "1999-01-01T00:00:00",
  "100": "0100-01-01T00:00:00",
  "999": "0999-01-01T00:00:00",
  "1000": "1000-01-01T00:00:00Z",
  "1000-": "1000-01-01T00:00:00",
  "20009": "+020009-01-01T00:00:00",
  "+20009": "+020009-01-01T00:00:00",

  // Rejecting e.g. S22 (see rejected patterns below) shouldn't
  // reject mday directly after month name
  "Sep26 1995": "1995-09-26T00:00:00",
};
const rejected = [
  "S22",
  "5C",
  "Sep26 foo 1995",
];

for (const [test, expected] of Object.entries(accepted)) {
  const testDate = new Date(test);
  const expectedDate = new Date(expected);

  assertEq(
    false, isNaN(testDate),
    `${test} should be accepted.`
  );

  assertEq(
    testDate.getTime(), expectedDate.getTime(),
    `"${test}" should be ${expectedDate} (got ${testDate}).`
  );
}

for (let i = 13; i <= 31; ++i) {
  assertEq(
    true, isNaN(new Date(`${i}`)),
    `"${i}" should be rejected.`
  );
}
for (const reject of rejected) {
  assertEq(
    true, isNaN(new Date(reject)),
    `"${reject}" should be rejected.`
  );
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
