"use strict";

add_task(async function testDoorhangerUserReject() {
  await waitForBalrogMigration();

  // Set up a passing environment and enable DoH.
  setPassingHeuristics();
  let promise = waitForDoorhanger();
  Preferences.set(prefs.DOH_ENABLED_PREF, true);

  await BrowserTestUtils.waitForCondition(() => {
    return Preferences.get(prefs.DOH_SELF_ENABLED_PREF);
  });
  is(Preferences.get(prefs.DOH_SELF_ENABLED_PREF), true, "Breadcrumb saved.");

  let panel = await promise;
  is(
    Preferences.get(prefs.DOH_DOORHANGER_SHOWN_PREF),
    undefined,
    "Doorhanger shown pref undefined before user interaction."
  );

  await ensureTRRMode(2);

  // Click the doorhanger's "reject" button.
  let button = panel.querySelector(".popup-notification-secondary-button");
  promise = BrowserTestUtils.waitForEvent(panel, "popuphidden");
  EventUtils.synthesizeMouseAtCenter(button, {});
  await promise;

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
    "UIDisabled",
    "Doorhanger decision saved."
  );
  is(
    Preferences.get(prefs.DOH_SELF_ENABLED_PREF),
    undefined,
    "Breadcrumb cleared."
  );

  await ensureTRRMode(5);

  // Simulate a network change.
  simulateNetworkChange();
  await ensureNoTRRModeChange(5);

  // Restart the add-on for good measure.
  await restartAddon();
  await ensureNoTRRModeChange(5);

  // Set failing environment and trigger another network change.
  setFailingHeuristics();
  simulateNetworkChange();
  await ensureNoTRRModeChange(5);

  // Clean up.
  await resetPrefsAndRestartAddon();
});
