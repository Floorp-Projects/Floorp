/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URL = "about:robots";
const ROW_URL_ID = "fxview-tab-row-url";
const ROW_DATE_ID = "fxview-tab-row-date";

let gInitialTab;
let gInitialTabURL;
const { NonPrivateTabs } = ChromeUtils.importESModule(
  "resource:///modules/OpenTabs.sys.mjs"
);

add_setup(function () {
  // This test opens a lot of windows and tabs and might run long on slower configurations
  requestLongerTimeout(2);
  gInitialTab = gBrowser.selectedTab;
  gInitialTabURL = gBrowser.selectedBrowser.currentURI.spec;
});

async function navigateToOpenTabs(browser) {
  const document = browser.contentDocument;
  if (document.querySelector("named-deck").selectedViewName != "opentabs") {
    await navigateToViewAndWait(browser.contentDocument, "opentabs");
  }
}

function getOpenTabsComponent(browser) {
  return browser.contentDocument.querySelector("named-deck > view-opentabs");
}

function getCards(browser) {
  return getOpenTabsComponent(browser).shadowRoot.querySelectorAll(
    "view-opentabs-card"
  );
}

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

async function getRowsForCard(card) {
  await TestUtils.waitForCondition(() => card.tabList.rowEls.length);
  return card.tabList.rowEls;
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
  const cards = getCards(browser);
  is(cards.length, expected.length, `There are ${expected.length} windows.`);
  for (let i = 0; i < cards.length; i++) {
    const tabItems = await getRowsForCard(cards[i]);
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
    const cards = getCards(browser);
    const tabItems = await getRowsForCard(cards[0]);
    tabItems[0].mainEl.click();
    await promiseHidden;
  });

  await BrowserTestUtils.waitForCondition(
    () => originalTab.selected,
    "The original tab is selected."
  );

  await openFirefoxViewTab(window).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    const cards = getCards(browser);
    let tabItems = await getRowsForCard(cards[0]);

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
    const [card] = getCards(browser);
    const [row] = await getRowsForCard(card);
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
    const cards = getCards(browser);
    const originalWinRows = await getRowsForCard(cards[1]);
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

    const cards = getCards(browser);
    is(cards.length, 2, "There are two windows.");
    const newWinRows = await getRowsForCard(cards[1]);

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

    const cards = getCards(browser);
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
      const [, secondCard] = getCards(linkedBrowser);
      const tabItems = await getRowsForCard(secondCard);
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

