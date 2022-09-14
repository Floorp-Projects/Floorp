/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { OnboardingMessageProvider } = ChromeUtils.import(
  "resource://activity-stream/lib/OnboardingMessageProvider.jsm"
);
const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

function getOnboardingScreenById(screens, screenId) {
  return screens.find(screen => {
    console.log(screen?.id);
    return screen?.id === screenId;
  });
}

add_task(
  async function test_OnboardingMessageProvider_getUpgradeMessage_no_pin() {
    let sandbox = sinon.createSandbox();
    sandbox.stub(OnboardingMessageProvider, "_doesAppNeedPin").resolves(true);
    const message = await OnboardingMessageProvider.getUpgradeMessage();
    // If Firefox is not pinned, the screen should have "pin" content
    equal(
      message.content.screens[0].id,
      "UPGRADE_PIN_FIREFOX",
      "Screen has pin screen id"
    );
    equal(
      message.content.screens[0].content.primary_button.action.type,
      "PIN_FIREFOX_TO_TASKBAR",
      "Primary button has pin action type"
    );
    sandbox.restore();
  }
);

add_task(
  async function test_OnboardingMessageProvider_getUpgradeMessage_pin_no_default() {
    let sandbox = sinon.createSandbox();
    sandbox.stub(OnboardingMessageProvider, "_doesAppNeedPin").resolves(false);
    sandbox
      .stub(OnboardingMessageProvider, "_doesAppNeedDefault")
      .resolves(true);
    const message = await OnboardingMessageProvider.getUpgradeMessage();
    // If Firefox is pinned, but not the default, the screen should have "make default" content
    equal(
      message.content.screens[0].id,
      "UPGRADE_ONLY_DEFAULT",
      "Screen has make default screen id"
    );
    equal(
      message.content.screens[0].content.primary_button.action.type,
      "SET_DEFAULT_BROWSER",
      "Primary button has make default action"
    );
    sandbox.restore();
  }
);

add_task(
  async function test_OnboardingMessageProvider_getUpgradeMessage_pin_and_default() {
    let sandbox = sinon.createSandbox();
    sandbox.stub(OnboardingMessageProvider, "_doesAppNeedPin").resolves(false);
    sandbox
      .stub(OnboardingMessageProvider, "_doesAppNeedDefault")
      .resolves(false);
    const message = await OnboardingMessageProvider.getUpgradeMessage();
    // If Firefox is pinned and the default, the screen should have "get started" content
    equal(
      message.content.screens[0].id,
      "UPGRADE_GET_STARTED",
      "Screen has get started screen id"
    );
    ok(
      !message.content.screens[0].content.primary_button.action.type,
      "Primary button has no action type"
    );
    sandbox.restore();
  }
);

add_task(async function test_OnboardingMessageProvider_getNoImport_default() {
  let sandbox = sinon.createSandbox();
  sandbox
    .stub(OnboardingMessageProvider, "_doesAppNeedDefault")
    .resolves(false);
  const message = await OnboardingMessageProvider.getUpgradeMessage();

  // No import screen is shown when user has Firefox both pinned and default
  Assert.notEqual(
    message.content.screens[1]?.id,
    "UPGRADE_IMPORT_SETTINGS",
    "Screen has no import screen id"
  );
  sandbox.restore();
});

add_task(async function test_OnboardingMessageProvider_getImport_nodefault() {
  Services.prefs.setBoolPref("browser.shell.checkDefaultBrowser", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.shell.checkDefaultBrowser");
  });

  let sandbox = sinon.createSandbox();
  sandbox.stub(OnboardingMessageProvider, "_doesAppNeedDefault").resolves(true);
  sandbox.stub(OnboardingMessageProvider, "_doesAppNeedPin").resolves(false);
  const message = await OnboardingMessageProvider.getUpgradeMessage();

  // Import screen is shown when user doesn't have Firefox pinned and default
  Assert.equal(
    message.content.screens[1]?.id,
    "UPGRADE_IMPORT_SETTINGS",
    "Screen has import screen id"
  );
  sandbox.restore();
});

