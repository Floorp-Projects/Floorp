/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URL = "about:robots";
const ROW_URL_ID = "fxview-tab-row-url";
const ROW_DATE_ID = "fxview-tab-row-date";

let gInitialTab;
let gInitialTabURL;

add_setup(function () {
  // This test opens a lot of windows and tabs and might run long on slower configurations
  requestLongerTimeout(2);
  gInitialTab = gBrowser.selectedTab;
  gInitialTabURL = gBrowser.selectedBrowser.currentURI.spec;
});

async function cleanup() {
  await SimpleTest.promiseFocus(window);
  await promiseAllButPrimaryWindowClosed();
  await BrowserTestUtils.switchTab(gBrowser, gInitialTab);
  await closeFirefoxViewTab(window);

  // clean up extra tabs
  while (gBrowser.tabs.length > 1) {
    BrowserTestUtils.removeTab(gBrowser.tabs.at(-1));
  }

  is(
    BrowserWindowTracker.orderedWindows.length,
    1,
    "One window at the end of test cleanup"
  );
  Assert.deepEqual(
    gBrowser.tabs.map(tab => tab.linkedBrowser.currentURI.spec),
    [gInitialTabURL],
    "One about:blank tab open at the end up test cleanup"
  );
}

/**
 * Verify that there are the expected number of cards, and that each card has
 * the expected URLs in order.
 *
 * @param {tabbrowser} browser
 *   The browser to verify in.
 * @param {string[][]} expected
 *   The expected URLs for each card.
 */
async function checkTabLists(browser, expected) {
  const cards = getOpenTabsCards(getOpenTabsComponent(browser));
  is(cards.length, expected.length, `There are ${expected.length} windows.`);
  for (let i = 0; i < cards.length; i++) {
    const tabItems = await getTabRowsForCard(cards[i]);
    const actual = Array.from(tabItems).map(({ url }) => url);
    Assert.deepEqual(
      actual,
      expected[i],
      "Tab list has items with URLs in the expected order"
    );
  }
}

add_task(async function open_tab_same_window() {
  await openFirefoxViewTab(window).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    await navigateToOpenTabs(browser);
    const openTabs = getOpenTabsComponent(browser);
    await openTabs.openTabsTarget.readyWindowsPromise;
    await openTabs.updateComplete;

    await checkTabLists(browser, [[gInitialTabURL]]);
    let promiseHidden = BrowserTestUtils.waitForEvent(
      browser.contentDocument,
      "visibilitychange"
    );
    let tabChangeRaised = BrowserTestUtils.waitForEvent(
      NonPrivateTabs,
      "TabChange"
    );
    await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
    await promiseHidden;
    await tabChangeRaised;
  });

  const [originalTab, newTab] = gBrowser.visibleTabs;

  await openFirefoxViewTab(window).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    const openTabs = getOpenTabsComponent(browser);
    setSortOption(openTabs, "tabStripOrder");
    await openTabs.openTabsTarget.readyWindowsPromise;
    await openTabs.updateComplete;

    await checkTabLists(browser, [[gInitialTabURL, TEST_URL]]);
    let promiseHidden = BrowserTestUtils.waitForEvent(
      browser.contentDocument,
      "visibilitychange"
    );
    const cards = getOpenTabsCards(openTabs);
    const tabItems = await getTabRowsForCard(cards[0]);
    tabItems[0].mainEl.click();
    await promiseHidden;
  });

  await BrowserTestUtils.waitForCondition(
    () => originalTab.selected,
    "The original tab is selected."
  );

  await openFirefoxViewTab(window).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    const cards = getOpenTabsCards(getOpenTabsComponent(browser));
    let tabItems = await getTabRowsForCard(cards[0]);

    let promiseHidden = BrowserTestUtils.waitForEvent(
      browser.contentDocument,
      "visibilitychange"
    );

    tabItems[1].mainEl.click();
    await promiseHidden;
  });

  await BrowserTestUtils.waitForCondition(
    () => newTab.selected,
    "The new tab is selected."
  );

  await openFirefoxViewTab(window).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    let tabChangeRaised = BrowserTestUtils.waitForEvent(
      NonPrivateTabs,
      "TabChange"
    );

    info("Bring the new tab to the front.");
    gBrowser.moveTabTo(newTab, 0);

    await tabChangeRaised;
    await checkTabLists(browser, [[TEST_URL, gInitialTabURL]]);
    tabChangeRaised = BrowserTestUtils.waitForEvent(
      NonPrivateTabs,
      "TabChange"
    );
    await BrowserTestUtils.removeTab(newTab);
    await tabChangeRaised;

    await checkTabLists(browser, [[gInitialTabURL]]);
    const [card] = getOpenTabsCards(getOpenTabsComponent(browser));
    const [row] = await getTabRowsForCard(card);
    ok(
      !row.shadowRoot.getElementById("fxview-tab-row-url").hidden,
      "The URL is displayed, since we have one window."
    );
    ok(
      !row.shadowRoot.getElementById("fxview-tab-row-date").hidden,
      "The date is displayed, since we have one window."
    );
  });

  await cleanup();
});

