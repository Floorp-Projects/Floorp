/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const FXA_CONTINUE_EVENT = [["firefoxview", "fxa_continue", "sync", undefined]];

const FXA_MOBILE_EVENT = [
  ["firefoxview", "fxa_mobile", "sync", undefined, { has_devices: "false" }],
];

var gMockFxaDevices = null;
var gUIStateStatus;

function promiseSyncReady() {
  let service = Cc["@mozilla.org/weave/service;1"].getService(
    Ci.nsISupports
  ).wrappedJSObject;
  return service.whenLoaded();
}

var gSandbox;

async function setupWithDesktopDevices() {
  const sandbox = setupMocks({
    state: UIState.STATUS_SIGNED_IN,
    fxaDevices: [
      {
        id: 1,
        name: "This Device",
        isCurrentDevice: true,
        type: "desktop",
        tabs: [],
      },
      {
        id: 2,
        name: "Other Device",
        type: "desktop",
        tabs: [],
      },
    ],
  });

  await SpecialPowers.pushPrefEnv({
    set: [["services.sync.engine.tabs", true]],
  });
  return sandbox;
}
add_setup(async function () {
  registerCleanupFunction(() => {
    // reset internal state so it doesn't affect the next tests
    TabsSetupFlowManager.resetInternalState();
  });

  // gSync.init() is called in a requestIdleCallback. Force its initialization.
  gSync.init();

  registerCleanupFunction(async function () {
    Services.prefs.clearUserPref("services.sync.engine.tabs");
    await tearDown(gSandbox);
  });
  // set tab sync false so we don't skip setup states
  await SpecialPowers.pushPrefEnv({
    set: [["services.sync.engine.tabs", false]],
  });
});

