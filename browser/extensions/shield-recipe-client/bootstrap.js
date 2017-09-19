/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PromiseUtils",
  "resource://gre/modules/PromiseUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LogManager",
  "resource://shield-recipe-client/lib/LogManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ShieldRecipeClient",
  "resource://shield-recipe-client/lib/ShieldRecipeClient.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PreferenceExperiments",
  "resource://shield-recipe-client/lib/PreferenceExperiments.jsm");

// Act as both a normal bootstrap.js and a JS module so that we can test
// startup methods without having to install/uninstall the add-on.
this.EXPORTED_SYMBOLS = ["Bootstrap"];

const REASON_APP_STARTUP = 1;
const UI_AVAILABLE_NOTIFICATION = "sessionstore-windows-restored";
const STARTUP_EXPERIMENT_MIGRATED_PREF = "extensions.shield-recipe-client.startupExperimentMigrated";
const STARTUP_EXPERIMENT_PREFS_BRANCH = "extensions.shield-recipe-client.startupExperimentPrefs.";
const PREF_LOGGING_LEVEL = "extensions.shield-recipe-client.logging.level";
const BOOTSTRAP_LOGGER_NAME = "extensions.shield-recipe-client.bootstrap";
const DEFAULT_PREFS = {
  "extensions.shield-recipe-client.api_url": "https://normandy.cdn.mozilla.net/api/v1",
  "extensions.shield-recipe-client.dev_mode": false,
  "extensions.shield-recipe-client.enabled": true,
  "extensions.shield-recipe-client.startup_delay_seconds": 300,
  "extensions.shield-recipe-client.logging.level": Log.Level.Warn,
  "extensions.shield-recipe-client.user_id": "",
  "extensions.shield-recipe-client.run_interval_seconds": 86400, // 24 hours
  "extensions.shield-recipe-client.first_run": true,
  "extensions.shield-recipe-client.shieldLearnMoreUrl": (
    "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/shield"
  ),
  "app.shield.optoutstudies.enabled": AppConstants.MOZ_DATA_REPORTING,
};

// Logging
const log = Log.repository.getLogger(BOOTSTRAP_LOGGER_NAME);
log.addAppender(new Log.ConsoleAppender(new Log.BasicFormatter()));
log.level = Services.prefs.getIntPref(PREF_LOGGING_LEVEL, Log.Level.Warn);

