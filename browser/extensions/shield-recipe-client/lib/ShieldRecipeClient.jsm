/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {utils: Cu, interfaces: Ci} = Components;
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/Log.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "LogManager",
  "resource://shield-recipe-client/lib/LogManager.jsm");
ChromeUtils.defineModuleGetter(this, "RecipeRunner",
  "resource://shield-recipe-client/lib/RecipeRunner.jsm");
ChromeUtils.defineModuleGetter(this, "CleanupManager",
  "resource://shield-recipe-client/lib/CleanupManager.jsm");
ChromeUtils.defineModuleGetter(this, "PreferenceExperiments",
  "resource://shield-recipe-client/lib/PreferenceExperiments.jsm");
ChromeUtils.defineModuleGetter(this, "AboutPages",
  "resource://shield-recipe-client-content/AboutPages.jsm");
ChromeUtils.defineModuleGetter(this, "ShieldPreferences",
  "resource://shield-recipe-client/lib/ShieldPreferences.jsm");
ChromeUtils.defineModuleGetter(this, "AddonStudies",
  "resource://shield-recipe-client/lib/AddonStudies.jsm");
ChromeUtils.defineModuleGetter(this, "TelemetryEvents",
  "resource://shield-recipe-client/lib/TelemetryEvents.jsm");

this.EXPORTED_SYMBOLS = ["ShieldRecipeClient"];

const PREF_LOGGING_LEVEL = "extensions.shield-recipe-client.logging.level";
const SHIELD_INIT_NOTIFICATION = "shield-init-complete";

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

    try {
      TelemetryEvents.init();
    } catch (err) {
      log.error("Failed to initialize telemetry events:", err);
    }

    await RecipeRunner.init();
    Services.obs.notifyObservers(null, SHIELD_INIT_NOTIFICATION);
  },

  async shutdown(reason) {
    await CleanupManager.cleanup();
  },
};