add_task(async function test_unconfigured_initial_state() {
  const sandbox = setupMocks({
    state: UIState.STATUS_NOT_CONFIGURED,
    syncEnabled: false,
  });
  await withFirefoxView({ openNewWindow: true }, async browser => {
    Services.obs.notifyObservers(null, UIState.ON_UPDATE);
    await waitForVisibleSetupStep(browser, {
      expectedVisible: "#tabpickup-steps-view1",
    });
    checkMobilePromo(browser, {
      mobilePromo: false,
      mobileConfirmation: false,
    });
    await clearAllParentTelemetryEvents();
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
        return events && events.length >= 1;
      },
      "Waiting for fxa_continue firefoxview telemetry events.",
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
  const sandbox = setupMocks({
    state: UIState.STATUS_SIGNED_IN,
    fxaDevices: [
      {
        id: 1,
        name: "This Device",
        isCurrentDevice: true,
        type: "desktop",
        tabs: [],
      },
    ],
  });

  await withFirefoxView({ openNewWindow: true }, async browser => {
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

    await clearAllParentTelemetryEvents();

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
        return events && events.length >= 1;
      },
      "Waiting for fxa_mobile firefoxview telemetry events.",
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

add_task(async function test_support_links() {
  await clearAllParentTelemetryEvents();
  setupMocks({
    state: UIState.STATUS_SIGNED_IN,
    fxaDevices: [
      {
        id: 1,
        name: "This Device",
        isCurrentDevice: true,
        type: "desktop",
        tabs: [],
      },
    ],
  });
  await withFirefoxView({}, async browser => {
    Services.obs.notifyObservers(null, UIState.ON_UPDATE);
    await waitForVisibleSetupStep(browser, {
      expectedVisible: "#tabpickup-steps-view2",
    });
    const { document } = browser.contentWindow;
    const container = document.getElementById("tab-pickup-container");
    const supportLinks = Array.from(
      container.querySelectorAll("a[href]")
    ).filter(a => !!a.href);
    is(supportLinks.length, 2, "Support links have non-empty hrefs");
  });
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
        tabs: [],
      },
      {
        id: 2,
        name: "Other Device",
        type: "desktop",
        tabs: [],
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
        device => device.type !== "mobile" && device.type !== "tablet"
      ),
      "No connected device is type:mobile or type:tablet"
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
        tabs: [],
      },
      {
        id: 2,
        name: "Other Device",
        type: "mobile",
        tabs: [],
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
        device => device.type == "mobile"
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

add_task(async function test_tablet_connected() {
  const sandbox = setupMocks({
    state: UIState.STATUS_SIGNED_IN,
    fxaDevices: [
      {
        id: 1,
        name: "This Device",
        isCurrentDevice: true,
        type: "desktop",
        tabs: [],
      },
      {
        id: 2,
        name: "Other Device",
        type: "tablet",
        tabs: [],
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
        device => device.type == "tablet"
      ),
      "A connected device is type:tablet"
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
        tabs: [],
      },
      {
        id: 2,
        name: "Other Device",
        type: "mobile",
        tabs: [],
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
    ok(true, "Tab pickup product tour screen renders when sync is enabled");
    ok(
      Services.prefs.getBoolPref("services.sync.engine.tabs", false),
      "tab sync pref should be enabled after button click"
    );
  });
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
      tabs: [],
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

    // Set the UIState to what we expect when the user signs out
    gUIStateStatus = UIState.STATUS_NOT_CONFIGURED;
    gUIStateSyncEnabled = undefined;

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
    await SpecialPowers.pushPrefEnv({
      set: [[MOBILE_PROMO_DISMISSED_PREF, false]],
    });
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

    info("Got window, now opening Firefox View in it");
    await withFirefoxView(
      { openNewWindow: true, resetFlowManager: false },
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
          tabs: [],
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
        EventUtils.sendMouseEvent(
          { type: "click" },
          closeButton,
          win2Browser.ownerGlobal
        );
        BrowserTestUtils.is_hidden(confirmBox);

        for (let fxviewBrowser of [browser, win2Browser]) {
          checkMobilePromo(fxviewBrowser, {
            mobilePromo: false,
            mobileConfirmation: false,
          });
        }
      }
    );
  });
  await tearDown(sandbox);
});

async function mockFxaDeviceConnected(win) {
  // We use an existing tab to navigate to the final "device connected" url
  // in order to fake the fxa device sync process
  const url = "https://example.org/pair/auth/complete";
  is(win.gBrowser.tabs.length, 3, "Tabs strip should contain three tabs");

  BrowserTestUtils.startLoadingURIString(
    win.gBrowser.selectedTab.linkedBrowser,
    url
  );

  await BrowserTestUtils.browserLoaded(
    win.gBrowser.selectedTab.linkedBrowser,
    null,
    url
  );

  is(
    win.gBrowser.selectedTab.linkedBrowser.currentURI.filePath,
    "/pair/auth/complete",
    "/pair/auth/complete is the selected tab"
  );
}

add_task(async function test_close_device_connected_tab() {
  // test that when a device has been connected to sync we close
  // that tab after the user is directed back to firefox view

  // Ensure we are in the correct state to start the task.
  TabsSetupFlowManager.resetInternalState();
  await SpecialPowers.pushPrefEnv({
    set: [["identity.fxaccounts.remote.root", "https://example.org/"]],
  });
  let win = await BrowserTestUtils.openNewBrowserWindow();
  let fxViewTab = await openFirefoxViewTab(win);

  await waitForVisibleSetupStep(win.gBrowser, {
    expectedVisible: "#tabpickup-steps-view1",
  });

  let actionButton = win.gBrowser.contentWindow.document.querySelector(
    "#tabpickup-steps-view1 button.primary"
  );
  // initiate the sign in flow from Firefox View, to check that didFxaTabOpen is set
  let tabSwitched = BrowserTestUtils.waitForEvent(
    win.gBrowser,
    "TabSwitchDone"
  );
  actionButton.click();
  await tabSwitched;

  // fake the end point of the device syncing flow
  await mockFxaDeviceConnected(win);
  let deviceConnectedTab = win.gBrowser.tabs[2];

  // remove the blank tab opened with the browser to check that we don't
  // close the window when the "Device connected" tab is closed
  const newTab = win.gBrowser.tabs.find(
    tab => tab != deviceConnectedTab && tab != fxViewTab
  );
  let removedTab = BrowserTestUtils.waitForTabClosing(newTab);
  BrowserTestUtils.removeTab(newTab);
  await removedTab;

  is(win.gBrowser.tabs.length, 2, "Tabs strip should only contain two tabs");

  is(
    win.gBrowser.selectedTab.linkedBrowser.currentURI.filePath,
    "/pair/auth/complete",
    "/pair/auth/complete is the selected tab"
  );

  // we use this instead of BrowserTestUtils.switchTab to get back to the firefox view tab
  // because this more accurately reflects how this tab is selected - via a custom onmousedown
  // and command that calls FirefoxViewHandler.openTab (both when the user manually clicks the tab
  // and when navigating from the fxa Device Connected tab, which also calls FirefoxViewHandler.openTab)
  await EventUtils.synthesizeMouseAtCenter(
    win.document.getElementById("firefox-view-button"),
    { type: "mousedown" },
    win
  );

  is(win.gBrowser.tabs.length, 2, "Tabs strip should only contain two tabs");

  is(
    win.gBrowser.tabs[0].linkedBrowser.currentURI.filePath,
    "firefoxview",
    "First tab is Firefox view"
  );

  is(
    win.gBrowser.tabs[1].linkedBrowser.currentURI.filePath,
    "newtab",
    "Second tab is about:newtab"
  );

  // now simulate the signed-in state with the prompt to download
  // and sync mobile
  const sandbox = setupMocks({
    state: UIState.STATUS_SIGNED_IN,
    fxaDevices: [
      {
        id: 1,
        name: "This Device",
        isCurrentDevice: true,
        type: "desktop",
        tabs: [],
      },
    ],
  });

  Services.obs.notifyObservers(null, UIState.ON_UPDATE);

  await waitForVisibleSetupStep(win.gBrowser, {
    expectedVisible: "#tabpickup-steps-view2",
  });

  actionButton = win.gBrowser.contentWindow.document.querySelector(
    "#tabpickup-steps-view2 button.primary"
  );
  // initiate the connect device (mobile) flow from Firefox View, to check that didFxaTabOpen is set
  tabSwitched = BrowserTestUtils.waitForEvent(win.gBrowser, "TabSwitchDone");
  actionButton.click();
  await tabSwitched;
  // fake the end point of the device syncing flow
  await mockFxaDeviceConnected(win);

  await EventUtils.synthesizeMouseAtCenter(
    win.document.getElementById("firefox-view-button"),
    { type: "mousedown" },
    win
  );
  is(win.gBrowser.tabs.length, 2, "Tabs strip should only contain two tabs");

  is(
    win.gBrowser.tabs[0].linkedBrowser.currentURI.filePath,
    "firefoxview",
    "First tab is Firefox view"
  );

  is(
    win.gBrowser.tabs[1].linkedBrowser.currentURI.filePath,
    "newtab",
    "Second tab is about:newtab"
  );

  // cleanup time
  await tearDown(sandbox);
  await BrowserTestUtils.closeWindow(win);
});
