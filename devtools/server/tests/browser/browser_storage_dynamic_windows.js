/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from storage-helpers.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/server/tests/browser/storage-helpers.js",
  this
);

// beforeReload references an object representing the initialized state of the
// storage actor.
const beforeReload = {
  cookies: {
    "http://test1.example.org": ["c1", "cs2", "c3", "uc1"],
    "http://sectest1.example.org": ["uc1", "cs2"],
  },
  "indexed-db": {
    "http://test1.example.org": [
      JSON.stringify(["idb1", "obj1"]),
      JSON.stringify(["idb1", "obj2"]),
      JSON.stringify(["idb2", "obj3"]),
    ],
    "http://sectest1.example.org": [],
  },
  "local-storage": {
    "http://test1.example.org": ["ls1", "ls2"],
    "http://sectest1.example.org": ["iframe-u-ls1"],
  },
  "session-storage": {
    "http://test1.example.org": ["ss1"],
    "http://sectest1.example.org": ["iframe-u-ss1", "iframe-u-ss2"],
  },
};

// afterIframeAdded references the items added when an iframe containing storage
// items is added to the page.
const afterIframeAdded = {
  cookies: {
    "https://sectest1.example.org": [
      getCookieId("cs2", ".example.org", "/"),
      getCookieId(
        "sc1",
        "sectest1.example.org",
        "/browser/devtools/server/tests/browser"
      ),
    ],
    "http://sectest1.example.org": [
      getCookieId(
        "sc1",
        "sectest1.example.org",
        "/browser/devtools/server/tests/browser"
      ),
    ],
  },
  "indexed-db": {
    // empty because indexed db creation happens after the page load, so at
    // the time of window-ready, there was no indexed db present.
    "https://sectest1.example.org": [],
  },
  "local-storage": {
    "https://sectest1.example.org": ["iframe-s-ls1"],
  },
  "session-storage": {
    "https://sectest1.example.org": ["iframe-s-ss1"],
  },
};

// afterIframeRemoved references the items deleted when an iframe containing
// storage items is removed from the page.
const afterIframeRemoved = {
  cookies: {
    "http://sectest1.example.org": [],
  },
  "indexed-db": {
    "http://sectest1.example.org": [],
  },
  "local-storage": {
    "http://sectest1.example.org": [],
  },
  "session-storage": {
    "http://sectest1.example.org": [],
  },
};

add_task(async function() {
  const { commands } = await openTabAndSetupStorage(
    MAIN_DOMAIN + "storage-dynamic-windows.html"
  );

  const { resourceCommand } = commands;
  const { TYPES } = resourceCommand;
  const allResources = {};
  const onAvailable = resources => {
    for (const resource of resources) {
      allResources[resource.resourceType] = resource;
    }
  };
  const parentProcessStorages = [TYPES.COOKIE, TYPES.INDEXED_DB];
  const contentProcessStorages = [TYPES.LOCAL_STORAGE, TYPES.SESSION_STORAGE];
  const allStorages = [...parentProcessStorages, ...contentProcessStorages];
  await resourceCommand.watchResources(allStorages, { onAvailable });
  is(
    Object.keys(allStorages).length,
    allStorages.length,
    "Got all the storage resources"
  );

  // Do a copy of all the initial storages as test function may spawn new resources for the same
  // type and override the initial ones.
  // We do not call unwatchResources as it would clear its cache and next call
  // to watchResources with ignoreExistingResources would break and reprocess all resources again.
  const initialResources = Object.assign({}, allResources);

  testWindowsBeforeReload(initialResources);

  await testAddIframe(commands, initialResources, {
    contentProcessStorages,
    parentProcessStorages,
    allStorages,
  });
  await testRemoveIframe(commands, initialResources, { allStorages });

  await clearStorage();

  // Forcing GC/CC to get rid of docshells and windows created by this test.
  forceCollections();
  await commands.destroy();
  forceCollections();
});

function testWindowsBeforeReload(resources) {
  for (const storageType in beforeReload) {
    ok(resources[storageType], `${storageType} storage actor is present`);

    // If this test is run with chrome debugging enabled we get an extra
    // key for "chrome". We don't want the test to fail in this case, so
    // ignore it.
    if (storageType == "indexedDB") {
      delete resources[storageType].hosts.chrome;
    }

    is(
      Object.keys(resources[storageType].hosts).length,
      Object.keys(beforeReload[storageType]).length,
      `Number of hosts for ${storageType} match`
    );
    for (const host in beforeReload[storageType]) {
      ok(resources[storageType].hosts[host], `Host ${host} is present`);
    }
  }
}

/**
 * Wait for new storage resources to be created of the given types.
 */
