"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "AddonManager",
  "resource://gre/modules/AddonManager.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "Preferences",
  "resource://gre/modules/Preferences.jsm"
);

const ADDON_ID = "doh-rollout@mozilla.org";

const prefs = {
  DOH_ENABLED_PREF: "doh-rollout.enabled",
  TRR_MODE_PREF: "network.trr.mode",
  DOH_SELF_ENABLED_PREF: "doh-rollout.self-enabled",
  DOH_PREVIOUS_TRR_MODE_PREF: "doh-rollout.previous.trr.mode",
  DOH_DOORHANGER_SHOWN_PREF: "doh-rollout.doorhanger-shown",
  DOH_DOORHANGER_USER_DECISION_PREF: "doh-rollout.doorhanger-decision",
  DOH_DISABLED_PREF: "doh-rollout.disable-heuristics",
  DOH_SKIP_HEURISTICS_PREF: "doh-rollout.skipHeuristicsCheck",
  DOH_DONE_FIRST_RUN_PREF: "doh-rollout.doneFirstRun",
  DOH_BALROG_MIGRATION_PREF: "doh-rollout.balrog-migration-done",
  DOH_DEBUG_PREF: "doh-rollout.debug",
  MOCK_HEURISTICS_PREF: "doh-rollout.heuristics.mockValues",
};

const fakePassingHeuristics = JSON.stringify({
  google: "enable_doh",
  youtube: "enable_doh",
  zscalerCanary: "enable_doh",
  canary: "enable_doh",
  modifiedRoots: "enable_doh",
  browserParent: "enable_doh",
  thirdPartyRoots: "enable_doh",
  policy: "enable_doh",
});

const fakeFailingHeuristics = JSON.stringify({
  google: "disable_doh",
  youtube: "disable_doh",
  zscalerCanary: "disable_doh",
  canary: "disable_doh",
  modifiedRoots: "disable_doh",
  browserParent: "disable_doh",
  thirdPartyRoots: "disable_doh",
  policy: "disable_doh",
});

async function setup() {
  SpecialPowers.pushPrefEnv({
    set: [["security.notification_enable_delay", 0]],
  });
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;
  Services.telemetry.clearEvents();

  registerCleanupFunction(() => {
    Services.telemetry.canRecordExtended = oldCanRecord;
    Services.telemetry.clearEvents();
  });
}

function setPassingHeuristics() {
  Preferences.set(prefs.MOCK_HEURISTICS_PREF, fakePassingHeuristics);
}

function setFailingHeuristics() {
  Preferences.set(prefs.MOCK_HEURISTICS_PREF, fakeFailingHeuristics);
}

async function restartAddon() {
  let addon = await AddonManager.getAddonByID(ADDON_ID);
  await addon.reload();
}

async function resetPrefsAndRestartAddon() {
  let addon = await AddonManager.getAddonByID(ADDON_ID);
  await addon.disable({ allowSystemAddons: true });
  Preferences.reset(Object.values(prefs));
  await addon.enable({ allowSystemAddons: true });
}

async function waitForDoorhanger() {
  const notificationID = "doh-first-time";
  let panel;
  await BrowserTestUtils.waitForEvent(document, "popupshown", true, event => {
    panel = event.originalTarget;
    let popupNotification = event.originalTarget.firstChild;
    return (
      popupNotification &&
      popupNotification.notification &&
      popupNotification.notification.id == notificationID
    );
  });
  return panel;
}

function simulateNetworkChange() {
  Services.obs.notifyObservers(null, "network:link-status-changed", "down");
  Services.obs.notifyObservers(null, "network:link-status-changed", "up");
}

async function ensureTRRMode(mode) {
  await BrowserTestUtils.waitForCondition(() => {
    return Preferences.get(prefs.TRR_MODE_PREF) == mode;
  });
  is(Preferences.get(prefs.TRR_MODE_PREF), mode, `TRR mode is ${mode}`);
}

async function ensureNoTRRModeChange(mode) {
  try {
    // Try and wait for the TRR pref to change... waitForCondition should throw
    // after trying for a while.
    await BrowserTestUtils.waitForCondition(() => {
      return Preferences.get(prefs.TRR_MODE_PREF, -1) !== mode;
    });
    // If we reach this, the waitForCondition didn't throw. Fail!
    ok(false, "TRR mode changed when it shouldn't have!");
  } catch (e) {
    // Assert for clarity.
    is(Preferences.get(prefs.TRR_MODE_PREF, -1), mode, "No change in TRR mode");
  }
}
