/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test getAllResources function of the ResourceCommand.

const TEST_URI = "data:text/html;charset=utf-8,getAllResources test";

add_task(async function() {
  const tab = await addTab(TEST_URI);

  const { client, resourceCommand, targetCommand } = await initResourceCommand(
    tab
  );

  info("Check the resources gotten from getAllResources at initial");
  is(
    resourceCommand.getAllResources(resourceCommand.TYPES.CONSOLE_MESSAGE)
      .length,
    0,
    "There is no resources at initial"
  );

  info(
    "Start to watch the available resources in order to compare with resources gotten from getAllResources"
  );
  const availableResources = [];
  const onAvailable = resources => availableResources.push(...resources);
  await resourceCommand.watchResources(
    [resourceCommand.TYPES.CONSOLE_MESSAGE],
    { onAvailable }
  );

  info("Check the resources after some resources are available");
  const messages = ["a", "b", "c"];
  await logMessages(tab.linkedBrowser, messages);

  try {
    await waitFor(() => availableResources.length === messages.length);
  } catch (e) {
    ok(
      false,
      `Didn't receive the expected number of resources. Got ${
        availableResources.length
      }, expected ${messages.length} - ${availableResources
        .map(r => r.message.arguments[0])
        .join(" - ")}`
    );
  }

  assertResources(
    resourceCommand.getAllResources(resourceCommand.TYPES.CONSOLE_MESSAGE),
    availableResources
  );
  assertResources(
    resourceCommand.getAllResources(resourceCommand.TYPES.STYLESHEET),
    []
  );

  info("Check the resources after reloading");
  const onReloaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  gBrowser.reloadTab(tab);
  await onReloaded;
  assertResources(
    resourceCommand.getAllResources(resourceCommand.TYPES.CONSOLE_MESSAGE),
    []
  );

  info("Append some resources again to test unwatching");
  const newMessages = ["d", "e", "f"];
  await logMessages(tab.linkedBrowser, messages);
  try {
    await waitFor(
      () =>
        resourceCommand.getAllResources(resourceCommand.TYPES.CONSOLE_MESSAGE)
          .length === newMessages.length
    );
  } catch (e) {
    const resources = resourceCommand.getAllResources(
      resourceCommand.TYPES.CONSOLE_MESSAGE
    );
    ok(
      false,
      `Didn't receive the expected number of resources. Got ${
        resources.length
      }, expected ${messages.length} - ${resources
        .map(r => r.message.arguments.join(" | "))
        .join(" - ")}`
    );
  }

  info("Check the resources after unwatching");
  resourceCommand.unwatchResources([resourceCommand.TYPES.CONSOLE_MESSAGE], {
    onAvailable,
  });
  assertResources(
    resourceCommand.getAllResources(resourceCommand.TYPES.CONSOLE_MESSAGE),
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
  return SpecialPowers.spawn(browser, [messages], innerMessages => {
    for (const message of innerMessages) {
      content.console.log(message);
    }
  });
}
