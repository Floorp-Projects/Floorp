/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { UIState } = ChromeUtils.import("resource://services-sync/UIState.jsm");
const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");
const { TabsSetupFlowManager } = ChromeUtils.importESModule(
  "resource:///modules/firefox-view-tabs-setup-manager.sys.mjs"
);

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

/* eslint-disable no-unused-vars */
function testVisibility(browser, expected) {
  const { document } = browser.contentWindow;
  for (let [selector, shouldBeVisible] of Object.entries(
    expected.expectedVisible
  )) {
    const elem = document.querySelector(selector);
    if (shouldBeVisible) {
      ok(
        BrowserTestUtils.is_visible(elem),
        `Expected ${selector} to be visible`
      );
    } else {
      ok(BrowserTestUtils.is_hidden(elem), `Expected ${selector} to be hidden`);
    }
  }
}

async function waitForElementVisible(browser, selector, isVisible = true) {
  const { document } = browser.contentWindow;
  const elem = document.querySelector(selector);
  ok(elem, `Got element with selector: ${selector}`);

  await BrowserTestUtils.waitForMutationCondition(
    elem,
    {
      attributeFilter: ["hidden"],
    },
    () => {
      return isVisible
        ? BrowserTestUtils.is_visible(elem)
        : BrowserTestUtils.is_hidden(elem);
    }
  );
}

async function addFirefoxViewButtonToToolbar() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.tabs.firefox-view", true]],
  });
  info(Services.prefs.getBoolPref("browser.tabs.firefox-view", false));
  CustomizableUI.addWidgetToArea(
    "firefox-view-button",
    CustomizableUI.AREA_TABSTRIP,
    0
  );
}

function removeFirefoxViewButtonFromToolbar() {
  CustomizableUI.removeWidgetFromArea("firefox-view-button");
}

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

function getMockTabData(clients) {
  let tabs = [];

  for (let client of clients) {
    for (let tab of client.tabs) {
      tab.device = client.name;
      tab.deviceType = client.clientType;
    }
    tabs = [...tabs, ...client.tabs.reverse()];
  }
  tabs = tabs.sort((a, b) => b.lastUsed - a.lastUsed).slice(0, 3);

  return tabs;
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
