/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the behavior of ResourceCommand when the top level target changes

const TEST_URI =
  "data:text/html;charset=utf-8,<script>console.log('foo');</script>";

add_task(async function() {
  const tab = await addTab(TEST_URI);

  const { client, resourceCommand, targetCommand } = await initResourceCommand(
    tab
  );
  const { CONSOLE_MESSAGE, SOURCE } = resourceCommand.TYPES;

  info("Check the resources gotten from getAllResources at initial");
  is(
    resourceCommand.getAllResources(CONSOLE_MESSAGE).length,
    0,
    "There is no resources before calling watchResources"
  );

  info(
    "Start to watch the available resources in order to compare with resources gotten from getAllResources"
  );
  const availableResources = [];
  const onAvailable = resources => {
    // Ignore message coming from shared worker started by previous tests and
    // logging late a console message
    resources
      .filter(r => {
        return !r.message.arguments[0].startsWith("[WORKER] started");
      })
      .map(r => availableResources.push(r));
  };
  await resourceCommand.watchResources([CONSOLE_MESSAGE], { onAvailable });

  is(availableResources.length, 1, "Got the page message");
  is(
    availableResources[0].message.arguments[0],
    "foo",
    "Got the expected page message"
  );

  // Register another listener before unregistering the console listener
  // otherwise the resource command stop watching for targets
  const onSourceAvailable = () => {};
  await resourceCommand.watchResources([SOURCE], {
    onAvailable: onSourceAvailable,
  });

  info(
    "Unregister the console listener and check that we no longer listen for console messages"
  );
  resourceCommand.unwatchResources([CONSOLE_MESSAGE], {
    onAvailable,
  });

  let onSwitched = targetCommand.once("switched-target");
  info("Navigate to another process");
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, "about:robots");
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  await onSwitched;

  is(
    availableResources.length,
    1,
    "about:robots doesn't fire any new message, so we should have a new one"
  );

  info("Navigate back to data: URI");
  onSwitched = targetCommand.once("switched-target");
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, TEST_URI);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  await onSwitched;

  is(
    availableResources.length,
    1,
    "the data:URI fired a message, but we are no longer listening to it, so no new one should be notified"
  );
  is(
    resourceCommand.getAllResources(CONSOLE_MESSAGE).length,
    0,
    "As we are no longer listening to CONSOLE message, we should not collect any"
  );

  resourceCommand.unwatchResources([SOURCE], {
    onAvailable: onSourceAvailable,
  });

  targetCommand.destroy();
  await client.close();
});
