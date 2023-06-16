/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceCommand API internal cache / ignoreExistingResources around NETWORK_EVENT

const ResourceCommand = require("resource://devtools/shared/commands/resource/resource-command.js");

const EXAMPLE_DOMAIN = "https://example.com/";
const TEST_URI = `${URL_ROOT_SSL}network_document.html`;

add_task(async function () {
  info("Test basic NETWORK_EVENT resources against ResourceCommand cache");
  await testNetworkEventResourcesWithExistingResources();
  await testNetworkEventResourcesWithoutExistingResources();
});

async function testNetworkEventResourcesWithExistingResources() {
  info(`Tests for network event resources with the existing resources`);
  await testNetworkEventResourcesWithCachedRequest({
    ignoreExistingResources: false,
    // 1 available event fired, for the existing resource in the cache.
    // 1 available event fired, when live request is created.
    expectedResourcesOnAvailable: {
      [`${EXAMPLE_DOMAIN}cached_post.html`]: {
        resourceType: ResourceCommand.TYPES.NETWORK_EVENT,
        method: "POST",
        isNavigationRequest: false,
      },
      [`${EXAMPLE_DOMAIN}live_get.html`]: {
        resourceType: ResourceCommand.TYPES.NETWORK_EVENT,
        method: "GET",
        isNavigationRequest: false,
      },
    },
    // 1 update events fired, when live request is updated.
    expectedResourcesOnUpdated: {
      [`${EXAMPLE_DOMAIN}live_get.html`]: {
        resourceType: ResourceCommand.TYPES.NETWORK_EVENT,
        method: "GET",
      },
    },
  });
}

async function testNetworkEventResourcesWithoutExistingResources() {
  info(`Tests for network event resources without the existing resources`);
  await testNetworkEventResourcesWithCachedRequest({
    ignoreExistingResources: true,
    // 1 available event fired, when live request is created.
    expectedResourcesOnAvailable: {
      [`${EXAMPLE_DOMAIN}live_get.html`]: {
        resourceType: ResourceCommand.TYPES.NETWORK_EVENT,
        method: "GET",
        isNavigationRequest: false,
      },
    },
    // 1 update events fired, when live request is updated.
    expectedResourcesOnUpdated: {
      [`${EXAMPLE_DOMAIN}live_get.html`]: {
        resourceType: ResourceCommand.TYPES.NETWORK_EVENT,
        method: "GET",
      },
    },
  });
}

/**
 * This test helper is slightly complex as we workaround the fact
 * that the server is not able to record network request done in the past.
 * Because of that we have to start observer requests via ResourceCommand.watchResources
 * before doing a request, and, before doing the actual call to watchResources
 * we want to assert the behavior of.
 */
async function testNetworkEventResourcesWithCachedRequest(options) {
  const tab = await addTab(TEST_URI);
  const commands = await CommandsFactory.forTab(tab);
  await commands.targetCommand.startListening();

  const { resourceCommand } = commands;

  info(
    `Trigger some network requests *before* calling ResourceCommand.watchResources
     in order to assert the behavior of already existing network events.`
  );

  // Register a first empty listener in order to ensure populating ResourceCommand
  // internal cache of NETWORK_EVENT's. We can't retrieved past network requests
  // when calling server's `watchResources`.
  let resolveCachedRequestAvailable;
  const onCachedRequestAvailable = new Promise(
    r => (resolveCachedRequestAvailable = r)
  );
  const onAvailableToPopulateInternalCache = () => {};
  const onUpdatedToPopulateInternalCache = resolveCachedRequestAvailable;
  await resourceCommand.watchResources([resourceCommand.TYPES.NETWORK_EVENT], {
    ignoreExistingResources: true,
    onAvailable: onAvailableToPopulateInternalCache,
    onUpdated: onUpdatedToPopulateInternalCache,
  });

  // We can only trigger the requests once `watchResources` settles,
  // otherwise we might miss some events and they won't be present in the cache
  const cachedRequest = `await fetch("/cached_post.html", { method: "POST" });`;
  await triggerNetworkRequests(tab.linkedBrowser, [cachedRequest]);

  // We have to ensure that ResourceCommand processed the Resource for this first
  // cached request before calling watchResource a second time and report it.
  // Wait for the updated notification to avoid receiving it during the next call
  // to watchResources.
  await onCachedRequestAvailable;

  const actualResourcesOnAvailable = {};
  const actualResourcesOnUpdated = {};

  const {
    expectedResourcesOnAvailable,
    expectedResourcesOnUpdated,

    ignoreExistingResources,
  } = options;

  const onAvailable = resources => {
    for (const resource of resources) {
      is(
        resource.resourceType,
        resourceCommand.TYPES.NETWORK_EVENT,
        "Received a network event resource"
      );
      actualResourcesOnAvailable[resource.url] = resource;
    }
  };

  const onUpdated = updates => {
    for (const { resource } of updates) {
      is(
        resource.resourceType,
        resourceCommand.TYPES.NETWORK_EVENT,
        "Received a network update event resource"
      );
      actualResourcesOnUpdated[resource.url] = resource;
    }
  };

  await resourceCommand.watchResources([resourceCommand.TYPES.NETWORK_EVENT], {
    onAvailable,
    onUpdated,
    ignoreExistingResources,
  });

  info(
    `Trigger the rest of the requests *after* calling ResourceCommand.watchResources
     in order to assert the behavior of live network events.`
  );
  const liveRequest = `await fetch("/live_get.html", { method: "GET" });`;
  await triggerNetworkRequests(tab.linkedBrowser, [liveRequest]);

  info("Check the resources on available");

  await waitUntil(
    () =>
      Object.keys(actualResourcesOnAvailable).length ==
      Object.keys(expectedResourcesOnAvailable).length
  );

  is(
    Object.keys(actualResourcesOnAvailable).length,
    Object.keys(expectedResourcesOnAvailable).length,
    "Got the expected number of network events fired onAvailable"
  );

  // assert the resources emitted when the network event is created
  for (const key in expectedResourcesOnAvailable) {
    const expected = expectedResourcesOnAvailable[key];
    const actual = actualResourcesOnAvailable[key];
    assertResources(actual, expected);
  }

  info("Check the resources on updated");

  await waitUntil(
    () =>
      Object.keys(actualResourcesOnUpdated).length ==
      Object.keys(expectedResourcesOnUpdated).length
  );

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
    // assert that the resourceId for the the available and updated events match
    is(
      actual.resourceId,
      actualResourcesOnAvailable[key].resourceId,
      `Available and update resource ids for ${key} are the same`
    );
  }

  resourceCommand.unwatchResources([resourceCommand.TYPES.NETWORK_EVENT], {
    onAvailable,
    onUpdated,
    ignoreExistingResources,
  });

  resourceCommand.unwatchResources([resourceCommand.TYPES.NETWORK_EVENT], {
    onAvailable: onAvailableToPopulateInternalCache,
  });

  await commands.destroy();

  BrowserTestUtils.removeTab(tab);
}

function assertResources(actual, expected) {
  is(
    actual.resourceType,
    expected.resourceType,
    "The resource type is correct"
  );
  is(actual.method, expected.method, "The method is correct");
  if ("isNavigationRequest" in expected) {
    is(
      actual.isNavigationRequest,
      expected.isNavigationRequest,
      "The isNavigationRequest attribute is correct"
    );
  }
}
