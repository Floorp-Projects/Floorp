"use strict";

add_task(setup);

add_task(async function testTRRModeMigration() {
  // Test that previous TRR mode migration is correctly done - the dirtyEnable
  // test verifies that the migration is not performed when unnecessary.
  setPassingHeuristics();
  Preferences.set(prefs.DOH_SELF_ENABLED_PREF, true);
  Preferences.set(prefs.NETWORK_TRR_MODE_PREF, 2);
  Preferences.set(prefs.DOH_PREVIOUS_TRR_MODE_PREF, 0);
  let modePromise = TestUtils.waitForPrefChange(prefs.NETWORK_TRR_MODE_PREF);
  let previousModePromise = TestUtils.waitForPrefChange(
    prefs.DOH_PREVIOUS_TRR_MODE_PREF
  );
  Preferences.set(prefs.DOH_ENABLED_PREF, true);
  await Promise.all([modePromise, previousModePromise]);
  await ensureTRRMode(2);
  await checkTRRSelectionTelemetry();
  await checkHeuristicsTelemetry("enable_doh", "startup");

  is(
    Preferences.get(prefs.DOH_PREVIOUS_TRR_MODE_PREF),
    undefined,
    "Previous TRR mode pref cleared."
  );
  is(
    Preferences.isSet(prefs.NETWORK_TRR_MODE_PREF),
    false,
    "TRR mode cleared."
  );
});
