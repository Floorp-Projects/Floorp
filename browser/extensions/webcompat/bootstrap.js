/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

const PREF_BRANCH = "extensions.webcompat.";
const PREF_DEFAULTS = {perform_ua_overrides: true};

const UA_ENABLE_PREF_NAME = "extensions.webcompat.perform_ua_overrides";

XPCOMUtils.defineLazyModuleGetter(this, "UAOverrider", "chrome://webcompat/content/lib/ua_overrider.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UAOverrides", "chrome://webcompat/content/data/ua_overrides.jsm");

let overrider;
let tabUpdateHandler;

function UAEnablePrefObserver() {
  let isEnabled = Services.prefs.getBoolPref(UA_ENABLE_PREF_NAME);
  if (isEnabled && !overrider) {
    overrider = new UAOverrider(UAOverrides);
    overrider.init();
  } else if (!isEnabled && overrider) {
    overrider.uninit();
    overrider = null;
  }
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
  Services.prefs.clearUserPref(UA_ENABLE_PREF_NAME);
  Services.prefs.addObserver(UA_ENABLE_PREF_NAME, UAEnablePrefObserver, false);

  overrider = new UAOverrider(UAOverrides);
  overrider.init();

  // Initialize the embedded WebExtension that gets used to log a notifications
  // about altered User Agents into the sites developer console.
  // Per default, we can only log into the Browser Console, which is not very
  // helpful in our use case since we want to talk to the site's developers.
  // Note that this is only a temporary solution, which will get replaced
  // by a more advanced implementation that will include some additional
  // information like the reason why we override the User Agent.
  webExtension.startup().then((api) => {
    const {browser} = api;

    // In tablog.js, we have a listener to tabs.onUpdated. That listener sends
    // a message to us, containing the URL that has been loaded. Here, we check
    // if the URL is one of the URLs we store User Agent overrides for and if
    // so, we return true back to the background script, which in turn displays
    // a message in the site's developer console.
    tabUpdateHandler = function(message, sender, sendResponse) {
      try {
        if (overrider) {
          let hasUAOverride = overrider.hasUAForURIInCache(Services.io.newURI(message.url, null, null));
          sendResponse({reply: hasUAOverride});
        }
      } catch (exception) {
        sendResponse({reply: false});
      }
    };

    browser.runtime.onMessage.addListener(tabUpdateHandler);
    return;
  }).catch((reason) => {
    console.log(reason);
  });
};

this.shutdown = function() {
  Services.prefs.removeObserver(UA_ENABLE_PREF_NAME, UAEnablePrefObserver);

  if (overrider) {
    overrider.uninit();
  }
};
