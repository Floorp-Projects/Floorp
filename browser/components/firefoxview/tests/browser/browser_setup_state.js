/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { TabsSetupFlowManager } = ChromeUtils.importESModule(
  "resource:///modules/firefox-view-tabs-setup-manager.sys.mjs"
);

const MOBILE_PROMO_DISMISSED_PREF =
  "browser.tabs.firefox-view.mobilePromo.dismissed";

const FXA_CONTINUE_EVENT = [
  ["firefoxview", "entered", "firefoxview", undefined],
  ["firefoxview", "fxa_continue", "sync", undefined],
];

const FXA_MOBILE_EVENT = [
  ["firefoxview", "entered", "firefoxview", undefined],
  ["firefoxview", "fxa_mobile", "sync", undefined, { has_devices: "false" }],
];

var gMockFxaDevices = null;
var gUIStateStatus;

function promiseSyncReady() {
  let service = Cc["@mozilla.org/weave/service;1"].getService(Ci.nsISupports)
    .wrappedJSObject;
  return service.whenLoaded();
}

async function touchLastTabFetch() {
  // lastTabFetch stores a timestamp in *seconds*.
  const nowSeconds = Math.floor(Date.now() / 1000);
  info("updating lastFetch:" + nowSeconds);
  Services.prefs.setIntPref("services.sync.lastTabFetch", nowSeconds);
  // wait so all pref observers can complete
  await TestUtils.waitForTick();
}

var gSandbox;
function setupMocks({ fxaDevices = null, state, syncEnabled = true }) {
  gUIStateStatus = state || UIState.STATUS_SIGNED_IN;
  if (gSandbox) {
    gSandbox.restore();
  }
  const sandbox = (gSandbox = sinon.createSandbox());
  gMockFxaDevices = fxaDevices;
  sandbox.stub(fxAccounts.device, "recentDeviceList").get(() => fxaDevices);
  sandbox.stub(UIState, "get").callsFake(() => {
    return {
      status: gUIStateStatus,
      syncEnabled,
    };
  });
  sandbox
    .stub(Weave.Service.clientsEngine, "getClientByFxaDeviceId")
    .callsFake(fxaDeviceId => {
      let target = gMockFxaDevices.find(c => c.id == fxaDeviceId);
      return target ? target.clientRecord : null;
    });
  sandbox.stub(Weave.Service.clientsEngine, "getClientType").returns("desktop");

  return sandbox;
}

async function setupWithDesktopDevices() {
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

  await SpecialPowers.pushPrefEnv({
    set: [["services.sync.engine.tabs", true]],
  });
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

function checkMobilePromo(browser, expected = {}) {
  const { document } = browser.contentWindow;
  const promoElem = document.querySelector(
    "#tab-pickup-container > .promo-box"
  );
  const successElem = document.querySelector(
    "#tab-pickup-container > .confirmation-message-box"
  );

  info("checkMobilePromo: " + JSON.stringify(expected));
  if (expected.mobilePromo) {
    ok(BrowserTestUtils.is_visible(promoElem), "Mobile promo is visible");
  } else {
    ok(
      !promoElem || BrowserTestUtils.is_hidden(promoElem),
      "Mobile promo is hidden"
    );
  }
  if (expected.mobileConfirmation) {
    ok(
      BrowserTestUtils.is_visible(successElem),
      "Success confirmation is visible"
    );
  } else {
    ok(
      !successElem || BrowserTestUtils.is_hidden(successElem),
      "Success confirmation is hidden"
    );
  }
}

async function tearDown(sandbox) {
  sandbox?.restore();
  Services.prefs.clearUserPref("services.sync.lastTabFetch");
  Services.prefs.clearUserPref(MOBILE_PROMO_DISMISSED_PREF);
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
  // set tab sync false so we don't skip setup states
  await SpecialPowers.pushPrefEnv({
    set: [["services.sync.engine.tabs", false]],
  });
});

add_task(async function test_unconfigured_initial_state() {
  await clearAllParentTelemetryEvents();
  const sandbox = setupMocks({
    state: UIState.STATUS_NOT_CONFIGURED,
    syncEnabled: false,
  });
  await withFirefoxView({}, async browser => {
    Services.obs.notifyObservers(null, UIState.ON_UPDATE);
    await waitForVisibleSetupStep(browser, {
      expectedVisible: "#tabpickup-steps-view1",
    });
    checkMobilePromo(browser, {
      mobilePromo: false,
      mobileConfirmation: false,
    });

    await BrowserTestUtils.synthesizeMouseAtCenter(
      'button[data-action="view1-primary-action"]',
      {},
      browser
    );

    await TestUtils.waitForCondition(
      () => {
        let events = Services.telemetry.snapshotEvents(
          Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
          false
        ).parent;
        return events && events.length >= 2;
      },
      "Waiting for entered and fxa_continue firefoxview telemetry events.",
      200,
      100
    );

    TelemetryTestUtils.assertEvents(
      FXA_CONTINUE_EVENT,
      { category: "firefoxview" },
      { clear: true, process: "parent" }
    );
  });
  await tearDown(sandbox);
});

