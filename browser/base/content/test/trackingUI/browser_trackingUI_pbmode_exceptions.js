/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that sites added to the Tracking Protection whitelist in private
// browsing mode don't persist once the private browsing window closes.

const CB_PREF = "browser.contentblocking.enabled";
const TP_PB_PREF = "privacy.trackingprotection.enabled";
const TRACKING_PAGE = "http://tracking.example.org/browser/browser/base/content/test/trackingUI/trackingPage.html";
var TrackingProtection = null;
var ContentBlocking = null;
var browser = null;

registerCleanupFunction(function() {
  Services.prefs.clearUserPref(TP_PB_PREF);
  Services.prefs.clearUserPref(CB_PREF);
  ContentBlocking = TrackingProtection = browser = null;
  UrlClassifierTestUtils.cleanupTestTrackers();
});

function hidden(sel) {
  let win = browser.ownerGlobal;
  let el = win.document.querySelector(sel);
  let display = win.getComputedStyle(el).getPropertyValue("display", null);
  return display === "none";
}

function identityPopupState() {
  let win = browser.ownerGlobal;
  return win.document.getElementById("identity-popup").state;
}

function clickButton(sel) {
  let win = browser.ownerGlobal;
  let el = win.document.querySelector(sel);
  el.doCommand();
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

add_task(async function testExceptionAddition() {
  await UrlClassifierTestUtils.addTestTrackers();
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({private: true});
  browser = privateWin.gBrowser;
  let tab = await BrowserTestUtils.openNewForegroundTab({ gBrowser: browser, waitForLoad: true, waitForStateStop: true });

  ContentBlocking = browser.ownerGlobal.ContentBlocking;
  ok(ContentBlocking, "CB is attached to the private window");
  TrackingProtection = browser.ownerGlobal.TrackingProtection;
  ok(TrackingProtection, "TP is attached to the private window");

  Services.prefs.setBoolPref(TP_PB_PREF, true);
  ok(TrackingProtection.enabled, "TP is enabled after setting the pref");
  Services.prefs.setBoolPref(CB_PREF, true);
  ok(TrackingProtection.enabled, "CB is enabled after setting the pref");

  info("Load a test page containing tracking elements");
  await promiseTabLoadEvent(tab, TRACKING_PAGE);

  testTrackingPage(tab.ownerGlobal);

  info("Disable TP for the page (which reloads the page)");
  let tabReloadPromise = promiseTabLoadEvent(tab);
  clickButton("#tracking-action-unblock");
  is(identityPopupState(), "closed", "Identity popup is closed");

  await tabReloadPromise;
  testTrackingPageUnblocked();

  info("Test that the exception is remembered across tabs in the same private window");
  tab = browser.selectedTab = BrowserTestUtils.addTab(browser);

  info("Load a test page containing tracking elements");
  await promiseTabLoadEvent(tab, TRACKING_PAGE);
  testTrackingPageUnblocked();

  privateWin.close();
});

add_task(async function testExceptionPersistence() {
  info("Open another private browsing window");
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({private: true});
  browser = privateWin.gBrowser;
  let tab = await BrowserTestUtils.openNewForegroundTab({ gBrowser: browser, waitForLoad: true, waitForStateStop: true });

  ContentBlocking = browser.ownerGlobal.ContentBlocking;
  ok(ContentBlocking, "CB is attached to the private window");
  TrackingProtection = browser.ownerGlobal.TrackingProtection;
  ok(TrackingProtection, "TP is attached to the private window");

  ok(ContentBlocking.enabled, "CB is still enabled");
  ok(TrackingProtection.enabled, "TP is still enabled");

  info("Load a test page containing tracking elements");
  await promiseTabLoadEvent(tab, TRACKING_PAGE);

  testTrackingPage(tab.ownerGlobal);

  info("Disable TP for the page (which reloads the page)");
  let tabReloadPromise = promiseTabLoadEvent(tab);
  clickButton("#tracking-action-unblock");
  is(identityPopupState(), "closed", "Identity popup is closed");

  await tabReloadPromise;
  testTrackingPageUnblocked();

  privateWin.close();
});
