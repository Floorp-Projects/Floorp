/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head-mocks.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "head-mocks.js", this);

/**
 * Check whether can toggle enable/disable connection prompt setting.
 */
add_task(async function() {
  // enable USB devices mocks
  const mocks = new Mocks();
  const runtime = mocks.createUSBRuntime("1337id", {
    deviceName: "Fancy Phone",
    name: "Lorem ipsum",
  });

  info("Set initial state for test");
  await pushPref("devtools.debugger.prompt-connection", true);

  // open a remote runtime page
  const { document, tab } = await openAboutDebugging();

  mocks.emitUSBUpdate();
  await connectToRuntime("Fancy Phone", document);
  await selectRuntime("Fancy Phone", "Lorem ipsum", document);

  info("Check whether connection prompt toggle button exists");
  let connectionPromptToggleButton =
    document.querySelector(".js-connection-prompt-toggle-button");
  ok(connectionPromptToggleButton, "Toggle button existed");
  ok(connectionPromptToggleButton.textContent.includes("Disable"),
    "Toggle button shows 'Disable'");

  info("Click on the toggle button");
  connectionPromptToggleButton =
    document.querySelector(".js-connection-prompt-toggle-button");
  connectionPromptToggleButton.click();
  info("Wait until the toggle button text is updated");
  await waitUntil(() => connectionPromptToggleButton.textContent.includes("Enable"));
  info("Check the preference");
  const disabledPref = runtime.getPreference("devtools.debugger.prompt-connection");
  is(disabledPref, false, "The preference should be updated");

  info("Click on the toggle button again");
  connectionPromptToggleButton.click();
  info("Wait until the toggle button text is updated");
  await waitUntil(() => connectionPromptToggleButton.textContent.includes("Disable"));
  info("Check the preference");
  const enabledPref = runtime.getPreference("devtools.debugger.prompt-connection");
  is(enabledPref, true, "The preference should be updated");

  await removeTab(tab);
});
