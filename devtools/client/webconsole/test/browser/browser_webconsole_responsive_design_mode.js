/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that messages are displayed in the console when RDM is enabled

const TEST_URI = "data:text/html,<meta charset=utf8>Test logging in RDM";

const ResponsiveUIManager = require("devtools/client/responsive/manager");
const message = require("devtools/client/responsive/utils/message");

add_task(async function() {
  const tab = await addTab(TEST_URI);

  // Use a local file for the device list, otherwise the panel tries to reach an external
  // URL, which makes the test fail.
  await pushPref(
    "devtools.devices.url",
    "http://example.com/browser/devtools/client/responsive/test/browser/devices.json"
  );

  info("Open responsive design mode");
  const { toolWindow } = await ResponsiveUIManager.openIfNeeded(
    tab.ownerGlobal,
    tab,
    {
      trigger: "test",
    }
  );
  await message.wait(toolWindow, "post-init");

  info("Log a message before the console is open");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.wrappedJSObject.console.log("Cached message");
  });

  info("Open the console");
  const hud = await openConsole(tab);
  await waitFor(
    () => findMessage(hud, "Cached message"),
    "Cached message isn't displayed in the console output"
  );
  ok(true, "Cached message is displayed in the console");

  info("Log a message while the console is open");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.wrappedJSObject.console.log("Live message");
  });

  await waitFor(
    () => findMessage(hud, "Live message"),
    "Live message isn't displayed in the console output"
  );
  ok(true, "Live message is displayed in the console");

  info("Close responsive design mode");
  await ResponsiveUIManager.closeIfNeeded(tab.ownerGlobal, tab);
});
