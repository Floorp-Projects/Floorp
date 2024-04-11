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
  sandbox.spy(TabsSetupFlowManager, "tryToClearError");
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    await navigateToViewAndWait(document, "syncedtabs");

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
    await TestUtils.waitForCondition(
      () =>
        syncedTabsComponent.emptyState.shadowRoot.textContent.includes(
          "Check your internet connection"
        ),
      "The expected network offline error message is displayed."
    );

    ok(
      syncedTabsComponent.emptyState
        .getAttribute("headerlabel")
        .includes("network-offline"),
      "Network offline message is shown"
    );
    syncedTabsComponent.emptyState
      .querySelector("button[data-action='network-offline']")
      .click();

    await BrowserTestUtils.waitForCondition(
      () => TabsSetupFlowManager.tryToClearError.calledOnce
    );

    ok(
      TabsSetupFlowManager.tryToClearError.calledOnce,
      "TabsSetupFlowManager.tryToClearError() was called once"
    );

    ok(
      syncedTabsComponent.emptyState
        .getAttribute("headerlabel")
        .includes("network-offline"),
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
    await navigateToViewAndWait(document, "syncedtabs");

    Services.obs.notifyObservers(null, UIState.ON_UPDATE);
    Services.obs.notifyObservers(null, "weave:service:sync:error");

    let syncedTabsComponent = document.querySelector(
      "view-syncedtabs:not([slot=syncedtabs])"
    );
    await TestUtils.waitForCondition(() => syncedTabsComponent.fullyUpdated);
    await TestUtils.waitForCondition(
      () =>
        syncedTabsComponent.emptyState.shadowRoot.textContent.includes(
          "having trouble syncing"
        ),
      "Sync error message is shown."
    );

    ok(
      syncedTabsComponent.emptyState
        .getAttribute("headerlabel")
        .includes("sync-error"),
      "Correct message should show when there's a sync service error"
    );

    // Clear the error.
    Services.obs.notifyObservers(null, "weave:service:sync:finish");
  });
  await tearDown(sandbox);
});

add_task(async function test_sync_disabled_by_policy() {
  await SpecialPowers.pushPrefEnv({
    set: [["identity.fxaccounts.enabled", false]],
  });
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    const recentBrowsingSyncedTabs = document.querySelector(
      "view-syncedtabs[slot=syncedtabs]"
    );
    const syncedtabsPageNavButton = document.querySelector(
      "moz-page-nav-button[view='syncedtabs']"
    );

    ok(
      BrowserTestUtils.isHidden(recentBrowsingSyncedTabs),
      "Synced tabs should not be visible from recent browsing."
    );
    ok(
      BrowserTestUtils.isHidden(syncedtabsPageNavButton),
      "Synced tabs nav button should not be visible."
    );

    document.location.assign(`${getFirefoxViewURL()}#syncedtabs`);
    await TestUtils.waitForTick();
    is(
      document.querySelector("moz-page-nav").currentView,
      "recentbrowsing",
      "Should not be able to navigate to synced tabs."
    );
  });
  await tearDown();
});

