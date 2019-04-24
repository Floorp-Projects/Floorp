/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals ChromeUtils, Services */
/* exported HomePage */

"use strict";

var EXPORTED_SYMBOLS = ["HomePage"];

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(this, "PrivateBrowsingUtils",
                               "resource://gre/modules/PrivateBrowsingUtils.jsm");

const kPrefName = "browser.startup.homepage";

function getHomepagePref(useDefault) {
  let homePage;
  let prefs = Services.prefs;
  if (useDefault) {
    prefs = prefs.getDefaultBranch(null);
  }
  try {
    // Historically, this was a localizable pref, but default Firefox builds
    // don't use this.
    // Distributions and local customizations might still use this, so let's
    // keep it.
    homePage = prefs.getComplexValue(kPrefName,
                                     Ci.nsIPrefLocalizedString).data;
  } catch (ex) {}

  if (!homePage) {
    homePage = prefs.getStringPref(kPrefName);
  }

  // Apparently at some point users ended up with blank home pages somehow.
  // If that happens, reset the pref and read it again.
  if (!homePage && !useDefault) {
    Services.prefs.clearUserPref(kPrefName);
    homePage = getHomepagePref(true);
  }

  return homePage;
}

let HomePage = {
  get(aWindow) {
    let homePages = getHomepagePref();
    if (PrivateBrowsingUtils.permanentPrivateBrowsing ||
        (aWindow && PrivateBrowsingUtils.isWindowPrivate(aWindow))) {
      // If an extension controls the setting and does not have private
      // browsing permission, use the default setting.
      let extensionControlled = Services.prefs.getBoolPref("browser.startup.homepage_override.extensionControlled", false);
      let privateAllowed = Services.prefs.getBoolPref("browser.startup.homepage_override.privateAllowed", false);
      // There is a potential on upgrade that the prefs are not set yet, so we double check
      // for moz-extension.
      if (!privateAllowed && (extensionControlled || homePages.includes("moz-extension://"))) {
        return this.getDefault();
      }
    }

    return homePages;
  },

  getDefault() {
    return getHomepagePref(true);
  },

  get overridden() {
    return Services.prefs.prefHasUserValue(kPrefName);
  },

  get locked() {
    return Services.prefs.prefIsLocked(kPrefName);
  },

  get isDefault() {
    return HomePage.get() === HomePage.getDefault();
  },

  set(value) {
    Services.prefs.setStringPref(kPrefName, value);
  },

  reset() {
    Services.prefs.clearUserPref(kPrefName);
  },
};