add_task(async function open_tab_new_window() {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  let winFocused;
  await BrowserTestUtils.openNewForegroundTab(win.gBrowser, TEST_URL);

  info("Open fxview in new window");
  await openFirefoxViewTab(win).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    await navigateToOpenTabs(browser);
    const openTabs = getOpenTabsComponent(browser);
    setSortOption(openTabs, "tabStripOrder");
    await openTabs.openTabsTarget.readyWindowsPromise;
    await openTabs.updateComplete;

    await checkTabLists(browser, [
      [gInitialTabURL, TEST_URL],
      [gInitialTabURL],
    ]);
    const cards = getOpenTabsCards(openTabs);
    const originalWinRows = await getTabRowsForCard(cards[1]);
    const [row] = originalWinRows;
    ok(
      row.shadowRoot.getElementById("fxview-tab-row-url").hidden,
      "The URL is hidden, since we have two windows."
    );
    ok(
      row.shadowRoot.getElementById("fxview-tab-row-date").hidden,
      "The date is hidden, since we have two windows."
    );
    info("Select a tab from the original window.");
    let tabChangeRaised = BrowserTestUtils.waitForEvent(
      NonPrivateTabs,
      "TabRecencyChange"
    );
    winFocused = BrowserTestUtils.waitForEvent(window, "focus", true);
    originalWinRows[0].mainEl.click();
    await tabChangeRaised;
  });

  info("Wait for the original window to be focused");
  await winFocused;

  await openFirefoxViewTab(window).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    await navigateToOpenTabs(browser);
    const openTabs = getOpenTabsComponent(browser);
    await openTabs.openTabsTarget.readyWindowsPromise;
    await openTabs.updateComplete;

    const cards = getOpenTabsCards(openTabs);
    is(cards.length, 2, "There are two windows.");
    const newWinRows = await getTabRowsForCard(cards[1]);

    info("Select a tab from the new window.");
    winFocused = BrowserTestUtils.waitForEvent(win, "focus", true);
    let tabChangeRaised = BrowserTestUtils.waitForEvent(
      NonPrivateTabs,
      "TabRecencyChange"
    );
    newWinRows[0].mainEl.click();
    await tabChangeRaised;
  });
  info("Wait for the new window to be focused");
  await winFocused;
  await cleanup();
});

add_task(async function open_tab_new_private_window() {
  await BrowserTestUtils.openNewBrowserWindow({ private: true });

  await SimpleTest.promiseFocus(window);
  await openFirefoxViewTab(window).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    await navigateToOpenTabs(browser);
    const openTabs = getOpenTabsComponent(browser);
    await openTabs.openTabsTarget.readyWindowsPromise;
    await openTabs.updateComplete;

    const cards = getOpenTabsCards(openTabs);
    is(cards.length, 1, "The private window is not displayed.");
  });
  await cleanup();
});

