/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
/* global browser, runHeuristics */

let DEBUG;

async function log() {
  // eslint-disable-next-line no-constant-condition
  if (DEBUG) {
    // eslint-disable-next-line no-console
    console.log(...arguments);
  }
}

// Gate-keeping pref to run the add-on
const DOH_ENABLED_PREF = "doh-rollout.enabled";

// When in automation, we parse this string pref as JSON and use it as our
// heuristics results set. This allows tests to set up different cases and
// verify the correct response.
const MOCK_HEURISTICS_PREF = "doh-rollout.heuristics.mockValues";

// Pref that sets DoH to on/off. It has multiple modes:
// 0: Off (default)
// 1: null (No setting)
// 2: Enabled, but will fall back to 0 on DNS lookup failure
// 3: Always on.
// 4: null (No setting)
// 5: Never on.
const TRR_MODE_PREF = "network.trr.mode";

// This preference is set to TRUE when DoH has been enabled via the add-on. It will
// allow the add-on to continue to function without the aid of the Normandy-triggered pref
// of "doh-rollout.enabled". Note that instead of setting it to false, it is cleared.
const DOH_SELF_ENABLED_PREF = "doh-rollout.self-enabled";

// This pref is part of a cache mechanism to see if the heuristics dictated a change in the DoH settings
const DOH_PREVIOUS_TRR_MODE_PREF = "doh-rollout.previous.trr.mode";

// Set after doorhanger has been interacted with by the user
const DOH_DOORHANGER_SHOWN_PREF = "doh-rollout.doorhanger-shown";

// Records if the user opted in/out of DoH study by clicking on doorhanger
const DOH_DOORHANGER_USER_DECISION_PREF = "doh-rollout.doorhanger-decision";

// Records if user has decided to opt out of study, either by disabling via the doorhanger,
// unchecking "DNS-over-HTTPS" with about:preferences, or manually setting network.trr.mode
const DOH_DISABLED_PREF = "doh-rollout.disable-heuristics";

// Set to true when a user has ANY enterprise policy set, making sure to not run
// heuristics, overwritting the policy.
const DOH_SKIP_HEURISTICS_PREF = "doh-rollout.skipHeuristicsCheck";

// Records when the add-on has been run once. This is in place to only check
// network.trr.mode for prefHasUserValue on first run.
const DOH_DONE_FIRST_RUN_PREF = "doh-rollout.doneFirstRun";

// This pref is set once a migration function has ran, updating local storage items to the
// new doh-rollot.X namespace. This applies to both `doneFirstRun` and `skipHeuristicsCheck`.
const DOH_BALROG_MIGRATION_PREF = "doh-rollout.balrog-migration-done";

// If set to true, debug logging will be enabled.
const DOH_DEBUG_PREF = "doh-rollout.debug";

