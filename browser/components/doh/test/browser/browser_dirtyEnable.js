"use strict";

add_task(setup);

add_task(async function testDirtyEnable() {
  // Set up a failing environment, pre-set DoH to enabled, and verify that
  // when the add-on is enabled, it doesn't do anything - DoH remains turned on.
  setFailingHeuristics();
  let prefPromise = TestUtils.waitForPrefChange(prefs.DOH_DISABLED_PREF);
  Preferences.set(prefs.NETWORK_TRR_MODE_PREF, 2);
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
  is(Preferences.get(prefs.NETWORK_TRR_MODE_PREF), 2, "TRR mode preserved.");
  ensureNoTRRSelectionTelemetry();
  await ensureNoTRRModeChange(undefined);
  ensureNoHeuristicsTelemetry();

  // Simulate a network change.
  simulateNetworkChange();
  await ensureNoTRRModeChange(undefined);
  ensureNoHeuristicsTelemetry();
  is(Preferences.get(prefs.NETWORK_TRR_MODE_PREF), 2, "TRR mode preserved.");

  // Restart for good measure.
  await restartAddon();
  await ensureNoTRRModeChange(undefined);
  ensureNoTRRSelectionTelemetry();
  ensureNoHeuristicsTelemetry();
  is(Preferences.get(prefs.NETWORK_TRR_MODE_PREF), 2, "TRR mode preserved.");

  // Simulate a network change.
  simulateNetworkChange();
  await ensureNoTRRModeChange(undefined);
  is(Preferences.get(prefs.NETWORK_TRR_MODE_PREF), 2, "TRR mode preserved.");
  ensureNoHeuristicsTelemetry();
});
