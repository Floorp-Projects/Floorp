// |reftest| skip-if(!this.hasOwnProperty("Intl"))

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Check for duplicate variants and singletons case-insensitively, but don't
// check in privateuse components.

function checkInvalidLocale(locale)
{
  try
  {
    new Intl.NumberFormat(locale);
    throw new Error("didn't throw");
  }
  catch (e)
  {
    assertEq(e instanceof RangeError, true,
             "expected RangeError for locale '" + locale + "', got " + e);
  }
}

var badLocales =
  [
   "en-u-foo-U-foo",
   "en-tester-Tester",
   "en-tesTER-TESter",
   "de-DE-u-kn-true-U-kn-true",
   "ar-u-foo-q-bar-u-baz",
   "ar-z-moo-u-foo-q-bar-z-eit-u-baz",
  ];

for (var locale of badLocales)
  checkInvalidLocale(locale);

// Fully-privateuse locales are okay.
for (var locale of badLocales)
  new Intl.NumberFormat("x-" + locale).format(5);

// Locales with trailing privateuse also okay.
for (var locale of badLocales)
{
  new Intl.NumberFormat("en-x-" + locale).format(5);
  new Intl.NumberFormat("en-u-foo-x-u-" + locale).format(5);
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
