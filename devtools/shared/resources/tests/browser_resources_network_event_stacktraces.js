/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceWatcher API around NETWORK_EVENT_STACKTRACE

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

const TEST_URI = `${URL_ROOT_SSL}network_document.html`;

const REQUEST_STUB = {
  code: `await fetch("/request_post_0.html", { method: "POST" });`,
  expected: {
    stacktraceAvailable: true,
    lastFrame: {
      filename:
        "https://example.com/browser/devtools/shared/resources/tests/network_document.html",
      lineNumber: 1,
      columnNumber: 40,
      functionName: "triggerRequest",
      asyncCause: null,
    },
  },
};

add_task(async function() {
  info("Test network stacktraces events legacy listener");
  await pushPref("devtools.testing.enableServerWatcherSupport", false);
  await testNetworkEventStackTraceResources(REQUEST_STUB);

  // These tests would be enabled when the server-side work for stacktraces is done. See Bug 1644191
  // info("Test network stacktrace events server listener");
  // await pushPref("devtools.testing.enableServerWatcherSupport", true);
  // await testNetworkEventStackTraceResources(REQUEST_STUB);
});

async function testNetworkEventStackTraceResources(requestStub) {
  const tab = await addTab(TEST_URI);
  const { client, resourceWatcher, targetList } = await initResourceWatcher(
    tab
  );

  const networkEvents = new Map();
  const stackTraces = new Map();

  function onResourceAvailable(resources) {
    for (const resource of resources) {
      if (
        resource.resourceType === ResourceWatcher.TYPES.NETWORK_EVENT_STACKTRACE
      ) {
        ok(
          !networkEvents.has(resource.resourceId),
          "The network event does not exist"
        );

        is(
          resource.stacktraceAvailable,
          requestStub.expected.stacktraceAvailable,
          "The stacktrace is available"
        );
        is(
          JSON.stringify(resource.lastFrame),
          JSON.stringify(requestStub.expected.lastFrame),
          "The last frame of the stacktrace is available"
        );

        stackTraces.set(resource.resourceId, true);
        return;
      }

      if (resource.resourceType === ResourceWatcher.TYPES.NETWORK_EVENT) {
        ok(
          stackTraces.has(resource.stacktraceResourceId),
          "The stack trace does exists"
        );

        networkEvents.set(resource.resourceId, true);
      }
    }
  }

  function onResourceUpdated() {}

  await resourceWatcher.watchResources(
    [
      ResourceWatcher.TYPES.NETWORK_EVENT_STACKTRACE,
      ResourceWatcher.TYPES.NETWORK_EVENT,
    ],
    {
      onAvailable: onResourceAvailable,
      onUpdated: onResourceUpdated,
    }
  );

  await triggerNetworkRequests(tab.linkedBrowser, [requestStub.code]);

  resourceWatcher.unwatchResources(
    [
      ResourceWatcher.TYPES.NETWORK_EVENT_STACKTRACE,
      ResourceWatcher.TYPES.NETWORK_EVENT,
    ],
    {
      onAvailable: onResourceAvailable,
      onUpdated: onResourceUpdated,
    }
  );

  await targetList.destroy();
  await client.close();
  BrowserTestUtils.removeTab(tab);
}
