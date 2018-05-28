/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals ChromeUtils, Services */
/* exported HomePage */

"use strict";

var EXPORTED_SYMBOLS = ["HomePage"];

ChromeUtils.import("resource://gre/modules/Services.jsm");

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
  get() {
    return getHomepagePref();
  },

  getDefault() {
    return getHomepagePref(true);
  },

  get overridden() {
    return Services.prefs.prefHasUserValue(kPrefName);
  },

  set(value) {
    Services.prefs.setStringPref(kPrefName, value);
  },

  reset() {
    Services.prefs.clearUserPref(kPrefName);
  }
};
