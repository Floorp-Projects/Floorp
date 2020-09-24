/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceWatcher API around NETWORK_EVENT

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

const EXAMPLE_DOMAIN = "https://example.com/";
const TEST_URI = `${URL_ROOT_SSL}network_document.html`;

add_task(async function() {
  info("Test network events legacy listener");
  await pushPref("devtools.testing.enableServerWatcherSupport", false);
  await testNetworkEventResourcesWithExistingResources();
  await testNetworkEventResourcesWithoutExistingResources();

  // These tests would be enabled when the server-side work is done. See Bug 1644191
  // info("Test network events server listener");
  // await pushPref("devtools.testing.enableServerWatcherSupport", true);
  // await testNetworkEventResources();
  // await testNetworkEventResourcesWithIgnoreExistingResources();
});

const UPDATES = [
  "requestHeaders",
  "requestCookies",
  "responseStart",
  "securityInfo",
  "responseHeaders",
  "responseCookies",
  "eventTimings",
  "responseContent",
];

async function testNetworkEventResourcesWithExistingResources() {
  info(`Tests for network event resources with the existing resources`);
  await testNetworkEventResources({
    ignoreExistingResources: false,
    // 1 available event fired, for the existing resource in the cache.
    // 1 available event fired, when live request is created.
    expectedOnAvailableCounts: 2,
    // 8 update events fired, when live request is updated.
    expectedOnUpdatedCounts: 8,
    expectedResourcesOnAvailable: {
      [`${EXAMPLE_DOMAIN}existing_post.html`]: {
        resourceType: ResourceWatcher.TYPES.NETWORK_EVENT,
        request: {
          url: `${EXAMPLE_DOMAIN}existing_post.html`,
          method: "POST",
        },
        // gets reset based on the type of request
        updates: [],
      },
      [`${EXAMPLE_DOMAIN}live_get.html`]: {
        resourceType: ResourceWatcher.TYPES.NETWORK_EVENT,
        request: {
          url: `${EXAMPLE_DOMAIN}live_get.html`,
          method: "GET",
        },
        // Throttling makes us receive the available event
        // after processing all the updates events
        updates: UPDATES,
      },
    },
    expectedResourcesOnUpdated: {
      [`${EXAMPLE_DOMAIN}live_get.html`]: {
        resourceType: ResourceWatcher.TYPES.NETWORK_EVENT,
        request: {
          url: `${EXAMPLE_DOMAIN}live_get.html`,
          method: "GET",
        },
        updates: UPDATES,
      },
    },
  });
}

async function testNetworkEventResourcesWithoutExistingResources() {
  info(`Tests for network event resources without the existing resources`);
  await testNetworkEventResources({
    ignoreExistingResources: true,
    // 1 available event fired, when live request is created.
    expectedOnAvailableCounts: 1,
    // 8 update events fired, when live request is updated.
    expectedOnUpdatedCounts: 8,
    expectedResourcesOnAvailable: {
      [`${EXAMPLE_DOMAIN}live_get.html`]: {
        resourceType: ResourceWatcher.TYPES.NETWORK_EVENT,
        request: {
          url: `${EXAMPLE_DOMAIN}live_get.html`,
          method: "GET",
        },
        // Throttling makes us receive the available event
        // after processing all the updates events
        updates: UPDATES,
      },
    },
    expectedResourcesOnUpdated: {
      [`${EXAMPLE_DOMAIN}live_get.html`]: {
        resourceType: ResourceWatcher.TYPES.NETWORK_EVENT,
        request: {
          url: `${EXAMPLE_DOMAIN}live_get.html`,
          method: "GET",
        },
        updates: UPDATES,
      },
    },
  });
}