add_task(async function open_tab_new_window_sort_by_recency() {
  info("Open new tabs in a new window.");
  const newWindow = await BrowserTestUtils.openNewBrowserWindow();
  const tabs = [
    newWindow.gBrowser.selectedTab,
    await BrowserTestUtils.openNewForegroundTab(newWindow.gBrowser, URLs[0]),
    await BrowserTestUtils.openNewForegroundTab(newWindow.gBrowser, URLs[1]),
  ];

  info("Open Firefox View in the original window.");
  await openFirefoxViewTab(window).then(async ({ linkedBrowser }) => {
    await navigateToOpenTabs(linkedBrowser);
    const openTabs = getOpenTabsComponent(linkedBrowser);
    setSortOption(openTabs, "recency");
    await openTabs.openTabsTarget.readyWindowsPromise;
    await openTabs.updateComplete;

    await checkTabLists(linkedBrowser, [
      [gInitialTabURL],
      [URLs[1], URLs[0], gInitialTabURL],
    ]);
    info("Select tabs in the new window to trigger recency changes.");
    await SimpleTest.promiseFocus(newWindow);
    await BrowserTestUtils.switchTab(newWindow.gBrowser, tabs[1]);
    await BrowserTestUtils.switchTab(newWindow.gBrowser, tabs[0]);
    await SimpleTest.promiseFocus(window);
    await TestUtils.waitForCondition(async () => {
      const [, secondCard] = getOpenTabsCards(openTabs);
      const tabItems = await getTabRowsForCard(secondCard);
      return tabItems[0].url === gInitialTabURL;
    });
    await checkTabLists(linkedBrowser, [
      [gInitialTabURL],
      [gInitialTabURL, URLs[0], URLs[1]],
    ]);
  });
  await cleanup();
});

add_task(async function styling_for_multiple_windows() {
  await openFirefoxViewTab(window).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    await navigateToOpenTabs(browser);
    const openTabs = getOpenTabsComponent(browser);
    setSortOption(openTabs, "tabStripOrder");
    await openTabs.openTabsTarget.readyWindowsPromise;
    await openTabs.updateComplete;

    ok(
      openTabs.shadowRoot.querySelector("[card-count=one]"),
      "The container shows one column when one window is open."
    );
  });

  await BrowserTestUtils.openNewBrowserWindow();
  let tabChangeRaised = BrowserTestUtils.waitForEvent(
    NonPrivateTabs,
    "TabChange"
  );
  await NonPrivateTabs.readyWindowsPromise;
  await tabChangeRaised;
  is(
    NonPrivateTabs.currentWindows.length,
    2,
    "NonPrivateTabs now has 2 currentWindows"
  );

  info("switch to firefox view in the first window");
  SimpleTest.promiseFocus(window);
  await openFirefoxViewTab(window).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    const openTabs = getOpenTabsComponent(browser);
    await openTabs.openTabsTarget.readyWindowsPromise;
    await openTabs.updateComplete;
    is(
      openTabs.openTabsTarget.currentWindows.length,
      2,
      "There should be 2 current windows"
    );
    ok(
      openTabs.shadowRoot.querySelector("[card-count=two]"),
      "The container shows two columns when two windows are open."
    );
  });
  await BrowserTestUtils.openNewBrowserWindow();
  tabChangeRaised = BrowserTestUtils.waitForEvent(NonPrivateTabs, "TabChange");
  await NonPrivateTabs.readyWindowsPromise;
  await tabChangeRaised;
  is(
    NonPrivateTabs.currentWindows.length,
    3,
    "NonPrivateTabs now has 2 currentWindows"
  );

  SimpleTest.promiseFocus(window);
  await openFirefoxViewTab(window).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    const openTabs = getOpenTabsComponent(browser);
    await openTabs.openTabsTarget.readyWindowsPromise;
    await openTabs.updateComplete;

    ok(
      openTabs.shadowRoot.querySelector("[card-count=three-or-more]"),
      "The container shows three columns when three windows are open."
    );
  });
  await cleanup();
});
