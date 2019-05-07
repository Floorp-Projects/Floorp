/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-addons.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-addons.js", this);

/**
 * Test the the path used to install a temporary addon is saved in a preference and reused
 * next time the feature is used.
 */

const EXTENSION_PATH = "resources/test-temporary-extension/manifest.json";
const EXTENSION_NAME = "test-temporary-extension";
const LAST_DIR_PREF = "devtools.aboutdebugging.tmpExtDirPath";

// Check that the preference is updated when trying to install a temporary extension.
add_task(async function testPreferenceUpdatedAfterInstallingExtension() {
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(LAST_DIR_PREF);
  });
  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  await installTemporaryExtension(EXTENSION_PATH, EXTENSION_NAME, document);

  info("Check whether the selected dir sets into the pref");
  const lastDirPath = Services.prefs.getCharPref(LAST_DIR_PREF, "");
  const expectedPath = getTestFilePath("resources/test-temporary-extension");
  is(lastDirPath, expectedPath, "The selected dir should set into the pref");

  await waitUntil(() => findDebugTargetByText(EXTENSION_NAME, document));
  await removeTemporaryExtension(EXTENSION_NAME, document);
  await removeTab(tab);
});

// Check that the preference is updated when trying to install a temporary extension.
add_task(async function testPreferenceRetrievedWhenInstallingExtension() {
  const selectedDir = getTestFilePath("resources/packaged-extension");

  await pushPref(LAST_DIR_PREF, selectedDir);

  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  const MockFilePicker = SpecialPowers.MockFilePicker;
  MockFilePicker.init(window);
  const onFilePickerShown = new Promise(resolve => {
    MockFilePicker.showCallback = fp => {
      resolve(fp);
    };
  });
  document.querySelector(".qa-temporary-extension-install-button").click();

  info("Check whether the shown dir is same as the pref");
  const fp = await onFilePickerShown;
  is(fp.displayDirectory.path, selectedDir, "Shown directory sets as same as the pref");

  await removeTab(tab);
});
