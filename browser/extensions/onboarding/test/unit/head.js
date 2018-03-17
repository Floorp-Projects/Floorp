/**
 * Provides infrastructure for automated onboarding components tests.
 */

"use strict";

/* global Cc, Ci, Cu */
ChromeUtils.import("resource://gre/modules/Preferences.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "resProto",
                                   "@mozilla.org/network/protocol;1?name=resource",
                                   "nsISubstitutingProtocolHandler");

// Load our bootstrap extension manifest so we can access our chrome/resource URIs.
// Cargo culted from formautofill system add-on
const EXTENSION_ID = "onboarding@mozilla.org";
let extensionDir = Services.dirsvc.get("GreD", Ci.nsIFile);
extensionDir.append("browser");
extensionDir.append("features");
extensionDir.append(EXTENSION_ID);
let resourceURI;
// If the unpacked extension doesn't exist, use the packed version.
if (!extensionDir.exists()) {
  extensionDir.leafName += ".xpi";

  resourceURI = "jar:" + Services.io.newFileURI(extensionDir).spec + "!/chrome/content/";
} else {
  resourceURI = Services.io.newFileURI(extensionDir).spec + "/chrome/content/";
}
Components.manager.addBootstrappedManifestLocation(extensionDir);

resProto.setSubstitution("onboarding", Services.io.newURI(resourceURI));

const TOURSET_VERSION = 1;
const NEXT_TOURSET_VERSION = 2;
const PREF_TOUR_TYPE = "browser.onboarding.tour-type";
const PREF_TOURSET_VERSION = "browser.onboarding.tourset-version";
const PREF_SEEN_TOURSET_VERSION = "browser.onboarding.seen-tourset-version";

function resetOnboardingDefaultState() {
  // All the prefs should be reset to what prefs should looks like in a new user profile
  Services.prefs.setIntPref(PREF_TOURSET_VERSION, TOURSET_VERSION);
  Services.prefs.clearUserPref(PREF_SEEN_TOURSET_VERSION);
  Services.prefs.clearUserPref(PREF_TOUR_TYPE);
}

function resetOldProfileDefaultState() {
  // All the prefs should be reset to what prefs should looks like in a older new user profile
  Services.prefs.setIntPref(PREF_TOURSET_VERSION, TOURSET_VERSION);
  Services.prefs.setIntPref(PREF_SEEN_TOURSET_VERSION, 0);
  Services.prefs.clearUserPref(PREF_TOUR_TYPE);
}
