/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check whether can toggle enable/disable connection prompt setting.
 */
add_task(async function() {
  info("Set initial state for test");
  await pushPref("devtools.debugger.prompt-connection", true);

  const { document, tab } = await openAboutDebugging();

  info("Check whether connection prompt toggle button exists");
  const connectionPromptToggleButton =
    document.querySelector(".js-connection-prompt-toggle-button");
  ok(connectionPromptToggleButton, "Toggle button existed");
  await waitUntil(() => connectionPromptToggleButton.textContent.includes("Disable"));

  info("Click on the toggle button");
  connectionPromptToggleButton.click();
  info("Wait until the toggle button text is updated");
  await waitUntil(() => connectionPromptToggleButton.textContent.includes("Enable"));
  info("Check the preference");
  is(Services.prefs.getBoolPref("devtools.debugger.prompt-connection"),
     false,
     "The preference should be updated");

  info("Click on the toggle button again");
  connectionPromptToggleButton.click();
  info("Wait until the toggle button text is updated");
  await waitUntil(() => connectionPromptToggleButton.textContent.includes("Disable"));
  info("Check the preference");
  is(Services.prefs.getBoolPref("devtools.debugger.prompt-connection"),
     true,
     "The preference should be updated");

  await removeTab(tab);
});
