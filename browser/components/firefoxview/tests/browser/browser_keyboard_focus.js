/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.defineESModuleGetters(globalThis, {
  SyncedTabs: "resource://services-sync/SyncedTabs.sys.mjs",
});

const SYNCED_URI = syncedTabsData1[0].tabs[1].url;

add_task(async function test_keyboard_focus() {
  await SpecialPowers.pushPrefEnv({
    set: [["accessibility.tabfocus", 7]],
  });

  await withFirefoxView({}, async browser => {
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
    document.querySelector(".page-section-header").focus();

    EventUtils.synthesizeKey("KEY_Tab");

    is(
      tabPickupEle,
      document.activeElement,
      "The first tab pickup link is focused"
    );

    let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, SYNCED_URI);
    EventUtils.synthesizeKey("KEY_Enter");
    await newTabPromise;

    is(
      SYNCED_URI,
      gBrowser.selectedBrowser.currentURI.displaySpec,
      "We opened the tab via keyboard"
    );

    let sessionStorePromise = BrowserTestUtils.waitForSessionStoreUpdate(
      gBrowser.selectedTab
    );
    gBrowser.removeTab(gBrowser.selectedTab);
    await sessionStorePromise;

    window.FirefoxViewHandler.openTab();

    let recentlyClosedEle = await TestUtils.waitForCondition(() =>
      document.querySelector(".closed-tab-li-main")
    );
    document.querySelectorAll(".page-section-header")[1].focus();

    EventUtils.synthesizeKey("KEY_Tab");

    is(
      recentlyClosedEle,
      document.activeElement,
      "The recently closed tab is focused"
    );

    newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, SYNCED_URI);
    EventUtils.synthesizeKey("KEY_Enter");
    await newTabPromise;
    is(
      SYNCED_URI,
      gBrowser.selectedBrowser.currentURI.displaySpec,
      "We opened the tab via keyboard"
    );
    gBrowser.removeTab(gBrowser.selectedTab);

    sessionStorePromise = TestUtils.topicObserved(
      "sessionstore-closed-objects-changed"
    );
    SessionStore.forgetClosedTab(window, 0);
    await sessionStorePromise;

    sandbox.restore();
  });
});
