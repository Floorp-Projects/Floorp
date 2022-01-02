/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check target-scoped actor lifetimes.
 */

const ACTORS_URL = EXAMPLE_URL + "testactors.js";
const TAB_URL = TEST_URI_ROOT + "doc_empty-tab-01.html";

add_task(async function test() {
  const tab = await addTab(TAB_URL);

  await registerActorInContentProcess(ACTORS_URL, {
    prefix: "testOne",
    constructor: "TestActor1",
    type: { target: true },
  });

  const target = await createAndAttachTargetForTab(tab);
  const { client } = target;
  const form = target.targetForm;

  await testTargetScopedActor(client, form);
  await removeTab(gBrowser.selectedTab);
  await target.destroy();
});

async function testTargetScopedActor(client, form) {
  ok(form.testOneActor, "Found the test target-scoped actor.");
  ok(
    form.testOneActor.includes("testOne"),
    "testOneActor's typeName should be used."
  );

  const response = await client.request({
    to: form.testOneActor,
    type: "ping",
  });
  is(response.pong, "pong", "Actor should respond to requests.");
}
