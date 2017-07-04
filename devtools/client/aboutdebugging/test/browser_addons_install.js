/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

loader.lazyImporter(this, "AddonTestUtils",
  "resource://testing-common/AddonTestUtils.jsm");

AddonTestUtils.initMochitest(this);

const ADDON_ID = "test-devtools@mozilla.org";
const ADDON_NAME = "test-devtools";

function mockFilePicker(window, file) {
  // Mock the file picker to select a test addon
  let MockFilePicker = SpecialPowers.MockFilePicker;
  MockFilePicker.init(window);
  MockFilePicker.setFiles([file]);
}

/**
 * Write out an extension with a manifest.json to dir.
 *
 * @param {object} manfiest
 *        The manifest file as an object.
 * @param {nsIFile} dir
 *        An nsIFile for representing the output folder.
 * @return {Promise} Promise that resolves to the output folder when done.
 */
function promiseWriteWebManifestForExtension(manifest, dir) {
  let files = {
    "manifest.json": JSON.stringify(manifest),
  };
  return AddonTestUtils.promiseWriteFilesToExtension(
    dir.path, manifest.applications.gecko.id, files, true);
}

add_task(function* testLegacyInstallSuccess() {
  let { tab, document } = yield openAboutDebugging("addons");
  yield waitForInitialAddonList(document);

  // Install this add-on, and verify that it appears in the about:debugging UI
  yield installAddon({
    document,
    path: "addons/unpacked/install.rdf",
    name: ADDON_NAME,
  });

  // Install the add-on, and verify that it disappears in the about:debugging UI
  yield uninstallAddon({document, id: ADDON_ID, name: ADDON_NAME});

  yield closeAboutDebugging(tab);
});

add_task(function* testWebextensionInstallError() {
  let { tab, document, window } = yield openAboutDebugging("addons");
  yield waitForInitialAddonList(document);

  // Start an observer that looks for the install error before
  // actually doing the install
  let top = document.querySelector(".addons-top");
  let promise = waitForMutation(top, { childList: true });

  mockFilePicker(window, getSupportsFile("addons/bad/manifest.json").file);

  // Trigger the file picker by clicking on the button
  document.getElementById("load-addon-from-file").click();

  // Now wait for the install error to appear.
  yield promise;

  // And check that it really is there.
  let err = document.querySelector(".addons-install-error");
  isnot(err, null, "Addon install error message appeared");

  yield closeAboutDebugging(tab);
});

add_task(function* testWebextensionInstallErrorRetry() {
  let { tab, document, window } = yield openAboutDebugging("addons");
  yield waitForInitialAddonList(document);

  let tempdir = AddonTestUtils.tempDir.clone();
  let addonId = "invalid-addon-install-retry@mozilla.org";
  let addonName = "invalid-addon-install-retry";
  let manifest = {
    name: addonName,
    description: "test invalid-addon-install-retry",
    // eslint-disable-next-line camelcase
    manifest_version: 2,
    version: "1.0",
    applications: { gecko: { id: addonId } },
    // These should all be wrapped in arrays.
    // eslint-disable-next-line camelcase
    content_scripts: { matches: "http://*/", js: "foo.js" },
  };

  yield promiseWriteWebManifestForExtension(manifest, tempdir);

  // Start an observer that looks for the install error before
  // actually doing the install.
  let top = document.querySelector(".addons-top");
  let contentUpdated = waitForMutation(top, { childList: true });

  // Mock the file picker to select a test addon.
  let manifestFile = tempdir.clone();
  manifestFile.append(addonId, "manifest.json");
  mockFilePicker(window, manifestFile);

  // Trigger the file picker by clicking on the button.
  document.getElementById("load-addon-from-file").click();

  // Now wait for the install error to appear.
  yield contentUpdated;

  // Check that the error is shown.
  let err = document.querySelector(".addons-install-error");
  isnot(err, null, "Addon install error message appeared");
  let retryButton = document.querySelector("button.addons-install-retry");
  is(retryButton.textContent, "Retry", "Retry button has a good label");

  // Fix the manifest so the add-on will install.
  // eslint-disable-next-line camelcase
  manifest.content_scripts = [{
    matches: ["http://*/"],
    js: ["foo.js"],
  }];
  yield promiseWriteWebManifestForExtension(manifest, tempdir);

  let getAddonEl = () => document.querySelector(`[data-addon-id="${addonId}"]`);

  // Verify this add-on isn't installed yet.
  ok(!getAddonEl(), "Addon is not installed yet");

  // Prepare to wait for the add-on to be added to the temporary list.
  let addonAdded = waitForMutation(
    getTemporaryAddonList(document), { childList: true });

  // Retry the install.
  retryButton.click();

  // Wait for the add-on to be shown.
  yield addonAdded;

  // Verify the add-on is installed.
  ok(getAddonEl(), "Addon is installed");

  // Install the add-on, and verify that it disappears in the about:debugging UI
  yield uninstallAddon({document, id: addonId, name: addonName});

  yield closeAboutDebugging(tab);
});
