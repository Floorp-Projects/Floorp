/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

async function openTab(URL) {
  let tab = BrowserTestUtils.addTab(gBrowser, URL);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  return tab;
}

async function closeTab(tab) {
  const sessionStorePromise = BrowserTestUtils.waitForSessionStoreUpdate(tab);
  BrowserTestUtils.removeTab(tab);
  await sessionStorePromise;
}

let panelMenuWidgetAdded = false;
function prepareHistoryPanel() {
  if (panelMenuWidgetAdded) {
    return;
  }
  CustomizableUI.addWidgetToArea(
    "history-panelmenu",
    CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
  );
  registerCleanupFunction(() => CustomizableUI.reset());
}

async function openRecentlyClosedTabsMenu() {
  prepareHistoryPanel();
  await openHistoryPanel();

  let recentlyClosedTabs = document.getElementById("appMenuRecentlyClosedTabs");
  let closeTabsPanel = document.getElementById(
    "appMenu-library-recentlyClosedTabs"
  );
  let panelView = closeTabsPanel && PanelView.forNode(closeTabsPanel);
  if (!panelView?.active) {
    recentlyClosedTabs.click();
    closeTabsPanel = document.getElementById(
      "appMenu-library-recentlyClosedTabs"
    );
    await BrowserTestUtils.waitForEvent(closeTabsPanel, "ViewShown");
    ok(
      PanelView.forNode(closeTabsPanel)?.active,
      "Opened 'Recently closed tabs' panel"
    );
  }

  return closeTabsPanel;
}

async function resetClosedTabs(win) {
  // Clear the lists of closed tabs.
  let sessionStoreChanged;
  let windows = win ? [win] : BrowserWindowTracker.orderedWindows;
  for (win of windows) {
    while (SessionStore.getClosedTabCountForWindow(win)) {
      sessionStoreChanged = TestUtils.topicObserved(
        "sessionstore-closed-objects-changed"
      );
      SessionStore.forgetClosedTab(win, 0);
      await sessionStoreChanged;
    }
  }
}

registerCleanupFunction(async () => {
  await resetClosedTabs();
});

add_task(async function testRecentlyClosedDisabled() {
  info("Check history recently closed tabs/windows section");

  prepareHistoryPanel();
  // We need to make sure the history is cleared before starting the test
  await Sanitizer.sanitize(["history"]);

  await openHistoryPanel();

  let recentlyClosedTabs = document.getElementById("appMenuRecentlyClosedTabs");
  let recentlyClosedWindows = document.getElementById(
    "appMenuRecentlyClosedWindows"
  );

  // Wait for the disabled attribute to change, as we receive
  // the "viewshown" event before this changes
  await BrowserTestUtils.waitForCondition(
    () => recentlyClosedTabs.getAttribute("disabled"),
    "Waiting for button to become disabled"
  );
  Assert.ok(
    recentlyClosedTabs.getAttribute("disabled"),
    "Recently closed tabs button disabled"
  );
  Assert.ok(
    recentlyClosedWindows.getAttribute("disabled"),
    "Recently closed windows button disabled"
  );

  await hideHistoryPanel();

  gBrowser.selectedTab.focus();
  let tab = await openTab(TEST_PATH + "dummy_history_item.html");
  await closeTab(tab);

  await openHistoryPanel();

  await BrowserTestUtils.waitForCondition(
    () => !recentlyClosedTabs.getAttribute("disabled"),
    "Waiting for button to be enabled"
  );
  Assert.ok(
    !recentlyClosedTabs.getAttribute("disabled"),
    "Recently closed tabs is available"
  );
  Assert.ok(
    recentlyClosedWindows.getAttribute("disabled"),
    "Recently closed windows button disabled"
  );

  await hideHistoryPanel();

  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  let loadedPromise = BrowserTestUtils.browserLoaded(
    newWin.gBrowser.selectedBrowser
  );
  BrowserTestUtils.loadURIString(
    newWin.gBrowser.selectedBrowser,
    "about:mozilla"
  );
  await loadedPromise;
  await BrowserTestUtils.closeWindow(newWin);

  await openHistoryPanel();

  await BrowserTestUtils.waitForCondition(
    () => !recentlyClosedWindows.getAttribute("disabled"),
    "Waiting for button to be enabled"
  );
  Assert.ok(
    !recentlyClosedTabs.getAttribute("disabled"),
    "Recently closed tabs is available"
  );
  Assert.ok(
    !recentlyClosedWindows.getAttribute("disabled"),
    "Recently closed windows is available"
  );

  await hideHistoryPanel();
});

