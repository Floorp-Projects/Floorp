/*
 * Test that the Tracking Protection section is visible in the Control Center
 * and has the correct state for the cases when:
 *
 * In a normal window as well as a private window,
 *   With TP enabled
 *     1) A page with no tracking elements is loaded.
 *     2) A page with tracking elements is loaded and they are blocked.
 *     3) A page with tracking elements is loaded and they are not blocked.
 *   With TP disabled
 *     1) A page with no tracking elements is loaded.
 *     2) A page with tracking elements is loaded.
 *
 * See also Bugs 1175327, 1043801, 1178985
 */

const CB_PREF = "browser.contentblocking.enabled";
const CB_UI_PREF = "browser.contentblocking.ui.enabled";
const TP_PREF = "privacy.trackingprotection.enabled";
const TP_PB_PREF = "privacy.trackingprotection.pbmode.enabled";
const FB_PREF = "browser.fastblock.enabled";
const FB_TIMEOUT_PREF = "browser.fastblock.timeout";
const FB_LIMIT_PREF = "browser.fastblock.limit";
const TPC_PREF = "network.cookie.cookieBehavior";
const BENIGN_PAGE = "http://tracking.example.org/browser/browser/base/content/test/trackingUI/benignPage.html";
const TRACKING_PAGE = "http://tracking.example.org/browser/browser/base/content/test/trackingUI/trackingPage.html";
const COOKIE_PAGE = "http://not-tracking.example.com/browser/browser/base/content/test/trackingUI/cookiePage.html";
var ContentBlocking = null;
var FastBlock = null;
var TrackingProtection = null;
var ThirdPartyCookies = null;
var tabbrowser = null;
var gTrackingPageURL = TRACKING_PAGE;

registerCleanupFunction(function() {
  TrackingProtection = ContentBlocking = FastBlock =
    ThirdPartyCookies = tabbrowser = null;
  UrlClassifierTestUtils.cleanupTestTrackers();
  Services.prefs.clearUserPref(TP_PREF);
  Services.prefs.clearUserPref(TP_PB_PREF);
  Services.prefs.clearUserPref(CB_PREF);
  Services.prefs.clearUserPref(FB_PREF);
  Services.prefs.clearUserPref(FB_TIMEOUT_PREF);
  Services.prefs.clearUserPref(FB_LIMIT_PREF);
  Services.prefs.clearUserPref(TPC_PREF);
});

// This is a special version of "hidden" that doesn't check for item
// visibility and just asserts the display and opacity attributes.
// That way we can test elements even when their panel is hidden...
function hidden(sel) {
  let win = tabbrowser.ownerGlobal;
  let el = win.document.querySelector(sel);
  let display = win.getComputedStyle(el).getPropertyValue("display", null);
  let opacity = win.getComputedStyle(el).getPropertyValue("opacity", null);
  return display === "none" || opacity === "0";
}

function clickButton(sel) {
  let win = tabbrowser.ownerGlobal;
  let el = win.document.querySelector(sel);
  el.doCommand();
}

function testBenignPage() {
  info("Non-tracking content must not be blocked");
  ok(!ContentBlocking.content.hasAttribute("detected"), "no trackers are detected");
  ok(!ContentBlocking.content.hasAttribute("hasException"), "content shows no exception");
  ok(!ContentBlocking.content.hasAttribute("active"), "content is not active");

  ok(!ContentBlocking.iconBox.hasAttribute("active"), "shield is not active");
  ok(!ContentBlocking.iconBox.hasAttribute("hasException"), "icon box shows no exception");
  ok(!ContentBlocking.iconBox.hasAttribute("tooltiptext"), "icon box has no tooltip");

  ok(BrowserTestUtils.is_hidden(ContentBlocking.iconBox), "icon box is hidden");
  ok(hidden("#tracking-action-block"), "blockButton is hidden");
  ok(hidden("#tracking-action-unblock"), "unblockButton is hidden");
  is(!hidden("#identity-popup-content-blocking-disabled-label"), !ContentBlocking.enabled,
    "disabled label is visible if CB is off");

  ok(!hidden("#identity-popup-content-blocking-not-detected"), "blocking not detected label is visible");
  ok(hidden("#identity-popup-content-blocking-detected"), "blocking detected label is hidden");

  if (Services.prefs.getBoolPref(CB_UI_PREF)) {
    ok(hidden("#identity-popup-content-blocking-category-list"), "category list is hidden");
  }
}

