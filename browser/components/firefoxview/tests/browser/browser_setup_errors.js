/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { TabsSetupFlowManager } = ChromeUtils.importESModule(
  "resource:///modules/firefox-view-tabs-setup-manager.sys.mjs"
);

async function setupWithDesktopDevices() {
  const sandbox = setupSyncFxAMocks({
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
  return sandbox;
}

async function tearDown(sandbox) {
  sandbox?.restore();
  Services.prefs.clearUserPref("services.sync.lastTabFetch");
  Services.prefs.clearUserPref("identity.fxaccounts.enabled");
}

add_setup(async function() {
  await promiseSyncReady();
  // gSync.init() is called in a requestIdleCallback. Force its initialization.
  gSync.init();

  await SpecialPowers.pushPrefEnv({
    set: [
      ["services.sync.engine.tabs", true],
      ["identity.fxaccounts.enabled", true],
    ],
  });

  registerCleanupFunction(async function() {
    // reset internal state so it doesn't affect the next tests
    TabsSetupFlowManager.resetInternalState();
    await tearDown(gSandbox);
  });
});

add_task(async function test_network_offline() {
  const sandbox = await setupWithDesktopDevices();
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;

    Services.obs.notifyObservers(null, UIState.ON_UPDATE);
    Services.obs.notifyObservers(
      null,
      "network:offline-status-changed",
      "offline"
    );
    await waitForElementVisible(browser, "#tabpickup-steps", true);
    await waitForVisibleSetupStep(browser, {
      expectedVisible: "#tabpickup-steps-view0",
    });

    const errorStateHeader = document.querySelector(
      "#tabpickup-steps-view0-header"
    );

    await BrowserTestUtils.waitForMutationCondition(
      errorStateHeader,
      { childList: true },
      () => errorStateHeader.textContent.includes("connection")
    );

    ok(
      errorStateHeader.getAttribute("data-l10n-id").includes("network-offline"),
      "Correct message should show when network connection is lost"
    );

    Services.obs.notifyObservers(
      null,
      "network:offline-status-changed",
      "online"
    );

    await waitForElementVisible(browser, "#tabpickup-tabs-container", true);
  });
  await tearDown(sandbox);
});

add_task(async function test_sync_error() {
  const sandbox = await setupWithDesktopDevices();
  sandbox.spy(TabsSetupFlowManager, "forceSyncTabs");
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;

    Services.obs.notifyObservers(null, UIState.ON_UPDATE);
    Services.obs.notifyObservers(null, "weave:service:sync:error");

    await waitForElementVisible(browser, "#tabpickup-steps", true);
    await waitForVisibleSetupStep(browser, {
      expectedVisible: "#tabpickup-steps-view0",
    });

    const errorStateHeader = document.querySelector(
      "#tabpickup-steps-view0-header"
    );

    await BrowserTestUtils.waitForMutationCondition(
      errorStateHeader,
      { childList: true },
      () => errorStateHeader.textContent.includes("trouble syncing")
    );

    ok(
      errorStateHeader.getAttribute("data-l10n-id").includes("sync-error"),
      "Correct message should show when there's a sync service error"
    );

    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#error-state-button",
      {},
      browser
    );

    await BrowserTestUtils.waitForCondition(() => {
      return TabsSetupFlowManager.forceSyncTabs.calledOnce;
    });

    ok(
      TabsSetupFlowManager.forceSyncTabs.calledOnce,
      "TabsSetupFlowManager.forceSyncTabs() was called once"
    );

    // Clear the error.
    Services.obs.notifyObservers(null, "weave:service:sync:finish");
  });

  // Now reopen the tab and check that sending an error state does not
  // start showing the error:
  Services.obs.notifyObservers(null, UIState.ON_UPDATE);
  const recentFetchTime = Math.floor(Date.now() / 1000);
  info("updating lastFetch:" + recentFetchTime);
  Services.prefs.setIntPref("services.sync.lastTabFetch", recentFetchTime);
  await withFirefoxView({ resetFlowManager: false }, async browser => {
    const { document } = browser.contentWindow;

    await waitForElementVisible(browser, "#synced-tabs-placeholder", true);

    Services.obs.notifyObservers(null, "weave:service:sync:error");
    await TestUtils.waitForTick();
    ok(
      BrowserTestUtils.is_visible(
        document.getElementById("synced-tabs-placeholder")
      ),
      "Should still be showing the placeholder content."
    );
    let stepHeader = document.getElementById("tabpickup-steps-view0-header");
    ok(
      !stepHeader || BrowserTestUtils.is_hidden(stepHeader),
      "Should not be showing an error state if we had previously synced successfully."
    );

    // Now drop a device:
    let someDevice = gMockFxaDevices.pop();
    Services.obs.notifyObservers(null, "fxaccounts:devicelist_updated");
    // This will trip a UI update where we decide we can't rely on
    // previously synced tabs anymore (they may be from the device
    // that was removed!), so we still show an error:

    await waitForElementVisible(browser, "#tabpickup-steps", true);
    await waitForVisibleSetupStep(browser, {
      expectedVisible: "#tabpickup-steps-view0",
    });

    const errorStateHeader = document.querySelector(
      "#tabpickup-steps-view0-header"
    );

    await BrowserTestUtils.waitForMutationCondition(
      errorStateHeader,
      { childList: true },
      () => errorStateHeader.textContent.includes("trouble syncing")
    );

    ok(
      errorStateHeader.getAttribute("data-l10n-id").includes("sync-error"),
      "Correct message should show when there's an error and tab information is outdated."
    );

    // Sneak device back in so as not to break other tests:
    gMockFxaDevices.push(someDevice);
    // Clear the error.
    Services.obs.notifyObservers(null, "weave:service:sync:finish");
  });
  Services.prefs.clearUserPref("services.sync.lastTabFetch");

  await tearDown(sandbox);
});

add_task(async function test_sync_disconnected_error() {
  // it's possible for fxa to be enabled but sync not enabled.
  const sandbox = setupSyncFxAMocks({
    state: UIState.STATUS_SIGNED_IN,
    syncEnabled: false,
  });
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;

    // triggered when user disconnects sync in about:preferences
    Services.obs.notifyObservers(null, UIState.ON_UPDATE);

    await waitForElementVisible(browser, "#tabpickup-steps", true);
    info("Waiting for the tabpickup error step to be visible");
    await waitForVisibleSetupStep(browser, {
      expectedVisible: "#tabpickup-steps-view0",
    });

    const errorStateHeader = document.querySelector(
      "#tabpickup-steps-view0-header"
    );

    info(
      "Waiting for a mutation condition to ensure the right syncing error message"
    );
    await BrowserTestUtils.waitForMutationCondition(
      errorStateHeader,
      { childList: true },
      () => errorStateHeader.textContent.includes("Turn on syncing to continue")
    );

    ok(
      errorStateHeader
        .getAttribute("data-l10n-id")
        .includes("sync-disconnected"),
      "Correct message should show when sync's been disconnected error"
    );

    let preferencesTabPromise = BrowserTestUtils.waitForNewTab(
      browser.getTabBrowser(),
      "about:preferences?action=choose-what-to-sync#sync",
      true
    );
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#error-state-button",
      {},
      browser
    );
    let preferencesTab = await preferencesTabPromise;
    await BrowserTestUtils.removeTab(preferencesTab);
  });
  await tearDown(sandbox);
});