async function waitForNewResourcesAndUpdates(commands, resourceTypes) {
  // When fission is off, we don't expect any new resource
  if (resourceTypes.length === 0) {
    return { newResources: [], updates: [] };
  }
  const { resourceCommand } = commands;
  let resolve;
  const promise = new Promise(r => (resolve = r));
  const allResources = {};
  const allUpdates = {};
  const onAvailable = resources => {
    for (const resource of resources) {
      if (resource.resourceType in allResources) {
        ok(false, `Got multiple ${resource.resourceTypes} resources`);
      }
      allResources[resource.resourceType] = resource;
      ok(true, `Got resource for ${resource.resourceType}`);

      // Stop watching for resources when we got them all
      if (Object.keys(allResources).length == resourceTypes.length) {
        resourceCommand.unwatchResources(resourceTypes, {
          onAvailable,
        });
      }

      // But also listen for updates on each new resource
      resource.once("single-store-update").then(update => {
        ok(true, `Got updates for ${resource.resourceType}`);
        allUpdates[resource.resourceType] = update;

        // Resolve only once we got all the updates, for all the resources
        if (Object.keys(allUpdates).length == resourceTypes.length) {
          resolve({ newResources: allResources, updates: allUpdates });
        }
      });
    }
  };
  await resourceCommand.watchResources(resourceTypes, {
    onAvailable,
    ignoreExistingResources: true,
  });
  return promise;
}

/**
 * Wait for single-store-update events on all the given storage resources.
 */
function waitForResourceUpdates(resources, resourceTypes) {
  const allUpdates = {};
  const promises = [];
  for (const type of resourceTypes) {
    const promise = resources[type].once("single-store-update");
    promise.then(update => {
      ok(true, `Got updates for ${type}`);
      allUpdates[type] = update;
    });
    promises.push(promise);
  }
  return Promise.all(promises).then(() => allUpdates);
}

async function testAddIframe(
  commands,
  resources,
  { contentProcessStorages, parentProcessStorages, allStorages }
) {
  info("Testing if new iframe addition works properly");

  // If Fission is enabled:
  // * we get new resources alongside single-store-update events for content process storages
  // * only single-store-update events for previous resources for parent process storages
  // Otherwise if fission is disables:
  // * we get single-store-update events for all previous resources
  const onResources = waitForNewResourcesAndUpdates(
    commands,
    isFissionEnabled() ? contentProcessStorages : []
  );
  // If fission is enabled, we only get update
  const onUpdates = waitForResourceUpdates(
    resources,
    isFissionEnabled() ? parentProcessStorages : allStorages
  );

  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [ALT_DOMAIN_SECURED],
    secured => {
      const doc = content.document;

      const iframe = doc.createElement("iframe");
      iframe.src = secured + "storage-secured-iframe.html";

      doc.querySelector("body").appendChild(iframe);
    }
  );

  info("Wait for all resources");
  const { newResources, updates } = await onResources;
  info("Wait for all updates");
  const previousResourceUpdates = await onUpdates;

  if (isFissionEnabled()) {
    for (const resourceType of contentProcessStorages) {
      const resource = newResources[resourceType];
      const expected = afterIframeAdded[resourceType];
      // The resource only comes with hosts, without any values.
      // Each host will be an empty array.
      Assert.deepEqual(
        Object.keys(resource.hosts),
        Object.keys(expected),
        `List of hosts for resource ${resourceType} is correct`
      );
      for (const host in resource.hosts) {
        is(
          resource.hosts[host].length,
          0,
          "For new resources, each host has no value and is an empty array"
        );
      }
      const update = updates[resourceType];
      const storageKey = resourceTypeToStorageKey(resourceType);
      Assert.deepEqual(
        update.added[storageKey],
        expected,
        "We get an update after the resource, with the host values"
      );
    }
  }

  const storagesWithUpdates = isFissionEnabled()
    ? parentProcessStorages
    : allStorages;
  for (const resourceType of storagesWithUpdates) {
    const expected = afterIframeAdded[resourceType];
    const update = previousResourceUpdates[resourceType];
    const storageKey = resourceTypeToStorageKey(resourceType);
    Assert.deepEqual(
      update.added[storageKey],
      expected,
      `We get an update after the resource ${resourceType}, with the host values`
    );
  }

  return newResources;
}

async function testRemoveIframe(commands, resources, { allStorages }) {
  info("Testing if iframe removal works properly");

  const onUpdates = waitForResourceUpdates(resources, allStorages);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    for (const iframe of content.document.querySelectorAll("iframe")) {
      if (iframe.src.startsWith("http:")) {
        iframe.remove();
        break;
      }
    }
  });

  info("Wait for all updates");
  const previousResourceUpdates = await onUpdates;

  for (const resourceType of allStorages) {
    const expected = afterIframeRemoved[resourceType];
    const update = previousResourceUpdates[resourceType];
    const storageKey = resourceTypeToStorageKey(resourceType);
    Assert.deepEqual(
      update.deleted[storageKey],
      expected,
      `We get an update after the resource ${resourceType}, with the host values`
    );
  }
}

/**
 * single-store-update emits objects using attributes with old "storage key" namings,
 * which is different from resource type namings.
 */
function resourceTypeToStorageKey(resourceType) {
  if (resourceType == "local-storage") {
    return "localStorage";
  }
  if (resourceType == "session-storage") {
    return "sessionStorage";
  }
  if (resourceType == "indexed-db") {
    return "indexedDB";
  }
  return resourceType;
}
