"use strict";

add_task(setup);

add_task(async function testDirtyEnable() {
  // Set up a failing environment, pre-set DoH to enabled, and verify that
  // when the add-on is enabled, it doesn't do anything - DoH remains turned on.
  setFailingHeuristics();
  let prefPromise = TestUtils.waitForPrefChange(prefs.DOH_DISABLED_PREF);
  Preferences.set(prefs.TRR_MODE_PREF, 2);
  Preferences.set(prefs.DOH_ENABLED_PREF, true);
  await prefPromise;
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
    Preferences.get(prefs.DOH_TRR_SELECT_URI_PREF),
    undefined,
    "TRR selection not performed."
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
