/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const REQ_LOC_CHANGE_EVENT = "intl:requested-locales-changed";

function promiseLocaleChanged(requestedLocale) {
  return new Promise(resolve => {
    let localeObserver = {
      observe(aSubject, aTopic, aData) {
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

add_task(async function test_requested_locale_array() {
  let originalLocales = Services.locale.requestedLocales;
  let localePromise = promiseLocaleChanged("de");
  await setupPolicyEngineWithJson({
    "policies": {
      "RequestedLocales": ["de"],
    },
  });
  await localePromise;
  Services.locale.requestedLocales = originalLocales;
});

add_task(async function test_requested_locale_string() {
  let originalLocales = Services.locale.requestedLocales;
  let localePromise = promiseLocaleChanged("fr");
  await setupPolicyEngineWithJson({
    "policies": {
      "RequestedLocales": "fr",
    },
  });
  await localePromise;
  Services.locale.requestedLocales = originalLocales;
});
