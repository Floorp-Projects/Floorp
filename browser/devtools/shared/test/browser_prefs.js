/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that ViewHelpers.Prefs work properly.

let {ViewHelpers} = Cu.import("resource:///modules/devtools/ViewHelpers.jsm", {});

function test() {
  let Prefs = new ViewHelpers.Prefs("devtools.debugger", {
    "foo": ["Bool", "enabled"]
  });

  let originalPrefValue = Services.prefs.getBoolPref("devtools.debugger.enabled");
  is(Prefs.foo, originalPrefValue, "The pref value was correctly fetched.");

  Prefs.foo = !originalPrefValue;
  is(Prefs.foo, !originalPrefValue,
    "The pref was was correctly changed (1).");
  is(Services.prefs.getBoolPref("devtools.debugger.enabled"), !originalPrefValue,
    "The pref was was correctly changed (2).");

  Services.prefs.setBoolPref("devtools.debugger.enabled", originalPrefValue);
  info("The pref value was reset.");

  is(Prefs.foo, !originalPrefValue,
    "The cached pref value hasn't changed yet.");

  Prefs.refresh();
  is(Prefs.foo, originalPrefValue,
    "The cached pref value has changed now.");

  finish();
}
