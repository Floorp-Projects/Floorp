/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "LogManager",
  "resource://shield-recipe-client/lib/LogManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "RecipeRunner",
  "resource://shield-recipe-client/lib/RecipeRunner.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CleanupManager",
  "resource://shield-recipe-client/lib/CleanupManager.jsm");

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
  api_url: "https://normandy.cdn.mozilla.net/api/v1",
  dev_mode: false,
  enabled: true,
  startup_delay_seconds: 300,
  "logging.level": Log.Level.Warn,
  user_id: "",
};
const PREF_DEV_MODE = "extensions.shield-recipe-client.dev_mode";
const PREF_SELF_SUPPORT_ENABLED = "browser.selfsupport.enabled";
const PREF_LOGGING_LEVEL = PREF_BRANCH + "logging.level";

let shouldRun = true;
let log = null;

this.install = function() {
  // Self Repair only checks its pref on start, so if we disable it, wait until
  // next startup to run, unless the dev_mode preference is set.
  if (Preferences.get(PREF_SELF_SUPPORT_ENABLED, true)) {
    Preferences.set(PREF_SELF_SUPPORT_ENABLED, false);
    if (!Preferences.get(PREF_DEV_MODE, false)) {
      shouldRun = false;
    }
  }
};

this.startup = function() {
  setDefaultPrefs();

  // Setup logging and listen for changes to logging prefs
  LogManager.configure(Services.prefs.getIntPref(PREF_LOGGING_LEVEL));
  log = LogManager.getLogger("bootstrap");
  Preferences.observe(PREF_LOGGING_LEVEL, LogManager.configure);
  CleanupManager.addCleanupHandler(
    () => Preferences.ignore(PREF_LOGGING_LEVEL, LogManager.configure));

  if (!shouldRun) {
    return;
  }

  RecipeRunner.init();
};

this.shutdown = function(data, reason) {
  CleanupManager.cleanup();

  if (reason === REASONS.ADDON_DISABLE || reason === REASONS.ADDON_UNINSTALL) {
    Services.prefs.setBoolPref(PREF_SELF_SUPPORT_ENABLED, true);
  }

  const modules = [
    "lib/CleanupManager.jsm",
    "lib/ClientEnvironment.jsm",
    "lib/FilterExpressions.jsm",
    "lib/EventEmitter.jsm",
    "lib/Heartbeat.jsm",
    "lib/LogManager.jsm",
    "lib/NormandyApi.jsm",
    "lib/NormandyDriver.jsm",
    "lib/RecipeRunner.jsm",
    "lib/Sampling.jsm",
    "lib/SandboxManager.jsm",
    "lib/Storage.jsm",
  ];
  for (const module of modules) {
    log.debug(`Unloading ${module}`);
    Cu.unload(`resource://shield-recipe-client/${module}`);
  }

  // Don't forget the logger!
  log = null;
};

this.uninstall = function() {
};

function setDefaultPrefs() {
  for (const [key, val] of Object.entries(DEFAULT_PREFS)) {
    const fullKey = PREF_BRANCH + key;
    // If someone beat us to setting a default, don't overwrite it.
    if (!Preferences.isSet(fullKey)) {
      Preferences.set(fullKey, val);
    }
  }
}
