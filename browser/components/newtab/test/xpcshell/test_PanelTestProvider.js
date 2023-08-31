/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { PanelTestProvider } = ChromeUtils.importESModule(
  "resource://activity-stream/lib/PanelTestProvider.sys.mjs"
);

const MESSAGE_VALIDATORS = {};
let EXPERIMENT_VALIDATOR;

add_setup(async function setup() {
  const validators = await makeValidators();

  EXPERIMENT_VALIDATOR = validators.experimentValidator;
  Object.assign(MESSAGE_VALIDATORS, validators.messageValidators);
});

add_task(async function test_PanelTestProvider() {
  const messages = await PanelTestProvider.getMessages();

  const EXPECTED_MESSAGE_COUNTS = {
    cfr_doorhanger: 1,
    milestone_message: 0,
    update_action: 1,
    whatsnew_panel_message: 7,
    spotlight: 3,
    feature_callout: 1,
    pb_newtab: 2,
    toast_notification: 3,
  };

  const EXPECTED_TOTAL_MESSAGE_COUNT = Object.values(
    EXPECTED_MESSAGE_COUNTS
  ).reduce((a, b) => a + b, 0);

  Assert.strictEqual(
    messages.length,
    EXPECTED_TOTAL_MESSAGE_COUNT,
    "PanelTestProvider should have the correct number of messages"
  );

  const messageCounts = Object.assign(
    {},
    ...Object.keys(EXPECTED_MESSAGE_COUNTS).map(key => ({ [key]: 0 }))
  );

  for (const message of messages) {
    const validator = MESSAGE_VALIDATORS[message.template];
    Assert.ok(
      typeof validator !== "undefined",
      typeof validator !== "undefined"
        ? `Schema validator found for ${message.template}`
        : `No schema validator found for template ${message.template}. Please update this test to add one.`
    );
    assertValidates(
      validator,
      message,
      `Message ${message.id} validates as ${message.template} template`
    );
    assertValidates(
      EXPERIMENT_VALIDATOR,
      message,
      `Message ${message.id} validates as MessagingExperiment`
    );

    messageCounts[message.template]++;
  }

  for (const [template, count] of Object.entries(messageCounts)) {
    Assert.equal(
      count,
      EXPECTED_MESSAGE_COUNTS[template],
      `Expected ${EXPECTED_MESSAGE_COUNTS[template]} ${template} messages`
    );
  }
});

add_task(async function test_emptyMessage() {
  info(
    "Testing blank FxMS messages validate with the Messaging Experiment schema"
  );

  assertValidates(EXPERIMENT_VALIDATOR, {}, "Empty message should validate");
});
