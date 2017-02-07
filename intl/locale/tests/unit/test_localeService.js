/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Make sure the locale service can be instantiated,
 * and returns something plausible for getAppLocale and getAppLocales.
 */

function run_test()
{
  var localeService =
    Components.classes["@mozilla.org/intl/localeservice;1"]
    .getService(Components.interfaces.mozILocaleService);

  var appLocale = localeService.getAppLocale();
  do_check_true(appLocale != "", "appLocale is non-empty");

  var appLocales = localeService.getAppLocales();
  do_check_true(Array.isArray(appLocales), "appLocales returns an array");

  do_check_true(appLocale == appLocales[0], "appLocale matches first entry in appLocales");

  var enUScount = 0;
  appLocales.forEach(function(loc) { if (loc == "en-US") { enUScount++; } });
  do_check_true(enUScount == 1, "en-US is present exactly one time");
}
