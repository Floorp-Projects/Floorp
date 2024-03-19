/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

const REQ_LOC_CHANGE_EVENT = "intl:requested-locales-changed";

function promiseLocaleChanged(requestedLocale) {
  return new Promise(resolve => {
    let localeObserver = {
      observe(aSubject, aTopic) {
        switch (aTopic) {
          case REQ_LOC_CHANGE_EVENT:
            let reqLocs = Services.locale.requestedLocales;
            equal(reqLocs[0], requestedLocale);
            Services.obs.removeObserver(localeObserver, REQ_LOC_CHANGE_EVENT);
            resolve();
        }
      },
    };
    Services.obs.addObserver(localeObserver, REQ_LOC_CHANGE_EVENT);
  });
}

function promiseLocaleNotChanged() {
  return new Promise(resolve => {
    let localeObserver = {
      observe(aSubject, aTopic) {
        switch (aTopic) {
          case REQ_LOC_CHANGE_EVENT:
            ok(false, "Locale should not change.");
            Services.obs.removeObserver(localeObserver, REQ_LOC_CHANGE_EVENT);
            resolve();
        }
      },
    };
    Services.obs.addObserver(localeObserver, REQ_LOC_CHANGE_EVENT);
    /* eslint-disable mozilla/no-arbitrary-setTimeout */
    setTimeout(function () {
      Services.obs.removeObserver(localeObserver, REQ_LOC_CHANGE_EVENT);
      resolve();
    }, 100);
  });
}

add_task(async function test_requested_locale_array() {
  let originalLocales = Services.locale.requestedLocales;
  let localePromise = promiseLocaleChanged("de");
  await setupPolicyEngineWithJson({
    policies: {
      RequestedLocales: ["de"],
    },
  });
  await localePromise;
  Services.locale.requestedLocales = originalLocales;
});

add_task(async function test_requested_locale_string() {
  let originalLocales = Services.locale.requestedLocales;
  let localePromise = promiseLocaleChanged("fr");
  await setupPolicyEngineWithJson({
    policies: {
      RequestedLocales: "fr",
    },
  });
  await localePromise;
  Services.locale.requestedLocales = originalLocales;
});

add_task(async function test_system_locale_string() {
  let originalLocales = Services.locale.requestedLocales;

  let localePromise = promiseLocaleChanged("und");
  Services.locale.requestedLocales = ["und"];
  await localePromise;

  let systemLocale = Cc["@mozilla.org/intl/ospreferences;1"].getService(
    Ci.mozIOSPreferences
  ).systemLocale;
  localePromise = promiseLocaleChanged(systemLocale);

  await setupPolicyEngineWithJson({
    policies: {
      RequestedLocales: "",
    },
  });
  await localePromise;
  Services.locale.requestedLocales = originalLocales;
});

add_task(async function test_user_requested_locale_change() {
  let originalLocales = Services.locale.requestedLocales;
  let localePromise = promiseLocaleChanged("fr");
  await setupPolicyEngineWithJson({
    policies: {
      RequestedLocales: "fr",
    },
  });
  await localePromise;

  // Simulate user change of locale
  localePromise = promiseLocaleChanged("de");
  Services.locale.requestedLocales = ["de"];
  await localePromise;

  localePromise = promiseLocaleNotChanged("fr");
  await setupPolicyEngineWithJson({
    policies: {
      RequestedLocales: "fr",
    },
  });
  await localePromise;

  Services.locale.requestedLocales = originalLocales;
});
