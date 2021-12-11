/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that sites added to the Tracking Protection whitelist in private
// browsing mode don't persist once the private browsing window closes.

const TP_PB_PREF = "privacy.trackingprotection.enabled";
const TRACKING_PAGE =
  "http://tracking.example.org/browser/browser/base/content/test/protectionsUI/trackingPage.html";
const DTSCBN_PREF = "dom.testing.sync-content-blocking-notifications";
var TrackingProtection = null;
var gProtectionsHandler = null;
var browser = null;

registerCleanupFunction(function() {
  Services.prefs.clearUserPref(TP_PB_PREF);
  Services.prefs.clearUserPref(DTSCBN_PREF);
  gProtectionsHandler = TrackingProtection = browser = null;
  UrlClassifierTestUtils.cleanupTestTrackers();
});

function hidden(sel) {
  let win = browser.ownerGlobal;
  let el = win.document.querySelector(sel);
  let display = win.getComputedStyle(el).getPropertyValue("display", null);
  return display === "none";
}

function protectionsPopupState() {
  let win = browser.ownerGlobal;
  return win.document.getElementById("protections-popup")?.state || "closed";
}

function clickButton(sel) {
  let win = browser.ownerGlobal;
  let el = win.document.querySelector(sel);
  el.doCommand();
}

function testTrackingPage() {
  info("Tracking content must be blocked");
  ok(gProtectionsHandler.anyDetected, "trackers are detected");
  ok(!gProtectionsHandler.hasException, "content shows no exception");

  ok(
    BrowserTestUtils.is_visible(gProtectionsHandler.iconBox),
    "icon box is visible"
  );
  ok(gProtectionsHandler.iconBox.hasAttribute("active"), "shield is active");
  ok(
    !gProtectionsHandler.iconBox.hasAttribute("hasException"),
    "icon box shows no exception"
  );
  is(
    gProtectionsHandler._trackingProtectionIconTooltipLabel.textContent,
    gNavigatorBundle.getString("trackingProtection.icon.activeTooltip2"),
    "correct tooltip"
  );
}

function testTrackingPageUnblocked() {
  info("Tracking content must be allowlisted and not blocked");
  ok(gProtectionsHandler.anyDetected, "trackers are detected");
  ok(gProtectionsHandler.hasException, "content shows exception");

  ok(!gProtectionsHandler.iconBox.hasAttribute("active"), "shield is active");
  ok(
    gProtectionsHandler.iconBox.hasAttribute("hasException"),
    "shield shows exception"
  );
  is(
    gProtectionsHandler._trackingProtectionIconTooltipLabel.textContent,
    gNavigatorBundle.getString("trackingProtection.icon.disabledTooltip2"),
    "correct tooltip"
  );

  ok(
    BrowserTestUtils.is_visible(gProtectionsHandler.iconBox),
    "icon box is visible"
  );
}

add_task(async function testExceptionAddition() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_first_pbm", false]],
  });

  await UrlClassifierTestUtils.addTestTrackers();
  Services.prefs.setBoolPref(DTSCBN_PREF, true);
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  browser = privateWin.gBrowser;
  let tab = await BrowserTestUtils.openNewForegroundTab(browser);

  gProtectionsHandler = browser.ownerGlobal.gProtectionsHandler;
  ok(gProtectionsHandler, "CB is attached to the private window");

  TrackingProtection =
    browser.ownerGlobal.gProtectionsHandler.blockers.TrackingProtection;
  ok(TrackingProtection, "TP is attached to the private window");

  Services.prefs.setBoolPref(TP_PB_PREF, true);
  ok(TrackingProtection.enabled, "TP is enabled after setting the pref");

  info("Load a test page containing tracking elements");
  await Promise.all([
    promiseTabLoadEvent(tab, TRACKING_PAGE),
    waitForContentBlockingEvent(2, tab.ownerGlobal),
  ]);

  testTrackingPage(tab.ownerGlobal);

  info("Disable TP for the page (which reloads the page)");
  let tabReloadPromise = promiseTabLoadEvent(tab);
  gProtectionsHandler.disableForCurrentPage();
  is(protectionsPopupState(), "closed", "protections popup is closed");

  await tabReloadPromise;
  testTrackingPageUnblocked();

  info(
    "Test that the exception is remembered across tabs in the same private window"
  );
  tab = browser.selectedTab = BrowserTestUtils.addTab(browser);

  info("Load a test page containing tracking elements");
  await promiseTabLoadEvent(tab, TRACKING_PAGE);
  testTrackingPageUnblocked();

  await BrowserTestUtils.closeWindow(privateWin);
});

add_task(async function testExceptionPersistence() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_first_pbm", false]],
  });

  info("Open another private browsing window");
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  browser = privateWin.gBrowser;
  let tab = await BrowserTestUtils.openNewForegroundTab(browser);

  gProtectionsHandler = browser.ownerGlobal.gProtectionsHandler;
  ok(gProtectionsHandler, "CB is attached to the private window");
  TrackingProtection =
    browser.ownerGlobal.gProtectionsHandler.blockers.TrackingProtection;
  ok(TrackingProtection, "TP is attached to the private window");

  ok(TrackingProtection.enabled, "TP is still enabled");

  info("Load a test page containing tracking elements");
  await Promise.all([
    promiseTabLoadEvent(tab, TRACKING_PAGE),
    waitForContentBlockingEvent(2, tab.ownerGlobal),
  ]);

  testTrackingPage(tab.ownerGlobal);

  info("Disable TP for the page (which reloads the page)");
  let tabReloadPromise = promiseTabLoadEvent(tab);
  gProtectionsHandler.disableForCurrentPage();
  is(protectionsPopupState(), "closed", "protections popup is closed");

  await Promise.all([
    tabReloadPromise,
    waitForContentBlockingEvent(2, tab.ownerGlobal),
  ]);
  testTrackingPageUnblocked();

  await BrowserTestUtils.closeWindow(privateWin);
});
