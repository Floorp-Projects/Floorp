/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceWatcher API around WEBSOCKET.

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

const TEST_URL = URL_ROOT + "websocket_frontend.html";
const IS_NUMBER = "IS_NUMBER";

add_task(async function() {
  const tab = await addTab(TEST_URL);

  const {
    client,
    resourceWatcher,
    targetList,
  } = await initResourceWatcherAndTarget(tab);

  info("Check available resources at initial");
  const availableResources = [];
  await resourceWatcher.watchResources([ResourceWatcher.TYPES.WEBSOCKET], {
    onAvailable: ({ resource }) => availableResources.push(resource),
  });
  is(
    availableResources.length,
    0,
    "Length of existing resources is correct at initial"
  );

  info("Check resource of opening websocket");
  await ContentTask.spawn(tab.linkedBrowser, [], async () => {
    await content.wrappedJSObject.openConnection();
  });
  await waitUntil(() => availableResources.length === 1);
  const httpChannelId = availableResources[0].httpChannelId;
  ok(httpChannelId, "httpChannelId is present in the resource");
  assertResource(availableResources[0], {
    wsMessageType: "webSocketOpened",
    effectiveURI:
      "ws://mochi.test:8888/browser/devtools/shared/resources/tests/websocket_backend",
    extensions: "permessage-deflate",
    protocols: "",
  });

  info("Check resource of sending/receiving the data via websocket");
  await ContentTask.spawn(tab.linkedBrowser, [], async () => {
    await content.wrappedJSObject.sendData("test");
  });
  await waitUntil(() => availableResources.length === 3);
  assertResource(availableResources[1], {
    wsMessageType: "frameSent",
    httpChannelId,
    data: {
      type: "sent",
      payload: "test",
    },
  });
  assertResource(availableResources[2], {
    wsMessageType: "frameReceived",
    httpChannelId,
    data: {
      type: "received",
      payload: "test",
    },
  });

  info("Check resource of closing websocket");
  await ContentTask.spawn(tab.linkedBrowser, [], async () => {
    await content.wrappedJSObject.closeConnection();
  });
  await waitUntil(() => availableResources.length === 6);
  assertResource(availableResources[3], {
    wsMessageType: "frameSent",
    httpChannelId,
    data: {
      type: "sent",
      payload: "",
    },
  });
  assertResource(availableResources[4], {
    wsMessageType: "frameReceived",
    httpChannelId,
    data: {
      type: "received",
      payload: "",
    },
  });
  assertResource(availableResources[5], {
    wsMessageType: "webSocketClosed",
    httpChannelId,
    code: IS_NUMBER,
    reason: "",
    wasClean: true,
  });

  info("Check existing resources");
  const existingResources = [];
  await resourceWatcher.watchResources([ResourceWatcher.TYPES.WEBSOCKET], {
    onAvailable: ({ resource }) => existingResources.push(resource),
  });
  is(
    availableResources.length,
    existingResources.length,
    "Length of existing resources is correct"
  );
  for (let i = 0; i < availableResources.length; i++) {
    const availableResource = availableResources[i];
    const existingResource = existingResources[i];
    ok(
      availableResource === existingResource,
      `The ${i}th resource is correct`
    );
  }

  await targetList.destroy();
  await client.close();
});

function assertResource(resource, expected) {
  is(
    resource.resourceType,
    ResourceWatcher.TYPES.WEBSOCKET,
    "Resource type is correct"
  );

  assertObject(resource, expected);
}

function assertObject(object, expected) {
  for (const field in expected) {
    if (typeof expected[field] === "object") {
      assertObject(object[field], expected[field]);
    } else if (expected[field] === IS_NUMBER) {
      ok(!isNaN(object[field]), `The value of ${field} is number`);
    } else {
      is(object[field], expected[field], `The value of ${field} is correct`);
    }
  }
}
