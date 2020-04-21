/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceWatcher API around PLATFORM_MESSAGES
// Reproduces assertions from: devtools/shared/webconsole/test/chrome/test_nsiconsolemessage.html

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

add_task(async function() {
  // Disable the preloaded process as it creates processes intermittently
  // which forces the emission of RDP requests we aren't correctly waiting for.
  await pushPref("dom.ipc.processPrelaunch.enabled", false);

  const {
    client,
    resourceWatcher,
    targetList,
  } = await initResourceWatcherAndTarget();

  const expectedMessages = [
    "This is a cached message",
    "This is another cached message",
    "This is a live message",
    "This is another live message",
  ];
  const receivedMessages = [];

  info(
    "Log some messages *before* calling ResourceWatcher.watch in order to assert the behavior of already existing messages."
  );
  Services.console.logStringMessage(expectedMessages[0]);
  Services.console.logStringMessage(expectedMessages[1]);

  let done;
  const onAllMessagesReceived = new Promise(resolve => (done = resolve));

  await resourceWatcher.watch(
    [ResourceWatcher.TYPES.PLATFORM_MESSAGES],
    ({ resourceType, targetFront, resource }) => {
      if (!expectedMessages.includes(resource.message)) {
        return;
      }

      receivedMessages.push(resource.message);
      is(
        resource.message,
        expectedMessages[receivedMessages.length - 1],
        `Received the expected «${resource.message}» message, in the expected order`
      );

      ok(
        resource.timeStamp.toString().match(/^\d+$/),
        "The resource has a timeStamp property"
      );

      if (receivedMessages.length == expectedMessages.length) {
        done();
      }
    }
  );

  info(
    "Now log messages *after* the call to ResourceWatcher.watch and after having received all existing messages"
  );
  Services.console.logStringMessage(expectedMessages[2]);
  Services.console.logStringMessage(expectedMessages[3]);

  info("Waiting for all expected messages to be received");
  await onAllMessagesReceived;
  ok(true, "All the expected messages were received");

  Services.console.reset();
  targetList.stopListening();
  await client.close();
});
