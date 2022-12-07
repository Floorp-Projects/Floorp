/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { TabsSetupFlowManager } = ChromeUtils.importESModule(
  "resource:///modules/firefox-view-tabs-setup-manager.sys.mjs"
);

XPCOMUtils.defineLazyModuleGetters(globalThis, {
  SyncedTabs: "resource://services-sync/SyncedTabs.jsm",
});

const twoTabs = [
  {
    type: "tab",
    title: "Phabricator Home",
    url: "https://phabricator.services.mozilla.com/",
    icon: "https://phabricator.services.mozilla.com/favicon.d25d81d39065.ico",
    lastUsed: 1655745700, // Mon, 20 Jun 2022 17:21:40 GMT
  },
  {
    type: "tab",
    title: "Firefox Privacy Notice",
    url: "https://www.mozilla.org/en-US/privacy/firefox/",
    icon:
      "https://www.mozilla.org/media/img/favicons/mozilla/favicon.d25d81d39065.ico",
    lastUsed: 1655745700, // Mon, 20 Jun 2022 17:21:40 GMT
  },
];
const syncedTabsData2 = structuredClone(syncedTabsData1);
syncedTabsData2[1].tabs = [...syncedTabsData2[1].tabs, ...twoTabs];

const syncedTabsData3 = [
  {
    id: 1,
    type: "client",
    name: "My desktop",
    clientType: "desktop",
    lastModified: 1655730486760,
    tabs: [
      {
        type: "tab",
        title: "Sandboxes - Sinon.JS",
        url: "https://sinonjs.org/releases/latest/sandbox/",
        icon: "https://sinonjs.org/assets/images/favicon.png",
        lastUsed: 1655391592, // Thu Jun 16 2022 14:59:52 GMT+0000
      },
    ],
  },
];

const syncedTabsData4 = structuredClone(syncedTabsData3);
syncedTabsData4[0].tabs = [...syncedTabsData4[0].tabs, ...twoTabs];

const syncedTabsData5 = [
  {
    id: 1,
    type: "client",
    name: "My desktop",
    clientType: "desktop",
    lastModified: Date.now(),
    tabs: [
      {
        type: "tab",
        title: "Example2",
        url: "https://example.com",
        icon: "https://example/favicon.png",
        lastUsed: Math.floor((Date.now() - 1000 * 60) / 1000), // This is one minute from now, which is below the threshold for 'Just now'
      },
    ],
  },
];

const NO_TABS_EVENTS = [
  ["firefoxview", "entered", "firefoxview", undefined],
  ["firefoxview", "synced_tabs", "tabs", undefined, { count: "0" }],
];

const TAB_PICKUP_EVENT = [
  ["firefoxview", "entered", "firefoxview", undefined],
  ["firefoxview", "synced_tabs", "tabs", undefined, { count: "1" }],
  [
    "firefoxview",
    "tab_pickup",
    "tabs",
    undefined,
    { position: "1", deviceType: "desktop" },
  ],
];

const TAB_PICKUP_OPEN_EVENT = [
  ["firefoxview", "tab_pickup_open", "tabs", "false"],
];

registerCleanupFunction(async function() {
  cleanup_tab_pickup();
});

add_task(async function test_tab_list_ordering() {
  const sandbox = setupRecentDeviceListMocks();
  const syncedTabsMock = sandbox.stub(SyncedTabs, "getRecentTabs");
  let mockTabs1 = getMockTabData(syncedTabsData1);
  let mockTabs2 = getMockTabData(syncedTabsData2);
  syncedTabsMock.callsFake(() => {
    info(
      `Stubbed SyncedTabs.getRecentTabs returning a promise that resolves to ${mockTabs1.length} tabs\n`
    );
    return Promise.resolve(mockTabs1);
  });

  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;

    await setupListState(browser);

    testVisibility(browser, {
      expectedVisible: {
        "ol.synced-tabs-list": true,
      },
    });

    ok(
      document.querySelector("ol.synced-tabs-list").children.length === 3,
      "synced-tabs-list should have three list items"
    );

    ok(
      document
        .querySelector("ol.synced-tabs-list")
        .firstChild.textContent.includes("Internet for people, not profits"),
      "First list item in synced-tabs-list is in the correct order"
    );

    ok(
      document
        .querySelector("ol.synced-tabs-list")
        .children[2].textContent.includes("Sandboxes - Sinon.JS"),
      "Last list item in synced-tabs-list is in the correct order"
    );

    syncedTabsMock.returns(mockTabs2);
    // Initiate a synced tabs update
    Services.obs.notifyObservers(null, "services.sync.tabs.changed");

    const syncedTabsList = document.querySelector("ol.synced-tabs-list");
    // first list item has been updated
    await BrowserTestUtils.waitForMutationCondition(
      syncedTabsList,
      { childList: true },
      () => syncedTabsList.firstChild.textContent.includes("Firefox")
    );

    ok(
      document.querySelector("ol.synced-tabs-list").children.length === 3,
      "Synced-tabs-list should still have three list items"
    );

    ok(
      document
        .querySelector("ol.synced-tabs-list")
        .children[1].textContent.includes("Phabricator"),
      "Second list item in synced-tabs-list has been updated"
    );

    ok(
      document
        .querySelector("ol.synced-tabs-list")
        .children[2].textContent.includes("Internet for people, not profits"),
      "Last list item in synced-tabs-list has been updated"
    );

    sandbox.restore();
    cleanup_tab_pickup();
  });
});

