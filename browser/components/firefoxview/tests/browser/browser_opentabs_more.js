/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let gInitialTab;
let gInitialTabURL;

// This test opens many tabs and regularly times out - especially with verify
requestLongerTimeout(2);

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
  info(`Restoring to browserState: ${JSON.stringify(browserState, null, 2)}`);
  await SessionStoreTestUtils.promiseBrowserState(browserState);
  info("Windows and tabs opened, waiting for readyWindowsPromise");
  await NonPrivateTabs.readyWindowsPromise;
  info("readyWindowsPromise resolved");

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

  await openFirefoxViewTab(window).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    await navigateToOpenTabs(browser);
    const openTabs = getOpenTabsComponent(browser);
    await openTabs.updateComplete;

    let cards;
    info(`Waiting for ${NUMBER_OF_WINDOWS} of window cards`);
    await BrowserTestUtils.waitForCondition(() => {
      cards = getOpenTabsCards(openTabs);
      return cards.length == NUMBER_OF_WINDOWS;
    });
    is(cards.length, NUMBER_OF_WINDOWS, "There are four windows.");
    lastCard = cards[NUMBER_OF_WINDOWS - 1];

    Assert.less(
      (await getTabRowsForCard(lastCard)).length,
      NUMBER_OF_TABS,
      "Not all tabs are shown yet."
    );
    info("Toggle the Show More link.");
    lastCard.shadowRoot.querySelector("div[slot=footer]").click();
    await BrowserTestUtils.waitForMutationCondition(
      lastCard.shadowRoot,
      { childList: true, subtree: true },
      async () => (await getTabRowsForCard(lastCard)).length === NUMBER_OF_TABS
    );

    info("Toggle the Show Less link.");
    lastCard.shadowRoot.querySelector("div[slot=footer]").click();
    await BrowserTestUtils.waitForMutationCondition(
      lastCard.shadowRoot,
      { childList: true, subtree: true },
      async () => (await getTabRowsForCard(lastCard)).length < NUMBER_OF_TABS
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
      async () => (await getTabRowsForCard(lastCard)).length === NUMBER_OF_TABS
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
      async () => (await getTabRowsForCard(lastCard)).length < NUMBER_OF_TABS
    );

    await SpecialPowers.popPrefEnv();
  });
  await cleanup();
});
