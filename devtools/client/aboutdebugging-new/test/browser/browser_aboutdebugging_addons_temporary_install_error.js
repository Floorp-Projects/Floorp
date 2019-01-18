/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-addons.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-addons.js", this);

/**
 * Test that the installation error messages are displayed when installing temporary
 * extensions.
 */

const BAD_EXTENSION_PATH = "resources/bad-extension/manifest.json";
const EXTENSION_PATH = "resources/test-temporary-extension/manifest.json";
const EXTENSION_NAME = "test-temporary-extension";

add_task(async function() {
  const { document, tab } = await openAboutDebugging();

  info("Install a bad extension");
  // Do not use installTemporaryAddon here since the install will fail.
  prepareMockFilePicker(BAD_EXTENSION_PATH);
  document.querySelector(".js-temporary-extension-install-button").click();

  info("Wait until the install error message appears");
  await waitUntil(() => document.querySelector(".js-message"));
  const installError = document.querySelector(".js-message");
  ok(installError.textContent.includes("JSON.parse: unexpected keyword"),
    "The expected installation error is displayed: " + installError.textContent);

  info("Install a valid extension to make the message disappear");
  await installTemporaryExtension(EXTENSION_PATH, EXTENSION_NAME, document);

  info("Wait until the error message disappears");
  await waitUntil(() => !document.querySelector(".js-message"));

  info("Wait for the temporary addon to be displayed as a debug target");
  await waitUntil(() => findDebugTargetByText(EXTENSION_NAME, document));

  await removeTemporaryExtension(EXTENSION_NAME, document);

  await removeTab(tab);
});