function testBenignPageWithException() {
  info("Non-tracking content must not be blocked");
  ok(!ContentBlocking.content.hasAttribute("detected"), "no trackers are detected");
  ok(ContentBlocking.content.hasAttribute("hasException"), "content shows exception");
  ok(!ContentBlocking.content.hasAttribute("active"), "content is not active");

  ok(!ContentBlocking.iconBox.hasAttribute("active"), "shield is not active");
  is(ContentBlocking.iconBox.hasAttribute("hasException"), ContentBlocking.enabled,
    "shield shows exception if CB is on");
  is(ContentBlocking.iconBox.getAttribute("tooltiptext"),
     gNavigatorBundle.getString("trackingProtection.icon.disabledTooltip"), "correct tooltip");

  is(!BrowserTestUtils.is_hidden(ContentBlocking.iconBox), ContentBlocking.enabled,
    "icon box is not hidden if CB is on");
  is(!hidden("#tracking-action-block"), ContentBlocking.enabled,
     "blockButton is visible if CB is on");
  ok(hidden("#tracking-action-unblock"), "unblockButton is hidden");
  ok(!hidden("#identity-popup-content-blocking-disabled-label"), "disabled label is visible");

  ok(!hidden("#identity-popup-content-blocking-not-detected"), "blocking not detected label is visible");
  ok(hidden("#identity-popup-content-blocking-detected"), "blocking detected label is hidden");

  if (Services.prefs.getBoolPref(CB_UI_PREF)) {
    ok(hidden("#identity-popup-content-blocking-category-list"), "category list is hidden");
  }
}

function areTrackersBlocked(isPrivateBrowsing) {
  let cbEnabled = Services.prefs.getBoolPref(CB_PREF);
  let blockedByTP = cbEnabled &&
                    Services.prefs.getBoolPref(isPrivateBrowsing ? TP_PB_PREF : TP_PREF);
  let blockedByFB = cbEnabled &&
                    Services.prefs.getBoolPref(FB_PREF) &&
                    // The timeout pref is only checked for completeness,
                    // checking it is technically unneeded for this test.
                    Services.prefs.getIntPref(FB_TIMEOUT_PREF) == 0;
  let blockedByTPC = cbEnabled &&
                     Services.prefs.getIntPref(TPC_PREF) == Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER;
  return blockedByTP || blockedByFB || blockedByTPC;
}

function testTrackingPage(window) {
  info("Tracking content must be blocked");
  ok(ContentBlocking.content.hasAttribute("detected"), "trackers are detected");
  ok(!ContentBlocking.content.hasAttribute("hasException"), "content shows no exception");

  let isPrivateBrowsing = PrivateBrowsingUtils.isWindowPrivate(window);
  let blockedByTP = areTrackersBlocked(isPrivateBrowsing);
  is(BrowserTestUtils.is_visible(ContentBlocking.iconBox), blockedByTP,
     "icon box is" + (blockedByTP ? "" : " not") + " visible");
  is(ContentBlocking.content.hasAttribute("active"), blockedByTP,
      "content is" + (blockedByTP ? "" : " not") + " active");
  is(ContentBlocking.iconBox.hasAttribute("active"), blockedByTP,
      "shield is" + (blockedByTP ? "" : " not") + " active");
  ok(!ContentBlocking.iconBox.hasAttribute("hasException"), "icon box shows no exception");
  is(ContentBlocking.iconBox.getAttribute("tooltiptext"),
     blockedByTP ? gNavigatorBundle.getString("trackingProtection.icon.activeTooltip") : "",
     "correct tooltip");

  ok(hidden("#tracking-action-block"), "blockButton is hidden");

  let isWindowPrivate = PrivateBrowsingUtils.isWindowPrivate(window);
  let cbUIEnabled = Services.prefs.getBoolPref(CB_UI_PREF);
  if (isWindowPrivate) {
    ok(hidden("#tracking-action-unblock"), "unblockButton is hidden");
    is(!hidden("#tracking-action-unblock-private"), blockedByTP,
       "unblockButtonPrivate is" + (blockedByTP ? "" : " not") + " visible");
  } else {
    ok(hidden("#tracking-action-unblock-private"), "unblockButtonPrivate is hidden");
    is(!hidden("#tracking-action-unblock"), blockedByTP,
       "unblockButton is" + (blockedByTP ? "" : " not") + " visible");
  }

  ok(hidden("#identity-popup-content-blocking-not-detected"), "blocking not detected label is hidden");
  ok(!hidden("#identity-popup-content-blocking-detected"), "blocking detected label is visible");

  if (cbUIEnabled) {
    ok(!hidden("#identity-popup-content-blocking-category-list"), "category list is visible");
    let category;
    if (Services.prefs.getBoolPref(FB_PREF)) {
      category = "#identity-popup-content-blocking-category-fastblock";
    } else {
      category = Services.prefs.getIntPref(TPC_PREF) == Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER ?
                   "#identity-popup-content-blocking-category-3rdpartycookies" :
                   "#identity-popup-content-blocking-category-tracking-protection";
    }
    is(hidden(category + " > .identity-popup-content-blocking-category-add-blocking"), blockedByTP,
      "Category item is" + (blockedByTP ? " not" : "") + " showing add blocking");
    is(hidden(category + " > .identity-popup-content-blocking-category-state-label"), !blockedByTP,
      "Category item is" + (blockedByTP ? "" : " not") + " set to blocked");
  }
}