add_task(async function test_signed_in() {
  await clearAllParentTelemetryEvents();
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

  await withFirefoxView({}, async browser => {
    Services.obs.notifyObservers(null, UIState.ON_UPDATE);
    await waitForVisibleSetupStep(browser, {
      expectedVisible: "#tabpickup-steps-view2",
    });

    is(
      fxAccounts.device.recentDeviceList?.length,
      1,
      "Just 1 device connected"
    );
    checkMobilePromo(browser, {
      mobilePromo: false,
      mobileConfirmation: false,
    });

    await BrowserTestUtils.synthesizeMouseAtCenter(
      'button[data-action="view2-primary-action"]',
      {},
      browser
    );

    await TestUtils.waitForCondition(
      () => {
        let events = Services.telemetry.snapshotEvents(
          Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
          false
        ).parent;
        return events && events.length >= 2;
      },
      "Waiting for entered and fxa_mobile firefoxview telemetry events.",
      200,
      100
    );

    TelemetryTestUtils.assertEvents(
      FXA_MOBILE_EVENT,
      { category: "firefoxview" },
      { clear: true, process: "parent" }
    );
  });
  await tearDown(sandbox);
});

add_task(async function test_2nd_desktop_connected() {
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
  await withFirefoxView({}, async browser => {
    // ensure tab sync is false so we don't skip onto next step
    ok(
      !Services.prefs.getBoolPref("services.sync.engine.tabs", false),
      "services.sync.engine.tabs is initially false"
    );

    Services.obs.notifyObservers(null, UIState.ON_UPDATE);
    await waitForVisibleSetupStep(browser, {
      expectedVisible: "#tabpickup-steps-view3",
    });

    is(fxAccounts.device.recentDeviceList?.length, 2, "2 devices connected");
    ok(
      fxAccounts.device.recentDeviceList?.every(
        device => device.type !== "mobile"
      ),
      "No connected device is type:mobile"
    );
    checkMobilePromo(browser, {
      mobilePromo: false,
      mobileConfirmation: false,
    });
  });
  await tearDown(sandbox);
});

add_task(async function test_mobile_connected() {
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
  await withFirefoxView({}, async browser => {
    // ensure tab sync is false so we don't skip onto next step
    ok(
      !Services.prefs.getBoolPref("services.sync.engine.tabs", false),
      "services.sync.engine.tabs is initially false"
    );

    Services.obs.notifyObservers(null, UIState.ON_UPDATE);
    await waitForVisibleSetupStep(browser, {
      expectedVisible: "#tabpickup-steps-view3",
    });

    is(fxAccounts.device.recentDeviceList?.length, 2, "2 devices connected");
    ok(
      fxAccounts.device.recentDeviceList?.some(
        device => device.type !== "mobile"
      ),
      "A connected device is type:mobile"
    );
    checkMobilePromo(browser, {
      mobilePromo: false,
      mobileConfirmation: false,
    });
  });
  await tearDown(sandbox);
});

add_task(async function test_tab_sync_enabled() {
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
  await withFirefoxView({}, async browser => {
    Services.obs.notifyObservers(null, UIState.ON_UPDATE);

    // test initial state, with the pref not enabled
    await waitForVisibleSetupStep(browser, {
      expectedVisible: "#tabpickup-steps-view3",
    });
    checkMobilePromo(browser, {
      mobilePromo: false,
      mobileConfirmation: false,
    });

    // test with the pref toggled on
    await SpecialPowers.pushPrefEnv({
      set: [["services.sync.engine.tabs", true]],
    });
    await waitForElementVisible(browser, "#tabpickup-steps", false);
    checkMobilePromo(browser, {
      mobilePromo: false,
      mobileConfirmation: false,
    });

    // reset and test clicking the action button
    await SpecialPowers.popPrefEnv();
    await waitForVisibleSetupStep(browser, {
      expectedVisible: "#tabpickup-steps-view3",
    });
    checkMobilePromo(browser, {
      mobilePromo: false,
      mobileConfirmation: false,
    });

    const actionButton = browser.contentWindow.document.querySelector(
      "#tabpickup-steps-view3 button.primary"
    );
    actionButton.click();

    await waitForElementVisible(browser, "#tabpickup-steps", false);
    checkMobilePromo(browser, {
      mobilePromo: false,
      mobileConfirmation: false,
    });
    await waitForElementVisible(browser, ".featureCallout .FEATURE_CALLOUT_1");
    ok(true, "Tab pickup product tour screen renders when sync is enabled");
    ok(
      Services.prefs.getBoolPref("services.sync.engine.tabs", false),
      "tab sync pref should be enabled after button click"
    );
  });
  await tearDown(sandbox);
});