const stateManager = {
  async setState(state) {
    log("setState: ", state);

    switch (state) {
      case "uninstalled":
        break;
      case "disabled":
        await rollout.setSetting(TRR_MODE_PREF, 0);
        break;
      case "manuallyDisabled":
        await browser.experiments.preferences.clearUserPref(
          DOH_SELF_ENABLED_PREF
        );
        break;
      case "UIOk":
        await rollout.setSetting(DOH_SELF_ENABLED_PREF, true);
        break;
      case "enabled":
        await rollout.setSetting(TRR_MODE_PREF, 2);
        await rollout.setSetting(DOH_SELF_ENABLED_PREF, true);
        break;
      case "UIDisabled":
        await rollout.setSetting(TRR_MODE_PREF, 5);
        await browser.experiments.preferences.clearUserPref(
          DOH_SELF_ENABLED_PREF
        );
        break;
    }

    await browser.experiments.heuristics.sendStatePing(state);
    await stateManager.rememberTRRMode();
  },

  async rememberTRRMode() {
    let curMode = await browser.experiments.preferences.getIntPref(
      TRR_MODE_PREF,
      0
    );
    log("Saving current trr mode:", curMode);
    await rollout.setSetting(DOH_PREVIOUS_TRR_MODE_PREF, curMode, true);
  },

  async rememberDoorhangerShown() {
    // This will be shown on startup and network changes until a user clicks
    // to confirm/disable DoH or presses the esc key (confirming)
    log("Remembering that doorhanger has been shown");
    await rollout.setSetting(DOH_DOORHANGER_SHOWN_PREF, true);
  },

  async rememberDoorhangerDecision(decision) {
    log("Remember doorhanger decision:", decision);
    await rollout.setSetting(DOH_DOORHANGER_USER_DECISION_PREF, decision, true);
  },

  async rememberDisableHeuristics() {
    log("Remembering to never run heuristics again");
    await rollout.setSetting(DOH_DISABLED_PREF, true);
  },

  async shouldRunHeuristics() {
    // Check if heuristics has been disabled from rememberDisableHeuristics()
    let disableHeuristics = await rollout.getSetting(DOH_DISABLED_PREF, false);
    let skipHeuristicsCheck = await rollout.getSetting(
      DOH_SKIP_HEURISTICS_PREF,
      false
    );

    if (disableHeuristics || skipHeuristicsCheck) {
      // Do not modify DoH for this user.
      log("shouldRunHeuristics: Will not run heuristics");
      return false;
    }

    let prevMode = await rollout.getSetting(DOH_PREVIOUS_TRR_MODE_PREF, 0);

    let curMode = await browser.experiments.preferences.getIntPref(
      TRR_MODE_PREF,
      0
    );

    log("Comparing previous trr mode to current mode:", prevMode, curMode);

    // Don't run heuristics if:
    //  1) Previous doesn't mode equals current mode, i.e. user overrode our changes
    //  2) TRR mode equals 5, i.e. user clicked "No" on doorhanger
    //  3) TRR mode equals 3, i.e. user enabled "strictly on" for DoH
    //  4) They've been disabled in the past for the reasons listed above
    //
    // In other words, if the user has made their own decision for DoH,
    // then we want to respect that and never run the heuristics again

    if (prevMode === curMode) {
      return true;
    }

    // On Mismatch - run never run again (make init check a function)
    log("Mismatched, curMode: ", curMode);

    // Cache results for Telemetry send, including setting eval reason
    let results = await runHeuristics();
    results.evaluateReason = "userModified";
    if (curMode === 0 || curMode === 5) {
      // If user has manually set trr.mode to 0, and it was previously something else.
      browser.experiments.heuristics.sendHeuristicsPing("disable_doh", results);
      browser.experiments.preferences.clearUserPref(DOH_SELF_ENABLED_PREF);
      await stateManager.rememberDisableHeuristics();
    } else {
      // Check if trr.mode is not in default value.
      await rollout.trrModePrefHasUserValue(
        "shouldRunHeuristics_mismatch",
        results
      );
    }

    return false;
  },

  async shouldShowDoorhanger() {
    let doorhangerShown = await rollout.getSetting(
      DOH_DOORHANGER_SHOWN_PREF,
      false
    );
    log("Should show doorhanger:", !doorhangerShown);

    return !doorhangerShown;
  },

  async showDoorhanger() {
    rollout.addDoorhangerListeners();

    let doorhangerShown = await browser.experiments.doorhanger.show({
      name: browser.i18n.getMessage("doorhangerName"),
      text: "<> " + browser.i18n.getMessage("doorhangerBodyNew"),
      okLabel: browser.i18n.getMessage("doorhangerButtonOk"),
      okAccessKey: browser.i18n.getMessage("doorhangerButtonOkAccessKey"),
      cancelLabel: browser.i18n.getMessage("doorhangerButtonCancel2"),
      cancelAccessKey: browser.i18n.getMessage(
        "doorhangerButtonCancelAccessKey"
      ),
    });

    if (!doorhangerShown) {
      // The profile was created after the go-live date of the privacy statement
      // that included DoH. Treat it as accepted.
      log("Profile is new, doorhanger not shown.");
      await stateManager.setState("UIOk");
      await stateManager.rememberDoorhangerDecision("NewProfile");
      await stateManager.rememberDoorhangerShown();
      rollout.removeDoorhangerListeners();
    }
  },
};