add_task(
  async function test_OnboardingMessageProvider_getPinPrivateWindow_noPrivatePin() {
    Services.prefs.setBoolPref("browser.shell.checkDefaultBrowser", true);
    registerCleanupFunction(() => {
      Services.prefs.clearUserPref("browser.shell.checkDefaultBrowser");
    });
    let sandbox = sinon.createSandbox();
    // User needs default to ensure Pin Private window shows as third screen after import
    sandbox
      .stub(OnboardingMessageProvider, "_doesAppNeedDefault")
      .resolves(true);

    let pinStub = sandbox.stub(OnboardingMessageProvider, "_doesAppNeedPin");
    pinStub.resolves(false);
    pinStub.withArgs(true).resolves(true);
    const message = await OnboardingMessageProvider.getUpgradeMessage();

    // Pin Private screen is shown when user doesn't have Firefox private pinned but has Firefox pinned
    Assert.ok(
      getOnboardingScreenById(
        message.content.screens,
        "UPGRADE_PIN_PRIVATE_WINDOW"
      )
    );
    sandbox.restore();
  }
);

add_task(
  async function test_OnboardingMessageProvider_getNoPinPrivateWindow_noPin() {
    Services.prefs.setBoolPref("browser.shell.checkDefaultBrowser", true);
    registerCleanupFunction(() => {
      Services.prefs.clearUserPref("browser.shell.checkDefaultBrowser");
    });
    let sandbox = sinon.createSandbox();
    // User needs default to ensure Pin Private window shows as third screen after import
    sandbox
      .stub(OnboardingMessageProvider, "_doesAppNeedDefault")
      .resolves(true);

    let pinStub = sandbox.stub(OnboardingMessageProvider, "_doesAppNeedPin");
    pinStub.resolves(true);
    const message = await OnboardingMessageProvider.getUpgradeMessage();

    // Pin Private screen is not shown when user doesn't have Firefox pinned
    Assert.ok(
      !getOnboardingScreenById(
        message.content.screens,
        "UPGRADE_PIN_PRIVATE_WINDOW"
      )
    );
    sandbox.restore();
  }
);

add_task(async function test_schemaValidation() {
  const { experimentValidator, messageValidators } = await makeValidators();

  const messages = await OnboardingMessageProvider.getMessages();
  for (const message of messages) {
    const validator = messageValidators[message.template];

    Assert.ok(
      typeof validator !== "undefined",
      typeof validator !== "undefined"
        ? `Schema validator found for ${message.template}.`
        : `No schema validator found for template ${message.template}. Please update this test to add one.`
    );
    assertValidates(
      validator,
      message,
      `Message ${message.id} validates as template ${message.template}`
    );
    assertValidates(
      experimentValidator,
      message,
      `Message ${message.id} validates as MessagingExperiment`
    );
  }
});

add_task(
  async function test_OnboardingMessageProvider_getPinPrivateWindow_pinPBMPrefDisabled() {
    Services.prefs.setBoolPref(
      "browser.startup.upgradeDialog.pinPBM.disabled",
      true
    );
    registerCleanupFunction(() => {
      Services.prefs.clearUserPref(
        "browser.startup.upgradeDialog.pinPBM.disabled"
      );
    });
    let sandbox = sinon.createSandbox();
    // User needs default to ensure Pin Private window shows as third screen after import
    sandbox
      .stub(OnboardingMessageProvider, "_doesAppNeedDefault")
      .resolves(true);

    let pinStub = sandbox.stub(OnboardingMessageProvider, "_doesAppNeedPin");
    pinStub.resolves(true);

    const message = await OnboardingMessageProvider.getUpgradeMessage();
    // Pin Private screen is not shown when pref is turned on
    Assert.ok(
      !getOnboardingScreenById(
        message.content.screens,
        "UPGRADE_PIN_PRIVATE_WINDOW"
      )
    );
    sandbox.restore();
  }
);
