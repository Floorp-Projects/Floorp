// |reftest| skip-if(!this.hasOwnProperty("Intl")||!this.hasOwnProperty("addIntlExtras"))
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the getCalendarInfo function with a diverse set of arguments.

function checkCalendarInfo(info, expected)
{
  assertEq(Object.getPrototypeOf(info), Object.prototype);

  assertEq(info.firstDayOfWeek, expected.firstDayOfWeek);
  assertEq(info.minDays, expected.minDays);
  assertEq(info.weekendStart, expected.weekendStart);
  assertEq(info.weekendEnd, expected.weekendEnd);
  assertEq(info.calendar, expected.calendar);
  assertEq(info.locale, expected.locale);
}

addIntlExtras(Intl);

let gCI = Intl.getCalendarInfo;

assertEq(gCI.length, 1);

checkCalendarInfo(gCI('en-US'), {
  firstDayOfWeek: 1,
  minDays: 1,
  weekendStart: 7,
  weekendEnd: 1,
  calendar: "gregory",
  locale: "en-US"
});

checkCalendarInfo(gCI('en-IL'), {
  firstDayOfWeek: 1,
  minDays: 1,
  weekendStart: 6,
  weekendEnd: 7,
  calendar: "gregory",
  locale: "en-IL"
});


checkCalendarInfo(gCI('en-GB'), {
  firstDayOfWeek: 2,
  minDays: 4,
  weekendStart: 7,
  weekendEnd: 1,
  calendar: "gregory",
  locale: "en-GB"
});


checkCalendarInfo(gCI('pl'), {
  firstDayOfWeek: 2,
  minDays: 4,
  weekendStart: 7,
  weekendEnd: 1,
  calendar: "gregory",
  locale: "pl"
});

checkCalendarInfo(gCI('ar-IQ'), {
  firstDayOfWeek: 7,
  minDays: 1,
  weekendStart: 6,
  weekendEnd: 7,
  calendar: "gregory",
  locale: "ar-IQ"
});

checkCalendarInfo(gCI('fa-IR'), {
  firstDayOfWeek: 7,
  minDays: 1,
  weekendStart: 6,
  weekendEnd: 6,
  calendar: "persian",
  locale: "fa-IR"
});

if (typeof reportCompare === 'function')
    reportCompare(0, 0);
