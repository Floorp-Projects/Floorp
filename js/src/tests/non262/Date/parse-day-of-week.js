/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const expectedDate = new Date("1995-09-26T00:00:00");

// Each prefix will be tested with each format:
const prefixes = [
  "Tuesday, ",
  "Tuesday ",
  "Tuesday,",
  "Tuesday.",
  "Tuesday-",
  "Tuesday/",

  // Case insensitive
  "tuesday, ",
  "tUeSdAy ",

  // Abbreviations are valid down to the first character
  "Tuesda ",
  "Tue ",
  "T ",
  "t,",

  // Floating delimiters at beginning are allowed/ignored
  " ",
  ",",
  ", ",
  ".",
  "-",
  "/",

  // It doesn't actually need to be the correct day of the week, or
  // a day of week at all...you can put anything there
  "Monday ",
  "foo bar "
];
const formats = [
  "Sep 26 1995",
  "26 Sep 1995",
  "September 26, 1995",
  "26-Sep-1995",
  "1995-9-26",
  // ISO format is non-formal with day of week in front
  "1995-09-26",

  // You can put anything between the month and mday
  "Sep foo bar 26 1995",
  "Sep-foo bar-26 1995",
  "Sep-foo-bar-26 1995",

  // Redundant month names are allowed
  "Sep sep 26 1995",
  "Sep 26 sep 1995",
  // Edge case: if multiple month names, use the last one
  "Jan 26 1995 sep",
];

const rejected = [
  "Sep 26 foo 1995",
  "Sep 26 1995 foo",
  "1995 foo Sep 26",
];

for (const format of formats) {
  for (const prefix of prefixes) {
    const test = prefix + format;
    const testDate = new Date(test);

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
