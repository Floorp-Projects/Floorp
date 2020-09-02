/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the cache mechanism of the ResourceWatcher.

const {
  ResourceWatcher,
} = require("devtools/shared/resources/resource-watcher");

const TEST_URI = "data:text/html;charset=utf-8,Cache Test";

add_task(async function() {
  info("Test whether multiple listener can get same cached resources");

  const tab = await addTab(TEST_URI);

  const {
    client,
    resourceWatcher,
    targetList,
  } = await initResourceWatcherAndTarget(tab);

  info("Add messages as existing resources");
  const messages = ["a", "b", "c"];
  await logMessages(tab.linkedBrowser, messages);

  info("Register first listener");
  const cachedResources1 = [];
  await resourceWatcher.watchResources(
    [ResourceWatcher.TYPES.CONSOLE_MESSAGE],
    {
      onAvailable: ({ resource }) => cachedResources1.push(resource),
    }
  );

  info("Register second listener");
  const cachedResources2 = [];
  await resourceWatcher.watchResources(
    [ResourceWatcher.TYPES.CONSOLE_MESSAGE],
    {
      onAvailable: ({ resource }) => cachedResources2.push(resource),
    }
  );

  assertContents(cachedResources1, messages);
  assertResources(cachedResources2, cachedResources1);

  await targetList.destroy();
  await client.close();
});

add_task(async function() {
  info(
    "Test whether the cache is reflecting existing resources and additional resources"
  );

  const tab = await addTab(TEST_URI);

  const {
    client,
    resourceWatcher,
    targetList,
  } = await initResourceWatcherAndTarget(tab);

  info("Add messages as existing resources");
  const existingMessages = ["a", "b", "c"];
  await logMessages(tab.linkedBrowser, existingMessages);

  info("Register first listener to get all available resources");
  const availableResources = [];
  await resourceWatcher.watchResources(
    [ResourceWatcher.TYPES.CONSOLE_MESSAGE],
    {
      onAvailable: ({ resource }) => availableResources.push(resource),
    }
  );

  info("Add messages as additional resources");
  const additionalMessages = ["d", "e"];
  await logMessages(tab.linkedBrowser, additionalMessages);

  info("Wait until onAvailable is called expected times");
  const allMessages = [...existingMessages, ...additionalMessages];
  await waitUntil(() => availableResources.length === allMessages.length);

  info("Register second listener to get the cached resources");
  const cachedResources = [];
  await resourceWatcher.watchResources(
    [ResourceWatcher.TYPES.CONSOLE_MESSAGE],
    {
      onAvailable: ({ resource }) => cachedResources.push(resource),
    }
  );

  assertContents(availableResources, allMessages);
  assertResources(cachedResources, availableResources);

  await targetList.destroy();
  await client.close();
});

add_task(async function() {
  info("Test whether the cache is cleared when navigation");

  const tab = await addTab(TEST_URI);

  const {
    client,
    resourceWatcher,
    targetList,
  } = await initResourceWatcherAndTarget(tab);

  info("Add messages as existing resources");
  const existingMessages = ["a", "b", "c"];
  await logMessages(tab.linkedBrowser, existingMessages);

  info("Register first listener");
  await resourceWatcher.watchResources(
    [ResourceWatcher.TYPES.CONSOLE_MESSAGE],
    {
      onAvailable: () => {},
    }
  );

  info("Reload the page");
  const onReloaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  gBrowser.reloadTab(tab);
  await onReloaded;

  info("Register second listener");
  const cachedResources = [];
  await resourceWatcher.watchResources(
    [ResourceWatcher.TYPES.CONSOLE_MESSAGE],
    {
      onAvailable: ({ resource }) => cachedResources.push(resource),
    }
  );

  is(cachedResources.length, 0, "The cache in ResourceWatcher is cleared");

  await targetList.destroy();
  await client.close();
});

