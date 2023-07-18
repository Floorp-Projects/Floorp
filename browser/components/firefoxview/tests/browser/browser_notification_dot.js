/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const tabsList1 = syncedTabsData1[0].tabs;
const tabsList2 = syncedTabsData1[1].tabs;
const BADGE_TOP_RIGHT = "75% 25%";

const { SyncedTabs } = ChromeUtils.importESModule(
  "resource://services-sync/SyncedTabs.sys.mjs"
);

function setupRecentDeviceListMocks() {
  const sandbox = sinon.createSandbox();
  sandbox.stub(fxAccounts.device, "recentDeviceList").get(() => [
    {
      id: 1,
      name: "My desktop",
      isCurrentDevice: true,
      type: "desktop",
      tabs: [],
    },
    {
      id: 2,
      name: "My iphone",
      type: "mobile",
      tabs: [],
    },
  ]);

  sandbox.stub(UIState, "get").returns({
    status: UIState.STATUS_SIGNED_IN,
    syncEnabled: true,
  });

  return sandbox;
}

function waitForWindowActive(win, active) {
  info("Waiting for window activation");
  return Promise.all([
    BrowserTestUtils.waitForEvent(win, active ? "focus" : "blur"),
    BrowserTestUtils.waitForEvent(win, active ? "activate" : "deactivate"),
  ]);
}

async function waitForNotificationBadgeToBeShowing(fxViewButton) {
  info("Waiting for attention attribute to be set");
  await BrowserTestUtils.waitForMutationCondition(
    fxViewButton,
    { attributes: true },
    () => fxViewButton.hasAttribute("attention")
  );
  return fxViewButton.hasAttribute("attention");
}

async function waitForNotificationBadgeToBeHidden(fxViewButton) {
  info("Waiting for attention attribute to be removed");
  await BrowserTestUtils.waitForMutationCondition(
    fxViewButton,
    { attributes: true },
    () => !fxViewButton.hasAttribute("attention")
  );
  return !fxViewButton.hasAttribute("attention");
}

async function clickFirefoxViewButton(win) {
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#firefox-view-button",
    { type: "mousedown" },
    win.browsingContext
  );
}

function getBackgroundPositionForElement(ele) {
  let style = ele.ownerGlobal.getComputedStyle(ele);
  return style.getPropertyValue("background-position");
}

let previousFetchTime = 0;

async function resetSyncedTabsLastFetched() {
  Services.prefs.clearUserPref("services.sync.lastTabFetch");
  previousFetchTime = 0;
  await TestUtils.waitForTick();
}

async function initTabSync() {
  let recentFetchTime = Math.floor(Date.now() / 1000);
  // ensure we don't try to set the pref with the same value, which will not produce
  // the expected pref change effects
  while (recentFetchTime == previousFetchTime) {
    await TestUtils.waitForTick();
    recentFetchTime = Math.floor(Date.now() / 1000);
  }
  ok(
    recentFetchTime > previousFetchTime,
    "The new lastTabFetch value is greater than the previous"
  );

  info("initTabSync, updating lastFetch:" + recentFetchTime);
  Services.prefs.setIntPref("services.sync.lastTabFetch", recentFetchTime);
  previousFetchTime = recentFetchTime;
  await TestUtils.waitForTick();
}

add_setup(async function () {
  await resetSyncedTabsLastFetched();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.firefox-view.notify-for-tabs", true]],
  });

  // Clear any synced tabs from previous tests
  FirefoxViewNotificationManager.syncedTabs = null;
  Services.obs.notifyObservers(
    null,
    "firefoxview-notification-dot-update",
    "false"
  );
});

/**
 * Test that the notification badge will show and hide in the correct cases
 */
