/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { TabsSetupFlowManager } = ChromeUtils.importESModule(
  "resource:///modules/firefox-view-tabs-setup-manager.sys.mjs"
);

XPCOMUtils.defineLazyModuleGetters(globalThis, {
  SyncedTabs: "resource://services-sync/SyncedTabs.jsm",
});

const CLOSED_URI = "https://www.example.com/";
const SYNCED_URI = syncedTabsData1[0].tabs[1].url;

add_task(async function test_keyboard_focus() {
  await SpecialPowers.pushPrefEnv({
    set: [["accessibility.tabfocus", 7]],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, CLOSED_URI);

  const closedObjectsChanged = () =>
    TestUtils.topicObserved("sessionstore-closed-objects-changed");

  const sessionStorePromise = BrowserTestUtils.waitForSessionStoreUpdate(tab);
  BrowserTestUtils.removeTab(tab);
  await sessionStorePromise;

  await closedObjectsChanged();

  await withFirefoxView({ win: window }, async browser => {
    const { document } = browser.contentWindow;

    const sandbox = setupRecentDeviceListMocks();
    const syncedTabsMock = sandbox.stub(SyncedTabs, "getRecentTabs");
    let mockTabs1 = getMockTabData(syncedTabsData1);
    syncedTabsMock.returns(mockTabs1);

    await setupListState(browser);

    testVisibility(browser, {
      expectedVisible: {
        "ol.synced-tabs-list": true,
      },
    });

    let tabPickupEle = document.querySelector(".synced-tab-a");
    document.querySelector("#collapsible-synced-tabs-button").focus();

    EventUtils.synthesizeKey("KEY_Tab");

    is(
      tabPickupEle,
      document.activeElement,
      "The first tab pickup link is focused"
    );

    let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, SYNCED_URI);
    EventUtils.synthesizeKey("KEY_Enter");
    await newTabPromise;

    gBrowser.removeTab(gBrowser.selectedTab);
    window.FirefoxViewHandler.openTab();

    let recentlyClosedEle = document.querySelector(".closed-tab-li");
    document.querySelector("#collapsible-tabs-button").focus();

    EventUtils.synthesizeKey("KEY_Tab");

    is(
      recentlyClosedEle,
      document.activeElement,
      "The recently closed tab is focused"
    );

    newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, CLOSED_URI);
    EventUtils.synthesizeKey("KEY_Enter");
    await newTabPromise;
    gBrowser.removeTab(gBrowser.selectedTab);
  });
});
