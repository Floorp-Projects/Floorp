/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

ChromeUtils.defineESModuleGetters(this, {
  pktTelemetry: "chrome://pocket/content/pktTelemetry.sys.mjs",
});

function test_runner(test) {
  let testTask = async () => {
    // Before each
    const sandbox = sinon.createSandbox();
    try {
      await test({ sandbox });
    } finally {
      // After each
      sandbox.restore();
    }
  };

  // Copy the name of the test function to identify the test
  Object.defineProperty(testTask, "name", { value: test.name });
  add_task(testTask);
}

test_runner(async function test_createPingPayload({ sandbox }) {
  const impressionId = "{7fd5a1ac-6089-4212-91a7-fcdec1d2f533}";
  const creationDate = "18578";
  await SpecialPowers.pushPrefEnv({
    set: [["browser.newtabpage.activity-stream.impressionId", impressionId]],
  });
  sandbox.stub(pktTelemetry, "_profileCreationDate").returns(creationDate);
  const result = pktTelemetry.createPingPayload({ test: "test" });

  Assert.deepEqual(result, {
    test: "test",
    pocket_logged_in_status: false,
    profile_creation_date: creationDate,
    impression_id: impressionId,
  });
});

test_runner(async function test_generateStructuredIngestionEndpoint({
  sandbox,
}) {
  sandbox
    .stub(pktTelemetry, "_generateUUID")
    .returns("{7fd5a1ac-6089-4212-91a7-fcdec1d2f533}");
  const endpoint = pktTelemetry._generateStructuredIngestionEndpoint();
  Assert.equal(
    endpoint,
    "https://incoming.telemetry.mozilla.org/submit/activity-stream/pocket-button/1/7fd5a1ac-6089-4212-91a7-fcdec1d2f533"
  );
});

test_runner(async function test_submitPocketButtonPing({ sandbox }) {
  const creationDate = "19640";
  const impressionId = "{422e3da9-c694-4fd2-b676-8ae070156128}";
  sandbox.stub(pktTelemetry, "impressionId").value(impressionId);
  sandbox.stub(pktTelemetry, "_profileCreationDate").returns(creationDate);

  const eventAction = "some action like 'click'";
  const eventSource = "some source like 'save_button'";

  const assertConstantStuff = () => {
    Assert.equal(
      "{" + Glean.pocketButton.impressionId.testGetValue() + "}",
      impressionId
    );
    Assert.equal(Glean.pocketButton.pocketLoggedInStatus.testGetValue(), false);
    Assert.equal(
      Glean.pocketButton.profileCreationDate.testGetValue(),
      creationDate
    );

    Assert.equal(Glean.pocketButton.eventAction.testGetValue(), eventAction);
    Assert.equal(Glean.pocketButton.eventSource.testGetValue(), eventSource);
  };

  let submitted = false;
  GleanPings.pocketButton.testBeforeNextSubmit(() => {
    submitted = true;
    assertConstantStuff();
    Assert.equal(Glean.pocketButton.eventPosition.testGetValue(), null);
    Assert.equal(Glean.pocketButton.model.testGetValue(), null);
  });

  pktTelemetry.submitPocketButtonPing(eventAction, eventSource);
  Assert.ok(submitted, "Ping submitted successfully");

  submitted = false;
  GleanPings.pocketButton.testBeforeNextSubmit(() => {
    submitted = true;
    assertConstantStuff();
    Assert.equal(Glean.pocketButton.eventPosition.testGetValue(), 0);
    Assert.equal(Glean.pocketButton.model.testGetValue(), null);
  });

  pktTelemetry.submitPocketButtonPing(eventAction, eventSource, 0, null);
  Assert.ok(submitted, "Ping submitted successfully");

  submitted = false;
  GleanPings.pocketButton.testBeforeNextSubmit(() => {
    submitted = true;
    assertConstantStuff();
    // falsey but not undefined positions will be omitted.
    Assert.equal(Glean.pocketButton.eventPosition.testGetValue(), null);
    Assert.equal(
      Glean.pocketButton.model.testGetValue(),
      "some-really-groovy-model"
    );
  });

  pktTelemetry.submitPocketButtonPing(
    eventAction,
    eventSource,
    false,
    "some-really-groovy-model"
  );
  Assert.ok(submitted, "Ping submitted successfully");
});
