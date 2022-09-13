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

function cleanup() {
  Services.prefs.clearUserPref("services.sync.engine.tabs");
  Services.prefs.clearUserPref("services.sync.lastTabFetch");
}

registerCleanupFunction(async function() {
  cleanup();
});

add_task(async function test_tab_list_ordering() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;

      const sandbox = setupRecentDeviceListMocks();
      const syncedTabsMock = sandbox.stub(SyncedTabs, "getRecentTabs");
      let mockTabs1 = getMockTabData(syncedTabsData1);
      let mockTabs2 = getMockTabData(syncedTabsData2);
      syncedTabsMock.returns(mockTabs1);

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
      cleanup();
    }
  );
});

add_task(async function test_empty_list_items() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;

      const sandbox = setupRecentDeviceListMocks();
      const syncedTabsMock = sandbox.stub(SyncedTabs, "getRecentTabs");
      let mockTabs1 = getMockTabData(syncedTabsData3);
      let mockTabs2 = getMockTabData(syncedTabsData4);
      syncedTabsMock.returns(mockTabs1);

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
          syncedTabsList.firstChild.textContent.includes(
            "Firefox Privacy Notice"
          )
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
      cleanup();
    }
  );
});

add_task(async function test_empty_list() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;

      const sandbox = setupRecentDeviceListMocks();
      const syncedTabsMock = sandbox.stub(SyncedTabs, "getRecentTabs");
      let mockTabs1 = getMockTabData([]);
      let mockTabs2 = getMockTabData(syncedTabsData4);
      syncedTabsMock.returns(mockTabs1);

      await setupListState(browser);

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

      syncedTabsMock.returns(mockTabs2);
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
      cleanup();
    }
  );
});

add_task(async function test_time_updates_correctly() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.firefox-view.updateTimeMs", 100]],
  });
  await clearAllParentTelemetryEvents();

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;

      const sandbox = setupRecentDeviceListMocks();
      const syncedTabsMock = sandbox.stub(SyncedTabs, "getRecentTabs");
      let mockTabs1 = getMockTabData(syncedTabsData5);
      syncedTabsMock.returns(mockTabs1);

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

      gBrowser.removeTab(gBrowser.selectedTab);

      await clearAllParentTelemetryEvents();

      await waitForElementVisible(browser, "#tab-pickup-container > summary");
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
      cleanup();
      await SpecialPowers.popPrefEnv();
    }
  );
});
