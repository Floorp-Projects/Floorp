/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the server ResourceCommand are destroyed when the associated target actors
// are destroyed.

add_task(async function() {
  const tab = await addTab("data:text/html,Test");
  const { client, resourceCommand, targetCommand } = await initResourceCommand(
    tab
  );

  // Start watching for console messages. We don't care about messages here, only the
  // registration/destroy mechanism, so we make onAvailable a no-op function.
  await resourceCommand.watchResources(
    [resourceCommand.TYPES.CONSOLE_MESSAGE],
    {
      onAvailable: () => {},
    }
  );

  info(
    "Spawn a content task in order to be able to manipulate actors and resource watchers directly"
  );
  await ContentTask.spawn(tab.linkedBrowser, [], function() {
    const { require } = ChromeUtils.import(
      "resource://devtools/shared/Loader.jsm"
    );
    const {
      TargetActorRegistry,
    } = require("devtools/server/actors/targets/target-actor-registry.jsm");
    const {
      getResourceWatcher,
      TYPES,
    } = require("devtools/server/actors/resources/index");

    // Retrieve the target actor instance and its watcher for console messages
    const targetActor = TargetActorRegistry.getTargetActor(
      content.browsingContext.browserId
    );
    const watcher = getResourceWatcher(targetActor, TYPES.CONSOLE_MESSAGE);

    // Storing the target actor in the global so we can retrieve it later, even if it
    // was destroyed
    content._testTargetActor = targetActor;

    is(!!watcher, true, "The console message resource watcher was created");
  });

  info("Close the client, which will destroy the target");
  targetCommand.destroy();
  await client.close();

  info(
    "Spawn a content task in order to run some assertions on actors and resource watchers directly"
  );
  await ContentTask.spawn(tab.linkedBrowser, [], function() {
    const { require } = ChromeUtils.import(
      "resource://devtools/shared/Loader.jsm"
    );
    const {
      getResourceWatcher,
      TYPES,
    } = require("devtools/server/actors/resources/index");

    ok(
      content._testTargetActor && !content._testTargetActor.actorID,
      "The target was destroyed when the client was closed"
    );

    // Retrieve the console message resource watcher
    const watcher = getResourceWatcher(
      content._testTargetActor,
      TYPES.CONSOLE_MESSAGE
    );

    is(
      !!watcher,
      false,
      "The console message resource watcher isn't registered anymore after the target was destroyed"
    );

    // Cleanup work variable
    delete content._testTargetActor;
  });
});
