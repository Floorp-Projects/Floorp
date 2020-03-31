/* eslint-disable mozilla/no-arbitrary-setTimeout */
/*
 * Test that the Content Blocking icon is properly animated in the identity
 * block when loading tabs and switching between tabs.
 * See also Bug 1175858.
 */

const TP_PREF = "privacy.trackingprotection.enabled";
const TP_PB_PREF = "privacy.trackingprotection.pbmode.enabled";
const NCB_PREF = "network.cookie.cookieBehavior";
const BENIGN_PAGE =
  "http://tracking.example.org/browser/browser/base/content/test/protectionsUI/benignPage.html";
const TRACKING_PAGE =
  "http://tracking.example.org/browser/browser/base/content/test/protectionsUI/trackingPage.html";
const COOKIE_PAGE =
  "http://tracking.example.org/browser/browser/base/content/test/protectionsUI/cookiePage.html";
const DTSCBN_PREF = "dom.testing.sync-content-blocking-notifications";

requestLongerTimeout(2);

registerCleanupFunction(function() {
  UrlClassifierTestUtils.cleanupTestTrackers();
  Services.prefs.clearUserPref(TP_PREF);
  Services.prefs.clearUserPref(TP_PB_PREF);
  Services.prefs.clearUserPref(NCB_PREF);
  Services.prefs.clearUserPref(DTSCBN_PREF);
});

async function testTrackingProtectionAnimation(tabbrowser) {
  Services.prefs.setBoolPref(DTSCBN_PREF, true);

  info("Load a test page not containing tracking elements");
  let benignTab = await BrowserTestUtils.openNewForegroundTab(
    tabbrowser,
    BENIGN_PAGE
  );
  let gProtectionsHandler = tabbrowser.ownerGlobal.gProtectionsHandler;

  ok(!gProtectionsHandler.iconBox.hasAttribute("active"), "iconBox not active");
  ok(
    !gProtectionsHandler.iconBox.hasAttribute("animate"),
    "iconBox not animating"
  );

  info("Load a test page containing tracking elements");
  let trackingTab = await BrowserTestUtils.openNewForegroundTab(
    tabbrowser,
    TRACKING_PAGE
  );

  ok(gProtectionsHandler.iconBox.hasAttribute("active"), "iconBox active");
  ok(gProtectionsHandler.iconBox.hasAttribute("animate"), "iconBox animating");
  await BrowserTestUtils.waitForEvent(
    gProtectionsHandler.animatedIcon,
    "animationend"
  );

  info("Load a test page containing tracking cookies");
  let trackingCookiesTab = await BrowserTestUtils.openNewForegroundTab(
    tabbrowser,
    COOKIE_PAGE
  );

  ok(gProtectionsHandler.iconBox.hasAttribute("active"), "iconBox active");
  ok(gProtectionsHandler.iconBox.hasAttribute("animate"), "iconBox animating");
  await BrowserTestUtils.waitForEvent(
    gProtectionsHandler.animatedIcon,
    "animationend"
  );

  info("Switch from tracking cookie -> benign tab");
  let securityChanged = waitForSecurityChange(1, tabbrowser.ownerGlobal);
  tabbrowser.selectedTab = benignTab;
  await securityChanged;

  ok(!gProtectionsHandler.iconBox.hasAttribute("active"), "iconBox not active");
  ok(
    !gProtectionsHandler.iconBox.hasAttribute("animate"),
    "iconBox not animating"
  );

  info("Switch from benign -> tracking tab");
  securityChanged = waitForSecurityChange(1, tabbrowser.ownerGlobal);
  tabbrowser.selectedTab = trackingTab;
  await securityChanged;

  ok(gProtectionsHandler.iconBox.hasAttribute("active"), "iconBox active");
  ok(
    !gProtectionsHandler.iconBox.hasAttribute("animate"),
    "iconBox not animating"
  );

  info("Switch from tracking -> tracking cookies tab");
  securityChanged = waitForSecurityChange(1, tabbrowser.ownerGlobal);
  tabbrowser.selectedTab = trackingCookiesTab;
  await securityChanged;

  ok(gProtectionsHandler.iconBox.hasAttribute("active"), "iconBox active");
  ok(
    !gProtectionsHandler.iconBox.hasAttribute("animate"),
    "iconBox not animating"
  );

  info("Reload tracking cookies tab");
  securityChanged = waitForSecurityChange(1, tabbrowser.ownerGlobal);
  let contentBlockingEvent = waitForContentBlockingEvent(
    2,
    tabbrowser.ownerGlobal
  );
  tabbrowser.reload();
  await Promise.all([securityChanged, contentBlockingEvent]);

  ok(gProtectionsHandler.iconBox.hasAttribute("active"), "iconBox active");
  ok(gProtectionsHandler.iconBox.hasAttribute("animate"), "iconBox animating");
  await BrowserTestUtils.waitForEvent(
    gProtectionsHandler.animatedIcon,
    "animationend"
  );

  info("Reload tracking tab");
  securityChanged = waitForSecurityChange(2, tabbrowser.ownerGlobal);
  contentBlockingEvent = waitForContentBlockingEvent(3, tabbrowser.ownerGlobal);
  tabbrowser.selectedTab = trackingTab;
  tabbrowser.reload();
  await Promise.all([securityChanged, contentBlockingEvent]);

  ok(gProtectionsHandler.iconBox.hasAttribute("active"), "iconBox active");
  ok(gProtectionsHandler.iconBox.hasAttribute("animate"), "iconBox animating");
  await BrowserTestUtils.waitForEvent(
    gProtectionsHandler.animatedIcon,
    "animationend"
  );

  info("Inject tracking cookie inside tracking tab");
  securityChanged = waitForSecurityChange(1, tabbrowser.ownerGlobal);
  let timeoutPromise = new Promise(resolve => setTimeout(resolve, 500));
  await SpecialPowers.spawn(tabbrowser.selectedBrowser, [], function() {
    content.postMessage("cookie", "*");
  });
  let result = await Promise.race([securityChanged, timeoutPromise]);
  is(result, undefined, "No securityChange events should be received");

  ok(gProtectionsHandler.iconBox.hasAttribute("active"), "iconBox active");
  ok(
    !gProtectionsHandler.iconBox.hasAttribute("animate"),
    "iconBox not animating"
  );

  info("Inject tracking element inside tracking tab");
  securityChanged = waitForSecurityChange(1, tabbrowser.ownerGlobal);
  timeoutPromise = new Promise(resolve => setTimeout(resolve, 500));
  await SpecialPowers.spawn(tabbrowser.selectedBrowser, [], function() {
    content.postMessage("tracking", "*");
  });
  result = await Promise.race([securityChanged, timeoutPromise]);
  is(result, undefined, "No securityChange events should be received");

  ok(gProtectionsHandler.iconBox.hasAttribute("active"), "iconBox active");
  ok(
    !gProtectionsHandler.iconBox.hasAttribute("animate"),
    "iconBox not animating"
  );

  tabbrowser.selectedTab = trackingCookiesTab;

  info("Inject tracking cookie inside tracking cookies tab");
  securityChanged = waitForSecurityChange(1, tabbrowser.ownerGlobal);
  timeoutPromise = new Promise(resolve => setTimeout(resolve, 500));
  await SpecialPowers.spawn(tabbrowser.selectedBrowser, [], function() {
    content.postMessage("cookie", "*");
  });
  result = await Promise.race([securityChanged, timeoutPromise]);
  is(result, undefined, "No securityChange events should be received");

  ok(gProtectionsHandler.iconBox.hasAttribute("active"), "iconBox active");
  ok(
    !gProtectionsHandler.iconBox.hasAttribute("animate"),
    "iconBox not animating"
  );

  info("Inject tracking element inside tracking cookies tab");
  securityChanged = waitForSecurityChange(1, tabbrowser.ownerGlobal);
  timeoutPromise = new Promise(resolve => setTimeout(resolve, 500));
  await SpecialPowers.spawn(tabbrowser.selectedBrowser, [], function() {
    content.postMessage("tracking", "*");
  });
  result = await Promise.race([securityChanged, timeoutPromise]);
  is(result, undefined, "No securityChange events should be received");

  ok(gProtectionsHandler.iconBox.hasAttribute("active"), "iconBox active");
  ok(
    !gProtectionsHandler.iconBox.hasAttribute("animate"),
    "iconBox not animating"
  );

  while (tabbrowser.tabs.length > 1) {
    tabbrowser.removeCurrentTab();
  }
}

