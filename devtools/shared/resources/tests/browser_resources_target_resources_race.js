/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test initial target resources are correctly retrieved even when several calls
 * to watchResources are made simultaneously.
 *
 * This checks a race condition which occurred when calling watchResources
 * simultaneously. This made the "second" call to watchResources miss existing
 * resources (in case those are emitted from the target instead of the watcher).
 * See Bug 1663896.
 */
add_task(async function() {
  // Disable the preloaded process as it creates processes intermittently
  // which forces the emission of RDP requests we aren't correctly waiting for.
  await pushPref("dom.ipc.processPrelaunch.enabled", false);

  const {
    client,
    resourceWatcher,
    targetCommand,
  } = await initMultiProcessResourceCommand();

  const expectedPlatformMessage = "expectedMessage";

  info("Log a message *before* calling ResourceCommand.watchResources");
  Services.console.logStringMessage(expectedPlatformMessage);

  info("Call watchResources from 2 separate call sites consecutively");

  // Empty onAvailable callback for CSS MESSAGES, we only want to check that
  // the second resource we watch correctly provides existing resources.
  const onCssMessageAvailable = resources => {};

  // First call to watchResources.
  // We do not await on `watchPromise1` here, in order to simulate simultaneous
  // calls to watchResources (which could come from 2 separate modules in a real
  // scenario).
  const initialWatchPromise = resourceWatcher.watchResources(
    [resourceWatcher.TYPES.CSS_MESSAGE],
    {
      onAvailable: onCssMessageAvailable,
    }
  );

  // `waitForNextResource` will trigger another call to `watchResources`.
  const onMessageReceived = waitForNextResource(
    resourceWatcher,
    resourceWatcher.TYPES.PLATFORM_MESSAGE,
    {
      ignoreExistingResources: false,
      predicate: r => r.message === expectedPlatformMessage,
    }
  );

  info("Waiting for the expected message to be received");
  await onMessageReceived;
  ok(true, "All the expected messages were received");

  info("Wait for the other watchResources promise to finish");
  await initialWatchPromise;

  // Unwatch all resources.
  resourceWatcher.unwatchResources([resourceWatcher.TYPES.CSS_MESSAGE], {
    onAvailable: onCssMessageAvailable,
  });

  Services.console.reset();
  targetCommand.destroy();
  await client.close();
});
