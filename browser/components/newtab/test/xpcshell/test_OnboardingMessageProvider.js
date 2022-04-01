/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

XPCOMUtils.defineLazyModuleGetters(this, {
  OnboardingMessageProvider:
    "resource://activity-stream/lib/OnboardingMessageProvider.jsm",
  sinon: "resource://testing-common/Sinon.jsm",
});

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
