/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const { Services } = Cu.import('resource://gre/modules/Services.jsm', {});
const osPrefs = Cc["@mozilla.org/intl/ospreferences;1"].
  getService(Ci.mozIOSPreferences);

const localeService =
  Components.classes["@mozilla.org/intl/localeservice;1"]
  .getService(Components.interfaces.mozILocaleService);

/**
 * Make sure the locale service can be instantiated.
 */
function run_test()
{
  run_next_test();
}

add_test(function test_defaultLocale() {
  const defaultLocale = localeService.defaultLocale;
  do_check_true(defaultLocale.length !== 0, "Default locale is not empty");
  run_next_test();
});

add_test(function test_lastFallbackLocale() {
  const lastFallbackLocale = localeService.lastFallbackLocale;
  do_check_true(lastFallbackLocale === "en-US", "Last fallback locale is en-US");
  run_next_test();
});

add_test(function test_getAppLocalesAsLangTags() {
  const appLocale = localeService.getAppLocaleAsLangTag();
  do_check_true(appLocale != "", "appLocale is non-empty");

  const appLocales = localeService.getAppLocalesAsLangTags();
  do_check_true(Array.isArray(appLocales), "appLocales returns an array");

  do_check_true(appLocale == appLocales[0], "appLocale matches first entry in appLocales");

  const enUSLocales = appLocales.filter(loc => loc === "en-US");
  do_check_true(enUSLocales.length == 1, "en-US is present exactly one time");

  run_next_test();
});

const PREF_REQUESTED_LOCALES = "intl.locale.requested";
const REQ_LOC_CHANGE_EVENT = "intl:requested-locales-changed";

add_test(function test_getRequestedLocales() {
  const requestedLocales = localeService.getRequestedLocales();
  do_check_true(Array.isArray(requestedLocales), "requestedLocales returns an array");

  run_next_test();
});

/**
 * In this test we verify that after we set an observer on the LocaleService
 * event for requested locales change, it will be fired when the
 * pref for matchOS is set to true.
 *
 * Then, we test that when the matchOS is set to true, we will retrieve
 * OS locale from getRequestedLocales.
 */
add_test(function test_getRequestedLocales_matchOS() {
  do_test_pending();

  Services.prefs.setCharPref(PREF_REQUESTED_LOCALES, "ar-IR");

  const observer = {
    observe: function (aSubject, aTopic, aData) {
      switch (aTopic) {
        case REQ_LOC_CHANGE_EVENT:
          const reqLocs = localeService.getRequestedLocales();
          do_check_true(reqLocs[0] === osPrefs.systemLocale);
          Services.obs.removeObserver(observer, REQ_LOC_CHANGE_EVENT);
          do_test_finished();
      }
    }
  };

  Services.obs.addObserver(observer, REQ_LOC_CHANGE_EVENT);
  Services.prefs.setCharPref(PREF_REQUESTED_LOCALES, "");

  run_next_test();
});

/**
 * In this test we verify that after we set an observer on the LocaleService
 * event for requested locales change, it will be fired when the
 * pref for browser UI locale changes.
 */
add_test(function test_getRequestedLocales_onChange() {
  do_test_pending();

  Services.prefs.setCharPref(PREF_REQUESTED_LOCALES, "ar-IR");

  const observer = {
    observe: function (aSubject, aTopic, aData) {
      switch (aTopic) {
        case REQ_LOC_CHANGE_EVENT:
          const reqLocs = localeService.getRequestedLocales();
          do_check_true(reqLocs[0] === "sr-RU");
          Services.obs.removeObserver(observer, REQ_LOC_CHANGE_EVENT);
          do_test_finished();
      }
    }
  };

  Services.obs.addObserver(observer, REQ_LOC_CHANGE_EVENT);
  Services.prefs.setCharPref(PREF_REQUESTED_LOCALES, "sr-RU");

  run_next_test();
});

add_test(function test_getRequestedLocale() {
  Services.prefs.setCharPref(PREF_REQUESTED_LOCALES, "tlh");

  let requestedLocale = localeService.getRequestedLocale();
  do_check_true(requestedLocale === "tlh", "requestedLocale returns the right value");

  Services.prefs.clearUserPref(PREF_REQUESTED_LOCALES);

  run_next_test();
});

add_test(function test_setRequestedLocales() {
  localeService.setRequestedLocales([]);

  localeService.setRequestedLocales(['de-AT', 'de-DE', 'de-CH']);

  let locales = localeService.getRequestedLocales();;
  do_check_true(locales[0] === 'de-AT');
  do_check_true(locales[1] === 'de-DE');
  do_check_true(locales[2] === 'de-CH');

  run_next_test();
});

add_test(function test_isAppLocaleRTL() {
  do_check_true(typeof localeService.isAppLocaleRTL === 'boolean');

  run_next_test();
});

/**
 * This test verifies that all values coming from the pref are sanitized.
 */
add_test(function test_getRequestedLocales_sanitize() {
  Services.prefs.setCharPref(PREF_REQUESTED_LOCALES, "de,2,#$@#,pl,!a2,DE-at,,;");

  let locales = localeService.getRequestedLocales();
  do_check_eq(locales[0], "de");
  do_check_eq(locales[1], "pl");
  do_check_eq(locales[2], "de-AT");
  do_check_eq(locales[3], "und");
  do_check_eq(locales[4], localeService.lastFallbackLocale);
  do_check_eq(locales.length, 5);

  Services.prefs.clearUserPref(PREF_REQUESTED_LOCALES);

  run_next_test();
});

do_register_cleanup(() => {
  Services.prefs.clearUserPref(PREF_REQUESTED_LOCALES);
});