add_task(async function toggle_show_more_link() {
  const tabEntry = url => ({
    entries: [{ url, triggeringPrincipal_base64 }],
  });
  const NUMBER_OF_WINDOWS = 4;
  const NUMBER_OF_TABS = 42;
  const browserState = { windows: [] };
  for (let windowIndex = 0; windowIndex < NUMBER_OF_WINDOWS; windowIndex++) {
    const winData = { tabs: [] };
    let tabCount = windowIndex == NUMBER_OF_WINDOWS - 1 ? NUMBER_OF_TABS : 1;
    for (let i = 0; i < tabCount; i++) {
      winData.tabs.push(tabEntry(`data:,Window${windowIndex}-Tab${i}`));
    }
    winData.selected = winData.tabs.length;
    browserState.windows.push(winData);
  }
  // use Session restore to batch-open windows and tabs
  await SessionStoreTestUtils.promiseBrowserState(browserState);
  // restoring this state requires an update to the initial tab globals
  // so cleanup expects the right thing
  gInitialTab = gBrowser.selectedTab;
  gInitialTabURL = gBrowser.selectedBrowser.currentURI.spec;

  const windows = Array.from(Services.wm.getEnumerator("navigator:browser"));
  is(windows.length, NUMBER_OF_WINDOWS, "There are four browser windows.");

  const tab = (win = window) => {
    info("Tab");
    EventUtils.synthesizeKey("KEY_Tab", {}, win);
  };

  const enter = (win = window) => {
    info("Enter");
    EventUtils.synthesizeKey("KEY_Enter", {}, win);
  };

  let lastCard;

  SimpleTest.promiseFocus(window);
  await openFirefoxViewTab(window).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    await navigateToOpenTabs(browser);
    const openTabs = getOpenTabsComponent(browser);
    await openTabs.openTabsTarget.readyWindowsPromise;
    await openTabs.updateComplete;

    const cards = getCards(browser);
    is(cards.length, NUMBER_OF_WINDOWS, "There are four windows.");
    lastCard = cards[NUMBER_OF_WINDOWS - 1];
  });

  await openFirefoxViewTab(window).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    const openTabs = getOpenTabsComponent(browser);
    await openTabs.openTabsTarget.readyWindowsPromise;
    await openTabs.updateComplete;
    Assert.less(
      (await getRowsForCard(lastCard)).length,
      NUMBER_OF_TABS,
      "Not all tabs are shown yet."
    );
    info("Toggle the Show More link.");
    lastCard.shadowRoot.querySelector("div[slot=footer]").click();
    await BrowserTestUtils.waitForMutationCondition(
      lastCard.shadowRoot,
      { childList: true, subtree: true },
      async () => (await getRowsForCard(lastCard)).length === NUMBER_OF_TABS
    );

    info("Toggle the Show Less link.");
    lastCard.shadowRoot.querySelector("div[slot=footer]").click();
    await BrowserTestUtils.waitForMutationCondition(
      lastCard.shadowRoot,
      { childList: true, subtree: true },
      async () => (await getRowsForCard(lastCard)).length < NUMBER_OF_TABS
    );

    // Setting this pref allows the test to run as expected with a keyboard on MacOS
    await SpecialPowers.pushPrefEnv({
      set: [["accessibility.tabfocus", 7]],
    });

    info("Toggle the Show More link with keyboard.");
    lastCard.shadowRoot.querySelector("card-container").summaryEl.focus();
    // Tab to first item in the list
    tab();
    // Tab to the footer
    tab();
    enter();
    await BrowserTestUtils.waitForMutationCondition(
      lastCard.shadowRoot,
      { childList: true, subtree: true },
      async () => (await getRowsForCard(lastCard)).length === NUMBER_OF_TABS
    );

    info("Toggle the Show Less link with keyboard.");
    lastCard.shadowRoot.querySelector("card-container").summaryEl.focus();
    // Tab to first item in the list
    tab();
    // Tab to the footer
    tab();
    enter();
    await BrowserTestUtils.waitForMutationCondition(
      lastCard.shadowRoot,
      { childList: true, subtree: true },
      async () => (await getRowsForCard(lastCard)).length < NUMBER_OF_TABS
    );

    await SpecialPowers.popPrefEnv();
  });
  await cleanup();
});

