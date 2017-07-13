/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test()
{
  const osprefs =
    Components.classes["@mozilla.org/intl/ospreferences;1"]
    .getService(Components.interfaces.mozIOSPreferences);

  const systemLocale = osprefs.systemLocale;
  do_check_true(systemLocale != "", "systemLocale is non-empty");

  const systemLocales = osprefs.getSystemLocales();
  do_check_true(Array.isArray(systemLocales), "systemLocales returns an array");

  do_check_true(systemLocale == systemLocales[0],
    "systemLocale matches first entry in systemLocales");

  const rgLocales = osprefs.getRegionalPrefsLocales();
  do_check_true(Array.isArray(rgLocales), "regionalPrefsLocales returns an array");

  const getDateTimePatternTests = [
    [osprefs.dateTimeFormatStyleNone, osprefs.dateTimeFormatStyleNone, ""],
    [osprefs.dateTimeFormatStyleShort, osprefs.dateTimeFormatStyleNone, ""],
    [osprefs.dateTimeFormatStyleNone, osprefs.dateTimeFormatStyleLong, "ar"],
    [osprefs.dateTimeFormatStyleFull, osprefs.dateTimeFormatStyleMedium, "ru"],
  ];

  for (let i = 0; i < getDateTimePatternTests.length; i++) {
    const test = getDateTimePatternTests[i];

    const pattern = osprefs.getDateTimePattern(...test);
    if (test[0] !== osprefs.dateTimeFormatStyleNone &&
        test[1] !== osprefs.dateTImeFormatStyleNone) {
      do_check_true(pattern.length > 0, "pattern is not empty.");
    }
  }

  do_check_true(1, "osprefs didn't crash");
}
