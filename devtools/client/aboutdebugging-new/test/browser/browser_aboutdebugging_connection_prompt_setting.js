/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const USB_RUNTIME_ID = "1337id";
const USB_DEVICE_NAME = "Fancy Phone";
const USB_APP_NAME = "Lorem ipsum";

/**
 * Check whether can toggle enable/disable connection prompt setting.
 */
add_task(async function() {
  // enable USB devices mocks
  const mocks = new Mocks();
  const runtime = mocks.createUSBRuntime(USB_RUNTIME_ID, {
    deviceName: USB_DEVICE_NAME,
    name: USB_APP_NAME,
  });

  info("Set initial state for test");
  await pushPref("devtools.debugger.prompt-connection", true);

  // open a remote runtime page
  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  mocks.emitUSBUpdate();
  await connectToRuntime(USB_DEVICE_NAME, document);
  await selectRuntime(USB_DEVICE_NAME, USB_APP_NAME, document);

  info("Check whether connection prompt toggle button exists");
  let connectionPromptToggleButton =
    document.querySelector(".qa-connection-prompt-toggle-button");
  ok(connectionPromptToggleButton, "Toggle button existed");
  ok(connectionPromptToggleButton.textContent.includes("Disable"),
    "Toggle button shows 'Disable'");

  info("Click on the toggle button");
  connectionPromptToggleButton =
    document.querySelector(".qa-connection-prompt-toggle-button");
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