add_task(async function() {
  info("Test with multiple resource types");

  const tab = await addTab(TEST_URI);

  const {
    client,
    resourceWatcher,
    targetList,
  } = await initResourceWatcherAndTarget(tab);

  info("Register first listener to get all available resources");
  const availableResources = [];
  await resourceWatcher.watchResources(
    [
      ResourceWatcher.TYPES.CONSOLE_MESSAGE,
      ResourceWatcher.TYPES.ERROR_MESSAGE,
    ],
    {
      onAvailable: ({ resource }) => availableResources.push(resource),
    }
  );

  info("Add messages as console message");
  const consoleMessages1 = ["a", "b", "c"];
  await logMessages(tab.linkedBrowser, consoleMessages1);

  info("Add message as error message");
  const errorMessages = ["document.doTheImpossible();"];
  await triggerErrors(tab.linkedBrowser, errorMessages);

  info("Add messages as console message again");
  const consoleMessages2 = ["d", "e"];
  await logMessages(tab.linkedBrowser, consoleMessages2);

  info("Wait until the getting all available resources");
  const totalResourceCount =
    consoleMessages1.length + errorMessages.length + consoleMessages2.length;
  await waitUntil(() => {
    return availableResources.length === totalResourceCount;
  });

  info("Register listener to get the cached resources");
  const cachedResources = [];
  await resourceWatcher.watchResources(
    [
      ResourceWatcher.TYPES.CONSOLE_MESSAGE,
      ResourceWatcher.TYPES.ERROR_MESSAGE,
    ],
    {
      onAvailable: ({ resource }) => cachedResources.push(resource),
    }
  );

  assertResources(cachedResources, availableResources);

  await targetList.destroy();
  await client.close();
});

add_task(async function() {
  info("Test multiple listeners with/without ignoreExistingResources");
  await testIgnoreExistingResources(true);
  await testIgnoreExistingResources(false);
});

async function testIgnoreExistingResources(isFirstListenerIgnoreExisting) {
  const tab = await addTab(TEST_URI);

  const {
    client,
    resourceWatcher,
    targetList,
  } = await initResourceWatcherAndTarget(tab);

  info("Add messages as existing resources");
  const existingMessages = ["a", "b", "c"];
  await logMessages(tab.linkedBrowser, existingMessages);

  info("Register first listener");
  const cachedResources1 = [];
  await resourceWatcher.watchResources(
    [ResourceWatcher.TYPES.CONSOLE_MESSAGE],
    {
      onAvailable: ({ resource }) => cachedResources1.push(resource),
      ignoreExistingResources: isFirstListenerIgnoreExisting,
    }
  );

  info("Register second listener");
  const cachedResources2 = [];
  await resourceWatcher.watchResources(
    [ResourceWatcher.TYPES.CONSOLE_MESSAGE],
    {
      onAvailable: ({ resource }) => cachedResources2.push(resource),
      ignoreExistingResources: !isFirstListenerIgnoreExisting,
    }
  );

  const cachedResourcesWithFlag = isFirstListenerIgnoreExisting
    ? cachedResources1
    : cachedResources2;
  const cachedResourcesWithoutFlag = isFirstListenerIgnoreExisting
    ? cachedResources2
    : cachedResources1;

  info("Check the existing resources both listeners got");
  assertContents(cachedResourcesWithFlag, []);
  assertContents(cachedResourcesWithoutFlag, existingMessages);

  info("Add messages as additional resources");
  const additionalMessages = ["d", "e"];
  await logMessages(tab.linkedBrowser, additionalMessages);

  info("Wait until onAvailable is called expected times");
  await waitUntil(
    () => cachedResourcesWithFlag.length === additionalMessages.length
  );
  const allMessages = [...existingMessages, ...additionalMessages];
  await waitUntil(
    () => cachedResourcesWithoutFlag.length === allMessages.length
  );

  info("Check the resources after adding messages");
  assertContents(cachedResourcesWithFlag, additionalMessages);
  assertContents(cachedResourcesWithoutFlag, allMessages);

  await targetList.destroy();
  await client.close();
}

function assertContents(resources, expectedMessages) {
  is(
    resources.length,
    expectedMessages.length,
    "Number of the resources is correct"
  );

  for (let i = 0; i < expectedMessages.length; i++) {
    const resource = resources[i];
    const message = resource.message.arguments[0];
    const expectedMessage = expectedMessages[i];
    is(message, expectedMessage, `The ${i}th content is correct`);
  }
}

function assertResources(resources, expectedResources) {
  is(
    resources.length,
    expectedResources.length,
    "Number of the resources is correct"
  );

  for (let i = 0; i < resources.length; i++) {
    const resource = resources[i];
    const expectedResource = expectedResources[i];
    ok(resource === expectedResource, `The ${i}th resource is correct`);
  }
}

function logMessages(browser, messages) {
  return ContentTask.spawn(browser, { messages }, args => {
    for (const message of args.messages) {
      content.console.log(message);
    }
  });
}

async function triggerErrors(browser, errorScripts) {
  for (const errorScript of errorScripts) {
    await ContentTask.spawn(browser, errorScript, expr => {
      const document = content.document;
      const container = document.createElement("script");
      document.body.appendChild(container);
      container.textContent = expr;
      container.remove();
    });
  }
}
