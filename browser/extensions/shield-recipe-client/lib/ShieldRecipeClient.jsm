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
XPCOMUtils.defineLazyModuleGetter(this, "AboutPages",
  "resource://shield-recipe-client-content/AboutPages.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ShieldPreferences",
  "resource://shield-recipe-client/lib/ShieldPreferences.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AddonStudies",
  "resource://shield-recipe-client/lib/AddonStudies.jsm");

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
const PREF_DEV_MODE = "extensions.shield-recipe-client.dev_mode";
const PREF_LOGGING_LEVEL = "extensions.shield-recipe-client.logging.level";

let log = null;

/**
 * Handles startup and shutdown of the entire add-on. Bootsrap.js defers to this
 * module for most tasks so that we can more easily test startup and shutdown
 * (bootstrap.js is difficult to import in tests).
 */
this.ShieldRecipeClient = {
  async startup() {
    // Setup logging and listen for changes to logging prefs
    LogManager.configure(Services.prefs.getIntPref(PREF_LOGGING_LEVEL));
    Services.prefs.addObserver(PREF_LOGGING_LEVEL, LogManager.configure);
    CleanupManager.addCleanupHandler(
      () => Services.prefs.removeObserver(PREF_LOGGING_LEVEL, LogManager.configure),
    );
    log = LogManager.getLogger("bootstrap");

    try {
      await AboutPages.init();
    } catch (err) {
      log.error("Failed to initialize about pages:", err);
    }

    try {
      await AddonStudies.init();
    } catch (err) {
      log.error("Failed to initialize addon studies:", err);
    }

    // Initialize experiments first to avoid a race between initializing prefs
    // and recipes rolling back pref changes when experiments end.
    try {
      await PreferenceExperiments.init();
    } catch (err) {
      log.error("Failed to initialize preference experiments:", err);
    }

    try {
      ShieldPreferences.init();
    } catch (err) {
      log.error("Failed to initialize preferences UI:", err);
    }

    await RecipeRunner.init();
  },

  shutdown(reason) {
    CleanupManager.cleanup();
  },
};
