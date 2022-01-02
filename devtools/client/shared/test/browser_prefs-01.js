/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the preference helpers work properly.

const { PrefsHelper } = require("devtools/client/shared/prefs");

function test() {
  const Prefs = new PrefsHelper("devtools.debugger", {
    foo: ["Bool", "enabled"],
  });

  const originalPrefValue = Services.prefs.getBoolPref(
    "devtools.debugger.enabled"
  );
  is(Prefs.foo, originalPrefValue, "The pref value was correctly fetched.");

  Prefs.foo = !originalPrefValue;
  is(Prefs.foo, !originalPrefValue, "The pref was was correctly changed (1).");
  is(
    Services.prefs.getBoolPref("devtools.debugger.enabled"),
    !originalPrefValue,
    "The pref was was correctly changed (2)."
  );

  Services.prefs.setBoolPref("devtools.debugger.enabled", originalPrefValue);
  info("The pref value was reset (1).");
  is(
    Prefs.foo,
    !originalPrefValue,
    "The cached pref value hasn't changed yet (1)."
  );

  Services.prefs.setBoolPref("devtools.debugger.enabled", !originalPrefValue);
  info("The pref value was reset (2).");
  is(
    Prefs.foo,
    !originalPrefValue,
    "The cached pref value hasn't changed yet (2)."
  );

  Prefs.registerObserver();

  Services.prefs.setBoolPref("devtools.debugger.enabled", originalPrefValue);
  info("The pref value was reset (3).");
  is(Prefs.foo, originalPrefValue, "The cached pref value has changed now.");

  Prefs.unregisterObserver();

  finish();
}
