/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from head-addons-script.js */

"use strict";

// Load addons helpers
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "head-addons-script.js", this);

/**
 * Test that an "empty" message is displayed when there are no debug targets in a debug
 * target pane.
 */

const EXTENSION_PATH = "resources/test-temporary-extension/manifest.json";
const EXTENSION_NAME = "test-temporary-extension";

add_task(async function() {
  prepareCollapsibilitiesTest();

  const { document, tab } = await openAboutDebugging();

  info("Check that the temporary extensions pane is empty");
  const temporaryExtensionPane = getDebugTargetPane("Temporary Extensions", document);
  ok(!temporaryExtensionPane.querySelector(".js-debug-target-item"),
    "Temporary Extensions pane contains no debug target");

  info("Check an empty target pane message is displayed");
  ok(temporaryExtensionPane.querySelector(".js-debug-target-list-empty"),
    "An empty target list message is displayed");

  info("Install a temporary extension");
  await installTemporaryExtension(EXTENSION_PATH, EXTENSION_NAME, document);

  info("Wait until a debug target item appears");
  await waitUntil(() => temporaryExtensionPane.querySelector(".js-debug-target-item"));

  info("Check the empty target pane message is no longer displayed");
  ok(!temporaryExtensionPane.querySelector(".js-debug-target-list-empty"),
    "The empty target list message is no longer displayed");

  const temporaryExtensionItem =
    temporaryExtensionPane.querySelector(".js-debug-target-item");
  ok(temporaryExtensionItem, "Temporary Extensions pane now shows debug target");

  info("Remove the temporary extension");
  temporaryExtensionItem.querySelector(".js-temporary-extension-remove-button").click();

  info("Wait until the debug target item disappears");
  await waitUntil(() => !temporaryExtensionPane.querySelector(".js-debug-target-item"));

  info("Check the empty target pane message is displayed again");
  ok(temporaryExtensionPane.querySelector(".js-debug-target-list-empty"),
    "An empty target list message is displayed again");

  await removeTab(tab);
});