add_task(async function test_empty_list_items() {
  const sandbox = setupRecentDeviceListMocks();
  const syncedTabsMock = sandbox.stub(SyncedTabs, "getRecentTabs");
  let mockTabs1 = getMockTabData(syncedTabsData3);
  let mockTabs2 = getMockTabData(syncedTabsData4);
  syncedTabsMock.callsFake(() => {
    info(
      `Stubbed SyncedTabs.getRecentTabs returning a promise that resolves to ${mockTabs1.length} tabs\n`
    );
    return Promise.resolve(mockTabs1);
  });

  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;

    await setupListState(browser);

    testVisibility(browser, {
      expectedVisible: {
        "ol.synced-tabs-list": true,
      },
    });

    ok(
      document.querySelector("ol.synced-tabs-list").children.length === 3,
      "synced-tabs-list should have three list items"
    );

    ok(
      document
        .querySelector("ol.synced-tabs-list")
        .firstChild.textContent.includes("Sandboxes - Sinon.JS"),
      "First list item in synced-tabs-list is in the correct order"
    );

    ok(
      document
        .querySelector("ol.synced-tabs-list")
        .children[1].classList.contains("synced-tab-li-placeholder"),
      "Second list item in synced-tabs-list should be a placeholder"
    );

    ok(
      document
        .querySelector("ol.synced-tabs-list")
        .lastChild.classList.contains("synced-tab-li-placeholder"),
      "Last list item in synced-tabs-list should be a placeholder"
    );

    syncedTabsMock.returns(mockTabs2);
    // Initiate a synced tabs update
    Services.obs.notifyObservers(null, "services.sync.tabs.changed");

    const syncedTabsList = document.querySelector("ol.synced-tabs-list");
    // first list item has been updated
    await BrowserTestUtils.waitForMutationCondition(
      syncedTabsList,
      { childList: true },
      () =>
        syncedTabsList.firstChild.textContent.includes("Firefox Privacy Notice")
    );

    ok(
      document
        .querySelector("ol.synced-tabs-list")
        .children[1].textContent.includes("Phabricator"),
      "Second list item in synced-tabs-list has been updated"
    );

    ok(
      document
        .querySelector("ol.synced-tabs-list")
        .lastChild.textContent.includes("Sandboxes - Sinon.JS"),
      "Last list item in synced-tabs-list has been updated"
    );

    sandbox.restore();
    cleanup_tab_pickup();
  });
});

add_task(async function test_empty_list() {
  await clearAllParentTelemetryEvents();
  const sandbox = setupRecentDeviceListMocks();
  const syncedTabsMock = sandbox.stub(SyncedTabs, "getRecentTabs");
  let mockTabs1 = getMockTabData([]);
  let mockTabs2 = getMockTabData(syncedTabsData4);
  syncedTabsMock.callsFake(() => {
    info(
      `Stubbed SyncedTabs.getRecentTabs returning a promise that resolves to ${mockTabs1.length} tabs\n`
    );
    return Promise.resolve(mockTabs1);
  });

  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;

    await setupListState(browser);
    info("setupListState complete, checking placeholder and list visibility");
    testVisibility(browser, {
      expectedVisible: {
        "#synced-tabs-placeholder": true,
        "ol.synced-tabs-list": false,
      },
    });

    ok(
      document
        .querySelector("#synced-tabs-placeholder")
        .classList.contains("empty-container"),
      "collapsible container should have correct styling when the list is empty"
    );

    await TestUtils.waitForCondition(
      () => {
        let events = Services.telemetry.snapshotEvents(
          Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
          false
        ).parent;
        return events && events.length >= 2;
      },
      "Waiting for entered and synced_tabs firefoxview telemetry events.",
      200,
      100
    );

    TelemetryTestUtils.assertEvents(
      NO_TABS_EVENTS,
      { category: "firefoxview" },
      { clear: true, process: "parent" }
    );

    syncedTabsMock.callsFake(() => {
      info(
        `Stubbed SyncedTabs.getRecentTabs returning a promise that resolves to ${mockTabs2.length} tabs\n`
      );
      return Promise.resolve(mockTabs2);
    });
    // Initiate a synced tabs update
    Services.obs.notifyObservers(null, "services.sync.tabs.changed");

    const syncedTabsList = document.querySelector("ol.synced-tabs-list");
    await BrowserTestUtils.waitForMutationCondition(
      syncedTabsList,
      { childList: true },
      () => syncedTabsList.children.length
    );

    testVisibility(browser, {
      expectedVisible: {
        "#synced-tabs-placeholder": false,
        "ol.synced-tabs-list": true,
      },
    });

    sandbox.restore();
    cleanup_tab_pickup();
  });
});