const rollout = {
  // Pretend that there was a network change at the beginning of time.
  lastNetworkChangeTime: 0,

  async isTesting() {
    if (this._isTesting === undefined) {
      this._isTesting = await browser.experiments.heuristics.isTesting();
    }

    return this._isTesting;
  },

  addDoorhangerListeners() {
    browser.experiments.doorhanger.onDoorhangerAccept.addListener(
      rollout.doorhangerAcceptListener
    );

    browser.experiments.doorhanger.onDoorhangerDecline.addListener(
      rollout.doorhangerDeclineListener
    );
  },

  removeDoorhangerListeners() {
    browser.experiments.doorhanger.onDoorhangerAccept.removeListener(
      rollout.doorhangerAcceptListener
    );

    browser.experiments.doorhanger.onDoorhangerDecline.removeListener(
      rollout.doorhangerDeclineListener
    );
  },

  async doorhangerAcceptListener(tabId) {
    log("Doorhanger accepted on tab", tabId);
    await stateManager.setState("UIOk");
    await stateManager.rememberDoorhangerDecision("UIOk");
    await stateManager.rememberDoorhangerShown();
    rollout.removeDoorhangerListeners();
  },

  async doorhangerDeclineListener(tabId) {
    log("Doorhanger declined on tab", tabId);
    await stateManager.setState("UIDisabled");
    await stateManager.rememberDoorhangerDecision("UIDisabled");
    let results = await runHeuristics();
    results.evaluateReason = "doorhangerDecline";
    browser.experiments.heuristics.sendHeuristicsPing("disable_doh", results);
    await stateManager.rememberDisableHeuristics();
    await stateManager.rememberDoorhangerShown();
    rollout.removeDoorhangerListeners();
  },

  async heuristics(evaluateReason) {
    let shouldRunHeuristics = await stateManager.shouldRunHeuristics();

    if (!shouldRunHeuristics) {
      return;
    }

    // Run heuristics defined in heuristics.js and experiments/heuristics/api.js
    let results;

    if (await rollout.isTesting()) {
      results = await browser.experiments.preferences.getCharPref(
        MOCK_HEURISTICS_PREF,
        `{ "test": "disable_doh" }`
      );
      results = JSON.parse(results);
    } else {
      results = await runHeuristics();
    }

    // Check if DoH should be disabled
    let decision = Object.values(results).includes("disable_doh")
      ? "disable_doh"
      : "enable_doh";

    log("Heuristics decision on " + evaluateReason + ": " + decision);

    // Send Telemetry on results of heuristics
    results.evaluateReason = evaluateReason;
    browser.experiments.heuristics.sendHeuristicsPing(decision, results);

    if (decision === "disable_doh") {
      await stateManager.setState("disabled");
    } else {
      await stateManager.setState("enabled");
      if (await stateManager.shouldShowDoorhanger()) {
        await stateManager.showDoorhanger();
      }
    }
  },

  async getSetting(name, defaultValue) {
    let value;

    switch (typeof defaultValue) {
      case "boolean":
        value = await browser.experiments.preferences.getBoolPref(
          name,
          defaultValue
        );
        break;
      case "number":
        value = await browser.experiments.preferences.getIntPref(
          name,
          defaultValue
        );
        break;
      case "string":
        value = await browser.experiments.preferences.getCharPref(
          name,
          defaultValue
        );
        break;
      default:
        throw new Error(
          `Invalid defaultValue argument when trying to fetch pref: ${JSON.stringify(
            name
          )}`
        );
    }

    log({
      context: "getSetting",
      type: typeof defaultValue,
      name,
      value,
    });

    return value;
  },

  /**
   * Exposed
   *
   * @param  {type} name  description
   * @param  {type} value description
   * @return {type}       description
   */
  async setSetting(name, value) {
    // Based on type of pref, set pref accordingly
    switch (typeof value) {
      case "boolean":
        await browser.experiments.preferences.setBoolPref(name, value);
        break;
      case "number":
        await browser.experiments.preferences.setIntPref(name, value);
        break;
      case "string":
        await browser.experiments.preferences.setCharPref(name, value);
        break;
      default:
        throw new Error("setSetting typeof value unknown!");
    }

    log({
      context: "setSetting",
      type: typeof value,
      name,
      value,
    });
  },

  async trrModePrefHasUserValue(event, results) {
    results.evaluateReason = event;

    // This confirms if a user has modified DoH (via the TRR_MODE_PREF) outside of the addon
    // This runs only on the FIRST time that add-on is enabled and if the stored pref
    // mismatches the current pref (Meaning something outside of the add-on has changed it)

    if (await browser.experiments.preferences.prefHasUserValue(TRR_MODE_PREF)) {
      // Send ping that user had specific trr.mode pref set before add-on study was ran.
      // Note that this does not include the trr.mode - just that the addon cannot be ran.
      browser.experiments.heuristics.sendHeuristicsPing(
        "prefHasUserValue",
        results
      );

      browser.experiments.preferences.clearUserPref(DOH_SELF_ENABLED_PREF);
      await this.setSetting(DOH_SKIP_HEURISTICS_PREF, true);
      await stateManager.rememberDisableHeuristics();
    }
  },

  async enterprisePolicyCheck(event, results) {
    results.evaluateReason = event;

    // Check if trrModePrefHasUserValue determined to not enable add-on on first run
    let skipHeuristicsCheck = await rollout.getSetting(
      DOH_SKIP_HEURISTICS_PREF,
      false
    );

    if (skipHeuristicsCheck) {
      return;
    }

    // Check for Policies before running the rest of the heuristics
    let policyEnableDoH = await browser.experiments.heuristics.checkEnterprisePolicies();

    log("Enterprise Policy Check:", policyEnableDoH);

    // Determine to skip additional heuristics (by presence of an enterprise policy)

    if (policyEnableDoH === "no_policy_set") {
      // Resetting skipHeuristicsCheck in case a user had a policy and then removed it!
      await this.setSetting(DOH_SKIP_HEURISTICS_PREF, false);
      return;
    }

    if (policyEnableDoH === "policy_without_doh") {
      await stateManager.setState("disabled");
    }

    // Don't check for prefHasUserValue if policy is set to disable DoH
    await this.setSetting(DOH_SKIP_HEURISTICS_PREF, true);

    browser.experiments.heuristics.sendHeuristicsPing(policyEnableDoH, results);
  },

  async migrateLocalStoragePrefs() {
    // Migrate updated local storage item names. If this has already been done once, skip the migration
    const isMigrated = await browser.experiments.preferences.getBoolPref(
      DOH_BALROG_MIGRATION_PREF,
      false
    );

    if (isMigrated) {
      log("User has already been migrated.");
      return;
    }

    // Check all local storage keys from v1.0.4 users and migrate them to prefs.
    // This only applies to keys that have a value.
    const legacyLocalStorageKeys = [
      "doneFirstRun",
      "skipHeuristicsCheck",
      DOH_PREVIOUS_TRR_MODE_PREF,
      DOH_DOORHANGER_SHOWN_PREF,
      DOH_DOORHANGER_USER_DECISION_PREF,
      DOH_DISABLED_PREF,
    ];

    for (let item of legacyLocalStorageKeys) {
      let data = await browser.storage.local.get(item);
      let value = data[item];

      log({ context: "migration", item, value });

      if (data.hasOwnProperty(item)) {
        let migratedName = item;

        if (!item.startsWith("doh-rollout.")) {
          migratedName = "doh-rollout." + item;
        }

        await this.setSetting(migratedName, value);
      }
    }

    // Set pref to skip this function in the future.
    browser.experiments.preferences.setBoolPref(
      DOH_BALROG_MIGRATION_PREF,
      true
    );

    log("User successfully migrated.");
  },

  async init() {
    log("calling init");
    // Check if the add-on has run before
    let doneFirstRun = await this.getSetting(DOH_DONE_FIRST_RUN_PREF, false);

    // Register the events for sending pings
    browser.experiments.heuristics.setupTelemetry();

    // Cache runHeuristics results for first run/start up checks
    let results = await runHeuristics();

    if (!doneFirstRun) {
      log("first run!");
      await this.setSetting(DOH_DONE_FIRST_RUN_PREF, true);
      // Check if user has a set a custom pref only on first run, not on each startup
      await this.trrModePrefHasUserValue("first_run", results);
      await this.enterprisePolicyCheck("first_run", results);
    } else {
      log("not first run!");
      await this.enterprisePolicyCheck("startup", results);
    }

    if (!(await stateManager.shouldRunHeuristics())) {
      return;
    }

    let networkStatus = (await browser.networkStatus.getLinkInfo()).status;
    let captiveState = "unknown";
    try {
      captiveState = await browser.captivePortal.getState();
    } catch (e) {
      // Captive Portal Service is disabled.
    }

    if (networkStatus == "up" && captiveState != "locked_portal") {
      await rollout.heuristics("startup");
    }

    // Listen for network change events to run heuristics again
    browser.networkStatus.onConnectionChanged.addListener(
      rollout.onConnectionChanged
    );

    // Listen to the captive portal when it unlocks
    try {
      browser.captivePortal.onStateChange.addListener(
        rollout.onCaptiveStateChanged
      );
    } catch (e) {
      // Captive Portal Service is disabled.
    }
  },

  async onConnectionChanged({ status }) {
    log("onConnectionChanged", status);

    if (status != "up") {
      return;
    }

    let captiveState = "unknown";
    try {
      captiveState = await browser.captivePortal.getState();
    } catch (e) {
      // Captive Portal Service is disabled.
    }

    if (captiveState == "locked_portal") {
      return;
    }

    // The network is up and we don't know that we're in a locked portal.
    // Run heuristics. If we detect a portal later, we'll run heuristics again
    // when it's unlocked. In that case, this run will likely have failed.
    await rollout.heuristics("netchange");
  },

  async onCaptiveStateChanged({ state }) {
    log("onCaptiveStateChanged", state);
    // unlocked_portal means we were previously in a locked portal and then
    // network access was granted.
    if (state == "unlocked_portal") {
      await rollout.heuristics("netchange");
    }
  },
};

