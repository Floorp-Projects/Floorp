/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const accepted = {
  "Sep 26 1995 12:34:56.789": "1995-09-26T12:34:56.789",
  "Sep 26 1995 12:34:56.7": "1995-09-26T12:34:56.700",
  "Sep 26 1995 12:34:56.78": "1995-09-26T12:34:56.780",
  "Sep 26 1995 12:34:56.001": "1995-09-26T12:34:56.001",
  "Sep 26 1995 12:34:56.0001": "1995-09-26T12:34:56",

  "Sep 26 1995 12:34:56.789Z": "1995-09-26T12:34:56.789Z",
  "Sep 26 1995 12:34:56.789 -0500": "1995-09-26T12:34:56.789-0500",
  "Sep 26 1995 12:34:56.789-0500": "1995-09-26T12:34:56.789-0500",

  // Note that the trailing '.' without milliseconds is rejected.
  // Rejecting this case could be done trivially, but Chrome allows it,
  // so we will allow it for parity.
  "Sep 26 1995 12:34:56.789.": "1995-09-26T12:34:56.789",
};
const rejected = [
  "Sep 26 1995 12:34:56.",
  "Sep 26 1995 12:34:56:789",
  "Sep 26 1995 12:34:56..789",
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

for (const reject of rejected) {
  assertEq(
    true, isNaN(new Date(reject)),
    `"${reject}" should be rejected.`
  );
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
