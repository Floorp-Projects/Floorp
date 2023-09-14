/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Verifies that middle-clicking "Recently Closed Tabs" in both history
// menus works as expected.

const URLS = [
  "http://example.com/",
  "http://example.org/",
  "http://example.net/",
];

async function setupTest() {
  // Navigate the initial tab to ensure that it won't be reused for the tab
  // that will be reopened.
  let loadPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    "https://example.com"
  );
  await loadPromise;

  // Populate the recently closed tabs list.
  for (let url of URLS) {
    await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  }
  for (let i = 0; i < URLS.length; i++) {
    gBrowser.removeTab(gBrowser.selectedTab);
  }

  return gBrowser.tabs.length;
}

add_task(async function testMenubar() {
  if (AppConstants.platform === "macosx") {
    ok(true, "Can't open menu items on macOS");
    return;
  }

  let nOpenTabs = await setupTest();

  // Open the "History" menu.
  let menu = document.getElementById("history-menu");
  let popupPromise = BrowserTestUtils.waitForEvent(menu, "popupshown");
  menu.open = true;
  await popupPromise;
  ok(true, "Opened 'History' menu");

  // Open the "Recently Closed Tabs" submenu.
  let undoMenu = document.getElementById("historyUndoMenu");
  popupPromise = BrowserTestUtils.waitForEvent(undoMenu, "popupshown");
  undoMenu.open = true;
  let popupEvent = await popupPromise;
  ok(true, "Opened 'Recently Closed Tabs' menu");

  // And now middle-click the first item in that menu, and ensure that we're
  // only opening a single new tab.
  let menuitems = popupEvent.target.querySelectorAll("menuitem");
  let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, null, true);
  popupEvent.target.activateItem(menuitems[0], { button: 1 });

  let newTab = await newTabPromise;
  is(newTab.linkedBrowser.currentURI.spec, URLS[0], "Opened correct URL");
  is(gBrowser.tabs.length, nOpenTabs + 1, "Only opened 1 new tab");

  gBrowser.removeTab(newTab);
});

add_task(async function testHistoryPanel() {
  let nOpenTabs = await setupTest();

  // Setup history panel.
  CustomizableUI.addWidgetToArea(
    "history-panelmenu",
    CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
  );
  registerCleanupFunction(() => CustomizableUI.reset());
  await openHistoryPanel();

  // Open the "Recently closed tabs" panel.
  let recentlyClosedTabs = document.getElementById("appMenuRecentlyClosedTabs");
  recentlyClosedTabs.click();

  let recentlyClosedTabsPanel = document.getElementById(
    "appMenu-library-recentlyClosedTabs"
  );
  await BrowserTestUtils.waitForEvent(recentlyClosedTabsPanel, "ViewShown");
  ok(true, "Opened 'Recently closed tabs' panel");

  let panelBody = recentlyClosedTabsPanel.querySelector(".panel-subview-body");
  let toolbarButtons = panelBody.querySelectorAll("toolbarbutton");

  // Middle-click the first toolbar button in the panel.
  let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, null, true);
  EventUtils.sendMouseEvent(
    { type: "click", button: 1 },
    toolbarButtons[0],
    window
  );

  let newTab = await newTabPromise;
  is(newTab.linkedBrowser.currentURI.spec, URLS[0], "Opened correct URL");
  is(gBrowser.tabs.length, nOpenTabs + 1, "Only opened 1 new tab");

  gBrowser.removeTab(newTab);
});
