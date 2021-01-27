/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceWatcher API around SERVER SENT EVENTS.

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

add_task(async function() {
  const tab = await addTab(URL_ROOT + "sse_frontend.html");

  const { client, resourceWatcher, targetList } = await initResourceWatcher(
    tab
  );

  info("Check available resources");
  const availableResources = [];
  await resourceWatcher.watchResources(
    [ResourceWatcher.TYPES.SERVER_SENT_EVENT],
    {
      onAvailable: resources => availableResources.push(...resources),
    }
  );

  info("Check resource of opening websocket");
  await ContentTask.spawn(tab.linkedBrowser, [], async () => {
    await content.wrappedJSObject.openConnection();
  });

  // We expect only 2 resources
  await waitUntil(() => availableResources.length === 2);

  // To make sure the channel id are the same
  const httpChannelId = availableResources[0].httpChannelId;

  ok(httpChannelId, "The channel id is set");
  is(typeof httpChannelId, "number", "The channel id is a number");

  assertResource(availableResources[0], {
    messageType: "eventReceived",
    httpChannelId,
    data: {
      payload: "Why so serious?",
      eventName: "message",
      lastEventId: "",
      retry: 5000,
    },
  });

  assertResource(availableResources[1], {
    messageType: "eventSourceConnectionClosed",
    httpChannelId,
  });

  await targetList.destroy();
  await client.close();
});

function assertResource(resource, expected) {
  is(
    resource.resourceType,
    ResourceWatcher.TYPES.SERVER_SENT_EVENT,
    "Resource type is correct"
  );

  checkObject(resource, expected);
}
