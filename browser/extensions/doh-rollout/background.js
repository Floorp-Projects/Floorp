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

// Platform TRR mode pref. Can be used to turn DoH off, on with fallback, or
// on without fallback. Exposed in about:preferences. We turn off our heuristics
// if we *ever* see a user-set value for this pref.
const NETWORK_TRR_MODE_PREF = "network.trr.mode";

// Platform TRR uri pref used to set a custom DoH endpoint. Exposed in about:preferences.
// We turn off heuristics if we *ever* see a user-set value for this pref.
const NETWORK_TRR_URI_PREF = "network.trr.uri";

// Pref that signals to turn DoH to on/off. It mirrors two possible values of
// network.trr.mode:
// 0: Off (default)
// 2: Enabled, but will fall back to 0 on DNS lookup failure
const ROLLOUT_TRR_MODE_PREF = "doh-rollout.mode";

// This preference is set to TRUE when DoH has been enabled via the add-on. It will
// allow the add-on to continue to function without the aid of the Normandy-triggered pref
// of "doh-rollout.enabled". Note that instead of setting it to false, it is cleared.
const DOH_SELF_ENABLED_PREF = "doh-rollout.self-enabled";

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

// This pref used to be part of a cache mechanism to see if the heuristics
// dictated a change in the DoH settings. We now clear it in the trr mode
// migration at startup.
const DOH_PREVIOUS_TRR_MODE_PREF = "doh-rollout.previous.trr.mode";

// If set to true, debug logging will be enabled.
const DOH_DEBUG_PREF = "doh-rollout.debug";

const stateManager = {
  async setState(state) {
    log("setState: ", state);

    switch (state) {
      case "uninstalled":
        break;
      case "disabled":
        await rollout.setSetting(ROLLOUT_TRR_MODE_PREF, 0);
        break;
      case "UIOk":
        await rollout.setSetting(DOH_SELF_ENABLED_PREF, true);
        break;
      case "enabled":
        await rollout.setSetting(ROLLOUT_TRR_MODE_PREF, 2);
        await rollout.setSetting(DOH_SELF_ENABLED_PREF, true);
        break;
      case "manuallyDisabled":
      case "UIDisabled":
        await browser.experiments.preferences.clearUserPref(
          DOH_SELF_ENABLED_PREF
        );
      // Fall through.
      case "rollback":
        await browser.experiments.preferences.clearUserPref(
          ROLLOUT_TRR_MODE_PREF
        );
        break;
    }

    await browser.experiments.heuristics.sendStatePing(state);
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

    return true;
  },
};

