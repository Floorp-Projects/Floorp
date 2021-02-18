/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

add_task(setup);

add_task(async function testTRRModeMigration() {
  // Test that previous TRR mode migration is correctly done - the dirtyEnable
  // test verifies that the migration is not performed when unnecessary.
  await DoHController._uninit();
  setPassingHeuristics();
  Preferences.set(prefs.NETWORK_TRR_MODE_PREF, 2);
  Preferences.set(prefs.PREVIOUS_TRR_MODE_PREF, 0);
  let modePromise = TestUtils.waitForPrefChange(prefs.NETWORK_TRR_MODE_PREF);
  let previousModePromise = TestUtils.waitForPrefChange(
    prefs.PREVIOUS_TRR_MODE_PREF
  );
  await DoHController.init();
  await Promise.all([modePromise, previousModePromise]);

  is(
    Preferences.get(prefs.PREVIOUS_TRR_MODE_PREF),
    undefined,
    "Previous TRR mode pref cleared."
  );
  is(
    Preferences.isSet(prefs.NETWORK_TRR_MODE_PREF),
    false,
    "TRR mode cleared."
  );

  await DoHController._uninit();
  Preferences.set(prefs.NETWORK_TRR_MODE_PREF, 5);
  Preferences.set(prefs.PREVIOUS_TRR_MODE_PREF, 0);
  previousModePromise = TestUtils.waitForPrefChange(
    prefs.PREVIOUS_TRR_MODE_PREF
  );
  await DoHController.init();
  try {
    await TestUtils.waitForCondition(
      () => !Preferences.isSet(prefs.NETWORK_TRR_MODE_PREF)
    );
    ok(false, "TRR mode should not have been cleared after migration");
  } catch (e) {
    ok(true, "TRR mode wasn't cleared after migration");
  }
  await previousModePromise;
  is(
    Preferences.get(prefs.PREVIOUS_TRR_MODE_PREF),
    undefined,
    "Previous TRR mode pref cleared."
  );
  is(Preferences.get(prefs.NETWORK_TRR_MODE_PREF), 5, "TRR mode remained 5.");
});