const setup = {
  async start() {
    const isAddonDisabled = await rollout.getSetting(DOH_DISABLED_PREF, false);
    const runAddonPref = await rollout.getSetting(DOH_ENABLED_PREF, false);
    const runAddonBypassPref = await rollout.getSetting(
      DOH_SELF_ENABLED_PREF,
      false
    );
    const runAddonDoorhangerDecision = await rollout.getSetting(
      DOH_DOORHANGER_USER_DECISION_PREF,
      ""
    );
    const runAddonPreviousTRRMode = await rollout.getSetting(
      DOH_PREVIOUS_TRR_MODE_PREF,
      -1
    );

    if (isAddonDisabled) {
      // Regardless of pref, the user has chosen/heuristics dictated that this add-on should be disabled.
      // DoH status will not be modified from whatever the current setting is at runtime
      log(
        "Addon has been disabled. DoH status will not be modified from current setting"
      );
      await stateManager.rememberDisableHeuristics();
      return;
    }

    if (
      runAddonPref ||
      runAddonBypassPref ||
      runAddonDoorhangerDecision === "UIOk" ||
      runAddonDoorhangerDecision === "enabled" ||
      runAddonPreviousTRRMode === 2 ||
      runAddonPreviousTRRMode === 0
    ) {
      rollout.init();
    } else {
      log("Disabled, aborting!");
    }
  },
};