add_task(async function test_time_updates_correctly() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.firefox-view.updateTimeMs", 100]],
  });
  await clearAllParentTelemetryEvents();

  const sandbox = setupRecentDeviceListMocks();
  const syncedTabsMock = sandbox.stub(SyncedTabs, "getRecentTabs");
  let mockTabs1 = getMockTabData(syncedTabsData5);
  syncedTabsMock.callsFake(() => {
    info(
      `Stubbed SyncedTabs.getRecentTabs returning a promise that resolves to ${mockTabs1.length} tabs\n`
    );
    return Promise.resolve(mockTabs1);
  });

  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;

    await setupListState(browser);

    let initialTimeText = document.querySelector("span.synced-tab-li-time")
      .textContent;
    Assert.stringContains(
      initialTimeText,
      "Just now",
      "synced-tab-li-time text is 'Just now'"
    );

    await SpecialPowers.pushPrefEnv({
      set: [["browser.tabs.firefox-view.updateTimeMs", 100]],
    });

    const timeLabel = document.querySelector("span.synced-tab-li-time");
    await BrowserTestUtils.waitForMutationCondition(
      timeLabel,
      { childList: true },
      () => !timeLabel.textContent.includes("now")
    );

    isnot(
      timeLabel.textContent,
      initialTimeText,
      "synced-tab-li-time text has updated"
    );

    document.querySelector(".synced-tab-a").click();

    await TestUtils.waitForCondition(
      () => {
        let events = Services.telemetry.snapshotEvents(
          Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
          false
        ).parent;
        return events && events.length >= 3;
      },
      "Waiting for entered, synced_tabs, and tab_pickup firefoxview telemetry events.",
      200,
      100
    );

    TelemetryTestUtils.assertEvents(
      TAB_PICKUP_EVENT,
      { category: "firefoxview" },
      { clear: true, process: "parent" }
    );

    let gBrowser = browser.getTabBrowser();
    is(
      gBrowser.visibleTabs.indexOf(gBrowser.selectedTab),
      0,
      "Tab opened at the beginning of the tab strip"
    );
    gBrowser.removeTab(gBrowser.selectedTab);
    // make sure we're back on fx-view
    browser.ownerGlobal.FirefoxViewHandler.openTab();

    info("Waiting for the tab pickup summary to be visible");
    await waitForElementVisible(browser, "#tab-pickup-container > summary");
    // click on the details summary and verify telemetry gets logged for this event
    await clearAllParentTelemetryEvents();
    info("clicking the summary to collapse it");
    document.querySelector("#tab-pickup-container > summary").click();

    await TestUtils.waitForCondition(
      () => {
        let events = Services.telemetry.snapshotEvents(
          Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
          false
        ).parent;
        return events && events.length >= 1;
      },
      "Waiting for tab_pickup_open firefoxview telemetry event.",
      200,
      100
    );
    TelemetryTestUtils.assertEvents(
      TAB_PICKUP_OPEN_EVENT,
      { category: "firefoxview" },
      { clear: true, process: "parent" }
    );

    sandbox.restore();
    cleanup_tab_pickup();
    await SpecialPowers.popPrefEnv();
  });
});

/**
 * Ensure that tabs sync when a user reloads Firefox View.
 * This is accomplished by asserting that a new set of tabs are loaded
 * on page reload.
 */
