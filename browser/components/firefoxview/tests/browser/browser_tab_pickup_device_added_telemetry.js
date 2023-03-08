/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.defineESModuleGetters(globalThis, {
  SyncedTabs: "resource://services-sync/SyncedTabs.sys.mjs",
});

SimpleTest.requestCompleteLog();

registerCleanupFunction(async function() {
  await clearAllParentTelemetryEvents();
  cleanup_tab_pickup();
});

function setupWithFxaDevices() {
  const sandbox = setupSyncFxAMocks({
    state: UIState.STATUS_SIGNED_IN,
    fxaDevices: [
      {
        id: 1,
        name: "My desktop",
        isCurrentDevice: true,
        type: "desktop",
      },
      {
        id: 2,
        name: "Other device",
        isCurrentDevice: false,
        type: "mobile",
      },
    ],
  });
  return sandbox;
}

const mockDesktopTab1 = {
  client: "6c12bonqXZh8",
  device: "My desktop",
  deviceType: "desktop",
  type: "tab",
  title: "Example2",
  url: "https://example.com",
  icon: "https://example/favicon.png",
  lastUsed: Math.floor((Date.now() - 1000 * 60) / 1000), // This is one minute from now, which is below the threshold for 'Just now'
};

const mockDesktopTab2 = {
  client: "6c12bonqXZh8",
  device: "My desktop",
  deviceType: "desktop",
  type: "tab",
  title: "Sandboxes - Sinon.JS",
  url: "https://sinonjs.org/releases/latest/sandbox/",
  icon: "https://sinonjs.org/assets/images/favicon.png",
  lastUsed: 1655391592, // Thu Jun 16 2022 14:59:52 GMT+0000
};

const mockMobileTab1 = {
  client: "9d0y686hBXel",
  device: "My phone",
  deviceType: "mobile",
  type: "tab",
  title: "Element",
  url: "https://chat.mozilla.org/#room:mozilla.org",
  icon: "https://chat.mozilla.org/vector-icons/favicon.ico",
  lastUsed: 1664571288,
};

const NO_TABS_EVENTS = [
  ["firefoxview", "entered", "firefoxview", undefined],
  ["firefoxview", "synced_tabs", "tabs", undefined, { count: "0" }],
];
const SINGLE_TAB_EVENTS = [
  ["firefoxview", "entered", "firefoxview", undefined],
  ["firefoxview", "synced_tabs", "tabs", undefined, { count: "1" }],
];
const DEVICE_ADDED_NO_TABS_EVENTS = [
  ["firefoxview", "synced_tabs", "tabs", undefined, undefined],
  ["firefoxview", "synced_tabs_empty", "since_device_added", undefined],
];
const DEVICE_ADDED_TABS_EVENTS = [
  ["firefoxview", "synced_tabs", "tabs", undefined, undefined],
];

