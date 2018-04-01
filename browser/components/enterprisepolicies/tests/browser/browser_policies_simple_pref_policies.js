/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
 * Use this file to add tests to policies that are
 * simple pref flips.
 *
 * It's best to make a test to actually test the feature
 * instead of the pref flip, but if that feature is well
 * covered by tests, including that its pref actually works,
 * it's OK to have the policy test here just to ensure
 * that the right pref values are set.
 */

const POLICIES_TESTS = [
  /*
   * Example:
   * {
   *   // Policies to be set at once through the engine
   *   policies: { "DisableFoo": true, "ConfigureBar": 42 },
   *
   *   // Locked prefs to check
   *   lockedPrefs: { "feature.foo": false },
   *
   *   // Unlocked prefs to check
   *   unlockedPrefs: { "bar.baz": 42 }
   * },
   */

   // POLICY: RememberPasswords
  {
    policies: { "RememberPasswords": false },
    lockedPrefs: { "signon.rememberSignons": false },
  },
  {
    policies: { "RememberPasswords": true },
    lockedPrefs: { "signon.rememberSignons": true },
  },

  // POLICY: DisableFormHistory
  {
    policies: { "DisableFormHistory": true },
    lockedPrefs: { "browser.formfill.enable": false },
  },

  // POLICY: EnableTrackingProtection
  {
    policies: {
      "EnableTrackingProtection": {
        "Value": true
      }
    },
    unlockedPrefs: {
      "privacy.trackingprotection.enabled": true,
      "privacy.trackingprotection.pbmode.enabled": true,
    }
  },
  {
    policies: {
      "EnableTrackingProtection": {
        "Value": false,
        "Locked": true
      }
    },
    lockedPrefs: {
      "privacy.trackingprotection.enabled": false,
      "privacy.trackingprotection.pbmode.enabled": false,
    }
  },

  // POLICY: OverrideFirstRunPage
  {
    policies: { "OverrideFirstRunPage": "https://www.example.com/" },
    lockedPrefs: { "startup.homepage_welcome_url": "https://www.example.com/" },
  },
];

add_task(async function test_policy_remember_passwords() {
  for (let test of POLICIES_TESTS) {
    await setupPolicyEngineWithJson({
      "policies": test.policies
    });

    info("Checking policy: " + Object.keys(test.policies)[0]);

    for (let [prefName, prefValue] of Object.entries(test.lockedPrefs || {})) {
      checkLockedPref(prefName, prefValue);
    }

    for (let [prefName, prefValue] of Object.entries(test.unlockedPrefs || {})) {
      checkUnlockedPref(prefName, prefValue);
    }
  }
 });
