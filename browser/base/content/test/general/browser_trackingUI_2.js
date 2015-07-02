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

function testTrackingPageOff() {
  ok (TrackingProtection.container.hidden, "The container is hidden");
}

function testBenignPageOff() {
  ok (TrackingProtection.container.hidden, "The container is hidden");
}

add_task(function* () {
  yield updateTrackingProtectionDatabase();

  let tab = gBrowser.selectedTab = gBrowser.addTab();

  TrackingProtection = gBrowser.ownerGlobal.TrackingProtection;
  ok (TrackingProtection, "TP is attached to the browser window");
  is (TrackingProtection.enabled, Services.prefs.getBoolPref(PREF),
    "TP.enabled is based on the original pref value");

  Services.prefs.setBoolPref(PREF, false);
  ok (!TrackingProtection.enabled, "TP is disabled after setting the pref");

  info("Load a test page containing tracking elements");
  yield promiseTabLoadEvent(tab, TRACKING_PAGE);
  testTrackingPageOff();

  info("Load a test page not containing tracking elements");
  yield promiseTabLoadEvent(tab, BENIGN_PAGE);
  testBenignPageOff();
});
