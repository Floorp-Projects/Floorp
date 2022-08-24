/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const tabsList1 = syncedTabsData1[0].tabs;
const tabsList2 = syncedTabsData1[1].tabs;
const BADGE_TOP_RIGHT = "75% 25%";

const { SyncedTabs } = ChromeUtils.import(
  "resource://services-sync/SyncedTabs.jsm"
);

function setupRecentDeviceListMocks() {
  const sandbox = sinon.createSandbox();
  sandbox.stub(fxAccounts.device, "recentDeviceList").get(() => [
    {
      id: 1,
      name: "My desktop",
      isCurrentDevice: true,
      type: "desktop",
    },
    {
      id: 2,
      name: "My iphone",
      type: "mobile",
    },
  ]);

  sandbox.stub(UIState, "get").returns({
    status: UIState.STATUS_SIGNED_IN,
    syncEnabled: true,
  });

  return sandbox;
}

function waitForWindowActive(win, active) {
  return Promise.all([
    BrowserTestUtils.waitForEvent(win, active ? "focus" : "blur"),
    BrowserTestUtils.waitForEvent(win, active ? "activate" : "deactivate"),
  ]);
}

async function waitForNotificationBadgeToBeShowing(fxViewButton) {
  await BrowserTestUtils.waitForMutationCondition(
    fxViewButton,
    { attributes: true },
    () => fxViewButton.hasAttribute("attention")
  );
  return fxViewButton.hasAttribute("attention");
}

async function waitForNotificationBadgeToBeHidden(fxViewButton) {
  await BrowserTestUtils.waitForMutationCondition(
    fxViewButton,
    { attributes: true },
    () => !fxViewButton.hasAttribute("attention")
  );
  return !fxViewButton.hasAttribute("attention");
}

function getBackgroundPositionForElement(ele) {
  let style = getComputedStyle(ele);
  return style.getPropertyValue("background-position");
}

let recentFetchTime = Math.floor(Date.now() / 1000);
async function initTabSync() {
  recentFetchTime += 1;
  info("updating lastFetch:" + recentFetchTime);
  Services.prefs.setIntPref("services.sync.lastTabFetch", recentFetchTime);
  await TestUtils.waitForTick();
}

add_setup(async () => {
  await window.delayedStartupPromise;
  await addFirefoxViewButtonToToolbar();

  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(FirefoxViewHandler.tab);
    removeFirefoxViewButtonFromToolbar();
  });
});

/**
 * Test that the notification badge will show and hide in the correct cases
 */
