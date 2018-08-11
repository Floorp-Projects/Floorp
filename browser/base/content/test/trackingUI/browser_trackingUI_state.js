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
const TP_PREF = "privacy.trackingprotection.enabled";
const TP_PB_PREF = "privacy.trackingprotection.enabled";
const BENIGN_PAGE = "http://tracking.example.org/browser/browser/base/content/test/trackingUI/benignPage.html";
const TRACKING_PAGE = "http://tracking.example.org/browser/browser/base/content/test/trackingUI/trackingPage.html";
var ContentBlocking = null;
var TrackingProtection = null;
var tabbrowser = null;

registerCleanupFunction(function() {
  TrackingProtection = ContentBlocking = tabbrowser = null;
  UrlClassifierTestUtils.cleanupTestTrackers();
  Services.prefs.clearUserPref(TP_PREF);
  Services.prefs.clearUserPref(TP_PB_PREF);
  Services.prefs.clearUserPref(CB_PREF);
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

  ok(hidden("#identity-popup-content-blocking-category-list"), "category list is hidden");
}

function testBenignPageWithException() {
  info("Non-tracking content must not be blocked");
  ok(!ContentBlocking.content.hasAttribute("detected"), "no trackers are detected");
  ok(ContentBlocking.content.hasAttribute("hasException"), "content shows exception");

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

  ok(hidden("#identity-popup-content-blocking-category-list"), "category list is hidden");
}

function testTrackingPage(window) {
  info("Tracking content must be blocked");
  ok(ContentBlocking.content.hasAttribute("detected"), "trackers are detected");
  ok(!ContentBlocking.content.hasAttribute("hasException"), "content shows no exception");

  ok(BrowserTestUtils.is_visible(ContentBlocking.iconBox), "icon box is visible");
  ok(ContentBlocking.iconBox.hasAttribute("active"), "shield is active");
  ok(!ContentBlocking.iconBox.hasAttribute("hasException"), "icon box shows no exception");
  is(ContentBlocking.iconBox.getAttribute("tooltiptext"),
     gNavigatorBundle.getString("trackingProtection.icon.activeTooltip"), "correct tooltip");

  ok(hidden("#tracking-action-block"), "blockButton is hidden");

  if (PrivateBrowsingUtils.isWindowPrivate(window)) {
    ok(hidden("#tracking-action-unblock"), "unblockButton is hidden");
    ok(!hidden("#tracking-action-unblock-private"), "unblockButtonPrivate is visible");
  } else {
    ok(!hidden("#tracking-action-unblock"), "unblockButton is visible");
    ok(hidden("#tracking-action-unblock-private"), "unblockButtonPrivate is hidden");
  }

  ok(hidden("#identity-popup-content-blocking-not-detected"), "blocking not detected label is hidden");
  ok(!hidden("#identity-popup-content-blocking-detected"), "blocking detected label is visible");

  ok(!hidden("#identity-popup-content-blocking-category-list"), "category list is visible");
  ok(hidden("#identity-popup-content-blocking-category-tracking-protection > .identity-popup-content-blocking-category-add-blocking"),
    "TP category item is not showing add blocking");
  ok(!hidden("#identity-popup-content-blocking-category-tracking-protection > .identity-popup-content-blocking-category-state-label"),
    "TP category item is set to blocked");
}

function testTrackingPageUnblocked() {
  info("Tracking content must be white-listed and not blocked");
  ok(ContentBlocking.content.hasAttribute("detected"), "trackers are detected");
  ok(ContentBlocking.content.hasAttribute("hasException"), "content shows exception");

  ok(ContentBlocking.iconBox.hasAttribute("active"), "shield is active");
  ok(ContentBlocking.iconBox.hasAttribute("hasException"), "shield shows exception");
  is(ContentBlocking.iconBox.getAttribute("tooltiptext"),
     gNavigatorBundle.getString("trackingProtection.icon.disabledTooltip"), "correct tooltip");

  ok(BrowserTestUtils.is_visible(ContentBlocking.iconBox), "icon box is visible");
  ok(!hidden("#tracking-action-block"), "blockButton is visible");
  ok(hidden("#tracking-action-unblock"), "unblockButton is hidden");
  ok(!hidden("#identity-popup-content-blocking-disabled-label"), "disabled label is visible");

  ok(hidden("#identity-popup-content-blocking-not-detected"), "blocking not detected label is hidden");
  ok(!hidden("#identity-popup-content-blocking-detected"), "blocking detected label is visible");

  ok(!hidden("#identity-popup-content-blocking-category-list"), "category list is visible");
  ok(hidden("#identity-popup-content-blocking-category-tracking-protection > .identity-popup-content-blocking-category-add-blocking"),
    "TP category item is not showing add blocking");
  ok(hidden("#identity-popup-content-blocking-category-tracking-protection > .identity-popup-content-blocking-category-state-label"),
    "TP category item is not set to blocked");
}

function testTrackingPageWithCBDisabled() {
  info("Tracking content must be white-listed and not blocked");
  ok(ContentBlocking.content.hasAttribute("detected"), "trackers are detected");
  ok(!ContentBlocking.content.hasAttribute("hasException"), "content shows no exception");

  ok(!ContentBlocking.iconBox.hasAttribute("active"), "shield is not active");
  ok(!ContentBlocking.iconBox.hasAttribute("hasException"), "shield shows no exception");
  ok(!ContentBlocking.iconBox.getAttribute("tooltiptext"), "icon box has no tooltip");

  ok(BrowserTestUtils.is_hidden(ContentBlocking.iconBox), "icon box is hidden");
  ok(hidden("#tracking-action-block"), "blockButton is hidden");
  ok(hidden("#tracking-action-unblock"), "unblockButton is hidden");
  ok(!hidden("#identity-popup-content-blocking-disabled-label"), "disabled label is visible");

  ok(hidden("#identity-popup-content-blocking-not-detected"), "blocking not detected label is hidden");
  ok(!hidden("#identity-popup-content-blocking-detected"), "blocking detected label is visible");

  ok(!hidden("#identity-popup-content-blocking-category-list"), "category list is visible");
  ok(!hidden("#identity-popup-content-blocking-category-tracking-protection > .identity-popup-content-blocking-category-add-blocking"),
    "TP category item is showing add blocking");
  ok(hidden("#identity-popup-content-blocking-category-tracking-protection > .identity-popup-content-blocking-category-state-label"),
    "TP category item is not set to blocked");
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
  await promiseTabLoadEvent(tab, TRACKING_PAGE);
  testTrackingPage(tab.ownerGlobal);

  info("Disable CB for the page (which reloads the page)");
  let tabReloadPromise = promiseTabLoadEvent(tab);
  clickButton("#tracking-action-unblock");
  await tabReloadPromise;
  testTrackingPageUnblocked();

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
  await promiseTabLoadEvent(tab, TRACKING_PAGE);
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

  Services.prefs.setBoolPref(TP_PREF, true);
  ok(TrackingProtection.enabled, "TP is enabled after setting the pref");
  Services.prefs.setBoolPref(CB_PREF, true);
  ok(ContentBlocking.enabled, "CB is enabled after setting the pref");

  await testContentBlockingEnabled(tab);

  Services.prefs.setBoolPref(CB_PREF, false);
  ok(!ContentBlocking.enabled, "CB is disabled after setting the pref");

  await testContentBlockingDisabled(tab);

  gBrowser.removeCurrentTab();
});

add_task(async function testPrivateBrowsing() {
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({private: true});
  tabbrowser = privateWin.gBrowser;
  let tab = tabbrowser.selectedTab = BrowserTestUtils.addTab(tabbrowser);

  ContentBlocking = tabbrowser.ownerGlobal.ContentBlocking;
  ok(ContentBlocking, "CB is attached to the private window");
  TrackingProtection = tabbrowser.ownerGlobal.TrackingProtection;
  ok(TrackingProtection, "TP is attached to the private window");
  is(TrackingProtection.enabled, Services.prefs.getBoolPref(TP_PB_PREF),
     "TP.enabled is based on the pb pref value");

  Services.prefs.setBoolPref(TP_PB_PREF, true);
  ok(TrackingProtection.enabled, "TP is enabled after setting the pref");
  Services.prefs.setBoolPref(CB_PREF, true);
  ok(TrackingProtection.enabled, "CB is enabled after setting the pref");

  await testContentBlockingEnabled(tab);

  Services.prefs.setBoolPref(CB_PREF, false);
  ok(!ContentBlocking.enabled, "CB is disabled after setting the pref");

  await testContentBlockingDisabled(tab);

  privateWin.close();
});
