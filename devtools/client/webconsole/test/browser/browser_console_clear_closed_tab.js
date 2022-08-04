/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that clearing the browser console output still works if the tab that emitted some
// was closed. See Bug 1628626.

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/test/browser/test-console.html";

add_task(async function() {
  // Show the content messages
  await pushPref("devtools.browserconsole.contentMessages", true);
  // Enable Fission browser console to see the logged content object
  await pushPref("devtools.browsertoolbox.fission", true);
  await pushPref("devtools.browsertoolbox.scope", "everything");

  // Disable the preloaded process as it creates processes intermittently
  // which forces the emission of RDP requests we aren't correctly waiting for.
  await pushPref("dom.ipc.processPrelaunch.enabled", false);

  const tab = await addTab(TEST_URI);
  const hud = await BrowserConsoleManager.toggleBrowserConsole();

  info("Log a new message from the content page");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.console.log({ hello: "world" });
  });

  await waitFor(() => findConsoleAPIMessage(hud, "hello"));

  await removeTab(tab);
  // Wait for a bit, so the actors and fronts are released.
  await wait(500);

  info("Clear the console output");
  hud.ui.outputNode.querySelector(".devtools-clear-icon").click();

  await waitFor(() => !findConsoleAPIMessage(hud, "hello"));
  ok(true, "Browser Console was cleared");
});
