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

const { CommonUtils } = ChromeUtils.import(
  "resource://services-common/utils.js"
);

const ADDON_ID = "doh-rollout@mozilla.org";

const EXAMPLE_URL = "https://example.com/";

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
  DOH_TRR_SELECT_DRY_RUN_RESULT_PREF:
    "doh-rollout.trr-selection.dry-run-result",
  MOCK_HEURISTICS_PREF: "doh-rollout.heuristics.mockValues",
  PROFILE_CREATION_THRESHOLD_PREF: "doh-rollout.profileCreationThreshold",
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

  // Set the profile creation threshold to very far in the future by defualt,
  // so that we can test the doorhanger. browser_doorhanger_newProfile.js
  // overrides this.
  Preferences.set(prefs.PROFILE_CREATION_THRESHOLD_PREF, "99999999999999");

  registerCleanupFunction(async () => {
    Services.telemetry.canRecordExtended = oldCanRecord;
    Services.telemetry.clearEvents();
    await resetPrefsAndRestartAddon();
  });
}

async function checkTRRSelectionTelemetry() {
  let events;
  await BrowserTestUtils.waitForCondition(() => {
    events = Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS
    ).parent;
    return events;
  });
  events = events.filter(
    e =>
      e[1] == "security.doh.trrPerformance" &&
      e[2] == "trrselect" &&
      e[3] == "dryrunresult"
  );
  is(events.length, 1, "Found the expected trrselect event.");
  is(events[0][4], "dummyTRR", "The event records the expected decision");
}

function ensureNoTRRSelectionTelemetry() {
  let events = Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS
  ).parent;
  if (!events) {
    ok(true, "Found no trrselect events.");
    return;
  }
  events = events.filter(
    e =>
      e[1] == "security.doh.trrPerformance" &&
      e[2] == "trrselect" &&
      e[3] == "dryrunresult"
  );
  is(events.length, 0, "Found no trrselect events.");
}

async function checkHeuristicsTelemetry(decision, evaluateReason) {
  let events;
  await BrowserTestUtils.waitForCondition(() => {
    events = Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS
    ).dynamic;
    return events;
  });
  events = events.filter(
    e => e[1] == "doh" && e[2] == "evaluate" && e[3] == "heuristics"
  );
  is(events.length, 1, "Found the expected heuristics event.");
  is(events[0][4], decision, "The event records the expected decision");
  if (evaluateReason) {
    is(events[0][5].evaluateReason, evaluateReason, "Got the expected reason.");
  }

  // After checking the event, clear all telemetry. Since we check for a single
  // event above, this ensures all heuristics events are intentional and tested.
  // TODO: Test events other than heuristics. Those tests would also work the
  // same way, so as to test one event at a time, and this clearEvents() call
  // will continue to exist as-is.
  Services.telemetry.clearEvents();
}

function ensureNoHeuristicsTelemetry() {
  let events = Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS
  ).dynamic;
  if (!events) {
    ok(true, "Found no heuristics events.");
    return;
  }
  events = events.filter(
    e => e[1] == "doh" && e[2] == "evaluate" && e[3] == "heuristics"
  );
  is(events.length, 0, "Found no heuristics events.");
}

async function waitForStateTelemetry() {
  let events;
  await BrowserTestUtils.waitForCondition(() => {
    events = Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS
    ).dynamic;
    return events;
  });
  events = events.filter(e => e[1] == "doh" && e[2] == "state");
  is(events.length, 1, "Found the expected state event.");
  Services.telemetry.clearEvents();
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

async function disableAddon() {
  let addon = await AddonManager.getAddonByID(ADDON_ID);
  await addon.disable({ allowSystemAddons: true });
}

async function enableAddon() {
  let addon = await AddonManager.getAddonByID(ADDON_ID);
  await addon.enable({ allowSystemAddons: true });
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
  // The networkStatus API does not actually propagate the link status we supply
  // here, but rather sends the link status from the NetworkLinkService.
  // This means there's no point sending a down and then an up - the extension
  // will just receive "up" twice.
  // TODO: Implement a mock NetworkLinkService and use it to also simulate
  // network down events.
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
