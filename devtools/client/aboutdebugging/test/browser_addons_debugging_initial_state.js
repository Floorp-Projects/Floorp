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

add_task(function* () {
  for (let testData of TEST_DATA) {
    yield testCheckboxState(testData);
  }
});

function* testCheckboxState(testData) {
  info("Set preferences as defined by the current test data.");
  yield new Promise(resolve => {
    let options = {"set": [
      ["devtools.chrome.enabled", testData.chromeEnabled],
      ["devtools.debugger.remote-enabled", testData.debuggerRemoteEnable],
    ]};
    SpecialPowers.pushPrefEnv(options, resolve);
  });

  let { tab, document } = yield openAboutDebugging("addons");
  yield waitForInitialAddonList(document);

  info("Install a test addon.");
  yield installAddon({
    document,
    path: "addons/unpacked/install.rdf",
    name: ADDON_NAME,
  });

  info("Test checkbox checked state.");
  let addonDebugCheckbox = document.querySelector("#enable-addon-debugging");
  is(addonDebugCheckbox.checked, testData.expected,
    "Addons debugging checkbox should be in expected state.");

  info("Test debug buttons disabled state.");
  let debugButtons = [...document.querySelectorAll("#addons .debug-button")];
  ok(debugButtons.every(b => b.disabled != testData.expected),
    "Debug buttons should be in the expected state");

  info("Uninstall test addon installed earlier.");
  yield uninstallAddon({document, id: ADDON_ID, name: ADDON_NAME});

  yield closeAboutDebugging(tab);
}