function testTrackingPageUnblocked(blockedByTP, window) {
  info("Tracking content must be white-listed and not blocked");
  ok(ContentBlocking.content.hasAttribute("detected"), "trackers are detected");
  ok(ContentBlocking.content.hasAttribute("hasException"), "content shows exception");

  let isWindowPrivate = PrivateBrowsingUtils.isWindowPrivate(window);
  let cbUIEnabled = Services.prefs.getBoolPref(CB_UI_PREF);
  let tpEnabled = isWindowPrivate ? Services.prefs.getBoolPref(TP_PB_PREF) : Services.prefs.getBoolPref(TP_PREF);
  let blockingEnabled = cbUIEnabled ? Services.prefs.getBoolPref(CB_PREF) : tpEnabled;
  ok(!ContentBlocking.content.hasAttribute("active"), "content is not active");
  ok(!ContentBlocking.iconBox.hasAttribute("active"), "shield is not active");
  is(ContentBlocking.iconBox.hasAttribute("hasException"), blockingEnabled,
     "shield" + (blockingEnabled ? " shows" : " doesn't show") + " exception");
  is(ContentBlocking.iconBox.getAttribute("tooltiptext"),
     gNavigatorBundle.getString("trackingProtection.icon.disabledTooltip"), "correct tooltip");

  is(BrowserTestUtils.is_visible(ContentBlocking.iconBox), blockingEnabled,
     "icon box is" + (blockingEnabled ? "" : " not") + " visible");
  is(hidden("#tracking-action-block"), !blockingEnabled,
     "blockButton is" + (blockingEnabled ? " not" : "") + " visible");
  ok(hidden("#tracking-action-unblock"), "unblockButton is hidden");
  ok(!hidden("#identity-popup-content-blocking-disabled-label"), "disabled label is visible");

  ok(hidden("#identity-popup-content-blocking-not-detected"), "blocking not detected label is hidden");
  ok(!hidden("#identity-popup-content-blocking-detected"), "blocking detected label is visible");

  if (Services.prefs.getBoolPref(CB_UI_PREF)) {
    ok(!hidden("#identity-popup-content-blocking-category-list"), "category list is visible");
    let category;
    if (Services.prefs.getBoolPref(FB_PREF)) {
      category = "#identity-popup-content-blocking-category-fastblock";
    } else {
      category = Services.prefs.getIntPref(TPC_PREF) == Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER ?
                   "#identity-popup-content-blocking-category-3rdpartycookies" :
                   "#identity-popup-content-blocking-category-tracking-protection";
    }
    is(hidden(category + " > .identity-popup-content-blocking-category-add-blocking"), blockedByTP,
      "Category item is" + (blockedByTP ? " not" : "") + " showing add blocking");
    // Always hidden no matter if blockedByTP or not, since we have an exception.
    ok(hidden("#identity-popup-content-blocking-category-tracking-protection > .identity-popup-content-blocking-category-state-label"),
      "TP category item is not set to blocked");
  }
}

