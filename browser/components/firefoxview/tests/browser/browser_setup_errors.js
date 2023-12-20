/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { LoginTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/LoginTestUtils.sys.mjs"
);

async function setupWithDesktopDevices(state = UIState.STATUS_SIGNED_IN) {
  const sandbox = setupSyncFxAMocks({
    state,
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
  return sandbox;
}

function promiseSyncReady() {
  let service = Cc["@mozilla.org/weave/service;1"].getService(
    Ci.nsISupports
  ).wrappedJSObject;
  return service.whenLoaded();
}
async function tearDown(sandbox) {
  sandbox?.restore();
  Services.prefs.clearUserPref("services.sync.lastTabFetch");
  Services.prefs.clearUserPref("identity.fxaccounts.enabled");
}

add_setup(async function () {
  // gSync.init() is called in a requestIdleCallback. Force its initialization.
  gSync.init();

  await SpecialPowers.pushPrefEnv({
    set: [
      ["services.sync.engine.tabs", true],
      ["identity.fxaccounts.enabled", true],
    ],
  });

  registerCleanupFunction(async function () {
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
  sandbox.spy(TabsSetupFlowManager, "tryToClearError");
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
      return TabsSetupFlowManager.tryToClearError.calledOnce;
    });

    ok(
      TabsSetupFlowManager.tryToClearError.calledOnce,
      "TabsSetupFlowManager.tryToClearError() was called once"
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

add_task(async function test_sync_error_signed_out() {
  // sync error should not show if user is not signed in
  let sandbox = await setupWithDesktopDevices(UIState.STATUS_NOT_CONFIGURED);
  await withFirefoxView({}, async browser => {
    Services.obs.notifyObservers(null, UIState.ON_UPDATE);
    Services.obs.notifyObservers(null, "weave:service:sync:error");

    await waitForElementVisible(browser, "#tabpickup-steps", true);
    await waitForVisibleSetupStep(browser, {
      expectedVisible: "#tabpickup-steps-view1",
    });
  });
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

add_task(async function test_password_change_disconnect_error() {
  // When the user changes their password on another device, we get into a state
  // where the user is signed out but sync is still enabled.
  const sandbox = setupSyncFxAMocks({
    state: UIState.STATUS_LOGIN_FAILED,
    syncEnabled: true,
  });
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;

    // triggered by the user changing fxa password on another device
    Services.obs.notifyObservers(null, UIState.ON_UPDATE);

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
      () => errorStateHeader.textContent.includes("Sign in to reconnect")
    );

    ok(
      errorStateHeader.getAttribute("data-l10n-id").includes("signed-out"),
      "Correct message should show when user has been logged out due to external password change."
    );
  });
  await tearDown(sandbox);
});

add_task(async function test_multiple_errors() {
  let sandbox = await setupWithDesktopDevices();
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    // Simulate conditions in which both the locked password and sync error
    // messages could be shown
    LoginTestUtils.primaryPassword.enable();
    Services.obs.notifyObservers(null, UIState.ON_UPDATE);
    Services.obs.notifyObservers(null, "weave:service:sync:error");

    info("Waiting for the primary password error message to be shown");
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
      () => errorStateHeader.textContent.includes("Enter your Primary Password")
    );

    ok(
      errorStateHeader.getAttribute("data-l10n-id").includes("password-locked"),
      "Password locked error message is shown"
    );

    const errorLink = document.querySelector("#error-state-link");
    ok(
      errorLink && BrowserTestUtils.is_visible(errorLink),
      "Error link is visible"
    );

    // Clear the primary password error message
    LoginTestUtils.primaryPassword.disable();
    Services.obs.notifyObservers(null, UIState.ON_UPDATE);

    info("Waiting for the sync error message to be shown");
    await BrowserTestUtils.waitForMutationCondition(
      errorStateHeader,
      { childList: true },
      () => errorStateHeader.textContent.includes("trouble syncing")
    );

    ok(
      errorStateHeader.getAttribute("data-l10n-id").includes("sync-error"),
      "Sync error message is now shown"
    );

    ok(
      errorLink && BrowserTestUtils.is_hidden(errorLink),
      "Error link is now hidden"
    );

    // Clear the sync error
    Services.obs.notifyObservers(null, "weave:service:sync:finish");
  });
  await tearDown(sandbox);
});
