/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-addons.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-addons.js", this);
/* import-globals-from helper-collapsibilities.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-collapsibilities.js", this);

const TEMPORARY_EXTENSION_ID = "test-devtools-webextension@mozilla.org";
const TEMPORARY_EXTENSION_NAME = "test-devtools-webextension";
const REGULAR_EXTENSION_ID = "packaged-extension@tests";
const REGULAR_EXTENSION_NAME = "Packaged extension";

const TEST_DATA = [
  {
    chromeEnabled: false,
    debuggerRemoteEnable: false,
    isEnabledAtInitial: false,
  }, {
    chromeEnabled: false,
    debuggerRemoteEnable: true,
    isEnabledAtInitial: false,
  }, {
    chromeEnabled: true,
    debuggerRemoteEnable: false,
    isEnabledAtInitial: false,
  }, {
    chromeEnabled: true,
    debuggerRemoteEnable: true,
    isEnabledAtInitial: true,
  },
];

/**
 * Check the state of extension debug setting checkbox, inspect buttons and prefs on
 * this-firefox at initializing and after toggling.
 */
add_task(async function() {
  info("Force all debug target panes to be expanded");
  prepareCollapsibilitiesTest();

  for (const testData of TEST_DATA) {
    await testState(testData);
  }
});

async function testState({ chromeEnabled, debuggerRemoteEnable, isEnabledAtInitial }) {
  info(`Test for chromeEnabled: ${ chromeEnabled }, ` +
       `debuggerRemoteEnable:${ debuggerRemoteEnable }`);

  info("Set initial state for test");
  await pushPref("devtools.chrome.enabled", chromeEnabled);
  await pushPref("devtools.debugger.remote-enabled", debuggerRemoteEnable);

  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);
  await installExtensions(document);

  info("Check the status of extension debug setting checkbox and inspect buttons");
  const checkbox = document.querySelector(".qa-extension-debug-checkbox");
  ok(checkbox, "Extension debug setting checkbox exists");
  is(checkbox.checked, isEnabledAtInitial, "The state of checkbox is correct");
  assertInspectButtons(isEnabledAtInitial, document);

  info("Check the status after toggling checkbox");
  checkbox.click();
  await waitUntil(() => checkbox.checked !== isEnabledAtInitial);
  ok(true, "The state of checkbox is changed correctly after toggling");
  assertPreferences(!isEnabledAtInitial);
  assertInspectButtons(!isEnabledAtInitial, document);

  await uninstallExtensions(document);
  await removeTab(tab);
}

function assertPreferences(expected) {
  for (const pref of ["devtools.chrome.enabled", "devtools.debugger.remote-enabled"]) {
    is(Services.prefs.getBoolPref(pref), expected, `${ pref } should be ${expected}`);
  }
}

function assertInspectButtons(shouldBeEnabled, document) {
  assertInspectButtonsOnCategory(shouldBeEnabled, "Temporary Extensions", document);
  assertInspectButtonsOnCategory(shouldBeEnabled, "Extensions", document);
  // Inspect button on Tabs category should be always enabled.
  assertInspectButtonsOnCategory(true, "Tabs", document);
}

function assertInspectButtonsOnCategory(shouldBeEnabled, category, document) {
  const pane = getDebugTargetPane(category, document);
  const buttons = pane.querySelectorAll(".qa-debug-target-inspect-button");
  ok([...buttons].every(b => b.disabled !== shouldBeEnabled),
     `disabled attribute should be ${ !shouldBeEnabled } on ${ category }`);
}

async function installExtensions(document) {
  await installTemporaryExtensionFromXPI({
    id: TEMPORARY_EXTENSION_ID,
    name: TEMPORARY_EXTENSION_NAME,
  }, document);

  await installRegularExtension("resources/packaged-extension/packaged-extension.xpi");
}

async function uninstallExtensions(document) {
  await removeTemporaryExtension(TEMPORARY_EXTENSION_NAME, document);
  await removeExtension(REGULAR_EXTENSION_ID, REGULAR_EXTENSION_NAME, document);
}