add_task(async function test_tab_sync_loading() {
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
  await withFirefoxView({}, async browser => {
    Services.obs.notifyObservers(null, UIState.ON_UPDATE);

    await SpecialPowers.pushPrefEnv({
      set: [["services.sync.engine.tabs", true]],
    });

    const { document } = browser.contentWindow;
    const tabsContainer = document.querySelector("#tabpickup-tabs-container");
    const tabsList = document.querySelector(
      "#tabpickup-tabs-container tab-pickup-list"
    );
    const loadingElem = document.querySelector(
      "#tabpickup-tabs-container .loading-content"
    );
    const setupElem = document.querySelector("#tabpickup-steps");

    await waitForElementVisible(browser, "#tabpickup-steps", false);
    await waitForElementVisible(browser, "#tabpickup-tabs-container", true);
    checkMobilePromo(browser, {
      mobilePromo: false,
      mobileConfirmation: false,
    });

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
    checkLoadingState(true);

    await touchLastTabFetch();

    await BrowserTestUtils.waitForMutationCondition(
      tabsContainer,
      { attributeFilter: ["class"], attributes: true },
      () => {
        return !tabsContainer.classList.contains("loading");
      }
    );
    checkLoadingState(false);
    checkMobilePromo(browser, {
      mobilePromo: false,
      mobileConfirmation: false,
    });

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
    checkMobilePromo(browser, {
      mobilePromo: false,
      mobileConfirmation: false,
    });
  });
  await SpecialPowers.popPrefEnv();
  await tearDown(sandbox);
});

add_task(async function test_mobile_promo() {
  const sandbox = await setupWithDesktopDevices();
  await withFirefoxView({}, async browser => {
    // ensure last tab fetch was just now so we don't get the loading state
    await touchLastTabFetch();

    Services.obs.notifyObservers(null, UIState.ON_UPDATE);
    await waitForElementVisible(browser, ".synced-tabs-container");
    is(fxAccounts.device.recentDeviceList?.length, 2, "2 devices connected");

    info("checking mobile promo, should be visible now");
    checkMobilePromo(browser, {
      mobilePromo: true,
      mobileConfirmation: false,
    });

    gMockFxaDevices.push({
      id: 3,
      name: "Mobile Device",
      type: "mobile",
    });

    Services.obs.notifyObservers(null, "fxaccounts:devicelist_updated");

    // Wait for the async refreshDeviceList(),
    // which should result in the promo being hidden
    await waitForElementVisible(
      browser,
      "#tab-pickup-container > .promo-box",
      false
    );
    is(fxAccounts.device.recentDeviceList?.length, 3, "3 devices connected");
    checkMobilePromo(browser, {
      mobilePromo: false,
      mobileConfirmation: true,
    });

    info("checking mobile promo disappears on log out");
    gMockFxaDevices.pop();
    Services.obs.notifyObservers(null, "fxaccounts:devicelist_updated");
    await waitForElementVisible(
      browser,
      "#tab-pickup-container > .promo-box",
      true
    );
    checkMobilePromo(browser, {
      mobilePromo: true,
      mobileConfirmation: false,
    });
    gUIStateStatus = UIState.STATUS_NOT_CONFIGURED;

    info(
      "notifying that we've signed out of fxa, UIState.get().status:" +
        UIState.get().status
    );
    Services.obs.notifyObservers(null, UIState.ON_UPDATE);
    info("waiting for setup card 1 to appear again");
    await waitForVisibleSetupStep(browser, {
      expectedVisible: "#tabpickup-steps-view1",
    });
    checkMobilePromo(browser, {
      mobilePromo: false,
      mobileConfirmation: false,
    });
  });
  await tearDown(sandbox);
});

