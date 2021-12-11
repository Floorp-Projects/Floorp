/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceCommand API around NETWORK_EVENT_STACKTRACE

const TEST_URI = `${URL_ROOT_SSL}network_document.html`;

const REQUEST_STUB = {
  code: `await fetch("/request_post_0.html", { method: "POST" });`,
  expected: {
    stacktraceAvailable: true,
    lastFrame: {
      filename:
        "https://example.com/browser/devtools/shared/commands/resource/tests/network_document.html",
      lineNumber: 1,
      columnNumber: 40,
      functionName: "triggerRequest",
      asyncCause: null,
    },
  },
};

add_task(async function() {
  info("Test network stacktraces events");
  const tab = await addTab(TEST_URI);
  const { client, resourceCommand, targetCommand } = await initResourceCommand(
    tab
  );

  const networkEvents = new Map();
  const stackTraces = new Map();

  function onResourceAvailable(resources) {
    for (const resource of resources) {
      if (
        resource.resourceType === resourceCommand.TYPES.NETWORK_EVENT_STACKTRACE
      ) {
        ok(
          !networkEvents.has(resource.resourceId),
          "The network event does not exist"
        );

        is(
          resource.stacktraceAvailable,
          REQUEST_STUB.expected.stacktraceAvailable,
          "The stacktrace is available"
        );
        is(
          JSON.stringify(resource.lastFrame),
          JSON.stringify(REQUEST_STUB.expected.lastFrame),
          "The last frame of the stacktrace is available"
        );

        stackTraces.set(resource.resourceId, true);
        return;
      }

      if (resource.resourceType === resourceCommand.TYPES.NETWORK_EVENT) {
        ok(
          stackTraces.has(resource.stacktraceResourceId),
          "The stack trace does exists"
        );

        networkEvents.set(resource.resourceId, true);
      }
    }
  }

  function onResourceUpdated() {}

  await resourceCommand.watchResources(
    [
      resourceCommand.TYPES.NETWORK_EVENT_STACKTRACE,
      resourceCommand.TYPES.NETWORK_EVENT,
    ],
    {
      onAvailable: onResourceAvailable,
      onUpdated: onResourceUpdated,
    }
  );

  await triggerNetworkRequests(tab.linkedBrowser, [REQUEST_STUB.code]);

  resourceCommand.unwatchResources(
    [
      resourceCommand.TYPES.NETWORK_EVENT_STACKTRACE,
      resourceCommand.TYPES.NETWORK_EVENT,
    ],
    {
      onAvailable: onResourceAvailable,
      onUpdated: onResourceUpdated,
    }
  );

  targetCommand.destroy();
  await client.close();
  BrowserTestUtils.removeTab(tab);
});