function testTrackingPageWithCBDisabled() {
  info("Tracking content must be white-listed and not blocked");
  ok(ContentBlocking.content.hasAttribute("detected"), "trackers are detected");
  ok(!ContentBlocking.content.hasAttribute("hasException"), "content shows no exception");
  ok(!ContentBlocking.content.hasAttribute("active"), "content is not active");

  ok(!ContentBlocking.iconBox.hasAttribute("active"), "shield is not active");
  ok(!ContentBlocking.iconBox.hasAttribute("hasException"), "shield shows no exception");
  ok(!ContentBlocking.iconBox.getAttribute("tooltiptext"), "icon box has no tooltip");

  ok(BrowserTestUtils.is_hidden(ContentBlocking.iconBox), "icon box is hidden");
  ok(hidden("#tracking-action-block"), "blockButton is hidden");
  ok(hidden("#tracking-action-unblock"), "unblockButton is hidden");
  ok(!hidden("#identity-popup-content-blocking-disabled-label"), "disabled label is visible");

  ok(hidden("#identity-popup-content-blocking-not-detected"), "blocking not detected label is hidden");
  ok(!hidden("#identity-popup-content-blocking-detected"), "blocking detected label is visible");

  if (Services.prefs.getBoolPref(CB_UI_PREF)) {
    ok(!hidden("#identity-popup-content-blocking-category-list"), "category list is visible");
    ok(!hidden("#identity-popup-content-blocking-category-tracking-protection > .identity-popup-content-blocking-category-add-blocking"),
      "TP category item is showing add blocking");
    ok(hidden("#identity-popup-content-blocking-category-tracking-protection > .identity-popup-content-blocking-category-state-label"),
      "TP category item is not set to blocked");
  }
}

async function testContentBlockingEnabled(tab) {
  info("Testing with Tracking Protection ENABLED.");

  info("Load a test page not containing tracking elements");
  await promiseTabLoadEvent(tab, BENIGN_PAGE);
  testBenignPage();

  info("Load a test page not containing tracking elements which has an exception.");
  let isPrivateBrowsing = PrivateBrowsingUtils.isWindowPrivate(tab.ownerGlobal);
  let uri = Services.io.newURI("https://example.org/");
  if (isPrivateBrowsing) {
    PrivateBrowsingUtils.addToTrackingAllowlist(uri);
  } else {
    Services.perms.add(uri, "trackingprotection", Services.perms.ALLOW_ACTION);
  }

  await promiseTabLoadEvent(tab, uri.spec);
  testBenignPageWithException();

  if (isPrivateBrowsing) {
    PrivateBrowsingUtils.removeFromTrackingAllowlist(uri);
  } else {
    Services.perms.remove(uri, "trackingprotection");
  }

  info("Load a test page containing tracking elements");
  await promiseTabLoadEvent(tab, gTrackingPageURL);
  testTrackingPage(tab.ownerGlobal);

  info("Disable CB for the page (which reloads the page)");
  let tabReloadPromise = promiseTabLoadEvent(tab);
  clickButton("#tracking-action-unblock");
  await tabReloadPromise;
  let blockedByTP = areTrackersBlocked(isPrivateBrowsing);
  testTrackingPageUnblocked(blockedByTP, tab.ownerGlobal);

  info("Re-enable TP for the page (which reloads the page)");
  tabReloadPromise = promiseTabLoadEvent(tab);
  clickButton("#tracking-action-block");
  await tabReloadPromise;
  testTrackingPage(tab.ownerGlobal);
}

async function testContentBlockingDisabled(tab) {
  info("Testing with Tracking Protection DISABLED.");

  info("Load a test page not containing tracking elements");
  await promiseTabLoadEvent(tab, BENIGN_PAGE);
  testBenignPage();

  info("Load a test page not containing tracking elements which has an exception.");
  let isPrivateBrowsing = PrivateBrowsingUtils.isWindowPrivate(tab.ownerGlobal);
  let uri = Services.io.newURI("https://example.org/");
  if (isPrivateBrowsing) {
    PrivateBrowsingUtils.addToTrackingAllowlist(uri);
  } else {
    Services.perms.add(uri, "trackingprotection", Services.perms.ALLOW_ACTION);
  }

  await promiseTabLoadEvent(tab, uri.spec);
  testBenignPageWithException();

  if (isPrivateBrowsing) {
    PrivateBrowsingUtils.removeFromTrackingAllowlist(uri);
  } else {
    Services.perms.remove(uri, "trackingprotection");
  }

  info("Load a test page containing tracking elements");
  await promiseTabLoadEvent(tab, gTrackingPageURL);
  testTrackingPageWithCBDisabled();
}

