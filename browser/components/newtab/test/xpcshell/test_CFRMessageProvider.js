/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { CFRMessageProvider } = ChromeUtils.import(
  "resource://activity-stream/lib/CFRMessageProvider.jsm"
);

add_task(async function test_cfrMessages() {
  const { experimentValidator, messageValidators } = await makeValidators();

  const messages = await CFRMessageProvider.getMessages();
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
