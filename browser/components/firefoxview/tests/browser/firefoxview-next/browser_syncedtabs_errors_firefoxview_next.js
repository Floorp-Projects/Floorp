/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../head.js */

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
      ["browser.tabs.firefox-view-next", true],
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
  sandbox.spy(TabsSetupFlowManager, "tryToClearError");
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    await navigateToCategoryAndWait(document, "syncedtabs");

    Services.obs.notifyObservers(null, UIState.ON_UPDATE);
    Services.obs.notifyObservers(
      null,
      "network:offline-status-changed",
      "offline"
    );

    let syncedTabsComponent = document.querySelector(
      "view-syncedtabs:not([slot=syncedtabs])"
    );
    await TestUtils.waitForCondition(() => syncedTabsComponent.fullyUpdated);
    await BrowserTestUtils.waitForMutationCondition(
      syncedTabsComponent.shadowRoot.querySelector(".cards-container"),
      { childList: true },
      () => syncedTabsComponent.shadowRoot.innerHTML.includes("network-offline")
    );

    let emptyState =
      syncedTabsComponent.shadowRoot.querySelector("fxview-empty-state");
    ok(
      emptyState.getAttribute("headerlabel").includes("network-offline"),
      "Network offline message is shown"
    );
    emptyState.querySelector("button[data-action='network-offline']").click();

    await BrowserTestUtils.waitForCondition(
      () => TabsSetupFlowManager.tryToClearError.calledOnce
    );

    ok(
      TabsSetupFlowManager.tryToClearError.calledOnce,
      "TabsSetupFlowManager.tryToClearError() was called once"
    );

    emptyState =
      syncedTabsComponent.shadowRoot.querySelector("fxview-empty-state");
    ok(
      emptyState.getAttribute("headerlabel").includes("network-offline"),
      "Network offline message is still shown"
    );

    Services.obs.notifyObservers(
      null,
      "network:offline-status-changed",
      "online"
    );
  });
  await tearDown(sandbox);
});

add_task(async function test_sync_error() {
  const sandbox = await setupWithDesktopDevices();
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    await navigateToCategoryAndWait(document, "syncedtabs");

    Services.obs.notifyObservers(null, UIState.ON_UPDATE);
    Services.obs.notifyObservers(null, "weave:service:sync:error");

    let syncedTabsComponent = document.querySelector(
      "view-syncedtabs:not([slot=syncedtabs])"
    );
    await TestUtils.waitForCondition(() => syncedTabsComponent.fullyUpdated);
    await BrowserTestUtils.waitForMutationCondition(
      syncedTabsComponent.shadowRoot.querySelector(".cards-container"),
      { childList: true },
      () => syncedTabsComponent.shadowRoot.innerHTML.includes("sync-error")
    );

    let emptyState =
      syncedTabsComponent.shadowRoot.querySelector("fxview-empty-state");
    ok(
      emptyState.getAttribute("headerlabel").includes("sync-error"),
      "Correct message should show when there's a sync service error"
    );

    // Clear the error.
    Services.obs.notifyObservers(null, "weave:service:sync:finish");
  });
  await tearDown(sandbox);
});
