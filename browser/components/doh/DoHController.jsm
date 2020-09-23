/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * This module runs the automated heuristics to enable/disable DoH on different
 * networks. Heuristics are run at startup and upon network changes.
 * Heuristics are disabled if the user sets their DoH provider or mode manually.
 */
var EXPORTED_SYMBOLS = ["DoHController"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.jsm",
  ClientID: "resource://gre/modules/ClientID.jsm",
  ExtensionStorageIDB: "resource://gre/modules/ExtensionStorageIDB.jsm",
  Config: "resource:///modules/DoHConfig.jsm",
  Heuristics: "resource:///modules/DoHHeuristics.jsm",
  Preferences: "resource://gre/modules/Preferences.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
  clearTimeout: "resource://gre/modules/Timer.jsm",
});

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "kDebounceTimeout",
  "doh-rollout.network-debounce-timeout",
  1000
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gCaptivePortalService",
  "@mozilla.org/network/captive-portal-service;1",
  "nsICaptivePortalService"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gDNSService",
  "@mozilla.org/network/dns-service;1",
  "nsIDNSService"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gNetworkLinkService",
  "@mozilla.org/network/network-link-service;1",
  "nsINetworkLinkService"
);

// Stores whether we've done first-run.
const FIRST_RUN_PREF = "doh-rollout.doneFirstRun";

// Records if the user opted in/out of DoH study by clicking on doorhanger
const DOORHANGER_USER_DECISION_PREF = "doh-rollout.doorhanger-decision";

// Set when we detect that the user set their DoH provider or mode manually.
// If set, we don't run heuristics.
const DISABLED_PREF = "doh-rollout.disable-heuristics";

// Set when we detect either a non-DoH enterprise policy, or a DoH policy that
// tells us to disable it. This pref's effect is to suppress the opt-out CFR.
const SKIP_HEURISTICS_PREF = "doh-rollout.skipHeuristicsCheck";

const BREADCRUMB_PREF = "doh-rollout.self-enabled";

// Necko TRR prefs to watch for user-set values.
const NETWORK_TRR_MODE_PREF = "network.trr.mode";
const NETWORK_TRR_URI_PREF = "network.trr.uri";

const TRR_LIST_PREF = "network.trr.resolvers";

const ROLLOUT_MODE_PREF = "doh-rollout.mode";
const ROLLOUT_URI_PREF = "doh-rollout.uri";

const TRR_SELECT_DRY_RUN_RESULT_PREF =
  "doh-rollout.trr-selection.dry-run-result";

const HEURISTICS_TELEMETRY_CATEGORY = "doh";
const TRRSELECT_TELEMETRY_CATEGORY = "security.doh.trrPerformance";

const kLinkStatusChangedTopic = "network:link-status-changed";
const kConnectivityTopic = "network:captive-portal-connectivity";
const kPrefChangedTopic = "nsPref:changed";

// Helper function to hash the network ID concatenated with telemetry client ID.
// This prevents us from being able to tell if 2 clients are on the same network.
function getHashedNetworkID() {
  let currentNetworkID = gNetworkLinkService.networkID;
  if (!currentNetworkID) {
    return "";
  }

  let hasher = Cc["@mozilla.org/security/hash;1"].createInstance(
    Ci.nsICryptoHash
  );

  hasher.init(Ci.nsICryptoHash.SHA256);
  // Concat the client ID with the network ID before hashing.
  let clientNetworkID = ClientID.getClientID() + currentNetworkID;
  hasher.update(
    clientNetworkID.split("").map(c => c.charCodeAt(0)),
    clientNetworkID.length
  );
  return hasher.finish(true);
}