add_task(async function testRecentlyClosedTabsDisabledPersists() {
  info("Check history recently closed tabs/windows section");

  prepareHistoryPanel();

  // We need to make sure the history is cleared before starting the test
  await Sanitizer.sanitize(["history"]);

  await openHistoryPanel();

  let recentlyClosedTabs = document.getElementById("appMenuRecentlyClosedTabs");
  Assert.ok(
    recentlyClosedTabs.getAttribute("disabled"),
    "Recently closed tabs button disabled"
  );

  await hideHistoryPanel();

  let newWin = await BrowserTestUtils.openNewBrowserWindow();

  await openHistoryPanel(newWin.document);
  recentlyClosedTabs = newWin.document.getElementById(
    "appMenuRecentlyClosedTabs"
  );
  Assert.ok(
    recentlyClosedTabs.getAttribute("disabled"),
    "Recently closed tabs is disabled"
  );

  // We close the window without hiding the panel first, which used to interfere
  // with populating the view subsequently.
  await BrowserTestUtils.closeWindow(newWin);

  newWin = await BrowserTestUtils.openNewBrowserWindow();
  await openHistoryPanel(newWin.document);
  recentlyClosedTabs = newWin.document.getElementById(
    "appMenuRecentlyClosedTabs"
  );
  Assert.ok(
    recentlyClosedTabs.getAttribute("disabled"),
    "Recently closed tabs is disabled"
  );
  await hideHistoryPanel(newWin.document);
  await BrowserTestUtils.closeWindow(newWin);
});

add_task(async function testRecentlyClosedRestoreAllTabs() {
  // We need to make sure the history is cleared before starting the test
  await Sanitizer.sanitize(["history"]);
  await resetClosedTabs();

  await BrowserTestUtils.withNewTab("about:mozilla", async browser => {
    // Open and close a few of tabs
    const closedTabUrls = [
      "about:robots",
      "https://example.com/",
      "https://example.org/",
    ];
    for (let url of closedTabUrls) {
      let tab = await openTab(url);
      await closeTab(tab);
    }

    // Open the "Recently closed tabs" panel.
    let closeTabsPanel = await openRecentlyClosedTabsMenu();

    // Click the first toolbar button in the panel.
    let toolbarButton = closeTabsPanel.querySelector(
      ".panel-subview-body toolbarbutton"
    );
    let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, null, true);
    EventUtils.sendMouseEvent({ type: "click" }, toolbarButton, window);

    info("waiting for reopenedTab");
    let reopenedTab = await newTabPromise;
    is(
      reopenedTab.linkedBrowser.currentURI.spec,
      closedTabUrls.pop(),
      "Opened correct URL"
    );
    info(`restored tab, total open tabs: ${gBrowser.tabs.length}`);
    info("waiting for closeTab");
    await closeTab(reopenedTab);

    let origTabCount = gBrowser.tabs.length;
    let reopenedTabs = [];

    await openRecentlyClosedTabsMenu();
    let restoreAllItem = closeTabsPanel.querySelector(".restoreallitem");
    ok(
      restoreAllItem && !restoreAllItem.hidden,
      "Restore all menu item is not hidden"
    );

    let tabsReOpenedPromise = new Promise(resolve => {
      gBrowser.tabContainer.addEventListener("TabOpen", function onTabOpen(e) {
        reopenedTabs.push(e.target);
        if (reopenedTabs.length == closedTabUrls.length) {
          gBrowser.tabContainer.removeEventListener("TabOpen", onTabOpen);
          resolve();
        }
      });
    });
    // Click the restore-all toolbar button in the panel.
    EventUtils.sendMouseEvent({ type: "click" }, restoreAllItem, window);

    info("waiting for restored tabs");
    await tabsReOpenedPromise;

    is(
      gBrowser.tabs.length,
      origTabCount + closedTabUrls.length,
      "The expected number of closed tabs were restored"
    );
    info(
      `restored ${reopenedTabs} tabs, total open tabs: ${gBrowser.tabs.length}`
    );
    for (let tab of reopenedTabs) {
      await closeTab(tab);
    }
  });
});

add_task(async function testRecentlyClosedWindows() {
  // We need to make sure the history is cleared before starting the test
  await Sanitizer.sanitize(["history"]);

  // Open and close a new window.
  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  let loadedPromise = BrowserTestUtils.browserLoaded(
    newWin.gBrowser.selectedBrowser
  );
  BrowserTestUtils.loadURIString(
    newWin.gBrowser.selectedBrowser,
    "https://example.com"
  );
  await loadedPromise;
  await BrowserTestUtils.closeWindow(newWin);

  prepareHistoryPanel();
  await openHistoryPanel();

  // Open the "Recently closed windows" panel.
  document.getElementById("appMenuRecentlyClosedWindows").click();

  let winPanel = document.getElementById(
    "appMenu-library-recentlyClosedWindows"
  );
  await BrowserTestUtils.waitForEvent(winPanel, "ViewShown");
  ok(true, "Opened 'Recently closed windows' panel");

  // Click the first toolbar button in the panel.
  let panelBody = winPanel.querySelector(".panel-subview-body");
  let toolbarButton = panelBody.querySelector("toolbarbutton");
  let newWindowPromise = BrowserTestUtils.waitForNewWindow();
  EventUtils.sendMouseEvent({ type: "click" }, toolbarButton, window);

  newWin = await newWindowPromise;
  is(
    newWin.gBrowser.currentURI.spec,
    "https://example.com/",
    "Opened correct URL"
  );
  is(gBrowser.tabs.length, 1, "Did not open new tabs");

  await BrowserTestUtils.closeWindow(newWin);
});
