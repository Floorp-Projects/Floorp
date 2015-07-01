/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that the Tracking Protection section is never visible in the
// Control Center when the feature is off.
// See also Bugs 1175327 and 1043801.

let PREF = "privacy.trackingprotection.enabled";
let BENIGN_PAGE = "http://tracking.example.org/browser/browser/base/content/test/general/benignPage.html";
let TRACKING_PAGE = "http://tracking.example.org/browser/browser/base/content/test/general/trackingPage.html";
let TrackingProtection = null;

registerCleanupFunction(function() {
  TrackingProtection = null;
  Services.prefs.clearUserPref(PREF);
  gBrowser.removeCurrentTab();
});

function testTrackingPageOFF() {
  ok (TrackingProtection.container.hidden, "The container is hidden");
}

function testBenignPageOFF() {
  ok (TrackingProtection.container.hidden, "The container is hidden");
}

add_task(function* () {
  yield updateTrackingProtectionDatabase();

  let tab = gBrowser.selectedTab = gBrowser.addTab();

  TrackingProtection = gBrowser.ownerGlobal.TrackingProtection;
  ok (TrackingProtection, "Functionality is attached to the browser window");
  is (TrackingProtection.enabled, Services.prefs.getBoolPref(PREF),
    "The initial enabled value is based on the default pref value");

  info ("Disable Tracking Protection");
  Services.prefs.setBoolPref(PREF, false);
  ok (!TrackingProtection.enabled, "Functionality is disabled after setting the pref");

  info ("Point tab to a test page containing tracking elements");
  yield promiseTabLoadEvent(tab, TRACKING_PAGE);
  testTrackingPageOFF();

  info ("Point tab to a test page NOT containing tracking elements");
  yield promiseTabLoadEvent(tab, BENIGN_PAGE);
  testBenignPageOFF();
});