add_task(async function testNotificationDot() {
  const sandbox = setupRecentDeviceListMocks();
  const syncedTabsMock = sandbox.stub(SyncedTabs, "getRecentTabs");
  sandbox.spy(SyncedTabs, "syncTabs");

  let win = await BrowserTestUtils.openNewBrowserWindow();
  let fxViewBtn = win.document.getElementById("firefox-view-button");
  ok(fxViewBtn, "Got the Firefox View button");

  // Initiate a synced tabs update with new tabs
  syncedTabsMock.returns(tabsList1);
  await initTabSync();

  ok(
    BrowserTestUtils.is_visible(fxViewBtn),
    "The Firefox View button is showing"
  );

  info(
    "testNotificationDot, button is showing, badge should be initially hidden"
  );
  ok(
    await waitForNotificationBadgeToBeHidden(fxViewBtn),
    "The notification badge is not showing initially"
  );

  // Initiate a synced tabs update with new tabs
  syncedTabsMock.returns(tabsList2);
  await initTabSync();

  ok(
    await waitForNotificationBadgeToBeShowing(fxViewBtn),
    "The notification badge is showing after first tab sync"
  );

  // check that switching to the firefoxviewtab removes the badge
  await clickFirefoxViewButton(win);

  info(
    "testNotificationDot, after clicking the button, badge should become hidden"
  );
  ok(
    await waitForNotificationBadgeToBeHidden(fxViewBtn),
    "The notification badge is not showing after going to Firefox View"
  );

  await BrowserTestUtils.waitForCondition(() => {
    return SyncedTabs.syncTabs.calledOnce;
  });

  ok(SyncedTabs.syncTabs.calledOnce, "SyncedTabs.syncTabs() was called once");

  syncedTabsMock.returns(tabsList1);
  // Initiate a synced tabs update  with new tabs
  await initTabSync();

  // The noti badge would show but we are on a Firefox View page so no need to show the noti badge
  info(
    "testNotificationDot, after updating the recent tabs, badge should be hidden"
  );
  ok(
    await waitForNotificationBadgeToBeHidden(fxViewBtn),
    "The notification badge is not showing after tab sync while Firefox View is focused"
  );

  let newTab = await BrowserTestUtils.openNewForegroundTab(win.gBrowser);
  syncedTabsMock.returns(tabsList2);
  await initTabSync();

  ok(
    await waitForNotificationBadgeToBeShowing(fxViewBtn),
    "The notification badge is showing after navigation to a new tab"
  );

  // check that switching back to the Firefox View tab removes the badge
  await clickFirefoxViewButton(win);

  info(
    "testNotificationDot, after switching back to fxview, badge should be hidden"
  );
  ok(
    await waitForNotificationBadgeToBeHidden(fxViewBtn),
    "The notification badge is not showing after focusing the Firefox View tab"
  );

  await BrowserTestUtils.switchTab(win.gBrowser, newTab);

  // Initiate a synced tabs update with no new tabs
  await initTabSync();

  info(
    "testNotificationDot, after switching back to fxview with no new tabs, badge should be hidden"
  );
  ok(
    await waitForNotificationBadgeToBeHidden(fxViewBtn),
    "The notification badge is not showing after a tab sync with the same tabs"
  );

  await BrowserTestUtils.closeWindow(win);

  sandbox.restore();
});

/**
 * Tests the notification badge with multiple windows
 */
