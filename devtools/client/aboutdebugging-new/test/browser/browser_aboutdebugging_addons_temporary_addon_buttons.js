/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/* import-globals-from helper-addons.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-addons.js", this);

// Test that the reload button updates the addon list with the correct metadata.
add_task(async function() {
  const { document, tab } = await openAboutDebugging();

  const ORIGINAL_EXTENSION_NAME = "Temporary web extension (original)";
  const UPDATED_EXTENSION_NAME = "Temporary web extension (updated)";
  const EXTENSION_ID = "test-devtools@mozilla.org";

  const manifestBase = {
    "manifest_version": 2,
    "name": ORIGINAL_EXTENSION_NAME,
    "version": "1.0",
    "applications": {
      "gecko": {
        "id": EXTENSION_ID,
      },
    },
  };

  const tempExt = new TemporaryExtension(EXTENSION_ID);
  tempExt.writeManifest(manifestBase);

  info("Install a temporary extension (original)");
  await AddonManager.installTemporaryAddon(tempExt.sourceDir);

  info("Wait until a debug target item appears");
  await waitUntil(() => findDebugTargetByText(ORIGINAL_EXTENSION_NAME, document));

  const originalTarget = findDebugTargetByText(ORIGINAL_EXTENSION_NAME, document);
  ok(!!originalTarget, "The temporary extension isinstalled with the expected name");

  info("Update the name of the temporary extension in the manifest");
  tempExt.writeManifest(Object.assign({}, manifestBase, {name: UPDATED_EXTENSION_NAME}));

  info("Click on the reload button for the temporary extension");
  const reloadButton =
    originalTarget.querySelector(".js-temporary-extension-reload-button");
  reloadButton.click();

  info("Wait until the debug target with the original extension name disappears");
  await waitUntil(() => !findDebugTargetByText(ORIGINAL_EXTENSION_NAME, document));

  info("Wait until the debug target with the updated extension name appears");
  await waitUntil(() => findDebugTargetByText(UPDATED_EXTENSION_NAME, document));

  const updatedTarget = findDebugTargetByText(UPDATED_EXTENSION_NAME, document);
  ok(!!updatedTarget, "The temporary extension name has been updated");

  info("Click on the remove button for the temporary extension");
  const removeButton =
    updatedTarget.querySelector(".js-temporary-extension-remove-button");
  removeButton.click();

  info("Wait until the debug target with the updated extension name disappears");
  await waitUntil(() => !findDebugTargetByText(UPDATED_EXTENSION_NAME, document));

  info("Remove the temporary web extension");
  tempExt.remove();

  await removeTab(tab);
});

add_task(async function() {
  const PACKAGED_EXTENSION_ID = "packaged-extension@tests";
  const PACKAGED_EXTENSION_NAME = "Packaged extension";

  const { document, tab } = await openAboutDebugging();

  await installRegularAddon("resources/packaged-extension/packaged-extension.xpi");

  info("Wait until extension appears in about:debugging");
  await waitUntil(() => findDebugTargetByText(PACKAGED_EXTENSION_NAME, document));
  const target = findDebugTargetByText(PACKAGED_EXTENSION_NAME, document);

  const reloadButton = target.querySelector(".js-temporary-extension-reload-button");
  ok(!reloadButton, "No reload button displayed for a regularly installed extension");

  const removeButton = target.querySelector(".js-temporary-extension-remove-button");
  ok(!removeButton, "No remove button displayed for a regularly installed extension");

  info("Retrieve the extension instance from the addon manager, and uninstall it");
  const extension = await AddonManager.getAddonByID(PACKAGED_EXTENSION_ID);
  extension.uninstall();

  info("Wait until the addon disappears from about:debugging");
  await waitUntil(() => !findDebugTargetByText(PACKAGED_EXTENSION_NAME, document));

  await removeTab(tab);
});
