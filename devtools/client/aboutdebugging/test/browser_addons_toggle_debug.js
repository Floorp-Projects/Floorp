/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that individual Debug buttons are disabled when "Addons debugging"
// is disabled.
// Test that the buttons are updated dynamically if the preference changes.

const ADDON_ID = "test-devtools@mozilla.org";
const ADDON_NAME = "test-devtools";

add_task(function* () {
  info("Turn off addon debugging.");
  yield new Promise(resolve => {
    let options = {"set": [
      ["devtools.chrome.enabled", false],
      ["devtools.debugger.remote-enabled", false],
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

  let addonDebugCheckbox = document.querySelector("#enable-addon-debugging");
  ok(!addonDebugCheckbox.checked, "Addons debugging should be disabled.");

  info("Check all debug buttons are disabled.");
  let debugButtons = [...document.querySelectorAll("#addons .debug-button")];
  ok(debugButtons.every(b => b.disabled), "Debug buttons should be disabled");

  info("Click on 'Enable addons debugging' checkbox.");
  let addonsContainer = document.getElementById("addons");
  let onAddonsMutation = waitForMutation(addonsContainer,
    { subtree: true, attributes: true });
  addonDebugCheckbox.click();
  yield onAddonsMutation;

  info("Check all debug buttons are enabled.");
  ok(addonDebugCheckbox.checked, "Addons debugging should be enabled.");
  debugButtons = [...document.querySelectorAll("#addons .debug-button")];
  ok(debugButtons.every(b => !b.disabled), "Debug buttons should be enabled");

  info("Click again on 'Enable addons debugging' checkbox.");
  onAddonsMutation = waitForMutation(addonsContainer,
    { subtree: true, attributes: true });
  addonDebugCheckbox.click();
  yield onAddonsMutation;

  info("Check all debug buttons are disabled again.");
  debugButtons = [...document.querySelectorAll("#addons .debug-button")];
  ok(debugButtons.every(b => b.disabled), "Debug buttons should be disabled");

  info("Uninstall addon installed earlier.");
  yield uninstallAddon({document, id: ADDON_ID, name: ADDON_NAME});

  yield closeAboutDebugging(tab);
});