async function testNetworkEventResources(options) {
  const tab = await addTab(TEST_URI);
  const {
    client,
    resourceWatcher,
    targetList,
  } = await initResourceWatcherAndTarget(tab);

  const actualResourcesOnAvailable = {};
  const actualResourcesOnUpdated = {};

  info(
    `Trigger some network requests *before* calling ResourceWatcher.watchResources
     in order to assert the behavior of already existing network events.`
  );
  let onResourceAvailable = () => {};
  let onResourceUpdated = () => {};
  const waitOnAllExpectedUpdatesForExistingRequests = new Promise(resolve => {
    const existingRequestUrl = `${EXAMPLE_DOMAIN}existing_post.html`;

    onResourceAvailable = resources => {
      for (const resource of resources) {
        // A blocked request would only have two updates so lets also resolve here
        if (
          resource.request.url == existingRequestUrl &&
          resource.blockedReason &&
          resource.updates.length == 2
        ) {
          // Reset the updates expectation as the request is blocked
          if (options.expectedResourcesOnAvailable[resource.request.url]) {
            options.expectedResourcesOnAvailable[
              resource.request.url
            ].updates = [...resource.updates];
          }
          resolve();
        }
      }
    };

    onResourceUpdated = updates => {
      for (const { resource } of updates) {
        // Wait until all the update events have fired for the existing request.
        // Handle both blocked and unblocked requests
        if (
          resource.request.url == existingRequestUrl &&
          (resource.updates.length == 8 ||
            (resource.blockedReason && resource.updates.length == 2))
        ) {
          // Makes sure the expectation always correct (for either blocked or unblocked requests)
          if (options.expectedResourcesOnAvailable[resource.request.url]) {
            options.expectedResourcesOnAvailable[
              resource.request.url
            ].updates = [...resource.updates];
          }
          resolve();
        }
      }
    };

    resourceWatcher
      .watchResources([ResourceWatcher.TYPES.NETWORK_EVENT], {
        onAvailable: onResourceAvailable,
        onUpdated: onResourceUpdated,
      })
      .then(() => {
        // We can only trigger the requests once `watchResources` settles, otherwise the
        // thread might be paused.
        triggerNetworkRequests(tab.linkedBrowser, EXISTING_REQUESTS_COMMANDS);
      });
  });

  await waitOnAllExpectedUpdatesForExistingRequests;

  let {
    expectedOnAvailableCounts,
    expectedOnUpdatedCounts,
    ignoreExistingResources,
  } = options;

  const waitForAllOnAvailableEvents = waitUntil(
    () => expectedOnAvailableCounts == 0
  );
  const waitForAllOnUpdatedEvents = waitUntil(
    () => expectedOnUpdatedCounts == 0
  );

  const onAvailable = resources => {
    for (const resource of resources) {
      is(
        resource.resourceType,
        ResourceWatcher.TYPES.NETWORK_EVENT,
        "Received a network event resource"
      );
      actualResourcesOnAvailable[resource.request.url] = {
        resourceId: resource.resourceId,
        resourceType: resource.resourceType,
        request: resource.request,
        updates: [...resource.updates],
      };
      expectedOnAvailableCounts--;
    }
  };

  const onUpdated = updates => {
    for (const { resource } of updates) {
      is(
        resource.resourceType,
        ResourceWatcher.TYPES.NETWORK_EVENT,
        "Received a network update event resource"
      );
      actualResourcesOnUpdated[resource.request.url] = {
        resourceId: resource.resourceId,
        resourceType: resource.resourceType,
        request: resource.request,
        updates: [...resource.updates],
      };
      expectedOnUpdatedCounts--;
    }
  };

  await resourceWatcher.watchResources([ResourceWatcher.TYPES.NETWORK_EVENT], {
    onAvailable,
    onUpdated,
    ignoreExistingResources,
  });

  info(
    `Trigger the rest of the requests *after* calling ResourceWatcher.watchResources
     in order to assert the behavior of live network events.`
  );
  await triggerNetworkRequests(tab.linkedBrowser, LIVE_REQUESTS_COMMANDS);

  await Promise.all([waitForAllOnAvailableEvents, waitForAllOnUpdatedEvents]);

  info("Check the resources on available");
  is(
    Object.keys(actualResourcesOnAvailable).length,
    Object.keys(options.expectedResourcesOnAvailable).length,
    "Got the expected number of network events fired onAvailable"
  );

  // assert that the resourceId for the the available and updated events match
  is(
    actualResourcesOnAvailable[`${EXAMPLE_DOMAIN}live_get.html`].resourceId,
    actualResourcesOnUpdated[`${EXAMPLE_DOMAIN}live_get.html`].resourceId,
    "The resource id's are the same"
  );

  // assert the resources emitted when the network event is created
  for (const key in options.expectedResourcesOnAvailable) {
    const expected = options.expectedResourcesOnAvailable[key];
    const actual = actualResourcesOnAvailable[key];
    assertResources(actual, expected);
  }

  info("Check the resources on updated");

  is(
    Object.keys(actualResourcesOnUpdated).length,
    Object.keys(options.expectedResourcesOnUpdated).length,
    "Got the expected number of network events fired onUpdated"
  );

  // assert the resources emitted when the network event is updated
  for (const key in options.expectedResourcesOnUpdated) {
    const expected = options.expectedResourcesOnUpdated[key];
    const actual = actualResourcesOnUpdated[key];
    assertResources(actual, expected);
  }

  await resourceWatcher.unwatchResources(
    [ResourceWatcher.TYPES.NETWORK_EVENT],
    {
      onAvailable,
      onUpdated,
      ignoreExistingResources,
    }
  );

  await resourceWatcher.unwatchResources(
    [ResourceWatcher.TYPES.NETWORK_EVENT],
    {
      onAvailable: onResourceAvailable,
      onUpdated: onResourceUpdated,
    }
  );
  await targetList.destroy();
  await client.close();
  BrowserTestUtils.removeTab(tab);
}

function assertResources(actual, expected) {
  is(
    actual.resourceType,
    expected.resourceType,
    "The resource type is correct"
  );
  is(actual.request.url, expected.request.url, "The url is correct");
  is(actual.request.method, expected.request.method, "The method is correct");
  is(
    actual.updates.length,
    expected.updates.length,
    "The number of updates is correct"
  );
}

const EXISTING_REQUESTS_COMMANDS = [
  `await fetch("/existing_post.html", { method: "POST" });`,
];

const LIVE_REQUESTS_COMMANDS = [
  `await fetch("/live_get.html", { method: "GET" });`,
];
