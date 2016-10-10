/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");

const REASONS = {
  APP_STARTUP: 1,      // The application is starting up.
  APP_SHUTDOWN: 2,     // The application is shutting down.
  ADDON_ENABLE: 3,     // The add-on is being enabled.
  ADDON_DISABLE: 4,    // The add-on is being disabled. (Also sent during uninstallation)
  ADDON_INSTALL: 5,    // The add-on is being installed.
  ADDON_UNINSTALL: 6,  // The add-on is being uninstalled.
  ADDON_UPGRADE: 7,    // The add-on is being upgraded.
  ADDON_DOWNGRADE: 8,  //The add-on is being downgraded.
};

const PREF_BRANCH = "extensions.shield-recipe-client.";
const PREFS = {
  api_url: "https://self-repair.mozilla.org/api/v1",
  dev_mode: false,
  enabled: true,
  startup_delay_seconds: 300,
};
const PREF_DEV_MODE = "extensions.shield-recipe-client.dev_mode";
const PREF_SELF_SUPPORT_ENABLED = "browser.selfsupport.enabled";

let shouldRun = true;

this.install = function() {
  // Self Repair only checks its pref on start, so if we disable it, wait until
  // next startup to run, unless the dev_mode preference is set.
  if (Preferences.get(PREF_SELF_SUPPORT_ENABLED, true)) {
    Preferences.set(PREF_SELF_SUPPORT_ENABLED, false);
    if (!Services.prefs.getBoolPref(PREF_DEV_MODE, false)) {
      shouldRun = false;
    }
  }
};

this.startup = function() {
  setDefaultPrefs();

  if (!shouldRun) {
    return;
  }

  Cu.import("resource://shield-recipe-client/lib/RecipeRunner.jsm");
  RecipeRunner.init();
};

this.shutdown = function(data, reason) {
  Cu.import("resource://shield-recipe-client/lib/CleanupManager.jsm");

  CleanupManager.cleanup();

  if (reason === REASONS.ADDON_DISABLE || reason === REASONS.ADDON_UNINSTALL) {
    Services.prefs.setBoolPref(PREF_SELF_SUPPORT_ENABLED, true);
  }

  const modules = [
    "data/EventEmitter.js",
    "lib/CleanupManager.jsm",
    "lib/EnvExpressions.jsm",
    "lib/Heartbeat.jsm",
    "lib/NormandyApi.jsm",
    "lib/NormandyDriver.jsm",
    "lib/RecipeRunner.jsm",
    "lib/Sampling.jsm",
    "lib/SandboxManager.jsm",
    "lib/Storage.jsm",
  ];
  for (const module in modules) {
    Cu.unload(`resource://shield-recipe-client/${module}`);
  }
};

this.uninstall = function() {
};

function setDefaultPrefs() {
  const branch = Services.prefs.getDefaultBranch(PREF_BRANCH);
  for (const [key, val] of Object.entries(PREFS)) {
    // If someone beat us to setting a default, don't overwrite it.
    if (branch.getPrefType(key) !== branch.PREF_INVALID)
      continue;
    switch (typeof val) {
      case "boolean":
        branch.setBoolPref(key, val);
        break;
      case "number":
        branch.setIntPref(key, val);
        break;
      case "string":
        branch.setCharPref(key, val);
        break;
    }
  }
}
