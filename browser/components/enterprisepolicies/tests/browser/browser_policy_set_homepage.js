/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

registerCleanupFunction(function restore_pref_values() {
  // These two prefs are set as user prefs in case the "Locked"
  // option from this policy was not used. In this case, it won't
  // be tracked nor restored by the PoliciesPrefTracker.
  Services.prefs.clearUserPref("browser.startup.homepage");
});

add_task(async function homepage_test_simple() {
  await setupPolicyEngineWithJson({
    policies: {
      Homepage: {
        URL: "http://example1.com/",
      },
    },
  });
  await check_homepage({ expectedURL: "http://example1.com/" });
});

add_task(async function homepage_test_repeat_same_policy_value() {
  // Simulate homepage change after policy applied
  Services.prefs.setStringPref(
    "browser.startup.homepage",
    "http://example2.com/"
  );
  Services.prefs.setIntPref("browser.startup.page", 3);

  // Policy should have no effect. Homepage has not been locked and policy value
  // has not changed. We should be respecting the homepage that the user gave.
  await setupPolicyEngineWithJson({
    policies: {
      Homepage: {
        URL: "http://example1.com/",
      },
    },
  });
  await check_homepage({
    expectedURL: "http://example2.com/",
    expectedPageVal: 3,
  });
  Services.prefs.clearUserPref("browser.startup.page");
  Services.prefs.clearUserPref("browser.startup.homepage");
});

add_task(async function homepage_test_empty_additional() {
  await setupPolicyEngineWithJson({
    policies: {
      Homepage: {
        URL: "http://example1.com/",
        Additional: [],
      },
    },
  });
  await check_homepage({ expectedURL: "http://example1.com/" });
});

add_task(async function homepage_test_single_additional() {
  await setupPolicyEngineWithJson({
    policies: {
      Homepage: {
        URL: "http://example1.com/",
        Additional: ["http://example2.com/"],
      },
    },
  });
  await check_homepage({
    expectedURL: "http://example1.com/|http://example2.com/",
  });
});

add_task(async function homepage_test_multiple_additional() {
  await setupPolicyEngineWithJson({
    policies: {
      Homepage: {
        URL: "http://example1.com/",
        Additional: ["http://example2.com/", "http://example3.com/"],
      },
    },
  });
  await check_homepage({
    expectedURL:
      "http://example1.com/|http://example2.com/|http://example3.com/",
  });
});

add_task(async function homepage_test_locked() {
  await setupPolicyEngineWithJson({
    policies: {
      Homepage: {
        URL: "http://example4.com/",
        Additional: ["http://example5.com/", "http://example6.com/"],
        Locked: true,
      },
    },
  });
  await check_homepage({
    expectedURL:
      "http://example4.com/|http://example5.com/|http://example6.com/",
    locked: true,
  });
});

add_task(async function homepage_test_anchor_link() {
  await setupPolicyEngineWithJson({
    policies: {
      Homepage: {
        URL: "http://example1.com/#test",
      },
    },
  });
  await check_homepage({ expectedURL: "http://example1.com/#test" });
});