add_task(async function testNormalBrowsing() {
  await UrlClassifierTestUtils.addTestTrackers();

  let gProtectionsHandler = gBrowser.ownerGlobal.gProtectionsHandler;
  ok(
    gProtectionsHandler,
    "gProtectionsHandler is attached to the browser window"
  );
  let TrackingProtection = gBrowser.ownerGlobal.TrackingProtection;
  ok(TrackingProtection, "TP is attached to the browser window");
  let ThirdPartyCookies = gBrowser.ownerGlobal.ThirdPartyCookies;
  ok(ThirdPartyCookies, "TPC is attached to the browser window");

  Services.prefs.setBoolPref(TP_PREF, true);
  ok(TrackingProtection.enabled, "TP is enabled after setting the pref");
  Services.prefs.setIntPref(
    NCB_PREF,
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER
  );
  ok(
    ThirdPartyCookies.enabled,
    "ThirdPartyCookies is enabled after setting the pref"
  );

  await testTrackingProtectionAnimation(gBrowser);
});

add_task(async function testPrivateBrowsing() {
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  let tabbrowser = privateWin.gBrowser;

  let gProtectionsHandler = tabbrowser.ownerGlobal.gProtectionsHandler;
  ok(
    gProtectionsHandler,
    "gProtectionsHandler is attached to the private window"
  );
  let TrackingProtection = tabbrowser.ownerGlobal.TrackingProtection;
  ok(TrackingProtection, "TP is attached to the private window");
  let ThirdPartyCookies = tabbrowser.ownerGlobal.ThirdPartyCookies;
  ok(ThirdPartyCookies, "TPC is attached to the browser window");

  Services.prefs.setBoolPref(TP_PB_PREF, true);
  ok(TrackingProtection.enabled, "TP is enabled after setting the pref");
  Services.prefs.setIntPref(
    NCB_PREF,
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER
  );
  ok(
    ThirdPartyCookies.enabled,
    "ThirdPartyCookies is enabled after setting the pref"
  );

  await testTrackingProtectionAnimation(tabbrowser);

  privateWin.close();
});
