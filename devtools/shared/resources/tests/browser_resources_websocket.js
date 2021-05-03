/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceCommand API around WEBSOCKET.

const ResourceCommand = require("devtools/shared/commands/resource/resource-command");

const IS_NUMBER = "IS_NUMBER";
const SHOULD_EXIST = "SHOULD_EXIST";

const targets = {
  TOP_LEVEL_DOCUMENT: "top-level-document",
  IN_PROCESS_IFRAME: "in-process-frame",
  OUT_PROCESS_IFRAME: "out-process-frame",
};

add_task(async function() {
  info("Testing the top-level document");
  await testWebsocketResources(targets.TOP_LEVEL_DOCUMENT);
  info("Testing the in-process iframe");
  await testWebsocketResources(targets.IN_PROCESS_IFRAME);
  info("Testing the out-of-process iframe");
  await testWebsocketResources(targets.OUT_PROCESS_IFRAME);
});

async function testWebsocketResources(target) {
  const tab = await addTab(URL_ROOT + "websocket_frontend.html");
  const { client, resourceWatcher, targetCommand } = await initResourceCommand(
    tab
  );

  const availableResources = [];
  function onResourceAvailable(resources) {
    availableResources.push(...resources);
  }

  await resourceWatcher.watchResources([resourceWatcher.TYPES.WEBSOCKET], {
    onAvailable: onResourceAvailable,
  });

  info("Check available resources at initial");
  is(
    availableResources.length,
    0,
    "Length of existing resources is correct at initial"
  );

  info("Check resource of opening websocket");
  await executeFunctionInContext(tab, target, "openConnection");

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
  await executeFunctionInContext(tab, target, "sendData", "test");

  await waitUntil(() => availableResources.length === 3);

  assertResource(availableResources[1], {
    wsMessageType: "frameSent",
    httpChannelId,
    data: {
      type: "sent",
      payload: "test",
      timeStamp: SHOULD_EXIST,
      finBit: SHOULD_EXIST,
      rsvBit1: SHOULD_EXIST,
      rsvBit2: SHOULD_EXIST,
      rsvBit3: SHOULD_EXIST,
      opCode: SHOULD_EXIST,
      mask: SHOULD_EXIST,
      maskBit: SHOULD_EXIST,
    },
  });
  assertResource(availableResources[2], {
    wsMessageType: "frameReceived",
    httpChannelId,
    data: {
      type: "received",
      payload: "test",
      timeStamp: SHOULD_EXIST,
      finBit: SHOULD_EXIST,
      rsvBit1: SHOULD_EXIST,
      rsvBit2: SHOULD_EXIST,
      rsvBit3: SHOULD_EXIST,
      opCode: SHOULD_EXIST,
      mask: SHOULD_EXIST,
      maskBit: SHOULD_EXIST,
    },
  });

  info("Check resource of closing websocket");
  await executeFunctionInContext(tab, target, "closeConnection");

  await waitUntil(() => availableResources.length === 6);
  assertResource(availableResources[3], {
    wsMessageType: "frameSent",
    httpChannelId,
    data: {
      type: "sent",
      payload: "",
      timeStamp: SHOULD_EXIST,
      finBit: SHOULD_EXIST,
      rsvBit1: SHOULD_EXIST,
      rsvBit2: SHOULD_EXIST,
      rsvBit3: SHOULD_EXIST,
      opCode: SHOULD_EXIST,
      mask: SHOULD_EXIST,
      maskBit: SHOULD_EXIST,
    },
  });
  assertResource(availableResources[4], {
    wsMessageType: "frameReceived",
    httpChannelId,
    data: {
      type: "received",
      payload: "",
      timeStamp: SHOULD_EXIST,
      finBit: SHOULD_EXIST,
      rsvBit1: SHOULD_EXIST,
      rsvBit2: SHOULD_EXIST,
      rsvBit3: SHOULD_EXIST,
      opCode: SHOULD_EXIST,
      mask: SHOULD_EXIST,
      maskBit: SHOULD_EXIST,
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

  function onExsistingResourceAvailable(resources) {
    existingResources.push(...resources);
  }

  await resourceWatcher.watchResources([resourceWatcher.TYPES.WEBSOCKET], {
    onAvailable: onExsistingResourceAvailable,
  });

  is(
    availableResources.length,
    existingResources.length,
    "Length of existing resources is correct"
  );

  for (let i = 0; i < availableResources.length; i++) {
    ok(
      availableResources[i] === existingResources[i],
      `The ${i}th resource is correct`
    );
  }

  await resourceWatcher.unwatchResources([resourceWatcher.TYPES.WEBSOCKET], {
    onAvailable: onResourceAvailable,
  });

  await resourceWatcher.unwatchResources([resourceWatcher.TYPES.WEBSOCKET], {
    onAvailable: onExsistingResourceAvailable,
  });

  targetCommand.destroy();
  await client.close();
  BrowserTestUtils.removeTab(tab);
}

/**
 * Execute global functions defined in the correct
 * target (top-level-window or frames) contexts.
 *
 * @param {object} tab The current window tab
 * @param {string} target A string identify if we want to test the top level document or iframes
 * @param {string} funcName The name of the global function which needs to be called.
 * @param {*} funcArgs The arguments to pass to the global function
 */
async function executeFunctionInContext(tab, target, funcName, ...funcArgs) {
  let browsingContext = tab.linkedBrowser.browsingContext;
  // If the target is an iframe get its window global
  if (target !== targets.TOP_LEVEL_DOCUMENT) {
    browsingContext = await SpecialPowers.spawn(
      tab.linkedBrowser,
      [target],
      async _target => {
        const iframe = content.document.getElementById(_target);
        return iframe.browsingContext;
      }
    );
  }

  return SpecialPowers.spawn(
    browsingContext,
    [funcName, funcArgs],
    async (_funcName, _funcArgs) => {
      await content.wrappedJSObject[_funcName](..._funcArgs);
    }
  );
}

function assertResource(resource, expected) {
  is(
    resource.resourceType,
    ResourceCommand.TYPES.WEBSOCKET,
    "Resource type is correct"
  );

  assertObject(resource, expected);
}

function assertObject(object, expected) {
  for (const field in expected) {
    if (typeof expected[field] === "object") {
      assertObject(object[field], expected[field]);
    } else if (expected[field] === SHOULD_EXIST) {
      ok(object[field] !== undefined, `The value of ${field} exists`);
    } else if (expected[field] === IS_NUMBER) {
      ok(!isNaN(object[field]), `The value of ${field} is number`);
    } else {
      is(object[field], expected[field], `The value of ${field} is correct`);
    }
  }
}
