/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { InflightAssetsMessageProvider } = ChromeUtils.importESModule(
  "resource://testing-common/InflightAssetsMessageProvider.sys.mjs"
);

const MESSAGE_VALIDATORS = {};
let EXPERIMENT_VALIDATOR;

add_setup(async function setup() {
  const validators = await makeValidators();

  EXPERIMENT_VALIDATOR = validators.experimentValidator;
  Object.assign(MESSAGE_VALIDATORS, validators.messageValidators);
});

add_task(function test_InflightAssetsMessageProvider() {
  const messages = InflightAssetsMessageProvider.getMessages();

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
      `Message ${message.id} validates as a MessagingExperiment`
    );
  }
});
