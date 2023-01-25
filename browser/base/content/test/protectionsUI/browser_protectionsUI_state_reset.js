/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TP_PREF = "privacy.trackingprotection.enabled";
const TRACKING_PAGE =
  "http://tracking.example.org/browser/browser/base/content/test/protectionsUI/trackingPage.html";
const BENIGN_PAGE =
  "http://tracking.example.org/browser/browser/base/content/test/protectionsUI/benignPage.html";
const ABOUT_PAGE = "about:preferences";

/* This asserts that the content blocking event state is correctly reset
 * when navigating to a new location, and that the user is correctly
 * reset when switching between tabs. */

add_task(async function testResetOnLocationChange() {
  Services.prefs.setBoolPref(TP_PREF, true);

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, BENIGN_PAGE);
  let browser = tab.linkedBrowser;

  is(
    browser.getContentBlockingEvents(),
    0,
    "Benign page has no content blocking event"
  );
  ok(
    !gProtectionsHandler.iconBox.hasAttribute("active"),
    "shield is not active"
  );

  await Promise.all([
    promiseTabLoadEvent(tab, TRACKING_PAGE),
    waitForContentBlockingEvent(2),
  ]);

  is(
    browser.getContentBlockingEvents(),
    Ci.nsIWebProgressListener.STATE_BLOCKED_TRACKING_CONTENT,
    "Tracking page has a content blocking event"
  );
  ok(gProtectionsHandler.iconBox.hasAttribute("active"), "shield is active");

  await promiseTabLoadEvent(tab, BENIGN_PAGE);

  is(
    browser.getContentBlockingEvents(),
    0,
    "Benign page has no content blocking event"
  );
  ok(
    !gProtectionsHandler.iconBox.hasAttribute("active"),
    "shield is not active"
  );

  let contentBlockingEvent = waitForContentBlockingEvent(3);
  let trackingTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TRACKING_PAGE
  );
  await contentBlockingEvent;

  is(
    trackingTab.linkedBrowser.getContentBlockingEvents(),
    Ci.nsIWebProgressListener.STATE_BLOCKED_TRACKING_CONTENT,
    "Tracking page has a content blocking event"
  );
  ok(gProtectionsHandler.iconBox.hasAttribute("active"), "shield is active");

  gBrowser.selectedTab = tab;
  is(
    browser.getContentBlockingEvents(),
    0,
    "Benign page has no content blocking event"
  );
  ok(
    !gProtectionsHandler.iconBox.hasAttribute("active"),
    "shield is not active"
  );

  gBrowser.removeTab(trackingTab);
  gBrowser.removeTab(tab);

  Services.prefs.clearUserPref(TP_PREF);
});

/* Test that the content blocking icon is correctly reset
 * when changing tabs or navigating to an about: page */
add_task(async function testResetOnTabChange() {
  Services.prefs.setBoolPref(TP_PREF, true);

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, ABOUT_PAGE);
  ok(
    !gProtectionsHandler.iconBox.hasAttribute("active"),
    "shield is not active"
  );

  await Promise.all([
    promiseTabLoadEvent(tab, TRACKING_PAGE),
    waitForContentBlockingEvent(3),
  ]);
  ok(gProtectionsHandler.iconBox.hasAttribute("active"), "shield is active");

  await promiseTabLoadEvent(tab, ABOUT_PAGE);
  ok(
    !gProtectionsHandler.iconBox.hasAttribute("active"),
    "shield is not active"
  );

  let contentBlockingEvent = waitForContentBlockingEvent(3);
  let trackingTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TRACKING_PAGE
  );
  await contentBlockingEvent;
  ok(gProtectionsHandler.iconBox.hasAttribute("active"), "shield is active");

  gBrowser.selectedTab = tab;
  ok(
    !gProtectionsHandler.iconBox.hasAttribute("active"),
    "shield is not active"
  );

  gBrowser.removeTab(trackingTab);
  gBrowser.removeTab(tab);

  Services.prefs.clearUserPref(TP_PREF);
});
