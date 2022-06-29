/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { UIState } = ChromeUtils.import("resource://services-sync/UIState.jsm");
const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

XPCOMUtils.defineLazyModuleGetters(globalThis, {
  SyncedTabs: "resource://services-sync/SyncedTabs.jsm",
});

const syncedTabsData1 = [
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
      {
        type: "tab",
        title: "Internet for people, not profits - Mozilla",
        url: "https://www.mozilla.org/",
        icon:
          "https://www.mozilla.org/media/img/favicons/mozilla/favicon.d25d81d39065.ico",
        lastUsed: 1655730486, // Mon Jun 20 2022 13:08:06 GMT+0000
      },
    ],
  },
  {
    id: 2,
    type: "client",
    name: "My iphone",
    clientType: "mobile",
    lastModified: 1655727832930,
    tabs: [
      {
        type: "tab",
        title: "The Guardian",
        url: "https://www.theguardian.com/",
        icon: "page-icon:https://www.theguardian.com/",
        lastUsed: 1655291890, // Wed Jun 15 2022 11:18:10 GMT+0000
      },
      {
        type: "tab",
        title: "The Times",
        url: "https://www.thetimes.co.uk/",
        icon: "page-icon:https://www.thetimes.co.uk/",
        lastUsed: 1655727485, // Mon Jun 20 2022 12:18:05 GMT+0000
      },
    ],
  },
];

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

function setupMocks(mockData1, mockData2) {
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

  const syncedTabsMock = sandbox.stub(SyncedTabs, "getTabClients");
  syncedTabsMock.onFirstCall().returns(mockData1);
  syncedTabsMock.onSecondCall().returns(mockData2);

  return sandbox;
}

async function setupListState(browser) {
  // Skip the synced tabs sign up flow to get to a loaded list state
  await SpecialPowers.pushPrefEnv({
    set: [["services.sync.engine.tabs", true]],
  });

  Services.obs.notifyObservers(null, UIState.ON_UPDATE);
  const recentFetchTime = Math.floor(Date.now() / 1000);
  info("updating lastFetch:" + recentFetchTime);
  Services.prefs.setIntPref("services.sync.lastTabFetch", recentFetchTime);

  await waitForElementVisible(browser, "#tabpickup-steps", false);
  await waitForElementVisible(browser, "#tabpickup-tabs-container", true);

  const tabsContainer = browser.contentWindow.document.querySelector(
    "#tabpickup-tabs-container"
  );
  await BrowserTestUtils.waitForMutationCondition(
    tabsContainer,
    { attributeFilter: ["class"], attributes: true },
    () => {
      return !tabsContainer.classList.contains("loading");
    }
  );
}

add_task(async function test_tab_list_ordering() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;

      const sandbox = setupMocks(syncedTabsData1, syncedTabsData2);
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
      Services.prefs.clearUserPref("services.sync.engine.tabs");
      Services.prefs.clearUserPref("services.sync.lastTabFetch");
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

      const sandbox = setupMocks(syncedTabsData3, syncedTabsData4);
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
      Services.prefs.clearUserPref("services.sync.engine.tabs");
      Services.prefs.clearUserPref("services.sync.lastTabFetch");
    }
  );
});
