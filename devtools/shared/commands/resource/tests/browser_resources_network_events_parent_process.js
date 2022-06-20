/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * !! AFTER MOVING OR RENAMING THIS METHOD, UPDATE `EXPECTED` CONSTANTS BELOW !!
 */
const createParentProcessRequests = async () => {
  info("Do some requests from the parent process");
  // The line:column for `fetch` should be EXPECTED_REQUEST_LINE_1/COL_1
  await fetch(FETCH_URI);

  const img = new Image();
  const onLoad = new Promise(r => img.addEventListener("load", r));
  // The line:column for `img` below should be EXPECTED_REQUEST_LINE_2/COL_2
  img.src = IMAGE_URI;
  await onLoad;
};

const EXPECTED_METHOD_NAME = "createParentProcessRequests";
const EXPECTED_REQUEST_LINE_1 = 12;
const EXPECTED_REQUEST_COL_1 = 9;
const EXPECTED_REQUEST_LINE_2 = 17;
const EXPECTED_REQUEST_COL_2 = 3;

// Test the ResourceCommand API around NETWORK_EVENT for the parent process

const FETCH_URI = "https://example.com/document-builder.sjs?html=foo";
// The img.src request gets cached regardless of `devtools.cache.disabled`.
// Add a random parameter to the request to bypass the cache.
const uuid = `${Date.now()}-${Math.random()}`;
const IMAGE_URI = URL_ROOT_SSL + "test_image.png?" + uuid;

add_task(async function testParentProcessRequests() {
  // The test expects the main process commands instance to receive resources
  // for content process requests.
  await pushPref("devtools.browsertoolbox.fission", true);

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

  await createParentProcessRequests();

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
  ok(fetchRequest.chromeContext, "The fetch request is privileged");

  const fetchStacktrace = receivedStacktraces[0].lastFrame;
  is(receivedStacktraces[0].resourceId, fetchRequest.stacktraceResourceId);
  is(fetchStacktrace.filename, gTestPath);
  is(fetchStacktrace.lineNumber, EXPECTED_REQUEST_LINE_1);
  is(fetchStacktrace.columnNumber, EXPECTED_REQUEST_COL_1);
  is(fetchStacktrace.functionName, EXPECTED_METHOD_NAME);
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
  ok(firstImageRequest.chromeContext, "The first image request is privileged");

  const firstImageStacktrace = receivedStacktraces[1].lastFrame;
  is(receivedStacktraces[1].resourceId, firstImageRequest.stacktraceResourceId);
  is(firstImageStacktrace.filename, gTestPath);
  is(firstImageStacktrace.lineNumber, EXPECTED_REQUEST_LINE_2);
  is(firstImageStacktrace.columnNumber, EXPECTED_REQUEST_COL_2);
  is(firstImageStacktrace.functionName, EXPECTED_METHOD_NAME);
  is(firstImageStacktrace.asyncCause, null);

  info("Assert the second image request");
  const secondImageRequest = receivedNetworkEvents[2];
  is(
    secondImageRequest.url,
    IMAGE_URI,
    "The third resource is for the second image request"
  );
  ok(secondImageRequest.fromCache, "The second image request is cached");
  ok(
    secondImageRequest.chromeContext,
    "The second image request is privileged"
  );

  await resourceCommand.unwatchResources(
    [resourceCommand.TYPES.NETWORK_EVENT],
    {
      onAvailable,
      onUpdated,
    }
  );

  await commands.destroy();
});
