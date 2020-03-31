/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TRACKING_PAGE =
  "http://tracking.example.org/browser/browser/base/content/test/protectionsUI/trackingPage.html";
const BENIGN_PAGE =
  "http://tracking.example.org/browser/browser/base/content/test/protectionsUI/benignPage.html";

const TP_PREF = "privacy.trackingprotection.enabled";

add_task(async function testBackgroundTabs() {
  info(
    "Testing receiving and storing content blocking events in non-selected tabs."
  );

  await SpecialPowers.pushPrefEnv({
    set: [[TP_PREF, true]],
  });
  await UrlClassifierTestUtils.addTestTrackers();

  registerCleanupFunction(() => {
    UrlClassifierTestUtils.cleanupTestTrackers();
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, BENIGN_PAGE);

  let backgroundTab = BrowserTestUtils.addTab(gBrowser);
  let browser = backgroundTab.linkedBrowser;
  let hasContentBlockingEvent = TestUtils.waitForCondition(
    () => browser.getContentBlockingEvents() != 0
  );
  await promiseTabLoadEvent(backgroundTab, TRACKING_PAGE);
  await hasContentBlockingEvent;

  is(
    browser.getContentBlockingEvents(),
    Ci.nsIWebProgressListener.STATE_BLOCKED_TRACKING_CONTENT,
    "Background tab has the correct content blocking event."
  );

  is(
    tab.linkedBrowser.getContentBlockingEvents(),
    0,
    "Foreground tab has the correct content blocking event."
  );

  ok(
    !gProtectionsHandler.iconBox.hasAttribute("active"),
    "shield is not active"
  );

  await BrowserTestUtils.switchTab(gBrowser, backgroundTab);

  is(
    browser.getContentBlockingEvents(),
    Ci.nsIWebProgressListener.STATE_BLOCKED_TRACKING_CONTENT,
    "Background tab still has the correct content blocking event."
  );

  is(
    tab.linkedBrowser.getContentBlockingEvents(),
    0,
    "Foreground tab still has the correct content blocking event."
  );

  ok(gProtectionsHandler.iconBox.hasAttribute("active"), "shield is active");

  gBrowser.removeTab(backgroundTab);
  gBrowser.removeTab(tab);
});
