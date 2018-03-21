/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let { Policies, setAndLockPref } = ChromeUtils.import("resource:///modules/policies/Policies.jsm", {});

add_task(async function test_API_directly() {
  setAndLockPref("policies.test.boolPref", true);
  checkLockedPref("policies.test.boolPref", true);

  // Check that a previously-locked pref can be changed
  // (it will be unlocked first).
  setAndLockPref("policies.test.boolPref", false);
  checkLockedPref("policies.test.boolPref", false);

  setAndLockPref("policies.test.intPref", 0);
  checkLockedPref("policies.test.intPref", 0);

  setAndLockPref("policies.test.stringPref", "policies test");
  checkLockedPref("policies.test.stringPref", "policies test");

  // Test that user values do not override the prefs, and the get*Pref call
  // still return the value set through setAndLockPref
  Services.prefs.setBoolPref("policies.test.boolPref", true);
  checkLockedPref("policies.test.boolPref", false);

  Services.prefs.setIntPref("policies.test.intPref", 10);
  checkLockedPref("policies.test.intPref", 0);

  Services.prefs.setStringPref("policies.test.stringPref", "policies test");
  checkLockedPref("policies.test.stringPref", "policies test");

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
    // policies.json
    {
      "policies": {
        "bool_policy": true,
        "int_policy": 42,
        "string_policy": "policies test 2"
      }
    },

    // custom schema
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
  checkLockedPref("policies.test2.boolPref", true);
  checkLockedPref("policies.test2.intPref", 42);
  checkLockedPref("policies.test2.stringPref", "policies test 2");

  delete Policies.bool_policy;
  delete Policies.int_policy;
  delete Policies.string_policy;
});
