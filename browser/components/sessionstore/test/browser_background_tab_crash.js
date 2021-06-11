"use strict";

/**
 * These tests the behaviour of the browser when background tabs crash,
 * while the foreground tab remains.
 *
 * The current behavioural rule is this: if only background tabs crash,
 * then only the first tab shown of that group should show the tab crash
 * page, and subsequent ones should restore on demand.
 */

ChromeUtils.defineModuleGetter(
  this,
  "TelemetryTestUtils",
  "resource://testing-common/TelemetryTestUtils.jsm"
);

// Telemetry key
const TABUI_PRESENTED_KEY = "dom.contentprocess.crash_tab_ui_presented";

/**
 * Makes the current browser tab non-remote, and then sets up two remote
 * background tabs, ensuring that both belong to the same content process.
 * Callers should pass in a testing function that will execute (and possibly
 * yield Promises) taking the created background tabs as arguments. Once
 * the testing function completes, this function will take care of closing
 * the opened tabs.
 *
 * @param testFn (function)
 *        A Promise-generating function that will be called once the tabs
 *        are opened and ready.
 * @return Promise
 *        Resolves once the testing function completes and the opened tabs
 *        have been completely closed.
 */
async function setupBackgroundTabs(testFn) {
  const REMOTE_PAGE = "http://www.example.com";
  const NON_REMOTE_PAGE = "about:robots";

  // Browse the initial tab to a non-remote page, which we'll have in the
  // foreground.
  let initialTab = gBrowser.selectedTab;
  let initialBrowser = initialTab.linkedBrowser;
  BrowserTestUtils.loadURI(initialBrowser, NON_REMOTE_PAGE);
  await BrowserTestUtils.browserLoaded(initialBrowser);

  // Open some tabs that should be running in the content process.
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, REMOTE_PAGE);
  let remoteBrowser1 = tab1.linkedBrowser;
  await TabStateFlusher.flush(remoteBrowser1);

  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, REMOTE_PAGE);
  let remoteBrowser2 = tab2.linkedBrowser;
  await TabStateFlusher.flush(remoteBrowser2);

  // Quick sanity check - the two browsers should be remote and share the
  // same childID, or else this test is not going to work.
  Assert.ok(
    remoteBrowser1.isRemoteBrowser,
    "Browser should be remote in order to crash."
  );
  Assert.ok(
    remoteBrowser2.isRemoteBrowser,
    "Browser should be remote in order to crash."
  );
  Assert.equal(
    remoteBrowser1.frameLoader.childID,
    remoteBrowser2.frameLoader.childID,
    "Both remote browsers should share the same content process."
  );

  // Now switch back to the non-remote browser...
  await BrowserTestUtils.switchTab(gBrowser, initialTab);

  await testFn([tab1, tab2]);

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
}

/**
 * Takes some set of background tabs that are assumed to all belong to
 * the same content process, and crashes them.
 *
 * @param tabs (Array(<xul:tab>))
 *        The tabs to crash.
 * @return Promise
 *        Resolves once the tabs have crashed and entered the pending
 *        background state.
 */
async function crashBackgroundTabs(tabs) {
  Assert.ok(!!tabs.length, "Need to crash at least one tab.");
  for (let tab of tabs) {
    Assert.ok(tab.linkedBrowser.isRemoteBrowser, "tab is remote");
  }

  let remotenessChangePromises = tabs.map(t => {
    return BrowserTestUtils.waitForEvent(t, "TabRemotenessChange");
  });

  let tabsRevived = tabs.map(t => {
    return promiseTabRestoring(t);
  });

  await BrowserTestUtils.crashFrame(tabs[0].linkedBrowser, false);
  await Promise.all(remotenessChangePromises);
  await Promise.all(tabsRevived);

  // Both background tabs should now be in the pending restore
  // state.
  for (let tab of tabs) {
    Assert.ok(!tab.linkedBrowser.isRemoteBrowser, "tab is not remote");
    Assert.ok(!tab.linkedBrowser.hasAttribute("crashed"), "tab is not crashed");
    Assert.ok(tab.hasAttribute("pending"), "tab is pending");
  }
}

/**
 * Check that the telemetry scalar for TABUI_PRESENTED_KEY has
 * the expected value.
 *
 * @param expectedCount the expected value
 * @param desc test description to output to the log
 */
function checkTelemetry(expectedCount, desc) {
  const scalars = TelemetryTestUtils.getProcessScalars("parent");
  // This telemetry adds up from 0, so expected 0 results in unset scalar.
  if (expectedCount === 0) {
    TelemetryTestUtils.assertScalarUnset(scalars, TABUI_PRESENTED_KEY);
    return;
  }

  TelemetryTestUtils.assertScalar(
    scalars,
    TABUI_PRESENTED_KEY,
    expectedCount,
    desc + " telemetry"
  );
}

add_task(async function setup() {
  // We'll simplify by making sure we only ever one content process for this
  // test.
  await SpecialPowers.pushPrefEnv({ set: [["dom.ipc.processCount", 1]] });

  // On debug builds, crashing tabs results in much thinking, which
  // slows down the test and results in intermittent test timeouts,
  // so we'll pump up the expected timeout for this test.
  requestLongerTimeout(5);
});

/**
 * Tests that if a content process crashes taking down only
 * background tabs, then the first of those tabs that the user
 * selects will show the tab crash page, but the rest will restore
 * on demand.
 */
