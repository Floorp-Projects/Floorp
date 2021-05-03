/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test getAllResources function of the ResourceWatcher.

const TEST_URI = "data:text/html;charset=utf-8,getAllResources test";

add_task(async function() {
  const tab = await addTab(TEST_URI);

  const { client, resourceWatcher, targetCommand } = await initResourceWatcher(
    tab
  );

  info("Check the resources gotten from getAllResources at initial");
  is(
    resourceWatcher.getAllResources(resourceWatcher.TYPES.CONSOLE_MESSAGE)
      .length,
    0,
    "There is no resources at initial"
  );

  info(
    "Start to watch the available resources in order to compare with resources gotten from getAllResources"
  );
  const availableResources = [];
  const onAvailable = resources => availableResources.push(...resources);
  await resourceWatcher.watchResources(
    [resourceWatcher.TYPES.CONSOLE_MESSAGE],
    { onAvailable }
  );

  info("Check the resources after some resources are available");
  const messages = ["a", "b", "c"];
  await logMessages(tab.linkedBrowser, messages);
  await waitUntil(() => availableResources.length >= messages.length);
  assertResources(
    resourceWatcher.getAllResources(resourceWatcher.TYPES.CONSOLE_MESSAGE),
    availableResources
  );
  assertResources(
    resourceWatcher.getAllResources(resourceWatcher.TYPES.STYLESHEET),
    []
  );

  info("Check the resources after reloading");
  const onReloaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  gBrowser.reloadTab(tab);
  await onReloaded;
  assertResources(
    resourceWatcher.getAllResources(resourceWatcher.TYPES.CONSOLE_MESSAGE),
    []
  );

  info("Append some resources again to test unwatching");
  await logMessages(tab.linkedBrowser, messages);
  await waitUntil(
    () =>
      resourceWatcher.getAllResources(resourceWatcher.TYPES.CONSOLE_MESSAGE)
        .length === messages.length
  );

  info("Check the resources after unwatching");
  resourceWatcher.unwatchResources([resourceWatcher.TYPES.CONSOLE_MESSAGE], {
    onAvailable,
  });
  assertResources(
    resourceWatcher.getAllResources(resourceWatcher.TYPES.CONSOLE_MESSAGE),
    []
  );

  targetCommand.destroy();
  await client.close();
});

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
