/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

const PREF_BRANCH = "extensions.webcompat.";
const PREF_DEFAULTS = {
  perform_injections: true,
  perform_ua_overrides: true
};

const INJECTIONS_ENABLE_PREF_NAME = "extensions.webcompat.perform_injections";

const BROWSER_STARTUP_FINISHED_TOPIC = "browser-delayed-startup-finished";

const UA_OVERRIDES_INIT_TOPIC = "useragentoverrides-initialized";
const UA_ENABLE_PREF_NAME = "extensions.webcompat.perform_ua_overrides";

ChromeUtils.defineModuleGetter(this, "UAOverrider", "chrome://webcompat/content/lib/ua_overrider.jsm");
ChromeUtils.defineModuleGetter(this, "UAOverrides", "chrome://webcompat/content/data/ua_overrides.jsm");

let overrider;
let webextensionPort;

function InjectionsEnablePrefObserver() {
  let isEnabled = Services.prefs.getBoolPref(INJECTIONS_ENABLE_PREF_NAME);
  webextensionPort.postMessage({
    type: "injection-pref-changed",
    prefState: isEnabled
  });
}

function UAEnablePrefObserver() {
  let isEnabled = Services.prefs.getBoolPref(UA_ENABLE_PREF_NAME);
  overrider.setShouldOverride(isEnabled);
}

function setDefaultPrefs() {
  const branch = Services.prefs.getDefaultBranch(PREF_BRANCH);
  for (const [key, val] of Object.entries(PREF_DEFAULTS)) {
    // If someone beat us to setting a default, don't overwrite it.
    if (branch.getPrefType(key) !== branch.PREF_INVALID) {
      continue;
    }

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

this.install = function() {};
this.uninstall = function() {};

this.startup = function({webExtension}) {
  setDefaultPrefs();

  // Intentionally reset the preference on every browser restart to avoid site
  // breakage by accidentally toggled preferences or by leaving it off after
  // debugging a site.
  Services.prefs.clearUserPref(INJECTIONS_ENABLE_PREF_NAME);
  Services.prefs.addObserver(INJECTIONS_ENABLE_PREF_NAME, InjectionsEnablePrefObserver);

  Services.prefs.clearUserPref(UA_ENABLE_PREF_NAME);
  Services.prefs.addObserver(UA_ENABLE_PREF_NAME, UAEnablePrefObserver);

  // Listen to the useragentoverrides-initialized notification we get and
  // initialize our overrider there. This is done to avoid slowing down the
  // apparent startup process, since we avoid loading anything before the first
  // window is visible to the user. See bug 1371442 for details.
  let uaStartupObserver = {
    observe(aSubject, aTopic, aData) {
      if (aTopic !== UA_OVERRIDES_INIT_TOPIC) {
        return;
      }

      Services.obs.removeObserver(this, UA_OVERRIDES_INIT_TOPIC);
      overrider = new UAOverrider(UAOverrides);
      overrider.init();
    }
  };
  Services.obs.addObserver(uaStartupObserver, UA_OVERRIDES_INIT_TOPIC);

  // Observe browser-delayed-startup-finished and only initialize our embedded
  // WebExtension after that. Otherwise, we'd try to initialize as soon as the
  // browser starts up, which adds a heavy startup penalty.
  let appStartupObserver = {
    observe(aSubject, aTopic, aData) {
      webExtension.startup().then((api) => {
        api.browser.runtime.onConnect.addListener((port) => {
          webextensionPort = port;
        });

        return Promise.resolve();
      }).catch((ex) => {
        console.error(ex);
      });
      Services.obs.removeObserver(this, BROWSER_STARTUP_FINISHED_TOPIC);
    }
  };
  Services.obs.addObserver(appStartupObserver, BROWSER_STARTUP_FINISHED_TOPIC);
};

this.shutdown = function() {
  Services.prefs.removeObserver(INJECTIONS_ENABLE_PREF_NAME, InjectionsEnablePrefObserver);
  Services.prefs.removeObserver(UA_ENABLE_PREF_NAME, UAEnablePrefObserver);
};
