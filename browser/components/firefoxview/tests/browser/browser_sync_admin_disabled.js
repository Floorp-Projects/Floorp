/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { TabsSetupFlowManager } = ChromeUtils.importESModule(
  "resource:///modules/firefox-view-tabs-setup-manager.sys.mjs"
);

var gSandbox;

add_setup(async function() {
  Services.prefs.lockPref("identity.fxaccounts.enabled");

  registerCleanupFunction(() => {
    gSandbox?.restore();
    Services.prefs.clearUserPref("services.sync.lastTabFetch");
    Services.prefs.unlockPref("identity.fxaccounts.enabled");
    Services.prefs.clearUserPref("identity.fxaccounts.enabled");
    // reset internal state so it doesn't affect the next tests
    TabsSetupFlowManager.resetInternalState();
  });

  await promiseSyncReady();
  // gSync.init() is called in a requestIdleCallback. Force its initialization.
  gSync.init();
});

add_task(async function test_sync_admin_disabled() {
  const sandbox = (gSandbox = sinon.createSandbox());
  sandbox.stub(UIState, "get").callsFake(() => {
    return {
      status: UIState.STATUS_NOT_CONFIGURED,
      syncEnabled: false,
    };
  });
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;

    Services.obs.notifyObservers(null, UIState.ON_UPDATE);
    is(
      Services.prefs.getBoolPref("identity.fxaccounts.enabled"),
      true,
      "Expected identity.fxaccounts.enabled pref to be false"
    );

    is(
      Services.prefs.prefIsLocked("identity.fxaccounts.enabled"),
      true,
      "Expected identity.fxaccounts.enabled pref to be locked"
    );

    await waitForVisibleSetupStep(browser, {
      expectedVisible: "#tabpickup-steps-view0",
    });

    const errorStateHeader = document.querySelector(
      "#tabpickup-steps-view0-header"
    );

    await BrowserTestUtils.waitForMutationCondition(
      errorStateHeader,
      { childList: true },
      () => errorStateHeader.textContent.includes("disabled")
    );

    ok(
      errorStateHeader
        .getAttribute("data-l10n-id")
        .includes("fxa-admin-disabled"),
      "Correct message should show when fxa is disabled by an admin"
    );
  });
  Services.prefs.unlockPref("identity.fxaccounts.enabled");
});
