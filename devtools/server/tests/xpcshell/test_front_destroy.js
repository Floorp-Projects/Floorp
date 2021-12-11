/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that fronts throw errors if they are called after being destroyed.
 */

"use strict";

// HACK: ServiceWorkerManager requires the "profile-change-teardown" to cleanly
// shutdown, and setting _profileInitialized to `true` will trigger those
// notifications (see /testing/xpcshell/head.js).
// eslint-disable-next-line no-undef
_profileInitialized = true;

add_task(async function test() {
  DevToolsServer.init();
  DevToolsServer.registerAllActors();

  info("Create and connect the DevToolsClient");
  const transport = DevToolsServer.connectPipe();
  const client = new DevToolsClient(transport);
  await client.connect();

  info("Get the device front and check calling getDescription() on it");
  const front = await client.mainRoot.getFront("device");
  const description = await front.getDescription();
  ok(
    !!description,
    "Check that the getDescription() method returns a valid response."
  );

  info("Destroy the device front and try calling getDescription again");
  front.destroy();
  Assert.throws(
    () => front.getDescription(),
    /Can not send request 'getDescription' because front 'device' is already destroyed\./,
    "Check device front throws when getDescription() is called after destroy()"
  );

  await client.close();
});