add_task(async function test_sync_error_signed_out() {
  // sync error should not show if user is not signed in
  let sandbox = await setupWithDesktopDevices(UIState.STATUS_NOT_CONFIGURED);
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    await navigateToViewAndWait(document, "syncedtabs");
    Services.obs.notifyObservers(null, UIState.ON_UPDATE);
    Services.obs.notifyObservers(null, "weave:service:sync:error");

    let syncedTabsComponent = document.querySelector(
      "view-syncedtabs:not([slot=syncedtabs])"
    );
    await TestUtils.waitForCondition(
      () => syncedTabsComponent.fullyUpdated,
      "The synced tabs component has finished updating."
    );
    await TestUtils.waitForCondition(
      () =>
        syncedTabsComponent.emptyState.shadowRoot.textContent.includes(
          "sign in to your account"
        ),
      "Sign in header is shown."
    );

    ok(
      syncedTabsComponent.emptyState
        .getAttribute("headerlabel")
        .includes("signin-header"),
      "Sign in message is shown"
    );
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
    await navigateToViewAndWait(document, "syncedtabs");

    // triggered when user disconnects sync in about:preferences
    Services.obs.notifyObservers(null, UIState.ON_UPDATE);

    let syncedTabsComponent = document.querySelector(
      "view-syncedtabs:not([slot=syncedtabs])"
    );
    info("Waiting for the synced tabs error step to be visible");
    await TestUtils.waitForCondition(
      () => syncedTabsComponent.fullyUpdated,
      "The synced tabs component has finished updating."
    );
    await TestUtils.waitForCondition(
      () =>
        syncedTabsComponent.emptyState.shadowRoot.textContent.includes(
          "allow syncing"
        ),
      "The expected synced tabs empty state header is shown."
    );

    info(
      "Waiting for a mutation condition to ensure the right syncing error message"
    );
    ok(
      syncedTabsComponent.emptyState
        .getAttribute("headerlabel")
        .includes("sync-disconnected-header"),
      "Correct message should show when sync's been disconnected error"
    );

    let preferencesTabPromise = BrowserTestUtils.waitForNewTab(
      browser.getTabBrowser(),
      "about:preferences?action=choose-what-to-sync#sync",
      true
    );
    let emptyStateButton = syncedTabsComponent.emptyState.querySelector(
      "button[data-action='sync-disconnected']"
    );
    EventUtils.synthesizeMouseAtCenter(emptyStateButton, {}, content);
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
    await navigateToViewAndWait(document, "syncedtabs");

    // triggered by the user changing fxa password on another device
    Services.obs.notifyObservers(null, UIState.ON_UPDATE);

    let syncedTabsComponent = document.querySelector(
      "view-syncedtabs:not([slot=syncedtabs])"
    );
    await TestUtils.waitForCondition(
      () => syncedTabsComponent.fullyUpdated,
      "The synced tabs component has finished updating."
    );
    await TestUtils.waitForCondition(
      () =>
        syncedTabsComponent.emptyState.shadowRoot.textContent.includes(
          "sign in to your account"
        ),
      "The expected synced tabs empty state header is shown."
    );

    ok(
      syncedTabsComponent.emptyState
        .getAttribute("headerlabel")
        .includes("signin-header"),
      "Sign in message is shown"
    );
  });
  await tearDown(sandbox);
});

add_task(async function test_multiple_errors() {
  let sandbox = await setupWithDesktopDevices();
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    await navigateToViewAndWait(document, "syncedtabs");
    // Simulate conditions in which both the locked password and sync error
    // messages could be shown
    LoginTestUtils.primaryPassword.enable();
    Services.obs.notifyObservers(null, UIState.ON_UPDATE);
    Services.obs.notifyObservers(null, "weave:service:sync:error");

    let syncedTabsComponent = document.querySelector(
      "view-syncedtabs:not([slot=syncedtabs])"
    );
    await TestUtils.waitForCondition(
      () => syncedTabsComponent.fullyUpdated,
      "The synced tabs component has finished updating."
    );
    info("Waiting for the primary password error message to be shown");
    await TestUtils.waitForCondition(
      () =>
        syncedTabsComponent.emptyState.shadowRoot.textContent.includes(
          "enter the Primary Password"
        ),
      "The expected synced tabs empty state header is shown."
    );

    ok(
      syncedTabsComponent.emptyState
        .getAttribute("headerlabel")
        .includes("password-locked-header"),
      "Password locked message is shown"
    );

    const errorLink = syncedTabsComponent.emptyState.shadowRoot.querySelector(
      "a[data-l10n-name=syncedtab-password-locked-link]"
    );
    ok(
      errorLink && BrowserTestUtils.isVisible(errorLink),
      "Error link is visible"
    );

    // Clear the primary password error message
    LoginTestUtils.primaryPassword.disable();
    Services.obs.notifyObservers(null, UIState.ON_UPDATE);

    info("Waiting for the sync error message to be shown");
    await TestUtils.waitForCondition(
      () => syncedTabsComponent.fullyUpdated,
      "The synced tabs component has finished updating."
    );
    await TestUtils.waitForCondition(
      () =>
        syncedTabsComponent.emptyState.shadowRoot.textContent.includes(
          "having trouble syncing"
        ),
      "The expected synced tabs empty state header is shown."
    );

    ok(
      errorLink && BrowserTestUtils.isHidden(errorLink),
      "Error link is now hidden"
    );

    // Clear the sync error
    Services.obs.notifyObservers(null, "weave:service:sync:finish");
  });
  await tearDown(sandbox);
});
