/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceCommand API around SERVER SENT EVENTS.

const ResourceCommand = require("devtools/shared/commands/resource/resource-command");

const targets = {
  TOP_LEVEL_DOCUMENT: "top-level-document",
  IN_PROCESS_IFRAME: "in-process-frame",
  OUT_PROCESS_IFRAME: "out-process-frame",
};

add_task(async function() {
  info("Testing the top-level document");
  await testServerSentEventResources(targets.TOP_LEVEL_DOCUMENT);
  info("Testing the in-process iframe");
  await testServerSentEventResources(targets.IN_PROCESS_IFRAME);
  info("Testing the out-of-process iframe");
  await testServerSentEventResources(targets.OUT_PROCESS_IFRAME);
});

async function testServerSentEventResources(target) {
  const tab = await addTab(URL_ROOT + "sse_frontend.html");

  const { client, resourceCommand, targetCommand } = await initResourceCommand(
    tab
  );

  const availableResources = [];

  function onResourceAvailable(resources) {
    availableResources.push(...resources);
  }

  await resourceCommand.watchResources(
    [resourceCommand.TYPES.SERVER_SENT_EVENT],
    { onAvailable: onResourceAvailable }
  );

  openConnectionInContext(tab, target);

  info("Check available resources");
  // We expect only 2 resources
  await waitUntil(() => availableResources.length === 2);

  info("Check resource details");
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

  await resourceCommand.unwatchResources(
    [resourceCommand.TYPES.SERVER_SENT_EVENT],
    { onAvailable: onResourceAvailable }
  );

  await targetCommand.destroy();
  await client.close();
  BrowserTestUtils.removeTab(tab);
}

function assertResource(resource, expected) {
  is(
    resource.resourceType,
    ResourceCommand.TYPES.SERVER_SENT_EVENT,
    "Resource type is correct"
  );

  checkObject(resource, expected);
}

async function openConnectionInContext(tab, target) {
  let browsingContext = tab.linkedBrowser.browsingContext;
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
  await SpecialPowers.spawn(browsingContext, [], async () => {
    await content.wrappedJSObject.openConnection();
  });
}
