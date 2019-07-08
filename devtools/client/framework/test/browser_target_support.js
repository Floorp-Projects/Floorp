/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test support methods on Target, such as `hasActor`, `getActorDescription`,
// `actorHasMethod` and `getTrait`.

async function testTarget(client, target) {
  await target.attach();

  is(
    target.hasActor("inspector"),
    true,
    "target.hasActor() true when actor exists."
  );
  is(
    target.hasActor("storage"),
    true,
    "target.hasActor() true when actor exists."
  );
  is(
    target.hasActor("notreal"),
    false,
    "target.hasActor() false when actor does not exist."
  );
  // Create a front to ensure the actor is loaded
  await target.getFront("storage");

  let desc = await target.getActorDescription("storage");
  is(
    desc.typeName,
    "storage",
    "target.getActorDescription() returns definition data for corresponding actor"
  );
  is(
    desc.events["stores-update"].type,
    "storesUpdate",
    "target.getActorDescription() returns event data for corresponding actor"
  );

  desc = await target.getActorDescription("nope");
  is(
    desc,
    undefined,
    "target.getActorDescription() returns undefined for non-existing actor"
  );
  desc = await target.getActorDescription();
  is(
    desc,
    undefined,
    "target.getActorDescription() returns undefined for undefined actor"
  );

  let hasMethod = await target.actorHasMethod("storage", "listStores");
  is(
    hasMethod,
    true,
    "target.actorHasMethod() returns true for existing actor with method"
  );
  hasMethod = await target.actorHasMethod("localStorage", "nope");
  is(
    hasMethod,
    false,
    "target.actorHasMethod() returns false for existing actor with no method"
  );
  hasMethod = await target.actorHasMethod("nope", "nope");
  is(
    hasMethod,
    false,
    "target.actorHasMethod() returns false for non-existing actor with no method"
  );
  hasMethod = await target.actorHasMethod();
  is(
    hasMethod,
    false,
    "target.actorHasMethod() returns false for undefined params"
  );

  is(
    target.getTrait("giddyup"),
    undefined,
    "target.getTrait() returns undefined when trait does not exist"
  );

  close(target, client);
}

// Ensure target is closed if client is closed directly
function test() {
  waitForExplicitFinish();

  getParentProcessActors(testTarget);
}

function close(target, client) {
  target.on("close", () => {
    ok(true, "Target was closed");
    finish();
  });
  client.close();
}
