/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that the Tracking Protection section is visible in the Control Center
// and has the correct state for the cases when:
//   * A page with no tracking elements is loaded.
//   * A page with tracking elements is loaded and they are blocked.
//   * A page with tracking elements is loaded and they are not blocked.
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

function hidden(sel) {
  let win = gBrowser.ownerGlobal;
  let el = win.document.querySelector(sel);
  let display = win.getComputedStyle(el).getPropertyValue("display", null);
  return display === "none";
}

function testBenignPage() {
  ok (!TrackingProtection.content.hasAttribute("block-disabled"), "blocking not disabled");
  ok (!TrackingProtection.content.hasAttribute("block-active"), "blocking is not active");

  // Make sure that the no tracking elements message appears
  ok (!hidden("#tracking-not-detected"), "labelNoTracking is visible");
  ok (hidden("#tracking-loaded"), "labelTrackingLoaded is hidden");
  ok (hidden("#tracking-blocked"), "labelTrackingBlocked is hidden");
}

function testTrackingPage() {
  ok (!TrackingProtection.content.hasAttribute("block-disabled"), "blocking not disabled");
  ok (TrackingProtection.content.hasAttribute("block-active"), "blocking is active");

  // Make sure that the blocked tracking elements message appears
  ok (hidden("#tracking-not-detected"), "labelNoTracking is hidden");
  ok (hidden("#tracking-loaded"), "labelTrackingLoaded is hidden");
  ok (!hidden("#tracking-blocked"), "labelTrackingBlocked is visible");
}

function testTrackingPageWhitelisted() {
  ok (TrackingProtection.content.hasAttribute("block-disabled"), "blocking is disabled");
  ok (!TrackingProtection.content.hasAttribute("block-active"), "blocking is not active");

  // Make sure that the blocked tracking elements message appears
  ok (hidden("#tracking-not-detected"), "labelNoTracking is hidden");
  ok (!hidden("#tracking-loaded"), "labelTrackingLoaded is visible");
  ok (hidden("#tracking-blocked"), "labelTrackingBlocked is hidden");
}

add_task(function* () {
  yield updateTrackingProtectionDatabase();

  let tab = gBrowser.selectedTab = gBrowser.addTab();

  TrackingProtection = gBrowser.ownerGlobal.TrackingProtection;
  ok (TrackingProtection, "Functionality is attached to the browser window");
  is (TrackingProtection.enabled, Services.prefs.getBoolPref(PREF),
    "The initial enabled value is based on the default pref value");

  info("Enable Tracking Protection");
  Services.prefs.setBoolPref(PREF, true);
  ok (TrackingProtection.enabled, "Functionality is enabled after setting the pref");

  info("Point tab to a test page NOT containing tracking elements");
  yield promiseTabLoadEvent(tab, BENIGN_PAGE);
  testBenignPage();

  info("Point tab to a test page containing tracking elements");
  yield promiseTabLoadEvent(tab, TRACKING_PAGE);

  info("Tracking content must be blocked");
  testTrackingPage();

  info("Disable Tracking Content Protection for the page (which reloads the page)");
  TrackingProtection.disableForCurrentPage();

  info("Wait for tab to reload following tracking-protection page white-listing");
  yield promiseTabLoadEvent(tab);

  info("Tracking content must be white-listed (NOT blocked)");
  testTrackingPageWhitelisted();

  info("Re-enable Tracking Content Protection for the page (which reloads the page)");
  TrackingProtection.enableForCurrentPage();

  info("Wait for tab to reload following tracking-protection page white-listing");
  yield promiseTabLoadEvent(tab);

  info("Tracking content must be blocked");
  testTrackingPage();
});
