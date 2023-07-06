/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(globalThis, {
  SessionStore: "resource:///modules/sessionstore/SessionStore.sys.mjs",
});

async function add_new_tab(URL, win = window) {
  const tab = await BrowserTestUtils.openNewForegroundTab(win.gBrowser, URL);
  return tab;
}

async function close_tab(tab) {
  const sessionStorePromise = BrowserTestUtils.waitForSessionStoreUpdate(tab);
  BrowserTestUtils.removeTab(tab);
  await sessionStorePromise;
}

async function restoreRecentlyClosedTab(browser, urlToRestore) {
  const { document } = browser.contentWindow;
  const tabListItem = document.querySelector(
    `ol.closed-tabs-list .closed-tab-li[data-targeturi="${urlToRestore}"]`
  );
  ok(tabListItem, `Found the ${urlToRestore} item to restore`);
  info(
    `will restore closed-tab ${urlToRestore} to sourceWindowId: ${tabListItem.dataset.sourceWindowid}`
  );

  let gBrowser = browser.getTabBrowser();
  let tabRestored = BrowserTestUtils.waitForNewTab(
    gBrowser,
    urlToRestore,
    true
  );
  let sessionStoreUpdated = TestUtils.topicObserved(
    "sessionstore-closed-objects-changed"
  );
  await SimpleTest.promiseFocus(browser.ownerGlobal);
  await BrowserTestUtils.synthesizeMouseAtCenter(
    `.closed-tab-li[data-targeturi="${urlToRestore}"] .closed-tab-li-main`,
    {},
    browser
  );
  info("waiting for sessionstore update");
  await sessionStoreUpdated;
  info("waiting the new tab to open");
  await tabRestored;
  is(
    gBrowser.selectedBrowser.currentURI.spec,
    urlToRestore,
    "Current tab is the restored tab"
  );
  return gBrowser.selectedTab;
}

async function dismissRecentlyClosedTab(browser, urlToDismiss) {
  const { document } = browser.contentWindow;

  const tabListItem = document.querySelector(
    `ol.closed-tabs-list .closed-tab-li[data-targeturi="${urlToDismiss}"]`
  );
  ok(tabListItem, `Found the ${urlToDismiss} item to dismiss`);
  const tabsList = tabListItem.closest(".closed-tabs-list");
  const initialCount = tabsList.children.length;
  info(
    `Before dismissing the ${urlToDismiss} item, there are ${initialCount} list items`
  );

  const sessionStoreUpdated = TestUtils.topicObserved(
    "sessionstore-closed-objects-changed"
  );
  const listUpdated = BrowserTestUtils.waitForMutationCondition(
    tabsList,
    { childList: true },
    () => {
      return tabsList.children.length == initialCount - 1;
    }
  );
  info("Waiting for the mouse click");
  await BrowserTestUtils.synthesizeMouseAtCenter(
    `.closed-tab-li[data-targeturi="${urlToDismiss}"] .closed-tab-li-dismiss`,
    {},
    browser
  );
  info("Waiting for sessionStore updated");
  await sessionStoreUpdated;
  info("Waiting for listUpdated");
  await listUpdated;
}

async function checkClosedTabList(browser, expected) {
  const { document } = browser.contentWindow;
  const tabsList = document.querySelector("ol.closed-tabs-list");

  ok(
    document.getElementById("recently-closed-tabs-container").open,
    "recently-closed-tabs container is open"
  );

  await BrowserTestUtils.waitForMutationCondition(
    tabsList,
    { childList: true },
    () => tabsList.children.length >= expected.length
  );
  let items = tabsList.querySelectorAll(".closed-tab-li");
  is(
    items.length,
    expected.length,
    `recently-closed-tabs-list should have ${expected.length} list items`
  );
  for (let expectedURL of expected) {
    const tabItem = tabsList.querySelector(
      `.closed-tab-li[data-targeturi="${expectedURL}"]`
    );
    ok(tabItem, "Expected closed tabItem with url:" + expectedURL);
  }
}

add_setup(async function setup() {
  Services.prefs.clearUserPref(RECENTLY_CLOSED_STATE_PREF);
  // disable the feature recommendations so they don't get in the way of fx-view interactions
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features",
        false,
      ],
      ["browser.firefox-view.view-count", 10],
    ],
  });
});

function taskSetup() {
  Services.obs.notifyObservers(null, "browser:purge-session-history");
  is(
    SessionStore.getClosedTabCount(window),
    0,
    "0 closed tabs after purging session history"
  );
  is(
    SessionStore.getClosedWindowCount(),
    0,
    "0 closed windows after purging session history"
  );
}

add_task(async function close_tab_in_other_window() {
  taskSetup();
  const initialWin = window;

  const urlsToOpen = [
    "https://example.org/",
    "about:about",
    "about:robots",
    "about:blank", // junk tab, we don't expect this to be recorded or restored
  ];
  const newWin = await BrowserTestUtils.openNewBrowserWindow();
  await SimpleTest.promiseFocus(newWin);

  const newTabs = [];
  for (let url of urlsToOpen) {
    const tab = await add_new_tab(url, newWin);
    info("Opened tab: " + tab);
    newTabs.push(tab);
  }
  info(`Opened ${newTabs.length} tabs`);

  for (let tab of newTabs) {
    info("closing tab:" + tab);
    await close_tab(tab);
  }

  // first check the closed tab is listed in the window it was opened in
  info("Checking firefoxview recently-closed tab list in new window");
  await withFirefoxView({ win: newWin }, async browser => {
    const expectedURLs = [
      "about:about",
      "https://example.org/",
      "about:robots",
    ];
    await checkClosedTabList(browser, expectedURLs);
  });

  info("Checking firefoxview recently-closed tab list in the original window");
  await withFirefoxView({ win: initialWin }, async browser => {
    const expectedURLs = [
      "about:about",
      "https://example.org/",
      "about:robots",
    ];
    await checkClosedTabList(browser, expectedURLs);
  });

  let restoredTab;
  let urlToRestore = "about:about";
  // check that tabs get restored into the current window
  info("restoring tab to the initial window");
  await withFirefoxView({ win: initialWin }, async browser => {
    restoredTab = await restoreRecentlyClosedTab(browser, urlToRestore);
    info("restored tab to the initial window");
  });
  ok(restoredTab, "Tab was restored");
  ok(
    newWin.gBrowser.selectedBrowser.currentURI.spec != urlToRestore,
    "We didnt restore to the source window"
  );

  // check the list was updated in fx-view in the other window
  await withFirefoxView({ win: newWin }, async browser => {
    await checkClosedTabList(browser, ["https://example.org/", "about:robots"]);
  });

  await withFirefoxView({ win: initialWin }, async browser => {
    let gBrowser = browser.getTabBrowser();
    is(
      gBrowser.selectedBrowser.currentURI.spec,
      "about:firefoxview",
      "Current tab is fx-view"
    );
    await checkClosedTabList(browser, ["https://example.org/", "about:robots"]);
  });

  // We closed tabs in newWin, verify that tabs can be dismissed from the initial window
  await withFirefoxView({ win: initialWin }, async browser => {
    const urlToDismiss = "https://example.org/";
    await dismissRecentlyClosedTab(browser, urlToDismiss);
    await checkClosedTabList(browser, ["about:robots"]);
  });
  await withFirefoxView({ win: newWin }, async browser => {
    await checkClosedTabList(browser, ["about:robots"]);
  });

  // Clean up
  await BrowserTestUtils.removeTab(restoredTab);
  await promiseAllButPrimaryWindowClosed();
});
