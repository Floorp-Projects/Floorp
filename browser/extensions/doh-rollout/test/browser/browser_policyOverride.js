"use strict";

add_task(setup);

const { EnterprisePolicyTesting } = ChromeUtils.import(
  "resource://testing-common/EnterprisePolicyTesting.jsm",
  null
);

add_task(async function testPolicyOverride() {
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
  is(
    Preferences.get(prefs.DOH_TRR_SELECT_DRY_RUN_RESULT_PREF),
    undefined,
    "TRR selection dry run not performed."
  );
  ensureNoTRRSelectionTelemetry();
  await ensureNoTRRModeChange(0);
  await checkHeuristicsTelemetry("policy_without_doh", "first_run");

  // Simulate a network change.
  simulateNetworkChange();
  await ensureNoTRRModeChange(0);
  ensureNoHeuristicsTelemetry();

  // Restart for good measure.
  await restartAddon();
  ensureNoTRRSelectionTelemetry();
  await ensureNoTRRModeChange(0);
  ensureNoHeuristicsTelemetry();

  // Simulate a network change.
  simulateNetworkChange();
  await ensureNoTRRModeChange(0);
  ensureNoHeuristicsTelemetry();

  // Clean up.
  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {},
  });
  EnterprisePolicyTesting.resetRunOnceState();

  is(
    Services.policies.status,
    Ci.nsIEnterprisePolicies.INACTIVE,
    "Policy engine is inactive at the end of the test"
  );
});
