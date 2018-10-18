/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const PACKAGED_ADDON_NAME = "bug 1273184";

function getTargetEl(document, id) {
  return document.querySelector(`[data-addon-id="${id}"]`);
}

function getRemoveButton(document, id) {
  return document.querySelector(`[data-addon-id="${id}"] .uninstall-button`);
}

add_task(async function removeLegacyExtension() {
  const addonID = "test-devtools@mozilla.org";
  const addonName = "test-devtools";

  const { tab, document } = await openAboutDebugging("addons");
  await waitForInitialAddonList(document);

  // Install this add-on, and verify that it appears in the about:debugging UI
  await installAddon({
    document,
    path: "addons/unpacked/install.rdf",
    name: addonName,
  });

  ok(getTargetEl(document, addonID), "add-on is shown");

  info("Click on the remove button and wait until the addon container is removed");
  getRemoveButton(document, addonID).click();
  await waitUntil(() => !getTargetEl(document, addonID), 100);

  info("add-on is not shown");

  await closeAboutDebugging(tab);
});

add_task(async function removeWebextension() {
  const addonID = "test-devtools-webextension@mozilla.org";
  const addonName = "test-devtools-webextension";

  const { tab, document } = await openAboutDebugging("addons");
  await waitForInitialAddonList(document);

  const addonFile = ExtensionTestCommon.generateXPI({
    manifest: {
      name: addonName,
      applications: {
        gecko: {id: addonID},
      },
    },
  });
  registerCleanupFunction(() => addonFile.remove(false));

  // Install this add-on, and verify that it appears in the about:debugging UI
  await installAddon({
    document,
    file: addonFile,
    name: addonName,
    isWebExtension: true,
  });

  ok(getTargetEl(document, addonID), "add-on is shown");

  info("Click on the remove button and wait until the addon container is removed");
  getRemoveButton(document, addonID).click();
  await waitUntil(() => !getTargetEl(document, addonID), 100);

  info("add-on is not shown");

  await closeAboutDebugging(tab);
});

add_task(async function onlyTempInstalledAddonsCanBeRemoved() {
  const { tab, document } = await openAboutDebugging("addons");
  await waitForInitialAddonList(document);

  await installAddonWithManager(getSupportsFile("addons/bug1273184.xpi").file);
  const addon = await getAddonByID("bug1273184@tests");

  info("Wait until addon appears in about:debugging#addons");
  await waitUntilAddonContainer(PACKAGED_ADDON_NAME, document);

  const removeButton = getRemoveButton(document, addon.id);
  ok(!removeButton, "remove button is not shown");

  await tearDownAddon(addon);
  await closeAboutDebugging(tab);
});
