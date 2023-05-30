/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { CFRMessageProvider } = ChromeUtils.importESModule(
  "resource://activity-stream/lib/CFRMessageProvider.sys.mjs"
);

add_task(async function test_multiMessageTreatment() {
  const { experimentValidator } = await makeValidators();
  // Use the entire list of messages as if it was a single treatment branch's
  // feature value.
  let messages = await CFRMessageProvider.getMessages();
  let featureValue = { template: "multi", messages };
  assertValidates(
    experimentValidator,
    featureValue,
    `Multi-message treatment validates as MessagingExperiment`
  );
  for (const message of messages) {
    assertValidates(
      experimentValidator,
      message,
      `Message ${message.id} validates as MessagingExperiment`
    );
  }

  // Add an invalid message to the list and make sure it fails validation.
  messages.push({
    id: "INVALID_MESSAGE",
    template: "cfr_doorhanger",
  });
  const result = experimentValidator.validate(featureValue);
  Assert.ok(
    !(result.valid && result.errors.length === 0),
    "Multi-message treatment with invalid message fails validation"
  );
});
