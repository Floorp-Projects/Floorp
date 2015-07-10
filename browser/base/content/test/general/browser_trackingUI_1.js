/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that the Tracking Protection section is visible in the Control Center
// and has the correct state for the cases when:
//   * A page with no tracking elements is loaded.
//   * A page with tracking elements is loaded and they are blocked.
//   * A page with tracking elements is loaded and they are not blocked.
// See also Bugs 1175327, 1043801, 1178985

const PREF = "privacy.trackingprotection.enabled";
const PB_PREF = "privacy.trackingprotection.pbmode.enabled";
const BENIGN_PAGE = "http://tracking.example.org/browser/browser/base/content/test/general/benignPage.html";
const TRACKING_PAGE = "http://tracking.example.org/browser/browser/base/content/test/general/trackingPage.html";
let TrackingProtection = null;
let browser = null;

registerCleanupFunction(function() {
  TrackingProtection = browser = null;
  Services.prefs.clearUserPref(PREF);
  Services.prefs.clearUserPref(PB_PREF);
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
});

function hidden(sel) {
  let win = browser.ownerGlobal;
  let el = win.document.querySelector(sel);
  let display = win.getComputedStyle(el).getPropertyValue("display", null);
  return display === "none";
}

function clickButton(sel) {
  let win = browser.ownerGlobal;
  let el = win.document.querySelector(sel);
  el.doCommand();
}

function testBenignPage() {
  info("Non-tracking content must not be blocked");
  ok (!TrackingProtection.container.hidden, "The container is visible");
  ok (!TrackingProtection.content.hasAttribute("block-disabled"), "blocking not disabled");
  ok (!TrackingProtection.content.hasAttribute("block-active"), "blocking is not active");

  ok (hidden("#tracking-action-block"), "blockButton is hidden");
  ok (hidden("#tracking-action-unblock"), "unblockButton is hidden");

  // Make sure that the no tracking elements message appears
  ok (!hidden("#tracking-not-detected"), "labelNoTracking is visible");
  ok (hidden("#tracking-loaded"), "labelTrackingLoaded is hidden");
  ok (hidden("#tracking-blocked"), "labelTrackingBlocked is hidden");
}

function testTrackingPage() {
  info("Tracking content must be blocked");
  ok (!TrackingProtection.container.hidden, "The container is visible");
  ok (!TrackingProtection.content.hasAttribute("block-disabled"), "blocking not disabled");
  ok (TrackingProtection.content.hasAttribute("block-active"), "blocking is active");

  ok (hidden("#tracking-action-block"), "blockButton is hidden");
  ok (!hidden("#tracking-action-unblock"), "unblockButton is visible");

  // Make sure that the blocked tracking elements message appears
  ok (hidden("#tracking-not-detected"), "labelNoTracking is hidden");
  ok (hidden("#tracking-loaded"), "labelTrackingLoaded is hidden");
  ok (!hidden("#tracking-blocked"), "labelTrackingBlocked is visible");
}

function testTrackingPageWhitelisted() {
  info("Tracking content must be white-listed and not blocked");
  ok (!TrackingProtection.container.hidden, "The container is visible");
  ok (TrackingProtection.content.hasAttribute("block-disabled"), "blocking is disabled");
  ok (!TrackingProtection.content.hasAttribute("block-active"), "blocking is not active");

  ok (!hidden("#tracking-action-block"), "blockButton is visible");
  ok (hidden("#tracking-action-unblock"), "unblockButton is hidden");

  // Make sure that the blocked tracking elements message appears
  ok (hidden("#tracking-not-detected"), "labelNoTracking is hidden");
  ok (!hidden("#tracking-loaded"), "labelTrackingLoaded is visible");
  ok (hidden("#tracking-blocked"), "labelTrackingBlocked is hidden");
}

function* testTrackingProtectionForTab(tab) {
  info("Load a test page not containing tracking elements");
  yield promiseTabLoadEvent(tab, BENIGN_PAGE);
  testBenignPage();

  info("Load a test page containing tracking elements");
  yield promiseTabLoadEvent(tab, TRACKING_PAGE);
  testTrackingPage();

  info("Disable TP for the page (which reloads the page)");
  clickButton("#tracking-action-unblock");

  info("Wait for tab to reload following TP white-listing");
  yield promiseTabLoadEvent(tab);
  testTrackingPageWhitelisted();

  info("Re-enable TP for the page (which reloads the page)");
  clickButton("#tracking-action-block");

  info("Wait for tab to reload following TP black-listing");
  yield promiseTabLoadEvent(tab);
  testTrackingPage();
}

add_task(function* testNormalBrowsing() {
  yield updateTrackingProtectionDatabase();

  browser = gBrowser;
  let tab = browser.selectedTab = browser.addTab();

  TrackingProtection = gBrowser.ownerGlobal.TrackingProtection;
  ok (TrackingProtection, "TP is attached to the browser window");
  is (TrackingProtection.enabled, Services.prefs.getBoolPref(PREF),
    "TP.enabled is based on the original pref value");

  Services.prefs.setBoolPref(PREF, true);
  ok (TrackingProtection.enabled, "TP is enabled after setting the pref");

  yield testTrackingProtectionForTab(tab);

  Services.prefs.setBoolPref(PREF, false);
  ok (!TrackingProtection.enabled, "TP is disabled after setting the pref");
});

add_task(function* testPrivateBrowsing() {
  let privateWin = yield promiseOpenAndLoadWindow({private: true}, true);
  browser = privateWin.gBrowser;
  let tab = browser.selectedTab = browser.addTab();

  TrackingProtection = browser.ownerGlobal.TrackingProtection;
  ok (TrackingProtection, "TP is attached to the private window");
  is (TrackingProtection.enabled, Services.prefs.getBoolPref(PB_PREF),
    "TP.enabled is based on the pb pref value");

  Services.prefs.setBoolPref(PB_PREF, true);
  ok (TrackingProtection.enabled, "TP is enabled after setting the pref");

  yield testTrackingProtectionForTab(tab);

  Services.prefs.setBoolPref(PB_PREF, false);
  ok (!TrackingProtection.enabled, "TP is disabled after setting the pref");

  privateWin.close();
});
