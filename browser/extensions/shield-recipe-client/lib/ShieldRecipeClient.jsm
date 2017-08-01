/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {utils: Cu, interfaces: Ci} = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "LogManager",
  "resource://shield-recipe-client/lib/LogManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "RecipeRunner",
  "resource://shield-recipe-client/lib/RecipeRunner.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CleanupManager",
  "resource://shield-recipe-client/lib/CleanupManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PreferenceExperiments",
  "resource://shield-recipe-client/lib/PreferenceExperiments.jsm");

this.EXPORTED_SYMBOLS = ["ShieldRecipeClient"];

const {PREF_STRING, PREF_BOOL, PREF_INT} = Ci.nsIPrefBranch;

const REASONS = {
  APP_STARTUP: 1,      // The application is starting up.
  APP_SHUTDOWN: 2,     // The application is shutting down.
  ADDON_ENABLE: 3,     // The add-on is being enabled.
  ADDON_DISABLE: 4,    // The add-on is being disabled. (Also sent during uninstallation)
  ADDON_INSTALL: 5,    // The add-on is being installed.
  ADDON_UNINSTALL: 6,  // The add-on is being uninstalled.
  ADDON_UPGRADE: 7,    // The add-on is being upgraded.
  ADDON_DOWNGRADE: 8,  // The add-on is being downgraded.
};
const PREF_BRANCH = "extensions.shield-recipe-client.";
const DEFAULT_PREFS = {
  api_url: ["https://normandy.cdn.mozilla.net/api/v1", PREF_STRING],
  dev_mode: [false, PREF_BOOL],
  enabled: [true, PREF_BOOL],
  startup_delay_seconds: [300, PREF_INT],
  "logging.level": [Log.Level.Warn, PREF_INT],
  user_id: ["", PREF_STRING],
  run_interval_seconds: [86400, PREF_INT], // 24 hours
  first_run: [true, PREF_BOOL],
};
const PREF_DEV_MODE = "extensions.shield-recipe-client.dev_mode";
const PREF_LOGGING_LEVEL = PREF_BRANCH + "logging.level";

let log = null;

/**
 * Handles startup and shutdown of the entire add-on. Bootsrap.js defers to this
 * module for most tasks so that we can more easily test startup and shutdown
 * (bootstrap.js is difficult to import in tests).
 */
this.ShieldRecipeClient = {
  async startup() {
    ShieldRecipeClient.setDefaultPrefs();

    // Setup logging and listen for changes to logging prefs
    LogManager.configure(Services.prefs.getIntPref(PREF_LOGGING_LEVEL));
    Services.prefs.addObserver(PREF_LOGGING_LEVEL, LogManager.configure);
    CleanupManager.addCleanupHandler(
      () => Services.prefs.removeObserver(PREF_LOGGING_LEVEL, LogManager.configure),
    );
    log = LogManager.getLogger("bootstrap");

    // Initialize experiments first to avoid a race between initializing prefs
    // and recipes rolling back pref changes when experiments end.
    try {
      await PreferenceExperiments.init();
    } catch (err) {
      log.error("Failed to initialize preference experiments:", err);
    }

    await RecipeRunner.init();
  },

  shutdown(reason) {
    CleanupManager.cleanup();
  },

  setDefaultPrefs() {
    for (const [key, [val, type]] of Object.entries(DEFAULT_PREFS)) {
      const fullKey = PREF_BRANCH + key;
      // If someone beat us to setting a default, don't overwrite it.
      if (!Services.prefs.prefHasUserValue(fullKey)) {
        switch (type) {
          case PREF_BOOL:
            Services.prefs.setBoolPref(fullKey, val);
            break;

          case PREF_INT:
            Services.prefs.setIntPref(fullKey, val);
            break;

          case PREF_STRING:
            Services.prefs.setStringPref(fullKey, val);
            break;

          default:
            throw new TypeError(`Unexpected type (${type}) for preference ${fullKey}.`)
        }
      }
    }
  },
};
