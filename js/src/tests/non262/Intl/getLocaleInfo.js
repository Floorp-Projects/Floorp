// |reftest| skip-if(!this.hasOwnProperty("Intl")||!this.hasOwnProperty("addIntlExtras"))
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the getCalendarInfo function with a diverse set of arguments.

function checkLocaleInfo(info, expected)
{
  assertEq(Object.getPrototypeOf(info), Object.prototype);

  assertEq(info.direction, expected.direction);
  assertEq(info.locale, expected.locale);
}

addIntlExtras(Intl);

let gLI = Intl.getLocaleInfo;

assertEq(gLI.length, 1);

checkLocaleInfo(gLI('en-US'), {
  direction: "ltr",
  locale: "en-US"
});

checkLocaleInfo(gLI('fr'), {
  direction: "ltr",
  locale: "fr"
});

checkLocaleInfo(gLI('ar'), {
  direction: "rtl",
  locale: "ar"
});

if (typeof reportCompare === 'function')
    reportCompare(0, 0);
