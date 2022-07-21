/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that watching/unwatching multiple times works as expected

add_task(async function() {
  const TEST_URL = "data:text/html;charset=utf-8,<!DOCTYPE html>foo";
  const tab = await addTab(TEST_URL);

  const { client, resourceCommand, targetCommand } = await initResourceCommand(
    tab
  );

  let resources = [];
  const onAvailable = _resources => {
    resources.push(..._resources);
  };

  info("Watch for error messages resources");
  await resourceCommand.watchResources([resourceCommand.TYPES.ERROR_MESSAGE], {
    onAvailable,
  });

  ok(
    resourceCommand.isResourceWatched(resourceCommand.TYPES.ERROR_MESSAGE),
    "The error message resource is currently been watched."
  );

  is(
    resources.length,
    0,
    "no resources were received after the first watchResources call"
  );

  info("Trigger an error in the page");
  await ContentTask.spawn(tab.linkedBrowser, [], function frameScript() {
    const document = content.document;
    const scriptEl = document.createElement("script");
    scriptEl.textContent = `document.unknownFunction()`;
    document.body.appendChild(scriptEl);
  });

  await waitFor(() => resources.length === 1);
  const EXPECTED_ERROR_MESSAGE =
    "TypeError: document.unknownFunction is not a function";
  is(
    resources[0].pageError.errorMessage,
    EXPECTED_ERROR_MESSAGE,
    "The resource was received"
  );

  info("Unwatching resources…");
  resourceCommand.unwatchResources([resourceCommand.TYPES.ERROR_MESSAGE], {
    onAvailable,
  });

  ok(
    !resourceCommand.isResourceWatched(resourceCommand.TYPES.ERROR_MESSAGE),
    "The error message resource is no longer been watched."
  );
  // clearing resources
  resources = [];

  info("…and watching again");
  await resourceCommand.watchResources([resourceCommand.TYPES.ERROR_MESSAGE], {
    onAvailable,
  });

  ok(
    resourceCommand.isResourceWatched(resourceCommand.TYPES.ERROR_MESSAGE),
    "The error message resource is been watched again."
  );
  is(
    resources.length,
    1,
    "we retrieve the expected number of existing resources"
  );
  is(
    resources[0].pageError.errorMessage,
    EXPECTED_ERROR_MESSAGE,
    "The resource is the expected one"
  );

  targetCommand.destroy();
  await client.close();
});
