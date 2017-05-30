/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function getTargetEl(document, id) {
  return document.querySelector(`[data-addon-id="${id}"]`);
}

function getRemoveButton(document, id) {
  return document.querySelector(`[data-addon-id="${id}"] .uninstall-button`);
}

add_task(function* removeLegacyExtension() {
  const addonID = "test-devtools@mozilla.org";
  const addonName = "test-devtools";

  const { tab, document } = yield openAboutDebugging("addons");
  yield waitForInitialAddonList(document);

  // Install this add-on, and verify that it appears in the about:debugging UI
  yield installAddon({
    document,
    path: "addons/unpacked/install.rdf",
    name: addonName,
  });

  ok(getTargetEl(document, addonID), "add-on is shown");

  // Click the remove button and wait for the DOM to change.
  const addonListMutation = waitForMutation(
    getTemporaryAddonList(document).parentNode,
    { childList: true, subtree: true });
  getRemoveButton(document, addonID).click();
  yield addonListMutation;

  ok(!getTargetEl(document, addonID), "add-on is not shown");

  yield closeAboutDebugging(tab);
});

add_task(function* removeWebextension() {
  const addonID = "test-devtools-webextension@mozilla.org";
  const addonName = "test-devtools-webextension";

  const { tab, document } = yield openAboutDebugging("addons");
  yield waitForInitialAddonList(document);

  // Install this add-on, and verify that it appears in the about:debugging UI
  yield installAddon({
    document,
    path: "addons/test-devtools-webextension/manifest.json",
    name: addonName,
    isWebExtension: true,
  });

  ok(getTargetEl(document, addonID), "add-on is shown");

  // Click the remove button and wait for the DOM to change.
  const addonListMutation = waitForMutation(
    getTemporaryAddonList(document).parentNode,
    { childList: true, subtree: true });
  getRemoveButton(document, addonID).click();
  yield addonListMutation;

  ok(!getTargetEl(document, addonID), "add-on is not shown");

  yield closeAboutDebugging(tab);
});

add_task(function* onlyTempInstalledAddonsCanBeRemoved() {
  const { tab, document } = yield openAboutDebugging("addons");
  yield waitForInitialAddonList(document);
  const onAddonListUpdated = waitForMutation(getAddonList(document),
                                             { childList: true });
  yield installAddonWithManager(getSupportsFile("addons/bug1273184.xpi").file);
  yield onAddonListUpdated;
  const addon = yield getAddonByID("bug1273184@tests");

  const removeButton = getRemoveButton(document, addon.id);
  ok(!removeButton, "remove button is not shown");

  yield tearDownAddon(addon);
  yield closeAboutDebugging(tab);
});