add_task(async function test_background_crash_simple() {
  Services.telemetry.clearScalars();

  await setupBackgroundTabs(async function([tab1, tab2]) {
    // Let's crash one of those background tabs now...
    await crashBackgroundTabs([tab1, tab2]);

    checkTelemetry(0, "simple crash initial value");

    // Selecting the first tab should now send it to the tab crashed page.
    let tabCrashedPagePromise = BrowserTestUtils.waitForContentEvent(
      tab1.linkedBrowser,
      "AboutTabCrashedReady",
      false,
      null,
      true
    );
    await BrowserTestUtils.switchTab(gBrowser, tab1);
    await tabCrashedPagePromise;

    checkTelemetry(1, "simple crash after tab switch");

    // Selecting the second tab should restore it.
    let tabRestored = promiseTabRestored(tab2);
    await BrowserTestUtils.switchTab(gBrowser, tab2);
    await tabRestored;

    checkTelemetry(1, "simple crash after tab switch");
  });
});

/**
 * Tests that if a content process crashes taking down only
 * background tabs, and the user is configured to send backlogged
 * crash reports automatically, that the tab crashed page is not
 * shown.
 */
add_task(async function test_background_crash_autosubmit_backlogged() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.crashReports.unsubmittedCheck.autoSubmit2", true]],
  });

  Services.telemetry.clearScalars();

  await setupBackgroundTabs(async function([tab1, tab2]) {
    // Let's crash one of those background tabs now...
    await crashBackgroundTabs([tab1, tab2]);

    // Selecting the first tab should restore it.
    let tabRestored = promiseTabRestored(tab1);
    await BrowserTestUtils.switchTab(gBrowser, tab1);
    await tabRestored;

    // Selecting the second tab should restore it.
    tabRestored = promiseTabRestored(tab2);
    await BrowserTestUtils.switchTab(gBrowser, tab2);
    await tabRestored;
  });

  // No telemetry when the crash page is not shown.
  checkTelemetry(0, "crash with autosubmit");

  await SpecialPowers.popPrefEnv();
});

/**
 * Tests that if there are two background tab crashes in a row, that
 * the two sets of background crashes don't interfere with one another.
 *
 * Specifically, if we start with two background tabs (1, 2) which crash,
 * and we visit 1, 1 should go to the tab crashed page. If we then have
 * two new background tabs (3, 4) crash, visiting 2 should still restore.
 * Visiting 4 should show us the tab crashed page, and then visiting 3
 * should restore.
 */
add_task(async function test_background_crash_multiple() {
  Services.telemetry.clearScalars();

  let initialTab = gBrowser.selectedTab;

  await setupBackgroundTabs(async function([tab1, tab2]) {
    // Let's crash one of those background tabs now...
    await crashBackgroundTabs([tab1, tab2]);

    // Selecting the first tab should now send it to the tab crashed page.
    let tabCrashedPagePromise = BrowserTestUtils.waitForContentEvent(
      tab1.linkedBrowser,
      "AboutTabCrashedReady",
      false,
      null,
      true
    );
    await BrowserTestUtils.switchTab(gBrowser, tab1);
    await tabCrashedPagePromise;

    checkTelemetry(1, "multiple crash after first tab select");

    // Now switch back to the original non-remote tab...
    await BrowserTestUtils.switchTab(gBrowser, initialTab);

    checkTelemetry(1, "multiple crash after original tab select");

    await setupBackgroundTabs(async function([tab3, tab4]) {
      await crashBackgroundTabs([tab3, tab4]);

      checkTelemetry(1, "multiple crash after second crash");

      // Selecting the second tab should restore it.
      let tabRestored = promiseTabRestored(tab2);
      await BrowserTestUtils.switchTab(gBrowser, tab2);
      await tabRestored;

      checkTelemetry(1, "multiple crash after second crash and tab select");

      // Selecting the fourth tab should now send it to the tab crashed page.
      tabCrashedPagePromise = BrowserTestUtils.waitForContentEvent(
        tab4.linkedBrowser,
        "AboutTabCrashedReady",
        false,
        null,
        true
      );
      await BrowserTestUtils.switchTab(gBrowser, tab4);
      await tabCrashedPagePromise;

      checkTelemetry(
        2,
        "multiple crash after second crash and third tab select"
      );

      // Selecting the third tab should restore it.
      tabRestored = promiseTabRestored(tab3);
      await BrowserTestUtils.switchTab(gBrowser, tab3);
      await tabRestored;

      checkTelemetry(
        2,
        "multiple crash after second crash and fourth tab select"
      );
    });
  });
});

// Tests that crashed preloaded tabs are removed and no unexpected errors are
// thrown.
add_task(async function test_preload_crash() {
  Services.telemetry.clearScalars();

  if (!Services.prefs.getBoolPref("browser.newtab.preload")) {
    return;
  }

  // Release any existing preloaded browser
  NewTabPagePreloading.removePreloadedBrowser(window);

  checkTelemetry(0, "preloaded close");

  // Create a fresh preloaded browser
  await BrowserTestUtils.maybeCreatePreloadedBrowser(gBrowser);

  await BrowserTestUtils.crashFrame(gBrowser.preloadedBrowser, false);

  checkTelemetry(0, "preloaded close after crash");

  Assert.ok(!gBrowser.preloadedBrowser);
});
