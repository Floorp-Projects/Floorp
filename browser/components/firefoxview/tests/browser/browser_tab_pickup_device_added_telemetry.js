/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

registerCleanupFunction(async function () {
  await clearAllParentTelemetryEvents();
  cleanup_tab_pickup();
});

function setupWithFxaDevices() {
  const sandbox = (gSandbox = setupSyncFxAMocks({
    state: UIState.STATUS_SIGNED_IN,
    fxaDevices: [
      {
        id: 1,
        name: "My desktop",
        isCurrentDevice: true,
        type: "desktop",
        tabs: [],
      },
      {
        id: 2,
        name: "Other device",
        isCurrentDevice: false,
        type: "mobile",
        tabs: [],
      },
    ],
  }));
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
  ["firefoxview", "synced_tabs", "tabs", undefined, { count: "0" }],
];
const SINGLE_TAB_EVENTS = [
  ["firefoxview", "synced_tabs", "tabs", undefined, { count: "1" }],
];
const DEVICE_ADDED_NO_TABS_EVENTS = [
  ["firefoxview", "synced_tabs", "tabs", undefined, undefined],
  ["firefoxview", "synced_tabs_empty", "since_device_added", undefined],
];
const DEVICE_ADDED_TABS_EVENTS = [
  ["firefoxview", "synced_tabs", "tabs", undefined, undefined],
];

async function whenResolved(functionSpy, functionLabel) {
  info(`Waiting for ${functionLabel} to be called`);
  await TestUtils.waitForCondition(
    () => functionSpy.called,
    `Waiting for ${functionLabel} to be called`
  );
  is(
    functionSpy.getCall(0).returnValue.constructor.name,
    "Promise",
    `${functionLabel} returned a promise`
  );
  info(`Waiting for the promise returned by ${functionLabel} to be resolved`);
  await functionSpy.getCall(0).returnValue;
  info(`${functionLabel} promise resolved`);
}

async function test_device_added({
  initialRecentTabsResult,
  expectedInitialTelementryEvents,
  expectedDeviceAddedTelementryEvents,
}) {
  const recentTabsResult = initialRecentTabsResult;
  const sandbox = setupWithFxaDevices();
  const syncedTabsMock = sandbox.stub(SyncedTabs, "getRecentTabs");

  syncedTabsMock.callsFake(() => {
    info(
      `Stubbed SyncedTabs.getRecentTabs returning a promise that resolves to ${recentTabsResult.length} tabs\n`
    );
    return Promise.resolve(recentTabsResult);
  });

  ok(
    !isFirefoxViewTabSelected(),
    "Before we call withFirefoxView, about:firefoxview tab is not selected"
  );
  ok(
    !TabsSetupFlowManager.hasVisibleViews,
    "Initially hasVisibleViews is false"
  );

  await withFirefoxView({}, async browser => {
    info("inside withFirefoxView taskFn, waiting for setupListState");
    const { document } = browser.contentWindow;
    const stopWaitingSpy = sandbox.spy(
      TabsSetupFlowManager,
      "stopWaitingForTabs"
    );
    const signedInChangeSpy = sandbox.spy(
      TabsSetupFlowManager,
      "onSignedInChange"
    );
    await clearAllParentTelemetryEvents();
    await setupListState(browser);
    info("setupListState finished");

    // ensure any tab syncs triggered by Fxa sign-in are complete before proceeding
    await whenResolved(signedInChangeSpy, "onSignedInChange");
    if (!recentTabsResult.length) {
      info("No synced tabs so we wait for the result of the sync we trigger");
      await whenResolved(stopWaitingSpy, "stopWaitingForTabs");
      info("stopWaitingForTabs finished");
    }

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

    const startWaitingSpy = sandbox.spy(
      TabsSetupFlowManager,
      "startWaitingForNewDeviceTabs"
    );
    // Notify of the newly added device
    info("Notifying devicelist_updated with the new mobile device");
    Services.obs.notifyObservers(null, "fxaccounts:devicelist_updated");

    // Some time passes here waiting for sync to get data from that device
    // we expect new-device handling to kick in. If there are 0 tabs we'll signal we're waiting,
    // create a timestamp and only clear it when there are > 0 tabs.
    // If there are already > 0 tabs, we'll basically do nothing, showing any new tabs when they arrive
    await whenResolved(startWaitingSpy, "startWaitingForNewDeviceTabs");

    info(
      "Initial tabs count: " +
        recentTabsResult.length +
        ", assert on _noTabsVisibleFromAddedDeviceTimestamp: " +
        TabsSetupFlowManager._noTabsVisibleFromAddedDeviceTimestamp
    );
    if (recentTabsResult.length) {
      ok(
        !TabsSetupFlowManager._noTabsVisibleFromAddedDeviceTimestamp,
        "Should not be waiting if there were > 0 tabs initially"
      );
    } else {
      ok(
        TabsSetupFlowManager._noTabsVisibleFromAddedDeviceTimestamp,
        "Should be waiting if there were 0 tabs initially"
      );
    }

    // Add tab data from this new device and notify of the changed data
    recentTabsResult.push(mockMobileTab1);
    stopWaitingSpy.resetHistory();

    info("Notifying tabs.changed with the new mobile device's tabs");
    Services.obs.notifyObservers(null, "services.sync.tabs.changed");

    // handling the tab.change and clearing the timestamp is necessarily async
    // as counting synced tabs via getRecentTabs() is async.
    // There may not be any outcome depending on the tab state, so we just wait
    // for stopWaitingForTabs to get called and its promise to resolve
    info("Waiting for the stopWaitingSpy to be called");
    await whenResolved(stopWaitingSpy, "stopWaitingForTabs");
    await TestUtils.waitForTick(); // allow time for the telemetry event to get recorded

    info(
      "We've added a synced tab and updated the tab list, got snapshotEvents:" +
        JSON.stringify(
          Services.telemetry.snapshotEvents(
            Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
            false
          ),
          null,
          2
        )
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
