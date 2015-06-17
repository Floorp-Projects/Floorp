/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that the Tracking Protection Doorhanger appears
// and has the correct state when tracking content is blocked (Bug 1043801)

var PREF = "privacy.trackingprotection.enabled";
var BENIGN_PAGE = "http://tracking.example.org/browser/browser/base/content/test/general/benignPage.html";
var TRACKING_PAGE = "http://tracking.example.org/browser/browser/base/content/test/general/trackingPage.html";

function testBenignPage(gTestBrowser)
{
  // Make sure the doorhanger does NOT appear
  var notification = PopupNotifications.getNotification("bad-content", gTestBrowser);
  is(notification, null, "Tracking Content Doorhanger did NOT appear when protection was ON and tracking was NOT present");
}

function* testTrackingPage(gTestBrowser)
{
  // Make sure the doorhanger appears
  var notification = PopupNotifications.getNotification("bad-content", gTestBrowser);
  isnot(notification, null, "Tracking Content Doorhanger did appear when protection was ON and tracking was present");
  notification.reshow();
  var notificationElement = PopupNotifications.panel.firstChild;

  // Wait for the method to be attached after showing the popup
  yield promiseWaitForCondition(() => {
    return notificationElement.disableTrackingContentProtection;
  });

  // Make sure the state of the doorhanger includes blocking tracking elements
  ok(notificationElement.isTrackingContentBlocked,
     "Tracking Content is being blocked");

  // Make sure the notification has no trackingblockdisabled attribute
  ok(!notificationElement.hasAttribute("trackingblockdisabled"),
    "Doorhanger must have no trackingblockdisabled attribute");
}

function* testTrackingPageWhitelisted(gTestBrowser)
{
  // Make sure the doorhanger appears
  var notification = PopupNotifications.getNotification("bad-content", gTestBrowser);
  isnot(notification, null, "Tracking Content Doorhanger did appear when protection was ON and tracking was present but white-listed");
  notification.reshow();
  var notificationElement = PopupNotifications.panel.firstChild;

  // Wait for the method to be attached after showing the popup
  yield promiseWaitForCondition(() => {
    return notificationElement.disableTrackingContentProtection;
  });

  var notificationElement = PopupNotifications.panel.firstChild;

  // Make sure the state of the doorhanger does NOT include blocking tracking elements
  ok(!notificationElement.isTrackingContentBlocked,
    "Tracking Content is NOT being blocked");

  // Make sure the notification has the trackingblockdisabled attribute set to true
  is(notificationElement.getAttribute("trackingblockdisabled"), "true",
    "Doorhanger must have [trackingblockdisabled='true'] attribute");
}

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

  // Enable Tracking Protection
  Services.prefs.setBoolPref(PREF, true);

  // Point tab to a test page NOT containing tracking elements
  yield promiseTabLoadEvent(tab, BENIGN_PAGE);
  testBenignPage(gBrowser.getBrowserForTab(tab));

  // Point tab to a test page containing tracking elements
  yield promiseTabLoadEvent(tab, TRACKING_PAGE);

  // Tracking content must be blocked
  yield testTrackingPage(gBrowser.getBrowserForTab(tab));

  // Disable Tracking Content Protection for the page (which reloads the page)
  PopupNotifications.panel.firstChild.disableTrackingContentProtection();

  // Wait for tab to reload following tracking-protection page white-listing
  yield promiseTabLoadEvent(tab);

  // Tracking content must be white-listed (NOT blocked)
  yield testTrackingPageWhitelisted(gBrowser.getBrowserForTab(tab));

  // Re-enable Tracking Content Protection for the page (which reloads the page)
  PopupNotifications.panel.firstChild.enableTrackingContentProtection();

  // Wait for tab to reload following tracking-protection page white-listing
  yield promiseTabLoadEvent(tab);

  // Tracking content must be blocked
  yield testTrackingPage(gBrowser.getBrowserForTab(tab));
});
