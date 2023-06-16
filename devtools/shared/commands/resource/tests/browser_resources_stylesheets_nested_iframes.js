/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that stylesheets are retrieved even if an iframe does not have a content document.

const TEST_URI = URL_ROOT_SSL + "stylesheets-nested-iframes.html";

add_task(async function () {
  const tab = await addTab(TEST_URI);

  const { client, resourceCommand, targetCommand } = await initResourceCommand(
    tab
  );

  info("Check whether ResourceCommand gets existing stylesheet");
  const availableResources = [];
  await resourceCommand.watchResources([resourceCommand.TYPES.STYLESHEET], {
    onAvailable: resources => availableResources.push(...resources),
  });

  // Bug 285395 limits the number of nested iframes to 10, and we have one stylesheet per document.
  await waitFor(() => availableResources.length >= 10);

  is(
    availableResources.length,
    10,
    "Got the expected number of stylesheets, even with documentless iframes"
  );

  targetCommand.destroy();
  await client.close();
});
