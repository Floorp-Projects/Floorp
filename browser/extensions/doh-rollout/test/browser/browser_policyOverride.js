"use strict";

const { EnterprisePolicyTesting } = ChromeUtils.import(
  "resource://testing-common/EnterprisePolicyTesting.jsm",
  null
);

add_task(async function testPolicyOverride() {
  await waitForBalrogMigration();

  // Set up an arbitrary enterprise policy. Its existence should be sufficient
  // to disable heuristics.
  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {
      EnableTrackingProtection: {
        Value: true,
      },
    },
  });
  is(
    Services.policies.status,
    Ci.nsIEnterprisePolicies.ACTIVE,
    "Policy engine is active."
  );

  Preferences.set(prefs.DOH_ENABLED_PREF, true);
  await BrowserTestUtils.waitForCondition(() => {
    return Preferences.get(prefs.DOH_SKIP_HEURISTICS_PREF, false);
  });
  is(
    Preferences.get(prefs.DOH_SKIP_HEURISTICS_PREF, false),
    true,
    "skipHeuristicsCheck pref is set to remember not to run heuristics."
  );
  is(
    Preferences.get(prefs.DOH_SELF_ENABLED_PREF),
    undefined,
    "Breadcrumb not saved."
  );
  await ensureNoTRRModeChange(0);

  // Simulate a network change.
  simulateNetworkChange();
  await ensureNoTRRModeChange(0);

  // Restart for good measure.
  await restartAddon();
  await ensureNoTRRModeChange(0);

  // Simulate a network change.
  simulateNetworkChange();
  await ensureNoTRRModeChange(0);

  // Clean up.
  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {},
  });
  EnterprisePolicyTesting.resetRunOnceState();
  await resetPrefsAndRestartAddon();

  is(
    Services.policies.status,
    Ci.nsIEnterprisePolicies.INACTIVE,
    "Policy engine is inactive at the end of the test"
  );
});
