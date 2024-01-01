/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const accepted = {
  "Sep 26 1995 UT": "1995-09-26T00:00:00Z",
  "Sep 26 1995 UTC": "1995-09-26T00:00:00Z",
  "Sep 26 1995 GMT": "1995-09-26T00:00:00Z",
  "Sep 26 1995 EST": "1995-09-26T00:00:00-0500",
  "Sep 26 1995 est": "1995-09-26T00:00:00-0500",
  "Sep 26 1995 10:00 am": "1995-09-26T10:00:00",
  "Sep 26 1995 10:00 AM": "1995-09-26T10:00:00",
  "Sep 26 1995 10:00 pm": "1995-09-26T22:00:00",
  "Sep 26 Thurs 1995 Mon 10:thursday:00": "1995-09-26T10:00:00",
};
const rejected = [
  "Sep 26 1995 G",
  "Sep 26 1995 GM",
  "Sep 26 1995 E",
  "Sep 26 1995 ES",
  "Sep 26 1995 10:00 a",
  "Sep 26 1995 10:00 p",
  "0/zx",
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
