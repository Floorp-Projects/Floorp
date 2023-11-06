// |reftest| skip-if(xulRuntime.OS=="WINNT") -- Windows doesn't accept IANA names for the TZ env variable
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const tests = [
  "Aug. 15, 2015",
  "Aug.. 15, 2015",
  "Aug.15.2015",
  "15.Aug.2015",
  "Aug 15 2015 12:00 am.",
  "Sat. Aug 15 2015",
  "2015.08.15",
  "2015.08.15.00:00:00",

  // These look weird but are accepted for Chrome parity
  // (dots are valid delimiters and are essentially ignored)
  "2015./08/15 00:00:00",
  "2015/08./15 00:00:00",
];
const rejected = [
  "2015/08/15 00:00:00.",
  "2015/08/15 00:00:00.GMT",
];

for (const test of tests) {
  assertEq(new Date(test).getTime(),
           new Date(2015, Month.August, 15).getTime(),
           `"${test}" should be accepted.`);
}

for (const reject of rejected) {
  assertEq(
    true, isNaN(new Date(reject)),
    `"${reject}" should be rejected.`
  );
}

inTimeZone("Etc/GMT-1", () => {
  let dt = new Date("Aug 15 2015 GMT.");
  assertEq(dt.getTime(), new Date(2015, Month.August, 15, 1).getTime());
});

if (typeof reportCompare === "function")
  reportCompare(true, true);
