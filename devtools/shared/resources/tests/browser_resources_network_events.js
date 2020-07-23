/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the ResourceWatcher API around NETWORK_EVENT

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

const EXAMPLE_DOMAIN = "https://example.com/";
const TEST_URI = `${URL_ROOT_SSL}/network_document.html`;

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
      [`${EXAMPLE_DOMAIN}existing_xhr_post.html`]: {
        resourceType: ResourceWatcher.TYPES.NETWORK_EVENT,
        request: {
          url: `${EXAMPLE_DOMAIN}existing_xhr_post.html`,
          method: "POST",
        },
        updates: UPDATES,
      },
      [`${EXAMPLE_DOMAIN}live_xhr_get.html`]: {
        resourceType: ResourceWatcher.TYPES.NETWORK_EVENT,
        request: {
          url: `${EXAMPLE_DOMAIN}live_xhr_get.html`,
          method: "GET",
        },
        updates: [],
      },
    },
    expectedResourcesOnUpdated: {
      [`${EXAMPLE_DOMAIN}live_xhr_get.html`]: {
        resourceType: ResourceWatcher.TYPES.NETWORK_EVENT,
        request: {
          url: `${EXAMPLE_DOMAIN}live_xhr_get.html`,
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
      [`${EXAMPLE_DOMAIN}live_xhr_get.html`]: {
        resourceType: ResourceWatcher.TYPES.NETWORK_EVENT,
        request: {
          url: `${EXAMPLE_DOMAIN}live_xhr_get.html`,
          method: "GET",
        },
        updates: [],
      },
    },
    expectedResourcesOnUpdated: {
      [`${EXAMPLE_DOMAIN}live_xhr_get.html`]: {
        resourceType: ResourceWatcher.TYPES.NETWORK_EVENT,
        request: {
          url: `${EXAMPLE_DOMAIN}live_xhr_get.html`,
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

  const waitForAllExpectedUpdates = new Promise(resolve => {
    resourceWatcher.watchResources([ResourceWatcher.TYPES.NETWORK_EVENT], {
      onAvailable: () => {},
      onUpdated: ({ resource }) => {
        // Wait until all the update events have fired
        // for the existing request.
        if (resource.updates.length == 8) {
          resolve();
        }
      },
    });
  });
  await triggerNetworkRequests(tab.linkedBrowser, EXISTING_REQUESTS_COMMANDS);
  await waitForAllExpectedUpdates;

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

  const onAvailable = ({ resourceType, targetFront, resource }) => {
    is(
      resourceType,
      ResourceWatcher.TYPES.NETWORK_EVENT,
      "Received a network event resource"
    );
    actualResourcesOnAvailable[resource.request.url] = {
      resourceType: resource.resourceType,
      request: resource.request,
      updates: [...resource.updates],
    };
    expectedOnAvailableCounts--;
  };

  const onUpdated = ({ resourceType, targetFront, resource }) => {
    is(
      resourceType,
      ResourceWatcher.TYPES.NETWORK_EVENT,
      "Received a network update event resource"
    );
    actualResourcesOnUpdated[resource.request.url] = {
      resourceType: resource.resourceType,
      request: resource.request,
      updates: [...resource.updates],
    };
    expectedOnUpdatedCounts--;
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
  await targetList.stopListening();
  await client.close();
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
  `await fetch("/existing_xhr_post.html", { method: "POST" });`,
];

const LIVE_REQUESTS_COMMANDS = [
  `await fetch("/live_xhr_get.html", { method: "GET" });`,
];

async function triggerNetworkRequests(browser, commands) {
  for (let i = 0; i < commands.length; i++) {
    await SpecialPowers.spawn(browser, [commands[i]], async function(code) {
      const script = content.document.createElement("script");
      script.append(
        content.document.createTextNode(
          `async function triggerRequest() {${code}}`
        )
      );
      content.document.body.append(script);
      await content.wrappedJSObject.triggerRequest();
      script.remove();
    });
  }
}
