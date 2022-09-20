/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { TabsSetupFlowManager } = ChromeUtils.importESModule(
  "resource:///modules/firefox-view-tabs-setup-manager.sys.mjs"
);

const { LoginTestUtils } = ChromeUtils.import(
  "resource://testing-common/LoginTestUtils.jsm"
);

async function tearDown(sandbox) {
  sandbox?.restore();
  Services.prefs.clearUserPref("services.sync.lastTabFetch");
}

function setupMocks() {
  const sandbox = (gSandbox = setupRecentDeviceListMocks());
  return sandbox;
}

add_setup(async function() {
  registerCleanupFunction(async () => {
    // reset internal state so it doesn't affect the next tests
    TabsSetupFlowManager.resetInternalState();
    LoginTestUtils.primaryPassword.disable();
    await tearDown(gSandbox);
  });
  await SpecialPowers.pushPrefEnv({
    set: [["services.sync.username", "username@example.com"]],
  });
});

add_task(async function test_primary_password_locked() {
  LoginTestUtils.primaryPassword.enable();
  const sandbox = setupMocks();

  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    Services.obs.notifyObservers(null, UIState.ON_UPDATE);

    info("waiting for the error setup step to be visible");
    await waitForVisibleSetupStep(browser, {
      expectedVisible: "#tabpickup-steps-view0",
    });
    const primaryButton = document.querySelector("#error-state-button");
    ok(
      primaryButton && BrowserTestUtils.is_visible(primaryButton),
      "Error primary button is visible"
    );

    const clearErrorStub = sandbox.stub(
      TabsSetupFlowManager,
      "tryToClearError"
    );
    info("Setup state:" + TabsSetupFlowManager.currentSetupState.name);

    info("clicking the error panel button");
    primaryButton.click();
    ok(
      clearErrorStub.called,
      "tryToClearError was called when the try-again button was clicked"
    );
    TabsSetupFlowManager.tryToClearError.restore();

    info("Clearing the primary password");
    LoginTestUtils.primaryPassword.disable();
    ok(
      !TabsSetupFlowManager.isPrimaryPasswordLocked,
      "primary password is unlocked"
    );

    info("notifying of the primary-password unlock");
    const clearErrorSpy = sandbox.spy(TabsSetupFlowManager, "tryToClearError");
    Services.obs.notifyObservers(null, "passwordmgr-crypto-login");
    await TestUtils.waitForTick();
    ok(
      clearErrorSpy.called,
      "tryToClearError was called when the primary-password unlock notification was received"
    );

    // We expect the waiting state until we get a sync update/finished
    info("Setup state:" + TabsSetupFlowManager.currentSetupState.name);
    ok(TabsSetupFlowManager.waitingForTabs, "Now waiting for tabs");
    ok(
      document
        .querySelector("#tabpickup-tabs-container")
        .classList.contains("loading"),
      "Synced tabs container has loading class"
    );

    info("notifying of sync:finish");
    Services.obs.notifyObservers(null, "weave:service:sync:finish");
    await TestUtils.waitForTick();
    ok(
      !document
        .querySelector("#tabpickup-tabs-container")
        .classList.contains("loading"),
      "Synced tabs isn't loading any more"
    );
  });
  tearDown();
});
