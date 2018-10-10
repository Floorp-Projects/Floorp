/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const REQ_LOC_CHANGE_EVENT = "intl:requested-locales-changed";

function promiseLocaleChanged() {
  return new Promise(resolve => {
    let localeObserver = {
      observe(aSubject, aTopic, aData) {
        switch (aTopic) {
          case REQ_LOC_CHANGE_EVENT:
            let reqLocs = Services.locale.requestedLocales;
            is(reqLocs[0], "de");
            Services.obs.removeObserver(localeObserver, REQ_LOC_CHANGE_EVENT);
            resolve();
        }
      },
    };
    Services.obs.addObserver(localeObserver, REQ_LOC_CHANGE_EVENT);
  });
}

add_task(async function test_requested_locale() {
  let localePromise = promiseLocaleChanged();
  await setupPolicyEngineWithJson({
    "policies": {
      "RequestedLocales": ["de"],
    },
  });
  await localePromise;
});