add_task(async function testNormalBrowsing() {
  await UrlClassifierTestUtils.addTestTrackers();

  tabbrowser = gBrowser;
  let tab = tabbrowser.selectedTab = BrowserTestUtils.addTab(tabbrowser);

  ContentBlocking = gBrowser.ownerGlobal.ContentBlocking;
  ok(ContentBlocking, "CB is attached to the browser window");
  TrackingProtection = gBrowser.ownerGlobal.TrackingProtection;
  ok(TrackingProtection, "TP is attached to the browser window");
  is(TrackingProtection.enabled, Services.prefs.getBoolPref(TP_PREF),
     "TP.enabled is based on the original pref value");

  Services.prefs.setBoolPref(FB_PREF, false);
  Services.prefs.setIntPref(TPC_PREF, Ci.nsICookieService.BEHAVIOR_ACCEPT);

  await testContentBlockingEnabled(tab);

  if (Services.prefs.getBoolPref(CB_UI_PREF)) {
    Services.prefs.setBoolPref(CB_PREF, false);
    ok(!ContentBlocking.enabled, "CB is disabled after setting the pref");
  } else {
    Services.prefs.setBoolPref(TP_PREF, false);
    ok(!TrackingProtection.enabled, "TP is disabled after setting the pref");
  }

  await testContentBlockingDisabled(tab);

  Services.prefs.setBoolPref(TP_PREF, true);
  ok(TrackingProtection.enabled, "TP is enabled after setting the pref");
  Services.prefs.setBoolPref(CB_PREF, true);
  ok(ContentBlocking.enabled, "CB is enabled after setting the pref");

  await testContentBlockingEnabled(tab);

  if (Services.prefs.getBoolPref(CB_UI_PREF)) {
    Services.prefs.setBoolPref(CB_PREF, false);
    ok(!ContentBlocking.enabled, "CB is disabled after setting the pref");
  } else {
    Services.prefs.setBoolPref(TP_PREF, false);
    ok(!TrackingProtection.enabled, "TP is disabled after setting the pref");
  }

  await testContentBlockingDisabled(tab);

  gBrowser.removeCurrentTab();

  Services.prefs.clearUserPref(FB_PREF);
  Services.prefs.clearUserPref(TPC_PREF);
});

add_task(async function testPrivateBrowsing() {
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({private: true});
  tabbrowser = privateWin.gBrowser;
  let tab = tabbrowser.selectedTab = BrowserTestUtils.addTab(tabbrowser);

  // Set the normal mode pref to false to check the pbmode pref.
  Services.prefs.setBoolPref(TP_PREF, false);

  Services.prefs.setBoolPref(FB_PREF, false);
  Services.prefs.setIntPref(TPC_PREF, Ci.nsICookieService.BEHAVIOR_ACCEPT);

  ContentBlocking = tabbrowser.ownerGlobal.ContentBlocking;
  ok(ContentBlocking, "CB is attached to the private window");
  TrackingProtection = tabbrowser.ownerGlobal.TrackingProtection;
  ok(TrackingProtection, "TP is attached to the private window");
  is(TrackingProtection.enabled, Services.prefs.getBoolPref(TP_PB_PREF),
     "TP.enabled is based on the pb pref value");

  await testContentBlockingEnabled(tab);

  if (Services.prefs.getBoolPref(CB_UI_PREF)) {
    Services.prefs.setBoolPref(CB_PREF, false);
    ok(!ContentBlocking.enabled, "CB is disabled after setting the pref");
  } else {
    Services.prefs.setBoolPref(TP_PB_PREF, false);
    ok(!TrackingProtection.enabled, "TP is disabled after setting the pref");
  }

  await testContentBlockingDisabled(tab);

  Services.prefs.setBoolPref(TP_PB_PREF, true);
  ok(TrackingProtection.enabled, "TP is enabled after setting the pref");
  Services.prefs.setBoolPref(CB_PREF, true);
  ok(TrackingProtection.enabled, "CB is enabled after setting the pref");

  await testContentBlockingEnabled(tab);

  if (Services.prefs.getBoolPref(CB_UI_PREF)) {
    Services.prefs.setBoolPref(CB_PREF, false);
    ok(!ContentBlocking.enabled, "CB is disabled after setting the pref");
  } else {
    Services.prefs.setBoolPref(TP_PB_PREF, false);
    ok(!TrackingProtection.enabled, "TP is disabled after setting the pref");
  }

  await testContentBlockingDisabled(tab);

  privateWin.close();

  Services.prefs.clearUserPref(FB_PREF);
  Services.prefs.clearUserPref(TPC_PREF);
});

