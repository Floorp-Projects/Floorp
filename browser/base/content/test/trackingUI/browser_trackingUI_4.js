/*
 * Test that the Tracking Protection icon is properly animated in the identity
 * block when loading tabs and switching between tabs.
 * See also Bug 1175858.
 */

var TrackingProtection = null;
var tabbrowser = null;

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
      onSecurityChange() {
        n = n + 1;
        info("Received onSecurityChange event " + n + " of " + numChanges);
        if (n >= numChanges) {
          tabbrowser.removeProgressListener(listener);
          resolve();
        }
      }
    };
    tabbrowser.addProgressListener(listener);
  });
}

async function testTrackingProtectionAnimation() {
  info("Load a test page not containing tracking elements");
  let benignTab = await BrowserTestUtils.openNewForegroundTab(tabbrowser, BENIGN_PAGE);

  ok(!TrackingProtection.icon.hasAttribute("state"), "icon: no state");
  ok(TrackingProtection.icon.hasAttribute("animate"), "icon: animate");

  info("Load a test page containing tracking elements");
  let trackingTab = await BrowserTestUtils.openNewForegroundTab(tabbrowser, TRACKING_PAGE);

  ok(TrackingProtection.icon.hasAttribute("state"), "icon: state");
  ok(TrackingProtection.icon.hasAttribute("animate"), "icon: animate");

  info("Switch from tracking -> benign tab");
  let securityChanged = waitForSecurityChange();
  tabbrowser.selectedTab = benignTab;
  await securityChanged;

  ok(!TrackingProtection.icon.hasAttribute("state"), "icon: no state");
  ok(!TrackingProtection.icon.hasAttribute("animate"), "icon: no animate");

  info("Switch from benign -> tracking tab");
  securityChanged = waitForSecurityChange();
  tabbrowser.selectedTab = trackingTab;
  await securityChanged;

  ok(TrackingProtection.icon.hasAttribute("state"), "icon: state");
  ok(!TrackingProtection.icon.hasAttribute("animate"), "icon: no animate");

  info("Reload tracking tab");
  securityChanged = waitForSecurityChange(2);
  tabbrowser.reload();
  await securityChanged;

  ok(TrackingProtection.icon.hasAttribute("state"), "icon: state");
  ok(TrackingProtection.icon.hasAttribute("animate"), "icon: animate");
}

add_task(async function testNormalBrowsing() {
  await UrlClassifierTestUtils.addTestTrackers();

  tabbrowser = gBrowser;

  TrackingProtection = gBrowser.ownerGlobal.TrackingProtection;
  ok(TrackingProtection, "TP is attached to the browser window");

  Services.prefs.setBoolPref(PREF, true);
  ok(TrackingProtection.enabled, "TP is enabled after setting the pref");

  await testTrackingProtectionAnimation();
});

add_task(async function testPrivateBrowsing() {
  let privateWin = await promiseOpenAndLoadWindow({private: true}, true);
  tabbrowser = privateWin.gBrowser;

  TrackingProtection = tabbrowser.ownerGlobal.TrackingProtection;
  ok(TrackingProtection, "TP is attached to the private window");

  Services.prefs.setBoolPref(PB_PREF, true);
  ok(TrackingProtection.enabled, "TP is enabled after setting the pref");

  await testTrackingProtectionAnimation();

  privateWin.close();
});
