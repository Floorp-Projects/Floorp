/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const OLD_PREFERENCES_TESTS = [
  {
    policies: {
      Preferences: {
        "network.IDN_show_punycode": true,
        "accessibility.force_disabled": 1,
        "security.default_personal_cert": "Select Automatically",
        "geo.enabled": 1,
        "extensions.getAddons.showPane": 0,
      },
    },
    lockedPrefs: {
      "network.IDN_show_punycode": true,
      "accessibility.force_disabled": 1,
      "security.default_personal_cert": "Select Automatically",
      "geo.enabled": true,
      "extensions.getAddons.showPane": false,
    },
  },
];

const NEW_PREFERENCES_TESTS = [
  {
    policies: {
      Preferences: {
        "browser.policies.test.default.boolean": {
          Value: true,
          Status: "default",
        },
        "browser.policies.test.default.string": {
          Value: "string",
          Status: "default",
        },
        "browser.policies.test.default.number": {
          Value: 11,
          Status: "default",
        },
        "browser.policies.test.locked.boolean": {
          Value: true,
          Status: "locked",
        },
        "browser.policies.test.locked.string": {
          Value: "string",
          Status: "locked",
        },
        "browser.policies.test.locked.number": {
          Value: 11,
          Status: "locked",
        },
        "browser.policies.test.user.boolean": {
          Value: true,
          Status: "user",
        },
        "browser.policies.test.user.string": {
          Value: "string",
          Status: "user",
        },
        "browser.policies.test.user.number": {
          Value: 11,
          Status: "user",
        },
        "browser.policies.test.default.number.implicit": {
          Value: 0,
          Status: "default",
        },
        "browser.policies.test.default.number.explicit": {
          Value: 0,
          Status: "default",
          Type: "number",
        },
      },
    },
    defaultPrefs: {
      "browser.policies.test.default.boolean": true,
      "browser.policies.test.default.string": "string",
      "browser.policies.test.default.number": 11,
      "browser.policies.test.default.number.implicit": false,
      "browser.policies.test.default.number.explicit": 0,
    },
    lockedPrefs: {
      "browser.policies.test.locked.boolean": true,
      "browser.policies.test.locked.string": "string",
      "browser.policies.test.locked.number": 11,
    },
    userPrefs: {
      "browser.policies.test.user.boolean": true,
      "browser.policies.test.user.string": "string",
      "browser.policies.test.user.number": 11,
    },
  },
  {
    policies: {
      Preferences: {
        "browser.policies.test.user.boolean": {
          Status: "clear",
        },
        "browser.policies.test.user.string": {
          Status: "clear",
        },
        "browser.policies.test.user.number": {
          Status: "clear",
        },
      },
    },

    clearPrefs: {
      "browser.policies.test.user.boolean": true,
      "browser.policies.test.user.string": "string",
      "browser.policies.test.user.number": 11,
    },
  },
];

const BAD_PREFERENCES_TESTS = [
  {
    policies: {
      Preferences: {
        "not.a.valid.branch": {
          Value: true,
          Status: "default",
        },
        "security.turn_off_all_security_so_that_viruses_can_take_over_this_computer":
          {
            Value: true,
            Status: "default",
          },
      },
    },
    defaultPrefs: {
      "not.a.valid.branch": true,
      "security.turn_off_all_security_so_that_viruses_can_take_over_this_computer": true,
    },
  },
];

add_task(async function test_old_preferences() {
  for (let test of OLD_PREFERENCES_TESTS) {
    await setupPolicyEngineWithJson({
      policies: test.policies,
    });

    info("Checking policy: " + Object.keys(test.policies)[0]);

    for (let [prefName, prefValue] of Object.entries(test.lockedPrefs || {})) {
      checkLockedPref(prefName, prefValue);
    }
  }
});

add_task(async function test_new_preferences() {
  for (let test of NEW_PREFERENCES_TESTS) {
    await setupPolicyEngineWithJson({
      policies: test.policies,
    });

    info("Checking policy: " + Object.keys(test.policies)[0]);

    for (let [prefName, prefValue] of Object.entries(test.lockedPrefs || {})) {
      checkLockedPref(prefName, prefValue);
    }

    for (let [prefName, prefValue] of Object.entries(test.defaultPrefs || {})) {
      checkDefaultPref(prefName, prefValue);
    }

    for (let [prefName, prefValue] of Object.entries(test.userPrefs || {})) {
      checkUserPref(prefName, prefValue);
    }

    for (let [prefName, prefValue] of Object.entries(test.clearPrefs || {})) {
      checkClearPref(prefName, prefValue);
    }
  }
});

add_task(async function test_bad_preferences() {
  for (let test of BAD_PREFERENCES_TESTS) {
    await setupPolicyEngineWithJson({
      policies: test.policies,
    });

    info("Checking policy: " + Object.keys(test.policies)[0]);

    for (let prefName of Object.entries(test.defaultPrefs || {})) {
      checkUnsetPref(prefName);
    }
  }
});

add_task(async function test_user_default_preference() {
  Services.prefs
    .getDefaultBranch("")
    .setBoolPref("browser.policies.test.override", true);

  await setupPolicyEngineWithJson({
    policies: {
      Preferences: {
        "browser.policies.test.override": {
          Value: true,
          Status: "user",
        },
      },
    },
  });

  checkUserPref("browser.policies.test.override", true);
});

add_task(async function test_security_preference() {
  await setupPolicyEngineWithJson({
    policies: {
      Preferences: {
        "security.this.should.not.work": {
          Value: true,
          Status: "default",
        },
      },
    },
  });

  checkUnsetPref("security.this.should.not.work");
});

add_task(async function test_JSON_preferences() {
  await setupPolicyEngineWithJson({
    policies: {
      Preferences:
        '{"browser.policies.test.default.boolean.json": {"Value": true,"Status": "default"}}',
    },
  });

  checkDefaultPref("browser.policies.test.default.boolean.json", true);
});

add_task(async function test_bug_1666836() {
  await setupPolicyEngineWithJson({
    policies: {
      Preferences: {
        "browser.tabs.warnOnClose": {
          Value: 0,
          Status: "default",
        },
      },
    },
  });

  equal(
    Preferences.get("browser.tabs.warnOnClose"),
    false,
    `browser.tabs.warnOnClose should be false`
  );
});
