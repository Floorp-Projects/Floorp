/*
 * Test that the Tracking Protection icon is properly animated in the identity
 * block when loading tabs and switching between tabs.
 * See also Bug 1175858.
 */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;
const PREF = "privacy.trackingprotection.enabled";
const PB_PREF = "privacy.trackingprotection.pbmode.enabled";
const BENIGN_PAGE = "http://tracking.example.org/browser/browser/base/content/test/general/benignPage.html";
const TRACKING_PAGE = "http://tracking.example.org/browser/browser/base/content/test/general/trackingPage.html";
let TrackingProtection = null;
let tabbrowser = null;

let {UrlClassifierTestUtils} = Cu.import("resource://testing-common/UrlClassifierTestUtils.jsm", {});

registerCleanupFunction(function() {
  TrackingProtection = tabbrowser = null;
  UrlClassifierTestUtils.cleanupTestTrackers();
  Services.prefs.clearUserPref(PREF);
  Services.prefs.clearUserPref(PB_PREF);
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
});

function waitForSecurityChange(numChanges = 1) {
  return new Promise(resolve => {
    let n = 0;
    let listener = {
      onSecurityChange: function() {
        n = n + 1;
        info ("Recieved onSecurityChange event " + n + " of " + numChanges);
        if (n >= numChanges) {
          tabbrowser.removeProgressListener(listener);
          resolve();
        }
      }
    };
    tabbrowser.addProgressListener(listener);
  });
}

function* testTrackingProtectionAnimation() {
  info("Load a test page not containing tracking elements");
  let benignTab = yield BrowserTestUtils.openNewForegroundTab(tabbrowser, BENIGN_PAGE);

  ok(!TrackingProtection.icon.hasAttribute("state"), "icon: no state");
  ok(TrackingProtection.icon.hasAttribute("animate"), "icon: animate");

  info("Load a test page containing tracking elements");
  let trackingTab = yield BrowserTestUtils.openNewForegroundTab(tabbrowser, TRACKING_PAGE);

  ok(TrackingProtection.icon.hasAttribute("state"), "icon: state");
  ok(TrackingProtection.icon.hasAttribute("animate"), "icon: animate");

  info("Switch from tracking -> benign tab");
  let securityChanged = waitForSecurityChange();
  tabbrowser.selectedTab = benignTab;
  yield securityChanged;

  ok(!TrackingProtection.icon.hasAttribute("state"), "icon: no state");
  ok(!TrackingProtection.icon.hasAttribute("animate"), "icon: no animate");

  info("Switch from benign -> tracking tab");
  securityChanged = waitForSecurityChange();
  tabbrowser.selectedTab = trackingTab;
  yield securityChanged;

  ok(TrackingProtection.icon.hasAttribute("state"), "icon: state");
  ok(!TrackingProtection.icon.hasAttribute("animate"), "icon: no animate");

  info("Reload tracking tab");
  securityChanged = waitForSecurityChange(2);
  tabbrowser.reload();
  yield securityChanged;

  ok(TrackingProtection.icon.hasAttribute("state"), "icon: state");
  ok(TrackingProtection.icon.hasAttribute("animate"), "icon: animate");
}

add_task(function* testNormalBrowsing() {
  yield UrlClassifierTestUtils.addTestTrackers();

  tabbrowser = gBrowser;

  TrackingProtection = gBrowser.ownerGlobal.TrackingProtection;
  ok(TrackingProtection, "TP is attached to the browser window");

  Services.prefs.setBoolPref(PREF, true);
  ok(TrackingProtection.enabled, "TP is enabled after setting the pref");

  yield testTrackingProtectionAnimation();
});

add_task(function* testPrivateBrowsing() {
  let privateWin = yield promiseOpenAndLoadWindow({private: true}, true);
  tabbrowser = privateWin.gBrowser;

  TrackingProtection = tabbrowser.ownerGlobal.TrackingProtection;
  ok(TrackingProtection, "TP is attached to the private window");

  Services.prefs.setBoolPref(PB_PREF, true);
  ok(TrackingProtection.enabled, "TP is enabled after setting the pref");

  yield testTrackingProtectionAnimation();

  privateWin.close();
});
