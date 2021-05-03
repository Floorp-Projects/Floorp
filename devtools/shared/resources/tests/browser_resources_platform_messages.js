/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceWatcher API around PLATFORM_MESSAGE
// Reproduces assertions from: devtools/shared/webconsole/test/chrome/test_nsiconsolemessage.html

add_task(async function() {
  // Disable the preloaded process as it creates processes intermittently
  // which forces the emission of RDP requests we aren't correctly waiting for.
  await pushPref("dom.ipc.processPrelaunch.enabled", false);

  await testPlatformMessagesResources();
  await testPlatformMessagesResourcesWithIgnoreExistingResources();
});

async function testPlatformMessagesResources() {
  const {
    client,
    resourceWatcher,
    targetCommand,
  } = await initMultiProcessResourceWatcher();

  const cachedMessages = [
    "This is a cached message",
    "This is another cached message",
  ];
  const liveMessages = [
    "This is a live message",
    "This is another live message",
  ];
  const expectedMessages = [...cachedMessages, ...liveMessages];
  const receivedMessages = [];

  info(
    "Log some messages *before* calling ResourceWatcher.watchResources in order to assert the behavior of already existing messages."
  );
  Services.console.logStringMessage(expectedMessages[0]);
  Services.console.logStringMessage(expectedMessages[1]);

  let done;
  const onAllMessagesReceived = new Promise(resolve => (done = resolve));
  const onAvailable = resources => {
    for (const resource of resources) {
      if (!expectedMessages.includes(resource.message)) {
        continue;
      }

      is(
        resource.targetFront,
        targetCommand.targetFront,
        "The targetFront property is the expected one"
      );

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

      const isCachedMessage = receivedMessages.length <= cachedMessages.length;
      is(
        resource.isAlreadyExistingResource,
        isCachedMessage,
        "isAlreadyExistingResource has the expected value"
      );

      if (receivedMessages.length == expectedMessages.length) {
        done();
      }
    }
  };

  await resourceWatcher.watchResources(
    [resourceWatcher.TYPES.PLATFORM_MESSAGE],
    {
      onAvailable,
    }
  );

  info(
    "Now log messages *after* the call to ResourceWatcher.watchResources and after having received all existing messages"
  );
  Services.console.logStringMessage(expectedMessages[2]);
  Services.console.logStringMessage(expectedMessages[3]);

  info("Waiting for all expected messages to be received");
  await onAllMessagesReceived;
  ok(true, "All the expected messages were received");

  Services.console.reset();
  targetCommand.destroy();
  await client.close();
}

async function testPlatformMessagesResourcesWithIgnoreExistingResources() {
  const {
    client,
    resourceWatcher,
    targetCommand,
  } = await initMultiProcessResourceWatcher();

  info(
    "Check whether onAvailable will not be called with existing platform messages"
  );
  const expectedMessages = ["This is 1st message", "This is 2nd message"];
  Services.console.logStringMessage(expectedMessages[0]);
  Services.console.logStringMessage(expectedMessages[1]);

  const availableResources = [];
  await resourceWatcher.watchResources(
    [resourceWatcher.TYPES.PLATFORM_MESSAGE],
    {
      onAvailable: resources => {
        for (const resource of resources) {
          if (!expectedMessages.includes(resource.message)) {
            continue;
          }

          availableResources.push(resource);
        }
      },
      ignoreExistingResources: true,
    }
  );
  is(
    availableResources.length,
    0,
    "onAvailable wasn't called for existing platform messages"
  );

  info(
    "Check whether onAvailable will be called with the future platform messages"
  );
  Services.console.logStringMessage(expectedMessages[0]);
  Services.console.logStringMessage(expectedMessages[1]);

  await waitUntil(() => availableResources.length === expectedMessages.length);
  for (let i = 0; i < expectedMessages.length; i++) {
    const resource = availableResources[i];
    const { message } = resource;
    const expected = expectedMessages[i];
    is(message, expected, `Message[${i}] is correct`);
    is(
      resource.isAlreadyExistingResource,
      false,
      "isAlreadyExistingResource is false since we ignore existing resources"
    );
  }

  Services.console.reset();
  targetCommand.destroy();
  await client.close();
}