add_task(async function search_open_tabs() {
  // Open a new window and navigate to TEST_URL. Then, when we search for
  // TEST_URL, it should show a search result in the new window's card.
  const win = await BrowserTestUtils.openNewBrowserWindow();
  await BrowserTestUtils.openNewForegroundTab(win.gBrowser, TEST_URL);

  await SpecialPowers.pushPrefEnv({
    set: [["browser.firefox-view.search.enabled", true]],
  });
  await openFirefoxViewTab(window).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    await navigateToOpenTabs(browser);
    const openTabs = getOpenTabsComponent(browser);
    await openTabs.openTabsTarget.readyWindowsPromise;
    await openTabs.updateComplete;

    const cards = getCards(browser);
    is(cards.length, 2, "There are two windows.");
    const winTabs = await getRowsForCard(cards[0]);
    const newWinTabs = await getRowsForCard(cards[1]);

    info("Input a search query.");
    EventUtils.synthesizeMouseAtCenter(openTabs.searchTextbox, {}, content);
    EventUtils.sendString(TEST_URL, content);
    await TestUtils.waitForCondition(
      () => openTabs.viewCards[0].tabList.rowEls.length === 0,
      "There are no matching search results in the original window."
    );
    await TestUtils.waitForCondition(
      () => openTabs.viewCards[1].tabList.rowEls.length === 1,
      "There is one matching search result in the new window."
    );

    info("Clear the search query.");
    EventUtils.synthesizeMouseAtCenter(
      openTabs.searchTextbox.clearButton,
      {},
      content
    );
    await TestUtils.waitForCondition(
      () => openTabs.viewCards[0].tabList.rowEls.length === winTabs.length,
      "The original window's list is restored."
    );
    await TestUtils.waitForCondition(
      () => openTabs.viewCards[1].tabList.rowEls.length === newWinTabs.length,
      "The new window's list is restored."
    );
    openTabs.searchTextbox.blur();

    info("Input a search query with keyboard.");
    EventUtils.synthesizeKey("f", { accelKey: true }, content);
    EventUtils.sendString(TEST_URL, content);
    await TestUtils.waitForCondition(
      () => openTabs.viewCards[0].tabList.rowEls.length === 0,
      "There are no matching search results in the original window."
    );
    await TestUtils.waitForCondition(
      () => openTabs.viewCards[1].tabList.rowEls.length === 1,
      "There is one matching search result in the new window."
    );

    info("Clear the search query with keyboard.");
    is(
      openTabs.shadowRoot.activeElement,
      openTabs.searchTextbox,
      "Search input is focused"
    );
    EventUtils.synthesizeKey("KEY_Tab", {}, content);
    ok(
      openTabs.searchTextbox.clearButton.matches(":focus-visible"),
      "Clear Search button is focused"
    );
    EventUtils.synthesizeKey("KEY_Enter", {}, content);
    await TestUtils.waitForCondition(
      () => openTabs.viewCards[0].tabList.rowEls.length === winTabs.length,
      "The original window's list is restored."
    );
    await TestUtils.waitForCondition(
      () => openTabs.viewCards[1].tabList.rowEls.length === newWinTabs.length,
      "The new window's list is restored."
    );
  });

  await SpecialPowers.popPrefEnv();
  await cleanup();
});

add_task(async function search_open_tabs_recent_browsing() {
  const NUMBER_OF_TABS = 6;
  const win = await BrowserTestUtils.openNewBrowserWindow();
  for (let i = 0; i < NUMBER_OF_TABS; i++) {
    await BrowserTestUtils.openNewForegroundTab(win.gBrowser, TEST_URL);
  }
  await SpecialPowers.pushPrefEnv({
    set: [["browser.firefox-view.search.enabled", true]],
  });
  await openFirefoxViewTab(window).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    await navigateToViewAndWait(browser.contentDocument, "recentbrowsing");
    const recentBrowsing = browser.contentDocument.querySelector(
      "view-recentbrowsing"
    );

    info("Input a search query.");
    EventUtils.synthesizeMouseAtCenter(
      recentBrowsing.searchTextbox,
      {},
      content
    );
    EventUtils.sendString(TEST_URL, content);
    const slot = recentBrowsing.querySelector("[slot='opentabs']");
    await TestUtils.waitForCondition(
      () => slot.viewCards[0].tabList.rowEls.length === 5,
      "Not all search results are shown yet."
    );

    info("Click the Show All link.");
    const showAllLink = await TestUtils.waitForCondition(() => {
      const elt = slot.viewCards[0].shadowRoot.querySelector(
        "[data-l10n-id='firefoxview-show-all']"
      );
      EventUtils.synthesizeMouseAtCenter(elt, {}, content);
      if (slot.viewCards[0].tabList.rowEls.length === NUMBER_OF_TABS) {
        return elt;
      }
      return false;
    }, "All search results are shown.");
    is(showAllLink.role, "link", "The show all control is a link.");
    ok(BrowserTestUtils.isHidden(showAllLink), "The show all link is hidden.");
  });
  await SpecialPowers.popPrefEnv();
  await cleanup();
});
