/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URL = "about:robots";
let gInitialTab;
let gInitialTabURL;

add_setup(function () {
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
}

add_task(async function search_open_tabs() {
  // Open a new window and navigate to TEST_URL. Then, when we search for
  // TEST_URL, it should show a search result in the new window's card.
  const win2 = await BrowserTestUtils.openNewBrowserWindow();
  await switchToWindow(win2);
  await NonPrivateTabs.readyWindowsPromise;
  await BrowserTestUtils.openNewForegroundTab(win2.gBrowser, TEST_URL);

  await openFirefoxViewTab(window).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    await navigateToOpenTabs(browser);
    const openTabs = getOpenTabsComponent(browser);
    await openTabs.updateComplete;

    const cards = getOpenTabsCards(openTabs);
    is(cards.length, 2, "There are two windows.");
    const winTabs = await getTabRowsForCard(cards[0]);
    const newWinTabs = await getTabRowsForCard(cards[1]);

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
  const win2 = await BrowserTestUtils.openNewBrowserWindow();
  await switchToWindow(win2);
  await NonPrivateTabs.readyWindowsPromise;

  for (let i = 0; i < NUMBER_OF_TABS; i++) {
    await BrowserTestUtils.openNewForegroundTab(win2.gBrowser, TEST_URL);
  }
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