add_task(async function testNotificationDotOnMultipleWindows() {
  const sandbox = setupRecentDeviceListMocks();

  await resetSyncedTabsLastFetched();
  const syncedTabsMock = sandbox.stub(SyncedTabs, "getRecentTabs");

  // Create a new window
  let win1 = await BrowserTestUtils.openNewBrowserWindow();
  await win1.delayedStartupPromise;
  let fxViewBtn = win1.document.getElementById("firefox-view-button");
  ok(fxViewBtn, "Got the Firefox View button");

  syncedTabsMock.returns(tabsList1);
  // Initiate a synced tabs update
  await initTabSync();

  // Create another window
  let win2 = await BrowserTestUtils.openNewBrowserWindow();
  await win2.delayedStartupPromise;
  let fxViewBtn2 = win2.document.getElementById("firefox-view-button");

  await clickFirefoxViewButton(win2);

  // Make sure the badge doesn't show on any window
  info(
    "testNotificationDotOnMultipleWindows, badge is initially hidden on window 1"
  );
  ok(
    await waitForNotificationBadgeToBeHidden(fxViewBtn),
    "The notification badge is not showing in the inital window"
  );
  info(
    "testNotificationDotOnMultipleWindows, badge is initially hidden on window 2"
  );
  ok(
    await waitForNotificationBadgeToBeHidden(fxViewBtn2),
    "The notification badge is not showing in the second window"
  );

  // Minimize the window.
  win2.minimize();

  await TestUtils.waitForCondition(
    () => !win2.gBrowser.selectedBrowser.docShellIsActive,
    "Waiting for docshell to be marked as inactive after minimizing the window"
  );

  syncedTabsMock.returns(tabsList2);
  info("Initiate a synced tabs update with new tabs");
  await initTabSync();

  // The badge will show because the View tab is minimized
  // Make sure the badge shows on all windows
  info(
    "testNotificationDotOnMultipleWindows, after new synced tabs and minimized win2, badge is visible on window 1"
  );
  ok(
    await waitForNotificationBadgeToBeShowing(fxViewBtn),
    "The notification badge is showing in the initial window"
  );
  info(
    "testNotificationDotOnMultipleWindows, after new synced tabs and minimized win2, badge is visible on window 2"
  );
  ok(
    await waitForNotificationBadgeToBeShowing(fxViewBtn2),
    "The notification badge is showing in the second window"
  );

  win2.restore();
  await TestUtils.waitForCondition(
    () => win2.gBrowser.selectedBrowser.docShellIsActive,
    "Waiting for docshell to be marked as active after restoring the window"
  );

  await BrowserTestUtils.closeWindow(win1);
  await BrowserTestUtils.closeWindow(win2);

  sandbox.restore();
});

/**
 * Tests the notification badge is in the correct spot and that the badge shows when opening a new window
 * if another window is showing the badge
 */
add_task(async function testNotificationDotLocation() {
  const sandbox = setupRecentDeviceListMocks();
  await resetSyncedTabsLastFetched();
  const syncedTabsMock = sandbox.stub(SyncedTabs, "getRecentTabs");

  syncedTabsMock.returns(tabsList1);

  let win1 = await BrowserTestUtils.openNewBrowserWindow();
  let fxViewBtn = win1.document.getElementById("firefox-view-button");
  ok(fxViewBtn, "Got the Firefox View button");

  // Initiate a synced tabs update
  await initTabSync();
  syncedTabsMock.returns(tabsList2);
  // Initiate another synced tabs update
  await initTabSync();

  ok(
    await waitForNotificationBadgeToBeShowing(fxViewBtn),
    "The notification badge is showing initially"
  );

  // Create a new window
  let win2 = await BrowserTestUtils.openNewBrowserWindow();
  await win2.delayedStartupPromise;

  // Make sure the badge is showing on the new window
  let fxViewBtn2 = win2.document.getElementById("firefox-view-button");
  ok(
    await waitForNotificationBadgeToBeShowing(fxViewBtn2),
    "The notification badge is showing in the second window after opening"
  );

  // Make sure the badge is below and center now
  isnot(
    getBackgroundPositionForElement(fxViewBtn),
    BADGE_TOP_RIGHT,
    "The notification badge is not showing in the top right in the initial window"
  );
  isnot(
    getBackgroundPositionForElement(fxViewBtn2),
    BADGE_TOP_RIGHT,
    "The notification badge is not showing in the top right in the second window"
  );

  CustomizableUI.addWidgetToArea(
    "firefox-view-button",
    CustomizableUI.AREA_NAVBAR
  );

  // Make sure both windows still have the notification badge
  ok(
    await waitForNotificationBadgeToBeShowing(fxViewBtn),
    "The notification badge is showing in the initial window"
  );
  ok(
    await waitForNotificationBadgeToBeShowing(fxViewBtn2),
    "The notification badge is showing in the second window"
  );

  // Make sure the badge is in the top right now
  is(
    getBackgroundPositionForElement(fxViewBtn),
    BADGE_TOP_RIGHT,
    "The notification badge is showing in the top right in the initial window"
  );
  is(
    getBackgroundPositionForElement(fxViewBtn2),
    BADGE_TOP_RIGHT,
    "The notification badge is showing in the top right in the second window"
  );

  CustomizableUI.reset();
  await BrowserTestUtils.closeWindow(win1);
  await BrowserTestUtils.closeWindow(win2);

  sandbox.restore();
});
