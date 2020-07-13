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

ChromeUtils.defineModuleGetter(
  this,
  "AsyncShutdown",
  "resource://gre/modules/AsyncShutdown.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "ExtensionStorageIDB",
  "resource://gre/modules/ExtensionStorageIDB.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "Heuristics",
  "resource:///modules/DoHHeuristics.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "Preferences",
  "resource://gre/modules/Preferences.jsm"
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

// Enables this controller. Turned on via Normandy rollout.
const ENABLED_PREF = "doh-rollout.enabled";

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

const TRR_SELECT_ENABLED_PREF = "doh-rollout.trr-selection.enabled";
const TRR_SELECT_DRY_RUN_RESULT_PREF =
  "doh-rollout.trr-selection.dry-run-result";
const TRR_SELECT_COMMIT_RESULT_PREF = "doh-rollout.trr-selection.commit-result";

const HEURISTICS_TELEMETRY_CATEGORY = "doh";
const TRRSELECT_TELEMETRY_CATEGORY = "security.doh.trrPerformance";

const kLinkStatusChangedTopic = "network:link-status-changed";
const kConnectivityTopic = "network:captive-portal-connectivity";
const kPrefChangedTopic = "nsPref:changed";

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

    Preferences.observe(ENABLED_PREF, this);
    Preferences.observe(NETWORK_TRR_MODE_PREF, this);
    Preferences.observe(NETWORK_TRR_URI_PREF, this);

    if (Preferences.get(ENABLED_PREF, false)) {
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

  // Used by tests to reset DoHController state (prefs are not cleared here -
  // tests do that when needed between _uninit and init).
  async _uninit() {
    Preferences.ignore(ENABLED_PREF, this);
    Preferences.ignore(NETWORK_TRR_MODE_PREF, this);
    Preferences.ignore(NETWORK_TRR_URI_PREF, this);
    AsyncShutdown.profileBeforeChange.removeBlocker(this._asyncShutdownBlocker);
    await this.disableHeuristics();
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

  async runHeuristics(evaluateReason) {
    let results = await Heuristics.run();

    let decision = Object.values(results).includes(Heuristics.DISABLE_DOH)
      ? Heuristics.DISABLE_DOH
      : Heuristics.ENABLE_DOH;

    results.evaluateReason = evaluateReason;

    if (results.steeredProvider) {
      gDNSService.setDetectedTrrURI(results.steeredProvider.uri);
      results.steeredProvider = results.steeredProvider.name;
    }

    if (decision === Heuristics.DISABLE_DOH) {
      await this.setState("disabled");
    } else {
      await this.setState("enabled");
    }

    Services.telemetry.recordEvent(
      HEURISTICS_TELEMETRY_CATEGORY,
      "evaluate",
      "heuristics",
      decision,
      results
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
    if (!Preferences.get(TRR_SELECT_COMMIT_RESULT_PREF, false)) {
      Preferences.reset(ROLLOUT_URI_PREF);
    }

    if (!Preferences.get(TRR_SELECT_ENABLED_PREF, false)) {
      return;
    }

    if (Preferences.isSet(ROLLOUT_URI_PREF)) {
      return;
    }

    await this.runTRRSelectionDryRun();

    // If persisting the selection is disabled, don't commit the value.
    if (!Preferences.get(TRR_SELECT_COMMIT_RESULT_PREF, false)) {
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
    }
  },

  async onPrefChanged(pref) {
    switch (pref) {
      case ENABLED_PREF:
        if (Preferences.get(ENABLED_PREF, false)) {
          await this.maybeEnableHeuristics();
        } else {
          await this.rollback();
        }
        break;
      case NETWORK_TRR_URI_PREF:
      case NETWORK_TRR_MODE_PREF:
        await this.setState("manuallyDisabled");
        Preferences.set(DISABLED_PREF, true);
        await this.disableHeuristics();
        break;
    }
  },

  async onConnectionChanged() {
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
    await this.runHeuristics("connectivity");
  },
};
