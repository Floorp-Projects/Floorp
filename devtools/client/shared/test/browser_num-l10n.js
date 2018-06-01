/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the localization utils work properly.

const { LocalizationHelper } = require("devtools/shared/l10n");

function test() {
  const l10n = new LocalizationHelper();

  is(l10n.numberWithDecimals(1234.56789, 2), "1,234.57",
    "The first number was properly localized.");
  is(l10n.numberWithDecimals(0.0001, 2), "0",
    "The second number was properly localized.");
  is(l10n.numberWithDecimals(1.0001, 2), "1",
    "The third number was properly localized.");
  is(l10n.numberWithDecimals(NaN, 2), "0",
    "NaN was properly localized.");
  is(l10n.numberWithDecimals(null, 2), "0",
    "`null` was properly localized.");
  is(l10n.numberWithDecimals(undefined, 2), "0",
    "`undefined` was properly localized.");
  is(l10n.numberWithDecimals(-1234.56789, 2), "-1,234.57",
    "Negative number was properly localized.");
  is(l10n.numberWithDecimals(1234.56789, 0), "1,235",
    "Number was properly localized with decimals set 0.");
  is(l10n.numberWithDecimals(-1234.56789, 0), "-1,235",
    "Negative number was properly localized with decimals set 0.");
  is(l10n.numberWithDecimals(12, 2), "12",
    "The integer was properly localized, without decimals.");
  is(l10n.numberWithDecimals(-12, 2), "-12",
    "The negative integer was properly localized, without decimals.");
  is(l10n.numberWithDecimals(1200, 2), "1,200",
    "The big integer was properly localized, no decimals but with a separator.");
  is(l10n.numberWithDecimals(-1200, 2), "-1,200",
    "The negative big integer was properly localized, no decimals but with a separator.");

  finish();
}
