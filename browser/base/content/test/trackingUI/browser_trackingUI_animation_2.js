/*
 * Test that the Content Blocking icon is properly animated in the identity
 * block when loading tabs and switching between tabs.
 * See also Bug 1175858.
 */

const CB_PREF = "browser.contentblocking.enabled";
const TP_PREF = "privacy.trackingprotection.enabled";
const TP_PB_PREF = "privacy.trackingprotection.enabled";
const BENIGN_PAGE = "http://tracking.example.org/browser/browser/base/content/test/trackingUI/benignPage.html";
const TRACKING_PAGE = "http://tracking.example.org/browser/browser/base/content/test/trackingUI/trackingPage.html";

registerCleanupFunction(function() {
  UrlClassifierTestUtils.cleanupTestTrackers();
  Services.prefs.clearUserPref(TP_PREF);
  Services.prefs.clearUserPref(TP_PB_PREF);
  Services.prefs.clearUserPref(CB_PREF);
});

function waitForSecurityChange(tabbrowser, numChanges = 1) {
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
      },
    };
    tabbrowser.addProgressListener(listener);
  });
}

async function testTrackingProtectionAnimation(tabbrowser) {
  info("Load a test page not containing tracking elements");
  let benignTab = await BrowserTestUtils.openNewForegroundTab(tabbrowser, BENIGN_PAGE);
  let ContentBlocking = tabbrowser.ownerGlobal.ContentBlocking;

  ok(!ContentBlocking.iconBox.hasAttribute("active"), "iconBox not active");
  ok(!ContentBlocking.iconBox.hasAttribute("animate"), "iconBox not animating");

  info("Load a test page containing tracking elements");
  let trackingTab = await BrowserTestUtils.openNewForegroundTab(tabbrowser, TRACKING_PAGE);

  ok(ContentBlocking.iconBox.hasAttribute("active"), "iconBox active");
  ok(ContentBlocking.iconBox.hasAttribute("animate"), "iconBox animating");

  info("Switch from tracking -> benign tab");
  let securityChanged = waitForSecurityChange(tabbrowser);
  tabbrowser.selectedTab = benignTab;
  await securityChanged;

  ok(!ContentBlocking.iconBox.hasAttribute("active"), "iconBox not active");
  ok(!ContentBlocking.iconBox.hasAttribute("animate"), "iconBox not animating");

  info("Switch from benign -> tracking tab");
  securityChanged = waitForSecurityChange(tabbrowser);
  tabbrowser.selectedTab = trackingTab;
  await securityChanged;

  ok(ContentBlocking.iconBox.hasAttribute("active"), "iconBox active");
  ok(!ContentBlocking.iconBox.hasAttribute("animate"), "iconBox not animating");

  info("Reload tracking tab");
  securityChanged = waitForSecurityChange(tabbrowser, 2);
  tabbrowser.reload();
  await securityChanged;

  ok(ContentBlocking.iconBox.hasAttribute("active"), "iconBox active");
  ok(ContentBlocking.iconBox.hasAttribute("animate"), "iconBox animating");

  while (tabbrowser.tabs.length > 1) {
    tabbrowser.removeCurrentTab();
  }
}

add_task(async function testNormalBrowsing() {
  await UrlClassifierTestUtils.addTestTrackers();

  let ContentBlocking = gBrowser.ownerGlobal.ContentBlocking;
  ok(ContentBlocking, "CB is attached to the browser window");
  let TrackingProtection = gBrowser.ownerGlobal.TrackingProtection;
  ok(TrackingProtection, "TP is attached to the browser window");

  Services.prefs.setBoolPref(TP_PREF, true);
  ok(TrackingProtection.enabled, "TP is enabled after setting the pref");
  Services.prefs.setBoolPref(CB_PREF, true);
  ok(ContentBlocking.enabled, "CB is enabled after setting the pref");

  await testTrackingProtectionAnimation(gBrowser);
});

add_task(async function testPrivateBrowsing() {
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({private: true});
  let tabbrowser = privateWin.gBrowser;

  let ContentBlocking = tabbrowser.ownerGlobal.ContentBlocking;
  ok(ContentBlocking, "CB is attached to the private window");
  let TrackingProtection = tabbrowser.ownerGlobal.TrackingProtection;
  ok(TrackingProtection, "TP is attached to the private window");

  Services.prefs.setBoolPref(TP_PB_PREF, true);
  ok(TrackingProtection.enabled, "TP is enabled after setting the pref");
  Services.prefs.setBoolPref(CB_PREF, true);
  ok(TrackingProtection.enabled, "CB is enabled after setting the pref");

  await testTrackingProtectionAnimation(tabbrowser);

  privateWin.close();
});