add_task(async function testFastBlock() {
  if (!SpecialPowers.getBoolPref(CB_UI_PREF)) {
    info("The FastBlock test is disabled when the Content Blocking UI is disabled");
    return;
  }

  await UrlClassifierTestUtils.addTestTrackers();

  tabbrowser = gBrowser;
  let tab = tabbrowser.selectedTab = BrowserTestUtils.addTab(tabbrowser);

  Services.prefs.setBoolPref(FB_PREF, false);
  Services.prefs.setIntPref(TPC_PREF, Ci.nsICookieService.BEHAVIOR_ACCEPT);

  ContentBlocking = gBrowser.ownerGlobal.ContentBlocking;
  ok(ContentBlocking, "CB is attached to the browser window");
  FastBlock = gBrowser.ownerGlobal.FastBlock;
  ok(FastBlock, "TP is attached to the browser window");
  is(FastBlock.enabled, Services.prefs.getBoolPref(FB_PREF),
     "FB.enabled is based on the original pref value");
  Services.prefs.setBoolPref(CB_PREF, true);
  ok(ContentBlocking.enabled, "CB is enabled after setting the pref");

  await testContentBlockingEnabled(tab);

  ok(Services.prefs.getBoolPref(CB_UI_PREF), "CB UI must be enabled here");
  Services.prefs.setBoolPref(CB_PREF, false);
  ok(!ContentBlocking.enabled, "CB is disabled after setting the pref");

  await testContentBlockingDisabled(tab);

  Services.prefs.setBoolPref(FB_PREF, true);
  Services.prefs.setIntPref(FB_TIMEOUT_PREF, 0);
  Services.prefs.setIntPref(FB_LIMIT_PREF, 0);
  ok(FastBlock.enabled, "FB is enabled after setting the pref");
  Services.prefs.setBoolPref(CB_PREF, true);
  ok(ContentBlocking.enabled, "CB is enabled after setting the pref");

  await testContentBlockingEnabled(tab);

  ok(Services.prefs.getBoolPref(CB_UI_PREF), "CB UI must be enabled here");
  Services.prefs.setBoolPref(CB_PREF, false);
  ok(!ContentBlocking.enabled, "CB is disabled after setting the pref");

  await testContentBlockingDisabled(tab);

  Services.prefs.clearUserPref(FB_PREF);
  Services.prefs.clearUserPref(FB_TIMEOUT_PREF);
  Services.prefs.clearUserPref(FB_LIMIT_PREF);
  Services.prefs.clearUserPref(TPC_PREF);
  gBrowser.removeCurrentTab();
});

add_task(async function testThirdPartyCookies() {
  if (!SpecialPowers.getBoolPref(CB_UI_PREF)) {
    info("The ThirdPartyCookies test is disabled when the Content Blocking UI is disabled");
    return;
  }

  await UrlClassifierTestUtils.addTestTrackers();
  gTrackingPageURL = COOKIE_PAGE;

  Services.prefs.setBoolPref(FB_PREF, false);

  tabbrowser = gBrowser;
  let tab = tabbrowser.selectedTab = BrowserTestUtils.addTab(tabbrowser);

  ContentBlocking = gBrowser.ownerGlobal.ContentBlocking;
  ok(ContentBlocking, "CB is attached to the browser window");
  ThirdPartyCookies = gBrowser.ownerGlobal.ThirdPartyCookies;
  ok(ThirdPartyCookies, "TP is attached to the browser window");
  is(ThirdPartyCookies.enabled,
     Services.prefs.getIntPref(TPC_PREF) == Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
     "TPC.enabled is based on the original pref value");
  Services.prefs.setBoolPref(CB_PREF, true);
  ok(ContentBlocking.enabled, "CB is enabled after setting the pref");

  await testContentBlockingEnabled(tab);

  ok(Services.prefs.getBoolPref(CB_UI_PREF), "CB UI must be enabled here");
  Services.prefs.setBoolPref(CB_PREF, false);
  ok(!ContentBlocking.enabled, "CB is disabled after setting the pref");

  await testContentBlockingDisabled(tab);

  Services.prefs.setIntPref(TPC_PREF, Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER);
  ok(ThirdPartyCookies.enabled, "TPC is enabled after setting the pref");
  Services.prefs.setBoolPref(CB_PREF, true);
  ok(ContentBlocking.enabled, "CB is enabled after setting the pref");

  await testContentBlockingEnabled(tab);

  ok(Services.prefs.getBoolPref(CB_UI_PREF), "CB UI must be enabled here");
  Services.prefs.setBoolPref(CB_PREF, false);
  ok(!ContentBlocking.enabled, "CB is disabled after setting the pref");

  await testContentBlockingDisabled(tab);

  Services.prefs.clearUserPref(FB_PREF);
  Services.prefs.clearUserPref(TPC_PREF);
  gBrowser.removeCurrentTab();
});
