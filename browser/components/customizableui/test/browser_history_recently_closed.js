/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { SessionStoreTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SessionStoreTestUtils.sys.mjs"
);
const triggeringPrincipal_base64 = E10SUtils.SERIALIZED_SYSTEMPRINCIPAL;

SessionStoreTestUtils.init(this, window);

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

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
  Assert.ok(
    !recentlyClosedTabs.getAttribute("disabled"),
    "Recently closed tabs button enabled"
  );
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

function resetClosedTabsAndWindows() {
  // Clear the lists of closed windows and tabs.
  Services.obs.notifyObservers(null, "browser:purge-session-history");
  is(SessionStore.getClosedWindowCount(), 0, "Expect 0 closed windows");
  for (const win of BrowserWindowTracker.orderedWindows) {
    is(
      SessionStore.getClosedTabCountForWindow(win),
      0,
      "Expect 0 closed tabs for this window"
    );
  }
}

registerCleanupFunction(async () => {
  await resetClosedTabsAndWindows();
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
  await SessionStoreTestUtils.openAndCloseTab(
    window,
    TEST_PATH + "dummy_history_item.html"
  );

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
  BrowserTestUtils.startLoadingURIString(
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
  await resetClosedTabsAndWindows();
  const initialTabCount = gBrowser.visibleTabs.length;

  const closedTabUrls = [
    "about:robots",
    "https://example.com/",
    "https://example.org/",
  ];
  const windowState = {
    tabs: [
      {
        entries: [{ url: "about:mozilla", triggeringPrincipal_base64 }],
      },
    ],
    _closedTabs: closedTabUrls.map(url => {
      return {
        title: url,
        state: {
          entries: [
            {
              url,
              triggeringPrincipal_base64,
            },
          ],
        },
      };
    }),
  };
  await SessionStoreTestUtils.promiseBrowserState({
    windows: [windowState],
  });

  is(gBrowser.visibleTabs.length, 1, "We start with one tab open");
  // Open the "Recently closed tabs" panel.
  let closeTabsPanel = await openRecentlyClosedTabsMenu();

  // Click the first toolbar button in the panel.
  let toolbarButton = closeTabsPanel.querySelector(
    ".panel-subview-body toolbarbutton"
  );
  let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, null, true);
  EventUtils.sendMouseEvent({ type: "click" }, toolbarButton, window);

  info(
    "We should reopen the first of closedTabUrls: " +
      JSON.stringify(closedTabUrls)
  );
  let reopenedTab = await newTabPromise;
  is(
    reopenedTab.linkedBrowser.currentURI.spec,
    closedTabUrls[0],
    "Opened the first URL"
  );
  info(`restored tab, total open tabs: ${gBrowser.tabs.length}`);

  info("waiting for closeTab");
  await SessionStoreTestUtils.closeTab(reopenedTab);

  await openRecentlyClosedTabsMenu();
  let restoreAllItem = closeTabsPanel.querySelector(".restoreallitem");
  ok(
    restoreAllItem && !restoreAllItem.hidden,
    "Restore all menu item is not hidden"
  );

  // Click the restore-all toolbar button in the panel.
  EventUtils.sendMouseEvent({ type: "click" }, restoreAllItem, window);

  info("waiting for restored tabs");
  await BrowserTestUtils.waitForCondition(
    () => SessionStore.getClosedTabCount() === 0,
    "Waiting for all the closed tabs to be opened"
  );

  is(
    gBrowser.tabs.length,
    initialTabCount + closedTabUrls.length,
    "The expected number of closed tabs were restored"
  );

  // clean up extra tabs
  while (gBrowser.tabs.length > 1) {
    BrowserTestUtils.removeTab(gBrowser.tabs.at(-1));
  }
});

add_task(async function testRecentlyClosedWindows() {
  // We need to make sure the history is cleared before starting the test
  await Sanitizer.sanitize(["history"]);
  await resetClosedTabsAndWindows();

  // Open and close a new window.
  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  let loadedPromise = BrowserTestUtils.browserLoaded(
    newWin.gBrowser.selectedBrowser
  );
  BrowserTestUtils.startLoadingURIString(
    newWin.gBrowser.selectedBrowser,
    "https://example.com"
  );
  await loadedPromise;
  let closedObjectsChangePromise = TestUtils.topicObserved(
    "sessionstore-closed-objects-changed"
  );
  await BrowserTestUtils.closeWindow(newWin);
  await closedObjectsChangePromise;

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
  let newWindowPromise = BrowserTestUtils.waitForNewWindow({
    url: "https://example.com/",
  });
  closedObjectsChangePromise = TestUtils.topicObserved(
    "sessionstore-closed-objects-changed"
  );
  EventUtils.sendMouseEvent({ type: "click" }, toolbarButton, window);

  newWin = await newWindowPromise;
  await closedObjectsChangePromise;
  is(gBrowser.tabs.length, 1, "Did not open new tabs");

  await BrowserTestUtils.closeWindow(newWin);
});

add_task(async function testRecentlyClosedTabsFromClosedWindows() {
  await resetClosedTabsAndWindows();
  const closedTabUrls = [
    "about:robots",
    "https://example.com/",
    "https://example.org/",
  ];
  const closedWindowState = {
    tabs: [
      {
        entries: [{ url: "about:mozilla", triggeringPrincipal_base64 }],
      },
    ],
    _closedTabs: closedTabUrls.map(url => {
      return {
        title: url,
        state: {
          entries: [
            {
              url,
              triggeringPrincipal_base64,
            },
          ],
        },
      };
    }),
  };
  await SessionStoreTestUtils.promiseBrowserState({
    windows: [
      {
        tabs: [
          {
            entries: [{ url: "about:mozilla", triggeringPrincipal_base64 }],
          },
        ],
      },
    ],
    _closedWindows: [closedWindowState],
  });
  Assert.equal(
    SessionStore.getClosedTabCountFromClosedWindows(),
    closedTabUrls.length,
    "Sanity check number of closed tabs from closed windows"
  );

  prepareHistoryPanel();
  let closeTabsPanel = await openRecentlyClosedTabsMenu();
  // make sure we can actually restore one of these closed tabs
  const closedTabItems = closeTabsPanel.querySelectorAll(
    "toolbarbutton[targetURI]"
  );
  Assert.equal(
    closedTabItems.length,
    closedTabUrls.length,
    "We have expected number of closed tab items"
  );

  const newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, null, true);
  const closedObjectsChangePromise = TestUtils.topicObserved(
    "sessionstore-closed-objects-changed"
  );
  EventUtils.sendMouseEvent({ type: "click" }, closedTabItems[0], window);
  await newTabPromise;
  await closedObjectsChangePromise;

  // flip the pref so none of the closed tabs from closed window are included
  await SpecialPowers.pushPrefEnv({
    set: [["browser.sessionstore.closedTabsFromClosedWindows", false]],
  });
  await openHistoryPanel();

  // verify the recently-closed-tabs menu item is disabled
  let recentlyClosedTabsItem = document.getElementById(
    "appMenuRecentlyClosedTabs"
  );
  Assert.ok(
    recentlyClosedTabsItem.hasAttribute("disabled"),
    "Recently closed tabs button is now disabled"
  );
  SpecialPowers.popPrefEnv();
  while (gBrowser.tabs.length > 1) {
    await SessionStoreTestUtils.closeTab(
      gBrowser.tabs[gBrowser.tabs.length - 1]
    );
  }
});