add_task(async function test_mobile_promo_pref() {
  const sandbox = await setupWithDesktopDevices();
  await SpecialPowers.pushPrefEnv({
    set: [[MOBILE_PROMO_DISMISSED_PREF, true]],
  });
  await withFirefoxView({}, async browser => {
    // ensure tab sync is false so we don't skip onto next step
    info("starting test, will notify of UIState update");
    // ensure last tab fetch was just now so we don't get the loading state
    await touchLastTabFetch();

    Services.obs.notifyObservers(null, UIState.ON_UPDATE);
    await waitForElementVisible(browser, ".synced-tabs-container");
    is(fxAccounts.device.recentDeviceList?.length, 2, "2 devices connected");

    info("checking mobile promo, should be still hidden because of the pref");
    checkMobilePromo(browser, {
      mobilePromo: false,
      mobileConfirmation: false,
    });

    // reset the dismissed pref, which should case the promo to get shown
    await SpecialPowers.popPrefEnv();
    await waitForElementVisible(
      browser,
      "#tab-pickup-container > .promo-box",
      true
    );

    const promoElem = browser.contentWindow.document.querySelector(
      "#tab-pickup-container > .promo-box"
    );
    const promoElemClose = promoElem.querySelector(".close");
    ok(promoElemClose.hasAttribute("aria-label"), "Button has an a11y name");
    // check that dismissing the promo sets the pref
    info("Clicking the promo close button: " + promoElemClose);
    EventUtils.sendMouseEvent({ type: "click" }, promoElemClose);

    info("Check the promo box got hidden");
    BrowserTestUtils.is_hidden(promoElem);
    ok(
      SpecialPowers.getBoolPref(MOBILE_PROMO_DISMISSED_PREF),
      "Promo pref is updated when close is clicked"
    );
  });
  await tearDown(sandbox);
});

add_task(async function test_mobile_promo_windows() {
  // make sure interacting with the promo and success confirmation in one window
  // also updates the others
  const sandbox = await setupWithDesktopDevices();
  await withFirefoxView({}, async browser => {
    // ensure last tab fetch was just now so we don't get the loading state
    await touchLastTabFetch();

    Services.obs.notifyObservers(null, UIState.ON_UPDATE);
    await waitForElementVisible(browser, ".synced-tabs-container");
    is(fxAccounts.device.recentDeviceList?.length, 2, "2 devices connected");

    info("checking mobile promo is visible");
    checkMobilePromo(browser, {
      mobilePromo: true,
      mobileConfirmation: false,
    });

    info(
      "opening new window, pref is: " +
        SpecialPowers.getBoolPref("browser.tabs.firefox-view")
    );

    let win2 = await BrowserTestUtils.openNewBrowserWindow();
    info("Got window, now opening Firefox View in it");
    await withFirefoxView(
      { resetFlowManager: false, win: win2 },
      async win2Browser => {
        info("In withFirefoxView taskFn for win2");
        // promo should be visible in the 2nd window too
        info("check mobile promo is visible in the new window");
        checkMobilePromo(win2Browser, {
          mobilePromo: true,
          mobileConfirmation: false,
        });

        // add the mobile device to get the success confirmation in both instances
        info("add a mobile device and send device_connected notification");
        gMockFxaDevices.push({
          id: 3,
          name: "Mobile Device",
          type: "mobile",
        });

        Services.obs.notifyObservers(null, "fxaccounts:devicelist_updated");
        is(
          fxAccounts.device.recentDeviceList?.length,
          3,
          "3 devices connected"
        );

        // Wait for the async refreshDevices(),
        // which should result in the promo being hidden
        info("waiting for the confirmation box to be visible");
        await waitForElementVisible(
          win2Browser,
          "#tab-pickup-container > .promo-box",
          false
        );

        for (let fxviewBrowser of [browser, win2Browser]) {
          info(
            "checking promo is hidden and confirmation is visible in each window"
          );
          checkMobilePromo(fxviewBrowser, {
            mobilePromo: false,
            mobileConfirmation: true,
          });
        }

        // dismiss the confirmation and check its gone from both instances
        const confirmBox = win2Browser.contentWindow.document.querySelector(
          "#tab-pickup-container > .confirmation-message-box"
        );
        const closeButton = confirmBox.querySelector(".close");
        ok(closeButton.hasAttribute("aria-label"), "Button has an a11y name");
        EventUtils.sendMouseEvent({ type: "click" }, closeButton, win2);
        BrowserTestUtils.is_hidden(confirmBox);

        for (let fxviewBrowser of [browser, win2Browser]) {
          checkMobilePromo(fxviewBrowser, {
            mobilePromo: false,
            mobileConfirmation: false,
          });
        }
      }
    );
    await BrowserTestUtils.closeWindow(win2);
  });
  await tearDown(sandbox);
});
