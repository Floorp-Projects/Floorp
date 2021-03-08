/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test support methods on Target, such as `hasActor` and `getTrait`.

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
