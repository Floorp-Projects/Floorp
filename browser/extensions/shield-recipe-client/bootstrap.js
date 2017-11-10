/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {results: Cr, utils: Cu} = Components;
Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

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

let studyPrefsChanged = {};

this.Bootstrap = {
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

  initExperimentPrefs() {
    studyPrefsChanged = {};
    const defaultBranch = Services.prefs.getDefaultBranch("");
    const experimentBranch = Services.prefs.getBranch(STARTUP_EXPERIMENT_PREFS_BRANCH);

    for (const prefName of experimentBranch.getChildList("")) {
      const experimentPrefType = experimentBranch.getPrefType(prefName);
      const realPrefType = defaultBranch.getPrefType(prefName);

      if (realPrefType !== Services.prefs.PREF_INVALID && realPrefType !== experimentPrefType) {
        log.error(`Error setting startup pref ${prefName}; pref type does not match.`);
        continue;
      }

      // record the value of the default branch before setting it
      try {
        switch (realPrefType) {
          case Services.prefs.PREF_STRING:
            studyPrefsChanged[prefName] = defaultBranch.getCharPref(prefName);
            break;

          case Services.prefs.PREF_INT:
            studyPrefsChanged[prefName] = defaultBranch.getIntPref(prefName);
            break;

          case Services.prefs.PREF_BOOL:
            studyPrefsChanged[prefName] = defaultBranch.getBoolPref(prefName);
            break;

          case Services.prefs.PREF_INVALID:
            studyPrefsChanged[prefName] = null;
            break;

          default:
            // This should never happen
            log.error(`Error getting startup pref ${prefName}; unknown value type ${experimentPrefType}.`);
        }
      } catch (e) {
        if (e.result == Cr.NS_ERROR_UNEXPECTED) {
          // There is a value for the pref on the user branch but not on the default branch. This is ok.
          studyPrefsChanged[prefName] = null;
        } else {
          // rethrow
          throw e;
        }
      }

      // now set the new default value
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
      this.finishStartup();
    }
  },

  install() {
    // Nothing to do during install
  },

  startup(data, reason) {
    // Initialization that needs to happen before the first paint on startup.
    this.initShieldPrefs(DEFAULT_PREFS);
    this.initExperimentPrefs();

    // If the app is starting up, wait until the UI is available before finishing
    // init.
    if (reason === REASON_APP_STARTUP) {
      Services.obs.addObserver(this, UI_AVAILABLE_NOTIFICATION);
    } else {
      this.finishStartup();
    }
  },

  async finishStartup() {
    await PreferenceExperiments.recordOriginalValues(studyPrefsChanged);
    ShieldRecipeClient.startup();
  },

  async shutdown(data, reason) {
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
