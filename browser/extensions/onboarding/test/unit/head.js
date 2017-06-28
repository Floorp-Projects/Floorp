/**
 * Provides infrastructure for automated onboarding components tests.
 */

"use strict";

/* global Cc, Ci, Cu */
const {classes: Cc, interfaces: Ci, utils: Cu} = Components;
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

// Load our bootstrap extension manifest so we can access our chrome/resource URIs.
// Cargo culted from formautofill system add-on
const EXTENSION_ID = "onboarding@mozilla.org";
let extensionDir = Services.dirsvc.get("GreD", Ci.nsIFile);
extensionDir.append("browser");
extensionDir.append("features");
extensionDir.append(EXTENSION_ID);
// If the unpacked extension doesn't exist, use the packed version.
if (!extensionDir.exists()) {
  extensionDir = extensionDir.parent;
  extensionDir.leafName += ".xpi";
}
Components.manager.addBootstrappedManifestLocation(extensionDir);

const TOURSET_VERSION = 1;
const PREF_TOUR_TYPE = "browser.onboarding.tour-type";
const PREF_TOURSET_VERSION = "browser.onboarding.tourset-version";
const PREF_SEEN_TOURSET_VERSION = "browser.onboarding.seen-tourset-version";
const PREF_ONBOARDING_HIDDEN = "browser.onboarding.hidden";

function resetOnboardingDefaultState() {
  // All the prefs should be reset to what prefs should looks like in a new user profile
  Services.prefs.setBoolPref(PREF_ONBOARDING_HIDDEN, false);
  Services.prefs.setIntPref(PREF_TOURSET_VERSION, TOURSET_VERSION);
  Services.prefs.clearUserPref(PREF_SEEN_TOURSET_VERSION);
  Services.prefs.clearUserPref(PREF_TOUR_TYPE);
}