const DoHController = {
  _heuristicsAreEnabled: false,

  async init() {
    await this.migrateLocalStoragePrefs();
    await this.migrateOldTrrMode();
    await this.migrateNextDNSEndpoint();

    Services.telemetry.setEventRecordingEnabled(
      HEURISTICS_TELEMETRY_CATEGORY,
      true
    );
    Services.telemetry.setEventRecordingEnabled(
      TRRSELECT_TELEMETRY_CATEGORY,
      true
    );

    Services.obs.addObserver(this, Config.kConfigUpdateTopic);
    Preferences.observe(NETWORK_TRR_MODE_PREF, this);
    Preferences.observe(NETWORK_TRR_URI_PREF, this);

    if (Config.enabled) {
      await this.maybeEnableHeuristics();
    } else if (Preferences.get(FIRST_RUN_PREF, false)) {
      await this.rollback();
    }

    this._asyncShutdownBlocker = async () => {
      await this.disableHeuristics();
    };

    AsyncShutdown.profileBeforeChange.addBlocker(
      "DoHController: clear state and remove observers",
      this._asyncShutdownBlocker
    );

    Preferences.set(FIRST_RUN_PREF, true);
  },

  // Also used by tests to reset DoHController state (prefs are not cleared
  // here - tests do that when needed between _uninit and init).
  async _uninit() {
    Services.obs.removeObserver(this, Config.kConfigUpdateTopic);
    Preferences.ignore(NETWORK_TRR_MODE_PREF, this);
    Preferences.ignore(NETWORK_TRR_URI_PREF, this);
    AsyncShutdown.profileBeforeChange.removeBlocker(this._asyncShutdownBlocker);
    await this.disableHeuristics();
  },

  // Called to reset state when a new config is available.
  async reset() {
    await this._uninit();
    await this.init();
  },

  async migrateLocalStoragePrefs() {
    const BALROG_MIGRATION_COMPLETED_PREF = "doh-rollout.balrog-migration-done";
    const ADDON_ID = "doh-rollout@mozilla.org";

    // Migrate updated local storage item names. If this has already been done once, skip the migration
    const isMigrated = Preferences.get(BALROG_MIGRATION_COMPLETED_PREF, false);

    if (isMigrated) {
      return;
    }

    let policy = WebExtensionPolicy.getByID(ADDON_ID);
    if (!policy) {
      return;
    }

    const storagePrincipal = ExtensionStorageIDB.getStoragePrincipal(
      policy.extension
    );
    const idbConn = await ExtensionStorageIDB.open(storagePrincipal);

    // Previously, the DoH heuristics were bundled as an add-on. Early versions
    // of this add-on used local storage instead of prefs to persist state. This
    // function migrates the values that are still relevant to their new pref
    // counterparts.
    const legacyLocalStorageKeys = [
      "doneFirstRun",
      DOORHANGER_USER_DECISION_PREF,
      DISABLED_PREF,
    ];

    for (let item of legacyLocalStorageKeys) {
      let data = await idbConn.get(item);
      let value = data[item];

      if (data.hasOwnProperty(item)) {
        let migratedName = item;

        if (!item.startsWith("doh-rollout.")) {
          migratedName = "doh-rollout." + item;
        }

        Preferences.set(migratedName, value);
      }
    }

    await idbConn.clear();
    await idbConn.close();

    // Set pref to skip this function in the future.
    Preferences.set(BALROG_MIGRATION_COMPLETED_PREF, true);
  },

  // Previous versions of the DoH frontend worked by setting network.trr.mode
  // directly to turn DoH on/off. This makes sure we clear that value and also
  // the pref we formerly used to track changes to it.
  async migrateOldTrrMode() {
    const PREVIOUS_TRR_MODE_PREF = "doh-rollout.previous.trr.mode";

    if (Preferences.get(PREVIOUS_TRR_MODE_PREF) === undefined) {
      return;
    }

    Preferences.reset(NETWORK_TRR_MODE_PREF);
    Preferences.reset(PREVIOUS_TRR_MODE_PREF);
  },

  async migrateNextDNSEndpoint() {
    // NextDNS endpoint changed from trr.dns.nextdns.io to firefox.dns.nextdns.io
    // This migration updates any pref values that might be using the old value
    // to the new one. We support values that match the exact URL that shipped
    // in the network.trr.resolvers pref in prior versions of the browser.
    // The migration is a direct static replacement of the string.
    const oldURL = "https://trr.dns.nextdns.io/";
    const newURL = "https://firefox.dns.nextdns.io/";
    const prefsToMigrate = [
      "network.trr.resolvers",
      "network.trr.uri",
      "network.trr.custom_uri",
      "doh-rollout.trr-selection.dry-run-result",
      "doh-rollout.uri",
    ];

    for (let pref of prefsToMigrate) {
      if (!Preferences.isSet(pref)) {
        continue;
      }
      Preferences.set(pref, Preferences.get(pref).replaceAll(oldURL, newURL));
    }
  },

  // The "maybe" is because there are two cases when we don't enable heuristics:
  // 1. If we detect that TRR mode or URI have user values, or we previously
  //    detected this (i.e. DISABLED_PREF is true)
  // 2. If there are any non-DoH enterprise policies active
  async maybeEnableHeuristics() {
    if (Preferences.get(DISABLED_PREF)) {
      return;
    }

    let policyResult = await Heuristics.checkEnterprisePolicy();

    if (["policy_without_doh", "disable_doh"].includes(policyResult)) {
      await this.setState("policyDisabled");
      Preferences.set(SKIP_HEURISTICS_PREF, true);
      return;
    }

    Preferences.reset(SKIP_HEURISTICS_PREF);

    if (
      Preferences.isSet(NETWORK_TRR_MODE_PREF) ||
      Preferences.isSet(NETWORK_TRR_URI_PREF)
    ) {
      await this.setState("manuallyDisabled");
      Preferences.set(DISABLED_PREF, true);
      return;
    }

    await this.runTRRSelection();
    await this.runHeuristics("startup");
    Services.obs.addObserver(this, kLinkStatusChangedTopic);
    Services.obs.addObserver(this, kConnectivityTopic);

    this._heuristicsAreEnabled = true;
  },

  _lastHeuristicsRunTimestamp: 0,
  async runHeuristics(evaluateReason) {
    let start = Date.now();
    // If this function is called in quick succession, _lastHeuristicsRunTimestamp
    // might be refreshed while we are still awaiting Heuristics.run() below.
    this._lastHeuristicsRunTimestamp = start;

    let results = await Heuristics.run();

    if (
      !gNetworkLinkService.isLinkUp ||
      this._lastDebounceTimestamp > start ||
      this._lastHeuristicsRunTimestamp > start ||
      gCaptivePortalService.state == gCaptivePortalService.LOCKED_PORTAL
    ) {
      // If the network is currently down or there was a debounce triggered
      // while we were running heuristics, it means the network fluctuated
      // during this heuristics run. We simply discard the results in this case.
      // Same thing if there was another heuristics run triggered or if we have
      // detected a locked captive portal while this one was ongoing.
      return;
    }

    let decision = Object.values(results).includes(Heuristics.DISABLE_DOH)
      ? Heuristics.DISABLE_DOH
      : Heuristics.ENABLE_DOH;

    let getCaptiveStateString = () => {
      switch (gCaptivePortalService.state) {
        case gCaptivePortalService.NOT_CAPTIVE:
          return "not_captive";
        case gCaptivePortalService.UNLOCKED_PORTAL:
          return "unlocked";
        case gCaptivePortalService.LOCKED_PORTAL:
          return "locked";
        default:
          return "unknown";
      }
    };

    let resultsForTelemetry = {
      evaluateReason,
      steeredProvider: "",
      captiveState: getCaptiveStateString(),
      // NOTE: This might not yet be available after a network change. We mainly
      // care about the startup case though - we want to look at whether the
      // heuristics result is consistent for networkIDs often seen at startup.
      // TODO: Use this data to implement cached results to use early at startup.
      networkID: getHashedNetworkID(),
    };

    if (results.steeredProvider) {
      gDNSService.setDetectedTrrURI(results.steeredProvider.uri);
      resultsForTelemetry.steeredProvider = results.steeredProvider.name;
    }

    if (decision === Heuristics.DISABLE_DOH) {
      await this.setState("disabled");
    } else {
      await this.setState("enabled");
    }

    // For telemetry, we group the heuristics results into three categories.
    // Only heuristics with a DISABLE_DOH result are included.
    // Each category is finally included in the event as a comma-separated list.
    let canaries = [];
    let filtering = [];
    let enterprise = [];
    let platform = [];

    for (let [heuristicName, result] of Object.entries(results)) {
      if (result !== Heuristics.DISABLE_DOH) {
        continue;
      }

      if (["canary", "zscalerCanary"].includes(heuristicName)) {
        canaries.push(heuristicName);
      } else if (
        ["browserParent", "google", "youtube"].includes(heuristicName)
      ) {
        filtering.push(heuristicName);
      } else if (
        ["policy", "modifiedRoots", "thirdPartyRoots"].includes(heuristicName)
      ) {
        enterprise.push(heuristicName);
      } else if (["vpn", "proxy", "nrpt"].includes(heuristicName)) {
        platform.push(heuristicName);
      }
    }

    resultsForTelemetry.canaries = canaries.join(",");
    resultsForTelemetry.filtering = filtering.join(",");
    resultsForTelemetry.enterprise = enterprise.join(",");
    resultsForTelemetry.platform = platform.join(",");

    Services.telemetry.recordEvent(
      HEURISTICS_TELEMETRY_CATEGORY,
      "evaluate_v2",
      "heuristics",
      decision,
      resultsForTelemetry
    );
  },

  async setState(state) {
    switch (state) {
      case "disabled":
        Preferences.set(ROLLOUT_MODE_PREF, 0);
        break;
      case "UIOk":
        Preferences.set(BREADCRUMB_PREF, true);
        break;
      case "enabled":
        Preferences.set(ROLLOUT_MODE_PREF, 2);
        Preferences.set(BREADCRUMB_PREF, true);
        break;
      case "policyDisabled":
      case "manuallyDisabled":
      case "UIDisabled":
        Preferences.reset(BREADCRUMB_PREF);
      // Fall through.
      case "shutdown":
      case "rollback":
        Preferences.reset(ROLLOUT_MODE_PREF);
        break;
    }

    Services.telemetry.recordEvent(
      HEURISTICS_TELEMETRY_CATEGORY,
      "state",
      state,
      "null"
    );
  },

  async disableHeuristics() {
    if (!this._heuristicsAreEnabled) {
      return;
    }

    await this.setState("shutdown");
    Services.obs.removeObserver(this, kLinkStatusChangedTopic);
    Services.obs.removeObserver(this, kConnectivityTopic);
    this._heuristicsAreEnabled = false;
  },

  async rollback() {
    await this.setState("rollback");
    await this.disableHeuristics();
  },

  async runTRRSelection() {
    // If persisting the selection is disabled, clear the existing
    // selection.
    if (!Config.trrSelection.commitResult) {
      Preferences.reset(ROLLOUT_URI_PREF);
    }

    if (!Config.trrSelection.enabled) {
      return;
    }

    if (Preferences.isSet(ROLLOUT_URI_PREF)) {
      return;
    }

    await this.runTRRSelectionDryRun();

    // If persisting the selection is disabled, don't commit the value.
    if (!Config.trrSelection.commitResult) {
      return;
    }

    Preferences.set(
      ROLLOUT_URI_PREF,
      Preferences.get(TRR_SELECT_DRY_RUN_RESULT_PREF)
    );
  },

  async runTRRSelectionDryRun() {
    if (Preferences.isSet(TRR_SELECT_DRY_RUN_RESULT_PREF)) {
      // Check whether the existing dry-run-result is in the default
      // list of TRRs. If it is, all good. Else, run the dry run again.
      let dryRunResult = Preferences.get(TRR_SELECT_DRY_RUN_RESULT_PREF);
      let defaultTRRs = JSON.parse(
        Services.prefs.getDefaultBranch("").getCharPref(TRR_LIST_PREF)
      );
      let dryRunResultIsValid = defaultTRRs.some(
        trr => trr.url == dryRunResult
      );
      if (dryRunResultIsValid) {
        return;
      }
    }

    let setDryRunResultAndRecordTelemetry = trr => {
      Preferences.set(TRR_SELECT_DRY_RUN_RESULT_PREF, trr);
      Services.telemetry.recordEvent(
        TRRSELECT_TELEMETRY_CATEGORY,
        "trrselect",
        "dryrunresult",
        trr.substring(0, 40) // Telemetry payload max length
      );
    };

    if (Cu.isInAutomation) {
      // For mochitests, just record telemetry with a dummy result.
      // TRRPerformance.jsm is tested in xpcshell.
      setDryRunResultAndRecordTelemetry("https://dummytrr.com/query");
      return;
    }

    // Importing the module here saves us from having to do it at startup, and
    // ensures tests have time to set prefs before the module initializes.
    let { TRRRacer } = ChromeUtils.import(
      "resource:///modules/TRRPerformance.jsm"
    );
    await new Promise(resolve => {
      let racer = new TRRRacer(() => {
        setDryRunResultAndRecordTelemetry(racer.getFastestTRR(true));
        resolve();
      });
      racer.run();
    });
  },

  observe(subject, topic, data) {
    switch (topic) {
      case kLinkStatusChangedTopic:
        this.onConnectionChanged();
        break;
      case kConnectivityTopic:
        this.onConnectivityAvailable();
        break;
      case kPrefChangedTopic:
        this.onPrefChanged(data);
        break;
      case Config.kConfigUpdateTopic:
        this.reset();
        break;
    }
  },

  async onPrefChanged(pref) {
    switch (pref) {
      case NETWORK_TRR_URI_PREF:
      case NETWORK_TRR_MODE_PREF:
        await this.setState("manuallyDisabled");
        Preferences.set(DISABLED_PREF, true);
        await this.disableHeuristics();
        break;
    }
  },

  // Connection change events are debounced to allow the network to settle.
  // We wait for the network to be up for a period of kDebounceTimeout before
  // handling the change. The timer is canceled when the network goes down and
  // restarted the first time we learn that it went back up.
  _debounceTimer: null,
  _cancelDebounce() {
    if (!this._debounceTimer) {
      return;
    }

    clearTimeout(this._debounceTimer);
    this._debounceTimer = null;
  },

  _lastDebounceTimestamp: 0,
  onConnectionChanged() {
    if (!gNetworkLinkService.isLinkUp) {
      // Network is down - reset debounce timer.
      this._cancelDebounce();
      return;
    }

    if (this._debounceTimer) {
      // Already debouncing - nothing to do.
      return;
    }

    this._lastDebounceTimestamp = Date.now();
    this._debounceTimer = setTimeout(() => {
      this._cancelDebounce();
      this.onConnectionChangedDebounced();
    }, kDebounceTimeout);
  },

  async onConnectionChangedDebounced() {
    if (!gNetworkLinkService.isLinkUp) {
      return;
    }

    if (gCaptivePortalService.state == gCaptivePortalService.LOCKED_PORTAL) {
      return;
    }

    // The network is up and we don't know that we're in a locked portal.
    // Run heuristics. If we detect a portal later, we'll run heuristics again
    // when it's unlocked. In that case, this run will likely have failed.
    await this.runHeuristics("netchange");
  },

  async onConnectivityAvailable() {
    if (this._debounceTimer) {
      // Already debouncing - nothing to do.
      return;
    }

    await this.runHeuristics("connectivity");
  },
};
