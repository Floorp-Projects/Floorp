"use strict";

/* global XPCOMUtils, equal, Preferences, NewTabPrefsProvider, run_next_test */
/* jscs:disable requireCamelCaseOrUpperCaseIdentifiers */

const Cu = Components.utils;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NewTabPrefsProvider",
    "resource:///modules/NewTabPrefsProvider.jsm");

function run_test() {
  run_next_test();
}

add_task(function* test_observe() {
  let prefsMap = NewTabPrefsProvider.prefs.prefsMap;
  for (let prefName of prefsMap.keys()) {
    let prefValueType = prefsMap.get(prefName);

    let beforeVal;
    let afterVal;

    switch (prefValueType) {
      case "bool":
        beforeVal = false;
        afterVal = true;
        Preferences.set(prefName, beforeVal);
        break;
      case "localized":
      case "str":
        beforeVal = "";
        afterVal = "someStr";
        Preferences.set(prefName, beforeVal);
        break;
    }
    NewTabPrefsProvider.prefs.init();
    let promise = new Promise(resolve => {
      NewTabPrefsProvider.prefs.once(prefName, (name, data) => { // jshint ignore:line
        resolve([name, data]);
      });
    });
    Preferences.set(prefName, afterVal);
    let [actualName, actualData] = yield promise;
    equal(prefName, actualName, `emitter sent the correct pref: ${prefName}`);
    equal(afterVal, actualData, `emitter collected correct pref data for ${prefName}`);
    NewTabPrefsProvider.prefs.uninit();
  }
});
