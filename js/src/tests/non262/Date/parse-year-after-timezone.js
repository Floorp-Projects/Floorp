/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const accepted = {
  "1997-11-05T00:00:00-0800": [
    "Wed Nov 05 00:00:00 GMT-0800 1997",
    "Nov 05 00:00:00 GMT-0800 1997",
    "Nov-05 00:00:00 GMT-0800 1997",
    "05-Nov 00:00:00 GMT-0800 1997",
    "05-Nov GMT-0800 1997",
  ],

  "-001997-11-05T00:00:00-0800": [
    "Wed Nov 05 00:00:00 GMT-0800 -1997",
    "Nov 05 00:00:00 GMT-0800 -1997",
    "Nov-05 00:00:00 GMT-0800 -1997",
    "05-Nov 00:00:00 GMT-0800 -1997",
    "05-Nov GMT-0800 -1997",
  ],
};

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

if (typeof reportCompare === "function")
    reportCompare(true, true);
