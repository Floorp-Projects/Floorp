"use strict";

add_task(setup);

add_task(async function testDoorhanger() {
  Preferences.reset(prefs.PROFILE_CREATION_THRESHOLD_PREF);
  // Set up a passing environment and enable DoH.
  setPassingHeuristics();
  Preferences.set(prefs.DOH_ENABLED_PREF, true);

  await BrowserTestUtils.waitForCondition(() => {
    return Preferences.get(prefs.DOH_SELF_ENABLED_PREF);
  });
  is(Preferences.get(prefs.DOH_SELF_ENABLED_PREF), true, "Breadcrumb saved.");
  is(
    Preferences.get(prefs.DOH_TRR_SELECT_DRY_RUN_RESULT_PREF),
    "dummyTRR",
    "TRR selection dry run complete."
  );
  await checkTRRSelectionTelemetry();

  await BrowserTestUtils.waitForCondition(() => {
    return Preferences.get(prefs.DOH_DOORHANGER_SHOWN_PREF);
  });
  is(
    Preferences.get(prefs.DOH_DOORHANGER_SHOWN_PREF),
    true,
    "Doorhanger shown pref saved."
  );
  is(
    Preferences.get(prefs.DOH_DOORHANGER_USER_DECISION_PREF),
    "NewProfile",
    "Doorhanger decision saved."
  );
  is(
    Preferences.get(prefs.DOH_SELF_ENABLED_PREF),
    true,
    "Breadcrumb not cleared."
  );
});
