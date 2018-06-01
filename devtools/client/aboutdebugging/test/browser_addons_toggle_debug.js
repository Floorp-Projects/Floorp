/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that individual Debug buttons are disabled when "Addons debugging"
// is disabled.
// Test that the buttons are updated dynamically if the preference changes.

const ADDON_ID = "test-devtools@mozilla.org";
const ADDON_NAME = "test-devtools";

add_task(async function() {
  info("Turn off addon debugging.");
  await new Promise(resolve => {
    const options = {"set": [
      ["devtools.chrome.enabled", false],
      ["devtools.debugger.remote-enabled", false],
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

  const addonDebugCheckbox = document.querySelector("#enable-addon-debugging");
  ok(!addonDebugCheckbox.checked, "Addons debugging should be disabled.");

  info("Check all debug buttons are disabled.");
  const debugButtons = [...document.querySelectorAll("#addons .debug-button")];
  ok(debugButtons.every(b => b.disabled), "Debug buttons should be disabled");

  info("Click on 'Enable addons debugging' checkbox.");
  addonDebugCheckbox.click();

  info("Wait until all debug buttons are enabled.");
  waitUntil(() => addonDebugCheckbox.checked && areDebugButtonsEnabled(document), 100);
  info("Addons debugging should be enabled and debug buttons are enabled");

  info("Click again on 'Enable addons debugging' checkbox.");
  addonDebugCheckbox.click();

  info("Wait until all debug buttons are enabled.");
  waitUntil(() => areDebugButtonsDisabled(document), 100);
  info("All debug buttons are disabled again.");

  info("Uninstall addon installed earlier.");
  await uninstallAddon({document, id: ADDON_ID, name: ADDON_NAME});

  await closeAboutDebugging(tab);
});

function getDebugButtons(document) {
  return [...document.querySelectorAll("#addons .debug-button")];
}

function areDebugButtonsEnabled(document) {
  return getDebugButtons(document).every(b => !b.disabled);
}

function areDebugButtonsDisabled(document) {
  return getDebugButtons(document).every(b => b.disabled);
}
