/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that the Tracking Protection Doorhanger does not ever appear
// when the feature is off (Bug 1043801)

var PREF = "privacy.trackingprotection.enabled";
var BENIGN_PAGE = "http://tracking.example.org/browser/browser/base/content/test/general/benignPage.html";
var TRACKING_PAGE = "http://tracking.example.org/browser/browser/base/content/test/general/trackingPage.html";

function testTrackingPageOFF(gTestBrowser)
{
  // Make sure the doorhanger does NOT appear
  var notification = PopupNotifications.getNotification("bad-content", gTestBrowser);
  is(notification, null, "Tracking Content Doorhanger did NOT appear when protection was OFF and tracking was present");
}

function testBenignPageOFF(gTestBrowser)
{
  // Make sure the doorhanger does NOT appear
  var notification = PopupNotifications.getNotification("bad-content", gTestBrowser);
  is(notification, null, "Tracking Content Doorhanger did NOT appear when protection was OFF and tracking was NOT present");
}

add_task(function* () {
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref(PREF);
    gBrowser.removeCurrentTab();
  });

  yield updateTrackingProtectionDatabase();

  let tab = gBrowser.selectedTab = gBrowser.addTab();

  // Disable Tracking Protection
  Services.prefs.setBoolPref(PREF, false);

  // Point tab to a test page containing tracking elements
  yield promiseTabLoadEvent(tab, TRACKING_PAGE);
  testTrackingPageOFF(gBrowser.getBrowserForTab(tab));

  // Point tab to a test page NOT containing tracking elements
  yield promiseTabLoadEvent(tab, BENIGN_PAGE);
  testBenignPageOFF(gBrowser.getBrowserForTab(tab));
});
