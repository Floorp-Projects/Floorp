/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { JsonSchema } = ChromeUtils.import(
  "resource://gre/modules/JsonSchema.jsm"
);
const { OnboardingMessageProvider } = ChromeUtils.import(
  "resource://activity-stream/lib/OnboardingMessageProvider.jsm"
);
const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

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

add_task(async function test_schemaValidation() {
  function schemaValidator(uri) {
    return fetch(uri, { credentials: "omit" })
      .then(rsp => rsp.json())
      .then(schema => new JsonSchema.Validator(schema));
  }

  function assertValid(validator, obj, msg) {
    const result = validator.validate(obj);
    Assert.deepEqual(
      result,
      { valid: true, errors: [] },
      `${msg} - errors = ${JSON.stringify(result.errors, undefined, 2)}`
    );
  }

  const experimentValidator = await schemaValidator(
    "resource://activity-stream/schemas/MessagingExperiment.schema.json"
  );
  const schemas = {
    toolbar_badge: await schemaValidator(
      "resource://testing-common/ToolbarBadgeMessage.schema.json"
    ),
    cfr_doorhanger: await schemaValidator(
      "resource://testing-common/ExtensionDoorhanger.schema.json"
    ),
    spotlight: await schemaValidator(
      "resource://testing-common/Spotlight.schema.json"
    ),
    pb_newtab: await schemaValidator(
      "resource://testing-common/NewtabPromoMessage.schema.json"
    ),
    protections_panel: null, // TODO: There is no schema for protections_panel.
  };

  const messages = await OnboardingMessageProvider.getMessages();
  for (const message of messages) {
    const validator = schemas[message.template];

    if (validator === null) {
      continue;
    } else if (typeof validator === "undefined") {
      Assert.ok(
        false,
        `No schema validator found for message template ${message.template}. Please update this test to add one.`
      );
    } else {
      assertValid(
        validator,
        message,
        `Message ${message.id} validates as template ${message.template}`
      );
      assertValid(
        experimentValidator,
        message,
        `Message ${message.id} validates as MessagingExperiment`
      );
    }
  }
});