this.Bootstrap = {
  _readyToStartup: null,
  _startupFinished: null,

  initShieldPrefs(defaultPrefs) {
    const prefBranch = Services.prefs.getDefaultBranch("");
    for (const [name, value] of Object.entries(defaultPrefs)) {
      switch (typeof value) {
        case "string":
          prefBranch.setCharPref(name, value);
          break;
        case "number":
          prefBranch.setIntPref(name, value);
          break;
        case "boolean":
          prefBranch.setBoolPref(name, value);
          break;
        default:
          throw new Error(`Invalid default preference type ${typeof value}`);
      }
    }
  },

  async initExperimentPrefs() {
    const defaultBranch = Services.prefs.getDefaultBranch("");
    const experimentBranch = Services.prefs.getBranch(STARTUP_EXPERIMENT_PREFS_BRANCH);

    // If the user has upgraded from a Shield version that doesn't save experiment prefs on
    // shutdown, we need to manually import them. This incurs a one-time startup i/o hit, but we
    // already had this startup i/o hit anyway in the affected versions. (bug 1399936)
    if (!Services.prefs.getBoolPref(STARTUP_EXPERIMENT_MIGRATED_PREF, false)) {
      await PreferenceExperiments.saveStartupPrefs();
      Services.prefs.setBoolPref(STARTUP_EXPERIMENT_MIGRATED_PREF, true);
    }

    for (const prefName of experimentBranch.getChildList("")) {
      const experimentPrefType = experimentBranch.getPrefType(prefName);
      const realPrefType = defaultBranch.getPrefType(prefName);

      if (realPrefType !== Services.prefs.PREF_INVALID && realPrefType !== experimentPrefType) {
        log.error(`Error setting startup pref ${prefName}; pref type does not match.`);
        continue;
      }

      switch (experimentPrefType) {
        case Services.prefs.PREF_STRING:
          defaultBranch.setCharPref(prefName, experimentBranch.getCharPref(prefName));
          break;

        case Services.prefs.PREF_INT:
          defaultBranch.setIntPref(prefName, experimentBranch.getIntPref(prefName));
          break;

        case Services.prefs.PREF_BOOL:
          defaultBranch.setBoolPref(prefName, experimentBranch.getBoolPref(prefName));
          break;

        case Services.prefs.PREF_INVALID:
          // This should never happen.
          log.error(`Error setting startup pref ${prefName}; pref type is invalid (${experimentPrefType}).`);
          break;

        default:
          // This should never happen either.
          log.error(`Error getting startup pref ${prefName}; unknown value type ${experimentPrefType}.`);
      }
    }
  },

  observe(subject, topic, data) {
    if (topic === UI_AVAILABLE_NOTIFICATION) {
      Services.obs.removeObserver(this, UI_AVAILABLE_NOTIFICATION);
      this._readyToStartup.resolve();
    }
  },

  install() {
    // Nothing to do during install
  },

  async startup(data, reason) {
    this._readyToStartup = PromiseUtils.defer();
    this._startupFinished = PromiseUtils.defer();

    // _readyToStartup should only be resolved after first paint has
    // already occurred.
    if (reason === REASON_APP_STARTUP) {
      Services.obs.addObserver(this, UI_AVAILABLE_NOTIFICATION);
    } else {
      this._readyToStartup.resolve();
    }

    // Initialization that needs to happen before the first paint on startup.
    try {
      this.initShieldPrefs(DEFAULT_PREFS);
      await this.initExperimentPrefs();

      await this._readyToStartup.promise;
      await ShieldRecipeClient.startup();
    } finally {
      this._startupFinished.resolve();
    }

  },

  async shutdown(data, reason) {
    // If startup is still in progress, wait for it to finish
    await this._startupFinished.promise;

    // Wait for async write operations during shutdown before unloading modules.
    await ShieldRecipeClient.shutdown(reason);

    // In case the observer didn't run, clean it up.
    try {
      Services.obs.removeObserver(this, UI_AVAILABLE_NOTIFICATION);
    } catch (err) {
      // It must already be removed!
    }

    // Unload add-on modules. We don't do this in ShieldRecipeClient so that
    // modules are not unloaded accidentally during tests.
    let modules = [
      "lib/ActionSandboxManager.jsm",
      "lib/Addons.jsm",
      "lib/AddonStudies.jsm",
      "lib/CleanupManager.jsm",
      "lib/ClientEnvironment.jsm",
      "lib/FilterExpressions.jsm",
      "lib/EventEmitter.jsm",
      "lib/Heartbeat.jsm",
      "lib/LogManager.jsm",
      "lib/NormandyApi.jsm",
      "lib/NormandyDriver.jsm",
      "lib/PreferenceExperiments.jsm",
      "lib/RecipeRunner.jsm",
      "lib/Sampling.jsm",
      "lib/SandboxManager.jsm",
      "lib/ShieldPreferences.jsm",
      "lib/ShieldRecipeClient.jsm",
      "lib/Storage.jsm",
      "lib/Uptake.jsm",
      "lib/Utils.jsm",
    ].map(m => `resource://shield-recipe-client/${m}`);
    modules = modules.concat([
      "resource://shield-recipe-client-content/AboutPages.jsm",
      "resource://shield-recipe-client-vendor/mozjexl.js",
    ]);

    for (const module of modules) {
      log.debug(`Unloading ${module}`);
      Cu.unload(module);
    }
  },

  uninstall() {
    // Do nothing
  },
};

// Expose bootstrap methods on the global
for (const methodName of ["install", "startup", "shutdown", "uninstall"]) {
  this[methodName] = Bootstrap[methodName].bind(Bootstrap);
}
