/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_policy_enable_tracking_protection1() {
  await setupPolicyEngineWithJson({
    "policies": {
      "EnableTrackingProtection": {
        "Value": true
      }
    }
  });

  is(Services.prefs.getBoolPref("privacy.trackingprotection.enabled"), true, "Tracking protection has been enabled by default.");
  is(Services.prefs.getBoolPref("privacy.trackingprotection.pbmode.enabled"), true, "Tracking protection has been enabled by default in private browsing mode.");
  is(Services.prefs.prefIsLocked("privacy.trackingprotection.enabled"), false, "Tracking protection pref is not locked.");
});

add_task(async function test_policy_enable_tracking_protection_locked() {
  await setupPolicyEngineWithJson({
    "policies": {
      "EnableTrackingProtection": {
        "Value": false,
        "Locked": true
      }
    }
  });

  is(Services.prefs.getBoolPref("privacy.trackingprotection.enabled"), false, "Tracking protection has been disabled by default.");
  is(Services.prefs.getBoolPref("privacy.trackingprotection.pbmode.enabled"), false, "Tracking protection has been disabled by default in private browsing mode.");
  is(Services.prefs.prefIsLocked("privacy.trackingprotection.enabled"), true, "Tracking protection pref is locked.");
});
