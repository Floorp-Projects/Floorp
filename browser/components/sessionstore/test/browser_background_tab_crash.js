"use strict";

/**
 * These tests the behaviour of the browser when background tabs crash,
 * while the foreground tab remains.
 *
 * The current behavioural rule is this: if only background tabs crash,
 * then only the first tab shown of that group should show the tab crash
 * page, and subsequent ones should restore on demand.
 */

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
function* setupBackgroundTabs(testFn) {
  const REMOTE_PAGE = "http://www.example.com";
  const NON_REMOTE_PAGE = "about:robots";

  // Browse the initial tab to a non-remote page, which we'll have in the
  // foreground.
  let initialTab = gBrowser.selectedTab;
  let initialBrowser = initialTab.linkedBrowser;
  initialBrowser.loadURI(NON_REMOTE_PAGE);
  yield BrowserTestUtils.browserLoaded(initialBrowser);

  // Open some tabs that should be running in the content process.
  let tab1 =
    yield BrowserTestUtils.openNewForegroundTab(gBrowser, REMOTE_PAGE);
  let remoteBrowser1 = tab1.linkedBrowser;
  yield TabStateFlusher.flush(remoteBrowser1);

  let tab2 =
    yield BrowserTestUtils.openNewForegroundTab(gBrowser, REMOTE_PAGE);
  let remoteBrowser2 = tab2.linkedBrowser;
  yield TabStateFlusher.flush(remoteBrowser2);

  // Quick sanity check - the two browsers should be remote and share the
  // same childID, or else this test is not going to work.
  Assert.ok(remoteBrowser1.isRemoteBrowser,
            "Browser should be remote in order to crash.");
  Assert.ok(remoteBrowser2.isRemoteBrowser,
            "Browser should be remote in order to crash.");
  Assert.equal(remoteBrowser1.frameLoader.childID,
               remoteBrowser2.frameLoader.childID,
               "Both remote browsers should share the same content process.");

  // Now switch back to the non-remote browser...
  yield BrowserTestUtils.switchTab(gBrowser, initialTab);

  yield testFn([tab1, tab2]);

  yield BrowserTestUtils.removeTab(tab1);
  yield BrowserTestUtils.removeTab(tab2);
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
function* crashBackgroundTabs(tabs) {
  Assert.ok(tabs.length > 0, "Need to crash at least one tab.");
  for (let tab of tabs) {
    Assert.ok(tab.linkedBrowser.isRemoteBrowser, "tab is remote");
  }

  let remotenessChangePromises = tabs.map((t) => {
    return BrowserTestUtils.waitForEvent(t, "TabRemotenessChange");
  });

  let tabsRevived = tabs.map((t) => {
    return promiseTabRestoring(t);
  });

  yield BrowserTestUtils.crashBrowser(tabs[0].linkedBrowser, false);
  yield Promise.all(remotenessChangePromises);
  yield Promise.all(tabsRevived);

  // Both background tabs should now be in the pending restore
  // state.
  for (let tab of tabs) {
    Assert.ok(!tab.linkedBrowser.isRemoteBrowser, "tab is not remote");
    Assert.ok(!tab.linkedBrowser.hasAttribute("crashed"), "tab is not crashed");
    Assert.ok(tab.linkedBrowser.hasAttribute("pending"), "tab is pending");
  }
}

add_task(function* setup() {
  // We'll simplify by making sure we only ever one content process for this
  // test.
  yield SpecialPowers.pushPrefEnv({ set: [[ "dom.ipc.processCount", 1 ]] });

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
add_task(function* test_background_crash_simple() {
  yield setupBackgroundTabs(function*([tab1, tab2]) {
    // Let's crash one of those background tabs now...
    yield crashBackgroundTabs([tab1, tab2]);

    // Selecting the first tab should now send it to the tab crashed page.
    let tabCrashedPagePromise =
      BrowserTestUtils.waitForContentEvent(tab1.linkedBrowser,
                                           "AboutTabCrashedReady",
                                           false, null, true);
    yield BrowserTestUtils.switchTab(gBrowser, tab1);
    yield tabCrashedPagePromise;

    // Selecting the second tab should restore it.
    let tabRestored = promiseTabRestored(tab2);
    yield BrowserTestUtils.switchTab(gBrowser, tab2);
    yield tabRestored;
  });
});

/**
 * Tests that if a content process crashes taking down only
 * background tabs, and the user is configured to send backlogged
 * crash reports automatically, that the tab crashed page is not
 * shown.
 */
add_task(function* test_background_crash_autosubmit_backlogged() {
  yield SpecialPowers.pushPrefEnv({
    set: [["browser.crashReports.unsubmittedCheck.autoSubmit", true]],
  });

  yield setupBackgroundTabs(function*([tab1, tab2]) {
    // Let's crash one of those background tabs now...
    yield crashBackgroundTabs([tab1, tab2]);

    // Selecting the first tab should restore it.
    let tabRestored = promiseTabRestored(tab1);
    yield BrowserTestUtils.switchTab(gBrowser, tab1);
    yield tabRestored;

    // Selecting the second tab should restore it.
    tabRestored = promiseTabRestored(tab2);
    yield BrowserTestUtils.switchTab(gBrowser, tab2);
    yield tabRestored;
  });

  yield SpecialPowers.popPrefEnv();
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
add_task(function* test_background_crash_multiple() {
  let initialTab = gBrowser.selectedTab;

  yield setupBackgroundTabs(function*([tab1, tab2]) {
    // Let's crash one of those background tabs now...
    yield crashBackgroundTabs([tab1, tab2]);

    // Selecting the first tab should now send it to the tab crashed page.
    let tabCrashedPagePromise =
      BrowserTestUtils.waitForContentEvent(tab1.linkedBrowser,
                                           "AboutTabCrashedReady",
                                           false, null, true);
    yield BrowserTestUtils.switchTab(gBrowser, tab1);
    yield tabCrashedPagePromise;

    // Now switch back to the original non-remote tab...
    yield BrowserTestUtils.switchTab(gBrowser, initialTab);

    yield setupBackgroundTabs(function*([tab3, tab4]) {
      yield crashBackgroundTabs([tab3, tab4]);

      // Selecting the second tab should restore it.
      let tabRestored = promiseTabRestored(tab2);
      yield BrowserTestUtils.switchTab(gBrowser, tab2);
      yield tabRestored;

      // Selecting the fourth tab should now send it to the tab crashed page.
      tabCrashedPagePromise =
        BrowserTestUtils.waitForContentEvent(tab4.linkedBrowser,
                                             "AboutTabCrashedReady",
                                             false, null, true);
      yield BrowserTestUtils.switchTab(gBrowser, tab4);
      yield tabCrashedPagePromise;

      // Selecting the third tab should restore it.
      tabRestored = promiseTabRestored(tab3);
      yield BrowserTestUtils.switchTab(gBrowser, tab3);
      yield tabRestored;
    });
  });
});