(async () => {
  DEBUG = await browser.experiments.preferences.getBoolPref(
    DOH_DEBUG_PREF,
    false
  );

  // Run Migration First, to continue to run rest of start up logic
  await rollout.migrateLocalStoragePrefs();

  log("Watching `doh-rollout.enabled` pref");
  browser.experiments.preferences.onPrefChanged.addListener(async () => {
    let enabled = await rollout.getSetting(DOH_ENABLED_PREF, false);
    if (enabled) {
      setup.start();
    } else {
      // Reset the TRR mode if we were running normally with no user-interference.
      if (await stateManager.shouldRunHeuristics()) {
        await stateManager.setState("disabled");
      }

      // Remove our listeners.
      browser.networkStatus.onConnectionChanged.removeListener(
        rollout.onConnectionChanged
      );

      try {
        browser.captivePortal.onStateChange.removeListener(
          rollout.onCaptiveStateChanged
        );
      } catch (e) {
        // Captive Portal Service is disabled.
      }
    }
  });

  if (await rollout.getSetting(DOH_ENABLED_PREF, false)) {
    await setup.start();
  } else if (
    (await rollout.getSetting(DOH_DONE_FIRST_RUN_PREF, false)) &&
    (await stateManager.shouldRunHeuristics())
  ) {
    // We previously had turned on DoH, and now after a restart we've been
    // rolled back. Reset TRR mode.
    await stateManager.setState("disabled");
  }
})();
