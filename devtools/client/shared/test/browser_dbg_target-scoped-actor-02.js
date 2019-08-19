/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check target-scoped actor lifetimes.
 */

const ACTORS_URL = EXAMPLE_URL + "testactors.js";
const TAB_URL = TEST_URI_ROOT + "doc_empty-tab-01.html";

add_task(async function() {
  const tab = await addTab(TAB_URL);

  await registerActorInContentProcess(ACTORS_URL, {
    prefix: "testOne",
    constructor: "TestActor1",
    type: { target: true },
  });

  const target = await TargetFactory.forTab(tab);
  await target.attach();
  const { client } = target;
  const form = target.targetForm;

  await testTargetScopedActor(client, form);
  await closeTab(client, form);
  await target.destroy();
});

async function testTargetScopedActor(client, form) {
  ok(form.testOneActor, "Found the test target-scoped actor.");
  ok(
    form.testOneActor.includes("testOne"),
    "testOneActor's actorPrefix should be used."
  );

  const response = await client.request({
    to: form.testOneActor,
    type: "ping",
  });
  is(response.pong, "pong", "Actor should respond to requests.");
}

async function closeTab(client, form) {
  // We need to start listening for the rejection before removing the tab
  /* eslint-disable-next-line mozilla/rejects-requires-await*/
  const onReject = Assert.rejects(
    client.request({ to: form.testOneActor, type: "ping" }),
    err =>
      err.message ===
      `'ping' active request packet to '${form.testOneActor}' ` +
        `can't be sent as the connection just closed.`,
    "testOneActor went away."
  );
  await removeTab(gBrowser.selectedTab);
  await onReject;
}
