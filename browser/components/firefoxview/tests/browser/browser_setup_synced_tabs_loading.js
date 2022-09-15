/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { TabsSetupFlowManager } = ChromeUtils.importESModule(
  "resource:///modules/firefox-view-tabs-setup-manager.sys.mjs"
);

XPCOMUtils.defineLazyModuleGetters(globalThis, {
  SyncedTabs: "resource://services-sync/SyncedTabs.jsm",
});

async function tearDown(sandbox) {
  sandbox?.restore();
  Services.prefs.clearUserPref("services.sync.lastTabFetch");
}

function checkLoadingState(browser, isLoading = false) {
  const { document } = browser.contentWindow;
  const tabsContainer = document.querySelector("#tabpickup-tabs-container");
  const tabsList = document.querySelector(
    "#tabpickup-tabs-container tab-pickup-list"
  );
  const loadingElem = document.querySelector(
    "#tabpickup-tabs-container .loading-content"
  );
  const setupElem = document.querySelector("#tabpickup-steps");

  if (isLoading) {
    ok(
      tabsContainer.classList.contains("loading"),
      "Tabs container has loading class"
    );
    BrowserTestUtils.is_visible(
      loadingElem,
      "Loading content is visible when loading"
    );
    !tabsList ||
      BrowserTestUtils.is_hidden(
        tabsList,
        "Synced tabs list is not visible when loading"
      );
    !setupElem ||
      BrowserTestUtils.is_hidden(
        setupElem,
        "Setup content is not visible when loading"
      );
  } else {
    ok(
      !tabsContainer.classList.contains("loading"),
      "Tabs container has no loading class"
    );
    !loadingElem ||
      BrowserTestUtils.is_hidden(
        loadingElem,
        "Loading content is not visible when tabs are loaded"
      );
    BrowserTestUtils.is_visible(
      tabsList,
      "Synced tabs list is visible when loaded"
    );
    !setupElem ||
      BrowserTestUtils.is_hidden(
        setupElem,
        "Setup content is not visible when tabs are loaded"
      );
  }
}

function setupMocks(recentTabs, syncEnabled = true) {
  const sandbox = (gSandbox = setupRecentDeviceListMocks());
  sandbox.stub(SyncedTabs, "getRecentTabs").callsFake(() => {
    info(
      `SyncedTabs.getRecentTabs will return a promise resolving to ${recentTabs.length} tabs`
    );
    return Promise.resolve(recentTabs);
  });
  return sandbox;
}

add_setup(async function() {
  registerCleanupFunction(() => {
    // reset internal state so it doesn't affect the next tests
    TabsSetupFlowManager.resetInternalState();
  });

  await promiseSyncReady();
  // gSync.init() is called in a requestIdleCallback. Force its initialization.
  gSync.init();

  registerCleanupFunction(async function() {
    Services.prefs.clearUserPref("services.sync.engine.tabs");
    await tearDown(gSandbox);
  });
});

add_task(async function test_tab_sync_loading() {
  // empty synced tabs, so we're relying on tabs.changed or sync:finish notifications to clear the waiting state
  const recentTabsData = [];
  const sandbox = setupMocks(recentTabsData);
  // stub syncTabs so it resolves to true - meaning yes it will trigger a sync, which is the case
  // we want to cover in this test.
  sandbox.stub(SyncedTabs._internal, "syncTabs").resolves(true);

  await withFirefoxView({}, async browser => {
    Services.obs.notifyObservers(null, UIState.ON_UPDATE);

    const { document } = browser.contentWindow;
    const tabsContainer = document.querySelector("#tabpickup-tabs-container");

    await waitForElementVisible(browser, "#tabpickup-steps", false);
    await waitForElementVisible(browser, "#tabpickup-tabs-container", true);
    checkMobilePromo(browser, {
      mobilePromo: false,
      mobileConfirmation: false,
    });

    ok(TabsSetupFlowManager.waitingForTabs, "waitingForTabs is true");
    checkLoadingState(browser, true);

    Services.obs.notifyObservers(null, "services.sync.tabs.changed");

    await BrowserTestUtils.waitForMutationCondition(
      tabsContainer,
      { attributeFilter: ["class"], attributes: true },
      () => {
        return !tabsContainer.classList.contains("loading");
      }
    );
    checkLoadingState(browser, false);
    checkMobilePromo(browser, {
      mobilePromo: false,
      mobileConfirmation: false,
    });
  });
  await tearDown(sandbox);
});

add_task(async function test_tab_no_sync() {
  // Ensure we take down the waiting message if SyncedTabs determines it doesnt need to sync
  const recentTabsData = [];
  const sandbox = setupMocks(recentTabsData);
  // stub syncTabs so it resolves to false - meaning it will not trigger a sync, which is the case
  // we want to cover in this test.
  sandbox.stub(SyncedTabs._internal, "syncTabs").resolves(false);

  await withFirefoxView({}, async browser => {
    Services.obs.notifyObservers(null, UIState.ON_UPDATE);

    await waitForElementVisible(browser, "#tabpickup-steps", false);
    await waitForElementVisible(browser, "#tabpickup-tabs-container", true);

    ok(!TabsSetupFlowManager.waitingForTabs, "waitingForTabs is false");
    checkLoadingState(browser, false);
  });
  await tearDown(sandbox);
});

add_task(async function test_recent_tabs_loading() {
  // Simulate stale data by setting lastTabFetch to 10mins ago
  const TEN_MINUTES_MS = 1000 * 60 * 10;
  const staleFetchSeconds = Math.floor((Date.now() - TEN_MINUTES_MS) / 1000);
  info("updating lastFetch:" + staleFetchSeconds);
  Services.prefs.setIntPref("services.sync.lastTabFetch", staleFetchSeconds);

  // cached tabs data is available, so we shouldn't wait on lastTabFetch pref value
  const recentTabsData = structuredClone(syncedTabsData1[0].tabs);
  const sandbox = setupMocks(recentTabsData);

  await withFirefoxView({}, async browser => {
    Services.obs.notifyObservers(null, UIState.ON_UPDATE);

    await waitForElementVisible(browser, "#tabpickup-steps", false);
    await waitForElementVisible(browser, "#tabpickup-tabs-container", true);
    checkMobilePromo(browser, {
      mobilePromo: false,
      mobileConfirmation: false,
    });
    checkLoadingState(browser, false);
  });
  await tearDown(sandbox);
});
