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
  do_check_true(defaultLocale === "en-US", "Default locale is en-US");
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

const PREF_MATCH_OS_LOCALE = "intl.locale.matchOS";
const PREF_SELECTED_LOCALE = "general.useragent.locale";
const PREF_OS_LOCALE       = "intl.locale.os";
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

  Services.prefs.setBoolPref(PREF_MATCH_OS_LOCALE, false);
  Services.prefs.setCharPref(PREF_SELECTED_LOCALE, "ar-IR");
  Services.prefs.setCharPref(PREF_OS_LOCALE, "en-US");

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

  Services.obs.addObserver(observer, REQ_LOC_CHANGE_EVENT, false);
  Services.prefs.setBoolPref(PREF_MATCH_OS_LOCALE, true);

  run_next_test();
});

/**
 * In this test we verify that after we set an observer on the LocaleService
 * event for requested locales change, it will be fired when the
 * pref for browser UI locale changes.
 */
add_test(function test_getRequestedLocales_matchOS() {
  do_test_pending();

  Services.prefs.setBoolPref(PREF_MATCH_OS_LOCALE, false);
  Services.prefs.setCharPref(PREF_SELECTED_LOCALE, "ar-IR");

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

  Services.obs.addObserver(observer, REQ_LOC_CHANGE_EVENT, false);
  Services.prefs.setCharPref(PREF_SELECTED_LOCALE, "sr-RU");

  run_next_test();
});

add_test(function test_setRequestedLocales() {
  localeService.setRequestedLocales([]);

  let matchOS = Services.prefs.getBoolPref(PREF_MATCH_OS_LOCALE);
  do_check_true(matchOS === true);

  localeService.setRequestedLocales(['de-AT']);

  matchOS = Services.prefs.getBoolPref(PREF_MATCH_OS_LOCALE);
  do_check_true(matchOS === false);
  let locales = localeService.getRequestedLocales();;
  do_check_true(locales[0] === 'de-AT');

  run_next_test();
});

add_test(function test_isAppLocaleRTL() {
  do_check_true(typeof localeService.isAppLocaleRTL === 'boolean');

  run_next_test();
});

do_register_cleanup(() => {
  Services.prefs.clearUserPref(PREF_SELECTED_LOCALE);
  Services.prefs.clearUserPref(PREF_MATCH_OS_LOCALE);
  Services.prefs.clearUserPref(PREF_OS_LOCALE, "en-US");
});
