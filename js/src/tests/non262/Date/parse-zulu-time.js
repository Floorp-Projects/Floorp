// |reftest| skip-if(xulRuntime.OS=="WINNT") -- Windows doesn't accept IANA names for the TZ env variable
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const tests = [
  "Aug 15, 2015 10:00Z",
  "08/15/2015 10:00Z",
  "Aug 15, 2015 10:00 Z",
]

inTimeZone("GMT", () => {
  for (const test of tests) {
    const dt = new Date(test);
    assertEq(dt.getTime(), new Date(2015, Month.August, 15, 10).getTime());
  }
});

if (typeof reportCompare === "function")
  reportCompare(true, true);
