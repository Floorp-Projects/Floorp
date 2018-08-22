/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that we can attach and detach to the PromisesActor under the correct
 * states.
 */

const { PromisesFront } = require("devtools/shared/fronts/promises");

add_task(async function() {
  const client = await startTestDebuggerServer("promises-actor-test");
  const parentProcessActors = await getParentProcessActors(client);

  // We have to attach the chrome target actor before playing with the PromiseActor
  await attachTab(client, parentProcessActors);
  await testAttach(client, parentProcessActors);

  const response = await listTabs(client);
  const targetTab = findTab(response.tabs, "promises-actor-test");
  ok(targetTab, "Found our target tab.");

  const [ tabResponse ] = await attachTab(client, targetTab);

  await testAttach(client, tabResponse);

  await close(client);
});

async function testAttach(client, parent) {
  const promises = PromisesFront(client, parent);

  try {
    await promises.detach();
    ok(false, "Should not be able to detach when in a detached state.");
  } catch (e) {
    ok(true, "Expected detach to fail when already in a detached state.");
  }

  await promises.attach();
  ok(true, "Expected attach to succeed.");

  try {
    await promises.attach();
    ok(false, "Should not be able to attach when in an attached state.");
  } catch (e) {
    ok(true, "Expected attach to fail when already in an attached state.");
  }

  await promises.detach();
  ok(true, "Expected detach to succeed.");
}
