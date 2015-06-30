// |reftest| skip-if(!this.hasOwnProperty("Intl"))

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Locale processing is supposed to internally remove any Unicode extension
// sequences in the locale.  Test that various weird testcases invoking
// algorithmic edge cases don't assert or throw exceptions.

var weirdCases =
  [
   "x-u-foo",
   "en-x-u-foo",
   "en-a-bar-x-u-foo",
   "en-x-u-foo-a-bar",
   "en-a-bar-u-baz-x-u-foo",
  ];

for (var locale of weirdCases)
  Intl.NumberFormat(locale).format(5);

if (typeof reportCompare === "function")
  reportCompare(true, true);
