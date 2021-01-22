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
  info("Test network events");
  await testNetworkEventResourcesWithExistingResources();
  await testNetworkEventResourcesWithoutExistingResources();
});

async function testNetworkEventResourcesWithExistingResources() {
  info(`Tests for network event resources with the existing resources`);
  await testNetworkEventResources({
    ignoreExistingResources: false,
    // 1 available event fired, for the existing resource in the cache.
    // 1 available event fired, when live request is created.
    totalExpectedOnAvailableCounts: 2,
    // 1 update events fired, when live request is updated.
    totalExpectedOnUpdatedCounts: 1,
    expectedResourcesOnAvailable: {
      [`${EXAMPLE_DOMAIN}cached_post.html`]: {
        resourceType: ResourceWatcher.TYPES.NETWORK_EVENT,
        method: "POST",
      },
      [`${EXAMPLE_DOMAIN}live_get.html`]: {
        resourceType: ResourceWatcher.TYPES.NETWORK_EVENT,
        method: "GET",
      },
    },
    expectedResourcesOnUpdated: {
      [`${EXAMPLE_DOMAIN}live_get.html`]: {
        resourceType: ResourceWatcher.TYPES.NETWORK_EVENT,
        method: "GET",
      },
    },
  });
}

async function testNetworkEventResourcesWithoutExistingResources() {
  info(`Tests for network event resources without the existing resources`);
  await testNetworkEventResources({
    ignoreExistingResources: true,
    // 1 available event fired, when live request is created.
    totalExpectedOnAvailableCounts: 1,
    // 1 update events fired, when live request is updated.
    totalExpectedOnUpdatedCounts: 1,
    expectedResourcesOnAvailable: {
      [`${EXAMPLE_DOMAIN}live_get.html`]: {
        resourceType: ResourceWatcher.TYPES.NETWORK_EVENT,
        method: "GET",
      },
    },
    expectedResourcesOnUpdated: {
      [`${EXAMPLE_DOMAIN}live_get.html`]: {
        resourceType: ResourceWatcher.TYPES.NETWORK_EVENT,
        method: "GET",
      },
    },
  });
}

async function testNetworkEventResources(options) {
  const tab = await addTab(TEST_URI);
  const { client, resourceWatcher, targetList } = await initResourceWatcher(
    tab
  );

  info(
    `Trigger some network requests *before* calling ResourceWatcher.watchResources
     in order to assert the behavior of already existing network events.`
  );

  let onResourceAvailable = () => {};
  let onResourceUpdated = () => {};

  // Lets make sure there is already a network event resource in the cache.
  const waitOnRequestForResourceWatcherCache = new Promise(resolve => {
    onResourceAvailable = resources => {
      for (const resource of resources) {
        is(
          resource.resourceType,
          ResourceWatcher.TYPES.NETWORK_EVENT,
          "Received a network event resource"
        );
      }
    };

    onResourceUpdated = updates => {
      for (const { resource } of updates) {
        is(
          resource.resourceType,
          ResourceWatcher.TYPES.NETWORK_EVENT,
          "Received a network update event resource"
        );
        resolve();
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
        triggerNetworkRequests(tab.linkedBrowser, [cachedRequest]);
      });
  });

  await waitOnRequestForResourceWatcherCache;

  const actualResourcesOnAvailable = {};
  const actualResourcesOnUpdated = {};

  let {
    totalExpectedOnAvailableCounts,
    totalExpectedOnUpdatedCounts,
    expectedResourcesOnAvailable,
    expectedResourcesOnUpdated,

    ignoreExistingResources,
  } = options;

  const waitForAllExpectedOnAvailableEvents = waitUntil(
    () => totalExpectedOnAvailableCounts == 0
  );
  const waitForAllExpectedOnUpdatedEvents = waitUntil(
    () => totalExpectedOnUpdatedCounts == 0
  );

  const onAvailable = resources => {
    for (const resource of resources) {
      is(
        resource.resourceType,
        ResourceWatcher.TYPES.NETWORK_EVENT,
        "Received a network event resource"
      );
      actualResourcesOnAvailable[resource.url] = {
        resourceId: resource.resourceId,
        resourceType: resource.resourceType,
        method: resource.method,
      };
      totalExpectedOnAvailableCounts--;
    }
  };

  const onUpdated = updates => {
    for (const { resource } of updates) {
      is(
        resource.resourceType,
        ResourceWatcher.TYPES.NETWORK_EVENT,
        "Received a network update event resource"
      );
      actualResourcesOnUpdated[resource.url] = {
        resourceId: resource.resourceId,
        resourceType: resource.resourceType,
        method: resource.method,
      };
      totalExpectedOnUpdatedCounts--;
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
  await triggerNetworkRequests(tab.linkedBrowser, [liveRequest]);

  await Promise.all([
    waitForAllExpectedOnAvailableEvents,
    waitForAllExpectedOnUpdatedEvents,
  ]);

  info("Check the resources on available");
  is(
    Object.keys(actualResourcesOnAvailable).length,
    Object.keys(expectedResourcesOnAvailable).length,
    "Got the expected number of network events fired onAvailable"
  );

  // assert that the resourceId for the the available and updated events match
  is(
    actualResourcesOnAvailable[`${EXAMPLE_DOMAIN}live_get.html`].resourceId,
    actualResourcesOnUpdated[`${EXAMPLE_DOMAIN}live_get.html`].resourceId,
    "The resource id's are the same"
  );

  // assert the resources emitted when the network event is created
  for (const key in expectedResourcesOnAvailable) {
    const expected = expectedResourcesOnAvailable[key];
    const actual = actualResourcesOnAvailable[key];
    assertResources(actual, expected);
  }

  info("Check the resources on updated");

  is(
    Object.keys(actualResourcesOnUpdated).length,
    Object.keys(expectedResourcesOnUpdated).length,
    "Got the expected number of network events fired onUpdated"
  );

  // assert the resources emitted when the network event is updated
  for (const key in expectedResourcesOnUpdated) {
    const expected = expectedResourcesOnUpdated[key];
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
  is(actual.method, expected.method, "The method is correct");
}

const cachedRequest = `await fetch("/cached_post.html", { method: "POST" });`;
const liveRequest = `await fetch("/live_get.html", { method: "GET" });`;
