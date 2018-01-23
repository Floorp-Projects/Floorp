/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function test_clean_slate() {
  await startWithCleanSlate();
});

let { Policies, setAndLockPref } = Cu.import("resource:///modules/policies/Policies.jsm", {});

function checkPref(prefName, expectedValue) {
  let prefType, prefValue;
  switch (typeof(expectedValue)) {
    case "boolean":
      prefType = Services.prefs.PREF_BOOL;
      prefValue = Services.prefs.getBoolPref(prefName);
      break;

    case "number":
      prefType = Services.prefs.PREF_INT;
      prefValue = Services.prefs.getIntPref(prefName);
      break;

    case "string":
      prefType = Services.prefs.PREF_STRING;
      prefValue = Services.prefs.getStringPref(prefName);
      break;
  }

  ok(Services.prefs.prefIsLocked(prefName), `Pref ${prefName} is correctly locked`);
  is(Services.prefs.getPrefType(prefName), prefType, `Pref ${prefName} has the correct type`);
  is(prefValue, expectedValue, `Pref ${prefName} has the correct value`);
}

add_task(async function test_API_directly() {
  setAndLockPref("policies.test.boolPref", true);
  checkPref("policies.test.boolPref", true);

  // Check that a previously-locked pref can be changed
  // (it will be unlocked first).
  setAndLockPref("policies.test.boolPref", false);
  checkPref("policies.test.boolPref", false);

  setAndLockPref("policies.test.intPref", 0);
  checkPref("policies.test.intPref", 0);

  setAndLockPref("policies.test.stringPref", "policies test");
  checkPref("policies.test.stringPref", "policies test");

  // Test that user values do not override the prefs, and the get*Pref call
  // still return the value set through setAndLockPref
  Services.prefs.setBoolPref("policies.test.boolPref", true);
  checkPref("policies.test.boolPref", false);

  Services.prefs.setIntPref("policies.test.intPref", 10);
  checkPref("policies.test.intPref", 0);

  Services.prefs.setStringPref("policies.test.stringPref", "policies test");
  checkPref("policies.test.stringPref", "policies test");

  try {
    // Test that a non-integer value is correctly rejected, even though
    // typeof(val) == "number"
    setAndLockPref("policies.test.intPref", 1.5);
    ok(false, "Integer value should be rejected");
  } catch (ex) {
    ok(true, "Integer value was rejected");
  }
});

add_task(async function test_API_through_policies() {
  // Ensure that the values received by the policies have the correct
  // type to make sure things are properly working.

  // Implement functions to handle the three simple policies
  // that will be added to the schema.
  Policies.bool_policy = {
    onBeforeUIStartup(manager, param) {
      setAndLockPref("policies.test2.boolPref", param);
    }
  };

  Policies.int_policy = {
    onBeforeUIStartup(manager, param) {
      setAndLockPref("policies.test2.intPref", param);
    }
  };

  Policies.string_policy = {
    onBeforeUIStartup(manager, param) {
      setAndLockPref("policies.test2.stringPref", param);
    }
  };

  await setupPolicyEngineWithJson(
    "config_setAndLockPref.json",
    /* custom schema */
    {
      properties: {
        "bool_policy": {
          "type": "boolean"
        },

        "int_policy": {
          "type": "integer"
        },

        "string_policy": {
          "type": "string"
        }
      }
    }
  );

  is(Services.policies.status, Ci.nsIEnterprisePolicies.ACTIVE, "Engine is active");

  // The expected values come from config_setAndLockPref.json
  checkPref("policies.test2.boolPref", true);
  checkPref("policies.test2.intPref", 42);
  checkPref("policies.test2.stringPref", "policies test 2");

  delete Policies.bool_policy;
  delete Policies.int_policy;
  delete Policies.string_policy;
});