const rollout = {
  // Pretend that there was a network change at the beginning of time.
  lastNetworkChangeTime: 0,

  async heuristics(evaluateReason) {
    let shouldRunHeuristics = await stateManager.shouldRunHeuristics();

    if (!shouldRunHeuristics) {
      return;
    }

    // Run heuristics defined in heuristics.js and experiments/heuristics/api.js
    let results = await runHeuristics();

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

  async trrPrefUserModifiedCheck() {
    let modeHasUserValue = await browser.experiments.preferences.prefHasUserValue(
      NETWORK_TRR_MODE_PREF
    );
    let uriHasUserValue = await browser.experiments.preferences.prefHasUserValue(
      NETWORK_TRR_URI_PREF
    );
    if (modeHasUserValue || uriHasUserValue) {
      await stateManager.setState("manuallyDisabled");
      await stateManager.rememberDisableHeuristics();
    }
  },

  async enterprisePolicyCheck() {
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

  // Previous versions of the add-on worked by setting network.trr.mode directly
  // to turn DoH on/off. This makes sure we clear that value and also the pref
  // we formerly used to track changes to it.
  async migrateOldTrrMode() {
    const needsMigration = await browser.experiments.preferences.getIntPref(
      DOH_PREVIOUS_TRR_MODE_PREF,
      -1
    );

    if (needsMigration === -1) {
      log("User's TRR mode prefs already migrated");
      return;
    }

    await browser.experiments.preferences.clearUserPref(NETWORK_TRR_MODE_PREF);
    await browser.experiments.preferences.clearUserPref(
      DOH_PREVIOUS_TRR_MODE_PREF
    );

    log("TRR mode prefs migrated");
  },

  async init() {
    log("calling init");

    await this.setSetting(DOH_DONE_FIRST_RUN_PREF, true);

    // Register the events for sending pings
    browser.experiments.heuristics.setupTelemetry();

    await this.enterprisePolicyCheck();
    await this.trrPrefUserModifiedCheck();

    if (!(await stateManager.shouldRunHeuristics())) {
      return;
    }

    // Perform TRR selection before running heuristics.
    await browser.experiments.trrselect.run();
    log("TRR selection complete!");

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
      browser.captivePortal.onConnectivityAvailable.addListener(
        rollout.onConnectivityAvailable
      );
    } catch (e) {
      // Captive Portal Service is disabled.
    }

    browser.experiments.preferences.onTRRPrefChanged.addListener(
      async function listener() {
        await stateManager.setState("manuallyDisabled");
        await stateManager.rememberDisableHeuristics();
        await setup.stop();
        browser.experiments.preferences.onTRRPrefChanged.removeListener(
          listener
        );
      }
    );
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
      // Captive Portal Service is disabled. Run heuristics optimistically, but
      // there's a chance the network is unavailable at this point. In that case
      // we also wouldn't know when the network is back up. Worst case, we don't
      // enable DoH in this case, but that's better than never enabling it.
      await rollout.heuristics("netchange");
      return;
    }

    if (captiveState == "locked_portal") {
      return;
    }

    // The network is up and we don't know that we're in a locked portal.
    // Run heuristics. When we detect a portal or lack thereof later, we'll run
    // heuristics again. In that case, this run will likely have failed.
    await rollout.heuristics("netchange");
  },

  async onConnectivityAvailable() {
    log("onConnectivityAvailable");
    await rollout.heuristics("connectivity");
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

    if (isAddonDisabled) {
      // Regardless of pref, the user has chosen/heuristics dictated that this add-on should be disabled.
      // DoH status will not be modified from whatever the current setting is at runtime
      log(
        "Addon has been disabled. DoH status will not be modified from current setting"
      );
      await stateManager.rememberDisableHeuristics();
      return;
    }

    if (runAddonBypassPref) {
      // runAddonBypassPref being set means that this is not first-run, and we
      // were still running heuristics when we shutdown - so it's safe to
      // do the TRR mode migration and clear network.trr.mode.
      rollout.migrateOldTrrMode();
    }

    if (
      runAddonPref ||
      runAddonBypassPref ||
      runAddonDoorhangerDecision === "UIOk" ||
      runAddonDoorhangerDecision === "enabled"
    ) {
      rollout.init();
    } else {
      log("Disabled, aborting!");
    }
  },

  async stop() {
    // Remove our listeners.
    browser.networkStatus.onConnectionChanged.removeListener(
      rollout.onConnectionChanged
    );

    try {
      browser.captivePortal.onConnectivityAvailable.removeListener(
        rollout.onConnectivityAvailable
      );
    } catch (e) {
      // Captive Portal Service is disabled.
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
  await browser.experiments.preferences.migrateNextDNSEndpoint();

  log("Watching `doh-rollout.enabled` pref");
  browser.experiments.preferences.onEnabledChanged.addListener(async () => {
    let enabled = await rollout.getSetting(DOH_ENABLED_PREF, false);
    if (enabled) {
      setup.start();
    } else {
      // Reset the TRR mode if we were running normally with no user-interference.
      if (await stateManager.shouldRunHeuristics()) {
        await stateManager.setState("rollback");
      }
      setup.stop();
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
    await stateManager.setState("rollback");
  }
})();
