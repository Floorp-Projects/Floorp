/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that watcher front/actor APIs do not lead to create duplicate actors.

const TEST_URL = "data:text/html;charset=utf-8,Actor caching test";

add_task(async function () {
  info("Setup the test page with workers of all types");
  const tab = await addTab(TEST_URL);

  info("Create a target list for a tab target");
  const commands = await CommandsFactory.forTab(tab);
  const targetCommand = commands.targetCommand;
  await targetCommand.startListening();

  const { watcherFront } = targetCommand;
  ok(watcherFront, "A watcherFront is available on targetCommand");

  info("Check that getNetworkParentActor does not create duplicate actors");
  testActorGetter(
    watcherFront,
    () => watcherFront.getNetworkParentActor(),
    "networkParent"
  );

  info("Check that getBreakpointListActor does not create duplicate actors");
  testActorGetter(
    watcherFront,
    () => watcherFront.getBreakpointListActor(),
    "breakpoint-list"
  );

  info(
    "Check that getTargetConfigurationActor does not create duplicate actors"
  );
  testActorGetter(
    watcherFront,
    () => watcherFront.getTargetConfigurationActor(),
    "target-configuration"
  );

  info(
    "Check that getThreadConfigurationActor does not create duplicate actors"
  );
  testActorGetter(
    watcherFront,
    () => watcherFront.getThreadConfigurationActor(),
    "thread-configuration"
  );

  targetCommand.destroy();
  await commands.waitForRequestsToSettle();
  await commands.destroy();
});

/**
 * Check that calling an actor getter method on the watcher front leads to the
 * creation of at most 1 actor.
 */
async function testActorGetter(watcherFront, actorGetterFn, typeName) {
  checkPoolChildrenSize(watcherFront, typeName, 0);

  const actor = await actorGetterFn();
  checkPoolChildrenSize(watcherFront, typeName, 1);

  const otherActor = await actorGetterFn();
  is(actor, otherActor, "Returned the same actor for " + typeName);

  checkPoolChildrenSize(watcherFront, typeName, 1);
}

/**
 * Assert that a given parent pool has the expected number of children for
 * a given typeName.
 */
function checkPoolChildrenSize(parentPool, typeName, expected) {
  const children = [...parentPool.poolChildren()];
  const childrenByType = children.filter(pool => pool.typeName === typeName);
  is(
    childrenByType.length,
    expected,
    `${parentPool.actorID} should have ${expected} children of type ${typeName}`
  );
}
