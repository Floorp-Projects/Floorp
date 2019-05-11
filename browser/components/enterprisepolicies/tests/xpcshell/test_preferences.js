/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* This test is not intended to test all preferences that
   can be set with Preferences, just a subset to verify
   the overall functionality */

"use strict";

add_task(async function test_policy_preferences() {
  await setupPolicyEngineWithJson({
    "policies": {
      "Preferences": {
        "network.IDN_show_punycode": true,
        "app.update.log": true,
      },
    },
  });

  checkLockedPref("network.IDN_show_punycode", true);
  equal(Services.prefs.getBoolPref("app.update.log"), false, "Disallowed pref was not been changed");
 });
