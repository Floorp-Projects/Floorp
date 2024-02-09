/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the clearResources function of the ResourceCommand

add_task(async () => {
  const tab = await addTab(`${URL_ROOT_SSL}empty.html`);
  const { client, resourceCommand, targetCommand } = await initResourceCommand(
    tab
  );

  info("Assert the initial no of resources");
  assertNoOfResources(resourceCommand, 0, 0);

  const onAvailable = () => {};
  const onUpdated = () => {};

  await resourceCommand.watchResources(
    [
      resourceCommand.TYPES.CONSOLE_MESSAGE,
      resourceCommand.TYPES.NETWORK_EVENT,
    ],
    { onAvailable, onUpdated }
  );

  info("Log some messages");
  await logConsoleMessages(tab.linkedBrowser, ["log1", "log2", "log3"]);

  info("Trigger some network requests");
  const EXAMPLE_DOMAIN = "https://example.com/";
  await triggerNetworkRequests(tab.linkedBrowser, [
    `await fetch("${EXAMPLE_DOMAIN}/request1.html", { method: "GET" });`,
    `await fetch("${EXAMPLE_DOMAIN}/request2.html", { method: "GET" });`,
  ]);

  assertNoOfResources(resourceCommand, 3, 2);

  info("Clear the network event resources");
  await resourceCommand.clearResources([resourceCommand.TYPES.NETWORK_EVENT]);
  assertNoOfResources(resourceCommand, 3, 0);

  info("Clear the console message resources");
  await resourceCommand.clearResources([resourceCommand.TYPES.CONSOLE_MESSAGE]);
  assertNoOfResources(resourceCommand, 0, 0);

  resourceCommand.unwatchResources(
    [
      resourceCommand.TYPES.CONSOLE_MESSAGE,
      resourceCommand.TYPES.NETWORK_EVENT,
    ],
    { onAvailable, onUpdated, ignoreExistingResources: true }
  );

  targetCommand.destroy();
  await client.close();
});

function assertNoOfResources(
  resourceCommand,
  expectedNoOfConsoleMessageResources,
  expectedNoOfNetworkEventResources
) {
  const actualNoOfConsoleMessageResources = resourceCommand.getAllResources(
    resourceCommand.TYPES.CONSOLE_MESSAGE
  ).length;
  is(
    actualNoOfConsoleMessageResources,
    expectedNoOfConsoleMessageResources,
    `There are ${actualNoOfConsoleMessageResources} console messages resources`
  );

  const actualNoOfNetworkEventResources = resourceCommand.getAllResources(
    resourceCommand.TYPES.NETWORK_EVENT
  ).length;
  is(
    actualNoOfNetworkEventResources,
    expectedNoOfNetworkEventResources,
    `There are ${actualNoOfNetworkEventResources} network event resources`
  );
}

function logConsoleMessages(browser, messages) {
  return SpecialPowers.spawn(browser, [messages], innerMessages => {
    for (const message of innerMessages) {
      content.console.log(message);
    }
  });
}