async function test_device_added({
  initialRecentTabsResult,
  expectedInitialTelementryEvents,
  expectedDeviceAddedTelementryEvents,
}) {
  const recentTabsResult = initialRecentTabsResult;
  await clearAllParentTelemetryEvents();
  const sandbox = setupWithFxaDevices();
  const syncedTabsMock = sandbox.stub(SyncedTabs, "getRecentTabs");

  syncedTabsMock.callsFake(() => {
    info(
      `Stubbed SyncedTabs.getRecentTabs returning a promise that resolves to ${recentTabsResult.length} tabs\n`
    );
    return Promise.resolve(recentTabsResult);
  });

  ok(
    !TabsSetupFlowManager.hasVisibleViews,
    "Initially hasVisibleViews is false"
  );
  is(
    TabsSetupFlowManager._viewVisibilityStates.size,
    0,
    "Initially, there are no visible views"
  );
  ok(
    !isFirefoxViewTabSelected(),
    "Before we call withFirefoxView, about:firefoxview tab is not selected"
  );

  await withFirefoxView({}, async browser => {
    info("inside withFirefoxView taskFn, waiting for setupListState");
    const { document } = browser.contentWindow;
    await setupListState(browser);
    info("setupListState finished");

    const isTablistVisible = !!initialRecentTabsResult.length;
    testVisibility(browser, {
      expectedVisible: {
        "ol.synced-tabs-list": isTablistVisible,
        "#synced-tabs-placeholder": !isTablistVisible,
      },
    });
    const syncedTabsItems = document.querySelectorAll(
      "ol.synced-tabs-list > li:not(.synced-tab-li-placeholder)"
    );
    info(
      "list items: " +
        Array.from(syncedTabsItems)
          .map(li => `li.${li.className}`)
          .join(", ")
    );
    is(
      syncedTabsItems.length,
      initialRecentTabsResult.length,
      `synced-tabs-list should have initial count of ${initialRecentTabsResult.length} non-placeholder list items`
    );

    // confirm telemetry is in expected state?
    info(
      "Checking telemetry against expectedInitialTelementryEvents: " +
        JSON.stringify(expectedInitialTelementryEvents, null, 2)
    );
    TelemetryTestUtils.assertEvents(
      expectedInitialTelementryEvents,
      { category: "firefoxview" },
      { clear: true, process: "parent" }
    );

    // add a new mock device
    info("Adding a new mock fxa dedvice");
    gMockFxaDevices.push({
      id: 1,
      name: "My primary phone",
      isCurrentDevice: false,
      type: "mobile",
    });

    // Notify of the newly added device
    const startWaitingSpy = sandbox.spy(
      TabsSetupFlowManager,
      "startWaitingForNewDeviceTabs"
    );

    info("Notifying devicelist_updated with the new mobile device");
    Services.obs.notifyObservers(null, "fxaccounts:devicelist_updated");
    info("Waiting for the startWaitingForNewDeviceTabs spy to be called");
    await TestUtils.waitForCondition(() => startWaitingSpy.called);

    is(
      startWaitingSpy.getCall(0).returnValue.constructor.name,
      "Promise",
      "startWaitingForNewDeviceTabs returned a promise"
    );

    // Some time passes here waiting for sync to get data from that device
    // we expect new-device handling to kick in. If there are 0 tabs we'll signal we're waiting,
    // create a timestamp and only clear it when there are > 0 tabs.
    // If there are already > 0 tabs, we'll basically do nothing, showing any new tabs when the arrive
    info("Waiting for the startWaitingForNewDeviceTabs promise to resolve");
    await startWaitingSpy.getCall(0).returnValue;

    info(
      "Initial tabs count: " +
        recentTabsResult.length +
        ", assert on waitingForTabs, TabsSetupFlowManager.waitingForTabs: " +
        TabsSetupFlowManager.waitingForTabs
    );
    is(
      TabsSetupFlowManager.waitingForTabs,
      !recentTabsResult.length,
      "Should be waiting if there were 0 tabs initially"
    );

    // Add tab data from this new device and notify of the changed data
    recentTabsResult.push(mockMobileTab1);
    const stopWaitingSpy = sandbox.spy(
      TabsSetupFlowManager,
      "stopWaitingForTabs"
    );

    info("Notifying tabs.changed with the new mobile device's tabs");
    Services.obs.notifyObservers(null, "services.sync.tabs.changed");

    // handling the tab.change and clearing the timestamp is necessarily async
    // as counting synced tabs via getRecentTabs() is async.
    // There may not be any outcome depending on the tab state, so we just wait
    // for stopWaitingForTabs to get called and its promise to resolve
    if (TabsSetupFlowManager.waitingForTabs) {
      // the setup manager will notify when we stop loading/waiting
      info("Waiting for setupstate.changed to be notified");
      await TestUtils.topicObserved("firefox-view.setupstate.changed");
    }
    info("Waiting for the stopWaitingSpy to be called");
    await TestUtils.waitForCondition(() => stopWaitingSpy.called);
    is(
      stopWaitingSpy.getCall(0).returnValue.constructor.name,
      "Promise",
      "stopWaitingTabs returned a promise"
    );
    info("Waiting for the stopWaiting promise to resolve");
    await stopWaitingSpy.getCall(0).returnValue;

    let snapshots = Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
      false
    );
    info(
      "Weve added a synced tab and updated the tab list, got snapshotEvents:" +
        JSON.stringify(snapshots, null, 2)
    );
    // confirm no telemetry was recorded for tabs from the newly-added device
    // as the tab list was never empty
    info(
      "Checking telemetry against expectedDeviceAddedTelementryEvents: " +
        JSON.stringify(expectedDeviceAddedTelementryEvents, null, 2)
    );
    TelemetryTestUtils.assertEvents(
      expectedDeviceAddedTelementryEvents,
      { category: "firefoxview" },
      { clear: true, process: "parent" }
    );
  });
  sandbox.restore();
  cleanup_tab_pickup();
}

add_task(async function test_device_added_with_existing_tabs() {
  /* Confirm that no telemetry is recorded when a new device is added while the synced tabs list has tabs */
  await test_device_added({
    initialRecentTabsResult: [mockDesktopTab1],
    expectedInitialTelementryEvents: SINGLE_TAB_EVENTS,
    expectedDeviceAddedTelementryEvents: DEVICE_ADDED_TABS_EVENTS,
  });
});

add_task(async function test_device_added_with_empty_list() {
  /* Confirm that telemetry is recorded when a device is added and the synced tabs list
     is empty until its tabs get synced
  */
  await test_device_added({
    initialRecentTabsResult: [],
    expectedInitialTelementryEvents: NO_TABS_EVENTS,
    expectedDeviceAddedTelementryEvents: DEVICE_ADDED_NO_TABS_EVENTS,
  });
});