add_task(async function test_tabs_sync_on_user_page_reload() {
  const sandbox = setupRecentDeviceListMocks();
  sandbox.stub(SyncedTabs._internal, "syncTabs").resolves(true);
  const syncedTabsMock = sandbox.stub(SyncedTabs, "getRecentTabs");
  let mockTabs1 = getMockTabData(syncedTabsData1);
  let expectedTabsAfterReload = getMockTabData(syncedTabsData3);
  syncedTabsMock.callsFake(() => {
    info(
      `Stubbed SyncedTabs.getRecentTabs returning a promise that resolves to ${mockTabs1.length} tabs\n`
    );
    return Promise.resolve(mockTabs1);
  });

  await withFirefoxView({}, async browser => {
    let reloadButton = browser.ownerDocument.getElementById("reload-button");

    await setupListState(browser);

    let tabLoaded = BrowserTestUtils.browserLoaded(browser);
    EventUtils.synthesizeMouseAtCenter(reloadButton, {}, browser.ownerGlobal);
    await tabLoaded;
    // Wait until the window is reloaded, then get the current instance
    // of the contentWindow
    const { document } = browser.contentWindow;
    ok(true, "Firefox View has been reloaded");
    ok(TabsSetupFlowManager.waitingForTabs, "waitingForTabs is true");

    syncedTabsMock.returns(expectedTabsAfterReload);
    Services.obs.notifyObservers(null, "services.sync.tabs.changed");
    ok(!TabsSetupFlowManager.waitingForTabs, "waitingForTabs is false");

    const syncedTabsList = document.querySelector("ol.synced-tabs-list");
    // The tab pickup list has been updated
    await BrowserTestUtils.waitForMutationCondition(
      syncedTabsList,
      { childList: true },
      () =>
        syncedTabsList.firstChild.textContent.includes("Sandboxes - Sinon.JS")
    );

    sandbox.restore();
    cleanup_tab_pickup();
  });
});

add_task(async function test_keyboard_navigation() {
  // Setting this pref allows the test to run as expected on MacOS
  await SpecialPowers.pushPrefEnv({ set: [["accessibility.tabfocus", 7]] });
  TabsSetupFlowManager.resetInternalState();

  const sandbox = setupRecentDeviceListMocks();
  const syncedTabsMock = sandbox.stub(SyncedTabs, "getRecentTabs");
  let mockTabs1 = getMockTabData(syncedTabsData1);
  syncedTabsMock.callsFake(() => {
    info(
      `Stubbed SyncedTabs.getRecentTabs returning a promise that resolves to ${mockTabs1.length} tabs\n`
    );
    return Promise.resolve(mockTabs1);
  });

  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    let win = browser.ownerGlobal;

    await setupListState(browser);
    const tab = (shiftKey = false) => {
      info(`${shiftKey ? "Shift + Tab" : "Tab"}`);
      EventUtils.synthesizeKey("KEY_Tab", { shiftKey }, win);
    };
    const arrowDown = () => {
      info("Arrow Down");
      EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
    };
    const arrowUp = () => {
      info("Arrow Up");
      EventUtils.synthesizeKey("KEY_ArrowUp", {}, win);
    };
    const arrowLeft = () => {
      info("Arrow Left");
      EventUtils.synthesizeKey("KEY_ArrowLeft", {}, win);
    };
    const arrowRight = () => {
      info("Arrow Right");
      EventUtils.synthesizeKey("KEY_ArrowRight", {}, win);
    };

    let syncedTabsLinks = document
      .querySelector("ol.synced-tabs-list")
      .querySelectorAll("a");
    let summary = document
      .getElementById("tab-pickup-container")
      .querySelector("summary");
    summary.focus();
    tab();
    is(
      syncedTabsLinks[0],
      document.activeElement,
      "First synced tab should be focused"
    );
    arrowDown();
    is(
      syncedTabsLinks[1],
      document.activeElement,
      "Second synced tab should be focused"
    );
    arrowDown();
    is(
      syncedTabsLinks[2],
      document.activeElement,
      "Third synced tab should be focused"
    );
    arrowDown();
    is(
      syncedTabsLinks[2],
      document.activeElement,
      "Third synced tab should still be focused"
    );
    arrowUp();
    is(
      syncedTabsLinks[1],
      document.activeElement,
      "Second synced tab should be focused"
    );
    arrowLeft();
    is(
      syncedTabsLinks[0],
      document.activeElement,
      "First synced tab should be focused"
    );
    arrowRight();
    is(
      syncedTabsLinks[1],
      document.activeElement,
      "Second synced tab should be focused"
    );
    arrowDown();
    is(
      syncedTabsLinks[2],
      document.activeElement,
      "Third synced tab should be focused"
    );
    arrowLeft();
    is(
      syncedTabsLinks[0],
      document.activeElement,
      "First synced tab should be focused"
    );

    tab(true);
    is(
      summary,
      document.activeElement,
      "Summary element should be focused when shift tabbing away from list"
    );

    sandbox.restore();
    cleanup_tab_pickup();
  });
});
