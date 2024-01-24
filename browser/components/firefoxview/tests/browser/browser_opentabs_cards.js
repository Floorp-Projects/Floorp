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

async function navigateToOpenTabs(browser) {
  const document = browser.contentDocument;
  if (document.querySelector("named-deck").selectedViewName != "opentabs") {
    await navigateToCategoryAndWait(browser.contentDocument, "opentabs");
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

add_task(async function open_tab_same_window() {
  await openFirefoxViewTab(window).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    await navigateToOpenTabs(browser);
    const openTabs = getOpenTabsComponent(browser);
    await openTabs.getUpdateComplete();

    const cards = getCards(browser);
    is(cards.length, 1, "There is one window.");
    let tabItems = await getRowsForCard(cards[0]);
    is(tabItems.length, 1, "There is one items.");
    is(
      tabItems[0].url,
      gBrowser.visibleTabs[0].linkedBrowser.currentURI.spec,
      "The first item represents the first visible tab"
    );
    let promiseHidden = BrowserTestUtils.waitForEvent(
      browser.contentDocument,
      "visibilitychange"
    );
    await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
    await promiseHidden;
  });

  const [originalTab, newTab] = gBrowser.visibleTabs;

  await openFirefoxViewTab(window).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    await TestUtils.waitForTick();
    const cards = getCards(browser);
    is(cards.length, 1, "There is one window.");
    let tabItems = await getRowsForCard(cards[0]);
    is(tabItems.length, 2, "There are two items.");
    is(tabItems[1].url, TEST_URL, "The newly opened tab appears last.");

    let promiseHidden = BrowserTestUtils.waitForEvent(
      browser.contentDocument,
      "visibilitychange"
    );
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
    const cards = getCards(browser);
    let tabItems;

    info("Bring the new tab to the front.");
    gBrowser.moveTabTo(newTab, 0);

    await BrowserTestUtils.waitForMutationCondition(
      cards[0].shadowRoot,
      { childList: true, subtree: true },
      async () => {
        tabItems = await getRowsForCard(cards[0]);
        return tabItems[0].url === TEST_URL;
      }
    );
    await BrowserTestUtils.removeTab(newTab);

    const [card] = getCards(browser);
    await TestUtils.waitForCondition(
      async () => (await getRowsForCard(card)).length === 1,
      "There is one tab left after closing the new one."
    );
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
    await openTabs.getUpdateComplete();

    const cards = getCards(browser);
    is(cards.length, 2, "There are two windows.");
    const newWinRows = await getRowsForCard(cards[0]);
    const originalWinRows = await getRowsForCard(cards[1]);
    is(
      originalWinRows.length,
      1,
      "There is one tab item in the original window."
    );
    is(newWinRows.length, 2, "There are two tab items in the new window.");
    is(newWinRows[1].url, TEST_URL, "The new tab item appears last.");
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
    winFocused = BrowserTestUtils.waitForEvent(window, "focus", true);
    originalWinRows[0].mainEl.click();
  });

  info("Wait for the original window to be focused");
  await winFocused;

  await openFirefoxViewTab(window).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    await navigateToOpenTabs(browser);
    const openTabs = getOpenTabsComponent(browser);
    await openTabs.getUpdateComplete();

    const cards = getCards(browser);
    is(cards.length, 2, "There are two windows.");
    const newWinRows = await getRowsForCard(cards[1]);

    info("Select a tab from the new window.");
    winFocused = BrowserTestUtils.waitForEvent(win, "focus", true);
    newWinRows[0].mainEl.click();
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
    await openTabs.getUpdateComplete();

    const cards = getCards(browser);
    is(cards.length, 1, "The private window is not displayed.");
  });
  await cleanup();
});

add_task(async function styling_for_multiple_windows() {
  await openFirefoxViewTab(window).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    await navigateToOpenTabs(browser);
    const openTabs = getOpenTabsComponent(browser);
    await openTabs.getUpdateComplete();

    ok(
      openTabs.shadowRoot.querySelector("[card-count=one]"),
      "The container shows one column when one window is open."
    );
  });
  await BrowserTestUtils.openNewBrowserWindow();

  info("switch to firefox view in the first window");
  SimpleTest.promiseFocus(window);
  await openFirefoxViewTab(window).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    const openTabs = getOpenTabsComponent(browser);
    await openTabs.getUpdateComplete();
    ok(
      openTabs.shadowRoot.querySelector("[card-count=two]"),
      "The container shows two columns when two windows are open."
    );
  });
  await BrowserTestUtils.openNewBrowserWindow();

  SimpleTest.promiseFocus(window);
  await openFirefoxViewTab(window).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    const openTabs = getOpenTabsComponent(browser);
    await openTabs.getUpdateComplete();

    ok(
      openTabs.shadowRoot.querySelector("[card-count=three-or-more]"),
      "The container shows three columns when three windows are open."
    );
  });
  await cleanup();
});

add_task(async function toggle_show_more_link() {
  const NUMBER_OF_WINDOWS = 4;
  const NUMBER_OF_TABS = 42;
  const windows = [];
  for (let i = 0; i < NUMBER_OF_WINDOWS - 1; i++) {
    windows.push(await BrowserTestUtils.openNewBrowserWindow());
  }

  const tab = (win = window) => {
    info("Tab");
    EventUtils.synthesizeKey("KEY_Tab", {}, win);
  };

  const enter = (win = window) => {
    info("Enter");
    EventUtils.synthesizeKey("KEY_Enter", {}, win);
  };

  let lastWindow;
  let lastCard;

  SimpleTest.promiseFocus(window);
  await openFirefoxViewTab(window).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    await navigateToOpenTabs(browser);
    const openTabs = getOpenTabsComponent(browser);
    await openTabs.getUpdateComplete();

    const cards = getCards(browser);
    is(cards.length, NUMBER_OF_WINDOWS, "There are four windows.");
    lastCard = cards[NUMBER_OF_WINDOWS - 1];
    lastWindow = windows[NUMBER_OF_WINDOWS - 2];

    for (let i = 0; i < NUMBER_OF_TABS - 1; i++) {
      await BrowserTestUtils.openNewForegroundTab(lastWindow.gBrowser);
    }
  });

  await openFirefoxViewTab(window).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    const openTabs = getOpenTabsComponent(browser);
    await openTabs.getUpdateComplete();
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
    await openTabs.getUpdateComplete();

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
    await navigateToCategoryAndWait(browser.contentDocument, "recentbrowsing");
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
