/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "resProto",
                                   "@mozilla.org/network/protocol;1?name=resource",
                                   "nsISubstitutingProtocolHandler");

const RESOURCE_HOST = "activity-stream";

/**
 * Check if an old pref has a custom value to migrate. Clears the pref so that
 * it's the default after migrating (to avoid future need to migrate).
 *
 * @param oldPrefName {string} Pref to check and migrate
 * @param cbIfNotDefault {function} Callback that gets the current pref value
 */
function migratePref(oldPrefName, cbIfNotDefault) {
  // Nothing to do if the user doesn't have a custom value
  if (!Services.prefs.prefHasUserValue(oldPrefName)) {
    return;
  }

  // Figure out what kind of pref getter to use
  let prefGetter;
  switch (Services.prefs.getPrefType(oldPrefName)) {
    case Services.prefs.PREF_BOOL:
      prefGetter = "getBoolPref";
      break;
    case Services.prefs.PREF_INT:
      prefGetter = "getIntPref";
      break;
    case Services.prefs.PREF_STRING:
      prefGetter = "getStringPref";
      break;
  }

  // Give the callback the current value then clear the pref
  cbIfNotDefault(Services.prefs[prefGetter](oldPrefName));
  Services.prefs.clearUserPref(oldPrefName);
}

// The functions below are required by bootstrap.js

this.install = function install(data, reason) {};

this.startup = function startup(data, reason) {
  resProto.setSubstitutionWithFlags(RESOURCE_HOST,
                                    Services.io.newURI("chrome/content/", null, data.resourceURI),
                                    resProto.ALLOW_CONTENT_ACCESS);

  // Do a one time migration of Tiles about:newtab prefs that have been modified
  migratePref("browser.newtabpage.rows", rows => {
    // Just disable top sites if rows are not desired
    if (rows <= 0) {
      Services.prefs.setBoolPref("browser.newtabpage.activity-stream.feeds.topsites", false);
    } else {
      Services.prefs.setIntPref("browser.newtabpage.activity-stream.topSitesRows", rows);
    }
  });

  migratePref("browser.newtabpage.activity-stream.showTopSites", value => {
    if (value === false) {
      Services.prefs.setBoolPref("browser.newtabpage.activity-stream.feeds.topsites", false);
    }
  });

  // Old activity stream topSitesCount pref showed 6 per row
  migratePref("browser.newtabpage.activity-stream.topSitesCount", count => {
    Services.prefs.setIntPref("browser.newtabpage.activity-stream.topSitesRows", Math.ceil(count / 6));
  });
};

this.shutdown = function shutdown(data, reason) {
  resProto.setSubstitution(RESOURCE_HOST, null);
};

this.uninstall = function uninstall(data, reason) {};
