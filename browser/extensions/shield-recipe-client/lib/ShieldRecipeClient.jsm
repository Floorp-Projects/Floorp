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
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryEvents",
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
