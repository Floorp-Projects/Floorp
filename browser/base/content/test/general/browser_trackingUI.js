/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that the Tracking Protection Doorhanger appears
// and has the correct state when tracking content is blocked (Bug 1043801)

var PREF = "privacy.trackingprotection.enabled";
var TABLE = "urlclassifier.trackingTable";

// Update tracking database
function doUpdate() {
  // Add some URLs to the tracking database (to be blocked)
  var testData = "tracking.example.com/";
  var testUpdate =
    "n:1000\ni:test-track-simple\nad:1\n" +
    "a:524:32:" + testData.length + "\n" +
    testData;

  var dbService = Cc["@mozilla.org/url-classifier/dbservice;1"]
                  .getService(Ci.nsIUrlClassifierDBService);

  let deferred = Promise.defer();

  var listener = {
    QueryInterface: function(iid)
    {
      if (iid.equals(Ci.nsISupports) ||
          iid.equals(Ci.nsIUrlClassifierUpdateObserver))
        return this;

      throw Cr.NS_ERROR_NO_INTERFACE;
    },
    updateUrlRequested: function(url) { },
    streamFinished: function(status) { },
    updateError: function(errorCode) {
      ok(false, "Couldn't update classifier.");
      deferred.resolve();
    },
    updateSuccess: function(requestedTimeout) {
      deferred.resolve();
    }
  };

  dbService.beginUpdate(listener, "test-track-simple", "");
  dbService.beginStream("", "");
  dbService.updateStream(testUpdate);
  dbService.finishStream();
  dbService.finishUpdate();

  return deferred.promise;
}

function testBenignPage(gTestBrowser)
{
  // Make sure the doorhanger does NOT appear
  var notification = PopupNotifications.getNotification("bad-content", gTestBrowser);
  is(notification, null, "Tracking Content Doorhanger did NOT appear when protection was ON and tracking was NOT present");
}

function testTrackingPage(gTestBrowser)
{
  // Make sure the doorhanger appears
  var notification = PopupNotifications.getNotification("bad-content", gTestBrowser);
  isnot(notification, null, "Tracking Content Doorhanger did appear when protection was ON and tracking was present");
  notification.reshow();
  // Make sure the state of the doorhanger includes blocking tracking elements
  isnot(PopupNotifications.panel.firstChild.isTrackingContentBlocked, 0,
    "Tracking Content is being blocked");

  // Disable Tracking Content Protection for the page (which reloads the page)
  PopupNotifications.panel.firstChild.disableTrackingContentProtection();
}

function testTrackingPageWhitelisted(gTestBrowser)
{
  // Make sure the doorhanger appears
  var notification = PopupNotifications.getNotification("bad-content", gTestBrowser);
  isnot(notification, null, "Tracking Content Doorhanger did appear when protection was ON and tracking was present but white-listed");
  notification.reshow();
  // Make sure the state of the doorhanger does NOT include blocking tracking elements
  is(PopupNotifications.panel.firstChild.isTrackingContentBlocked, 0,
    "Tracking Content is NOT being blocked");
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
    Services.prefs.clearUserPref(TABLE);
    gBrowser.removeCurrentTab();
  });

  // Populate and use 'test-track-simple' for tracking protection lookups
  Services.prefs.setCharPref(TABLE, "test-track-simple");
  yield doUpdate();

  let tab = gBrowser.selectedTab = gBrowser.addTab();

  // Enable Tracking Protection
  Services.prefs.setBoolPref(PREF, true);

  // Point tab to a test page NOT containing tracking elements
  yield promiseTabLoadEvent(tab, "http://tracking.example.org/browser/browser/base/content/test/general/benignPage.html");
  testBenignPage(gBrowser.getBrowserForTab(tab));

  // Point tab to a test page containing tracking elements
  yield promiseTabLoadEvent(tab, "http://tracking.example.org/browser/browser/base/content/test/general/trackingPage.html");
  testTrackingPage(gBrowser.getBrowserForTab(tab));

  // Wait for tab to reload following tracking-protection page white-listing
  yield promiseTabLoadEvent(tab);
  // Tracking content must be white-listed (NOT blocked)
  testTrackingPageWhitelisted(gBrowser.getBrowserForTab(tab));

  // Disable Tracking Protection
  Services.prefs.setBoolPref(PREF, false);

  // Point tab to a test page containing tracking elements
  yield promiseTabLoadEvent(tab, "http://tracking.example.org/browser/browser/base/content/test/general/trackingPage.html");
  testTrackingPageOFF(gBrowser.getBrowserForTab(tab));

  // Point tab to a test page NOT containing tracking elements
  yield promiseTabLoadEvent(tab, "http://tracking.example.org/browser/browser/base/content/test/general/benignPage.html");
  testBenignPageOFF(gBrowser.getBrowserForTab(tab));
});
