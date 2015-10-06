/*
 * Test that the Tracking Protection section is never visible in the
 * Control Center when the feature is off.
 * See also Bugs 1175327, 1043801, 1178985.
 */

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;
const PREF = "privacy.trackingprotection.enabled";
const PB_PREF = "privacy.trackingprotection.pbmode.enabled";
const BENIGN_PAGE = "http://tracking.example.org/browser/browser/base/content/test/general/benignPage.html";
const TRACKING_PAGE = "http://tracking.example.org/browser/browser/base/content/test/general/trackingPage.html";
var TrackingProtection = null;
var tabbrowser = null;

var {UrlClassifierTestUtils} = Cu.import("resource://testing-common/UrlClassifierTestUtils.jsm", {});

registerCleanupFunction(function() {
  TrackingProtection = tabbrowser = null;
  UrlClassifierTestUtils.cleanupTestTrackers();
  Services.prefs.clearUserPref(PREF);
  Services.prefs.clearUserPref(PB_PREF);
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
});

function hidden(el) {
  let win = el.ownerDocument.defaultView;
  let display = win.getComputedStyle(el).getPropertyValue("display", null);
  let opacity = win.getComputedStyle(el).getPropertyValue("opacity", null);

  return display === "none" || opacity === "0";
}

add_task(function* testNormalBrowsing() {
  yield UrlClassifierTestUtils.addTestTrackers();

  tabbrowser = gBrowser;
  let {gIdentityHandler} = tabbrowser.ownerGlobal;
  let tab = tabbrowser.selectedTab = tabbrowser.addTab();

  TrackingProtection = tabbrowser.ownerGlobal.TrackingProtection;
  ok(TrackingProtection, "TP is attached to the browser window");
  is(TrackingProtection.enabled, Services.prefs.getBoolPref(PREF),
    "TP.enabled is based on the original pref value");

  Services.prefs.setBoolPref(PREF, true);
  ok(TrackingProtection.enabled, "TP is enabled after setting the pref");

  Services.prefs.setBoolPref(PREF, false);
  ok(!TrackingProtection.enabled, "TP is disabled after setting the pref");

  info("Load a test page containing tracking elements");
  yield promiseTabLoadEvent(tab, TRACKING_PAGE);
  gIdentityHandler._identityBox.click();
  ok(hidden(TrackingProtection.container), "The container is hidden");
  gIdentityHandler._identityPopup.hidden = true;

  info("Load a test page not containing tracking elements");
  yield promiseTabLoadEvent(tab, BENIGN_PAGE);
  gIdentityHandler._identityBox.click();
  ok(hidden(TrackingProtection.container), "The container is hidden");
  gIdentityHandler._identityPopup.hidden = true;
});

add_task(function* testPrivateBrowsing() {
  let privateWin = yield promiseOpenAndLoadWindow({private: true}, true);
  tabbrowser = privateWin.gBrowser;
  let {gIdentityHandler} = tabbrowser.ownerGlobal;
  let tab = tabbrowser.selectedTab = tabbrowser.addTab();

  TrackingProtection = tabbrowser.ownerGlobal.TrackingProtection;
  ok(TrackingProtection, "TP is attached to the private window");
  is(TrackingProtection.enabled, Services.prefs.getBoolPref(PB_PREF),
    "TP.enabled is based on the pb pref value");

  Services.prefs.setBoolPref(PB_PREF, true);
  ok(TrackingProtection.enabled, "TP is enabled after setting the pref");

  Services.prefs.setBoolPref(PB_PREF, false);
  ok(!TrackingProtection.enabled, "TP is disabled after setting the pref");

  info("Load a test page containing tracking elements");
  yield promiseTabLoadEvent(tab, TRACKING_PAGE);
  gIdentityHandler._identityBox.click();
  ok(hidden(TrackingProtection.container), "The container is hidden");
  gIdentityHandler._identityPopup.hidden = true;

  info("Load a test page not containing tracking elements");
  gIdentityHandler._identityBox.click();
  yield promiseTabLoadEvent(tab, BENIGN_PAGE);
  ok(hidden(TrackingProtection.container), "The container is hidden");
  gIdentityHandler._identityPopup.hidden = true;

  privateWin.close();
});
