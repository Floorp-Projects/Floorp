/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { UIState } = ChromeUtils.import("resource://services-sync/UIState.jsm");
const { TabsSetupFlowManager } = ChromeUtils.importESModule(
  "resource:///modules/firefox-view-tabs-setup-manager.sys.mjs"
);

const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

function promiseSyncReady() {
  let service = Cc["@mozilla.org/weave/service;1"].getService(Ci.nsISupports)
    .wrappedJSObject;
  return service.whenLoaded();
}

function setupMocks({ fxaDevices = null, state = UIState.STATUS_SIGNED_IN }) {
  const sandbox = sinon.createSandbox();
  sandbox.stub(fxAccounts.device, "recentDeviceList").get(() => fxaDevices);
  sandbox.stub(UIState, "get").returns({
    status: state,
    syncEnabled: true,
  });
  sandbox.stub(fxAccounts.device, "refreshDeviceList").resolves(true);

  sandbox
    .stub(Weave.Service.clientsEngine, "getClientByFxaDeviceId")
    .callsFake(fxaDeviceId => {
      let target = fxaDevices.find(c => c.id == fxaDeviceId);
      return target ? target.clientRecord : null;
    });
  sandbox.stub(Weave.Service.clientsEngine, "getClientType").returns("desktop");

  return sandbox;
}

