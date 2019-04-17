/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/* import-globals-from helper-addons.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-addons.js", this);

// Test that the reload button updates the addon list with the correct metadata.
add_task(async function() {
  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  const EXTENSION_ID = "test-devtools@mozilla.org";
  const EXTENSION_NAME = "Temporary web extension";

  const addonFile = await installTemporaryExtensionFromXPI({
    id: EXTENSION_ID,
    name: EXTENSION_NAME,
  }, document);

  const target = findDebugTargetByText(EXTENSION_NAME, document);
  ok(!!target, "The temporary extension is installed with the expected name");

  info("Update the name of the temporary extension in the manifest");
  updateTemporaryXPI({}, addonFile);

  info("Click on the reload button for the invalid temporary extension");
  const reloadButton = target.querySelector(".js-temporary-extension-reload-button");
  reloadButton.click();

  info("Wait until the error message appears");
  await waitUntil(() => target.querySelector(".qa-temporary-extension-reload-error"));
  ok(true, "The error message of reloading appears");

  info("Click on the reload button for the valid temporary extension");
  updateTemporaryXPI({ id: EXTENSION_ID, name: EXTENSION_NAME }, addonFile);
  reloadButton.click();

  info("Wait until the error message disappears");
  await waitUntil(() => !target.querySelector(".qa-temporary-extension-reload-error"));
  ok(true, "The error message of reloading disappears");

  info("Click on the remove button for the temporary extension");
  const removeButton = target.querySelector(".js-temporary-extension-remove-button");
  removeButton.click();

  info("Wait until the debug target with the extension disappears");
  await waitUntil(() => !findDebugTargetByText(EXTENSION_NAME, document));

  await removeTab(tab);
});
