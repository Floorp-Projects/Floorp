/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const tests = [
  "Aug. 15, 2015",
  "Aug.. 15, 2015",
  "Aug 15 2015 12:00 am.",
  "Sat. Aug 15 2015",
]

for (const test of tests) {
  assertEq(Date.parse("Aug 15, 2015"), Date.parse(test));
}

assertEq(Date.parse("Aug 15 2015 GMT."),
         Date.parse("Aug 15 2015 GMT"));

if (typeof reportCompare === "function")
  reportCompare(true, true);
