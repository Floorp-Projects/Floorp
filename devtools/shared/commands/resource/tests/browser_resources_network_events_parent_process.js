/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceCommand API around NETWORK_EVENT for the parent process

const FETCH_URI = "https://example.com/document-builder.sjs?html=foo";
const IMAGE_URI = URL_ROOT_SSL + "test_image.png";

add_task(async function testParentProcessRequests() {
  const commands = await CommandsFactory.forMainProcess();
  await commands.targetCommand.startListening();
  const { resourceCommand } = commands;

  const receivedNetworkEvents = [];
  const receivedStacktraces = [];
  const onAvailable = resources => {
    for (const resource of resources) {
      if (resource.resourceType == resourceCommand.TYPES.NETWORK_EVENT) {
        receivedNetworkEvents.push(resource);
      } else if (
        resource.resourceType == resourceCommand.TYPES.NETWORK_EVENT_STACKTRACE
      ) {
        receivedStacktraces.push(resource);
      }
    }
  };
  const onUpdated = updates => {
    for (const { resource } of updates) {
      is(
        resource.resourceType,
        resourceCommand.TYPES.NETWORK_EVENT,
        "Received a network update event resource"
      );
    }
  };

  await resourceCommand.watchResources(
    [
      resourceCommand.TYPES.NETWORK_EVENT,
      resourceCommand.TYPES.NETWORK_EVENT_STACKTRACE,
    ],
    {
      ignoreExistingResources: true,
      onAvailable,
      onUpdated,
    }
  );

  info("Do some requests from the parent process");
  await fetch(FETCH_URI);

  const img = new Image();
  const onLoad = new Promise(r => img.addEventListener("load", r));
  img.src = IMAGE_URI;
  await onLoad;

  const img2 = new Image();
  img2.src = IMAGE_URI;

  info("Wait for the network events");
  await waitFor(() => receivedNetworkEvents.length == 3);
  info("Wait for the network events stack traces");
  // Note that we aren't getting any stacktrace for the second cached request
  await waitFor(() => receivedStacktraces.length == 2);

  info("Assert the fetch request");
  const fetchRequest = receivedNetworkEvents[0];
  is(
    fetchRequest.url,
    FETCH_URI,
    "The first resource is for the fetch request"
  );
  const fetchStacktrace = receivedStacktraces[0].lastFrame;
  is(receivedStacktraces[0].resourceId, fetchRequest.stacktraceResourceId);
  is(fetchStacktrace.filename, gTestPath);
  is(fetchStacktrace.lineNumber, 52);
  is(fetchStacktrace.columnNumber, 9);
  is(fetchStacktrace.functionName, "testParentProcessRequests");
  is(fetchStacktrace.asyncCause, null);

  async function getResponseContent(networkEvent) {
    const packet = {
      to: networkEvent.actor,
      type: "getResponseContent",
    };
    const response = await commands.client.request(packet);
    return response.content.text;
  }

  const fetchContent = await getResponseContent(fetchRequest);
  is(fetchContent, "foo");

  info("Assert the first image request");
  const firstImageRequest = receivedNetworkEvents[1];
  is(
    firstImageRequest.url,
    IMAGE_URI,
    "The second resource is for the first image request"
  );
  ok(!firstImageRequest.fromCache, "The first image request isn't cached");
  const firstImageStacktrace = receivedStacktraces[1].lastFrame;
  is(receivedStacktraces[1].resourceId, firstImageRequest.stacktraceResourceId);
  is(firstImageStacktrace.filename, gTestPath);
  is(firstImageStacktrace.lineNumber, 56);
  is(firstImageStacktrace.columnNumber, 3);
  is(firstImageStacktrace.functionName, "testParentProcessRequests");
  is(firstImageStacktrace.asyncCause, null);

  info("Assert the second image request");
  const secondImageRequest = receivedNetworkEvents[2];
  is(
    secondImageRequest.url,
    IMAGE_URI,
    "The third resource is for the second image request"
  );
  ok(secondImageRequest.fromCache, "The second image request is cached");

  await resourceCommand.unwatchResources(
    [resourceCommand.TYPES.NETWORK_EVENT],
    {
      onAvailable,
      onUpdated,
    }
  );

  await commands.destroy();
});
