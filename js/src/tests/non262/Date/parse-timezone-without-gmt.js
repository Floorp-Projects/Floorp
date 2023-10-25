/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const accepted = {
  "1995-09-26T00:00:00-0500": [
    "Sep 26 1995 GMT-0500",
    "Sep 26 1995 00:00:00 GMT-0500",
    "Sep 26 1995 00:00:00 gmt-0500",
    "Sep 26 1995 00:00:00 Z-0500",
    "Sep 26 1995 00:00:00 UT-0500",
    "Sep 26 1995 00:00:00 UTC-0500",
    "Sep 26 1995 00:00:00 -0500",
    "Sep 26 1995 00:00:00 -05",
    "Sep 26 1995 00:00:00-0500",
    "Sep 26 1995 00:00:00-05",
    "Sep 26 1995 00:00 -0500",
    "Sep 26 1995 00:00 -05",
    "Sep 26 1995 00:00-0500",
    "Sep 26 1995 00:00-05",
  ],

  "1995-09-26T00:00:00+0500": [
    "Sep 26 1995 GMT+0500",
    "Sep 26 1995 00:00:00 GMT+0500",
    "Sep 26 1995 00:00:00 gmt+0500",
    "Sep 26 1995 00:00:00 Z+0500",
    "Sep 26 1995 00:00:00 UT+0500",
    "Sep 26 1995 00:00:00 UTC+0500",
    "Sep 26 1995 00:00:00 +0500",
    "Sep 26 1995 00:00:00 +05",
    "Sep 26 1995 00:00:00+0500",
    "Sep 26 1995 00:00:00+05",
    "Sep 26 1995 00:00 +0500",
    "Sep 26 1995 00:00 +05",
    "Sep 26 1995 00:00+0500",
    "Sep 26 1995 00:00+05",
  ],

  "1995-09-26T00:00:00-0430": [
    "Sep 26 1995 GMT-04:30",
    "Sep 26 1995 00:00:00 GMT-04:30",
    "Sep 26 1995 00:00:00 -04:30",
    "Sep 26 1995 00:00:00-04:30",
    "Sep 26 1995 00:00 -04:30",
    "Sep 26 1995 00:00-04:30",
  ],

  "1995-09-26T00:00:00+0430": [
    "Sep 26 1995 GMT+04:30",
    "Sep 26 1995 00:00:00 GMT+04:30",
    "Sep 26 1995 00:00:00 +04:30",
    "Sep 26 1995 00:00:00+04:30",
    "Sep 26 1995 00:00 +04:30",
    "Sep 26 1995 00:00+04:30",
  ],

  "1995-09-26T04:30:00": [
    "Sep 26 1995-04:30",
    "1995-09-26-04:30",
    "1995-Sep-26-04:30",
  ],
};
const rejected = [
  "Sep 26 1995 -05",
  "Sep 26 1995-05",
  "Sep 26 1995 -04:30",
  "1995-09-26 -05",
  "1995-09-26 -04:30",
  "1995-09-26-05",
  "1995-Sep-26 -05",
  "1995-Sep-26-05",
  "1995-Sep-26,-05",

  "Sep 26 1995 +05",
  "Sep 26 1995 +04:30",
  "Sep 26 1995+05",
  "Sep 26 1995+04:30",
  "1995-09-26 +05",
  "1995-09-26+05",
  "1995-Sep-26 +05",
  "1995-Sep-26+05",
  "1995-Sep-26,+05",

  // These cases are allowed by V8 but are parsed as GMT-XXXX no matter the
  // abbreviation (e.g. EST-0500 is parsed as GMT-0500 and not GMT-1000). This
  // is unexpected and so we are explicitly rejecting them.
  "Sep 26 1995 00:00:00 EST-0500",
  "Sep 26 1995 00:00:00 MDT-0500",
];

for (const [expected, patterns] of Object.entries(accepted)) {
  for (const test of patterns) {
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
}

for (const reject of rejected) {
  assertEq(
    true, isNaN(new Date(reject)),
    `"${reject}" should be rejected.`
  );
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