async function waitForVisibleStep(browser, expected) {
  const { document } = browser.contentWindow;

  const deck = document.querySelector(".sync-setup-container");
  const nextStepElem = deck.querySelector(expected.expectedVisible);
  const stepElems = deck.querySelectorAll(".setup-step");

  await BrowserTestUtils.waitForMutationCondition(
    deck,
    {
      attributeFilter: ["selected-view"],
    },
    () => {
      return BrowserTestUtils.is_visible(nextStepElem);
    }
  );

  for (let elem of stepElems) {
    if (elem == nextStepElem) {
      ok(
        BrowserTestUtils.is_visible(elem),
        `Expected ${elem.id || elem.className} to be visible`
      );
    } else {
      ok(
        BrowserTestUtils.is_hidden(elem),
        `Expected ${elem.id || elem.className} to be hidden`
      );
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

add_setup(async function() {
  await promiseSyncReady();
  // gSync.init() is called in a requestIdleCallback. Force its initialization.
  gSync.init();

  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:firefoxview"
  );
  registerCleanupFunction(async function() {
    BrowserTestUtils.removeTab(tab);
    Services.prefs.clearUserPref("services.sync.engine.tabs");
    Services.prefs.clearUserPref("services.sync.lastTabFetch");
  });
  // set tab sync false so we don't skip setup states
  await SpecialPowers.pushPrefEnv({
    set: [["services.sync.engine.tabs", false]],
  });
});

add_task(async function test_unconfigured_initial_state() {
  const browser = gBrowser.selectedBrowser;
  const sandbox = setupMocks({ state: UIState.STATUS_NOT_CONFIGURED });

  Services.obs.notifyObservers(null, UIState.ON_UPDATE);
  await waitForVisibleStep(browser, {
    expectedVisible: "#tabpickup-steps-view0",
  });

  sandbox.restore();
});

add_task(async function test_signed_in() {
  const browser = gBrowser.selectedBrowser;
  const sandbox = setupMocks({
    state: UIState.STATUS_SIGNED_IN,
    fxaDevices: [
      {
        id: 1,
        name: "This Device",
        isCurrentDevice: true,
        type: "desktop",
      },
    ],
  });
  Services.obs.notifyObservers(null, UIState.ON_UPDATE);
  await waitForVisibleStep(browser, {
    expectedVisible: "#tabpickup-steps-view1",
  });

  is(fxAccounts.device.recentDeviceList?.length, 1, "Just 1 device connected");

  sandbox.restore();
});

add_task(async function test_2nd_desktop_connected() {
  const browser = gBrowser.selectedBrowser;
  const sandbox = setupMocks({
    state: UIState.STATUS_SIGNED_IN,
    fxaDevices: [
      {
        id: 1,
        name: "This Device",
        isCurrentDevice: true,
        type: "desktop",
      },
      {
        id: 2,
        name: "Other Device",
        type: "desktop",
      },
    ],
  });

  // ensure tab sync is false so we don't skip onto next step
  ok(
    !Services.prefs.getBoolPref("services.sync.engine.tabs", false),
    "services.sync.engine.tabs is initially false"
  );

  Services.obs.notifyObservers(null, UIState.ON_UPDATE);
  await waitForVisibleStep(browser, {
    expectedVisible: "#tabpickup-steps-view2",
  });

  is(fxAccounts.device.recentDeviceList?.length, 2, "2 devices connected");
  ok(
    fxAccounts.device.recentDeviceList?.every(
      device => device.type !== "mobile"
    ),
    "No connected device is type:mobile"
  );

  sandbox.restore();
});

add_task(async function test_mobile_connected() {
  const browser = gBrowser.selectedBrowser;
  const sandbox = setupMocks({
    state: UIState.STATUS_SIGNED_IN,
    fxaDevices: [
      {
        id: 1,
        name: "This Device",
        isCurrentDevice: true,
        type: "desktop",
      },
      {
        id: 2,
        name: "Other Device",
        type: "mobile",
      },
    ],
  });
  // ensure tab sync is false so we don't skip onto next step
  ok(
    !Services.prefs.getBoolPref("services.sync.engine.tabs", false),
    "services.sync.engine.tabs is initially false"
  );

  Services.obs.notifyObservers(null, UIState.ON_UPDATE);
  await waitForVisibleStep(browser, {
    expectedVisible: "#tabpickup-steps-view2",
  });

  is(fxAccounts.device.recentDeviceList?.length, 2, "2 devices connected");
  ok(
    fxAccounts.device.recentDeviceList?.some(
      device => device.type !== "mobile"
    ),
    "A connected device is type:mobile"
  );

  sandbox.restore();
});

add_task(async function test_tab_sync_enabled() {
  const browser = gBrowser.selectedBrowser;
  const sandbox = setupMocks({
    state: UIState.STATUS_SIGNED_IN,
    fxaDevices: [
      {
        id: 1,
        name: "This Device",
        isCurrentDevice: true,
        type: "desktop",
      },
      {
        id: 2,
        name: "Other Device",
        type: "mobile",
      },
    ],
  });
  Services.obs.notifyObservers(null, UIState.ON_UPDATE);

  // test initial state, with the pref not enabled
  await waitForVisibleStep(browser, {
    expectedVisible: "#tabpickup-steps-view2",
  });

  // test with the pref toggled on
  await SpecialPowers.pushPrefEnv({
    set: [["services.sync.engine.tabs", true]],
  });
  await waitForElementVisible(browser, "#tabpickup-steps", false);

  // reset and test clicking the action button
  await SpecialPowers.popPrefEnv();
  await waitForVisibleStep(browser, {
    expectedVisible: "#tabpickup-steps-view2",
  });

  const actionButton = browser.contentWindow.document.querySelector(
    "#tabpickup-steps-view2 button.primary"
  );
  actionButton.click();

  await waitForElementVisible(browser, "#tabpickup-steps", false);

  ok(
    Services.prefs.getBoolPref("services.sync.engine.tabs", false),
    "tab sync pref should be enabled after button click"
  );

  sandbox.restore();
  Services.prefs.clearUserPref("services.sync.engine.tabs");
});

add_task(async function test_tab_sync_loading() {
  const browser = gBrowser.selectedBrowser;
  const sandbox = setupMocks({
    state: UIState.STATUS_SIGNED_IN,
    fxaDevices: [
      {
        id: 1,
        name: "This Device",
        isCurrentDevice: true,
        type: "desktop",
      },
      {
        id: 2,
        name: "Other Device",
        type: "mobile",
      },
    ],
  });
  const { document } = browser.contentWindow;

  await SpecialPowers.pushPrefEnv({
    set: [["services.sync.engine.tabs", true]],
  });
  Services.obs.notifyObservers(null, UIState.ON_UPDATE);

  await waitForElementVisible(browser, "#tabpickup-steps", false);
  await waitForElementVisible(browser, "#tabpickup-tabs-container", true);

  const tabsContainer = document.querySelector("#tabpickup-tabs-container");
  const tabsList = document.querySelector(
    "#tabpickup-tabs-container tab-pickup-list"
  );
  const loadingElem = document.querySelector(
    "#tabpickup-tabs-container .loading-content"
  );
  const setupElem = document.querySelector("#tabpickup-steps");

  function checkLoadingState(isLoading = false) {
    if (isLoading) {
      ok(
        tabsContainer.classList.contains("loading"),
        "Tabs container has loading class"
      );
      BrowserTestUtils.is_visible(
        loadingElem,
        "Loading content is visible when loading"
      );
      BrowserTestUtils.is_hidden(
        tabsList,
        "Synced tabs list is not visible when loading"
      );
      BrowserTestUtils.is_hidden(
        setupElem,
        "Setup content is not visible when loading"
      );
    } else {
      ok(
        !tabsContainer.classList.contains("loading"),
        "Tabs container has no loading class"
      );
      BrowserTestUtils.is_hidden(
        loadingElem,
        "Loading content is not visible when tabs are loaded"
      );
      BrowserTestUtils.is_visible(
        tabsList,
        "Synced tabs list is visible when loaded"
      );
      BrowserTestUtils.is_hidden(
        setupElem,
        "Setup content is not visible when tabs are loaded"
      );
    }
  }
  checkLoadingState(true);

  // lastTabFetch stores a timestamp in *seconds*.
  const nowSeconds = Math.floor(Date.now() / 1000);
  info("updating lastFetch:" + nowSeconds);
  Services.prefs.setIntPref("services.sync.lastTabFetch", nowSeconds);

  await BrowserTestUtils.waitForMutationCondition(
    tabsContainer,
    { attributeFilter: ["class"], attributes: true },
    () => {
      return !tabsContainer.classList.contains("loading");
    }
  );
  checkLoadingState(false);

  // Simulate stale data by setting lastTabFetch to 10mins ago
  const TEN_MINUTES_MS = 1000 * 60 * 10;
  const staleFetchSeconds = Math.floor((Date.now() - TEN_MINUTES_MS) / 1000);
  info("updating lastFetch:" + staleFetchSeconds);
  Services.prefs.setIntPref("services.sync.lastTabFetch", staleFetchSeconds);

  await BrowserTestUtils.waitForMutationCondition(
    tabsContainer,
    { attributeFilter: ["class"], attributes: true },
    () => {
      return tabsContainer.classList.contains("loading");
    }
  );
  checkLoadingState(true);

  await SpecialPowers.popPrefEnv();
  sandbox.restore();
  Services.prefs.clearUserPref("services.sync.engine.tabs");
  Services.prefs.clearUserPref("services.sync.lastTabFetch");
});
