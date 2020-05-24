"use strict";

requestLongerTimeout(2);

add_task(setup);

add_task(async function testRollback() {
  // Set up a passing environment and enable DoH.
  setPassingHeuristics();
  let promise = waitForDoorhanger();
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

  // Change the environment to failing and simulate a network change.
  setFailingHeuristics();
  simulateNetworkChange();
  await ensureTRRMode(0);
  await checkHeuristicsTelemetry("disable_doh", "netchange");

  // Trigger another network change.
  simulateNetworkChange();
  await ensureNoTRRModeChange(0);
  await checkHeuristicsTelemetry("disable_doh", "netchange");

  // Rollback!
  setPassingHeuristics();
  Preferences.reset(prefs.DOH_ENABLED_PREF);
  await waitForStateTelemetry();
  await ensureTRRMode(0);
  ensureNoTRRSelectionTelemetry();
  await ensureNoHeuristicsTelemetry();
  simulateNetworkChange();
  await ensureNoTRRModeChange(0);
  await ensureNoHeuristicsTelemetry();

  // Re-enable.
  Preferences.set(prefs.DOH_ENABLED_PREF, true);

  await ensureTRRMode(2);
  ensureNoTRRSelectionTelemetry();
  await checkHeuristicsTelemetry("enable_doh", "startup");

  // Change the environment to failing and simulate a network change.
  setFailingHeuristics();
  simulateNetworkChange();
  await ensureTRRMode(0);
  await checkHeuristicsTelemetry("disable_doh", "netchange");

  // Rollback again for good measure! This time with failing heuristics.
  Preferences.reset(prefs.DOH_ENABLED_PREF);
  await waitForStateTelemetry();
  await ensureNoTRRModeChange(0);
  ensureNoTRRSelectionTelemetry();
  await ensureNoHeuristicsTelemetry();
  simulateNetworkChange();
  await ensureNoTRRModeChange(0);
  await ensureNoHeuristicsTelemetry();

  // Re-enable.
  Preferences.set(prefs.DOH_ENABLED_PREF, true);

  await ensureNoTRRModeChange(0);
  ensureNoTRRSelectionTelemetry();
  await checkHeuristicsTelemetry("disable_doh", "startup");

  // Change the environment to passing and simulate a network change.
  setPassingHeuristics();
  simulateNetworkChange();
  await ensureTRRMode(2);
  await checkHeuristicsTelemetry("enable_doh", "netchange");

  // Rollback again, this time with TRR mode set to 2 prior to doing so.
  Preferences.reset(prefs.DOH_ENABLED_PREF);
  await waitForStateTelemetry();
  await ensureTRRMode(0);
  ensureNoTRRSelectionTelemetry();
  await ensureNoHeuristicsTelemetry();
  simulateNetworkChange();
  await ensureNoTRRModeChange(0);
  await ensureNoHeuristicsTelemetry();

  // Re-enable.
  Preferences.set(prefs.DOH_ENABLED_PREF, true);

  await ensureTRRMode(2);
  ensureNoTRRSelectionTelemetry();
  await checkHeuristicsTelemetry("enable_doh", "startup");
  simulateNetworkChange();
  await ensureNoTRRModeChange(2);
  await checkHeuristicsTelemetry("enable_doh", "netchange");

  // Rollback again. This time, disable the add-on first to ensure it reacts
  // correctly at startup.
  await disableAddon();
  Preferences.reset(prefs.DOH_ENABLED_PREF);
  await enableAddon();
  await ensureTRRMode(0);
  ensureNoTRRSelectionTelemetry();
  await ensureNoHeuristicsTelemetry();
  simulateNetworkChange();
  await ensureNoTRRModeChange(0);
  await ensureNoHeuristicsTelemetry();
});
