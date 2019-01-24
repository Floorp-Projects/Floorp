/* eslint-disable mozilla/no-arbitrary-setTimeout */
/*
 * Test that the Content Blocking icon is properly animated in the identity
 * block when loading tabs and switching between tabs.
 * See also Bug 1175858.
 */

const TP_PREF = "privacy.trackingprotection.enabled";
const TP_PB_PREF = "privacy.trackingprotection.enabled";
const NCB_PREF = "network.cookie.cookieBehavior";
const BENIGN_PAGE = "http://tracking.example.org/browser/browser/base/content/test/trackingUI/benignPage.html";
const TRACKING_PAGE = "http://tracking.example.org/browser/browser/base/content/test/trackingUI/trackingPage.html";
const COOKIE_PAGE = "http://tracking.example.org/browser/browser/base/content/test/trackingUI/cookiePage.html";

requestLongerTimeout(2);

registerCleanupFunction(function() {
  UrlClassifierTestUtils.cleanupTestTrackers();
  Services.prefs.clearUserPref(TP_PREF);
  Services.prefs.clearUserPref(TP_PB_PREF);
  Services.prefs.clearUserPref(NCB_PREF);
  Services.prefs.clearUserPref(ContentBlocking.prefIntroCount);
});

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
  await BrowserTestUtils.waitForEvent(ContentBlocking.animatedIcon, "animationend");

  info("Load a test page containing tracking cookies");
  let trackingCookiesTab = await BrowserTestUtils.openNewForegroundTab(tabbrowser, COOKIE_PAGE);

  ok(ContentBlocking.iconBox.hasAttribute("active"), "iconBox active");
  ok(ContentBlocking.iconBox.hasAttribute("animate"), "iconBox animating");
  await BrowserTestUtils.waitForEvent(ContentBlocking.animatedIcon, "animationend");

  info("Switch from tracking cookie -> benign tab");
  let securityChanged = waitForSecurityChange(1, tabbrowser.ownerGlobal);
  tabbrowser.selectedTab = benignTab;
  await securityChanged;

  ok(!ContentBlocking.iconBox.hasAttribute("active"), "iconBox not active");
  ok(!ContentBlocking.iconBox.hasAttribute("animate"), "iconBox not animating");

  info("Switch from benign -> tracking tab");
  securityChanged = waitForSecurityChange(1, tabbrowser.ownerGlobal);
  tabbrowser.selectedTab = trackingTab;
  await securityChanged;

  ok(ContentBlocking.iconBox.hasAttribute("active"), "iconBox active");
  ok(!ContentBlocking.iconBox.hasAttribute("animate"), "iconBox not animating");

  info("Switch from tracking -> tracking cookies tab");
  securityChanged = waitForSecurityChange(1, tabbrowser.ownerGlobal);
  tabbrowser.selectedTab = trackingCookiesTab;
  await securityChanged;

  ok(ContentBlocking.iconBox.hasAttribute("active"), "iconBox active");
  ok(!ContentBlocking.iconBox.hasAttribute("animate"), "iconBox not animating");

  info("Reload tracking cookies tab");
  securityChanged = waitForSecurityChange(1, tabbrowser.ownerGlobal);
  let contentBlockingEvent = waitForContentBlockingEvent(2, tabbrowser.ownerGlobal);
  tabbrowser.reload();
  await Promise.all([securityChanged, contentBlockingEvent]);

  ok(ContentBlocking.iconBox.hasAttribute("active"), "iconBox active");
  ok(ContentBlocking.iconBox.hasAttribute("animate"), "iconBox animating");
  await BrowserTestUtils.waitForEvent(ContentBlocking.animatedIcon, "animationend");

  info("Reload tracking tab");
  securityChanged = waitForSecurityChange(2, tabbrowser.ownerGlobal);
  contentBlockingEvent = waitForContentBlockingEvent(3, tabbrowser.ownerGlobal);
  tabbrowser.selectedTab = trackingTab;
  tabbrowser.reload();
  await Promise.all([securityChanged, contentBlockingEvent]);

  ok(ContentBlocking.iconBox.hasAttribute("active"), "iconBox active");
  ok(ContentBlocking.iconBox.hasAttribute("animate"), "iconBox animating");
  await BrowserTestUtils.waitForEvent(ContentBlocking.animatedIcon, "animationend");

  info("Inject tracking cookie inside tracking tab");
  securityChanged = waitForSecurityChange(1, tabbrowser.ownerGlobal);
  let timeoutPromise = new Promise(resolve => setTimeout(resolve, 500));
  await ContentTask.spawn(tabbrowser.selectedBrowser, {},
                          function() {
    content.postMessage("cookie", "*");
  });
  let result = await Promise.race([securityChanged, timeoutPromise]);
  is(result, undefined, "No securityChange events should be received");

  ok(ContentBlocking.iconBox.hasAttribute("active"), "iconBox active");
  ok(!ContentBlocking.iconBox.hasAttribute("animate"), "iconBox not animating");

  info("Inject tracking element inside tracking tab");
  securityChanged = waitForSecurityChange(1, tabbrowser.ownerGlobal);
  timeoutPromise = new Promise(resolve => setTimeout(resolve, 500));
  await ContentTask.spawn(tabbrowser.selectedBrowser, {},
                          function() {
    content.postMessage("tracking", "*");
  });
  result = await Promise.race([securityChanged, timeoutPromise]);
  is(result, undefined, "No securityChange events should be received");

  ok(ContentBlocking.iconBox.hasAttribute("active"), "iconBox active");
  ok(!ContentBlocking.iconBox.hasAttribute("animate"), "iconBox not animating");

  tabbrowser.selectedTab = trackingCookiesTab;

  info("Inject tracking cookie inside tracking cookies tab");
  securityChanged = waitForSecurityChange(1, tabbrowser.ownerGlobal);
  timeoutPromise = new Promise(resolve => setTimeout(resolve, 500));
  await ContentTask.spawn(tabbrowser.selectedBrowser, {},
                          function() {
    content.postMessage("cookie", "*");
  });
  result = await Promise.race([securityChanged, timeoutPromise]);
  is(result, undefined, "No securityChange events should be received");

  ok(ContentBlocking.iconBox.hasAttribute("active"), "iconBox active");
  ok(!ContentBlocking.iconBox.hasAttribute("animate"), "iconBox not animating");

  info("Inject tracking element inside tracking cookies tab");
  securityChanged = waitForSecurityChange(1, tabbrowser.ownerGlobal);
  timeoutPromise = new Promise(resolve => setTimeout(resolve, 500));
  await ContentTask.spawn(tabbrowser.selectedBrowser, {},
                          function() {
    content.postMessage("tracking", "*");
  });
  result = await Promise.race([securityChanged, timeoutPromise]);
  is(result, undefined, "No securityChange events should be received");

  ok(ContentBlocking.iconBox.hasAttribute("active"), "iconBox active");
  ok(!ContentBlocking.iconBox.hasAttribute("animate"), "iconBox not animating");

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
  let ThirdPartyCookies = gBrowser.ownerGlobal.ThirdPartyCookies;
  ok(ThirdPartyCookies, "TPC is attached to the browser window");

  Services.prefs.setBoolPref(TP_PREF, true);
  ok(TrackingProtection.enabled, "TP is enabled after setting the pref");
  Services.prefs.setIntPref(NCB_PREF, Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER);
  ok(ThirdPartyCookies.enabled, "ThirdPartyCookies is enabled after setting the pref");
  Services.prefs.setIntPref(ContentBlocking.prefIntroCount, ContentBlocking.MAX_INTROS);

  await testTrackingProtectionAnimation(gBrowser);
});

add_task(async function testPrivateBrowsing() {
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({private: true});
  let tabbrowser = privateWin.gBrowser;

  let ContentBlocking = tabbrowser.ownerGlobal.ContentBlocking;
  ok(ContentBlocking, "CB is attached to the private window");
  let TrackingProtection = tabbrowser.ownerGlobal.TrackingProtection;
  ok(TrackingProtection, "TP is attached to the private window");
  let ThirdPartyCookies = tabbrowser.ownerGlobal.ThirdPartyCookies;
  ok(ThirdPartyCookies, "TPC is attached to the browser window");

  Services.prefs.setBoolPref(TP_PB_PREF, true);
  ok(TrackingProtection.enabled, "TP is enabled after setting the pref");
  Services.prefs.setIntPref(NCB_PREF, Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER);
  ok(ThirdPartyCookies.enabled, "ThirdPartyCookies is enabled after setting the pref");
  Services.prefs.setIntPref(ContentBlocking.prefIntroCount, ContentBlocking.MAX_INTROS);

  await testTrackingProtectionAnimation(tabbrowser);

  privateWin.close();
});
