"use strict";

add_task(setup);

add_task(async function testTrrSelectionDisable() {
  // Set up a passing environment and enable DoH.
  Preferences.set(prefs.DOH_TRR_SELECT_ENABLED_PREF, false);
  setPassingHeuristics();
  let promise = waitForDoorhanger();
  Preferences.set(prefs.DOH_ENABLED_PREF, true);
  await BrowserTestUtils.waitForCondition(() => {
    return Preferences.get(prefs.DOH_SELF_ENABLED_PREF);
  });
  is(Preferences.get(prefs.DOH_SELF_ENABLED_PREF), true, "Breadcrumb saved.");
  is(
    Preferences.get(prefs.DOH_TRR_SELECT_DRY_RUN_RESULT_PREF),
    undefined,
    "TRR selection dry run not performed."
  );
  is(
    Preferences.get(prefs.DOH_TRR_SELECT_URI_PREF),
    undefined,
    "doh-rollout.uri remained unset."
  );
  ensureNoTRRSelectionTelemetry();

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, EXAMPLE_URL);
  let panel = await promise;
  is(
    Preferences.get(prefs.DOH_DOORHANGER_SHOWN_PREF),
    undefined,
    "Doorhanger shown pref undefined before user interaction."
  );

  // Click the doorhanger's "accept" button.
  let button = panel.querySelector(".popup-notification-primary-button");
  promise = BrowserTestUtils.waitForEvent(panel, "popuphidden");
  EventUtils.synthesizeMouseAtCenter(button, {});
  await promise;

  await ensureTRRMode(2);
  await checkHeuristicsTelemetry("enable_doh", "startup");

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
    "UIOk",
    "Doorhanger decision saved."
  );
  is(
    Preferences.get(prefs.DOH_SELF_ENABLED_PREF),
    true,
    "Breadcrumb not cleared."
  );

  BrowserTestUtils.removeTab(tab);

  // Restart the add-on for good measure.
  await restartAddon();
  ensureNoTRRSelectionTelemetry();
  is(
    Preferences.get(prefs.DOH_TRR_SELECT_DRY_RUN_RESULT_PREF),
    undefined,
    "TRR selection dry run not performed."
  );
  is(
    Preferences.get(prefs.DOH_TRR_SELECT_URI_PREF),
    undefined,
    "doh-rollout.uri remained unset."
  );
  await ensureNoTRRModeChange(2);
  await checkHeuristicsTelemetry("enable_doh", "startup");
});
