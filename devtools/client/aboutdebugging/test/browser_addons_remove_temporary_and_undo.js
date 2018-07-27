/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test for temporary removing and undoing an extension.

add_task(async () => {
  const addonID = "test-devtools-webextension@mozilla.org";
  const addonName = "test-devtools-webextension";

  const { document, tab } = await openAboutDebugging("addons");
  await waitForInitialAddonList(document);

  // Install this add-on, and verify that it appears in the about:debugging UI
  await installAddon({
    document,
    path: "addons/test-devtools-webextension/manifest.json",
    name: addonName,
    isWebExtension: true,
  });
  ok(getTargetEl(document, addonID), "Add-on is shown");

  info("Uninstall the addon but allow to undo the action " +
       "(same as when uninstalling from about:addons)");
  await uninstallAddon({ document, id: addonID, name: addonName, allowUndo: true });
  await waitUntil(() => !getTargetEl(document, addonID), 100);
  ok(true, "Add-on is hidden after removal");

  info("Canceling uninstalling to undo");
  await cancelUninstallAddon({ document, id: addonID, name: addonName });
  await waitUntil(() => getTargetEl(document, addonID), 100);
  ok(true, "Add-on re-appeared after undo");

  await uninstallAddon({ document, id: addonID, name: addonName });
  await closeAboutDebugging(tab);
});

function getTargetEl(document, id) {
  return document.querySelector(`[data-addon-id="${id}"]`);
}