add_task(async function testNotificationDot() {
  const sandbox = setupRecentDeviceListMocks();
  const syncedTabsMock = sandbox.stub(SyncedTabs, "getRecentTabs");

  syncedTabsMock.returns(tabsList1);
  // Initiate a synced tabs update  with new tabs
  await initTabSync();

  let fxViewBtn = document.getElementById("firefox-view-button");
  ok(fxViewBtn, "Got the Firefox View button");

  ok(
    BrowserTestUtils.is_visible(fxViewBtn),
    "The Firefox View button is showing"
  );

  ok(
    await waitForNotificationBadgeToBeHidden(fxViewBtn),
    "The notification badge is not showing initially"
  );

  syncedTabsMock.returns(tabsList2);
  // Initiate a synced tabs update  with new tabs
  await initTabSync();

  ok(
    await waitForNotificationBadgeToBeShowing(fxViewBtn),
    "The notification badge is showing after first tab sync"
  );

  // check that switching to the firefoxviewtab removes the badge
  fxViewBtn.click();

  ok(
    await waitForNotificationBadgeToBeHidden(fxViewBtn),
    "The notification badge is not showing after going to Firefox View"
  );

  syncedTabsMock.returns(tabsList1);
  // Initiate a synced tabs update  with new tabs
  await initTabSync();

  // The noti badge would show but we are on a Firefox View page so no need to show the noti badge
  ok(
    await waitForNotificationBadgeToBeHidden(fxViewBtn),
    "The notification badge is not showing after tab sync while Firefox View is focused"
  );

  let newTab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  syncedTabsMock.returns(tabsList2);
  await initTabSync();

  ok(
    await waitForNotificationBadgeToBeShowing(fxViewBtn),
    "The notification badge is showing after navigation to a new tab"
  );

  // check that switching back to the Firefox View tab removes the badge
  fxViewBtn.click();

  ok(
    await waitForNotificationBadgeToBeHidden(fxViewBtn),
    "The notification badge is not showing after focusing the Firefox View tab"
  );

  await BrowserTestUtils.switchTab(gBrowser, newTab);

  // Initiate a synced tabs update with no new tabs
  await initTabSync();

  ok(
    await waitForNotificationBadgeToBeHidden(fxViewBtn),
    "The notification badge is not showing after a tab sync with the same tabs"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  BrowserTestUtils.removeTab(newTab);

  sandbox.restore();
});

/**
 * Tests the notification badge with multiple windows
 */
add_task(async function testNotificationDotOnMultipleWindows() {
  const sandbox = setupRecentDeviceListMocks();
  const syncedTabsMock = sandbox.stub(SyncedTabs, "getRecentTabs");

  syncedTabsMock.returns(tabsList1);
  // Initiate a synced tabs update
  await initTabSync();

  let fxViewBtn = document.getElementById("firefox-view-button");
  ok(fxViewBtn, "Got the Firefox View button");

  // Create a new window
  let win = await BrowserTestUtils.openNewBrowserWindow();
  await win.delayedStartupPromise;
  let fxViewBtn2 = win.document.getElementById("firefox-view-button");
  fxViewBtn2.click();

  // Make sure the badge doesn't shows on all windows
  ok(
    await waitForNotificationBadgeToBeHidden(fxViewBtn),
    "The notification badge is not showing in the inital window"
  );
  ok(
    await waitForNotificationBadgeToBeHidden(fxViewBtn2),
    "The notification badge is not showing in the second window"
  );

  // Minimize the window.
  win.minimize();

  await TestUtils.waitForCondition(
    () => !win.gBrowser.selectedTab.linkedBrowser.docShellIsActive
  );

  syncedTabsMock.returns(tabsList2);
  // Initiate a synced tabs update  with new tabs
  await initTabSync();

  // The badge will show because the View tab is minimized
  // Make sure the badge shows on all windows
  ok(
    await waitForNotificationBadgeToBeShowing(fxViewBtn),
    "The notification badge is showing in the initial window"
  );
  ok(
    await waitForNotificationBadgeToBeShowing(fxViewBtn2),
    "The notification badge is showing in the second window"
  );

  win.restore();
  await TestUtils.waitForCondition(
    () => win.gBrowser.selectedTab.linkedBrowser.docShellIsActive
  );

  await BrowserTestUtils.closeWindow(win);

  sandbox.restore();
});

/**
 * Tests the notification badge is in the correct spot and that the badge shows when opening a new window
 * if another window is showing the badge
 */
add_task(async function testNotificationDotLocation() {
  const sandbox = setupRecentDeviceListMocks();
  const syncedTabsMock = sandbox.stub(SyncedTabs, "getRecentTabs");

  syncedTabsMock.returns(tabsList1);
  // Initiate a synced tabs update
  await initTabSync();

  let fxViewBtn = document.getElementById("firefox-view-button");
  ok(fxViewBtn, "Got the Firefox View button");

  syncedTabsMock.returns(tabsList2);
  // Initiate a synced tabs update
  await initTabSync();

  ok(
    await waitForNotificationBadgeToBeShowing(fxViewBtn),
    "The notification badge is showing initially"
  );

  // Create a new window
  let win = await BrowserTestUtils.openNewBrowserWindow();
  await win.delayedStartupPromise;
  // let newTab = await BrowserTestUtils.openNewForegroundTab(win.gBrowser, "");

  // Make sure the badge doesn't showing on the new window
  let fxViewBtn2 = win.document.getElementById("firefox-view-button");
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

  await BrowserTestUtils.closeWindow(win);

  sandbox.restore();
});
