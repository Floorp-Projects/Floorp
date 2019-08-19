/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Ensure target is closed if client is closed directly
function test() {
  waitForExplicitFinish();

  getParentProcessActors((client, target) => {
    target.on("close", () => {
      ok(true, "Target was closed");
      finish();
    });
    client.close();
  });
}
