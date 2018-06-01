/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that addons debugging controls are properly enabled/disabled depending
// on the values of the relevant preferences:
// - devtools.chrome.enabled
// - devtools.debugger.remote-enabled

const ADDON_ID = "test-devtools@mozilla.org";
const ADDON_NAME = "test-devtools";

const TEST_DATA = [
  {
    chromeEnabled: false,
    debuggerRemoteEnable: false,
    expected: false,
  }, {
    chromeEnabled: false,
    debuggerRemoteEnable: true,
    expected: false,
  }, {
    chromeEnabled: true,
    debuggerRemoteEnable: false,
    expected: false,
  }, {
    chromeEnabled: true,
    debuggerRemoteEnable: true,
    expected: true,
  }
];

add_task(async function() {
  for (const testData of TEST_DATA) {
    await testCheckboxState(testData);
  }
});

async function testCheckboxState(testData) {
  info("Set preferences as defined by the current test data.");
  await new Promise(resolve => {
    const options = {"set": [
      ["devtools.chrome.enabled", testData.chromeEnabled],
      ["devtools.debugger.remote-enabled", testData.debuggerRemoteEnable],
    ]};
    SpecialPowers.pushPrefEnv(options, resolve);
  });

  const { tab, document } = await openAboutDebugging("addons");
  await waitForInitialAddonList(document);

  info("Install a test addon.");
  await installAddon({
    document,
    path: "addons/unpacked/install.rdf",
    name: ADDON_NAME,
  });

  info("Test checkbox checked state.");
  const addonDebugCheckbox = document.querySelector("#enable-addon-debugging");
  is(addonDebugCheckbox.checked, testData.expected,
    "Addons debugging checkbox should be in expected state.");

  info("Test debug buttons disabled state.");
  const debugButtons = [...document.querySelectorAll("#addons .debug-button")];
  ok(debugButtons.every(b => b.disabled != testData.expected),
    "Debug buttons should be in the expected state");

  info("Uninstall test addon installed earlier.");
  await uninstallAddon({document, id: ADDON_ID, name: ADDON_NAME});

  await closeAboutDebugging(tab);
}
