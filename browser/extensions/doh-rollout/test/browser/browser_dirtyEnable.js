"use strict";

add_task(setup);

add_task(async function testDirtyEnable() {
  // Set up a failing environment, pre-set DoH to enabled, and verify that
  // when the add-on is enabled, it doesn't do anything - DoH remains turned on.
  setFailingHeuristics();
  Preferences.set(prefs.TRR_MODE_PREF, 2);
  Preferences.set(prefs.DOH_ENABLED_PREF, true);
  await BrowserTestUtils.waitForCondition(() => {
    return Preferences.get(prefs.DOH_DISABLED_PREF, false);
  });
  is(
    Preferences.get(prefs.DOH_DISABLED_PREF, false),
    true,
    "Disabled state recorded."
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
  await ensureNoTRRModeChange(2);
  checkHeuristicsTelemetry("prefHasUserValue", "first_run");

  // Simulate a network change.
  simulateNetworkChange();
  await ensureNoTRRModeChange(2);
  ensureNoHeuristicsTelemetry();

  // Restart for good measure.
  await restartAddon();
  ensureNoTRRSelectionTelemetry();
  await ensureNoTRRModeChange(2);
  ensureNoHeuristicsTelemetry();

  // Simulate a network change.
  simulateNetworkChange();
  await ensureNoTRRModeChange(2);
  ensureNoHeuristicsTelemetry();
});
